#pragma once

#include <memory>
#include <vector>
#include "air_quality_provider.h"
#include "alert_provider.h"
#include "data_models.h"
#include "fetch_operation.h"
#include "weather_provider.h"

/* Create the weather provider selected at compile time by WEATHER_API_*.
 * Returns nullptr if none is configured.
 */
WeatherProvider *createWeatherProvider();

/* Create the air quality provider selected at compile time by
 * AIR_QUALITY_API_*. Returns nullptr if none is configured.
 */
AirQualityProvider *createAirQualityProvider();

/* Create the alert provider, if any (standalone per ALERTS_API_*).
 * Returns nullptr when no alert source is available.
 * For OWM piggyback (WEATHER=OWM && ALERTS=OWM) use createProviders()
 * which aliases alert to weather correctly.
 */
AlertProvider *createAlertProvider();

struct ProviderBundle {
  std::shared_ptr<WeatherProvider> weather;
  std::shared_ptr<AirQualityProvider> airQuality;
  std::shared_ptr<AlertProvider> alert;
};

struct FetchBundle {
  ProviderBundle providers;
  std::vector<std::unique_ptr<FetchOperation>> ops;
};

ProviderBundle createProviders();
FetchBundle createFetchBundle(forecast_t &forecast, air_quality_t &airQuality,
                              std::vector<weather_alert_t> &alerts);
