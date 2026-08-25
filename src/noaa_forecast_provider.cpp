/* NOAA/National Weather Service forecast provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "logger.h"

#if defined(REMOTE_PROVIDER_NOAA_FORECAST)

#include <Arduino.h>
/* NWS station collections contain a long pagination.next URL (currently
 * roughly 826 bytes). The parser must consume that unknown field even though
 * the provider discards it, so use a larger but still bounded token buffer. */
#ifndef JSON_PARSER_BUFFER_MAX_LENGTH
#define JSON_PARSER_BUFFER_MAX_LENGTH 1024
#endif
#include <ArduinoStreamParser.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <WiFi.h>
#include "esp_http_client.h"
#if defined(NOAA_FORECAST_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include "_locale.h"
#include "display_utils.h"
#include "noaa_forecast_provider.h"
#include "provider_fetch_operations.h"

namespace {

constexpr const char *kEndpoint = "api.weather.gov";
constexpr const char *kUserAgent = "esp32-weather-epd";
constexpr const char *kAccept = "application/geo+json";
constexpr int kMaxStationCandidates = 3;

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool leapYear(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

bool validDate(int year, unsigned month, unsigned day) {
  static const unsigned days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return year >= 1970 && month >= 1 && month <= 12 && day >= 1 &&
         day <= days[month] + (month == 2 && leapYear(year) ? 1 : 0);
}

bool digits(const String &s, int start, int count) {
  if (start < 0 || start + count > static_cast<int>(s.length()))
    return false;
  for (int i = 0; i < count; ++i) {
    if (s.charAt(start + i) < '0' || s.charAt(start + i) > '9')
      return false;
  }
  return true;
}

const char *keyAt(ElementPath &path, int index) {
  ElementSelector *selector = path.get(index);
  return selector == nullptr ? nullptr : selector->getKey();
}

int indexAt(ElementPath &path, int index) {
  ElementSelector *selector = path.get(index);
  return selector == nullptr ? -1 : selector->getIndex();
}

bool keyIs(const char *actual, const char *wanted) { return actual != nullptr && strcmp(actual, wanted) == 0; }

float firstNumber(const String &text) {
  const char *start = text.c_str();
  while (*start != '\0' && ((*start < '0' || *start > '9') && *start != '-' && *start != '+' && *start != '.'))
    ++start;
  if (*start == '\0')
    return NAN;
  char *end = nullptr;
  const double value = strtod(start, &end);
  return end == start ? NAN : static_cast<float>(value);
}

float speedMs(float kmh) { return std::isnan(kmh) ? 0.0f : kmh / 3.6f; }

float observationSpeed(float value, const String &unit) {
  String code = unit;
  code.toLowerCase();
  if (code.indexOf("km_h") >= 0 || code.indexOf("km/h") >= 0 || code.indexOf("kilomet") >= 0)
    return value / 3.6f;
  if (code.indexOf("mile") >= 0 || code.indexOf("mi_h") >= 0 || code.indexOf("mph") >= 0)
    return value * 0.44704f;
  if (code.indexOf("knot") >= 0)
    return value * 0.514444f;
  return value;  // SI m/s, or an unknown unit already supplied by NWS.
}

int compassDegrees(const String &direction) {
  String d = direction;
  d.trim();
  char *end = nullptr;
  const double numeric = strtod(d.c_str(), &end);
  if (end != d.c_str() && *end == '\0')
    return static_cast<int>(std::lround(numeric));
  d.toUpperCase();
  static const char *names[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  for (int i = 0; i < 16; ++i) {
    if (d == names[i])
      return static_cast<int>(i * 22.5);
  }
  return 0;
}

String baseScheme(const String &url) { return url.startsWith("http://") ? "http" : "https"; }

/* Stream adapter around esp_http_client. The response body is never copied;
 * parser reads are forwarded directly to the bounded IDF client buffer. */
class HttpClientStream : public Stream {
 public:
  explicit HttpClientStream(esp_http_client_handle_t client) : client_(client) {}
  int available() override { return 1; }
  int read() override {
    uint8_t byte = 0;
    return readBytes(&byte, 1) == 1 ? byte : -1;
  }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 0; }
  size_t readBytes(char *buffer, size_t length) override {
    if (length == 0)
      return 0;
    const int n = esp_http_client_read(client_, buffer, static_cast<int>(length));
    if (n < 0)
      readError_ = true;
    return n > 0 ? static_cast<size_t>(n) : 0;
  }
  bool hadReadError() const { return readError_; }
  size_t readBytes(uint8_t *buffer, size_t length) { return readBytes(reinterpret_cast<char *>(buffer), length); }

 private:
  esp_http_client_handle_t client_;
  bool readError_ = false;
};

template<typename Complete, typename Started>
ProviderResult parseStreamingJson(Stream &json, JsonHandler &handler, Complete complete, Started started,
                                  const char *label) {
  ArduinoStreamParser parser;
  parser.setHandler(&handler);
  uint8_t buffer[256];
  while (!parser.hasParseError() && !complete()) {
    const size_t count = json.readBytes(buffer, sizeof(buffer));
    if (count == 0)
      break;
    parser.write(buffer, count);
  }
  if (parser.hasParseError()) {
    LOG_WARNING("NOAA %s JSON parse error: %s", label, parser.getErrorMessage());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (!complete())
    return ProviderResult::error(started() ? TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT
                                           : TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  return ProviderResult::ok();
}

ProviderResult requestNoaa(const String &url, std::function<ProviderResult(Stream &)> consume) {
  ProviderResult result;
  for (int attempt = 0; attempt < 3 && !result.isOk(); ++attempt) {
    if (WiFi.status() != WL_CONNECTED) {
      result = ProviderResult::error(getHttpResponsePhrase(-512 - static_cast<int>(WiFi.status())));
      break;
    }

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
    config.method = HTTP_METHOD_GET;
#if defined(NOAA_FORECAST_TRANSPORT_HTTPS_VERIFY)
    config.cert_pem = cert_NOAA_API_WEATHER_GOV;
#endif
    esp_http_client_handle_t client = esp_http_client_init(&config);
    int status = 0;
    if (client == nullptr) {
      result = ProviderResult::error(esp_err_to_name(ESP_FAIL));
    } else {
      esp_http_client_set_header(client, "User-Agent", kUserAgent);
      esp_http_client_set_header(client, "Accept", kAccept);
      const esp_err_t openError = esp_http_client_open(client, 0);
      if (openError != ESP_OK) {
        result = ProviderResult::error(esp_err_to_name(openError));
      } else {
        const int64_t contentLength = esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (contentLength < 0 || status != 200) {
          result = ProviderResult::error(status > 0 ? getHttpResponsePhrase(status)
                                                    : esp_err_to_name(ESP_ERR_HTTP_FETCH_HEADER));
        } else {
          HttpClientStream stream(client);
          result = consume(stream);
          if (stream.hadReadError())
            result = ProviderResult::error(esp_err_to_name(ESP_FAIL));
        }
      }
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
    }
    LOG_INFO("NOAA %d %s %s", status, result.isOk() ? getHttpResponsePhrase(status) : result.detail().c_str(),
             url.c_str());
    if (!result.isOk() && attempt < 2)
      delay(100);
  }
  return result;
}

struct PointsHandler : public JsonHandler {
  String forecast;
  String hourly;
  String stations;
  String timezone;
  bool started = false;
  bool finished = false;
  void startDocument() override { started = true; }
  void endDocument() override { finished = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}
  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() < 2 || !value.isString() || !keyIs(keyAt(path, 0), "properties"))
      return;
    const char *field = keyAt(path, 1);
    if (keyIs(field, "forecast") && path.getCount() == 2)
      forecast = value.getString();
    else if (keyIs(field, "forecastHourly") && path.getCount() == 2)
      hourly = value.getString();
    else if (keyIs(field, "observationStations") && path.getCount() == 2)
      stations = value.getString();
    else if (keyIs(field, "timeZone"))
      timezone = value.getString();
  }
};

struct PeriodData {
  String start;
  String shortForecast;
  String textDescription;
  String icon;
  String windSpeed;
  String windDirection;
  float temperature = 0;
  float dewpoint = 0;
  float humidity = 0;
  float pop = 0;
  bool hasTemperature = false;
  bool hasDewpoint = false;
  bool hasHumidity = false;
  bool hasPop = false;
  bool hasStart = false;
  bool isDay = false;
};

class ForecastPeriodsHandler : public JsonHandler {
 public:
  explicit ForecastPeriodsHandler(std::vector<PeriodData> &periods) : periods_(periods) {}
  bool started = false;
  bool finished = false;
  void startDocument() override { started = true; }
  void endDocument() override { finished = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}
  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() < 4 || !keyIs(keyAt(path, 0), "properties") || !keyIs(keyAt(path, 1), "periods"))
      return;
    const int index = indexAt(path, 2);
    if (index < 0 || index >= static_cast<int>(periods_.size()))
      return;
    PeriodData &period = periods_[index];
    const char *field = keyAt(path, 3);
    if (keyIs(field, "startTime") && value.isString()) {
      period.start = value.getString();
      period.hasStart = NoaaForecastProvider::parseIso8601(period.start) >= 0;
    } else if (keyIs(field, "temperature") && (value.isInt() || value.isFloat())) {
      period.temperature = value.getDouble();
      period.hasTemperature = true;
    } else if (keyIs(field, "dewpoint") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
               (value.isInt() || value.isFloat())) {
      period.dewpoint = value.getDouble();
      period.hasDewpoint = true;
    } else if (keyIs(field, "relativeHumidity") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
               (value.isInt() || value.isFloat())) {
      period.humidity = value.getDouble();
      period.hasHumidity = true;
    } else if (keyIs(field, "probabilityOfPrecipitation") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
               (value.isInt() || value.isFloat())) {
      period.pop = value.getDouble();
      period.hasPop = true;
    } else if (keyIs(field, "windSpeed") && value.isString()) {
      period.windSpeed = value.getString();
    } else if (keyIs(field, "windSpeed") && (value.isInt() || value.isFloat())) {
      period.windSpeed = String(value.getDouble());
    } else if (keyIs(field, "windSpeed") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
               (value.isInt() || value.isFloat())) {
      period.windSpeed = String(value.getDouble());
    } else if (keyIs(field, "windDirection") && value.isString()) {
      period.windDirection = value.getString();
    } else if (keyIs(field, "windDirection") && (value.isInt() || value.isFloat())) {
      period.windDirection = String(value.getDouble());
    } else if (keyIs(field, "windDirection") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
               (value.isInt() || value.isFloat())) {
      period.windDirection = String(value.getDouble());
    } else if (keyIs(field, "isDaytime") && value.isBool()) {
      period.isDay = value.getBool();
    } else if (keyIs(field, "shortForecast") && value.isString()) {
      period.shortForecast = value.getString();
    } else if (keyIs(field, "textDescription") && value.isString()) {
      period.textDescription = value.getString();
    } else if (keyIs(field, "icon") && value.isString()) {
      period.icon = value.getString();
    }
  }

 private:
  std::vector<PeriodData> &periods_;
};

class StationsHandler : public JsonHandler {
 public:
  explicit StationsHandler(std::vector<NoaaStationCandidate> &stations) : stations_(stations) {}
  bool started = false;
  bool finished = false;
  void startDocument() override { started = true; }
  void endDocument() override { finished = true; }
  void startObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}
  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() < 4 || !keyIs(keyAt(path, 0), "features") || !keyIs(keyAt(path, 2), "properties"))
      return;
    const int index = indexAt(path, 1);
    if (index < 0)
      return;
    if (index >= static_cast<int>(candidateIds_.size())) {
      candidateIds_.resize(index + 1);
      distances_.resize(index + 1, 0.0f);
    }
    if (keyIs(keyAt(path, 3), "stationIdentifier") && value.isString())
      candidateIds_[index] = value.getString();
    if (keyIs(keyAt(path, 3), "distance") && (value.isInt() || value.isFloat()))
      distances_[index] = value.getDouble();
    if (keyIs(keyAt(path, 3), "distance") && path.getCount() >= 5 && keyIs(keyAt(path, 4), "value") &&
        (value.isInt() || value.isFloat()))
      distances_[index] = value.getDouble();
  }
  void endObject(ElementPath path) override {
    if (path.getCount() != 2 || !keyIs(keyAt(path, 0), "features"))
      return;
    const int index = indexAt(path, 1);
    if (index < 0 || index >= static_cast<int>(candidateIds_.size()) || candidateIds_[index].isEmpty())
      return;
    NoaaStationCandidate candidate;
    candidate.stationId = candidateIds_[index];
    candidate.distance = index < static_cast<int>(distances_.size()) ? distances_[index] : 0.0f;
    stations_.push_back(candidate);
  }

 private:
  std::vector<NoaaStationCandidate> &stations_;
  std::vector<String> candidateIds_;
  std::vector<float> distances_;
};

class ObservationHandler : public JsonHandler {
 public:
  explicit ObservationHandler(current_t &current) : current_(current) {}
  bool started = false;
  bool finished = false;
  bool hasTimestamp = false;
  bool hasTemperature = false;
  bool hasIcon = false;
  String timestamp;
  String textDescription;
  String icon;
  String units[12];
  float values[12] = {};
  bool hasValue[12] = {};

  void startDocument() override { started = true; }
  void endDocument() override { finished = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}
  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() < 2 || !keyIs(keyAt(path, 0), "properties"))
      return;
    const char *field = keyAt(path, 1);
    const int slot = slotFor(field);
    if (path.getCount() == 2 && value.isString()) {
      if (keyIs(field, "timestamp")) {
        timestamp = value.getString();
        hasTimestamp = NoaaForecastProvider::parseIso8601(timestamp) >= 0;
      } else if (keyIs(field, "textDescription"))
        textDescription = value.getString();
      else if (keyIs(field, "icon")) {
        icon = value.getString();
        hasIcon = true;
      }
      return;
    }
    if (slot >= 0 && path.getCount() >= 3 && keyIs(keyAt(path, 2), "value") && (value.isInt() || value.isFloat())) {
      values[slot] = value.getDouble();
      hasValue[slot] = true;
    } else if (slot >= 0 && path.getCount() >= 3 && keyIs(keyAt(path, 2), "unitCode") && value.isString()) {
      units[slot] = value.getString();
    }
    if (keyIs(field, "windDirection") && path.getCount() == 2 && (value.isInt() || value.isFloat())) {
      values[slotFor("windDirection")] = value.getDouble();
      hasValue[slotFor("windDirection")] = true;
    }
    if (keyIs(field, "cloudLayers") && value.isString() && path.getCount() >= 4 && keyIs(keyAt(path, 3), "amount")) {
      String coverage = value.getString();
      coverage.toUpperCase();
      int amount = coverage == "FEW"   ? 25
                   : coverage == "SCT" ? 50
                   : coverage == "BKN" ? 75
                   : coverage == "OVC" ? 100
                                       : 0;
      if (amount > current_.clouds)
        current_.clouds = amount;
    } else if (keyIs(field, "cloudLayers") && path.getCount() >= 5 && keyIs(keyAt(path, 3), "amount") &&
               keyIs(keyAt(path, 4), "value") && value.isString()) {
      String coverage = value.getString();
      coverage.toUpperCase();
      int amount = coverage == "FEW"   ? 25
                   : coverage == "SCT" ? 50
                   : coverage == "BKN" ? 75
                   : coverage == "OVC" ? 100
                                       : 0;
      if (amount > current_.clouds)
        current_.clouds = amount;
    }
  }

 private:
  static int slotFor(const char *field) {
    static const char *names[] = {"temperature", "heatIndex",        "windChill",          "relativeHumidity",
                                  "dewpoint",    "seaLevelPressure", "barometricPressure", "visibility",
                                  "windSpeed",   "windGust",         "windDirection",      "precipitationLastHour"};
    for (int i = 0; i < 12; ++i)
      if (keyIs(field, names[i]))
        return i;
    return -1;
  }
  current_t &current_;
};

String conditionText(const PeriodData &period) {
  if (!period.shortForecast.isEmpty())
    return period.shortForecast;
  if (!period.textDescription.isEmpty())
    return period.textDescription;
  return period.icon;
}

void copyPeriod(const PeriodData &period, hourly_t &hourly) {
  hourly.dt = NoaaForecastProvider::parseIso8601(period.start);
  hourly.temp = period.temperature;
  hourly.feels_like = period.temperature;
  hourly.dew_point = period.hasDewpoint ? period.dewpoint : 0.0f;
  hourly.humidity = period.hasHumidity ? static_cast<int>(period.humidity) : 0;
  hourly.pop = period.hasPop ? static_cast<int>(period.pop) : 0;
  hourly.wind_speed = speedMs(firstNumber(period.windSpeed));
  hourly.wind_deg = compassDegrees(period.windDirection);
  hourly.is_day = period.isDay;
  hourly.weather.condition = NoaaForecastProvider::mapDescription(conditionText(period));
}

struct DailyBucket {
  String date;
  int64_t dt = 0;
  bool used = false;
  bool hasDay = false;
  bool hasNight = false;
  float day = 0;
  float night = 0;
  int pop = 0;
  String condition;
  float wind = 0;
  int windDeg = 0;
};

String localDate(const String &timestamp) { return timestamp.length() >= 10 ? timestamp.substring(0, 10) : String(); }

int64_t localMidnight(const String &timestamp) {
  if (timestamp.length() < 19)
    return -1;
  String midnight = timestamp.substring(0, 19);
  midnight.setCharAt(11, '0');
  midnight.setCharAt(12, '0');
  midnight.setCharAt(14, '0');
  midnight.setCharAt(15, '0');
  midnight.setCharAt(17, '0');
  midnight.setCharAt(18, '0');
  int position = 19;
  while (position < static_cast<int>(timestamp.length()) && timestamp.charAt(position) != 'Z' &&
         timestamp.charAt(position) != 'z' && timestamp.charAt(position) != '+' && timestamp.charAt(position) != '-')
    ++position;
  if (position < static_cast<int>(timestamp.length()))
    midnight += timestamp.substring(position);
  else
    return -1;
  return NoaaForecastProvider::parseIso8601(midnight);
}

}  // namespace

const char *NoaaForecastProvider::getApiName() const { return "NOAA/NWS API"; }

std::vector<std::unique_ptr<FetchOperation>> NoaaForecastProvider::createFetchOperations(weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  auto points = std::make_unique<CallbackFetchOperation>("NOAA points", true, [this, &out]() {
    out.resetForecast();
    forecastUrl_.clear();
    hourlyUrl_.clear();
    observationStationsUrl_.clear();
    timeZone_.clear();
    stations_.clear();
    out.forecast.lat = strtod(LAT.c_str(), nullptr);
    out.forecast.lon = strtod(LON.c_str(), nullptr);
    return fetchPoints(out.forecast);
  });
  FetchOperation *pointsOperation = points.get();
  operations.push_back(std::move(points));

  auto daily = std::make_unique<CallbackFetchOperation>("NOAA daily forecast", true, [this, &out]() {
    ProviderResult result = fetchDaily(out.forecast);
    if (!result.isOk())
      for (daily_t &entry : out.forecast.daily)
        entry = {};
    return result;
  });
  daily->dependsOn(*pointsOperation);
  operations.push_back(std::move(daily));

  auto hourly = std::make_unique<CallbackFetchOperation>("NOAA hourly forecast", true, [this, &out]() {
    ProviderResult result = fetchHourly(out.forecast);
    if (!result.isOk())
      for (hourly_t &entry : out.forecast.hourly)
        entry = {};
    return result;
  });
  hourly->dependsOn(*pointsOperation);
  operations.push_back(std::move(hourly));

  auto current = std::make_unique<CallbackFetchOperation>("NOAA current observation", true, [this, &out]() {
    ProviderResult result = fetchCurrent(out.forecast.current);
    if (!result.isOk())
      out.forecast.current = {};
    return result;
  });
  current->dependsOn(*pointsOperation);
  operations.push_back(std::move(current));
  return operations;
}

bool NoaaForecastProvider::normalizeApiUrl(const String &url, String &normalized) {
  normalized = url;
  normalized.trim();
  if (!(normalized.startsWith("https://") || normalized.startsWith("http://")))
    return false;
  const int schemeEnd = normalized.indexOf("://");
  const int hostStart = schemeEnd + 3;
  int hostEnd = normalized.indexOf('/', hostStart);
  if (hostEnd < 0)
    hostEnd = normalized.length();
  String host = normalized.substring(hostStart, hostEnd);
  host.toLowerCase();
  if (!(host == kEndpoint || host == String(kEndpoint) + ":80" || host == String(kEndpoint) + ":443") ||
      normalized.indexOf('@', hostStart) >= 0 || normalized.indexOf('?', hostStart) == hostStart)
    return false;
  return hostEnd < static_cast<int>(normalized.length());
}

String NoaaForecastProvider::normalizeApiUrl(const String &url) {
  String result;
  return normalizeApiUrl(url, result) ? result : String();
}

int64_t NoaaForecastProvider::parseIso8601(const String &s) {
  if (s.length() < 20 || s.charAt(4) != '-' || s.charAt(7) != '-' || (s.charAt(10) != 'T' && s.charAt(10) != 't') ||
      s.charAt(13) != ':' || s.charAt(16) != ':' || !digits(s, 0, 4) || !digits(s, 5, 2) || !digits(s, 8, 2) ||
      !digits(s, 11, 2) || !digits(s, 14, 2) || !digits(s, 17, 2))
    return -1;
  const int year = atoi(s.substring(0, 4).c_str());
  const unsigned month = atoi(s.substring(5, 7).c_str());
  const unsigned day = atoi(s.substring(8, 10).c_str());
  const int hour = atoi(s.substring(11, 13).c_str());
  const int minute = atoi(s.substring(14, 16).c_str());
  const int second = atoi(s.substring(17, 19).c_str());
  if (!validDate(year, month, day) || hour > 23 || minute > 59 || second > 60)
    return -1;
  int pos = 19;
  if (s.charAt(pos) == '.') {
    ++pos;
    const int begin = pos;
    while (pos < static_cast<int>(s.length()) && s.charAt(pos) >= '0' && s.charAt(pos) <= '9')
      ++pos;
    if (pos == begin)
      return -1;
  }
  int offsetMinutes = 0;
  if (pos >= static_cast<int>(s.length()))
    return -1;
  const char zone = s.charAt(pos++);
  if (zone == 'Z' || zone == 'z') {
    if (pos != static_cast<int>(s.length()))
      return -1;
  } else if (zone == '+' || zone == '-') {
    if (pos + 5 != static_cast<int>(s.length()) || !digits(s, pos, 2) || s.charAt(pos + 2) != ':' ||
        !digits(s, pos + 3, 2))
      return -1;
    const int hours = atoi(s.substring(pos, pos + 2).c_str());
    const int minutes = atoi(s.substring(pos + 3, pos + 5).c_str());
    if (hours > 23 || minutes > 59)
      return -1;
    offsetMinutes = (hours * 60 + minutes) * (zone == '-' ? -1 : 1);
  } else
    return -1;
  return daysFromCivil(year, month, day) * 86400LL + hour * 3600 + minute * 60 + second - offsetMinutes * 60LL;
}

weather_condition NoaaForecastProvider::mapDescription(const String &description) {
  String text = description;
  text.toLowerCase();
  text.replace('_', ' ');
  text.replace('-', ' ');
  if (text.indexOf("tornado") >= 0)
    return weather_condition::TORNADO;
  if (text.indexOf("thunderstorm") >= 0)
    return text.indexOf("hail") >= 0 ? weather_condition::THUNDERSTORM_HAIL : weather_condition::THUNDERSTORM;
  if (text.indexOf("freezing rain") >= 0)
    return weather_condition::FREEZING_RAIN;
  if (text.indexOf("hail") >= 0)
    return weather_condition::THUNDERSTORM_HAIL;
  if (text.indexOf("sleet") >= 0 || text.indexOf("wintry") >= 0)
    return weather_condition::SLEET;
  if (text.indexOf("snow shower") >= 0)
    return weather_condition::SNOW_SHOWERS;
  if (text.indexOf("snow") >= 0)
    return weather_condition::SNOW;
  if (text.indexOf("drizzle") >= 0)
    return weather_condition::DRIZZLE;
  if (text.indexOf("rain shower") >= 0)
    return weather_condition::RAIN_SHOWERS;
  if (text.indexOf("rain") >= 0)
    return weather_condition::RAIN;
  if (text.indexOf("fog") >= 0)
    return weather_condition::FOG;
  if (text.indexOf("smoke") >= 0)
    return weather_condition::SMOKE;
  if (text.indexOf("haze") >= 0)
    return weather_condition::HAZE;
  if (text.indexOf("dust") >= 0)
    return weather_condition::DUST;
  if (text.indexOf("overcast") >= 0 || text.indexOf("ovc") >= 0)
    return weather_condition::OVERCAST;
  if (text.indexOf("mostly cloudy") >= 0)
    return weather_condition::CLOUDY;
  if (text.indexOf("partly cloudy") >= 0 || text.indexOf("few") >= 0 || text.indexOf("sct") >= 0)
    return weather_condition::PARTLY_CLOUDY;
  if (text.indexOf("cloudy") >= 0)
    return weather_condition::CLOUDY;
  if (text.indexOf("sunny") >= 0 || text.indexOf("clear") >= 0 || text.indexOf("skc") >= 0)
    return weather_condition::CLEAR;
  return weather_condition::UNKNOWN;
}

ProviderResult NoaaForecastProvider::deserializePoints(Stream &json, String &forecastUrl, String &hourlyUrl,
                                                       String &observationStationsUrl, String &timeZone) {
  forecastUrl = String();
  hourlyUrl = String();
  observationStationsUrl = String();
  timeZone = String();
  PointsHandler handler;
  ProviderResult result = parseStreamingJson(
      json, handler, [&handler] { return handler.finished; }, [&handler] { return handler.started; }, "points");
  if (!result.isOk()) {
    forecastUrl = String();
    hourlyUrl = String();
    observationStationsUrl = String();
    timeZone = String();
    return result;
  }
  if (!normalizeApiUrl(handler.forecast, forecastUrl) || !normalizeApiUrl(handler.hourly, hourlyUrl) ||
      !normalizeApiUrl(handler.stations, observationStationsUrl) || handler.timezone.isEmpty()) {
    forecastUrl = String();
    hourlyUrl = String();
    observationStationsUrl = String();
    timeZone = String();
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  timeZone = handler.timezone;
  return result;
}

ProviderResult NoaaForecastProvider::deserializePoints(Stream &json) {
  return deserializePoints(json, forecastUrl_, hourlyUrl_, observationStationsUrl_, timeZone_);
}

ProviderResult NoaaForecastProvider::deserializeHourly(Stream &json, forecast_t &forecast) {
  for (hourly_t &entry : forecast.hourly)
    entry = {};
  std::vector<PeriodData> periods(NUM_HOURLY + 1);
  ForecastPeriodsHandler handler(periods);
  ProviderResult result = parseStreamingJson(
      json, handler, [&handler] { return handler.finished; }, [&handler] { return handler.started; }, "hourly");
  int usable = 0;
  for (int i = 0; i < NUM_HOURLY; ++i)
    if (periods[i].hasStart && periods[i].hasTemperature)
      ++usable;
  if (!result.isOk() || usable != NUM_HOURLY) {
    for (hourly_t &entry : forecast.hourly)
      entry = {};
    return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
  }
  for (int i = 0; i < NUM_HOURLY; ++i)
    copyPeriod(periods[i], forecast.hourly[i]);
  return ProviderResult::ok();
}

ProviderResult NoaaForecastProvider::deserializeDaily(Stream &json, forecast_t &forecast) {
  for (daily_t &entry : forecast.daily)
    entry = {};
  std::vector<PeriodData> periods(NUM_DAILY * 2 + 8);
  ForecastPeriodsHandler handler(periods);
  ProviderResult result = parseStreamingJson(
      json, handler, [&handler] { return handler.finished; }, [&handler] { return handler.started; }, "daily");
  std::vector<DailyBucket> buckets(NUM_DAILY);
  for (const PeriodData &period : periods) {
    if (!period.hasStart)
      continue;
    const String date = localDate(period.start);
    int bucket = -1;
    for (int i = 0; i < NUM_DAILY; ++i)
      if (buckets[i].used && buckets[i].date == date)
        bucket = i;
    if (bucket < 0)
      for (int i = 0; i < NUM_DAILY; ++i)
        if (!buckets[i].used) {
          bucket = i;
          break;
        }
    if (bucket < 0)
      continue;
    DailyBucket &day = buckets[bucket];
    if (!day.used) {
      day.used = true;
      day.date = date;
      day.dt = localMidnight(period.start);
    }
    day.pop = std::max(day.pop, period.hasPop ? static_cast<int>(period.pop) : 0);
    if (day.condition.isEmpty())
      day.condition = conditionText(period);
    if (period.isDay) {
      if (period.hasTemperature) {
        day.hasDay = true;
        day.day = period.temperature;
      }
      day.wind = speedMs(firstNumber(period.windSpeed));
      day.windDeg = compassDegrees(period.windDirection);
    } else if (period.hasTemperature) {
      day.hasNight = true;
      day.night = period.temperature;
    }
  }
  int used = 0;
  for (int i = 0; i < NUM_DAILY; ++i)
    if (buckets[i].used && buckets[i].dt >= 0)
      ++used;
  if (!result.isOk() || used < NUM_DAILY) {
    for (daily_t &entry : forecast.daily)
      entry = {};
    return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
  }
  for (int i = 0; i < NUM_DAILY; ++i) {
    const DailyBucket &day = buckets[i];
    daily_t &out = forecast.daily[i];
    out.dt = day.dt;
    out.temp.day = day.hasDay ? day.day : 0.0f;
    out.temp.max = day.hasDay ? std::optional<float>(day.day) : std::nullopt;
    out.temp.night = day.hasNight ? day.night : 0.0f;
    out.temp.min = day.hasNight ? std::optional<float>(day.night) : std::nullopt;
    out.pop = day.pop;
    out.wind_speed = day.wind;
    out.wind_deg = day.windDeg;
    out.weather.condition = NoaaForecastProvider::mapDescription(day.condition);
  }
  return ProviderResult::ok();
}

ProviderResult NoaaForecastProvider::deserializeObservationStations(Stream &json,
                                                                    std::vector<NoaaStationCandidate> &stations) {
  stations.clear();
  StationsHandler handler(stations);
  ProviderResult result = parseStreamingJson(
      json, handler, [&handler] { return handler.finished; }, [&handler] { return handler.started; }, "stations");
  std::stable_sort(stations.begin(), stations.end(), [](const NoaaStationCandidate &a, const NoaaStationCandidate &b) {
    return a.distance < b.distance;
  });
  if (stations.size() > kMaxStationCandidates)
    stations.resize(kMaxStationCandidates);
  if (!result.isOk() || stations.empty()) {
    stations.clear();
    return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
  }
  return result;
}

ProviderResult NoaaForecastProvider::deserializeObservation(Stream &json, current_t &current) {
  current = {};
  ObservationHandler handler(current);
  ProviderResult result = parseStreamingJson(
      json, handler, [&handler] { return handler.finished; }, [&handler] { return handler.started; }, "observation");
  if (!result.isOk() || !handler.hasTimestamp || !handler.hasValue[0]) {
    current = {};
    return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
  }
  current.dt = parseIso8601(handler.timestamp);
  current.temp = handler.values[0];
  const int apparent = handler.hasValue[1] ? 1 : handler.hasValue[2] ? 2 : -1;
  current.feels_like = apparent >= 0 ? handler.values[apparent] : current.temp;
  if (handler.hasValue[3])
    current.humidity = static_cast<int>(handler.values[3]);
  current.dew_point = handler.hasValue[4] ? handler.values[4] : 0.0f;
  int pressureSlot = handler.hasValue[5] ? 5 : handler.hasValue[6] ? 6 : -1;
  if (pressureSlot >= 0) {
    const bool pascals = handler.units[pressureSlot].indexOf("Pa") >= 0 || handler.values[pressureSlot] > 2000.0f;
    current.pressure = static_cast<int>(handler.values[pressureSlot] / (pascals ? 100.0f : 1.0f));
  }
  if (handler.hasValue[7])
    current.visibility = static_cast<int>(handler.values[7]);
  if (handler.hasValue[8])
    current.wind_speed = observationSpeed(handler.values[8], handler.units[8]);
  if (handler.hasValue[9])
    current.wind_gust = observationSpeed(handler.values[9], handler.units[9]);
  if (handler.hasValue[10])
    current.wind_deg = static_cast<int>(handler.values[10]);
  if (handler.hasValue[11])
    current.rain_1h = handler.values[11];
  current.weather.condition =
      NoaaForecastProvider::mapDescription(!handler.textDescription.isEmpty() ? handler.textDescription : handler.icon);
  String icon = handler.icon;
  icon.toLowerCase();
  current.is_day =
      handler.hasIcon ? (icon.indexOf("night") < 0) : ((current.dt / 3600) % 24 >= 6 && (current.dt / 3600) % 24 < 18);
  return ProviderResult::ok();
}

ProviderResult NoaaForecastProvider::fetchPoints(forecast_t &forecast) {
  (void) forecast;
#if defined(NOAA_FORECAST_TRANSPORT_HTTP)
  const String scheme = "http";
#else
  const String scheme = "https";
#endif
  const String url = scheme + "://" + NOAA_ENDPOINT + "/points/" + LAT + "," + LON;
  ProviderResult result = requestNoaa(url, [this](Stream &json) { return deserializePoints(json); });
  if (result.isOk()) {
    forecast.timezone = timeZone_;
    forecast.timezone_offset = 0;
  }
  return result;
}

ProviderResult NoaaForecastProvider::fetchDaily(forecast_t &forecast) {
  return requestNoaa(forecastUrl_ + (forecastUrl_.indexOf('?') >= 0 ? "&units=si" : "?units=si"),
                     [&forecast](Stream &json) { return deserializeDaily(json, forecast); });
}

ProviderResult NoaaForecastProvider::fetchHourly(forecast_t &forecast) {
  return requestNoaa(hourlyUrl_ + (hourlyUrl_.indexOf('?') >= 0 ? "&units=si" : "?units=si"),
                     [&forecast](Stream &json) { return deserializeHourly(json, forecast); });
}

ProviderResult NoaaForecastProvider::fetchCurrent(current_t &current) {
  ProviderResult result = requestNoaa(observationStationsUrl_,
                                      [this](Stream &json) { return deserializeObservationStations(json, stations_); });
  if (!result.isOk())
    return result;
  const String scheme = baseScheme(observationStationsUrl_);
  int tried = 0;
  for (const NoaaStationCandidate &station : stations_) {
    if (tried++ >= kMaxStationCandidates)
      break;
    current_t candidate = {};
    const String url = scheme + "://" + NOAA_ENDPOINT + "/stations/" + station.stationId + "/observations/latest";
    result = requestNoaa(url, [&candidate](Stream &json) { return deserializeObservation(json, candidate); });
    if (result.isOk()) {
      current = candidate;
      return result;
    }
  }
  current = {};
  return result.isOk() ? ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) : result;
}

#endif  // REMOTE_PROVIDER_NOAA_FORECAST
