#pragma once

#include <ArduinoJson.h>
#include "weather_provider.h"

/* Open-Meteo forecast API weather provider. */
class OpenMeteoWeatherProvider : public WeatherProvider {
 public:
  const char *getApiName() const override;
  int fetch(forecast_t &forecast) override;

 private:
  static DeserializationError deserializeCall(Stream &json, forecast_t &forecast);
};
