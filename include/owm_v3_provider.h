/* OpenWeatherMap One Call v3 provider.
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

/* One One Call v3 request supplies the mandatory forecast and, when present,
 * the optional alerts group. There is deliberately no cache or fetch-order
 * dependent piggyback behavior: the provider owns one combined operation. */
class OpenWeatherMapOneCallV3Provider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;
  ProviderResult fetch(weather_report_t &report);

  static weather_condition mapWeatherCode(int id);
  static ProviderResult deserializeOneCall(Stream &json, weather_report_t &report);
};

using OWMProvider = OpenWeatherMapOneCallV3Provider;
using OWMWeatherProvider = OpenWeatherMapOneCallV3Provider;
using OWMAlertProvider = OpenWeatherMapOneCallV3Provider;
