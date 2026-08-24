/* Sun rise and set helpers for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sun_tools.h"
#include "logger.h"

#include <SunRise.h>
#include <cmath>

namespace {

/* SunRise returns the nearest rise and set independently. Querying at local
 * noon keeps both events paired to the same civil day for ordinary locations,
 * just as the provider values previously used by the display were. */
time_t localNoon(time_t now) {
  tm date{};
  if (localtime_r(&now, &date) == nullptr) {
    return 0;
  }
  date.tm_hour = 12;
  date.tm_min = 0;
  date.tm_sec = 0;
  date.tm_isdst = -1;
  return mktime(&date);
}

}  // namespace

sun_state_t getSunState(float latitude, float longitude) { return getSunState(latitude, longitude, time(nullptr)); }

sun_state_t getSunState(float latitude, float longitude, time_t now) {
  if (!std::isfinite(latitude) || latitude < -90.0f || latitude > 90.0f || !std::isfinite(longitude) ||
      longitude < -180.0f || longitude > 180.0f) {
    LOG_WARNING("Invalid solar coordinates: latitude %.6f, longitude %.6f", latitude, longitude);
    return sun_state_t{0, 0};
  }

  const time_t query = localNoon(now);
  if (query == 0) {
    LOG_WARNING("Failed to determine local noon for solar calculation");
    return sun_state_t{0, 0};
  }

  SunRise calculator;
  calculator.calculate(latitude, longitude, query);
  return sun_state_t{calculator.hasRise ? calculator.riseTime : 0, calculator.hasSet ? calculator.setTime : 0};
}
