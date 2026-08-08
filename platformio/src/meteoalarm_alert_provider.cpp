#include "config.h"

#if defined(ALERTS_API_PROVIDER_METEOALARM)

#include <Arduino.h>
#include <cmath>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "client_utils.h"
#include "meteoalarm_alert_provider.h"

// Limit the number of parsed alerts. The renderer (drawAlerts) displays at
// most 2 alerts, so parsing stops as soon as 2 matching warnings have been
// collected: the remaining body is not read, cutting the (window-limited)
// download short.
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

/* Add an entry to the alert list if it covers the configured location and
 * has not expired yet.
 *
 * A location (lat/lon, e.g. parsed from D_LATITUDE/D_LONGITUDE) is optional:
 * if given, an alert with a polygon that does not contain the location is
 * skipped. Alerts without a polygon can not be evaluated and are kept. */
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

/* Returns true if (lat, lon) is inside the closed polygon given as
 * space-separated "lat,lon" pairs in WGS84 decimal degrees (CAP format).
 *
 * Implements the ray casting algorithm; the ring is assumed to be closed
 * (last point repeats the first), as emitted by MeteoAlarm. A polygon with
 * fewer than 3 valid points can not be evaluated and is treated as
 * non-matching-safe: true is returned so the alert is kept. Points on the
 * boundary count as inside.
 */
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

/* Perform an HTTP GET request to feeds.meteoalarm.org requesting the legacy
 * Atom feed of the configured country, and map the alert summary repeated in
 * each entry into the generic alert model.
 *
 * The feed is always fetched over HTTPS with certificate verification; the
 * feed server rejects plain HTTP with a 302 redirect to HTTPS.
 *
 * Returns the HTTP Status Code.
 */
int MeteoAlarmAlertProvider::fetch(std::vector<weather_alert_t> &alerts) {
  WiFiClientSecure client;
  client.setCACert(cert_GEANT_TLS_RSA_1);
  const uint16_t port = 443;
  if (METEOALARM_COUNTRY.isEmpty()) {
    return HTTP_CODE_NOT_FOUND;
  }

  String uri = "/feeds/meteoalarm-legacy-atom-" + METEOALARM_COUNTRY;
  String sanitizedUri = String(METEOALARM_ENDPOINT) + uri;

  // The configured location is used to filter warnings by their geographic
  // polygon; NaN means "not configured" (filter disabled).
  double lat = NAN;
  double lon = NAN;
  if (strlen(D_LATITUDE) > 0 && strlen(D_LONGITUDE) > 0) {
    lat = strtod(D_LATITUDE, nullptr);
    lon = strtod(D_LONGITUDE, nullptr);
  }

  // The feed can be several hundred KB (each warning repeats its polygon in
  // many variants), which needs far more than the default 2s read window.
  return httpGetWithRetry(client, METEOALARM_ENDPOINT, port, uri, sanitizedUri, false, 30000,
                          [&alerts, lat, lon](Stream &xml, size_t expectedLen) {
                            return parseFeed(xml, alerts, time(nullptr), lat, lon, expectedLen);
                          });
}  // MeteoAlarmAlertProvider::fetch

/* Streaming XML scanner for the MeteoAlarm Atom feed.
 *
 * The feed is a machine generated Atom document in which each <entry>
 * repeats the CAP summary of a warning:
 *
 *   <entry>
 *     <cap:event>...</cap:event>
 *     <cap:severity>...</cap:severity>
 *     <cap:effective>...</cap:effective>
 *     <cap:onset>...</cap:onset>
 *     <cap:expires>...</cap:expires>
 *     <cap:polygon>...</cap:polygon>
 *   </entry>
 *
 * Only the elements above are captured (the polygon is used to check whether
 * the configured location is affected), everything else (links, titles,
 * geocodes, area lists, ...) is skipped, so the document can be parsed as a
 * stream without buffering it in memory.
 *
 * Parsing stops as soon as METEOALARM_NUM_ALERTS matching warnings have been
 * collected (the renderer can not display more), and when `expectedLen` bytes
 * have been read the response body is considered fully consumed: no further
 * read is attempted, so a connection closed by the server right after the
 * body does not surface as a read error.
 */

/* Reads up to `length` bytes from `stream`, waiting for the next byte while
 * the stream reports no data. The wait is bounded by the stream's configured
 * read timeout (getTimeout), and each delivered byte refreshes the deadline:
 * 30s of silence fails, a slow-but-steady stream never does.
 *
 * Stream::readBytes cannot be used directly: on the TLS client the virtual
 * NetworkClient::readBytes treats the non-blocking read()'s -1 ("no data
 * right now") as a hard error and returns instantly, truncating the feed at
 * the first TCP/TLS burst boundary; the base Stream::readBytes busy-spins in
 * timedRead instead, which starves the idle task and trips the task watchdog
 * during long stalls. Polling with delay(1) yields to the idle task while
 * waiting.
 */
static size_t readBytesYielding(Stream &stream, char *buffer, size_t length) {
  size_t count = 0;
  unsigned long deadline = millis() + stream.getTimeout();
  while (count < length) {
    const int c = stream.read();
    if (c >= 0) {
      buffer[count++] = static_cast<char>(c);
      deadline = millis() + stream.getTimeout();
      continue;
    }
    if (millis() > deadline) {
      break;  // no data arrived within the read timeout
    }
    delay(1);  // yield so other tasks (and the watchdog) can run
  }
  return count;
}

DeserializationError MeteoAlarmAlertProvider::parseFeed(Stream &xml, std::vector<weather_alert_t> &alerts,
                                                        int64_t now, double lat, double lon, size_t expectedLen) {
  enum class St { TEXT, ENTITY, TAG_NAME, TAG_ATTR, TAG_ATTR_QUOTED, SKIP };

  St state = St::TEXT;
  bool inEntry = false;
  bool endTag = false;
  bool selfClosing = false;
  char quote = 0;
  String tagName;   // tag name of the tag currently being parsed
  String capture;   // entry element whose text is currently being accumulated
  String text;      // accumulated text of the captured element
  String entity;    // pending entity reference "&...;"
  entry_data_t entry;

  char buf[128];
  size_t total = 0;  // bytes read from the response body
  bool atExpectedEnd = false;
  char tail[64];      // rolling buffer with the last bytes read, for diagnostics
  size_t tailPos = 0;
  unsigned long parseStartMillis = millis();
  while (true) {
    if (expectedLen > 0 && total >= expectedLen) {
      // The whole advertised body was consumed; do not read past it (the
      // server may have closed the connection right after the response).
      atExpectedEnd = true;
      break;
    }
    // Request at most the bytes that remain in the body: readBytes stops as
    // soon as its count is reached, so an exact final chunk never probes the
    // connection past the end of the response (a server closing the socket
    // right after the body would otherwise surface as a TLS read error).
    size_t want = sizeof(buf);
    if (expectedLen > 0 && expectedLen - total < want) {
      want = expectedLen - total;
    }
    // Wait up to the stream timeout for the next byte, yielding in between
    // (see readBytesYielding): pauses between TCP/TLS bursts (window refills,
    // slow server first bytes) must not truncate the feed.
    const size_t n = readBytesYielding(xml, buf, want);
    if (n == 0) {
      break;  // end of stream (or read timeout)
    }
    total += n;
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
            addEntry(entry, alerts, now, lat, lon);
            inEntry = false;
            entry.reset();
            if (alerts.size() >= METEOALARM_NUM_ALERTS) {
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
            addEntry(entry, alerts, now, lat, lon);
            if (alerts.size() >= METEOALARM_NUM_ALERTS) {
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
  }

  // End of stream reached while still inside an entry: the feed was
  // truncated by a timeout. A break at the advertised content length is
  // expected and fine.
  if (!atExpectedEnd && (inEntry || !capture.isEmpty())) {
    Serial.println("[error] MeteoAlarm: feed ended early, " + String(total) + " of " +
                   String(expectedLen) + " bytes read, scanner in state " + String(static_cast<int>(state)) +
                   " (inEntry=" + String(inEntry) + ", capture='" + capture + "', tagName='" + tagName + "')");
    Serial.println("[error]   parse loop took " + String(millis() - parseStartMillis) +
                   " ms, stream read timeout " + String(xml.getTimeout()) + " ms, available " +
                   String(xml.available()));
#if DEBUG_LEVEL >= 1
    Serial.print("[debug] last " + String(sizeof(tail)) + " body bytes: ");
    for (size_t i = 0; i < sizeof(tail); ++i) {
      const char c = tail[(tailPos + i) % sizeof(tail)];
      if (c >= 0x20 && c <= 0x7E) {
        Serial.print(c);
      } else {
        Serial.printf("\\x%02X", static_cast<uint8_t>(c));
      }
    }
    Serial.println();
#endif
    return DeserializationError::InvalidInput;
  }
  return DeserializationError::Ok;
}  // MeteoAlarmAlertProvider::parseFeed

#endif  // ALERTS_API_PROVIDER_METEOALARM
