#pragma once

#include "data_models.h"

/* Interface for weather forecast providers.
 *
 * Implementations are responsible for fetching provider-specific data and
 * mapping it into the generic forecast model. Each implementation owns its
 * own transport (WiFiClient / WiFiClientSecure) and opens and closes the
 * connection inside fetch().
 *
 * Returns the HTTP status code on success (HTTP_CODE_OK). Negative codes:
 * -512 - WiFi status offset when disconnected, -256 - JSON deserialization
 * error code offset.
 */
class WeatherProvider {
 public:
  virtual ~WeatherProvider() = default;
  virtual const char *getApiName() const = 0;
  virtual int fetch(forecast_t &forecast) = 0;
};
