/* OpenWeatherMap One Call weather provider for esp32-weather-epd.
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

#if defined(WEATHER_API_OPEN_WEATHER_MAP)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "_locale.h"
#include "client_utils.h"
#include "owm_weather_provider.h"

// OpenWeatherMaps does not specify a limit, but if you need more alerts you
// are probably doomed.
#define OWM_NUM_ALERTS 8

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API and map the
 * response into the generic forecast model.
 *
 * Returns the HTTP Status Code.
 */
int OWMWeatherProvider::fetch(WiFiClient &client, forecast_t &forecast) {
  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=metric&exclude=minutely";
#if !defined(ALERTS_API_OPEN_WEATHER_MAP)
  // exclude alerts
  uri += ",alerts";
#endif

  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";

  uri += "&appid=" + OWM_APIKEY;

  std::vector<weather_alert_t> *alerts = nullptr;
#if defined(ALERTS_API_OPEN_WEATHER_MAP)
  alerts = &alerts_;
#endif

  int httpResponse = httpGetWithRetry(
      client, OWM_ENDPOINT, uri, sanitizedUri, false,
      [this, &forecast, alerts](Stream &json) { return deserializeOneCall(json, forecast, alerts); });

  fetchStatus_ = httpResponse;
  haveAlerts_ = (httpResponse == HTTP_CODE_OK);
  return httpResponse;
}  // OWMWeatherProvider::fetch

/* Serve national weather alerts extracted from the last One Call response.
 * No additional HTTP request is made.
 *
 * Returns the HTTP Status Code.
 */
int OWMWeatherProvider::fetch(WiFiClient &client, std::vector<weather_alert_t> &alerts) {
  if (haveAlerts_) {
    alerts = alerts_;
    return HTTP_CODE_OK;
  }
  return fetchStatus_;
}  // OWMWeatherProvider::fetch

DeserializationError OWMWeatherProvider::deserializeOneCall(Stream &json, forecast_t &forecast,
                                                            std::vector<weather_alert_t> *alerts) {
  int i;

  JsonDocument filter;
  filter["current"] = true;
  filter["minutely"] = false;
  filter["hourly"] = true;
  filter["daily"] = true;
#if !defined(ALERTS_API_OPEN_WEATHER_MAP)
  filter["alerts"] = false;
#else
  // description can be very long so they are filtered out to save on memory
  // along with sender_name
  for (int i = 0; i < OWM_NUM_ALERTS; ++i) {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"] = true;
    filter["alerts"][i]["start"] = true;
    filter["alerts"][i]["end"] = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"] = true;
  }
#endif

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  forecast.lat = doc["lat"].as<float>();
  forecast.lon = doc["lon"].as<float>();
  forecast.timezone = doc["timezone"].as<const char *>();
  forecast.timezone_offset = doc["timezone_offset"].as<int>();

  JsonObject current = doc["current"];
  forecast.current.dt = current["dt"].as<int64_t>();
  forecast.current.sunrise = current["sunrise"].as<int64_t>();
  forecast.current.sunset = current["sunset"].as<int64_t>();
  forecast.current.temp = current["temp"].as<float>();
  forecast.current.feels_like = current["feels_like"].as<float>();
  forecast.current.pressure = current["pressure"].as<int>();
  forecast.current.humidity = current["humidity"].as<int>();
  forecast.current.dew_point = current["dew_point"].as<float>();
  forecast.current.clouds = current["clouds"].as<int>();
  forecast.current.uvi = current["uvi"].as<float>();
  forecast.current.visibility = current["visibility"].as<int>();
  forecast.current.wind_speed = current["wind_speed"].as<float>();
  forecast.current.wind_gust = current["wind_gust"].as<float>();
  forecast.current.wind_deg = current["wind_deg"].as<int>();
  forecast.current.rain_1h = current["rain"]["1h"].as<float>();
  forecast.current.snow_1h = current["snow"]["1h"].as<float>();
  JsonObject current_weather = current["weather"][0];
  forecast.current.weather.id = current_weather["id"].as<int>();
  forecast.current.weather.main = current_weather["main"].as<const char *>();
  forecast.current.weather.description = current_weather["description"].as<const char *>();
  // OpenWeatherMap indicates sun is up with d otherwise n for night
  forecast.current.is_day = current_weather["icon"].as<String>().endsWith("d");

  i = 0;
  for (JsonObject hourly : doc["hourly"].as<JsonArray>()) {
    forecast.hourly[i].dt = hourly["dt"].as<int64_t>();
    forecast.hourly[i].temp = hourly["temp"].as<float>();
    forecast.hourly[i].feels_like = hourly["feels_like"].as<float>();
    forecast.hourly[i].pressure = hourly["pressure"].as<int>();
    forecast.hourly[i].humidity = hourly["humidity"].as<int>();
    forecast.hourly[i].dew_point = hourly["dew_point"].as<float>();
    forecast.hourly[i].clouds = hourly["clouds"].as<int>();
    forecast.hourly[i].uvi = hourly["uvi"].as<float>();
    forecast.hourly[i].visibility = hourly["visibility"].as<int>();
    forecast.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    forecast.hourly[i].wind_gust = hourly["wind_gust"].as<float>();
    forecast.hourly[i].wind_deg = hourly["wind_deg"].as<int>();
    forecast.hourly[i].pop = hourly["pop"].as<float>() * 100;
    forecast.hourly[i].rain_1h = hourly["rain"]["1h"].as<float>();
    forecast.hourly[i].snow_1h = hourly["snow"]["1h"].as<float>();
    JsonObject hourly_weather = hourly["weather"][0];
    forecast.hourly[i].weather.id = hourly_weather["id"].as<int>();
    forecast.hourly[i].weather.main = hourly_weather["main"].as<const char *>();
    forecast.hourly[i].weather.description = hourly_weather["description"].as<const char *>();
    // OpenWeatherMap indicates sun is up with d otherwise n for night
    forecast.hourly[i].is_day = hourly_weather["icon"].as<String>().endsWith("d");

    if (i == NUM_HOURLY - 1) {
      break;
    }
    ++i;
  }

  i = 0;
  for (JsonObject daily : doc["daily"].as<JsonArray>()) {
    forecast.daily[i].dt = daily["dt"].as<int64_t>();
    forecast.daily[i].sunrise = daily["sunrise"].as<int64_t>();
    forecast.daily[i].sunset = daily["sunset"].as<int64_t>();
    JsonObject daily_temp = daily["temp"];
    forecast.daily[i].temp.morn = daily_temp["morn"].as<float>();
    forecast.daily[i].temp.day = daily_temp["day"].as<float>();
    forecast.daily[i].temp.eve = daily_temp["eve"].as<float>();
    forecast.daily[i].temp.night = daily_temp["night"].as<float>();
    forecast.daily[i].temp.min = daily_temp["min"].as<float>();
    forecast.daily[i].temp.max = daily_temp["max"].as<float>();
    forecast.daily[i].pressure = daily["pressure"].as<int>();
    forecast.daily[i].humidity = daily["humidity"].as<int>();
    forecast.daily[i].dew_point = daily["dew_point"].as<float>();
    forecast.daily[i].clouds = daily["clouds"].as<int>();
    forecast.daily[i].uvi = daily["uvi"].as<float>();
    forecast.daily[i].visibility = daily["visibility"].as<int>();
    forecast.daily[i].wind_speed = daily["wind_speed"].as<float>();
    forecast.daily[i].wind_gust = daily["wind_gust"].as<float>();
    forecast.daily[i].wind_deg = daily["wind_deg"].as<int>();
    forecast.daily[i].pop = daily["pop"].as<float>() * 100;
    forecast.daily[i].rain = daily["rain"].as<float>();
    forecast.daily[i].snow = daily["snow"].as<float>();
    JsonObject daily_weather = daily["weather"][0];
    forecast.daily[i].weather.id = daily_weather["id"].as<int>();
    forecast.daily[i].weather.main = daily_weather["main"].as<const char *>();
    forecast.daily[i].weather.description = daily_weather["description"].as<const char *>();

    if (i == NUM_DAILY - 1) {
      break;
    }
    ++i;
  }

#if defined(ALERTS_API_OPEN_WEATHER_MAP)
  if (alerts != nullptr) {
    i = 0;
    for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
      weather_alert_t new_alert = {};
      // new_alert.sender_name = alert["sender_name"].as<const char *>();
      new_alert.event = alert["event"].as<const char *>();
      new_alert.start = alert["start"].as<int64_t>();
      new_alert.end = alert["end"].as<int64_t>();
      // new_alert.description = alert["description"].as<const char *>();
      new_alert.tags = alert["tags"][0].as<const char *>();
      alerts->push_back(new_alert);

      if (i == OWM_NUM_ALERTS - 1) {
        break;
      }
      ++i;
    }
  }
#endif

  return error;
}  // OWMWeatherProvider::deserializeOneCall

#endif  // WEATHER_API_OPEN_WEATHER_MAP
