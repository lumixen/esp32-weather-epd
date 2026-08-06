#pragma once

#include <WiFiClient.h>
#include "data_models.h"

/* Interface for air quality providers.
 *
 * Implementations are responsible for fetching provider-specific data and
 * mapping it into the generic air quality model.
 *
 * Returns the HTTP status code on success (HTTP_CODE_OK). Negative codes:
 * -512 - WiFi status offset when disconnected, -256 - JSON deserialization
 * error code offset.
 */
class AirQualityProvider {
 public:
  virtual ~AirQualityProvider() = default;
  virtual int fetch(WiFiClient &client, air_quality_t &airQuality) = 0;
};
