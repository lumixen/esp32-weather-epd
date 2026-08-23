/* Provider-agnostic fetch result, shared by every provider and the HTTP
 * retry layer. Replaces numeric status codes (HTTP code + negative error
 * offsets) as the contract between providers, the HTTP layer and the
 * renderer, so no magic numbers ever reach the screen.
 *
 * Providers author the failure detail themselves, composing an
 * already-localized message from the TXT_* phrase table (e.g.
 * TXT_DESERIALIZATION_ERROR_*) plus an optional diagnostic suffix. The
 * numeric HTTP/WiFi status stays private to the fetch layer for logging.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */
#pragma once

#include <WString.h>

class ProviderResult {
 public:
  // Default is a non-ok "no result yet" state, so a default-initialized
  // value (e.g. the retry-loop accumulator in httpGetWithRetry or the
  // cached status of OWMWeatherProvider) is never mistaken for success.
  ProviderResult() : ok_(false), detail_("") {}
  static ProviderResult ok() { return ProviderResult(true, ""); }
  static ProviderResult error(const String &detail = "") { return ProviderResult(false, detail); }

  bool isOk() const { return ok_; }
  const String &detail() const { return detail_; }

 private:
  ProviderResult(bool ok, const String &detail) : ok_(ok), detail_(detail) {}

  bool ok_;
  String detail_;
};