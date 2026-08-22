/* Provider fetch adapters — wrap concrete providers into generic FetchOperation.
 * Copyright (C) 2026  Lumixen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <memory>
#include <vector>
#include "air_quality_provider.h"
#include "alert_provider.h"
#include "data_models.h"
#include "fetch_operation.h"
#include "weather_provider.h"

class WeatherFetchOperation : public FetchOperation {
 public:
  WeatherFetchOperation(WeatherProvider *provider, forecast_t &out) : provider_(provider), out_(out) {}
  ProviderResult execute() override { return provider_->fetch(out_); }
  const char *name() const override { return provider_->getApiName(); }
  bool shouldAbortOnFailure() const override { return true; }

 private:
  WeatherProvider *provider_;
  forecast_t &out_;
};

class AirQualityFetchOperation : public FetchOperation {
 public:
  AirQualityFetchOperation(AirQualityProvider *provider, air_quality_t &out) : provider_(provider), out_(out) {}
  ProviderResult execute() override { return provider_->fetch(out_); }
  const char *name() const override { return "Air Pollution API"; }
  bool shouldAbortOnFailure() const override { return true; }

 private:
  AirQualityProvider *provider_;
  air_quality_t &out_;
};

class AlertFetchOperation : public FetchOperation {
 public:
  AlertFetchOperation(AlertProvider *provider, std::vector<weather_alert_t> &out) : provider_(provider), out_(out) {}
  ProviderResult execute() override { return provider_->fetch(out_); }
  const char *name() const override { return "Alerts API"; }
  bool shouldAbortOnFailure() const override { return false; }

 private:
  AlertProvider *provider_;
  std::vector<weather_alert_t> &out_;
};

/* Build fetch operations in alerts-first order so alerts goes first into the
 * bounded pool (max 2 concurrent). Caller owns providers; operations borrow them.
 */
std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(
    WeatherProvider *weatherProvider, AirQualityProvider *airQualityProvider, AlertProvider *alertProvider,
    forecast_t &forecast, air_quality_t &airQuality, std::vector<weather_alert_t> &alerts);
