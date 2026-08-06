/* OpenWeatherMap One Call weather provider for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>
#include <ArduinoJson.h>
#include "alert_provider.h"
#include "weather_provider.h"

/* OpenWeatherMap "One Call" API weather provider.
 *
 * Also implements AlertProvider: OpenWeatherMap serves national weather
 * alerts in the same OneCall response, so they are extracted during the
 * weather fetch and served from cache without an additional HTTP request.
 */
class OWMWeatherProvider : public WeatherProvider, public AlertProvider {
 public:
  int fetch(WiFiClient &client, forecast_t &forecast) override;
  int fetch(WiFiClient &client, std::vector<weather_alert_t> &alerts) override;

 private:
  static DeserializationError deserializeOneCall(Stream &json, forecast_t &forecast,
                                                 std::vector<weather_alert_t> *alerts);
  std::vector<weather_alert_t> cachedAlerts_;
  bool haveAlerts_ = false;
  int fetchStatus_ = -1;
};
