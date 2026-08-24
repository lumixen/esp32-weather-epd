/* Unified OWM One Call provider — single class for weather+alerts.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "logger.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "_locale.h"
#include "client_utils.h"
#include "owm_provider.h"
#include "provider_fetch_operations.h"
#include "provider_result_utils.h"

#define OWM_NUM_ALERTS 8

#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V3)

const char *OpenWeatherMapOneCallV3Provider::getApiName() const { return "One Call API"; }

std::vector<std::unique_ptr<FetchOperation>> OpenWeatherMapOneCallV3Provider::createFetchOperations(
    weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out]() {
    out.resetForecast();
    out.resetAlerts();
    ProviderResult result = fetch(out);
    if (!result.isOk()) {
      out.resetForecast();
      out.resetAlerts();
    }
    return result;
  }));
  return operations;
}

ProviderResult OpenWeatherMapOneCallV3Provider::fetch(weather_report_t &report) {
#if defined(OPENWEATHERMAP_ONECALL_V3_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(OPENWEATHERMAP_ONECALL_V3_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else
  WiFiClientSecure client;
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
  const uint16_t port = 443;
#endif
  String uri = "/data/3.0/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG + "&units=metric&exclude=minutely";
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";
  uri += "&appid=" + OPENWEATHERMAP_ONECALL_V3_API_KEY;
  return httpGetWithRetry(client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
                          [&report](Stream &json, size_t) { return deserializeOneCall(json, report); });
}

weather_condition OpenWeatherMapOneCallV3Provider::mapWeatherCode(int id) {
  switch (id) {
    case 200:
    case 201:
    case 202:
    case 210:
    case 211:
    case 212:
    case 221:
      return weather_condition::THUNDERSTORM;
    case 230:
    case 231:
    case 232:
      return weather_condition::THUNDERSTORM_HAIL;
    case 300:
    case 301:
    case 302:
    case 310:
    case 311:
    case 312:
    case 313:
    case 314:
    case 321:
      return weather_condition::DRIZZLE;
    case 500:
    case 501:
    case 502:
    case 503:
    case 504:
      return weather_condition::RAIN;
    case 511:
      return weather_condition::FREEZING_RAIN;
    case 520:
    case 521:
    case 522:
    case 531:
      return weather_condition::RAIN_SHOWERS;
    case 600:
    case 601:
    case 602:
      return weather_condition::SNOW;
    case 611:
    case 612:
    case 613:
      return weather_condition::SLEET;
    case 615:
    case 616:
    case 620:
    case 621:
    case 622:
      return weather_condition::RAIN_SNOW_MIX;
    case 701:
      return weather_condition::MIST;
    case 711:
      return weather_condition::SMOKE;
    case 721:
      return weather_condition::HAZE;
    case 731:
      return weather_condition::SAND_WHIRLS;
    case 741:
      return weather_condition::FOG;
    case 751:
      return weather_condition::SAND;
    case 761:
      return weather_condition::DUST;
    case 762:
      return weather_condition::ASH;
    case 771:
      return weather_condition::SQUALL;
    case 781:
      return weather_condition::TORNADO;
    case 800:
      return weather_condition::CLEAR;
    case 801:
      return weather_condition::PARTLY_CLOUDY;
    case 802:
    case 803:
      return weather_condition::CLOUDY;
    case 804:
      return weather_condition::OVERCAST;
    default:
      if (id >= 200 && id < 300)
        return weather_condition::THUNDERSTORM;
      if (id >= 300 && id < 400)
        return weather_condition::DRIZZLE;
      if (id >= 500 && id < 600)
        return weather_condition::RAIN;
      if (id >= 600 && id < 700)
        return weather_condition::SNOW;
      if (id >= 700 && id < 800)
        return weather_condition::FOG;
      if (id >= 800 && id < 900)
        return weather_condition::CLOUDY;
      return weather_condition::UNKNOWN;
  }
}

ProviderResult OpenWeatherMapOneCallV3Provider::deserializeOneCall(Stream &json, weather_report_t &report) {
  int i;
  // The destination is long-lived in the application. Clear it before every
  // parse so omitted fields cannot retain values from an earlier response.
  report.resetForecast();
  report.resetAlerts();

  JsonDocument filter;
  filter["lat"] = true;
  filter["lon"] = true;
  filter["timezone"] = true;
  filter["timezone_offset"] = true;
  filter["current"] = true;
  filter["minutely"] = false;
  filter["hourly"] = true;
  filter["daily"] = true;
  for (int i = 0; i < OWM_NUM_ALERTS; ++i) {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"] = true;
    filter["alerts"][i]["start"] = true;
    filter["alerts"][i]["end"] = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"] = true;
  }
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  LOG_DEBUG("doc.overflowed() : %s", doc.overflowed() ? "true" : "false");
  if (LogLevel::TRACE >= g_logLevel) {
    LOG_TRACE("pretty JSON dump:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  if (error) {
    report.resetForecast();
    report.resetAlerts();
    return mapDeserializationError(error);
  }
  report.forecast.lat = doc["lat"].as<float>();
  report.forecast.lon = doc["lon"].as<float>();
  report.forecast.timezone = doc["timezone"].as<const char *>();
  report.forecast.timezone_offset = doc["timezone_offset"].as<int>();

  JsonObject current = doc["current"];
  report.forecast.current.dt = current["dt"].as<int64_t>();
  report.forecast.current.temp = current["temp"].as<float>();
  report.forecast.current.feels_like = current["feels_like"].as<float>();
  report.forecast.current.pressure = current["pressure"].as<int>();
  report.forecast.current.humidity = current["humidity"].as<int>();
  report.forecast.current.dew_point = current["dew_point"].as<float>();
  report.forecast.current.clouds = current["clouds"].as<int>();
  report.forecast.current.uvi = current["uvi"].as<float>();
  report.forecast.current.visibility = current["visibility"].as<int>();
  report.forecast.current.wind_speed = current["wind_speed"].as<float>();
  report.forecast.current.wind_gust = current["wind_gust"].as<float>();
  report.forecast.current.wind_deg = current["wind_deg"].as<int>();
  report.forecast.current.rain_1h = current["rain"]["1h"].as<float>();
  report.forecast.current.snow_1h = current["snow"]["1h"].as<float>();
  JsonObject current_weather = current["weather"][0];
  report.forecast.current.weather.condition = mapWeatherCode(current_weather["id"].as<int>());
  report.forecast.current.is_day = current_weather["icon"].as<String>().endsWith("d");

  i = 0;
  for (JsonObject hourly : doc["hourly"].as<JsonArray>()) {
    report.forecast.hourly[i].dt = hourly["dt"].as<int64_t>();
    report.forecast.hourly[i].temp = hourly["temp"].as<float>();
    report.forecast.hourly[i].feels_like = hourly["feels_like"].as<float>();
    report.forecast.hourly[i].pressure = hourly["pressure"].as<int>();
    report.forecast.hourly[i].humidity = hourly["humidity"].as<int>();
    report.forecast.hourly[i].dew_point = hourly["dew_point"].as<float>();
    report.forecast.hourly[i].clouds = hourly["clouds"].as<int>();
    report.forecast.hourly[i].uvi = hourly["uvi"].as<float>();
    report.forecast.hourly[i].visibility = hourly["visibility"].as<int>();
    report.forecast.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    report.forecast.hourly[i].wind_gust = hourly["wind_gust"].as<float>();
    report.forecast.hourly[i].wind_deg = hourly["wind_deg"].as<int>();
    report.forecast.hourly[i].pop = hourly["pop"].as<float>() * 100;
    report.forecast.hourly[i].rain_1h = hourly["rain"]["1h"].as<float>();
    report.forecast.hourly[i].snow_1h = hourly["snow"]["1h"].as<float>();
    JsonObject hourly_weather = hourly["weather"][0];
    report.forecast.hourly[i].weather.condition = mapWeatherCode(hourly_weather["id"].as<int>());
    report.forecast.hourly[i].is_day = hourly_weather["icon"].as<String>().endsWith("d");
    if (i == NUM_HOURLY - 1)
      break;
    ++i;
  }

  i = 0;
  for (JsonObject daily : doc["daily"].as<JsonArray>()) {
    report.forecast.daily[i].dt = daily["dt"].as<int64_t>();
    JsonObject daily_temp = daily["temp"];
    report.forecast.daily[i].temp.morn = daily_temp["morn"].as<float>();
    report.forecast.daily[i].temp.day = daily_temp["day"].as<float>();
    report.forecast.daily[i].temp.eve = daily_temp["eve"].as<float>();
    report.forecast.daily[i].temp.night = daily_temp["night"].as<float>();
    report.forecast.daily[i].temp.min = daily_temp["min"].as<float>();
    report.forecast.daily[i].temp.max = daily_temp["max"].as<float>();
    report.forecast.daily[i].pressure = daily["pressure"].as<int>();
    report.forecast.daily[i].humidity = daily["humidity"].as<int>();
    report.forecast.daily[i].dew_point = daily["dew_point"].as<float>();
    report.forecast.daily[i].clouds = daily["clouds"].as<int>();
    report.forecast.daily[i].uvi = daily["uvi"].as<float>();
    report.forecast.daily[i].visibility = daily["visibility"].as<int>();
    report.forecast.daily[i].wind_speed = daily["wind_speed"].as<float>();
    report.forecast.daily[i].wind_gust = daily["wind_gust"].as<float>();
    report.forecast.daily[i].wind_deg = daily["wind_deg"].as<int>();
    report.forecast.daily[i].pop = daily["pop"].as<float>() * 100;
    report.forecast.daily[i].rain = daily["rain"].as<float>();
    report.forecast.daily[i].snow = daily["snow"].as<float>();
    JsonObject daily_weather = daily["weather"][0];
    report.forecast.daily[i].weather.condition = mapWeatherCode(daily_weather["id"].as<int>());
    if (i == NUM_DAILY - 1)
      break;
    ++i;
  }

  std::vector<weather_alert_t> &alerts = report.engageAlerts();
  if (doc["alerts"].is<JsonArray>()) {
    i = 0;
    for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
      weather_alert_t new_alert = {};
      new_alert.event = alert["event"].as<const char *>();
      new_alert.start = alert["start"].as<int64_t>();
      new_alert.end = alert["end"].as<int64_t>();
      new_alert.tags = alert["tags"][0].as<const char *>();
      alerts.push_back(new_alert);
      if (i == OWM_NUM_ALERTS - 1)
        break;
      ++i;
    }
  }

  return mapDeserializationError(error);
}

#endif  // REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V3
