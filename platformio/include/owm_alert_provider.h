/* OpenWeatherMap alert provider for esp32-weather-epd.
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

/* OpenWeatherMap "One Call" API alert provider.
 *
 * Used when the weather provider is not OpenWeatherMap, so alerts must be
 * fetched with their own request. When the weather provider is
 * OpenWeatherMap, alerts ride along in the weather response instead and
 * OWMWeatherProvider serves them from cache.
 */
class OWMAlertProvider : public AlertProvider {
 public:
  int fetch(WiFiClient &client, std::vector<weather_alert_t> &alerts) override;

 private:
  static DeserializationError deserializeAlerts(Stream &json, std::vector<weather_alert_t> &alerts);
};
