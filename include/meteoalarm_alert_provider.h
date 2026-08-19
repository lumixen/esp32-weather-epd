#pragma once

#include <vector>
#include <WiFiClient.h>
#include "alert_provider.h"
#include "provider_result.h"

/* MeteoAlarm (EUMETNET) national weather alert provider.
 *
 * Fetches the legacy Atom feed of a country from feeds.meteoalarm.org. Each
 * feed entry repeats the CAP summary of a warning (event, severity, validity
 * period, area polygon), so no per-alert requests are required. The feed is
 * parsed as a stream, only the fields needed for the alert model are kept.
 */
class MeteoAlarmAlertProvider : public AlertProvider {
 public:
  ProviderResult fetch(std::vector<weather_alert_t> &alerts) override;

  /* Stream the Atom feed and collect alerts that have not expired (`now` is
   * the current Unix time) and cover the optional location (lat/lon, NaN =
   * no polygon filter; alerts without a polygon are always kept). Parsing
   * stops after METEOALARM_NUM_ALERTS warnings; `expectedLen` (0 = unknown)
   * bounds the read. Public for unit testing. `networkClient` (optional), the
   * TLS client the response streams from, enables multi-KB block reads
   * (single-byte fallback when null, used by the unit test feeds). */
  static ProviderResult parseFeed(Stream &xml, std::vector<weather_alert_t> &alerts,
                                        int64_t now, double lat = NAN, double lon = NAN,
                                        size_t expectedLen = 0, NetworkClient *networkClient = nullptr);

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
