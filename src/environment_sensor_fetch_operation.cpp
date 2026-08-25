/* Local environment-sensor fetch operation for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "environment_sensor_fetch_operation.h"

#include "logger.h"

#if defined(LOCAL_PROVIDER_BME280)
#include "env_sensor_bme280.h"
#endif

EnvironmentSensorFetchOperation::EnvironmentSensorFetchOperation(weather_report_t &report,
                                                                 std::unique_ptr<EnvSensor> sensor)
    : report_(report), sensor_(std::move(sensor)) {}

ProviderResult EnvironmentSensorFetchOperation::execute() {
  // Do not publish individual getter results. In addition to avoiding stale
  // values on a failed read, this makes report.sensor a single publication
  // point for readers running after the operation has completed.
  if (!sensor_) {
    report_.sensor = {};
    return ProviderResult::error("Environment sensor is unavailable");
  }

  if (!sensor_->begin()) {
    sensor_->shutdown();
    report_.sensor = {};
    LOG_WARNING("Environment sensor initialization failed");
    return ProviderResult::error("Failed to initialize environment sensor");
  }

  sensor_readings readings{
      .temperature = sensor_->getTemperature(),
      .humidity = sensor_->getHumidity(),
      .pressure = sensor_->getPressure(),
  };
  // The operation remains alive for executor lifetime safety, but the
  // hardware no longer needs to remain powered after the readings are taken.
  sensor_->shutdown();
  report_.sensor = readings;

  LOG_INFO("Temp: %s°C, Humidity: %s%%, Pressure: %s hPa", String(readings.temperature.value_or(NAN)).c_str(),
           String(readings.humidity.value_or(NAN)).c_str(), String(readings.pressure.value_or(NAN)).c_str());
  return ProviderResult::ok();
}

const char *EnvironmentSensorFetchOperation::name() const { return "Environment sensor"; }

bool EnvironmentSensorFetchOperation::shouldAbortOnFailure() const { return false; }

std::unique_ptr<FetchOperation> createEnvironmentSensorOperation(weather_report_t &report) {
#if defined(LOCAL_PROVIDER_BME280)
  return std::make_unique<EnvironmentSensorFetchOperation>(report, std::make_unique<BME280EnvSensor>());
#else
  (void) report;
  return nullptr;
#endif
}
