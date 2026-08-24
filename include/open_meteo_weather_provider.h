/* Open-Meteo weather provider interface for esp32-weather-epd.
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

/* Open-Meteo forecast API weather provider. */
class OpenMeteoForecastProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;
  ProviderResult fetch(forecast_t &forecast);

  /* Map a WMO weather interpretation code onto the unified weather_condition
   * enum. Public for unit testing. */
  static weather_condition mapWeatherCode(int id);

  /* Map a streamed JSON response of the Open-Meteo forecast API into the
   * generic forecast model. Public for unit testing. */
  static ProviderResult deserializeCall(Stream &json, forecast_t &forecast);
};

/* Source compatibility for fixture code while the public provider name is
 * the capability-oriented OpenMeteoForecastProvider. */
using OpenMeteoWeatherProvider = OpenMeteoForecastProvider;
