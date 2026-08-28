/* OpenWeatherMap One Call v4 provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "logger.h"

#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V4)

#include <Arduino.h>
#include <ArduinoStreamParser.h>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>

#include "cert.h"
#include "_locale.h"
#include "esp_http_client_stream.h"
#include "esp_http_client_utils.h"
#include "owm_v4_provider.h"
#include "provider_fetch_operations.h"

namespace {

enum class ResponseKind { CURRENT, HOURLY, DAILY };

static bool keyIs(ElementSelector *selector, const char *key) {
  return selector != nullptr && selector->isObject() && selector->getKey() != nullptr &&
         strcmp(selector->getKey(), key) == 0;
}

static bool keyIs(const char *value, const char *key) { return value != nullptr && strcmp(value, key) == 0; }

static int dataIndex(ElementPath path) {
  if (path.getCount() < 2 || !keyIs(path.get(0), "data") || path.get(1) == nullptr || path.get(1)->isObject()) {
    return -1;
  }
  return path.get(1)->getIndex();
}

static void resetCurrent(forecast_t &forecast) {
  forecast.lat = 0.0f;
  forecast.lon = 0.0f;
  forecast.timezone = String();
  forecast.timezone_offset = 0;
  forecast.current = {};
}

static void resetHourly(forecast_t &forecast, size_t first = 0) {
  if (first >= NUM_HOURLY) {
    return;
  }
  for (size_t i = first; i < NUM_HOURLY; ++i) {
    forecast.hourly[i] = {};
  }
}

static void resetDaily(forecast_t &forecast) {
  for (daily_t &entry : forecast.daily) {
    entry = {};
  }
}

/* The handler consumes all JSON, but only retains fields which belong to the
 * selected response. This is important for timeline responses: pagination
 * metadata and (when requested) alert IDs are handled without allocating a
 * DOM. */
class OneCallV4Handler : public JsonHandler {
 public:
  OneCallV4Handler(forecast_t &forecast, ResponseKind kind, size_t destinationOffset = 0,
                   std::vector<String> *alertIds = nullptr)
      : forecast_(forecast), kind_(kind), destinationOffset_(destinationOffset), alertIds_(alertIds) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath path) override {
    if (path.getCount() != 2 || !keyIs(path.get(0), "data") || path.get(1) == nullptr || path.get(1)->isObject()) {
      return;
    }
    const int index = path.get(1)->getIndex();
    if ((kind_ == ResponseKind::CURRENT && index == 0) ||
        (kind_ == ResponseKind::HOURLY && destinationOffset_ + static_cast<size_t>(index) < NUM_HOURLY) ||
        (kind_ == ResponseKind::DAILY && index < NUM_DAILY)) {
      ++recordCount_;
    }
  }
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    const int index = dataIndex(path);
    if (kind_ == ResponseKind::CURRENT && path.getCount() == 1) {
      storeMetadata(path.getCurrent(), value);
      return;
    }
    if (index < 0) {
      return;
    }

    if (kind_ == ResponseKind::CURRENT) {
      if (alertIds_ != nullptr)
        storeAlertId(path, value);
      if (index == 0)
        storeCurrent(path, value);
      return;
    }

    const size_t destination = destinationOffset_ + static_cast<size_t>(index);
    if (kind_ == ResponseKind::HOURLY) {
      storeHourly(path, destination, value);
    } else {
      storeDaily(path, destination, value);
    }
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }
  size_t recordCount() const { return recordCount_; }
  size_t timestampCount() const { return timestampCount_; }
  int64_t lastTimestamp() const { return lastTimestamp_; }

 private:
  static bool numeric(ElementValue value) { return value.isInt() || value.isFloat(); }

  void storeMetadata(ElementSelector *fieldSelector, ElementValue value) {
    if (fieldSelector == nullptr || (!keyIs(fieldSelector, "lat") && !keyIs(fieldSelector, "lon") &&
                                     !keyIs(fieldSelector, "timezone") && !keyIs(fieldSelector, "timezone_offset"))) {
      return;
    }
    const char *field = fieldSelector->getKey();
    if (numeric(value)) {
      if (keyIs(field, "lat"))
        forecast_.lat = static_cast<float>(value.getDouble());
      else if (keyIs(field, "lon"))
        forecast_.lon = static_cast<float>(value.getDouble());
      else if (keyIs(field, "timezone_offset"))
        forecast_.timezone_offset = static_cast<int>(value.getDouble());
    } else if (value.isString() && keyIs(field, "timezone")) {
      forecast_.timezone = value.getString();
    }
  }

  void storeAlertId(ElementPath path, ElementValue value) {
    if (alertIds_ == nullptr || path.getCount() != 4 || !keyIs(path.get(2), "alerts") || path.get(3) == nullptr ||
        path.get(3)->isObject() || !value.isString()) {
      return;
    }
    const String id = value.getString();
    if (id.length() == 0 || alertIds_->size() >= 2)
      return;
    for (const String &existing : *alertIds_) {
      if (existing == id)
        return;
    }
    alertIds_->push_back(id);
  }

  void storeCurrent(ElementPath path, ElementValue value) {
    const char *field = path.getCurrent() != nullptr ? path.getCurrent()->getKey() : nullptr;
    if (path.getCount() == 3) {
      if (keyIs(field, "dt") && numeric(value)) {
        forecast_.current.dt = static_cast<int64_t>(value.getDouble());
        ++timestampCount_;
        lastTimestamp_ = forecast_.current.dt;
      } else if (numeric(value)) {
        storeCurrentNumber(field, value.getDouble());
      }
      return;
    }
    if (path.getCount() == 4 && (keyIs(path.get(2), "rain") || keyIs(path.get(2), "snow"))) {
      if (numeric(value)) {
        if (keyIs(path.get(2), "rain") && keyIs(field, "1h"))
          forecast_.current.rain_1h = static_cast<float>(value.getDouble());
        else if (keyIs(path.get(2), "snow") && keyIs(field, "1h"))
          forecast_.current.snow_1h = static_cast<float>(value.getDouble());
      }
      return;
    }
    if (path.getCount() == 5 && keyIs(path.get(2), "weather") && path.get(3) != nullptr && !path.get(3)->isObject() &&
        path.get(3)->getIndex() == 0) {
      if (keyIs(field, "id") && numeric(value))
        forecast_.current.weather.condition = OpenWeatherMapOneCallV4Provider::mapWeatherCode(value.getDouble());
      else if (keyIs(field, "icon") && value.isString())
        forecast_.current.is_day = String(value.getString()).endsWith("d");
    }
  }

  void storeCurrentNumber(const char *field, double value) {
    if (keyIs(field, "temp"))
      forecast_.current.temp = static_cast<float>(value);
    else if (keyIs(field, "feels_like"))
      forecast_.current.feels_like = static_cast<float>(value);
    else if (keyIs(field, "pressure"))
      forecast_.current.pressure = static_cast<int>(value);
    else if (keyIs(field, "humidity"))
      forecast_.current.humidity = static_cast<int>(value);
    else if (keyIs(field, "dew_point"))
      forecast_.current.dew_point = static_cast<float>(value);
    else if (keyIs(field, "uvi"))
      forecast_.current.uvi = static_cast<float>(value);
    else if (keyIs(field, "clouds"))
      forecast_.current.clouds = static_cast<int>(value);
    else if (keyIs(field, "visibility"))
      forecast_.current.visibility = static_cast<int>(value);
    else if (keyIs(field, "wind_speed"))
      forecast_.current.wind_speed = static_cast<float>(value);
    else if (keyIs(field, "wind_deg"))
      forecast_.current.wind_deg = static_cast<int>(value);
    else if (keyIs(field, "wind_gust"))
      forecast_.current.wind_gust = static_cast<float>(value);
  }

  void storeHourly(ElementPath path, size_t index, ElementValue value) {
    if (index >= NUM_HOURLY) {
      return;
    }
    const char *field = path.getCurrent() != nullptr ? path.getCurrent()->getKey() : nullptr;
    hourly_t &hourly = forecast_.hourly[index];
    if (path.getCount() == 3) {
      if (keyIs(field, "dt") && numeric(value)) {
        hourly.dt = static_cast<int64_t>(value.getDouble());
        ++timestampCount_;
        lastTimestamp_ = hourly.dt;
      } else if (numeric(value)) {
        storeHourlyNumber(hourly, field, value.getDouble());
      } else if (keyIs(field, "icon") && value.isString()) {
        hourly.is_day = String(value.getString()).endsWith("d");
      }
    } else if (path.getCount() == 4 && (keyIs(path.get(2), "rain") || keyIs(path.get(2), "snow"))) {
      if (numeric(value) && keyIs(field, "1h")) {
        if (keyIs(path.get(2), "rain"))
          hourly.rain_1h = static_cast<float>(value.getDouble());
        else
          hourly.snow_1h = static_cast<float>(value.getDouble());
      }
    } else if (path.getCount() == 5 && keyIs(path.get(2), "weather") && path.get(3) != nullptr &&
               !path.get(3)->isObject() && path.get(3)->getIndex() == 0) {
      if (keyIs(field, "id") && numeric(value))
        hourly.weather.condition = OpenWeatherMapOneCallV4Provider::mapWeatherCode(value.getDouble());
      else if (keyIs(field, "icon") && value.isString())
        hourly.is_day = String(value.getString()).endsWith("d");
    }
  }

  static void storeHourlyNumber(hourly_t &hourly, const char *field, double value) {
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
    else if (keyIs(field, "uvi"))
      hourly.uvi = static_cast<float>(value);
    else if (keyIs(field, "clouds"))
      hourly.clouds = static_cast<int>(value);
    else if (keyIs(field, "visibility"))
      hourly.visibility = static_cast<int>(value);
    else if (keyIs(field, "wind_speed"))
      hourly.wind_speed = static_cast<float>(value);
    else if (keyIs(field, "wind_deg"))
      hourly.wind_deg = static_cast<int>(value);
    else if (keyIs(field, "wind_gust"))
      hourly.wind_gust = static_cast<float>(value);
    else if (keyIs(field, "pop"))
      hourly.pop = static_cast<int>(value * 100.0);
  }

  void storeDaily(ElementPath path, size_t index, ElementValue value) {
    if (index >= NUM_DAILY) {
      return;
    }
    const char *field = path.getCurrent() != nullptr ? path.getCurrent()->getKey() : nullptr;
    daily_t &daily = forecast_.daily[index];
    if (path.getCount() == 3) {
      if (keyIs(field, "dt") && numeric(value)) {
        daily.dt = static_cast<int64_t>(value.getDouble());
        ++timestampCount_;
        lastTimestamp_ = daily.dt;
      } else if (numeric(value)) {
        storeDailyNumber(daily, field, value.getDouble());
      }
    } else if (path.getCount() == 4 && keyIs(path.get(2), "temp")) {
      if (numeric(value)) {
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
      }
    } else if (path.getCount() == 5 && keyIs(path.get(2), "weather") && path.get(3) != nullptr &&
               !path.get(3)->isObject() && path.get(3)->getIndex() == 0) {
      if (keyIs(field, "id") && numeric(value))
        daily.weather.condition = OpenWeatherMapOneCallV4Provider::mapWeatherCode(value.getDouble());
    }
  }

  static void storeDailyNumber(daily_t &daily, const char *field, double value) {
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
    else if (keyIs(field, "wind_deg"))
      daily.wind_deg = static_cast<int>(value);
    else if (keyIs(field, "wind_gust"))
      daily.wind_gust = static_cast<float>(value);
    else if (keyIs(field, "pop"))
      daily.pop = static_cast<int>(value * 100.0);
    else if (keyIs(field, "rain"))
      daily.rain = static_cast<float>(value);
    else if (keyIs(field, "snow"))
      daily.snow = static_cast<float>(value);
  }

  forecast_t &forecast_;
  ResponseKind kind_;
  size_t destinationOffset_;
  bool sawStart_ = false;
  bool documentDone_ = false;
  size_t recordCount_ = 0;
  size_t timestampCount_ = 0;
  int64_t lastTimestamp_ = 0;
  std::vector<String> *alertIds_ = nullptr;
};

class OneCallV4AlertHandler : public JsonHandler {
 public:
  explicit OneCallV4AlertHandler(weather_alert_t &alert) : alert_(alert) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath path) override {
    if (path.getCount() == 0)
      rootObjectSeen_ = true;
  }
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() == 1) {
      const char *field = path.getCurrent() != nullptr ? path.getCurrent()->getKey() : nullptr;
      if (keyIs(field, "sender_name") && value.isString()) {
        alert_.sender_name = value.getString();
        sawField_ = true;
      } else if (keyIs(field, "event") && value.isString()) {
        alert_.event = value.getString();
        sawField_ = true;
      } else if (keyIs(field, "start") && numeric(value)) {
        alert_.start = static_cast<int64_t>(value.getDouble());
        sawStartTimestamp_ = true;
        sawField_ = true;
      } else if (keyIs(field, "end") && numeric(value)) {
        alert_.end = static_cast<int64_t>(value.getDouble());
        sawEndTimestamp_ = true;
        sawField_ = true;
      } else if (keyIs(field, "description") && value.isString()) {
        stringDescription_ = value.getString();
        sawField_ = true;
      }
      return;
    }

    if (path.getCount() == 2 && keyIs(path.get(0), "tags") && path.get(1) != nullptr && !path.get(1)->isObject() &&
        value.isString()) {
      if (path.get(1)->getIndex() == 0)
        alert_.tags = value.getString();
      if (fallbackTag_.length() == 0 && value.getString() != nullptr && value.getString()[0] != '\0')
        fallbackTag_ = value.getString();
      sawField_ = true;
      return;
    }

    if (path.getCount() == 3 && keyIs(path.get(0), "description") && path.get(1) != nullptr &&
        !path.get(1)->isObject() && path.get(2) != nullptr && path.get(2)->isObject() && value.isString()) {
      const int index = path.get(1)->getIndex();
      if (index < 0)
        return;
      if (localized_.size() <= static_cast<size_t>(index))
        localized_.resize(static_cast<size_t>(index) + 1);
      if (keyIs(path.get(2), "language"))
        localized_[index].language = value.getString();
      else if (keyIs(path.get(2), "description"))
        localized_[index].description = value.getString();
      sawField_ = true;
    }
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }
  bool rootObjectSeen() const { return rootObjectSeen_; }
  bool sawField() const { return sawField_; }
  bool sawStartTimestamp() const { return sawStartTimestamp_; }
  bool sawEndTimestamp() const { return sawEndTimestamp_; }
  const String &fallbackTag() const { return fallbackTag_; }

  void selectDescription() {
    for (const LocalizedDescription &entry : localized_) {
      if (nonEmpty(entry.description) && languageMatches(entry.language, OWM_LANG, true)) {
        alert_.description = entry.description;
        return;
      }
    }
    for (const LocalizedDescription &entry : localized_) {
      if (nonEmpty(entry.description) && languageMatches(entry.language, OWM_LANG, false)) {
        alert_.description = entry.description;
        return;
      }
    }
    for (const LocalizedDescription &entry : localized_) {
      if (nonEmpty(entry.description) && languageMatches(entry.language, "en", false)) {
        alert_.description = entry.description;
        return;
      }
    }
    for (const LocalizedDescription &entry : localized_) {
      if (nonEmpty(entry.description)) {
        alert_.description = entry.description;
        return;
      }
    }
    alert_.description = stringDescription_;
  }

 private:
  struct LocalizedDescription {
    String language;
    String description;
  };

  static bool numeric(ElementValue value) { return value.isInt() || value.isFloat(); }
  static bool nonEmpty(const String &value) { return value.length() != 0; }

  static bool languageMatches(const String &language, const String &target, bool exactOnly) {
    if (language.length() == 0 || target.length() == 0)
      return false;
    if (language.length() == target.length()) {
      for (size_t i = 0; i < target.length(); ++i) {
        const char languageChar = language[i] == '_' ? '-' : language[i];
        const char targetChar = target[i] == '_' ? '-' : target[i];
        if (static_cast<char>(tolower(static_cast<unsigned char>(languageChar))) !=
            static_cast<char>(tolower(static_cast<unsigned char>(targetChar))))
          return false;
      }
      return true;
    }
    if (exactOnly || language.length() < target.length())
      return false;
    for (size_t i = 0; i < target.length(); ++i) {
      const char languageChar = language[i] == '_' ? '-' : language[i];
      const char targetChar = target[i] == '_' ? '-' : target[i];
      if (static_cast<char>(tolower(static_cast<unsigned char>(languageChar))) !=
          static_cast<char>(tolower(static_cast<unsigned char>(targetChar))))
        return false;
    }
    return language[target.length()] == '-' || language[target.length()] == '_';
  }

  weather_alert_t &alert_;
  String stringDescription_;
  String fallbackTag_;
  std::vector<LocalizedDescription> localized_;
  bool sawStart_ = false;
  bool documentDone_ = false;
  bool rootObjectSeen_ = false;
  bool sawField_ = false;
  bool sawStartTimestamp_ = false;
  bool sawEndTimestamp_ = false;
};

template<typename Handler> static ProviderResult consumeJson(Stream &json, Handler &handler) {
  ArduinoStreamParser parser;
  parser.setHandler(&handler);
  uint8_t buffer[256];
  while (!parser.hasParseError() && !handler.finishedDocument()) {
    const size_t count = json.readBytes(buffer, sizeof(buffer));
    if (count == 0)
      break;
    // A bounded read may include bytes after the root document. Feed bytes
    // individually so the parser stops cleanly at endDocument() and trailing
    // data cannot become a second JSON document.
    for (size_t i = 0; i < count && !parser.hasParseError() && !handler.finishedDocument(); ++i) {
      if (!handler.sawStart() && (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n' || buffer[i] == '\r'))
        continue;
      parser.write(buffer + i, 1);
    }
  }
  if (parser.hasParseError()) {
    LOG_WARNING("OpenWeatherMap One Call v4 JSON parse error: %s", parser.getErrorMessage());
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (handler.finishedDocument()) {
    return ProviderResult::ok();
  }
  if (!handler.sawStart()) {
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  }
  return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
}

static ProviderResult parseCurrent(Stream &json, forecast_t &forecast, std::vector<String> *alertIds) {
  resetCurrent(forecast);
  if (alertIds != nullptr)
    alertIds->clear();
  OneCallV4Handler handler(forecast, ResponseKind::CURRENT, 0, alertIds);
  ProviderResult result = consumeJson(json, handler);
  if (!result.isOk() || handler.recordCount() == 0 || handler.timestampCount() == 0) {
    resetCurrent(forecast);
    if (result.isOk())
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  return result;
}

static ProviderResult parseAlert(Stream &json, weather_alert_t &alert) {
  alert = {};
  OneCallV4AlertHandler handler(alert);
  ProviderResult result = consumeJson(json, handler);
  if (!result.isOk() || !handler.rootObjectSeen() || !handler.sawField() || !handler.sawStartTimestamp() ||
      !handler.sawEndTimestamp()) {
    alert = {};
    if (result.isOk())
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    return result;
  }
  handler.selectDescription();
  if (alert.event.length() == 0 && handler.fallbackTag().length() != 0)
    alert.event = handler.fallbackTag();
  return ProviderResult::ok();
}

static ProviderResult parseHourly(Stream &json, forecast_t &forecast, size_t destinationOffset, size_t &records,
                                  int64_t &lastTimestamp) {
  if (destinationOffset >= NUM_HOURLY) {
    resetHourly(forecast);
    records = 0;
    lastTimestamp = 0;
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (destinationOffset == 0)
    resetHourly(forecast);
  else
    resetHourly(forecast, destinationOffset);
  OneCallV4Handler handler(forecast, ResponseKind::HOURLY, destinationOffset);
  ProviderResult result = consumeJson(json, handler);
  records = handler.recordCount();
  lastTimestamp = handler.lastTimestamp();
  if (!result.isOk() || handler.recordCount() == 0 || handler.timestampCount() < handler.recordCount()) {
    resetHourly(forecast);
    records = 0;
    lastTimestamp = 0;
    if (result.isOk())
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  return result;
}

static ProviderResult parseDaily(Stream &json, forecast_t &forecast) {
  resetDaily(forecast);
  OneCallV4Handler handler(forecast, ResponseKind::DAILY);
  ProviderResult result = consumeJson(json, handler);
  if (!result.isOk() || handler.recordCount() < NUM_DAILY || handler.timestampCount() < handler.recordCount()) {
    resetDaily(forecast);
    if (result.isOk())
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  return result;
}

static String endpointUri(const String &path, const String &query, bool sanitized) {
#if defined(OPENWEATHERMAP_ONECALL_V4_TRANSPORT_HTTP)
  const char *scheme = "http://";
#else
  const char *scheme = "https://";
#endif
  String uri = String(scheme) + OWM_ENDPOINT + path;
  uri += "?";
  uri += query;
  uri += query.length() != 0 ? "&appid=" : "appid=";
  uri += sanitized ? String("{API key}") : OPENWEATHERMAP_ONECALL_V4_API_KEY;
  return uri;
}

static String urlEncodePathSegment(const String &value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0f];
    }
  }
  return encoded;
}

static ProviderResult requestV4(const String &url, const String &sanitizedUrl,
                                std::function<ProviderResult(Stream &)> consume) {
  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
#if defined(OPENWEATHERMAP_ONECALL_V4_TRANSPORT_HTTPS_VERIFY)
  config.cert_pem = cert_USERTrust_RSA_Certification_Authority;
#endif
  return espHttpGetWithRetry(url, sanitizedUrl, config, [consume](esp_http_client_handle_t client) {
    EspHttpClientStream stream(client);
    ProviderResult result = consume(stream);
    if (stream.hadReadError())
      return espHttpErrorResult(stream.readError());
    return result;
  });
}

}  // namespace

const char *OpenWeatherMapOneCallV4Provider::getApiName() const { return "OpenWeatherMap One Call API 4.0"; }

std::vector<std::unique_ptr<FetchOperation>> OpenWeatherMapOneCallV4Provider::createFetchOperations(
    weather_report_t &out) {
  out.resetForecast();
  std::vector<std::unique_ptr<FetchOperation>> operations;
#if defined(OPENWEATHERMAP_ONECALL_V4_ALERTS_ENABLED) && OPENWEATHERMAP_ONECALL_V4_ALERTS_ENABLED
  out.resetAlerts();
  auto alertIds = std::make_shared<std::vector<String>>();
  auto current = std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out, alertIds]() {
    ProviderResult result = fetchCurrent(out.forecast, alertIds.get());
    if (!result.isOk()) {
      resetCurrent(out.forecast);
      out.resetAlerts();
    }
    return result;
  });
#else
  auto current = std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out]() {
    ProviderResult result = fetchCurrent(out.forecast);
    if (!result.isOk())
      resetCurrent(out.forecast);
    return result;
  });
#endif
  FetchOperation *currentOperation = current.get();
  operations.push_back(std::move(current));

  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out]() {
    ProviderResult result = fetchHourly(out.forecast);
    if (!result.isOk())
      resetHourly(out.forecast);
    return result;
  }));
  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out]() {
    ProviderResult result = fetchDaily(out.forecast);
    if (!result.isOk())
      resetDaily(out.forecast);
    return result;
  }));
#if defined(OPENWEATHERMAP_ONECALL_V4_ALERTS_ENABLED) && OPENWEATHERMAP_ONECALL_V4_ALERTS_ENABLED
  auto alertOperation = std::make_unique<CallbackFetchOperation>(getApiName(), false, [this, &out, alertIds]() {
    std::vector<weather_alert_t> parsedAlerts;
    parsedAlerts.reserve(alertIds->size());
    for (const String &alertId : *alertIds) {
      weather_alert_t alert = {};
      ProviderResult result = fetchAlert(alertId, alert);
      if (!result.isOk()) {
        out.resetAlerts();
        return result;
      }
      parsedAlerts.push_back(std::move(alert));
    }
    out.engageAlerts() = std::move(parsedAlerts);
    return ProviderResult::ok();
  });
  alertOperation->dependsOn(*currentOperation);
  operations.push_back(std::move(alertOperation));
#endif
  return operations;
}

ProviderResult OpenWeatherMapOneCallV4Provider::fetchCurrent(forecast_t &forecast, std::vector<String> *alertIds) {
  const String query = "lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG + "&units=metric";
  const String uri = endpointUri("/data/4.0/onecall/current", query, false);
  const String sanitizedUri = endpointUri("/data/4.0/onecall/current", query, true);
  return requestV4(uri, sanitizedUri,
                   [&forecast, alertIds](Stream &json) { return deserializeCurrent(json, forecast, alertIds); });
}

ProviderResult OpenWeatherMapOneCallV4Provider::fetchAlert(const String &alertId, weather_alert_t &alert) {
  const String path = String("/data/4.0/onecall/alert/") + urlEncodePathSegment(alertId);
  const String uri = endpointUri(path, String(), false);
  const String sanitizedUri = endpointUri(path, String(), true);
  return requestV4(uri, sanitizedUri, [&alert](Stream &json) { return deserializeAlert(json, alert); });
}

ProviderResult OpenWeatherMapOneCallV4Provider::fetchHourly(forecast_t &forecast) {
  const String baseQuery = "lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG + "&units=metric";
  size_t firstRecords = 0;
  int64_t lastTimestamp = 0;
  String query = baseQuery + "&cnt=20";
  ProviderResult result = requestV4(endpointUri("/data/4.0/onecall/timeline/1h", query, false),
                                    endpointUri("/data/4.0/onecall/timeline/1h", query, true),
                                    [&forecast, &firstRecords, &lastTimestamp](Stream &json) {
                                      return parseHourly(json, forecast, 0, firstRecords, lastTimestamp);
                                    });
  if (!result.isOk()) {
    resetHourly(forecast);
    return result;
  }

  const size_t storedFirst = firstRecords > NUM_HOURLY ? NUM_HOURLY : firstRecords;
  if (storedFirst >= NUM_HOURLY)
    return ProviderResult::ok();
  if (lastTimestamp == 0) {
    resetHourly(forecast);
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }

  const size_t remaining = NUM_HOURLY - storedFirst;
  char start[24];
  snprintf(start, sizeof(start), "%lld", static_cast<long long>(lastTimestamp + 3600));
  query = baseQuery + "&start=" + start + "&cnt=" + String(static_cast<unsigned>(remaining));
  size_t continuationRecords = 0;
  result = requestV4(endpointUri("/data/4.0/onecall/timeline/1h", query, false),
                     endpointUri("/data/4.0/onecall/timeline/1h", query, true),
                     [&forecast, storedFirst, &continuationRecords, &lastTimestamp](Stream &json) {
                       return parseHourly(json, forecast, storedFirst, continuationRecords, lastTimestamp);
                     });
  if (!result.isOk()) {
    resetHourly(forecast);
    return result;
  }
  if (continuationRecords < remaining) {
    resetHourly(forecast);
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  for (size_t i = 0; i < NUM_HOURLY; ++i) {
    if (forecast.hourly[i].dt == 0) {
      resetHourly(forecast);
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    }
  }
  return ProviderResult::ok();
}

ProviderResult OpenWeatherMapOneCallV4Provider::fetchDaily(forecast_t &forecast) {
  const String query = "lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG + "&units=metric&cnt=" + String(NUM_DAILY);
  const String uri = endpointUri("/data/4.0/onecall/timeline/1day", query, false);
  const String sanitizedUri = endpointUri("/data/4.0/onecall/timeline/1day", query, true);
  ProviderResult result =
      requestV4(uri, sanitizedUri, [&forecast](Stream &json) { return deserializeDaily(json, forecast); });
  if (!result.isOk()) {
    resetDaily(forecast);
    return result;
  }
  for (size_t i = 0; i < NUM_DAILY; ++i) {
    if (forecast.daily[i].dt == 0) {
      resetDaily(forecast);
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    }
  }
  return result;
}

weather_condition OpenWeatherMapOneCallV4Provider::mapWeatherCode(int id) {
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

ProviderResult OpenWeatherMapOneCallV4Provider::deserializeCurrent(Stream &json, forecast_t &forecast,
                                                                   std::vector<String> *alertIds) {
  return parseCurrent(json, forecast, alertIds);
}

ProviderResult OpenWeatherMapOneCallV4Provider::deserializeAlert(Stream &json, weather_alert_t &alert) {
  return parseAlert(json, alert);
}

ProviderResult OpenWeatherMapOneCallV4Provider::deserializeHourly(Stream &json, forecast_t &forecast,
                                                                  size_t destinationOffset) {
  size_t records = 0;
  int64_t lastTimestamp = 0;
  return parseHourly(json, forecast, destinationOffset, records, lastTimestamp);
}

ProviderResult OpenWeatherMapOneCallV4Provider::deserializeDaily(Stream &json, forecast_t &forecast) {
  return parseDaily(json, forecast);
}

#endif  // REMOTE_PROVIDER_OPENWEATHERMAP_ONECALL_V4
