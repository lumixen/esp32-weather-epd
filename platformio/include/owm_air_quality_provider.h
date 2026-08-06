#pragma once

#include <ArduinoJson.h>
#include "air_quality_provider.h"

/* OpenWeatherMap "Air Pollution" API air quality provider. */
class OWMAirQualityProvider : public AirQualityProvider {
 public:
  int fetch(air_quality_t &airQuality) override;

 private:
  static DeserializationError deserializeAirQuality(Stream &json, air_quality_t &airQuality);
};
