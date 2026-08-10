#pragma once

#include <vector>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include "alert_provider.h"

/* MeteoAlarm (EUMETNET) national weather alert provider.
 *
 * Fetches the legacy Atom feed of a country from feeds.meteoalarm.org. Each
 * feed entry repeats the CAP summary of a warning (event, severity, validity
 * period, area polygon), so no per-alert requests are required. The feed is
 * parsed as a stream, only the fields needed for the alert model are kept.
 */
class MeteoAlarmAlertProvider : public AlertProvider {
 public:
  int fetch(std::vector<weather_alert_t> &alerts) override;

  /* Stream the Atom feed and collect the active alerts. `now` is the current
   * Unix time used to drop already expired warnings.
   *
   * An optional location (`lat`/`lon`, e.g. parsed from D_LATITUDE/
   * D_LONGITUDE) filters warnings by their geographic polygon: an alert whose
   * polygon does not contain the location is dropped. Pass NaN (default) to
   * disable the polygon filter; alerts without a polygon are always kept.
   *
   * Parsing stops once METEOALARM_NUM_ALERTS (2, what the renderer displays)
   * matching warnings have been collected; the remaining body is not read.
   * `expectedLen` is the response content length in bytes (0 = unknown): when
   * it is reached the body is considered fully consumed and no further read is
   * attempted, so a connection closed by the server right after the response
   * does not surface as a read error.
   *
   * Public for unit testing; the provider itself passes the system clock and
   * the configured location. `bulkClient` (optional) is the TLS client the
   * response streams from: its block read() serves the body in multi-KB
   * chunks from mbedTLS's plaintext buffer, far cheaper per byte than the
   * single-byte Stream fallback used when it is null (unit test feeds).
   */
  static DeserializationError parseFeed(Stream &xml, std::vector<weather_alert_t> &alerts,
                                        int64_t now, double lat = NAN, double lon = NAN,
                                        size_t expectedLen = 0, NetworkClient *bulkClient = nullptr);

  /* Parse an ISO 8601 timestamp ("YYYY-MM-DDTHH:MM:SSZ" or "±HH:MM") to Unix
   * epoch seconds in UTC. Returns -1 on failure. */
  static int64_t parseIso8601(const String &s);

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
