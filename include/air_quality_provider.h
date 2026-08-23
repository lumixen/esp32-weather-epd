/* Air-quality provider interface for esp32-weather-epd.
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

#include "data_models.h"
#include "provider_result.h"

/* Interface for air quality providers.
 *
 * Implementations are responsible for fetching provider-specific data and
 * mapping it into the generic air quality model. Each implementation owns its
 * own transport (WiFiClient / WiFiClientSecure) and opens and closes the
 * connection inside fetch().
 *
 * Returns ProviderResult::ok() on success. On failure, detail() holds a
 * already-localized message suitable for the error screen.
 */
class AirQualityProvider {
 public:
  virtual ~AirQualityProvider() = default;
  virtual ProviderResult fetch(air_quality_t &airQuality) = 0;
};
