/* MeteoAlarm alert provider interface for esp32-weather-epd.
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

#pragma once

#include <memory>
#include <vector>
#include "provider_result.h"
#include "remote_data_provider.h"

/* MeteoAlarm (EUMETNET) national weather alert provider.
 *
 * Fetches the legacy Atom feed of a country from feeds.meteoalarm.org. Each
 * feed entry repeats the CAP summary of a warning (event, severity, validity
 * period, area polygon), so no per-alert requests are required. The feed is
 * parsed incrementally as it is delivered via esp_http_client_read in
 * bounded chunks, only the fields needed for the alert model are kept. Once
 * METEOALARM_NUM_ALERTS distinct hazards are collected the connection is
 * closed without reading the remainder.
 */
class MeteoAlarmAlertProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override { return "MeteoAlarm API"; }
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;
  ProviderResult fetch(std::vector<weather_alert_t> &alerts);

  /* Incremental parser for the MeteoAlarm Atom feed. Bytes are fed in as
   * they arrive (e.g. from esp_http_client_read chunks) via feed(); collects
   * alerts that have not expired (`now` is the current Unix time) and cover
   * the optional location (lat/lon, NaN = no polygon filter; alerts without
   * a polygon are always kept). feed() becomes a no-op once
   * METEOALARM_NUM_ALERTS distinct-hazard warnings have been collected, at
   * which point isAlertCapReached() returns true and the caller should close
   * the connection without reading further. Call finish() exactly once after
   * the whole body has been fed (or the connection ended/closed early) to get
   * the final result: a body that ends while still inside an entry is
   * reported as truncated, unless the alert cap was already reached. Public
   * for unit testing. */
  class FeedParser {
   public:
    FeedParser(std::vector<weather_alert_t> &alerts, int64_t now, double lat = NAN, double lon = NAN);
    void feed(const char *data, size_t len);
    ProviderResult finish();
    bool isAlertCapReached() const { return alertCapReached_; }

   private:
    enum class St { TEXT, ENTITY, TAG_NAME, TAG_ATTR, TAG_ATTR_QUOTED, SKIP };

    // Data of the <entry> currently being parsed.
    struct EntryData {
      String event;
      String severity;
      String effective;
      String onset;
      String expires;
      String polygon;  // raw space-separated "lat,lon" ring, empty if absent
      bool any = false;

      void reset();
    };

    void addEntry();

    std::vector<weather_alert_t> &alerts_;
    int64_t now_;
    double lat_;
    double lon_;

    St state_ = St::TEXT;
    bool inEntry_ = false;
    bool endTag_ = false;
    bool selfClosing_ = false;
    bool alertCapReached_ = false;  // true once METEOALARM_NUM_ALERTS have been collected
    char quote_ = 0;
    String tagName_;  // tag currently being parsed
    String capture_;  // entry element currently accumulating text
    String text_;     // captured text so far
    String entity_;   // pending "&...;" reference
    EntryData entry_;

    size_t total_ = 0;  // bytes fed so far
    uint32_t tStart_ = 0;
  };

  /* Strip a leading color word ("yellow ", "orange ", "red ") and any
   * trailing "warning"/"watch"/"alert" suffix from an event name and return
   * the remaining hazard in title case, e.g.
   *   "Thunderstormwarning"              -> "Thunderstorm"
   *   "Yellow Rain Warning"              -> "Rain"
   *   "Orange High-temperature Warning"  -> "High-temperature" */
  static String hazardFromEvent(const String &event);

  /* Map the CAP severity to the MeteoAlarm awareness color ("", Yellow,
   * Orange or Red). Missing severity maps to an empty color. */
  static String colorFromSeverity(const String &severity);

  /* Returns true if (lat, lon) is inside the closed polygon given as
   * space-separated "lat,lon" pairs in WGS84 decimal degrees (CAP format,
   * ray casting). A polygon with fewer than 3 valid points can not be
   * evaluated and counts as containing every point (alert is kept); points
   * on the boundary count as inside. */
  static bool pointInPolygon(double lat, double lon, const String &polygon);
};
