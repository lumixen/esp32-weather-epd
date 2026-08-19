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
#include <WiFi.h>
#include <WiFiClient.h>
#include "config.h"
#include "provider_result.h"

wl_status_t startWiFi(int8_t &wifiRSSI);
void killWiFi();

/* Perform an HTTP GET request with retry.
 *
 * Returns ProviderResult::ok() once the response was received and parsed
 * successfully. On failure, the detail is already localized: HTTP/WiFi
 * errors are phrased from getHttpResponsePhrase() by this function, and
 * parse failures carry the message the `parse` callback returned.
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
ProviderResult httpGetWithRetry(WiFiClient &client, const String &host, uint16_t port, const String &uri,
                                const String &sanitizedUri, bool useHttp10, uint32_t timeoutMs,
                                std::function<ProviderResult(Stream &, size_t expectedLen)> parse);

/* Buffered input stream adapter feeding a streaming JSON parser (such as
 * rapidjson's SAX reader) from an Arduino Stream, e.g. the live HTTP
 * response body of an HTTPClient. The underlying stream is read in
 * 64-byte chunks, so each refill issues a single bulk read instead of one
 * call per byte. EOF is signalled by '\0', the rapidjson convention.
 *
 * Implements the rapidjson InputStream concept: Ch, Peek(), Take(), Tell()
 * and the in-place editing hooks PutBegin()/Put()/PutEnd() (only reached
 * with kParseInsituFlag/kParseNumbersAsStringsFlag, which streaming parsers
 * never enable, but the hooks must exist for the concept to compile).
 */
class StreamInput {
 public:
  typedef char Ch;

  explicit StreamInput(Stream &stream) : stream_(stream), pos_(0), len_(0), total_(0) {}

  char Peek() {
    if (pos_ >= len_ && !fill()) {
      reachedEof_ = true;
      return '\0';
    }
    return buffer_[pos_];
  }

  char Take() {
    if (pos_ >= len_ && !fill()) {
      reachedEof_ = true;
      return '\0';
    }
    ++total_;
    return buffer_[pos_++];
  }

  size_t Tell() { return total_; }

  /* True once the underlying stream returned no more bytes (seen by either
   * Peek() or Take()). Lets parsers classify a parse error as premature end
   * of input (IncompleteInput) instead of invalid syntax. */
  bool reachedEof() const { return reachedEof_; }

  // In-place editing hooks. Only reached with
  // kParseNumbersAsStringsFlag/kParseInsituFlag, which streaming parsers
  // never enable, but the unused branches still get compiled, so the hooks
  // must exist with the concept's signatures (InsituStringStream semantics).
  Ch *PutBegin() { return scratch_; }
  void Put(Ch c) {
    if (scratchLen_ < sizeof(scratch_)) {
      scratch_[scratchLen_++] = c;
    }
  }
  size_t PutEnd(Ch *) {
    const size_t n = scratchLen_;
    scratchLen_ = 0;
    return n;
  }

 private:
  bool fill() {
    const size_t n = stream_.readBytes(buffer_, sizeof(buffer_));
    pos_ = 0;
    len_ = n;
    return n > 0;
  }

  Stream &stream_;
  char buffer_[64];
  char scratch_[64];
  size_t scratchLen_ = 0;
  size_t pos_;
  size_t len_;
  size_t total_;
  bool reachedEof_ = false;
};