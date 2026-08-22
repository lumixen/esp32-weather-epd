#include "config.h"
#include "provider_factory.h"
#include "fetch_operation.h"
#include "provider_fetch_operations.h"

#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP) || defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
#include "owm_provider.h"
#endif
#if defined(WEATHER_API_PROVIDER_OPEN_METEO)
#include "open_meteo_weather_provider.h"
#endif
#if defined(AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP)
#include "owm_air_quality_provider.h"
#endif
#if defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)
#include "open_meteo_air_quality_provider.h"
#endif
#if defined(ALERTS_API_PROVIDER_METEOALARM)
#include "meteoalarm_alert_provider.h"
#endif

WeatherProvider *createWeatherProvider() {
#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)
  return new OWMWeatherProvider();
#elif defined(WEATHER_API_PROVIDER_OPEN_METEO)
  return new OpenMeteoWeatherProvider();
#else
  return nullptr;
#endif
}  // createWeatherProvider

AirQualityProvider *createAirQualityProvider() {
#if defined(AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP)
  return new OWMAirQualityProvider();
#elif defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)
  return new OpenMeteoAirQualityProvider();
#else
  return nullptr;
#endif
}  // createAirQualityProvider

AlertProvider *createAlertProvider() {
#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  return new OWMProvider();
#elif defined(ALERTS_API_PROVIDER_METEOALARM)
  return new MeteoAlarmAlertProvider();
#else
  return nullptr;
#endif
}  // createAlertProvider

ProviderBundle createProviders() {
  ProviderBundle bundle;
#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)
  bundle.weather = std::shared_ptr<WeatherProvider>(new OWMProvider());
#elif defined(WEATHER_API_PROVIDER_OPEN_METEO)
  bundle.weather = std::make_shared<OpenMeteoWeatherProvider>();
#endif
#if defined(AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP)
  bundle.airQuality = std::make_shared<OWMAirQualityProvider>();
#elif defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)
  bundle.airQuality = std::make_shared<OpenMeteoAirQualityProvider>();
#endif
#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP) && defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)
  // Alias alert to weather — same OWMProvider block, Alert subobject
  if (bundle.weather) {
    // Convert through the concrete type: WeatherProvider and AlertProvider
    // are unrelated interfaces, so casting directly between them is invalid.
    auto *owm = static_cast<OWMProvider *>(bundle.weather.get());
    bundle.alert = std::shared_ptr<AlertProvider>(bundle.weather, static_cast<AlertProvider *>(owm));
  }
#elif defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  bundle.alert = std::make_shared<OWMProvider>();
#elif defined(ALERTS_API_PROVIDER_METEOALARM)
  bundle.alert = std::make_shared<MeteoAlarmAlertProvider>();
#endif
  return bundle;
}

FetchBundle createFetchBundle(forecast_t &forecast, air_quality_t &airQuality,
                              std::vector<weather_alert_t> &alerts) {
  FetchBundle fb;
  fb.providers = createProviders();
  fb.ops = createFetchOperations(fb.providers.weather.get(), fb.providers.airQuality.get(), fb.providers.alert.get(),
                                 forecast, airQuality, alerts);
  return fb;
}
