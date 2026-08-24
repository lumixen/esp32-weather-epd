/* OpenWeatherMap air-quality provider interface for esp32-weather-epd.
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

/* OpenWeatherMap "Air Pollution" API air quality provider. */
class OpenWeatherMapAirQualityProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override { return "Air Pollution API"; }
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;
  ProviderResult fetch(air_quality_t &airQuality);

  /* Map an OWM Air Pollution response into the generic air-quality model.
   * Public for offline fixture-based unit testing. */
  static ProviderResult deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};

using OWMAirQualityProvider = OpenWeatherMapAirQualityProvider;
