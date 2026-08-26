/* ESP-IDF HTTP client utilities for esp32-weather-epd.
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

#include <functional>
#include <Arduino.h>
#include "esp_http_client.h"
#include "provider_result.h"

/* Called after a successful GET and HTTP 200 response. The callback owns
 * response-body consumption and parsing, but must not close or clean up the
 * client; espHttpGetWithRetry() does that on every path. */
using EspHttpResponseHandler = std::function<ProviderResult(esp_http_client_handle_t)>;

/* Decode a negative esp_http_client_read() result. Known -ESP_ERR_HTTP_*
 * values are converted to their esp_err_t, while the generic -1 return and
 * unknown negative values become ESP_FAIL. Callers may handle
 * -ESP_ERR_HTTP_EAGAIN separately when a timeout should be treated as an
 * end-of-read condition. */
esp_err_t espHttpReadError(int result);

/* Perform an ESP-IDF HTTP GET request with the common WiFi/status/retry and
 * client lifecycle handling. `config` is copied for each attempt; its URL
 * and method are replaced with the supplied URL and HTTP_METHOD_GET. The
 * caller may configure TLS, timeout, buffers, redirects, and other
 * esp_http_client options before passing it here.
 *
 * The response handler is called only for HTTP 200 responses. It may read
 * the body with esp_http_client_read(), stop early, or return a parse/read
 * error. Failed attempts are retried up to three times. */
ProviderResult espHttpGetWithRetry(const String &url, const String &sanitizedUrl, esp_http_client_config_t config,
                                   EspHttpResponseHandler handleResponse);
