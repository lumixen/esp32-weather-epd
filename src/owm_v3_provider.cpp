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
#include <cstdint>
#include <cstring>
#include "cert.h"
#include "_locale.h"
#include "esp_http_client_stream.h"
#include "esp_http_client_utils.h"
#include "json_stream_utils.h"
#include "owm_v3_provider.h"
#include "provider_fetch_operations.h"

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
  const char *scheme = "http://";
#elif defined(OPENWEATHERMAP_ONECALL_V3_TRANSPORT_HTTPS_NO_VERIFY)
  const char *scheme = "https://";
#else
  const char *scheme = "https://";
#endif
  String uri = "/data/3.0/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG + "&units=metric&exclude=minutely";
  String sanitizedUrl = String(scheme) + OWM_ENDPOINT + uri + "&appid={API key}";
  uri += "&appid=" + OPENWEATHERMAP_ONECALL_V3_API_KEY;
  String url = String(scheme) + OWM_ENDPOINT + uri;

  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
#if defined(OPENWEATHERMAP_ONECALL_V3_TRANSPORT_HTTPS_VERIFY)
  config.cert_pem = cert_USERTrust_RSA_Certification_Authority;
#endif

  return espHttpGetWithRetry(url, sanitizedUrl, config, [&report](esp_http_client_handle_t client) {
    EspHttpClientStream stream(client);
    ProviderResult result = deserializeOneCall(stream, report);
    if (stream.hadReadError())
      return espHttpErrorResult(stream.readError());
    return result;
  });
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

namespace {

static bool keyIs(ElementSelector *selector, const char *key) {
  return selector != nullptr && selector->isObject() && selector->getKey() != nullptr &&
         strcmp(selector->getKey(), key) == 0;
}

static bool numeric(ElementValue value) { return value.isInt() || value.isFloat(); }

static double numberOrZero(ElementValue value) { return numeric(value) ? value.getDouble() : 0.0; }

static bool arrayIndex(ElementSelector *selector, int &index) {
  if (selector == nullptr || selector->isObject())
    return false;
  index = selector->getIndex();
  return index >= 0;
}

/* Maps the One Call v3 response directly into the application model. The
 * parser deliberately consumes the complete JSON document, while fields
 * outside the model are ignored without allocating a DOM. */
class OneCallV3Handler : public JsonHandler {
 public:
  OneCallV3Handler(weather_report_t &report, std::vector<weather_alert_t> &alerts)
      : forecast_(report.forecast), alerts_(alerts) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath path) override {
    if (path.getCount() != 2 || !keyIs(path.get(0), "alerts"))
      return;
    int index;
    if (!arrayIndex(path.get(1), index) || index >= OWM_NUM_ALERTS)
      return;
    activeAlertIndex_ = index;
    activeAlertData_ = {};
    activeAlert_ = true;
  }
  void endObject(ElementPath path) override {
    if (!activeAlert_ || path.getCount() != 2 || !keyIs(path.get(0), "alerts"))
      return;
    int index;
    if (!arrayIndex(path.get(1), index) || index != activeAlertIndex_)
      return;
    alerts_.push_back(activeAlertData_);
    activeAlert_ = false;
  }
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() == 1) {
      storeMetadata(path.getCurrent(), value);
      return;
    }
    if (path.getCount() >= 3 && keyIs(path.get(0), "alerts")) {
      storeAlert(path, value);
      return;
    }
    if (keyIs(path.get(0), "current")) {
      storeCurrent(path, value);
      return;
    }

    int index;
    if (path.getCount() < 3 || !arrayIndex(path.get(1), index))
      return;
    if (keyIs(path.get(0), "hourly"))
      storeHourly(path, index, value);
    else if (keyIs(path.get(0), "daily"))
      storeDaily(path, index, value);
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }

 private:
  void storeMetadata(ElementSelector *field, ElementValue value) {
    if (field == nullptr)
      return;
    if (keyIs(field, "lat"))
      forecast_.lat = static_cast<float>(numberOrZero(value));
    else if (keyIs(field, "lon"))
      forecast_.lon = static_cast<float>(numberOrZero(value));
    else if (keyIs(field, "timezone") && value.isString())
      forecast_.timezone = value.getString();
    else if (keyIs(field, "timezone_offset"))
      forecast_.timezone_offset = static_cast<int>(numberOrZero(value));
  }

  void storeCurrent(ElementPath path, ElementValue value) {
    if (path.getCount() == 2) {
      ElementSelector *field = path.getCurrent();
      if (keyIs(field, "dt"))
        forecast_.current.dt = static_cast<int64_t>(numberOrZero(value));
      else if (numeric(value) || value.isNull())
        storeCurrentNumber(field, numberOrZero(value));
      return;
    }
    if (path.getCount() == 3 && (keyIs(path.get(1), "rain") || keyIs(path.get(1), "snow"))) {
      if (!keyIs(path.getCurrent(), "1h"))
        return;
      if (keyIs(path.get(1), "rain"))
        forecast_.current.rain_1h = static_cast<float>(numberOrZero(value));
      else
        forecast_.current.snow_1h = static_cast<float>(numberOrZero(value));
      return;
    }
    if (path.getCount() == 4 && keyIs(path.get(1), "weather")) {
      int index;
      if (!arrayIndex(path.get(2), index) || index != 0)
        return;
      if (keyIs(path.getCurrent(), "id"))
        forecast_.current.weather.condition = OpenWeatherMapOneCallV3Provider::mapWeatherCode(numberOrZero(value));
      else if (keyIs(path.getCurrent(), "icon") && value.isString())
        forecast_.current.is_day = String(value.getString()).endsWith("d");
    }
  }

  static void storeCurrentNumber(ElementSelector *field, double value, current_t &current) {
    if (keyIs(field, "temp"))
      current.temp = static_cast<float>(value);
    else if (keyIs(field, "feels_like"))
      current.feels_like = static_cast<float>(value);
    else if (keyIs(field, "pressure"))
      current.pressure = static_cast<int>(value);
    else if (keyIs(field, "humidity"))
      current.humidity = static_cast<int>(value);
    else if (keyIs(field, "dew_point"))
      current.dew_point = static_cast<float>(value);
    else if (keyIs(field, "clouds"))
      current.clouds = static_cast<int>(value);
    else if (keyIs(field, "uvi"))
      current.uvi = static_cast<float>(value);
    else if (keyIs(field, "visibility"))
      current.visibility = static_cast<int>(value);
    else if (keyIs(field, "wind_speed"))
      current.wind_speed = static_cast<float>(value);
    else if (keyIs(field, "wind_gust"))
      current.wind_gust = static_cast<float>(value);
    else if (keyIs(field, "wind_deg"))
      current.wind_deg = static_cast<int>(value);
  }

  void storeCurrentNumber(ElementSelector *field, double value) { storeCurrentNumber(field, value, forecast_.current); }

  void storeHourly(ElementPath path, int index, ElementValue value) {
    if (index >= NUM_HOURLY)
      return;
    hourly_t &hourly = forecast_.hourly[index];
    if (path.getCount() == 3) {
      ElementSelector *field = path.getCurrent();
      if (keyIs(field, "dt"))
        hourly.dt = static_cast<int64_t>(numberOrZero(value));
      else if (numeric(value) || value.isNull())
        storeHourlyNumber(field, numberOrZero(value), hourly);
      return;
    }
    if (path.getCount() == 4 && (keyIs(path.get(2), "rain") || keyIs(path.get(2), "snow"))) {
      if (!keyIs(path.getCurrent(), "1h"))
        return;
      if (keyIs(path.get(2), "rain"))
        hourly.rain_1h = static_cast<float>(numberOrZero(value));
      else
        hourly.snow_1h = static_cast<float>(numberOrZero(value));
      return;
    }
    if (path.getCount() == 5 && keyIs(path.get(2), "weather")) {
      int weatherIndex;
      if (!arrayIndex(path.get(3), weatherIndex) || weatherIndex != 0)
        return;
      if (keyIs(path.getCurrent(), "id"))
        hourly.weather.condition = OpenWeatherMapOneCallV3Provider::mapWeatherCode(numberOrZero(value));
      else if (keyIs(path.getCurrent(), "icon") && value.isString())
        hourly.is_day = String(value.getString()).endsWith("d");
    }
  }

  static void storeHourlyNumber(ElementSelector *field, double value, hourly_t &hourly) {
    if (keyIs(field, "temp"))
      hourly.temp = static_cast<float>(value);
    else if (keyIs(field, "feels_like"))
      hourly.feels_like = static_cast<float>(value);
    else if (keyIs(field, "pressure"))
      hourly.pressure = static_cast<int>(value);
    else if (keyIs(field, "humidity"))
      hourly.humidity = static_cast<int>(value);
    else if (keyIs(field, "dew_point"))
      hourly.dew_point = static_cast<float>(value);
    else if (keyIs(field, "clouds"))
      hourly.clouds = static_cast<int>(value);
    else if (keyIs(field, "uvi"))
      hourly.uvi = static_cast<float>(value);
    else if (keyIs(field, "visibility"))
      hourly.visibility = static_cast<int>(value);
    else if (keyIs(field, "wind_speed"))
      hourly.wind_speed = static_cast<float>(value);
    else if (keyIs(field, "wind_gust"))
      hourly.wind_gust = static_cast<float>(value);
    else if (keyIs(field, "wind_deg"))
      hourly.wind_deg = static_cast<int>(value);
    else if (keyIs(field, "pop"))
      hourly.pop = static_cast<int>(value * 100.0);
  }

  void storeDaily(ElementPath path, int index, ElementValue value) {
    if (index >= NUM_DAILY)
      return;
    daily_t &daily = forecast_.daily[index];
    if (path.getCount() == 3) {
      ElementSelector *field = path.getCurrent();
      if (keyIs(field, "dt"))
        daily.dt = static_cast<int64_t>(numberOrZero(value));
      else if (numeric(value) || value.isNull())
        storeDailyNumber(field, numberOrZero(value), daily);
      return;
    }
    if (path.getCount() == 4 && keyIs(path.get(2), "temp")) {
      if (!numeric(value))
        return;
      ElementSelector *field = path.getCurrent();
      if (keyIs(field, "morn"))
        daily.temp.morn = static_cast<float>(value.getDouble());
      else if (keyIs(field, "day"))
        daily.temp.day = static_cast<float>(value.getDouble());
      else if (keyIs(field, "eve"))
        daily.temp.eve = static_cast<float>(value.getDouble());
      else if (keyIs(field, "night"))
        daily.temp.night = static_cast<float>(value.getDouble());
      else if (keyIs(field, "min"))
        daily.temp.min = static_cast<float>(value.getDouble());
      else if (keyIs(field, "max"))
        daily.temp.max = static_cast<float>(value.getDouble());
      return;
    }
    if (path.getCount() == 5 && keyIs(path.get(2), "weather")) {
      int weatherIndex;
      if (!arrayIndex(path.get(3), weatherIndex) || weatherIndex != 0)
        return;
      if (keyIs(path.getCurrent(), "id"))
        daily.weather.condition = OpenWeatherMapOneCallV3Provider::mapWeatherCode(numberOrZero(value));
    }
  }

  static void storeDailyNumber(ElementSelector *field, double value, daily_t &daily) {
    if (keyIs(field, "pressure"))
      daily.pressure = static_cast<int>(value);
    else if (keyIs(field, "humidity"))
      daily.humidity = static_cast<int>(value);
    else if (keyIs(field, "dew_point"))
      daily.dew_point = static_cast<float>(value);
    else if (keyIs(field, "clouds"))
      daily.clouds = static_cast<int>(value);
    else if (keyIs(field, "uvi"))
      daily.uvi = static_cast<float>(value);
    else if (keyIs(field, "visibility"))
      daily.visibility = static_cast<int>(value);
    else if (keyIs(field, "wind_speed"))
      daily.wind_speed = static_cast<float>(value);
    else if (keyIs(field, "wind_gust"))
      daily.wind_gust = static_cast<float>(value);
    else if (keyIs(field, "wind_deg"))
      daily.wind_deg = static_cast<int>(value);
    else if (keyIs(field, "pop"))
      daily.pop = static_cast<int>(value * 100.0);
    else if (keyIs(field, "rain"))
      daily.rain = static_cast<float>(value);
    else if (keyIs(field, "snow"))
      daily.snow = static_cast<float>(value);
  }

  void storeAlert(ElementPath path, ElementValue value) {
    if (!activeAlert_ || path.getCount() < 3)
      return;
    if (path.getCount() == 3) {
      ElementSelector *field = path.getCurrent();
      if (keyIs(field, "event") && value.isString())
        activeAlertData_.event = value.getString();
      else if (keyIs(field, "start"))
        activeAlertData_.start = static_cast<int64_t>(numberOrZero(value));
      else if (keyIs(field, "end"))
        activeAlertData_.end = static_cast<int64_t>(numberOrZero(value));
      return;
    }
    if (path.getCount() == 4 && keyIs(path.get(2), "tags")) {
      int index;
      if (arrayIndex(path.get(3), index) && index == 0 && value.isString())
        activeAlertData_.tags = value.getString();
    }
  }

  forecast_t &forecast_;
  std::vector<weather_alert_t> &alerts_;
  weather_alert_t activeAlertData_{};
  int activeAlertIndex_ = -1;
  bool activeAlert_ = false;
  bool sawStart_ = false;
  bool documentDone_ = false;
};

}  // namespace

ProviderResult OpenWeatherMapOneCallV3Provider::deserializeOneCall(Stream &json, weather_report_t &report) {
  report.resetForecast();
  report.resetAlerts();
  std::vector<weather_alert_t> &alerts = report.engageAlerts();
  OneCallV3Handler handler(report, alerts);
  ProviderResult result = consumeJsonStream(
      json, handler, [&handler]() { return handler.finishedDocument(); }, [&handler]() { return handler.sawStart(); },
      "OpenWeatherMap One Call v3", true);
  if (!result.isOk()) {
    report.resetForecast();
    report.resetAlerts();
  }
  return result;
}

#endif  // REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V3
