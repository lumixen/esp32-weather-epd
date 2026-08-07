#pragma once

#include <vector>
#include "data_models.h"

/* Interface for national weather alert providers.
 *
 * Alerts may be served by the weather provider itself (when they ride along
 * in the weather response) or by a dedicated external provider.
 *
 * Returns the HTTP status code on success (HTTP_CODE_OK). Negative codes:
 * -512 - WiFi status offset when disconnected, -256 - JSON deserialization
 * error code offset.
 */
class AlertProvider {
 public:
  virtual ~AlertProvider() = default;
  virtual int fetch(std::vector<weather_alert_t> &alerts) = 0;
};
