#include "config.h"
#include "moon_tools.h"

MoonPhase moonPhase;

// The MoonRise library returns the nearest rise and the nearest set to the
// query time independently, so when the query time falls after today's set
// and before tomorrow's rise, the returned set precedes the rise. The set
// that follows a rise is about half a lunar day later at moderate latitudes,
// but at polar latitudes the moon can stay up for days; the search therefore
// walks forward one lunar day at a time until the set comes within the
// library's 48h search window.
static constexpr time_t HALF_LUNAR_DAY = 45000;  // 12h30m
static constexpr time_t LUNAR_DAY = 89400;       // 24h50m
static constexpr int MAX_SEARCH_STEPS = 16;      // ~16 days: longest polar stretches

moon_state_t getMoonState(float latitude, float longitude) {
  return getMoonState(latitude, longitude, time(nullptr));
}

moon_state_t getMoonState(float latitude, float longitude, time_t now) {
  MoonRise mr;
  mr.calculate(latitude, longitude, now);
  time_t moonrise = mr.riseTime;
  time_t moonset = mr.setTime;
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] Moon rise azimuth: " + String(mr.riseAz) + " Moon set azimuth: " + String(mr.setAz));
  Serial.println("[debug] Moonrise: " + String(moonrise) + " Moonset: " + String(moonset));
#endif
  if (mr.hasRise && moonset <= moonrise) {
    for (int step = 0; step < MAX_SEARCH_STEPS; ++step) {
      mr.calculate(latitude, longitude, moonrise + HALF_LUNAR_DAY + step * LUNAR_DAY);
      if (mr.hasSet && mr.setTime > moonrise) {
        moonset = mr.setTime;
#if DEBUG_LEVEL >= 1
        Serial.println("[debug] Moonset corrected to follow moonrise: " + String(moonset));
#endif
        break;
      }
    }
  }
  moonData_t moon = moonPhase.getPhase(now);
  // Convert angle (0-360) to phase cycle (0.0-1.0)
  // 0 deg = 0.0 (New)
  // 90 deg = 0.25 (First Quarter)
  // 180 deg = 0.5 (Full)
  // 270 deg = 0.75 (Third Quarter)
  float currentMoonPhase = moon.angleDeg / 360.0f;
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] Moon phase: " + String(currentMoonPhase, 4));
#endif
  return moon_state_t{moonrise, moonset, currentMoonPhase};
}