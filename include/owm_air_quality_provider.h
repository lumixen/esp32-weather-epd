#pragma once

#include "air_quality_provider.h"
#include "provider_result.h"

/* OpenWeatherMap "Air Pollution" API air quality provider. */
class OWMAirQualityProvider : public AirQualityProvider {
 public:
  ProviderResult fetch(air_quality_t &airQuality) override;

 private:
  static ProviderResult deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};
