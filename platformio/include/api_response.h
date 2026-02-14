/* API response deserialization declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#include <cstdint>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "data_models.h"

#define OWM_NUM_ALERTS \
  8  // OpenWeatherMaps does not specify a limit, but if you need more alerts you are probably doomed.
#define NUM_AIR_POLLUTION \
  24  // Depending on AQI scale, hourly concentrations will need to be averaged over a period of 1h to 24h

/*
 * Coordinates from the specified location (latitude, longitude)
 */
typedef struct coord {
  float lat;
  float lon;
} coord_t;

typedef struct air_components {
  float co[NUM_AIR_POLLUTION];     // Сoncentration of CO (Carbon monoxide), μg/m^3
  float no[NUM_AIR_POLLUTION];     // Сoncentration of NO (Nitrogen monoxide), μg/m^3
  float no2[NUM_AIR_POLLUTION];    // Сoncentration of NO2 (Nitrogen dioxide), μg/m^3
  float o3[NUM_AIR_POLLUTION];     // Сoncentration of O3 (Ozone), μg/m^3
  float so2[NUM_AIR_POLLUTION];    // Сoncentration of SO2 (Sulphur dioxide), μg/m^3
  float pm2_5[NUM_AIR_POLLUTION];  // Сoncentration of PM2.5 (Fine particles matter), μg/m^3
  float pm10[NUM_AIR_POLLUTION];   // Сoncentration of PM10 (Coarse particulate matter), μg/m^3
  float nh3[NUM_AIR_POLLUTION];    // Сoncentration of NH3 (Ammonia), μg/m^3
} air_components_t;

typedef struct air_pollution {
  coord_t coord;
  air_components_t components;
  int64_t dt[NUM_AIR_POLLUTION];  // Date and time, Unix, UTC;
} air_pollution_t;

DeserializationError deserializeOneCall(WiFiClient &json, environment_data_t &r);
DeserializationError deserializeOpenMeteoCall(WiFiClient &json, environment_data_t &r);
DeserializationError deserializeOWMAirQuality(WiFiClient &json, air_pollution_t &r);
DeserializationError deserializeOpenMeteoAirQuality(WiFiClient &json, air_pollution_t &r);
