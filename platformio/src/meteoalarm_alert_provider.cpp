#include "config.h"

#if defined(ALERTS_API_PROVIDER_METEOALARM)

#include <Arduino.h>
#include <cmath>
#include <esp_timer.h>
#include <memory>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "client_utils.h"
#include "meteoalarm_alert_provider.h"

// The renderer displays at most 2 alerts: parsing stops once that many
// matching warnings were collected, cutting the download short.
#define METEOALARM_NUM_ALERTS 2

static const char *METEOALARM_ENDPOINT = "feeds.meteoalarm.org";

namespace {

/* Data of a single feed entry that is being parsed. */
struct entry_data_t {
  String event;
  String severity;
  String effective;
  String onset;
  String expires;
  String polygon;  // raw space-separated "lat,lon" ring, empty if absent
  bool any = false;

  void reset() {
    event = "";
    severity = "";
    effective = "";
    onset = "";
    expires = "";
    polygon = "";
    any = false;
  }
};

/* Days from civil epoch (1970-01-01), from Howard Hinnant's date algorithms. */
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

/* Add an entry to the alerts if it has not expired yet and its polygon, if
 * any, contains the configured location (alerts without a polygon are
 * kept). */
void addEntry(entry_data_t &e, std::vector<weather_alert_t> &alerts, int64_t now, double lat,
              double lon) {
  if (!e.any) {
    return;
  }

  String hazard = MeteoAlarmAlertProvider::hazardFromEvent(e.event);
  if (hazard.isEmpty()) {
    return;
  }

  const String color = MeteoAlarmAlertProvider::colorFromSeverity(e.severity);
  weather_alert_t alert = {};
  alert.event = color.isEmpty() ? (hazard + " Warning") : (color + " " + hazard + " Warning");
  alert.start = MeteoAlarmAlertProvider::parseIso8601(!e.onset.isEmpty() ? e.onset : e.effective);
  alert.end = MeteoAlarmAlertProvider::parseIso8601(e.expires);

  // Skip warnings that have already expired, unless the clock is not
  // synchronized yet (epoch < 2021).
  if (alert.end > 0 && now > 1609459200LL && alert.end < now) {
    return;
  }

  // Skip warnings whose polygon does not contain the configured location.
  if (!std::isnan(lat) && !std::isnan(lon) && !e.polygon.isEmpty() &&
      !MeteoAlarmAlertProvider::pointInPolygon(lat, lon, e.polygon)) {
    return;
  }

  alert.tags = hazard;
  alert.tags.toLowerCase();

  alerts.push_back(alert);
}

}  // namespace

/* Returns true if (lat, lon) lies inside the polygon given as space-separated
 * "lat,lon" pairs in WGS84 decimal degrees (CAP format, ray casting, closed
 * ring). A polygon with fewer than 3 valid points can not be evaluated: true
 * is returned so the alert is kept. Boundary points count as inside. */
bool MeteoAlarmAlertProvider::pointInPolygon(double lat, double lon, const String &polygon) {
  const char *p = polygon.c_str();
  const char *end = p + polygon.length();

  double firstLat = 0, firstLon = 0;
  double prevLat = 0, prevLon = 0;
  unsigned numPoints = 0;
  bool inside = false;

  while (p < end) {
    char *next = nullptr;
    const double y = strtod(p, &next);  // CAP polygon points are "lat,lon"
    if (next == p) {
      p += 1;  // skip a stray separator
      continue;
    }
    p = next;
    while (p < end && *p == ',') {
      ++p;  // skip the lat/lon separator
    }
    const double x = strtod(p, &next);
    if (next == p) {
      break;  // malformed trailing token
    }
    p = next;

    if (numPoints == 0) {
      firstLat = y;
      firstLon = x;
    } else {
      // Ray casting crossing test for the edge (prev -> cur)
      if ((prevLat > lat) != (y > lat) &&
          lon < (x - prevLon) * (lat - prevLat) / (y - prevLat) + prevLon) {
        inside = !inside;
      }
    }
    prevLat = y;
    prevLon = x;
    ++numPoints;
  }

  if (numPoints < 3) {
    return true;  // not a usable polygon, keep the alert
  }

  // Closing edge (last -> first) if the ring is not explicitly closed
  if (prevLat != firstLat || prevLon != firstLon) {
    if ((prevLat > lat) != (firstLat > lat) &&
        lon < (firstLon - prevLon) * (lat - prevLat) / (firstLat - prevLat) + prevLon) {
      inside = !inside;
    }
  }

  return inside;
}  // MeteoAlarmAlertProvider::pointInPolygon

int64_t MeteoAlarmAlertProvider::parseIso8601(const String &s) {
  if (s.length() < 19) {
    return -1;
  }
  for (int i = 0; i < 19; ++i) {
    const char c = s.charAt(i);
    bool valid;
    if (i == 10) {
      valid = (c == 'T' || c == 't');
    } else if (i == 4 || i == 7) {
      valid = (c == '-');
    } else if (i == 13 || i == 16) {
      valid = (c == ':');
    } else {
      valid = (c >= '0' && c <= '9');
    }
    if (!valid) {
      return -1;
    }
  }
  const int64_t y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  const unsigned mo = (s[5] - '0') * 10 + (s[6] - '0');
  const unsigned d = (s[8] - '0') * 10 + (s[9] - '0');
  const int h = (s[11] - '0') * 10 + (s[12] - '0');
  const int mi = (s[14] - '0') * 10 + (s[15] - '0');
  const int se = (s[17] - '0') * 10 + (s[18] - '0');

  int64_t offsetMin = 0;  // minutes east of UTC
  if (s.length() >= 20) {
    const char tz = s.charAt(19);
    if (tz == '+' || tz == '-') {
      if (s.length() < 25) {
        return -1;
      }
      offsetMin = ((s[20] - '0') * 10 + (s[21] - '0')) * 60 + (s[23] - '0') * 10 + (s[24] - '0');
      if (tz == '-') {
        offsetMin = -offsetMin;
      }
    } else if (tz != 'Z' && tz != 'z') {
      return -1;
    }
  }

  return daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se - offsetMin * 60;
}  // MeteoAlarmAlertProvider::parseIso8601

/* Strip the leading color word ("yellow ", "orange ", "red ") and any
 * trailing "warning"/"watch"/"alert" suffix from an event name and return the
 * remaining hazard in title case, e.g.
 *   "Thunderstormwarning"              -> "Thunderstorm"
 *   "Yellow Rain Warning"              -> "Rain"
 *   "Orange High-temperature Warning"  -> "High-temperature"
 */
String MeteoAlarmAlertProvider::hazardFromEvent(const String &event) {
  String h = event;
  h.trim();
  h.toLowerCase();

  if (h.startsWith("yellow ")) {
    h = h.substring(7);
  } else if (h.startsWith("orange ")) {
    h = h.substring(7);
  } else if (h.startsWith("red ")) {
    h = h.substring(4);
  }

  if (h.endsWith(" warning")) {
    h = h.substring(0, h.length() - 8);
  } else if (h.endsWith(" watch")) {
    h = h.substring(0, h.length() - 6);
  } else if (h.endsWith(" alert")) {
    h = h.substring(0, h.length() - 6);
  } else if (h.endsWith("warning")) {
    h = h.substring(0, h.length() - 7);
  } else if (h.endsWith("watch")) {
    h = h.substring(0, h.length() - 5);
  } else if (h.endsWith("alert")) {
    h = h.substring(0, h.length() - 5);
  }

  h.trim();
  if (h.isEmpty()) {
    return h;
  }
  if (h[0] >= 'a' && h[0] <= 'z') {
    h[0] = h[0] - 'a' + 'A';
  }
  return h;
}  // MeteoAlarmAlertProvider::hazardFromEvent

/* Map the CAP severity to the MeteoAlarm awareness color ("", Yellow, Orange
 * or Red). Missing severity is mapped to an empty color. */
String MeteoAlarmAlertProvider::colorFromSeverity(const String &severity) {
  if (severity == "Severe") {
    return "Orange";
  }
  if (severity == "Extreme") {
    return "Red";
  }
  if (severity.isEmpty()) {
    return "";
  }
  return "Yellow";  // Minor and Moderate
}  // MeteoAlarmAlertProvider::colorFromSeverity

/* Fetch the legacy Atom feed of the configured country over HTTPS (plain
 * HTTP is 302-redirected) and map its entries into the alert model.
 * Returns the HTTP status code. */
int MeteoAlarmAlertProvider::fetch(std::vector<weather_alert_t> &alerts) {
  WiFiClientSecure client;
  client.setCACert(cert_GEANT_TLS_RSA_1);
  const uint16_t port = 443;
  if (METEOALARM_COUNTRY.isEmpty()) {
    return HTTP_CODE_NOT_FOUND;
  }

  String uri = "/feeds/meteoalarm-legacy-atom-" + METEOALARM_COUNTRY;
  String sanitizedUri = String(METEOALARM_ENDPOINT) + uri;

  // Optional configured location; NaN means the polygon filter is disabled.
  double lat = NAN;
  double lon = NAN;
  if (strlen(D_LATITUDE) > 0 && strlen(D_LONGITUDE) > 0) {
    lat = strtod(D_LATITUDE, nullptr);
    lon = strtod(D_LONGITUDE, nullptr);
  }

  // The feed (up to several hundred KB) needs far more than the default 2s
  // read window.
  const uint32_t t0 = millis();
  const int code = httpGetWithRetry(client, METEOALARM_ENDPOINT, port, uri, sanitizedUri, false, 30000,
                                    [&alerts, lat, lon, t0, &client](Stream &xml, size_t expectedLen) {
#if DEBUG_LEVEL >= 1
                                      Serial.printf("[METEOALARM] fetch: headers at +%u ms\n",
                                                    static_cast<unsigned>(millis() - t0));
#endif
                                      return parseFeed(xml, alerts, time(nullptr), lat, lon, expectedLen, &client);
                                    });
#if DEBUG_LEVEL >= 1
  Serial.printf("[METEOALARM] fetch total=%u ms status=%d\n", static_cast<unsigned>(millis() - t0), code);
#endif
  return code;
}  // MeteoAlarmAlertProvider::fetch

/* Streaming XML scanner for the MeteoAlarm Atom feed. Each <entry> repeats
 * the CAP summary of a warning; only event, severity, effective, onset,
 * expires and polygon are captured, everything else is skipped, so the
 * document is parsed as a stream without buffering it.
 *
 * Parsing stops once METEOALARM_NUM_ALERTS matching warnings were collected
 * and once `expectedLen` bytes were read the body is considered fully
 * consumed: no read past it, so a connection closed right after the body
 * never surfaces as a read error. */

/* Reads up to `length` bytes from `stream`, waiting for each within the
 * stream's read timeout (a delivered byte refreshes the deadline). The TLS
 * client's block read() serves multi-KB mbedTLS plaintext per call; other
 * streams (unit test feeds) fall back to single-byte reads. Stream::readBytes
 * cannot be used: it treats the non-blocking -1 as a hard error (truncating
 * at TCP/TLS burst boundaries) or busy-spins in timedRead (starving the idle
 * task), so poll with delay(1) instead. `stallMs`/`stalls` count dry-read
 * waits: a stall-dominated read points at the network stack, not the parser.
 */
static size_t readBytesYielding(Stream &stream, NetworkClient *networkClient, char *buffer, size_t length,
                                unsigned long &stallMs, unsigned long &stalls) {
  size_t count = 0;
  unsigned long deadline = millis() + stream.getTimeout();
  unsigned long stallStart = 0;
  while (count < length) {
    if (networkClient != nullptr) {
      // Bulk path (production TLS client): read() returns the byte count.
      const int r = networkClient->read(reinterpret_cast<uint8_t *>(buffer) + count, length - count);
      if (r > 0) {
        if (stallStart != 0) {
          stallMs += millis() - stallStart;
          ++stalls;
          stallStart = 0;
        }
        count += r;
        deadline = millis() + stream.getTimeout();
        continue;
      }
    } else {
      // Single-byte path (unit test feeds): read() returns one byte value.
      const int c = stream.read();
      if (c >= 0) {
        if (stallStart != 0) {
          stallMs += millis() - stallStart;
          ++stalls;
          stallStart = 0;
        }
        buffer[count++] = static_cast<char>(c);
        deadline = millis() + stream.getTimeout();
        continue;
      }
    }
    // No data right now (non-blocking TLS client / exhausted feed).
    if (millis() > deadline) {
      break;  // read timeout expired
    }
    if (stallStart == 0) {
      stallStart = millis();
    }
    delay(1);  // yield to other tasks / the watchdog
  }
  return count;
}

DeserializationError MeteoAlarmAlertProvider::parseFeed(Stream &xml, std::vector<weather_alert_t> &alerts,
                                                        int64_t now, double lat, double lon, size_t expectedLen,
                                                        NetworkClient *networkClient) {
  enum class St { TEXT, ENTITY, TAG_NAME, TAG_ATTR, TAG_ATTR_QUOTED, SKIP };

  St state = St::TEXT;
  bool inEntry = false;
  bool endTag = false;
  bool selfClosing = false;
  char quote = 0;
  String tagName;   // tag currently being parsed
  String capture;   // entry element currently accumulating text
  String text;      // captured text so far
  String entity;    // pending "&...;" reference
  entry_data_t entry;

#if DEBUG_LEVEL >= 1
  const uint32_t tStart = millis();
  Serial.printf("[METEOALARM] feed: headers at +%u ms (body %u B, heap %u)\n",
                static_cast<unsigned>(millis() - tStart), static_cast<unsigned>(expectedLen),
                static_cast<unsigned>(ESP.getFreeHeap()));
#endif

  std::unique_ptr<char[]> buf(new char[4096]);
  size_t total = 0;  // bytes read from the response body
  bool atExpectedEnd = false;
  char tail[64];      // rolling buffer of the last bytes read, for diagnostics
  size_t tailPos = 0;
  unsigned long parseStartMillis = millis();
  unsigned long stallMs = 0;
  unsigned long stalls = 0;
#if DEBUG_LEVEL >= 1
  uint64_t readUs = 0;    // µs spent in stream reads (network/TLS waits)
  uint64_t parseUs = 0;   // µs spent in the XML scanner
  uint64_t entryUs = 0;   // µs spent in addEntry/pointInPolygon at entry close
  uint32_t tFirstByte = 0;
  auto logTiming = [&](const char *reason) {
    const uint32_t tEnd = millis();
    const uint32_t firstByte = (tFirstByte != 0) ? tFirstByte - tStart : 0;
    const uint32_t body = (tFirstByte != 0) ? tEnd - tFirstByte : 0;
    Serial.printf("[METEOALARM] feed: %s firstByte=%u ms body=%u ms parse=%u ms "
                  "readUs=%lu ms parseUs=%lu ms entryUs=%lu ms bytes=%u alerts=%u "
                  "stalls=%lu stallMs=%lu ms rssi=%d heap=%u\n",
                  reason, static_cast<unsigned>(firstByte), static_cast<unsigned>(body),
                  static_cast<unsigned>(tEnd - tStart), static_cast<unsigned long>(readUs / 1000),
                  static_cast<unsigned long>(parseUs / 1000), static_cast<unsigned long>(entryUs / 1000),
                  static_cast<unsigned>(total), static_cast<unsigned>(alerts.size()), stalls, stallMs, WiFi.RSSI(),
                  static_cast<unsigned>(ESP.getFreeHeap()));
  };
#endif
  while (true) {
    if (expectedLen > 0 && total >= expectedLen) {
      // Body consumed; do not read past it (the server may close right after).
      atExpectedEnd = true;
      break;
    }
    // Never request beyond the remaining body: a server closing the socket
    // right after it would otherwise surface as a TLS read error.
    size_t want = 4096;
    if (expectedLen > 0 && expectedLen - total < want) {
      want = expectedLen - total;
    }
    // Wait up to the stream timeout for each chunk, yielding in between (see
    // readBytesYielding): TCP/TLS pauses must not truncate the feed.
#if DEBUG_LEVEL >= 1
    const int64_t tRead0 = esp_timer_get_time();
#endif
    const size_t n = readBytesYielding(xml, networkClient, buf.get(), want, stallMs, stalls);
#if DEBUG_LEVEL >= 1
    readUs += static_cast<uint64_t>(esp_timer_get_time() - tRead0);
#endif
    if (n == 0) {
      break;  // end of stream (or read timeout)
    }
    total += n;
#if DEBUG_LEVEL >= 1
    if (tFirstByte == 0) {
      tFirstByte = millis();
    }
#endif
    #if DEBUG_LEVEL >= 1
    const int64_t tParse0 = esp_timer_get_time();
#endif
    for (size_t i = 0; i < n; ++i) {
      const char c = buf[i];
      tail[tailPos] = c;
      tailPos = (tailPos + 1) % sizeof(tail);

      switch (state) {
        case St::TEXT: {
          if (c == '<') {
            state = St::TAG_NAME;
            tagName = "";
            endTag = false;
            selfClosing = false;
          } else if (c == '&') {
            entity = "";
            state = St::ENTITY;
          } else if (!capture.isEmpty()) {
            text += c;
          }
          break;
        }
        case St::ENTITY: {
          if (c == ';') {
            if (entity == "amp") {
              text += '&';
            } else if (entity == "lt") {
              text += '<';
            } else if (entity == "gt") {
              text += '>';
            } else if (entity == "quot") {
              text += '"';
            } else if (entity == "apos") {
              text += '\'';
            } else {
              text += '&';
              text += entity;
              text += ';';
            }
            state = St::TEXT;
          } else if (entity.length() >= 8) {
            // too long to be a named entity, keep it as raw text
            text += '&';
            text += entity;
            text += c;
            entity = "";
            state = St::TEXT;
          } else {
            entity += c;
          }
          break;
        }
        case St::TAG_NAME: {
          if (c == '?' || c == '!') {
            // <?xml ...?> declaration or <!DOCTYPE ...>/<!-- ... -->, skip
            state = St::SKIP;
          } else if (c == '/') {
            endTag = true;
          } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            state = St::TAG_ATTR;
          } else if (c == '>') {
            // tag complete
            goto finish_tag;  // NOLINT(bugprone-branch-clone)
          } else {
            tagName += c;
          }
          break;
        }
        case St::TAG_ATTR: {
          if (c == '"' || c == '\'') {
            quote = c;
            state = St::TAG_ATTR_QUOTED;
          } else if (c == '/') {
            selfClosing = true;
          } else if (c == '>') {
            goto finish_tag;  // NOLINT(bugprone-branch-clone)
          }
          break;
        }
        case St::TAG_ATTR_QUOTED: {
          if (c == quote) {
            state = St::TAG_ATTR;
          }
          break;
        }
        case St::SKIP: {
          if (c == '>') {
            state = St::TEXT;
          }
          break;
        }
      }

      continue;

    finish_tag:;
      state = St::TEXT;  // a finished tag is always followed by text
      const String local = tagName.substring(tagName.indexOf(':') + 1);

      if (endTag) {
        if (local == "entry") {
          if (inEntry) {
            #if DEBUG_LEVEL >= 1
            const int64_t tEntry0 = esp_timer_get_time();
#endif
            addEntry(entry, alerts, now, lat, lon);
#if DEBUG_LEVEL >= 1
            entryUs += static_cast<uint64_t>(esp_timer_get_time() - tEntry0);
#endif
            inEntry = false;
            entry.reset();
            if (alerts.size() >= METEOALARM_NUM_ALERTS) {
              #if DEBUG_LEVEL >= 1
              logTiming("early-exit");
#endif
              return DeserializationError::Ok;
            }
          }
        } else if (inEntry && local == capture) {
          if (local == "event") {
            entry.event = text;
          } else if (local == "severity") {
            entry.severity = text;
          } else if (local == "effective") {
            entry.effective = text;
          } else if (local == "onset") {
            entry.onset = text;
          } else if (local == "expires") {
            entry.expires = text;
          } else if (local == "polygon") {
            entry.polygon = text;
          }
          entry.any = true;
          capture = "";
          text = "";
        }
      } else {
        if (local == "entry") {
          if (inEntry) {
            // previous entry closed implicitly by a new one
            #if DEBUG_LEVEL >= 1
            const int64_t tEntry0 = esp_timer_get_time();
#endif
            addEntry(entry, alerts, now, lat, lon);
#if DEBUG_LEVEL >= 1
            entryUs += static_cast<uint64_t>(esp_timer_get_time() - tEntry0);
#endif
            if (alerts.size() >= METEOALARM_NUM_ALERTS) {
              #if DEBUG_LEVEL >= 1
              logTiming("early-exit");
#endif
              return DeserializationError::Ok;
            }
          }
          inEntry = true;
          entry.reset();
          if (selfClosing) {
            inEntry = false;
          }
        } else if (inEntry && (local == "event" || local == "severity" || local == "effective" ||
                               local == "onset" || local == "expires" || local == "polygon")) {
          capture = local;
          text = "";
        }
      }
    }
    #if DEBUG_LEVEL >= 1
    parseUs += static_cast<uint64_t>(esp_timer_get_time() - tParse0);
#endif
  }

  // End of stream while still inside an entry: the feed was truncated by a
  // timeout (a break at the advertised content length is expected).
  if (!atExpectedEnd && (inEntry || !capture.isEmpty())) {
    Serial.println("[error] MeteoAlarm: feed ended early, " + String(total) + " of " +
                   String(expectedLen) + " bytes read, scanner in state " + String(static_cast<int>(state)) +
                   " (inEntry=" + String(inEntry) + ", capture='" + capture + "', tagName='" + tagName + "')");
    Serial.println("[error]   parse loop took " + String(millis() - parseStartMillis) +
                   " ms, stream read timeout " + String(xml.getTimeout()) + " ms, available " +
                   String(xml.available()));
#if DEBUG_LEVEL >= 1
    const size_t avail = total < sizeof(tail) ? total : sizeof(tail);
    const size_t start = total < sizeof(tail) ? 0 : tailPos;
    Serial.print("[debug] last " + String(avail) + " body bytes: ");
    for (size_t i = 0; i < avail; ++i) {
      const char c = tail[(start + i) % sizeof(tail)];
      if (c >= 0x20 && c <= 0x7E) {
        Serial.print(c);
      } else {
        Serial.printf("\\x%02X", static_cast<uint8_t>(c));
      }
    }
    Serial.println();
#endif
    #if DEBUG_LEVEL >= 1
    logTiming("truncated");
#endif
    return DeserializationError::InvalidInput;
  }
  #if DEBUG_LEVEL >= 1
  logTiming(atExpectedEnd ? "complete" : "end-of-stream");
#endif
  return DeserializationError::Ok;
}  // MeteoAlarmAlertProvider::parseFeed

#endif  // ALERTS_API_PROVIDER_METEOALARM
