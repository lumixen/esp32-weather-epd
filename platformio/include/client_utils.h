/* Client side utility declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#include <Arduino.h>
#include <WiFiClient.h>
#include "api_response.h"
#include "config.h"
#ifdef HOME_ASSISTANT_MQTT_ENABLED
#include <PubSubClient.h>
#endif

wl_status_t startWiFi(int &wifiRSSI);
void killWiFi();
bool waitForSNTPSync(tm *timeInfo);
bool printLocalTime(tm *timeInfo);

int getOWMonecall(WiFiClient &client, environment_data_t &r);
int getOMCall(WiFiClient &client, environment_data_t &r);
int getAirPollution(WiFiClient &client, air_pollution_t &r);

#ifdef HOME_ASSISTANT_MQTT_ENABLED
void sendMQTTStatus(uint32_t batteryVoltage, uint8_t batteryPercentage, int8_t wifiRSSI,
                    unsigned long networkActivityDuration);
#endif