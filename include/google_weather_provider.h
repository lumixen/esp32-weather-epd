/* Google Weather API forecast provider for esp32-weather-epd.
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

/* Google Weather API current conditions and independent hourly/daily
 * forecast resources, mapped into the shared forecast model. */
class GoogleWeatherForecastProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;

  static weather_condition mapWeatherCondition(const char *type);
  static ProviderResult deserializeCurrent(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeHourly(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeDaily(Stream &json, forecast_t &forecast);

 private:
  ProviderResult fetchForecast(forecast_t &forecast);
};
