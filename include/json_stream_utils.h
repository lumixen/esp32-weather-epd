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

#include "_locale.h"
#include "logger.h"
#include "provider_result.h"

/* Consume a JSON document in bounded chunks, feeding parser callbacks one byte
 * at a time. The bounded read avoids one transport read per JSON byte, while
 * the byte-wise feed lets callers stop cleanly at endDocument() when a read
 * also contains bytes after the root value. */
template <typename Complete, typename Started>
static ProviderResult consumeJsonStream(Stream &json, JsonHandler &handler, Complete complete, Started started,
                                        const char *label, bool skipLeadingWhitespace = false) {
  ArduinoStreamParser parser;
  parser.setHandler(&handler);
  uint8_t buffer[256];
  while (!parser.hasParseError() && !complete()) {
    const size_t count = json.readBytes(buffer, sizeof(buffer));
    if (count == 0)
      break;
    for (size_t i = 0; i < count && !parser.hasParseError() && !complete(); ++i) {
      if (skipLeadingWhitespace && !started() &&
          (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n' || buffer[i] == '\r'))
        continue;
      parser.write(buffer + i, 1);
    }
  }
  if (parser.hasParseError()) {
    LOG_WARNING("%s JSON parse error: %s", label, parser.getErrorMessage());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (complete())
    return ProviderResult::ok();
  return ProviderResult::error(started() ? TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT
                                         : TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
}
