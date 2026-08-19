/* Shared mapping from ArduinoJson's DeserializationError to ProviderResult,
 * the single source of truth for the DOM-based providers: each failure kind
 * maps 1:1 to its localized phrase, so every provider produces identical
 * wording for the same parse failure.
 *
 * GPL-3.0, see LICENSE.
 */
#pragma once

#include <ArduinoJson.h>

#include "_locale.h"
#include "provider_result.h"

/* Map ArduinoJson's DeserializationError to ProviderResult, 1:1. The message
 * is composed from the phrase table, so each failure kind keeps its
 * localized wording no matter which provider produced it. */
static inline ProviderResult mapDeserializationError(DeserializationError code) {
  switch (code.code()) {
    case DeserializationError::Code::Ok:
      return ProviderResult::ok();
    case DeserializationError::Code::EmptyInput:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
    case DeserializationError::Code::IncompleteInput:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
    case DeserializationError::Code::InvalidInput:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    case DeserializationError::Code::NoMemory:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_NO_MEMORY);
    case DeserializationError::Code::TooDeep:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_TOO_DEEP);
    default:
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
}