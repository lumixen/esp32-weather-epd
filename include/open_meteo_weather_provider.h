#pragma once

#include <ArduinoJson.h>
#include "weather_provider.h"

/* Open-Meteo forecast API weather provider. */
class OpenMeteoWeatherProvider : public WeatherProvider {
 public:
  const char *getApiName() const override;
  int fetch(forecast_t &forecast) override;

  /* Map a streamed JSON response of the Open-Meteo forecast API into the
   * generic forecast model. Public for unit testing. */
  static DeserializationError deserializeCall(Stream &json, forecast_t &forecast);
};
