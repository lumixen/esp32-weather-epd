/* MeteoSwiss App forecast provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 */

#include "config.h"
#include "logger.h"

#if defined(REMOTE_PROVIDER_METEOSWISS_FORECAST)

#include <Arduino.h>
#include <ArduinoStreamParser.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include "_locale.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_client_stream.h"
#include "esp_http_client_utils.h"
#include "iso8601.h"
#include "meteo_swiss_forecast_provider.h"
#include "provider_fetch_operations.h"

namespace {

constexpr const char *kUserAgent = "esp32-weather-epd/meteoswiss";
constexpr int kGraphCapacity = 256;
constexpr int kDailyCapacity = 16;

bool keyIs(const char *actual, const char *wanted) { return actual != nullptr && strcmp(actual, wanted) == 0; }

const char *keyAt(ElementPath &path, int index) {
  ElementSelector *selector = path.get(index);
  return selector == nullptr ? nullptr : selector->getKey();
}

int indexAt(ElementPath &path, int index) {
  ElementSelector *selector = path.get(index);
  return selector == nullptr ? -1 : selector->getIndex();
}

bool numeric(ElementValue value) { return value.isInt() || value.isFloat(); }

int64_t epochMillis(double value) { return static_cast<int64_t>(value / 1000.0); }

int64_t localMidnight(const String &date) {
  if (date.length() != 10 || date.charAt(4) != '-' || date.charAt(7) != '-')
    return -1;
  for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
    if (date.charAt(i) < '0' || date.charAt(i) > '9')
      return -1;

  // The configured TZ is also used by the clock and renderer. mktime() is
  // deliberately used here instead of applying a fixed offset: Switzerland
  // changes between CET and CEST.
  setenv("TZ", TIMEZONE, 1);
  tzset();
  struct tm value = {};
  value.tm_year = date.substring(0, 4).toInt() - 1900;
  value.tm_mon = date.substring(5, 7).toInt() - 1;
  value.tm_mday = date.substring(8, 10).toInt();
  value.tm_isdst = -1;
  const time_t result = mktime(&value);
  return result == static_cast<time_t>(-1) ? -1 : static_cast<int64_t>(result);
}

struct MeteoSwissDailyData {
  String dayDate;
  float maximum = 0.0f;
  float minimum = 0.0f;
  float precipitation = 0.0f;
  int icon = -1;
  int iconV2 = -1;
  bool hasMaximum = false;
  bool hasMinimum = false;
  bool hasPrecipitation = false;
  bool hasIcon = false;
  bool hasIconV2 = false;
};

class ForecastHandler : public JsonHandler {
 public:
  explicit ForecastHandler(forecast_t &forecast)
      : forecast_(forecast),
        graphTemperature_(kGraphCapacity, NAN),
        graphWind_(kGraphCapacity, NAN),
        graphGust_(kGraphCapacity, NAN),
        graphDirection_(kGraphCapacity, NAN),
        graphIcon_(kGraphCapacity, -1),
        graphIconV2_(kGraphCapacity, -1),
        graphPop_(kGraphCapacity, NAN),
        graphRain1h_(kGraphCapacity, NAN),
        graphRain10m_(kGraphCapacity, NAN),
        daily_(kDailyCapacity) {}

  void startDocument() override { started_ = true; }
  void endDocument() override { finished_ = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    const int depth = path.getCount();
    if (depth == 2) {
      const char *section = keyAt(path, 0);
      const char *field = keyAt(path, 1);
      if (keyIs(section, "currentWeather")) {
        if (keyIs(field, "time") && numeric(value)) {
          currentTimeMs_ = value.getDouble();
          hasCurrentTime_ = true;
        } else if (keyIs(field, "temperature") && numeric(value)) {
          forecast_.current.temp = value.getDouble();
          hasCurrentTemperature_ = true;
        } else if (keyIs(field, "icon") && numeric(value)) {
          currentIcon_ = static_cast<int>(value.getDouble());
          hasCurrentIcon_ = true;
        } else if (keyIs(field, "iconV2") && numeric(value)) {
          currentIconV2_ = static_cast<int>(value.getDouble());
          hasCurrentIconV2_ = true;
        }
      } else if (keyIs(section, "graph") && numeric(value)) {
        if (keyIs(field, "start")) {
          graphStartMs_ = value.getDouble();
          hasGraphStart_ = true;
        } else if (keyIs(field, "startLowResolution")) {
          graphStartLowMs_ = value.getDouble();
          hasGraphStartLow_ = true;
        }
      }
      return;
    }

    if (depth != 3)
      return;
    const char *section = keyAt(path, 0);
    const bool forecastObject = keyIs(section, "forecast");
    const char *field = keyAt(path, forecastObject ? 2 : 1);
    const int index = indexAt(path, forecastObject ? 1 : 2);
    if (index < 0)
      return;

    if (keyIs(section, "forecast")) {
      if (index >= kDailyCapacity)
        return;
      MeteoSwissDailyData &day = daily_[index];
      if (keyIs(field, "dayDate") && value.isString()) {
        day.dayDate = value.getString();
      } else if (!numeric(value)) {
        return;
      } else if (keyIs(field, "temperatureMax")) {
        day.maximum = value.getDouble();
        day.hasMaximum = true;
      } else if (keyIs(field, "temperatureMin")) {
        day.minimum = value.getDouble();
        day.hasMinimum = true;
      } else if (keyIs(field, "precipitation")) {
        day.precipitation = value.getDouble();
        day.hasPrecipitation = true;
      } else if (keyIs(field, "iconDay")) {
        day.icon = static_cast<int>(value.getDouble());
        day.hasIcon = true;
      } else if (keyIs(field, "iconDayV2")) {
        day.iconV2 = static_cast<int>(value.getDouble());
        day.hasIconV2 = true;
      }
      return;
    }

    if (!keyIs(section, "graph") || index >= kGraphCapacity || !numeric(value))
      return;
    const double number = value.getDouble();
    if (keyIs(field, "temperatureMean1h"))
      graphTemperature_[index] = number;
    else if (keyIs(field, "windSpeed1h"))
      graphWind_[index] = number;
    else if (keyIs(field, "gustSpeed1h"))
      graphGust_[index] = number;
    else if (keyIs(field, "windDirection3h"))
      graphDirection_[index] = number;
    else if (keyIs(field, "weatherIcon3h"))
      graphIcon_[index] = static_cast<int>(number);
    else if (keyIs(field, "weatherIcon3hV2"))
      graphIconV2_[index] = static_cast<int>(number);
    else if (keyIs(field, "precipitationProbability3h"))
      graphPop_[index] = number;
    else if (keyIs(field, "precipitation1h"))
      graphRain1h_[index] = number;
    else if (keyIs(field, "precipitation10m"))
      graphRain10m_[index] = number;
  }

  bool started() const { return started_; }
  bool finished() const { return finished_; }
  bool complete() const {
    if (!hasCurrentTime_ || !hasCurrentTemperature_ || !(hasCurrentIconV2_ || hasCurrentIcon_) || !hasGraphStart_ ||
        !hasGraphStartLow_) {
      return false;
    }
    for (int i = 0; i < NUM_DAILY; ++i) {
      if (daily_[i].dayDate.isEmpty() || !daily_[i].hasMaximum || !daily_[i].hasMinimum ||
          !daily_[i].hasPrecipitation || !(daily_[i].hasIconV2 || daily_[i].hasIcon)) {
        return false;
      }
    }
    return true;
  }

  int64_t currentTime() const { return epochMillis(currentTimeMs_); }
  int64_t graphStart() const { return epochMillis(graphStartMs_); }
  int64_t graphStartLow() const { return epochMillis(graphStartLowMs_); }
  int currentIcon() const { return hasCurrentIconV2_ ? currentIconV2_ : currentIcon_; }
  const std::vector<float> &temperature() const { return graphTemperature_; }
  const std::vector<float> &wind() const { return graphWind_; }
  const std::vector<float> &gust() const { return graphGust_; }
  const std::vector<float> &direction() const { return graphDirection_; }
  const std::vector<int> &icon() const { return graphIconV2_; }
  const std::vector<int> &iconFallback() const { return graphIcon_; }
  const std::vector<float> &pop() const { return graphPop_; }
  const std::vector<float> &rain1h() const { return graphRain1h_; }
  const std::vector<float> &rain10m() const { return graphRain10m_; }
  const std::vector<MeteoSwissDailyData> &daily() const { return daily_; }

 private:
  forecast_t &forecast_;
  std::vector<float> graphTemperature_;
  std::vector<float> graphWind_;
  std::vector<float> graphGust_;
  std::vector<float> graphDirection_;
  std::vector<int> graphIcon_;
  std::vector<int> graphIconV2_;
  std::vector<float> graphPop_;
  std::vector<float> graphRain1h_;
  std::vector<float> graphRain10m_;
  std::vector<MeteoSwissDailyData> daily_;
  double currentTimeMs_ = 0.0;
  double graphStartMs_ = 0.0;
  double graphStartLowMs_ = 0.0;
  int currentIcon_ = -1;
  int currentIconV2_ = -1;
  bool hasCurrentTime_ = false;
  bool hasCurrentTemperature_ = false;
  bool hasCurrentIcon_ = false;
  bool hasCurrentIconV2_ = false;
  bool hasGraphStart_ = false;
  bool hasGraphStartLow_ = false;
  bool started_ = false;
  bool finished_ = false;
};

ProviderResult parseJson(Stream &json, ForecastHandler &handler) {
  ArduinoStreamParser parser;
  parser.setHandler(&handler);
  uint8_t buffer[256];
  while (!parser.hasParseError() && !handler.finished()) {
    const size_t count = json.readBytes(buffer, sizeof(buffer));
    if (count == 0)
      break;
    // A read may bring bytes beyond the closing brace into the local buffer.
    // Feed them one at a time so a valid document followed by an unrelated
    // suffix is not mistaken for malformed JSON.
    for (size_t i = 0; i < count && !handler.finished() && !parser.hasParseError(); ++i)
      parser.write(buffer + i, 1);
  }
  if (parser.hasParseError()) {
    LOG_WARNING("MeteoSwiss JSON parse error: %s", parser.getErrorMessage());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (!handler.finished())
    return ProviderResult::error(handler.started() ? TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT
                                                   : TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  return ProviderResult::ok();
}

bool finiteAt(const std::vector<float> &values, int index) {
  return index >= 0 && index < static_cast<int>(values.size()) && std::isfinite(values[index]);
}

int iconAt(const ForecastHandler &handler, int index) {
  const std::vector<int> &v2 = handler.icon();
  if (index >= 0 && index < static_cast<int>(v2.size()) && v2[index] >= 0)
    return v2[index];
  const std::vector<int> &original = handler.iconFallback();
  return index >= 0 && index < static_cast<int>(original.size()) ? original[index] : -1;
}

bool parseCsvNumber(const String &text, float &value) {
  String trimmed = text;
  trimmed.trim();
  if (trimmed.isEmpty() || trimmed == "-")
    return false;
  char *end = nullptr;
  const double parsed = strtod(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0' || !std::isfinite(parsed))
    return false;
  value = static_cast<float>(parsed);
  return true;
}

bool parseCsvTimestamp(const String &text, int64_t &timestamp) {
  if (text.length() != 12)
    return false;
  for (int i = 0; i < text.length(); ++i)
    if (text.charAt(i) < '0' || text.charAt(i) > '9')
      return false;
  String iso = text.substring(0, 4) + "-" + text.substring(4, 6) + "-" + text.substring(6, 8) + "T" +
               text.substring(8, 10) + ":" + text.substring(10, 12) + ":00Z";
  return iso8601::parse(iso.c_str(), timestamp);
}

bool csvField(const String &line, int column, int &start, String &field) {
  start = 0;
  for (int i = 0; i <= column; ++i) {
    const int separator = line.indexOf(';', start);
    if (i == column) {
      field = separator < 0 ? line.substring(start) : line.substring(start, separator);
      return true;
    }
    if (separator < 0)
      return false;
    start = separator + 1;
  }
  return false;
}

}  // namespace

const char *MeteoSwissForecastProvider::getApiName() const { return "MeteoSwiss API"; }

weather_condition MeteoSwissForecastProvider::mapWeatherCode(int code) {
  if (code >= 101 && code <= 142)
    code -= 100;
  switch (code) {
    case 1:
      return weather_condition::CLEAR;
    case 2:
    case 26:
      return weather_condition::PARTLY_CLOUDY;
    case 3:
    case 4:
      return weather_condition::CLOUDY;
    case 5:
    case 35:
      return weather_condition::OVERCAST;
    case 6:
    case 29:
    case 32:
    case 33:
      return weather_condition::RAIN_SHOWERS;
    case 7:
    case 10:
    case 15:
    case 18:
    case 21:
    case 31:
      return weather_condition::RAIN_SNOW_MIX;
    case 8:
    case 11:
    case 30:
    case 34:
      return weather_condition::SNOW_SHOWERS;
    case 9:
    case 17:
    case 20:
      return weather_condition::RAIN;
    case 14:
      return weather_condition::DRIZZLE;
    case 16:
    case 19:
    case 22:
      return weather_condition::SNOW;
    case 12:
    case 13:
    case 23:
    case 24:
    case 25:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
      return weather_condition::THUNDERSTORM;
    case 27:
    case 28:
      return weather_condition::FOG;
    default:
      return weather_condition::UNKNOWN;
  }
}

bool MeteoSwissForecastProvider::isDayIcon(int code) { return code >= 1 && code <= 42; }

ProviderResult MeteoSwissForecastProvider::deserializeForecast(Stream &json, forecast_t &forecast) {
  forecast.reset();
  ForecastHandler handler(forecast);
  ProviderResult result = parseJson(json, handler);
  if (!result.isOk() || !handler.complete()) {
    forecast.reset();
    return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
  }

  const int64_t graphStart = handler.graphStart();
  const int64_t graphStartLow = handler.graphStartLow();
  const int64_t current = handler.currentTime();
  auto invalid = [&forecast]() {
    forecast.reset();
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  };
  if (graphStart < 0 || graphStartLow < graphStart || current < 0 || (graphStartLow - graphStart) % 3600 != 0)
    return invalid();

  // The App graph contains the current/partly historical point at index zero.
  // The display starts at the next full hour, never at a historical point.
  const int64_t firstHour = (current / 3600 + 1) * 3600;
  if (firstHour < graphStart || (firstHour - graphStart) % 3600 != 0)
    return invalid();
  const int firstIndex = static_cast<int>((firstHour - graphStart) / 3600);
  const int prefixHours = static_cast<int>((graphStartLow - graphStart) / 3600);
  if (prefixHours < 0 || prefixHours * 6 > static_cast<int>(handler.rain10m().size()))
    return invalid();

  for (int i = 0; i < NUM_HOURLY; ++i) {
    const int source = firstIndex + i;
    const int threeHour = source / 3;
    if (!finiteAt(handler.temperature(), source) || !finiteAt(handler.wind(), source) ||
        !finiteAt(handler.gust(), source) || !finiteAt(handler.direction(), threeHour) ||
        !finiteAt(handler.pop(), threeHour) || iconAt(handler, threeHour) < 0)
      return invalid();
    if (source < prefixHours) {
      for (int tenMinute = source * 6; tenMinute < source * 6 + 6; ++tenMinute)
        if (!finiteAt(handler.rain10m(), tenMinute))
          return invalid();
    } else if (!finiteAt(handler.rain1h(), source - prefixHours)) {
      return invalid();
    }
  }

  forecast.current.dt = current;
  forecast.current.feels_like = forecast.current.temp;
  forecast.current.weather.condition = mapWeatherCode(handler.currentIcon());
  forecast.current.is_day = isDayIcon(handler.currentIcon());
  forecast.current.wind_speed = handler.wind()[firstIndex] / 3.6f;
  forecast.current.wind_gust = handler.gust()[firstIndex] / 3.6f;
  forecast.current.wind_deg = static_cast<int>(handler.direction()[firstIndex / 3]);

  for (int i = 0; i < NUM_HOURLY; ++i) {
    const int source = firstIndex + i;
    const int threeHour = source / 3;
    hourly_t &out = forecast.hourly[i];
    out.dt = graphStart + static_cast<int64_t>(source) * 3600;
    out.temp = handler.temperature()[source];
    out.feels_like = out.temp;
    out.wind_speed = handler.wind()[source] / 3.6f;
    out.wind_gust = handler.gust()[source] / 3.6f;
    out.wind_deg = static_cast<int>(handler.direction()[threeHour]);
    out.pop = static_cast<int>(handler.pop()[threeHour]);
    out.weather.condition = mapWeatherCode(iconAt(handler, threeHour));
    out.is_day = isDayIcon(iconAt(handler, threeHour));
    if (source < prefixHours) {
      float total = 0.0f;
      for (int tenMinute = source * 6; tenMinute < source * 6 + 6; ++tenMinute)
        total += handler.rain10m()[tenMinute];
      out.rain_1h = total;
    } else {
      out.rain_1h = handler.rain1h()[source - prefixHours];
    }
  }

  for (int i = 0; i < NUM_DAILY; ++i) {
    const MeteoSwissDailyData &day = handler.daily()[i];
    const int icon = day.hasIconV2 ? day.iconV2 : day.icon;
    daily_t &out = forecast.daily[i];
    out.dt = localMidnight(day.dayDate);
    out.temp.max = day.maximum;
    out.temp.min = day.minimum;
    out.temp.day = day.maximum;
    out.temp.night = day.minimum;
    out.rain = day.precipitation;
    out.weather.condition = mapWeatherCode(icon);
    if (out.dt < 0)
      return invalid();
  }
  return ProviderResult::ok();
}

ProviderResult MeteoSwissForecastProvider::deserializeObservationCsv(Stream &csv, const String &stationId,
                                                                     current_t &current) {
  int stationColumn = -1;
  int dateColumn = -1;
  int columns[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
  const char *names[] = {"tre200s0", "ure200s0", "tde200s0", "dkl010z0", "fu3010z0",
                         "fu3010z1", "pp0qnhs0", "pp0qffs0", "prestas0"};
  bool sawInput = false;
  bool sawHeader = false;
  bool stationFound = false;
  String line;

  auto processLine = [&](const String &raw) -> ProviderResult {
    String value = raw;
    value.trim();
    if (value.isEmpty())
      return ProviderResult::ok();
    sawInput = true;
    if (!sawHeader) {
      if (value.startsWith("\xEF\xBB\xBF"))
        value = value.substring(3);
      int start = 0;
      int index = 0;
      while (start <= value.length()) {
        String field;
        int ignored;
        csvField(value, index, ignored, field);
        field.trim();
        if (field == "Station/Location")
          stationColumn = index;
        else if (field == "Date")
          dateColumn = index;
        for (int i = 0; i < 9; ++i)
          if (field == names[i])
            columns[i] = index;
        const int separator = value.indexOf(';', start);
        if (separator < 0)
          break;
        start = separator + 1;
        ++index;
      }
      sawHeader = stationColumn >= 0 && dateColumn >= 0;
      return sawHeader ? ProviderResult::ok() : ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    }

    String station;
    int ignored;
    if (!csvField(value, stationColumn, ignored, station) || station != stationId)
      return ProviderResult::ok();
    String timestampText;
    if (!csvField(value, dateColumn, ignored, timestampText))
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    int64_t timestamp = 0;
    if (!parseCsvTimestamp(timestampText, timestamp))
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    current.dt = timestamp;

    float parsed = 0.0f;
    String field;
    if (columns[0] >= 0 && csvField(value, columns[0], ignored, field) && parseCsvNumber(field, parsed)) {
      current.temp = parsed;
      current.feels_like = parsed;
    }
    if (columns[1] >= 0 && csvField(value, columns[1], ignored, field) && parseCsvNumber(field, parsed))
      current.humidity = static_cast<int>(parsed);
    if (columns[2] >= 0 && csvField(value, columns[2], ignored, field) && parseCsvNumber(field, parsed))
      current.dew_point = parsed;
    if (columns[3] >= 0 && csvField(value, columns[3], ignored, field) && parseCsvNumber(field, parsed))
      current.wind_deg = static_cast<int>(parsed);
    if (columns[4] >= 0 && csvField(value, columns[4], ignored, field) && parseCsvNumber(field, parsed))
      current.wind_speed = parsed / 3.6f;
    if (columns[5] >= 0 && csvField(value, columns[5], ignored, field) && parseCsvNumber(field, parsed))
      current.wind_gust = parsed / 3.6f;

    float pressure = 0.0f;
    bool hasPressure = false;
    if (columns[6] >= 0 && csvField(value, columns[6], ignored, field) && parseCsvNumber(field, parsed)) {
      pressure = parsed;
      hasPressure = true;
    }
    if (!hasPressure && columns[7] >= 0 && csvField(value, columns[7], ignored, field) &&
        parseCsvNumber(field, parsed)) {
      pressure = parsed;
      hasPressure = true;
    }
    if (!hasPressure && columns[8] >= 0 && csvField(value, columns[8], ignored, field) &&
        parseCsvNumber(field, parsed)) {
      pressure = parsed;
      hasPressure = true;
    }
    if (hasPressure)
      current.pressure = static_cast<int>(pressure);
    stationFound = true;
    return ProviderResult::ok();
  };

  while (true) {
    const int character = csv.read();
    if (character < 0) {
      if (!line.isEmpty()) {
        ProviderResult result = processLine(line);
        if (!result.isOk())
          return result;
      }
      break;
    }
    if (character == '\n') {
      ProviderResult result = processLine(line);
      line = String();
      if (!result.isOk())
        return result;
      // The observation file is ordered by station, but not guaranteed to
      // remain so. We intentionally continue only until the matching row;
      // the return below is handled by the station marker.
    } else if (character != '\r') {
      line += static_cast<char>(character);
    }

    // A matching station row is recognized by the timestamp being updated.
    // Keep parsing the current line through processLine, then stop on the
    // next loop iteration without buffering the rest of the CSV.
    if (stationFound)
      break;
  }

  if (!sawInput)
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  if (!sawHeader || !stationFound)
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  return ProviderResult::ok();
}

std::vector<std::unique_ptr<FetchOperation>> MeteoSwissForecastProvider::createFetchOperations(weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  auto forecast = std::make_unique<CallbackFetchOperation>("MeteoSwiss forecast", true, [this, &out]() {
    out.resetForecast();
    return fetchForecast(out.forecast);
  });
  FetchOperation *forecastOperation = forecast.get();
  operations.push_back(std::move(forecast));

  auto observation = std::make_unique<CallbackFetchOperation>("MeteoSwiss current observation", false, [this, &out]() {
    current_t candidate = out.forecast.current;
    ProviderResult result = fetchObservation(candidate);
    if (result.isOk())
      out.forecast.current = candidate;
    return result;
  });
  observation->dependsOn(*forecastOperation);
  operations.push_back(std::move(observation));
  return operations;
}

ProviderResult MeteoSwissForecastProvider::fetchForecast(forecast_t &forecast) {
  const String url =
      "https://" + String(METEOSWISS_FORECAST_ENDPOINT) + "/v1/plzDetail?plz=" + String(METEOSWISS_FORECAST_POINT_ID);
  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
  config.port = 443;
  config.user_agent = kUserAgent;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  ProviderResult result = espHttpGetWithRetry(
      url, url, config,
      [&forecast](esp_http_client_handle_t client) {
        EspHttpClientStream stream(client);
        ProviderResult result = MeteoSwissForecastProvider::deserializeForecast(stream, forecast);
        if (stream.hadReadError())
          return espHttpErrorResult(stream.readError());
        return result;
      },
      [](esp_http_client_handle_t client) {
        esp_http_client_set_header(client, "Accept", "application/json");
        esp_http_client_set_header(client, "Accept-Language", OWM_LANG.c_str());
      });
  if (result.isOk()) {
    forecast.lat = strtod(LAT.c_str(), nullptr);
    forecast.lon = strtod(LON.c_str(), nullptr);
    forecast.timezone = TIMEZONE;
    forecast.timezone_offset = 0;
  }
  return result;
}

ProviderResult MeteoSwissForecastProvider::fetchObservation(current_t &current) {
  const String url =
      "https://" + String(METEOSWISS_OBSERVATION_ENDPOINT) + "/ch.meteoschweiz.messwerte-aktuell/VQHA80.csv";
  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
  config.port = 443;
  config.user_agent = kUserAgent;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  return espHttpGetWithRetry(
      url, url, config,
      [&current](esp_http_client_handle_t client) {
        EspHttpClientStream stream(client);
        ProviderResult result =
            MeteoSwissForecastProvider::deserializeObservationCsv(stream, METEOSWISS_STATION_ID, current);
        if (stream.hadReadError())
          return espHttpErrorResult(stream.readError());
        return result;
      },
      [](esp_http_client_handle_t client) {
        esp_http_client_set_header(client, "Accept", "text/csv");
        esp_http_client_set_header(client, "Accept-Language", OWM_LANG.c_str());
      });
}

#endif  // REMOTE_PROVIDER_METEOSWISS_FORECAST
