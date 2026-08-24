/* Unified weather report for remote and locally produced rendering data.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <optional>
#include <vector>
#include "data_models.h"
#include "moon_tools.h"

/* All data consumed by the weather renderer. Forecast data is mandatory;
 * optional groups are engaged only when a provider successfully supplies the
 * corresponding data. */
struct weather_report_t {
  forecast_t forecast;
  std::optional<air_quality_t> air_quality;
  std::optional<std::vector<weather_alert_t>> alerts;
  sensor_readings sensor;
  moon_state_t moon{};

  void resetForecast() { forecast.reset(); }
  void resetAirQuality() { air_quality.reset(); }
  void resetAlerts() { alerts.reset(); }
  air_quality_t &engageAirQuality() { return air_quality.emplace(); }
  std::vector<weather_alert_t> &engageAlerts() { return alerts.emplace(); }
};
