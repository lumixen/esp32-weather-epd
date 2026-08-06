/* Alert provider interface for esp32-weather-epd.
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
#include <WiFiClient.h>
#include "data_models.h"

/* Interface for national weather alert providers.
 *
 * Alerts may be served by the weather provider itself (when they ride along
 * in the weather response) or by a dedicated external provider.
 *
 * Returns the HTTP status code on success (HTTP_CODE_OK). Negative codes:
 * -512 - WiFi status offset when disconnected, -256 - JSON deserialization
 * error code offset.
 */
class AlertProvider {
 public:
  virtual ~AlertProvider() = default;
  virtual int fetch(WiFiClient &client, std::vector<weather_alert_t> &alerts) = 0;
};
