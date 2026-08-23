/* Unified OWM One Call provider — single class for weather+alerts.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <vector>
#include "alert_provider.h"
#include "provider_result.h"
#include "weather_provider.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/* OpenWeatherMap "One Call" API provider — single class for both weather
 * and alerts. Handles piggyback (WEATHER=OWM && ALERTS=OWM) via internal
 * mutex so either fetch() can be called first and the other returns cache
 * without a second HTTP. Standalone alerts (WEATHER!=OWM) does
 * alerts-only request (exclude=current,minutely,hourly,daily).
 */
class OWMProvider : public WeatherProvider, public AlertProvider {
 public:
  OWMProvider();
  ~OWMProvider() override;
  const char *getApiName() const override;
  ProviderResult fetch(forecast_t &forecast) override;
  ProviderResult fetch(std::vector<weather_alert_t> &alerts) override;

  static weather_condition mapWeatherCode(int id);

 private:
  static ProviderResult deserializeOneCall(Stream &json, forecast_t &forecast, std::vector<weather_alert_t> *alerts);
  static ProviderResult deserializeAlerts(Stream &json, std::vector<weather_alert_t> &alerts);
  ProviderResult fetchInternal(forecast_t *forecast, std::vector<weather_alert_t> *alertsOut);

  std::vector<weather_alert_t> alerts_;
  bool haveAlerts_ = false;
  ProviderResult fetchStatus_;
  forecast_t cachedForecast_;
  bool fetched_ = false;
  SemaphoreHandle_t fetchMutex_ = nullptr;
};

using OWMWeatherProvider = OWMProvider;
using OWMAlertProvider = OWMProvider;
