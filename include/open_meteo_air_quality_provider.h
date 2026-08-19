#pragma once

#include "air_quality_provider.h"
#include "provider_result.h"

/* Open-Meteo air quality API provider. */
class OpenMeteoAirQualityProvider : public AirQualityProvider {
 public:
  ProviderResult fetch(air_quality_t &airQuality) override;

 private:
  static ProviderResult deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};
