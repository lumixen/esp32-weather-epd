/* Home Assistant MQTT client interface for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "config.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include "home_assistant_mqtt.h"

typedef struct {
  uint32_t batteryVoltage;
  uint8_t batteryPercentage;
  int8_t wifiRSSI;
  unsigned long apiActivityDuration;
  std::optional<float> temperature;
  std::optional<float> humidity;
  std::optional<float> pressure;
} mqtt_status_params_t;

void sendMQTTStatus(const mqtt_status_params_t &params);
#endif  // HOME_ASSISTANT_MQTT_ENABLED