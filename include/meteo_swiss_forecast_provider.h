/* MeteoSwiss App forecast provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 */
#pragma once

#include <Arduino.h>
#include <memory>
#include <vector>
#include "provider_result.h"
#include "remote_data_provider.h"

/* Forecast provider using the MeteoSwiss App local forecast point and the
 * official SwissMetNet current-observation CSV. */
class MeteoSwissForecastProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;

  static weather_condition mapWeatherCode(int code);
  static bool isDayIcon(int code);

  /* Public fixture seams. The forecast parser resets its destination before
   * parsing and again after every unsuccessful parse. The CSV parser overlays
   * values present in its matching station row and leaves unrelated fields
   * untouched, which lets the caller preserve forecast-derived conditions. */
  static ProviderResult deserializeForecast(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeCall(Stream &json, forecast_t &forecast) {
    return deserializeForecast(json, forecast);
  }
  static ProviderResult deserializeObservationCsv(Stream &csv, const String &stationId, current_t &current);
  static ProviderResult deserializeObservationCsv(Stream &csv, current_t &current, const String &stationId) {
    return deserializeObservationCsv(csv, stationId, current);
  }

 private:
  ProviderResult fetchForecast(forecast_t &forecast);
  ProviderResult fetchObservation(current_t &current);
};
