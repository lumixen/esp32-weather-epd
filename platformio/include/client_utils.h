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

#include <functional>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "config.h"

wl_status_t startWiFi(int8_t &wifiRSSI);
void killWiFi();

/* Perform an HTTP GET request with retry.
 *
 * Returns the HTTP status code on success (HTTP_CODE_OK). Negative codes:
 * -512 - WiFi status offset when disconnected, -256 - JSON deserialization
 * error code offset.
 *
 * The `parse` callback is invoked with the response stream once the request
 * succeeds and is responsible for deserializing and mapping the provider
 * response into the output model. `expectedLen` is the response content
 * length in bytes (0 if unknown); providers may use it to stop reading
 * exactly at the end of the body instead of reading until EOF.
 *
 * `timeoutMs` sets both the TCP connect timeout and the read timeout of the
 * response stream. Providers with large responses (e.g. the MeteoAlarm feed
 * is hundreds of KB) must pass a value large enough to stream the whole body.
 */
int httpGetWithRetry(WiFiClient &client, const String &host, uint16_t port, const String &uri,
                     const String &sanitizedUri, bool useHttp10, uint32_t timeoutMs,
                     std::function<DeserializationError(Stream &, size_t expectedLen)> parse);