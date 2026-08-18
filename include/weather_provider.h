#pragma once

#include "data_models.h"
#include "provider_result.h"

/* Interface for weather forecast providers.
 *
 * Implementations are responsible for fetching provider-specific data and
 * mapping it into the generic forecast model. Each implementation owns its
 * own transport (WiFiClient / WiFiClientSecure) and opens and closes the
 * connection inside fetch().
 *
 * Returns ProviderResult::ok() on success. On failure, detail() holds a
 * already-localized message suitable for the error screen.
 */
class WeatherProvider {
 public:
  virtual ~WeatherProvider() = default;
  virtual const char *getApiName() const = 0;
  virtual ProviderResult fetch(forecast_t &forecast) = 0;
};
