/* MeteoAlarm alert provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "logger.h"

#if defined(ALERTS_API_PROVIDER_METEOALARM)

#include <Arduino.h>
#include <cmath>
#include <WiFi.h>
#include "esp_http_client.h"
#include "cert.h"
#include "_locale.h"
#include "display_utils.h"
#include "meteoalarm_alert_provider.h"

// The renderer displays at most 2 alerts: parsing stops once that many
// matching warnings of distinct hazards were collected and the connection
// is closed without reading the remainder (see fetch() early-close loop).
// Same-hazard entries are merged, see FeedParser::addEntry.
#define METEOALARM_NUM_ALERTS 2

// Severity rank of the MeteoAlarm awareness colors, used to keep the most
// urgent occurrence when same-hazard warnings are merged (higher wins).
#define METEOALARM_SEVERITY_RANK_NONE 0
#define METEOALARM_SEVERITY_RANK_YELLOW 1
#define METEOALARM_SEVERITY_RANK_ORANGE 2
#define METEOALARM_SEVERITY_RANK_RED 3

static const char *METEOALARM_ENDPOINT = "feeds.meteoalarm.org";

namespace {

constexpr int kHttpStatusOk = 200;
constexpr int kHttpStatusNotFound = 404;

/* Severity rank of an alert event text, derived from its leading awareness
 * color word (see colorFromSeverity: "Red/Orange/Yellow <hazard> Warning",
 * or "<hazard> Warning" when no color was mapped). */
int severityRankFromEvent(const String &event) {
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
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

/* No event handler is used: fetch() reads via esp_http_client_read in a
 * loop and feeds the parser directly, so it can close the connection as
 * soon as METEOALARM_NUM_ALERTS distinct hazards are collected. */

}  // namespace

void MeteoAlarmAlertProvider::FeedParser::EntryData::reset() {
  event = "";
  severity = "";
  effective = "";
  onset = "";
  expires = "";
  polygon = "";
  any = false;
}

MeteoAlarmAlertProvider::FeedParser::FeedParser(std::vector<weather_alert_t> &alerts, int64_t now, double lat,
                                                double lon)
    : alerts_(alerts), now_(now), lat_(lat), lon_(lon), tStart_(millis()) {}

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
 * HTTP is 302-redirected) and map its entries into the alert model via
 * esp_http_client_read in bounded chunks, so the body (which has no declared
 * Content-Length) is never buffered in full. Once METEOALARM_NUM_ALERTS
 * matching warnings are collected the connection is closed without reading
 * the remainder to save time/bandwidth.
 *
 * The feed (up to several hundred KB) needs far more than a short read
 * window, hence the explicit 30 s timeout. */
ProviderResult MeteoAlarmAlertProvider::fetch(std::vector<weather_alert_t> &alerts) {
  if (METEOALARM_COUNTRY.isEmpty()) {
    return ProviderResult::error(getHttpResponsePhrase(kHttpStatusNotFound));
  }

  const String uri = "/feeds/meteoalarm-legacy-atom-" + METEOALARM_COUNTRY;
  const String url = "https://" + String(METEOALARM_ENDPOINT) + uri;

  // Optional configured location; NaN means the polygon filter is disabled.
  double lat = NAN;
  double lon = NAN;
  if (LAT.length() > 0 && LON.length() > 0) {
    lat = strtod(LAT.c_str(), nullptr);
    lon = strtod(LON.c_str(), nullptr);
  }

  LOG_INFO("%s: %s", TXT_ATTEMPTING_HTTP_REQ, url.c_str());

  const uint32_t t0 = millis();
  int attempts = 0;
  ProviderResult result;
  while (!result.isOk() && attempts < 3) {
    const wl_status_t connectionStatus = WiFi.status();
    if (connectionStatus != WL_CONNECTED) {
      // The -512 offset stays private here: it only feeds the phrase lookup.
      result = ProviderResult::error(getHttpResponsePhrase(-512 - static_cast<int>(connectionStatus)));
      break;
    }

    alerts.clear();
    FeedParser parser(alerts, time(nullptr), lat, lon);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.cert_pem = cert_GEANT_TLS_RSA_1;
    config.timeout_ms = 30000;
    config.method = HTTP_METHOD_GET;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
      result = ProviderResult::error(esp_err_to_name(ESP_FAIL));
      LOG_INFO("%d %s", 0, result.detail().c_str());
      ++attempts;
      if (!result.isOk()) {
        delay(100);
      }
      continue;
    }

    esp_err_t openErr = esp_http_client_open(client, 0);
    int status = 0;
    if (openErr != ESP_OK) {
      result = ProviderResult::error(esp_err_to_name(openErr));
      esp_http_client_cleanup(client);
      LOG_INFO("%d %s", status, result.detail().c_str());
      ++attempts;
      if (!result.isOk()) {
        delay(100);
      }
      continue;
    }

    // Fetch headers to obtain the status code; content length is ignored
    // (the feed is chunked / close-delimited).
    esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);

    if (status != kHttpStatusOk) {
      if (status > 0) {
        result = ProviderResult::error(getHttpResponsePhrase(status));
      } else {
        result = ProviderResult::error(esp_err_to_name(ESP_ERR_HTTP_FETCH_HEADER));
      }
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      LOG_INFO("%d %s", status, result.detail().c_str());
      ++attempts;
      if (!result.isOk()) {
        delay(100);
      }
      continue;
    }

    // Stream the body in bounded chunks directly into the parser.
    // Close early once the alert cap is reached. Buffer is heap-allocated
    // (vector) to avoid 1 KB stack pressure; 1024 B balances TLS record
    // size (~1.4 KB) and heap usage.
    std::vector<char> buf(1024);
    bool readFailed = false;
    esp_err_t readErr = ESP_OK;
    while (!parser.isAlertCapReached()) {
      int n = esp_http_client_read(client, buf.data(), buf.size());
      if (n > 0) {
        parser.feed(buf.data(), static_cast<size_t>(n));
      } else if (n == 0) {
        break;
      } else {
        // n < 0: error or timeout (-ESP_ERR_HTTP_EAGAIN etc.)
        readFailed = true;
        if (n == -ESP_ERR_HTTP_EAGAIN) {
          readErr = ESP_ERR_HTTP_EAGAIN;
        } else {
          readErr = ESP_FAIL;
        }
        break;
      }
    }

    if (parser.isAlertCapReached()) {
      LOG_INFO("MeteoAlarm: alert cap reached, closing connection early");
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (readFailed && !parser.isAlertCapReached()) {
      result = ProviderResult::error(esp_err_to_name(readErr));
    } else {
      result = parser.finish();
    }

    LOG_INFO("%d %s", status, result.isOk() ? getHttpResponsePhrase(status) : result.detail().c_str());
    ++attempts;
    if (!result.isOk()) {
      delay(100);
    }
  }

  if (!result.isOk()) {
    LOG_ERROR("Alerts API: %s", result.detail().c_str());
    alerts.clear();
  }
  LOG_DEBUG("fetch total=%u ms ok=%u detail='%s'", static_cast<unsigned>(millis() - t0), result.isOk(),
            result.detail().c_str());
  return result;
}  // MeteoAlarmAlertProvider::fetch

/* Add the current entry to the alerts if it has not expired yet and its
 * polygon, if any, contains the configured location (alerts without a
 * polygon are kept). Entries of the same hazard (e.g. separate time windows
 * or oblast clusters of one warning) are merged into the existing alert
 * rather than appended: the validity span becomes the union of both, and the
 * text keeps the color of the most urgent severity. Merged entries do not
 * count toward METEOALARM_NUM_ALERTS, so the cap is consumed by distinct
 * hazards only. */
void MeteoAlarmAlertProvider::FeedParser::addEntry() {
  if (!entry_.any) {
    return;
  }

  String hazard = hazardFromEvent(entry_.event);
  if (hazard.isEmpty()) {
    return;
  }

  const String color = colorFromSeverity(entry_.severity);
  weather_alert_t alert = {};
  alert.event = color.isEmpty() ? (hazard + " Warning") : (color + " " + hazard + " Warning");
  alert.start = parseIso8601(!entry_.onset.isEmpty() ? entry_.onset : entry_.effective);
  alert.end = parseIso8601(entry_.expires);

  // Skip warnings that have already expired, unless the clock is not
  // synchronized yet (epoch < 2021).
  if (alert.end > 0 && now_ > 1609459200LL && alert.end < now_) {
    return;
  }

  // Skip warnings whose polygon does not contain the configured location.
  if (!std::isnan(lat_) && !std::isnan(lon_) && !entry_.polygon.isEmpty() &&
      !pointInPolygon(lat_, lon_, entry_.polygon)) {
    return;
  }

  alert.tags = hazard;
  alert.tags.toLowerCase();

  // Merge same-hazard warnings: keep the most urgent color and expand the
  // validity span to the union of both time ranges. The 2-alert cap is
  // therefore filled with distinct hazards, not feed entries.
  for (weather_alert_t &a : alerts_) {
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

  alerts_.push_back(alert);
}  // MeteoAlarmAlertProvider::FeedParser::addEntry

/* Streaming XML scanner for the MeteoAlarm Atom feed, fed in chunks by
 * esp_http_client_read as the body is streamed. Each <entry> repeats the
 * CAP summary of a warning; only event, severity, effective, onset, expires
 * and polygon are captured, everything else is skipped, so the document is
 * never buffered in full.
 *
 * feed() becomes a no-op once METEOALARM_NUM_ALERTS matching warnings have
 * been collected; the caller checks isAlertCapReached() and closes the connection
 * without reading the remainder (see fetch()). */
void MeteoAlarmAlertProvider::FeedParser::feed(const char *data, size_t len) {
  if (alertCapReached_) {
    return;
  }

  for (size_t i = 0; i < len; ++i) {
    const char c = data[i];
    ++total_;
    switch (state_) {
      case St::TEXT: {
        if (c == '<') {
          state_ = St::TAG_NAME;
          tagName_ = "";
          endTag_ = false;
          selfClosing_ = false;
        } else if (c == '&') {
          entity_ = "";
          state_ = St::ENTITY;
        } else if (!capture_.isEmpty()) {
          text_ += c;
        }
        break;
      }
      case St::ENTITY: {
        if (c == ';') {
          if (entity_ == "amp") {
            text_ += '&';
          } else if (entity_ == "lt") {
            text_ += '<';
          } else if (entity_ == "gt") {
            text_ += '>';
          } else if (entity_ == "quot") {
            text_ += '"';
          } else if (entity_ == "apos") {
            text_ += '\'';
          } else {
            text_ += '&';
            text_ += entity_;
            text_ += ';';
          }
          state_ = St::TEXT;
        } else if (entity_.length() >= 8) {
          // too long to be a named entity, keep it as raw text
          text_ += '&';
          text_ += entity_;
          text_ += c;
          entity_ = "";
          state_ = St::TEXT;
        } else {
          entity_ += c;
        }
        break;
      }
      case St::TAG_NAME: {
        if (c == '?' || c == '!') {
          // <?xml ...?> declaration or <!DOCTYPE ...>/<!-- ... -->, skip
          state_ = St::SKIP;
        } else if (c == '/') {
          endTag_ = true;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          state_ = St::TAG_ATTR;
        } else if (c == '>') {
          // tag complete
          goto finish_tag;  // NOLINT(bugprone-branch-clone)
        } else {
          tagName_ += c;
        }
        break;
      }
      case St::TAG_ATTR: {
        if (c == '"' || c == '\'') {
          quote_ = c;
          state_ = St::TAG_ATTR_QUOTED;
        } else if (c == '/') {
          selfClosing_ = true;
        } else if (c == '>') {
          goto finish_tag;  // NOLINT(bugprone-branch-clone)
        }
        break;
      }
      case St::TAG_ATTR_QUOTED: {
        if (c == quote_) {
          state_ = St::TAG_ATTR;
        }
        break;
      }
      case St::SKIP: {
        if (c == '>') {
          state_ = St::TEXT;
        }
        break;
      }
    }

    continue;

  finish_tag:;
    state_ = St::TEXT;  // a finished tag is always followed by text
    const String local = tagName_.substring(tagName_.indexOf(':') + 1);

    if (endTag_) {
      if (local == "entry") {
        if (inEntry_) {
          addEntry();
          inEntry_ = false;
          entry_.reset();
          if (alerts_.size() >= METEOALARM_NUM_ALERTS) {
            alertCapReached_ = true;
            return;
          }
        }
      } else if (inEntry_ && local == capture_) {
        if (local == "event") {
          entry_.event = text_;
        } else if (local == "severity") {
          entry_.severity = text_;
        } else if (local == "effective") {
          entry_.effective = text_;
        } else if (local == "onset") {
          entry_.onset = text_;
        } else if (local == "expires") {
          entry_.expires = text_;
        } else if (local == "polygon") {
          entry_.polygon = text_;
        }
        entry_.any = true;
        capture_ = "";
        text_ = "";
      }
    } else {
      if (local == "entry") {
        if (inEntry_) {
          // previous entry closed implicitly by a new one
          addEntry();
          if (alerts_.size() >= METEOALARM_NUM_ALERTS) {
            alertCapReached_ = true;
            return;
          }
        }
        inEntry_ = true;
        entry_.reset();
        if (selfClosing_) {
          inEntry_ = false;
        }
      } else if (inEntry_ && (local == "event" || local == "severity" || local == "effective" || local == "onset" ||
                              local == "expires" || local == "polygon")) {
        capture_ = local;
        text_ = "";
      }
    }
  }
}  // MeteoAlarmAlertProvider::FeedParser::feed

/* Call once after the whole body has been fed (or the connection ended).
 * A body that ends while still inside an entry means it was truncated (e.g.
 * by a connection drop), unless the alert cap was already reached. */
ProviderResult MeteoAlarmAlertProvider::FeedParser::finish() {
  if (alertCapReached_) {
    LOG_DEBUG("feed: %u bytes -> %u alerts (cap reached) in %u ms", static_cast<unsigned>(total_),
              static_cast<unsigned>(alerts_.size()), static_cast<unsigned>(millis() - tStart_));
    return ProviderResult::ok();
  }

  if (inEntry_ || !capture_.isEmpty()) {
    LOG_WARNING("MeteoAlarm: feed ended early after %u bytes, scanner in state %d (inEntry=%u, capture='%s', "
                "tagName='%s')",
                static_cast<unsigned>(total_), static_cast<int>(state_), inEntry_, capture_.c_str(), tagName_.c_str());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
  }
  LOG_DEBUG("feed: %u bytes -> %u alerts in %u ms", static_cast<unsigned>(total_),
            static_cast<unsigned>(alerts_.size()), static_cast<unsigned>(millis() - tStart_));
  return ProviderResult::ok();
}  // MeteoAlarmAlertProvider::FeedParser::finish

#endif  // ALERTS_API_PROVIDER_METEOALARM
