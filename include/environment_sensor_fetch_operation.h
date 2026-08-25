/* Local environment-sensor fetch operation for esp32-weather-epd.
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

#include "env_sensor.h"
#include "fetch_operation.h"
#include "weather_report.h"

/* A self-contained operation so the sensor and its implementation remain
 * alive until an asynchronous executor has finished using them. Keeping the
 * injected sensor in this class also makes the hardware-independent behavior
 * testable without a physical I2C device. */
class EnvironmentSensorFetchOperation final : public FetchOperation {
 public:
  EnvironmentSensorFetchOperation(weather_report_t &report, std::unique_ptr<EnvSensor> sensor);

  ProviderResult execute() override;
  const char *name() const override;
  bool shouldAbortOnFailure() const override;

 private:
  weather_report_t &report_;
  std::unique_ptr<EnvSensor> sensor_;
};

/* Returns the configured local operation, or nullptr when no local sensor is
 * configured. The operation class itself is available for tests even in a
 * BME_TYPE_NONE build. */
std::unique_ptr<FetchOperation> createEnvironmentSensorOperation(weather_report_t &report);
std::vector<std::unique_ptr<FetchOperation>> createEnvironmentSensorOperations(weather_report_t &report);
