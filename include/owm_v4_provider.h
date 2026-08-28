/* OpenWeatherMap One Call v4 provider.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <memory>
#include <vector>
#include "provider_result.h"
#include "remote_data_provider.h"

/* One Call API 4.0 exposes current conditions and forecast timelines as
 * independent resources. The provider deliberately keeps those requests
 * independent so a response cannot overwrite a section owned by another
 * operation. */
class OpenWeatherMapOneCallV4Provider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;

  ProviderResult fetchCurrent(forecast_t &forecast);
  ProviderResult fetchHourly(forecast_t &forecast);
  ProviderResult fetchDaily(forecast_t &forecast);

  static weather_condition mapWeatherCode(int id);
  static ProviderResult deserializeCurrent(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeHourly(Stream &json, forecast_t &forecast, size_t destinationOffset = 0);
  static ProviderResult deserializeDaily(Stream &json, forecast_t &forecast);
};
