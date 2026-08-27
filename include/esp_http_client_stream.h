/* Arduino Stream adapter for an ESP-IDF HTTP client response.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 */
#pragma once

#include <Arduino.h>
#include "esp_http_client.h"
#include "esp_http_client_utils.h"

/* Stream the response body directly from the bounded ESP-IDF client buffer;
 * no complete response is copied into RAM. */
class EspHttpClientStream : public Stream {
 public:
  explicit EspHttpClientStream(esp_http_client_handle_t client) : client_(client) {}

  int available() override { return 1; }
  int read() override {
    uint8_t byte = 0;
    return readBytes(&byte, 1) == 1 ? byte : -1;
  }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 0; }
  size_t readBytes(char *buffer, size_t length) override {
    if (length == 0)
      return 0;
    const int count = esp_http_client_read(client_, buffer, static_cast<int>(length));
    if (count < 0)
      readError_ = espHttpReadError(count);
    return count > 0 ? static_cast<size_t>(count) : 0;
  }

  bool hadReadError() const { return readError_ != ESP_OK; }
  esp_err_t readError() const { return readError_; }
  size_t readBytes(uint8_t *buffer, size_t length) { return readBytes(reinterpret_cast<char *>(buffer), length); }

 private:
  esp_http_client_handle_t client_;
  esp_err_t readError_ = ESP_OK;
};
