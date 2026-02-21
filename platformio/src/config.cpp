/* Configuration options for esp32-weather-epd.
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

#include <Arduino.h>
#include "config.h"

// WIFI
const char *WIFI_SSID = D_WIFI_SSID;

// HTTP
// The following errors are likely the result of insuffient http client tcp
// timeout:
//   -1   Connection Refused
//   -11  Read Timeout
//   -258 Deserialization Incomplete Input
const unsigned HTTP_CLIENT_TCP_TIMEOUT = 2000;  // ms

// OPENWEATHERMAP API
// OpenWeatherMap API key, https://openweathermap.org/
const String OWM_APIKEY = D_OWM_APIKEY;
// OpenWeatherMap One Call 2.5 API is deprecated for all new free users
// (accounts created after Summer 2022).
//
// Please note, that One Call API 3.0 is included in the "One Call by Call"
// subscription only. This separate subscription includes 1,000 calls/day for
// free and allows you to pay only for the number of API calls made to this
// product.
//
// Here’s how to subscribe and avoid any credit card changes:
// - Go to https://home.openweathermap.org/subscriptions/billing_info/onecall_30/base?key=base&service=onecall_30
// - Follow the instructions to complete the subscription.
// - Go to https://home.openweathermap.org/subscriptions and set the "Calls per
//   day (no more than)" to 1,000. This ensures you will never overrun the free
//   calls.
const String OWM_ONECALL_VERSION = D_OWM_ONECALL_VERSION;

// OPENWEATHERMAP API
// OpenWeatherMap API key, https://openweathermap.org/
const String OWM_ENDPOINT = "api.openweathermap.org";

// OPENMETEO API
// Open Meteo API, https://open-meteo.com/
const String OM_ENDPOINT = "api.open-meteo.com";

// Open Meteo Air Quality API
// https://open-meteo.com/en/docs/air-quality-api
const String OM_AIR_QUALITY_ENDPOINT = "air-quality-api.open-meteo.com";

// LOCATION
// Set your latitude and longitude.
// (used to get weather data as part of API requests to OpenWeatherMap)
const String LAT = D_LATITUDE;
const String LON = D_LONGITUDE;
// City name that will be shown in the top-right corner of the display.
const String CITY_STRING = D_CITY;

// TIME
// Time format used when displaying sunrise/set times. (Max 11 characters)
// For more information about formatting see
// https://man7.org/linux/man-pages/man3/strftime.3.html
const char *TIME_FORMAT = D_TIME_FORMAT;  // 24-hour ex: 01:23   23:00
// Time format used when displaying axis labels. (Max 11 characters)
// For more information about formatting see
// https://man7.org/linux/man-pages/man3/strftime.3.html
// const char *HOUR_FORMAT = "%l%P"; // 12-hour ex: 1am  11pm
const char *HOUR_FORMAT = D_HOUR_FORMAT;  // 24-hour ex: 01   23
// NTP_SERVER_1 is the primary time server, while NTP_SERVER_2 is a fallback.
// pool.ntp.org will find the closest available NTP server to you.
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";
// If you encounter the 'Failed To Fetch The Time' error, try increasing
// NTP_TIMEOUT or select closer/lower latency time servers.
const unsigned long NTP_TIMEOUT = 20000;  // ms

// BATTERY
// To protect the battery upon LOW_BATTERY_VOLTAGE, the display will cease to
// update until battery is charged again. The ESP32 will deep-sleep (consuming
// < 11μA), waking briefly check the voltage at the corresponding interval (in
// minutes). Once the battery voltage has fallen to CRIT_LOW_BATTERY_VOLTAGE,
// the esp32 will hibernate and a manual press of the reset (RST) button to
// begin operating again.
const uint32_t WARN_BATTERY_VOLTAGE = 3535;                 // (millivolts) ~20%
const uint32_t LOW_BATTERY_VOLTAGE = 3462;                  // (millivolts) ~10%
const uint32_t VERY_LOW_BATTERY_VOLTAGE = 3442;             // (millivolts)  ~8%
const uint32_t CRIT_LOW_BATTERY_VOLTAGE = 3404;             // (millivolts)  ~5%
const unsigned long LOW_BATTERY_SLEEP_INTERVAL = 30;        // (minutes)
const unsigned long VERY_LOW_BATTERY_SLEEP_INTERVAL = 120;  // (minutes)
// Battery voltage calculations are based on a typical 3.7v LiPo.
const uint32_t MAX_BATTERY_VOLTAGE = 4200;  // (millivolts)
const uint32_t MIN_BATTERY_VOLTAGE = 3000;  // (millivolts)

// See config.h for the below options
// E-PAPER PANEL
// LOCALE
// UNITS
// WIND ICON PRECISION
// FONTS
// ALERTS
// BATTERY MONITORING