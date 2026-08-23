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

#pragma once

#include <time.h>
#include <MoonRise.h>
#include <MoonPhase.hpp>

typedef struct moon_state {
  time_t moonrise;
  time_t moonset;
  float phase;  // 0.0 - 1.0
} moon_state_t;

moon_state_t getMoonState(float latitude, float longitude);
moon_state_t getMoonState(float latitude, float longitude, time_t now);
