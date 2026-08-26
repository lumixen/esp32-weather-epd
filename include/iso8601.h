/* ISO 8601 timestamp parsing for esp32-weather-epd.
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
#pragma once

#include <cstdint>

namespace iso8601 {

/* Parse a common RFC 3339/ISO 8601 timestamp into Unix epoch seconds.
 *
 * Supported forms are YYYY-MM-DDTHH:MM:SS[.fraction]Z and
 * YYYY-MM-DDTHH:MM:SS[.fraction]+/-HH:MM. Fractional seconds are accepted
 * but discarded because the application stores timestamps at one-second
 * resolution. Returns false when value is null, malformed, or invalid. */
bool parse(const char *value, int64_t &epoch);

}  // namespace iso8601
