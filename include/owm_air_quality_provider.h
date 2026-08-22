#pragma once

#include "air_quality_provider.h"
#include "provider_result.h"

/* OpenWeatherMap "Air Pollution" API air quality provider. */
class OWMAirQualityProvider : public AirQualityProvider {
 public:
  ProviderResult fetch(air_quality_t &airQuality) override;

  /* Map an OWM Air Pollution response into the generic air-quality model.
   * Public for offline fixture-based unit testing. */
  static ProviderResult deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};
