/* Provider-agnostic data models for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 * Copyright (C) 2026  Max Bodaniuk
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
#pragma once

#include <Arduino.h>
#include <optional>
#include <vector>

#define NUM_HOURLY 24  // 48
#define NUM_DAILY 5    // 8
#define NUM_AIR_POLLUTION \
  24  // Depending on AQI scale, hourly concentrations will need to be averaged over a period of 1h to 24h

struct sensor_readings {
  std::optional<float> temperature;
  std::optional<float> humidity;
  std::optional<float> pressure;
};

/*
 * Unified weather condition. Providers map their native condition codes
 * (e.g. OWM condition ids or WMO weather codes) onto this enum; the display
 * layer maps it onto the condition bitmaps.
 *
 * UNKNOWN is first so that a zero-initialized forecast_t (fields not yet
 * parsed) resolves to the "not available" icon.
 */
enum class weather_condition {
  UNKNOWN,
  CLEAR,              // WMO 0, OWM 800
  PARTLY_CLOUDY,      // WMO 1, OWM 801
  CLOUDY,             // WMO 2, OWM 802, 803
  OVERCAST,           // WMO 3, OWM 804
  FOG,                // WMO 45, 48, OWM 741
  DRIZZLE,            // WMO 51, 53, 55, OWM 300-321
  FREEZING_DRIZZLE,   // WMO 56, 57
  RAIN,               // WMO 61, 63, 65, OWM 500-504
  FREEZING_RAIN,      // WMO 66, 67, OWM 511
  RAIN_SHOWERS,       // WMO 80-82, OWM 520-531
  SNOW,               // WMO 71-75, OWM 600-602
  SNOW_GRAINS,        // WMO 77
  SNOW_SHOWERS,       // WMO 85, 86
  SLEET,              // OWM 611-613
  RAIN_SNOW_MIX,      // OWM 615-622
  THUNDERSTORM,       // WMO 95, OWM 200-221
  THUNDERSTORM_HAIL,  // WMO 96, 99, OWM 230-232
  MIST,               // OWM 701
  SMOKE,              // OWM 711
  HAZE,               // OWM 721
  SAND_WHIRLS,        // OWM 731
  SAND,               // OWM 751
  DUST,               // OWM 761
  ASH,                // OWM 762
  SQUALL,             // OWM 771
  TORNADO             // OWM 781
};

/*
 * Weather condition, mapped to the unified weather_condition enum by the
 * provider.
 */
typedef struct weather {
  weather_condition condition;
} weather_t;

/*
 * Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
 */
typedef struct temperature {
  float morn;                // Morning temperature.
  float day;                 // Day temperature.
  float eve;                 // Evening temperature.
  float night;               // Night temperature.
  std::optional<float> min;  // Min daily temperature, when available.
  std::optional<float> max;  // Max daily temperature, when available.
} temperature_t;

/*
 * Current weather data
 */
typedef struct current {
  int64_t dt;        // Current time, Unix, UTC
  float temp;        // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  float feels_like;  // Temperature. This temperature parameter accounts for the human perception of weather. Units –
                     // default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int pressure;      // Atmospheric pressure on the sea level, hPa
  std::optional<int> humidity;  // Humidity, %, when available
  float
      dew_point;  // Atmospheric temperature (varying according to pressure and humidity) below which water droplets
                  // begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int clouds;                     // Cloudiness, %
  float uvi;                      // Current UV index
  std::optional<int> visibility;  // Average visibility, metres, when available. Maximum 10 km
  float wind_speed;  // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float wind_gust;  // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int wind_deg;     // Wind direction, degrees (meteorological)
  float rain_1h;    // (where available) Rain volume for last hour, mm
  float snow_1h;    // (where available) Snow volume for last hour, mm
  bool is_day;      // Is set to true if the sun is currently up
  float soil_temperature_18cm;  // (where available) Soil temperature at 18cm depth, °C
  weather_t weather;
} current_t;

/*
 * Hourly forecast weather data
 */
typedef struct hourly {
  int64_t dt;        // Time of the forecasted data, unix, UTC
  float temp;        // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  float feels_like;  // Temperature. This temperature parameter accounts for the human perception of weather. Units –
                     // default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int pressure;      // Atmospheric pressure on the sea level, hPa
  int humidity;      // Humidity, %
  float
      dew_point;  // Atmospheric temperature (varying according to pressure and humidity) below which water droplets
                  // begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int clouds;                     // Cloudiness, %
  float uvi;                      // Current UV index
  std::optional<int> visibility;  // Average visibility, metres, when available. Maximum 10 km
  float wind_speed;  // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float wind_gust;  // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int wind_deg;     // Wind direction, degrees (meteorological)
  int pop;          // Probability of precipitation, %
  float rain_1h;    // (where available) Rain volume for last hour, mm
  float snow_1h;    // (where available) Snow volume for last hour, mm
  bool is_day;      // Is set to true if the sun is up at the time
  weather_t weather;
} hourly_t;

/*
 * Daily forecast weather data
 */
typedef struct daily {
  int64_t dt;  // Time of the forecasted data, unix, UTC
  temperature_t temp;
  int pressure;  // Atmospheric pressure on the sea level, hPa
  int humidity;  // Humidity, %
  float
      dew_point;  // Atmospheric temperature (varying according to pressure and humidity) below which water droplets
                  // begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int clouds;                     // Cloudiness, %
  float uvi;                      // Current UV index
  std::optional<int> visibility;  // Average visibility, metres, when available. Maximum 10 km
  float wind_speed;  // Wind speed. Wind speed. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  float wind_gust;  // (where available) Wind gust. Units – default: metre/sec, metric: metre/sec, imperial: miles/hour.
  int wind_deg;     // Wind direction, degrees (meteorological)
  int pop;          // Probability of precipitation, %
  float rain;       // (where available) Precipitation volume, mm
  float snow;       // (where available) Snow volume, mm
  float shortwave_radiation_sum;  // (where available) Sum of shortwave radiation received, MJ/m²
  weather_t weather;
} daily_t;

/*
 * National weather alerts data from major national weather warning systems
 */
typedef struct weather_alert {
  String sender_name;  // Name of the alert source.
  String event;        // Alert event name
  int64_t start;       // Date and time of the start of the alert, Unix, UTC
  int64_t end;         // Date and time of the end of the alert, Unix, UTC
  String description;  // Description of the alert
  String tags;         // Type of severe weather
} weather_alert_t;

/*
 * Hourly concentrations of air pollutants, μg/m^3
 */
typedef struct air_quality_components {
  float co[NUM_AIR_POLLUTION];     // Сoncentration of CO (Carbon monoxide), μg/m^3
  float no[NUM_AIR_POLLUTION];     // Сoncentration of NO (Nitrogen monoxide), μg/m^3
  float no2[NUM_AIR_POLLUTION];    // Сoncentration of NO2 (Nitrogen dioxide), μg/m^3
  float o3[NUM_AIR_POLLUTION];     // Сoncentration of O3 (Ozone), μg/m^3
  float so2[NUM_AIR_POLLUTION];    // Сoncentration of SO2 (Sulphur dioxide), μg/m^3
  float pm2_5[NUM_AIR_POLLUTION];  // Сoncentration of PM2.5 (Fine particles matter), μg/m^3
  float pm10[NUM_AIR_POLLUTION];   // Сoncentration of PM10 (Coarse particulate matter), μg/m^3
  float nh3[NUM_AIR_POLLUTION];    // Сoncentration of NH3 (Ammonia), μg/m^3
} air_quality_components_t;

/*
 * Hourly air quality data
 */
typedef struct air_quality {
  air_quality_components_t components;
  int64_t dt[NUM_AIR_POLLUTION];  // Date and time, Unix, UTC;
} air_quality_t;

/*
 * Forecast data, provider-agnostic. Weather providers map their response
 * into this model.
 */
typedef struct forecast {
  float lat;            // Geographical coordinates of the location (latitude)
  float lon;            // Geographical coordinates of the location (longitude)
  String timezone;      // Timezone name for the requested location
  int timezone_offset;  // Shift in seconds from UTC
  current_t current;

  hourly_t hourly[NUM_HOURLY];
  daily_t daily[NUM_DAILY];

  /* Zero every field. Providers parse into long-lived instances, so a
   * response that omits fields must never leave previous values behind.
   * Clear the aggregate members separately instead of assigning forecast{},
   * which would create a large temporary on the caller's stack. */
  void reset() {
    lat = 0.0f;
    lon = 0.0f;
    timezone = String();
    timezone_offset = 0;
    current = {};
    for (hourly_t &entry : hourly)
      entry = {};
    for (daily_t &entry : daily)
      entry = {};
  }
} forecast_t;
