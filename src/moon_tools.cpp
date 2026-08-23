/* Moon rise, set, and phase helpers for esp32-weather-epd.
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

moon_state_t getMoonState(float latitude, float longitude) { return getMoonState(latitude, longitude, time(nullptr)); }

moon_state_t getMoonState(float latitude, float longitude, time_t now) {
  MoonRise mr;
  mr.calculate(latitude, longitude, now);
  time_t moonrise = mr.riseTime;
  time_t moonset = mr.setTime;
  LOG_DEBUG("Moon rise azimuth: %s Moon set azimuth: %s", String(mr.riseAz).c_str(), String(mr.setAz).c_str());
  LOG_DEBUG("Moonrise: %ld Moonset: %ld", static_cast<long>(moonrise), static_cast<long>(moonset));
  if (mr.hasRise && moonset <= moonrise) {
    for (int step = 0; step < MAX_SEARCH_STEPS; ++step) {
      mr.calculate(latitude, longitude, moonrise + HALF_LUNAR_DAY + step * LUNAR_DAY);
      if (mr.hasSet && mr.setTime > moonrise) {
        moonset = mr.setTime;
        LOG_DEBUG("Moonset corrected to follow moonrise: %ld", static_cast<long>(moonset));
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
  LOG_DEBUG("Moon phase: %s", String(currentMoonPhase, 4).c_str());
  return moon_state_t{moonrise, moonset, currentMoonPhase};
}