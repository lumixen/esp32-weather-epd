/* Unified remote provider factory for esp32-weather-epd.
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
#include "environment_sensor_fetch_operation.h"
#include "fetch_operation.h"
#include "remote_data_provider.h"
#include "weather_report.h"

struct ProviderBundle {
  std::vector<std::shared_ptr<RemoteDataProvider>> providers;
};

struct FetchBundle {
  ProviderBundle providers;
  std::vector<std::unique_ptr<FetchOperation>> ops;
};

ProviderBundle createProviders();
FetchBundle createFetchBundle(weather_report_t &report);
