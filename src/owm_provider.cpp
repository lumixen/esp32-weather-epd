/* Unified OWM One Call provider — single class for weather+alerts.
 * Copyright (C) 2026  Lumixen
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
#include "provider_result_utils.h"

#define OWM_NUM_ALERTS 8

OWMProvider::OWMProvider() {
  fetchMutex_ = xSemaphoreCreateMutex();
}

OWMProvider::~OWMProvider() {
  if (fetchMutex_) {
    vSemaphoreDelete(fetchMutex_);
    fetchMutex_ = nullptr;
  }
}

const char *OWMProvider::getApiName() const {
  return "One Call API";
}

ProviderResult OWMProvider::fetchInternal(forecast_t *forecast, std::vector<weather_alert_t> *alertsOut) {
  // Unified fetch: handles both full forecast and alerts-only via single method.
  // Chooses URI and deserializer based on what is requested.
  bool isAlertsOnly = (forecast == nullptr && alertsOut != nullptr);
#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP) && defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  // Piggyback: alerts-only first should still fetch full to cache forecast
  // so treat alerts-only as full when piggyback
  isAlertsOnly = false;
#endif

  if (isAlertsOnly) {
#if defined(ALERTS_API_TRANSPORT_HTTP)
    WiFiClient client;
    const uint16_t port = 80;
#elif defined(ALERTS_API_TRANSPORT_HTTPS_NO_VERIFY)
    WiFiClientSecure client;
    client.setInsecure();
    const uint16_t port = 443;
#else
    WiFiClientSecure client;
    client.setCACert(cert_USERTrust_RSA_Certification_Authority);
    const uint16_t port = 443;
#endif
    String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
                 "&units=metric&exclude=current,minutely,hourly,daily";
    String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";
    uri += "&appid=" + OWM_APIKEY;
    std::vector<weather_alert_t> tmp;
    ProviderResult result = httpGetWithRetry(
        client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
        [&tmp](Stream &json, size_t) { return deserializeAlerts(json, tmp); });
    if (result.isOk()) {
      *alertsOut = tmp;
      alerts_ = tmp;
      haveAlerts_ = true;
      fetchStatus_ = result;
      fetched_ = true;
    } else {
      fetchStatus_ = result;
      haveAlerts_ = false;
      fetched_ = true;
    }
    return result;
  }

  // Full fetch (forecast or piggyback)
#if defined(WEATHER_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(WEATHER_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else
  WiFiClientSecure client;
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
  const uint16_t port = 443;
#endif

  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=metric&exclude=minutely";
#if !defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  uri += ",alerts";
#endif

  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";
  uri += "&appid=" + OWM_APIKEY;

  forecast_t tmpForecast;
  std::vector<weather_alert_t> tmpAlerts;
  forecast_t *fcPtr = nullptr;
  std::vector<weather_alert_t> *alPtr = nullptr;

#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP) && defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)
  if (forecast && alertsOut) {
    fcPtr = forecast;
    alPtr = alertsOut;
  } else if (forecast && !alertsOut) {
    fcPtr = forecast;
    alPtr = &tmpAlerts;
  } else if (!forecast && alertsOut) {
    fcPtr = &tmpForecast;
    alPtr = alertsOut;
  } else {
    fcPtr = &tmpForecast;
    alPtr = &tmpAlerts;
  }
#elif defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  if (forecast) {
    fcPtr = forecast;
    alPtr = nullptr;
  } else if (alertsOut) {
    fcPtr = &tmpForecast;
    alPtr = alertsOut;
  }
#else
  fcPtr = forecast ? forecast : &tmpForecast;
  alPtr = nullptr;
#endif

  ProviderResult result = httpGetWithRetry(
      client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
      [fcPtr, alPtr](Stream &json, size_t) { return deserializeOneCall(json, *fcPtr, alPtr); });

  if (result.isOk()) {
    if (forecast && fcPtr != forecast) {
      *forecast = *fcPtr;
    }
    if (fcPtr) cachedForecast_ = *fcPtr;
    if (alPtr) {
      alerts_ = *alPtr;
      haveAlerts_ = true;
    } else {
      haveAlerts_ = true;
    }
    fetchStatus_ = result;
    fetched_ = true;
  } else {
    fetchStatus_ = result;
    haveAlerts_ = false;
    fetched_ = true;
  }
  return result;
}

ProviderResult OWMProvider::fetch(forecast_t &forecast) {
  if (fetchMutex_) xSemaphoreTake(fetchMutex_, portMAX_DELAY);
  if (fetched_) {
    ProviderResult r = fetchStatus_;
    if (r.isOk()) {
      forecast = cachedForecast_;
    }
    if (fetchMutex_) xSemaphoreGive(fetchMutex_);
    return r;
  }
  ProviderResult r = fetchInternal(&forecast, nullptr);
  if (fetchMutex_) xSemaphoreGive(fetchMutex_);
  return r;
}

ProviderResult OWMProvider::fetch(std::vector<weather_alert_t> &alerts) {
  if (fetchMutex_) xSemaphoreTake(fetchMutex_, portMAX_DELAY);
  if (fetched_) {
    ProviderResult r = fetchStatus_;
    if (r.isOk()) {
      alerts = alerts_;
    } else {
      LOG_ERROR("Alerts API: %s", r.detail().c_str());
      alerts.clear();
    }
    if (fetchMutex_) xSemaphoreGive(fetchMutex_);
    if (r.isOk()) return ProviderResult::ok();
    return r;
  }
  ProviderResult r = fetchInternal(nullptr, &alerts);
  if (!r.isOk()) {
    LOG_ERROR("Alerts API: %s", r.detail().c_str());
    alerts.clear();
  }
  if (fetchMutex_) xSemaphoreGive(fetchMutex_);
  return r;
}

weather_condition OWMProvider::mapWeatherCode(int id) {
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
      if (id >= 200 && id < 300) return weather_condition::THUNDERSTORM;
      if (id >= 300 && id < 400) return weather_condition::DRIZZLE;
      if (id >= 500 && id < 600) return weather_condition::RAIN;
      if (id >= 600 && id < 700) return weather_condition::SNOW;
      if (id >= 700 && id < 800) return weather_condition::FOG;
      if (id >= 800 && id < 900) return weather_condition::CLOUDY;
      return weather_condition::UNKNOWN;
  }
}

ProviderResult OWMProvider::deserializeOneCall(Stream &json, forecast_t &forecast, std::vector<weather_alert_t> *alerts) {
  int i;
  JsonDocument filter;
  filter["current"] = true;
  filter["minutely"] = false;
  filter["hourly"] = true;
  filter["daily"] = true;
#if !defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  filter["alerts"] = false;
#else
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
  LOG_DEBUG("doc.overflowed() : %s", doc.overflowed() ? "true" : "false");
  if (LogLevel::TRACE >= g_logLevel) {
    LOG_TRACE("pretty JSON dump:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  if (error) {
    return mapDeserializationError(error);
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
  forecast.current.weather.condition = mapWeatherCode(current_weather["id"].as<int>());
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
    forecast.hourly[i].weather.condition = mapWeatherCode(hourly_weather["id"].as<int>());
    forecast.hourly[i].is_day = hourly_weather["icon"].as<String>().endsWith("d");
    if (i == NUM_HOURLY - 1) break;
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
    forecast.daily[i].weather.condition = mapWeatherCode(daily_weather["id"].as<int>());
    if (i == NUM_DAILY - 1) break;
    ++i;
  }

#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  if (alerts != nullptr) {
    i = 0;
    for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
      weather_alert_t new_alert = {};
      new_alert.event = alert["event"].as<const char *>();
      new_alert.start = alert["start"].as<int64_t>();
      new_alert.end = alert["end"].as<int64_t>();
      new_alert.tags = alert["tags"][0].as<const char *>();
      alerts->push_back(new_alert);
      if (i == OWM_NUM_ALERTS - 1) break;
      ++i;
    }
  }
#endif

  return mapDeserializationError(error);
}

ProviderResult OWMProvider::deserializeAlerts(Stream &json, std::vector<weather_alert_t> &alerts) {
  JsonDocument filter;
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
    return mapDeserializationError(error);
  }
  int i = 0;
  for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
    weather_alert_t new_alert = {};
    new_alert.event = alert["event"].as<const char *>();
    new_alert.start = alert["start"].as<int64_t>();
    new_alert.end = alert["end"].as<int64_t>();
    new_alert.tags = alert["tags"][0].as<const char *>();
    alerts.push_back(new_alert);
    if (i == OWM_NUM_ALERTS - 1) break;
    ++i;
  }
  return mapDeserializationError(error);
}
