#include "config.h"
#include "logger.h"

#if defined(ALERTS_API_PROVIDER_METEOALARM)

#include <Arduino.h>
#include <cmath>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "_locale.h"
#include "client_utils.h"
#include "display_utils.h"
#include "meteoalarm_alert_provider.h"

// The renderer displays at most 2 alerts: parsing stops once that many
// matching warnings of distinct hazards were collected, cutting the download
// short (same-hazard entries are merged, see addEntry).
#define METEOALARM_NUM_ALERTS 2

// Severity rank of the MeteoAlarm awareness colors, used to keep the most
// urgent occurrence when same-hazard warnings are merged (higher wins).
#define METEOALARM_SEVERITY_RANK_NONE 0
#define METEOALARM_SEVERITY_RANK_YELLOW 1
#define METEOALARM_SEVERITY_RANK_ORANGE 2
#define METEOALARM_SEVERITY_RANK_RED 3

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

/* Severity rank of an alert event text, derived from its leading awareness
 * color word (see colorFromSeverity: "Red/Orange/Yellow <hazard> Warning",
 * or "<hazard> Warning" when no color was mapped). */
static int severityRankFromEvent(const String &event) {
  if (event.startsWith("Red ")) {
    return METEOALARM_SEVERITY_RANK_RED;
  }
  if (event.startsWith("Orange ")) {
    return METEOALARM_SEVERITY_RANK_ORANGE;
  }
  if (event.startsWith("Yellow ")) {
    return METEOALARM_SEVERITY_RANK_YELLOW;
  }
  return METEOALARM_SEVERITY_RANK_NONE;
}

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
 * kept). Entries of the same hazard (e.g. separate time windows or oblast
 * clusters of one warning) are merged into the existing alert rather than
 * appended: the validity span becomes the union of both, and the text keeps
 * the color of the most urgent severity. Merged entries do not count toward
 * METEOALARM_NUM_ALERTS, so the cap is consumed by distinct hazards only.
 */
void addEntry(entry_data_t &e, std::vector<weather_alert_t> &alerts, int64_t now, double lat, double lon) {
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

  // Merge same-hazard warnings: keep the most urgent color and expand the
  // validity span to the union of both time ranges. The 2-alert cap is
  // therefore filled with distinct hazards, not feed entries.
  for (weather_alert_t &a : alerts) {
    if (a.tags != alert.tags) {
      continue;
    }
    if (severityRankFromEvent(alert.event) > severityRankFromEvent(a.event)) {
      a.event = alert.event;
    }
    if (alert.start > 0 && (a.start <= 0 || alert.start < a.start)) {
      a.start = alert.start;
    }
    if (alert.end > a.end) {
      a.end = alert.end;
    }
    return;
  }

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
      if ((prevLat > lat) != (y > lat) && lon < (x - prevLon) * (lat - prevLat) / (y - prevLat) + prevLon) {
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
 * HTTP is 302-redirected) and map its entries into the alert model. The
 * request is sent with HTTP/1.0 so the server replies Connection: close
 * (verified against feeds.meteoalarm.org): the body is close-delimited and
 * the parser streams it to EOF without a declared content length.
 *
 * The feed (up to several hundred KB) needs far more than the default 2s
 * read window, hence the explicit 30 s timeout. */
ProviderResult MeteoAlarmAlertProvider::fetch(std::vector<weather_alert_t> &alerts) {
  WiFiClientSecure client;
  client.setCACert(cert_GEANT_TLS_RSA_1);
  const uint16_t port = 443;
  if (METEOALARM_COUNTRY.isEmpty()) {
    return ProviderResult::error(getHttpResponsePhrase(HTTP_CODE_NOT_FOUND));
  }

  String uri = "/feeds/meteoalarm-legacy-atom-" + METEOALARM_COUNTRY;
  String sanitizedUri = String(METEOALARM_ENDPOINT) + uri;

  // Optional configured location; NaN means the polygon filter is disabled.
  double lat = NAN;
  double lon = NAN;
  if (LAT.length() > 0 && LON.length() > 0) {
    lat = strtod(LAT.c_str(), nullptr);
    lon = strtod(LON.c_str(), nullptr);
  }

  const uint32_t t0 = millis();
  const ProviderResult result = httpGetWithRetry(
      client, METEOALARM_ENDPOINT, port, uri, sanitizedUri, true, 30000,
      [&alerts, lat, lon](Stream &xml, size_t) { return parseFeed(xml, alerts, time(nullptr), lat, lon); });
  LOG_DEBUG("fetch total=%u ms ok=%u detail='%s'", static_cast<unsigned>(millis() - t0), result.isOk(),
            result.detail().c_str());
  return result;
}  // MeteoAlarmAlertProvider::fetch

/* Streaming XML scanner for the MeteoAlarm Atom feed. Each <entry> repeats
 * the CAP summary of a warning; only event, severity, effective, onset,
 * expires and polygon are captured, everything else is skipped, so the
 * document is parsed as a stream without buffering it.
 *
 * The body is close-delimited (the fetch requests HTTP/1.0, so the server
 * closes the connection right after the feed): the scanner is fed one byte
 * at a time and the end of the body coincides with the end of the stream,
 * there is no declared content length to read up to. Parsing stops once
 * METEOALARM_NUM_ALERTS matching warnings were collected, cutting the
 * download short. A stream end while still inside an entry means the body
 * was truncated (e.g. by a read timeout) and is reported as an error. */

ProviderResult MeteoAlarmAlertProvider::parseFeed(Stream &xml, std::vector<weather_alert_t> &alerts, int64_t now,
                                                  double lat, double lon) {
  enum class St { TEXT, ENTITY, TAG_NAME, TAG_ATTR, TAG_ATTR_QUOTED, SKIP };

  St state = St::TEXT;
  bool inEntry = false;
  bool endTag = false;
  bool selfClosing = false;
  char quote = 0;
  String tagName;  // tag currently being parsed
  String capture;  // entry element currently accumulating text
  String text;     // captured text so far
  String entity;   // pending "&...;" reference
  entry_data_t entry;

  const uint32_t tStart = millis();
  size_t total = 0;  // bytes read from the response body

  // Feed the scanner one byte at a time: `readBytes` waits within the
  // stream's read timeout (set by httpGetWithRetry) and returns 0 when the
  // body ends, which is clean for the close-delimited HTTP/1.0 response.
  char c;
  while (xml.readBytes(&c, 1) > 0) {
    ++total;
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
            return ProviderResult::ok();
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
            return ProviderResult::ok();
          }
        }
        inEntry = true;
        entry.reset();
        if (selfClosing) {
          inEntry = false;
        }
      } else if (inEntry && (local == "event" || local == "severity" || local == "effective" || local == "onset" ||
                             local == "expires" || local == "polygon")) {
        capture = local;
        text = "";
      }
    }
  }

  // End of stream while still inside an entry: the feed was truncated (e.g.
  // by a read timeout); a complete close-delimited body ends after </feed>.
  if (inEntry || !capture.isEmpty()) {
    LOG_WARNING("MeteoAlarm: feed ended early after %u bytes, scanner in state %d (inEntry=%u, capture='%s', "
                "tagName='%s')",
                static_cast<unsigned>(total), static_cast<int>(state), inEntry, capture.c_str(), tagName.c_str());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
  }
  LOG_DEBUG("feed: %u bytes -> %u alerts in %u ms", static_cast<unsigned>(total), static_cast<unsigned>(alerts.size()),
            static_cast<unsigned>(millis() - tStart));
  return ProviderResult::ok();
}  // MeteoAlarmAlertProvider::parseFeed

#endif  // ALERTS_API_PROVIDER_METEOALARM
