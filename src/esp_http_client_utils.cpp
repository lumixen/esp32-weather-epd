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

#include "esp_http_client_utils.h"

#include <WiFi.h>
#include "_locale.h"
#include "display_utils.h"
#include "logger.h"

namespace {

constexpr int kHttpStatusOk = 200;
constexpr int kMaxAttempts = 3;
constexpr uint32_t kRetryDelayMs = 100;

}  // namespace

ProviderResult espHttpGetWithRetry(const String &url, const String &sanitizedUrl, esp_http_client_config_t config,
                                   EspHttpResponseHandler handleResponse) {
  LOG_INFO("%s: %s", TXT_ATTEMPTING_HTTP_REQ, sanitizedUrl.c_str());

  ProviderResult result;
  int status = 0;
  for (int attempt = 0; !result.isOk() && attempt < kMaxAttempts; ++attempt) {
    const wl_status_t connectionStatus = WiFi.status();
    if (connectionStatus != WL_CONNECTED) {
      // Keep the -512 offset private to the HTTP phrase table, matching the
      // Arduino HTTP retry helper's WiFi error behavior.
      return ProviderResult::error(getHttpResponsePhrase(-512 - static_cast<int>(connectionStatus)));
    }

    // esp_http_client_config_t contains pointers, so copy the caller's
    // options for each attempt but point the URL at the still-live String
    // owned by the caller.
    esp_http_client_config_t attemptConfig = config;
    attemptConfig.url = url.c_str();
    attemptConfig.method = HTTP_METHOD_GET;

    esp_http_client_handle_t client = esp_http_client_init(&attemptConfig);
    bool opened = false;
    if (client == nullptr) {
      status = 0;
      result = ProviderResult::error(esp_err_to_name(ESP_FAIL));
    } else {
      const esp_err_t openError = esp_http_client_open(client, 0);
      if (openError != ESP_OK) {
        status = 0;
        result = ProviderResult::error(esp_err_to_name(openError));
      } else {
        opened = true;
        // A negative/no-content-length result is valid for some
        // close-delimited or chunked responses. The status code is the
        // authoritative response validation; a non-positive status still
        // indicates that the headers could not be read.
        (void) esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (status != kHttpStatusOk) {
          result = ProviderResult::error(status > 0 ? getHttpResponsePhrase(status)
                                                    : esp_err_to_name(ESP_ERR_HTTP_FETCH_HEADER));
        } else if (!handleResponse) {
          result = ProviderResult::error(esp_err_to_name(ESP_ERR_INVALID_ARG));
        } else {
          result = handleResponse(client);
        }
      }

      if (opened) {
        esp_http_client_close(client);
      }
      esp_http_client_cleanup(client);
    }

    LOG_INFO("%d %s", status, result.isOk() ? getHttpResponsePhrase(status) : result.detail().c_str());
    if (!result.isOk() && attempt + 1 < kMaxAttempts) {
      delay(kRetryDelayMs);
    }
  }

  return result;
}  // espHttpGetWithRetry
