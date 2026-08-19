#pragma once

#include <vector>
#include "data_models.h"
#include "provider_result.h"

/* Interface for national weather alert providers.
 *
 * Alerts may be served by the weather provider itself (when they ride along
 * in the weather response) or by a dedicated external provider.
 *
 * Returns ProviderResult::ok() on success. On failure, detail() holds a
 * already-localized message suitable for the error screen.
 */
class AlertProvider {
 public:
  virtual ~AlertProvider() = default;
  virtual ProviderResult fetch(std::vector<weather_alert_t> &alerts) = 0;
};
