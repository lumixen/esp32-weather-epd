#pragma once

#include "air_quality_provider.h"
#include "alert_provider.h"
#include "weather_provider.h"

/* Create the weather provider selected at compile time by WEATHER_API_*.
 * Returns nullptr if none is configured.
 */
WeatherProvider *createWeatherProvider();

/* Create the air quality provider selected at compile time by
 * AIR_QUALITY_API_*. Returns nullptr if none is configured.
 */
AirQualityProvider *createAirQualityProvider();

/* Create the alert provider, if any.
 *
 * The weather provider may also serve alerts when they ride along in the
 * weather response (e.g. OpenWeatherMap). Returns nullptr when no alert
 * source is available, in which case no alerts will be displayed.
 */
AlertProvider *createAlertProvider(WeatherProvider *weatherProvider);
