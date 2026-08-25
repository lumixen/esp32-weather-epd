/* Unified remote provider factory for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "provider_factory.h"

#if defined(REMOTE_PROVIDER_OPEN_METEO_FORECAST)
#include "open_meteo_weather_provider.h"
#endif
#if defined(REMOTE_PROVIDER_NOAA_FORECAST)
#include "noaa_forecast_provider.h"
#endif
#if defined(REMOTE_PROVIDER_OPEN_METEO_AIR_QUALITY)
#include "open_meteo_air_quality_provider.h"
#endif
#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V3)
#include "owm_provider.h"
#endif
#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_AIR_QUALITY)
#include "owm_air_quality_provider.h"
#endif
#if defined(REMOTE_PROVIDER_METEOALARM_ALERT)
#include "meteoalarm_alert_provider.h"
#endif

ProviderBundle createProviders() {
  ProviderBundle bundle;
#if defined(REMOTE_PROVIDER_METEOALARM_ALERT)
  bundle.providers.push_back(std::make_shared<MeteoAlarmAlertProvider>());
#endif
#if defined(REMOTE_PROVIDER_OPEN_METEO_FORECAST)
  bundle.providers.push_back(std::make_shared<OpenMeteoForecastProvider>());
#endif
#if defined(REMOTE_PROVIDER_NOAA_FORECAST)
  bundle.providers.push_back(std::make_shared<NoaaForecastProvider>());
#endif
#if defined(REMOTE_PROVIDER_OPEN_METEO_AIR_QUALITY)
  bundle.providers.push_back(std::make_shared<OpenMeteoAirQualityProvider>());
#endif
#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V3)
  bundle.providers.push_back(std::make_shared<OpenWeatherMapOneCallV3Provider>());
#endif
#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_AIR_QUALITY)
  bundle.providers.push_back(std::make_shared<OpenWeatherMapAirQualityProvider>());
#endif
  return bundle;
}

FetchBundle createFetchBundle(weather_report_t &report) {
  FetchBundle bundle;
  bundle.providers = createProviders();
  for (const auto &provider : bundle.providers.providers) {
    auto operations = provider->createFetchOperations(report);
    for (auto &operation : operations) {
      bundle.ops.push_back(std::move(operation));
    }
  }
  return bundle;
}
