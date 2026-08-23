/* Provider fetch adapters — wrap concrete providers into generic FetchOperation.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "provider_fetch_operations.h"

std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(WeatherProvider *weatherProvider,
                                                                   AirQualityProvider *airQualityProvider,
                                                                   AlertProvider *alertProvider, forecast_t &forecast,
                                                                   air_quality_t &airQuality,
                                                                   std::vector<weather_alert_t> &alerts) {
  std::vector<std::unique_ptr<FetchOperation>> ops;
  // Alerts first so it goes first into the bounded pool (max 2)
  if (alertProvider) {
    ops.push_back(std::make_unique<AlertFetchOperation>(alertProvider, alerts));
  }
  if (weatherProvider) {
    ops.push_back(std::make_unique<WeatherFetchOperation>(weatherProvider, forecast));
  }
  if (airQualityProvider) {
    ops.push_back(std::make_unique<AirQualityFetchOperation>(airQualityProvider, airQuality));
  }
  return ops;
}
