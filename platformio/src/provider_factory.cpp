/* Provider factory for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
#if defined(WEATHER_API_OPEN_WEATHER_MAP) && DISPLAY_ALERTS
  // OpenWeatherMap serves alerts in the same One Call response as the
  // weather, so the weather provider itself implements AlertProvider.
  return static_cast<OWMWeatherProvider *>(weatherProvider);
#else
  // No alert source configured. A dedicated external alert provider (e.g.
  // MeteoAlarm) can be added here in the future.
  return nullptr;
#endif
}  // createAlertProvider
