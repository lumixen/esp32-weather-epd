#pragma once

#include "provider_result.h"
#include "weather_provider.h"

/* Open-Meteo forecast API weather provider. */
class OpenMeteoWeatherProvider : public WeatherProvider {
 public:
  const char *getApiName() const override;
  ProviderResult fetch(forecast_t &forecast) override;

  /* Map a WMO weather interpretation code onto the unified weather_condition
   * enum. Public for unit testing. */
  static weather_condition mapWeatherCode(int id);

  /* Map a streamed JSON response of the Open-Meteo forecast API into the
   * generic forecast model. Public for unit testing. */
  static ProviderResult deserializeCall(Stream &json, forecast_t &forecast);
};
