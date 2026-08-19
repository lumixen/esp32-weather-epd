#pragma once

#include "air_quality_provider.h"
#include "provider_result.h"

/* Open-Meteo air quality API provider. */
class OpenMeteoAirQualityProvider : public AirQualityProvider {
 public:
  ProviderResult fetch(air_quality_t &airQuality) override;

  /* Map a streamed JSON response of the Open-Meteo air quality API into the
   * generic air quality model. Public for unit testing. */
  static ProviderResult deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};
