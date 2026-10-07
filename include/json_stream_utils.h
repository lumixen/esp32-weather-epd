/* Shared helpers for feeding json-streaming-parser2 from Arduino Streams.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <ArduinoStreamParser.h>
#include <utility>

#include "_locale.h"
#include "logger.h"
#include "provider_result.h"

/* Incremental JSON document parser shared by pull-based Streams and push-based
 * HTTP data callbacks. `feed()` accepts arbitrary chunk boundaries and keeps
 * parser state between calls. */
template<typename Complete, typename Started> class JsonStreamFeeder {
 public:
  JsonStreamFeeder(JsonHandler &handler, Complete complete, Started started, const char *label,
                   bool skipLeadingWhitespace = false)
      : complete_(std::move(complete)),
        started_(std::move(started)),
        label_(label),
        skipLeadingWhitespace_(skipLeadingWhitespace) {
    parser_.setHandler(&handler);
  }

  void feed(const uint8_t *buffer, size_t length) {
    if (buffer == nullptr || length == 0)
      return;

    for (size_t i = 0; i < length && !parser_.hasParseError() && !complete_(); ++i) {
      if (skipLeadingWhitespace_ && !started_() && isWhitespace(buffer[i]))
        continue;
      parser_.write(buffer + i, 1);
    }
  }

  bool shouldReadMore() const { return !parser_.hasParseError() && !complete_(); }

  ProviderResult finish() const {
    if (parser_.hasParseError()) {
      LOG_WARNING("%s JSON parse error: %s", label_, parser_.getErrorMessage());
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    }
    if (complete_())
      return ProviderResult::ok();
    return ProviderResult::error(started_() ? TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT
                                            : TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  }

 private:
  static bool isWhitespace(uint8_t byte) { return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r'; }

  ArduinoStreamParser parser_;
  Complete complete_;
  Started started_;
  const char *label_;
  bool skipLeadingWhitespace_;
};

/* Consume a JSON document in bounded chunks. Push-based callers can also feed
 * response chunks directly to JsonStreamFeeder without buffering the body. */
template<typename Complete, typename Started>
inline ProviderResult consumeJsonStream(Stream &json, JsonHandler &handler, Complete complete, Started started,
                                        const char *label, bool skipLeadingWhitespace = false) {
  JsonStreamFeeder parser(
      handler, [&complete]() { return complete(); }, [&started]() { return started(); }, label, skipLeadingWhitespace);
  uint8_t buffer[256];
  while (parser.shouldReadMore()) {
    const size_t count = json.readBytes(buffer, sizeof(buffer));
    if (count == 0)
      break;
    parser.feed(buffer, count);
  }
  return parser.finish();
}
