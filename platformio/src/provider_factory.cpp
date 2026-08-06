#include "config.h"
#include "provider_factory.h"

#if defined(WEATHER_API_OPEN_WEATHER_MAP)
#include "owm_weather_provider.h"
#endif
#if defined(WEATHER_API_OPEN_METEO)
#include "open_meteo_weather_provider.h"
#endif
#if defined(AIR_QUALITY_API_OPEN_WEATHER_MAP)
#include "owm_air_quality_provider.h"
#endif
#if defined(AIR_QUALITY_API_OPEN_METEO)
#include "open_meteo_air_quality_provider.h"
#endif
#if defined(ALERTS_API_OPEN_WEATHER_MAP) && !defined(WEATHER_API_OPEN_WEATHER_MAP)
#include "owm_alert_provider.h"
#endif

WeatherProvider *createWeatherProvider() {
#if defined(WEATHER_API_OPEN_WEATHER_MAP)
  return new OWMWeatherProvider();
#elif defined(WEATHER_API_OPEN_METEO)
  return new OpenMeteoWeatherProvider();
#else
  return nullptr;
#endif
}  // createWeatherProvider

AirQualityProvider *createAirQualityProvider() {
#if defined(AIR_QUALITY_API_OPEN_WEATHER_MAP)
  return new OWMAirQualityProvider();
#elif defined(AIR_QUALITY_API_OPEN_METEO)
  return new OpenMeteoAirQualityProvider();
#else
  return nullptr;
#endif
}  // createAirQualityProvider

AlertProvider *createAlertProvider(WeatherProvider *weatherProvider) {
#if defined(ALERTS_API_OPEN_WEATHER_MAP) && defined(WEATHER_API_OPEN_WEATHER_MAP)
  // OpenWeatherMap serves alerts in the same One Call response as the
  // weather, so the weather provider itself implements AlertProvider.
  return static_cast<OWMWeatherProvider *>(weatherProvider);
#elif defined(ALERTS_API_OPEN_WEATHER_MAP)
  // The weather provider does not serve alerts, use a standalone alert
  // provider that fetches them separately.
  return new OWMAlertProvider();
#else
  // No alert source configured. A dedicated external alert provider (e.g.
  // MeteoAlarm) can be added here in the future.
  return nullptr;
#endif
}  // createAlertProvider
