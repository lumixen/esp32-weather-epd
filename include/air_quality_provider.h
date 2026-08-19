#pragma once

#include "data_models.h"
#include "provider_result.h"

/* Interface for air quality providers.
 *
 * Implementations are responsible for fetching provider-specific data and
 * mapping it into the generic air quality model. Each implementation owns its
 * own transport (WiFiClient / WiFiClientSecure) and opens and closes the
 * connection inside fetch().
 *
 * Returns ProviderResult::ok() on success. On failure, detail() holds a
 * already-localized message suitable for the error screen.
 */
class AirQualityProvider {
 public:
  virtual ~AirQualityProvider() = default;
  virtual ProviderResult fetch(air_quality_t &airQuality) = 0;
};
