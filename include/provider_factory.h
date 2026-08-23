/* Provider factory interface for esp32-weather-epd.
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
#include "air_quality_provider.h"
#include "alert_provider.h"
#include "data_models.h"
#include "fetch_operation.h"
#include "weather_provider.h"

/* Create the weather provider selected at compile time by WEATHER_API_*.
 * Returns nullptr if none is configured.
 */
WeatherProvider *createWeatherProvider();

/* Create the air quality provider selected at compile time by
 * AIR_QUALITY_API_*. Returns nullptr if none is configured.
 */
AirQualityProvider *createAirQualityProvider();

/* Create the alert provider, if any (standalone per ALERTS_API_*).
 * Returns nullptr when no alert source is available.
 * For OWM piggyback (WEATHER=OWM && ALERTS=OWM) use createProviders()
 * which aliases alert to weather correctly.
 */
AlertProvider *createAlertProvider();

struct ProviderBundle {
  std::shared_ptr<WeatherProvider> weather;
  std::shared_ptr<AirQualityProvider> airQuality;
  std::shared_ptr<AlertProvider> alert;
};

struct FetchBundle {
  ProviderBundle providers;
  std::vector<std::unique_ptr<FetchOperation>> ops;
};

ProviderBundle createProviders();
FetchBundle createFetchBundle(forecast_t &forecast, air_quality_t &airQuality, std::vector<weather_alert_t> &alerts);
