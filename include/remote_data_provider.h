/* Unified remote data provider interface for esp32-weather-epd.
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
#include "weather_report.h"

class FetchOperation;

/* A provider may return one or more independent operations. The returned
 * operations borrow the provider and write directly into their assigned
 * groups in the report; the fetch bundle keeps providers alive. */
class RemoteDataProvider {
 public:
  virtual ~RemoteDataProvider() = default;
  virtual const char *getApiName() const = 0;
  virtual std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) = 0;
};
