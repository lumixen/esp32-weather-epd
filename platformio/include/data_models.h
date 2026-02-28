#pragma once

#include <Arduino.h>

#define NUM_HOURLY 24  // 48
#define NUM_DAILY 5    // 8

struct sensor_readings {
  std::optional<float> temperature;
  std::optional<float> humidity;
  std::optional<float> pressure;
};

typedef struct weather {
  int id;              // Weather condition id
  String main;         // Group of weather parameters (Rain, Snow, Extreme etc.)
  String description;  // Weather condition within the group (full list of weather conditions). Get the output in your
                       // language
} weather_t;

/*
 * Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
 */
typedef struct temperature {
  float morn;   // Morning temperature.
  float day;    // Day temperature.
  float eve;    // Evening temperature.
  float night;  // Night temperature.
  float min;    // Min daily temperature.
  float max;    // Max daily temperature.
} temperature_t;

/*
 * Current weather data
 */
typedef struct current {
  int64_t dt;        // Current time, Unix, UTC
  int64_t sunrise;   // Sunrise time, Unix, UTC
  int64_t sunset;    // Sunset time, Unix, UTC
  float temp;        // Temperature. Units - default: kelvin, metric: Celsius, imperial: Fahrenheit.
  float feels_like;  // Temperature. This temperature parameter accounts for the human perception of weather. Units –
                     // default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int pressure;      // Atmospheric pressure on the sea level, hPa
  int humidity;      // Humidity, %
  float
      dew_point;  // Atmospheric temperature (varying according to pressure and humidity) below which water droplets
                  // begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int clouds;        // Cloudiness, %
  float uvi;         // Current UV index
  int visibility;    // Average visibility, metres. The maximum value of the visibility is 10km
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
  int clouds;        // Cloudiness, %
  float uvi;         // Current UV index
  int visibility;    // Average visibility, metres. The maximum value of the visibility is 10km
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
  int64_t dt;        // Time of the forecasted data, unix, UTC
  int64_t sunrise;   // Sunrise time, Unix, UTC
  int64_t sunset;    // Sunset time, Unix, UTC
  int64_t moonrise;  // The time of when the moon rises for this day, Unix, UTC
  int64_t moonset;   // The time of when the moon sets for this day, Unix, UTC
  float moon_phase;  // Moon phase. 0 and 1 are 'new moon', 0.25 is 'first quarter moon', 0.5 is 'full moon' and 0.75 is
                     // 'last quarter moon'. The periods in between are called 'waxing crescent', 'waxing gibous',
                     // 'waning gibous', and 'waning crescent', respectively.
  temperature_t temp;
  int pressure;  // Atmospheric pressure on the sea level, hPa
  int humidity;  // Humidity, %
  float
      dew_point;  // Atmospheric temperature (varying according to pressure and humidity) below which water droplets
                  // begin to condense and dew can form. Units – default: kelvin, metric: Celsius, imperial: Fahrenheit.
  int clouds;        // Cloudiness, %
  float uvi;         // Current UV index
  int visibility;    // Average visibility, metres. The maximum value of the visibility is 10km
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
typedef struct owm_alerts {
  String sender_name;  // Name of the alert source.
  String event;        // Alert event name
  int64_t start;       // Date and time of the start of the alert, Unix, UTC
  int64_t end;         // Date and time of the end of the alert, Unix, UTC
  String description;  // Description of the alert
  String tags;         // Type of severe weather
} owm_alerts_t;

/*
 * Response from OpenWeatherMap's OneCall API
 *
 * https://openweathermap.org/api/one-call-api
 */
typedef struct environment_data {
  float lat;            // Geographical coordinates of the location (latitude)
  float lon;            // Geographical coordinates of the location (longitude)
  String timezone;      // Timezone name for the requested location
  int timezone_offset;  // Shift in seconds from UTC
  current_t current;

  hourly_t hourly[NUM_HOURLY];
  daily_t daily[NUM_DAILY];
  std::vector<owm_alerts_t> alerts;
} environment_data_t;
