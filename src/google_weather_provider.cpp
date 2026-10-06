/* Google Weather API forecast provider for esp32-weather-epd.
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

#if defined(REMOTE_PROVIDER_GOOGLE_WEATHER_FORECAST)

#include <Arduino.h>
#include <cstdint>
#include <cstring>

#include "_locale.h"
#include "cert.h"
#include "esp_http_client_utils.h"
#include "google_weather_provider.h"
#include "iso8601.h"
#include "json_stream_utils.h"
#include "provider_fetch_operations.h"

namespace {

constexpr int GOOGLE_WEATHER_HTTP_TIMEOUT_MS = 5000;

enum class ResponseKind { CURRENT, HOURLY, DAILY };

static bool keyIs(const char *value, const char *key) { return value != nullptr && strcmp(value, key) == 0; }

static bool keyIs(ElementSelector *selector, const char *key) {
  return selector != nullptr && selector->isObject() && keyIs(selector->getKey(), key);
}

static const char *keyAt(ElementPath path, int index) {
  ElementSelector *selector = path.get(index);
  return selector != nullptr && selector->isObject() ? selector->getKey() : nullptr;
}

static int indexAt(ElementPath path, int index) {
  ElementSelector *selector = path.get(index);
  return selector != nullptr && !selector->isObject() ? selector->getIndex() : -1;
}

static bool numeric(ElementValue value) { return value.isInt() || value.isFloat(); }

static float metricWindSpeed(double kilometersPerHour) { return static_cast<float>(kilometersPerHour / 3.6); }

static float metricVisibility(double kilometers) { return static_cast<float>(kilometers * 1000.0); }

static bool isSnowPrecipitation(const String &type) {
  return type.indexOf("SNOW") >= 0 || type.indexOf("SLEET") >= 0 || type.indexOf("ICE") >= 0;
}

static void applyPrecipitation(const String &type, float amount, float &rain, float &snow) {
  if (type.indexOf("RAIN_AND_SNOW") >= 0) {
    rain = amount / 2.0f;
    snow = amount / 2.0f;
  } else if (isSnowPrecipitation(type)) {
    snow = amount;
  } else {
    rain = amount;
  }
}

static void resetCurrent(forecast_t &forecast) {
  forecast.current = {};
  forecast.timezone = String();
  forecast.timezone_offset = 0;
}

static void resetHourly(forecast_t &forecast) {
  for (hourly_t &entry : forecast.hourly)
    entry = {};
}

static void resetDaily(forecast_t &forecast) {
  for (daily_t &entry : forecast.daily)
    entry = {};
}

/* The API returns all quantities in nested records. Parse directly from the
 * HTTP stream so hourly and daily payloads do not require large JSON DOMs. */
class GoogleWeatherHandler : public JsonHandler {
 public:
  GoogleWeatherHandler(forecast_t &forecast, ResponseKind kind) : forecast_(forecast), kind_(kind) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath path) override {
    if (kind_ == ResponseKind::HOURLY && path.getCount() == 2 && keyIs(path.get(0), "forecastHours") &&
        indexAt(path, 1) >= 0) {
      ++recordCount_;
    } else if (kind_ == ResponseKind::DAILY && path.getCount() == 2 && keyIs(path.get(0), "forecastDays") &&
               indexAt(path, 1) >= 0) {
      ++recordCount_;
    }
  }
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    switch (kind_) {
      case ResponseKind::CURRENT:
        parseCurrent(path, value);
        break;
      case ResponseKind::HOURLY:
        parseHourly(path, value);
        break;
      case ResponseKind::DAILY:
        parseDaily(path, value);
        break;
    }
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }
  bool hasCurrentTime() const { return hasCurrentTime_; }
  size_t recordCount() const { return recordCount_; }
  size_t timestampCount() const { return timestampCount_; }

  void finish() {
    if (kind_ == ResponseKind::CURRENT) {
      applyPrecipitation(currentPrecipitationType_, currentPrecipitationAmount_, forecast_.current.rain_1h,
                         forecast_.current.snow_1h);
    } else if (kind_ == ResponseKind::HOURLY) {
      for (size_t i = 0; i < NUM_HOURLY; ++i) {
        if (hasHourlyPrecipitation_[i]) {
          applyPrecipitation(hourlyPrecipitationType_[i], hourlyPrecipitationAmount_[i], forecast_.hourly[i].rain_1h,
                             forecast_.hourly[i].snow_1h);
        }
      }
    } else {
      for (size_t i = 0; i < NUM_DAILY; ++i) {
        for (size_t part = 0; part < 2; ++part) {
          if (hasDailyPrecipitation_[i][part]) {
            float rain = 0.0f;
            float snow = 0.0f;
            applyPrecipitation(dailyPrecipitationType_[i][part], dailyPrecipitationAmount_[i][part], rain, snow);
            forecast_.daily[i].rain += rain;
            forecast_.daily[i].snow += snow;
          }
        }
      }
    }
  }

 private:
  static bool parseTimestamp(ElementValue value, int64_t &destination) {
    int64_t timestamp = 0;
    if (!value.isString() || !iso8601::parse(value.getString(), timestamp))
      return false;
    destination = timestamp;
    return true;
  }

  void parseCurrent(ElementPath path, ElementValue value) {
    if (path.getCount() == 1) {
      ElementSelector *fieldSelector = path.getCurrent();
      if (keyIs(fieldSelector, "currentTime")) {
        hasCurrentTime_ = parseTimestamp(value, forecast_.current.dt);
        if (hasCurrentTime_)
          ++timestampCount_;
      } else if (keyIs(fieldSelector, "isDaytime") && value.isBool()) {
        forecast_.current.is_day = value.getBool();
      } else if (numeric(value)) {
        const double number = value.getDouble();
        if (keyIs(fieldSelector, "relativeHumidity"))
          forecast_.current.humidity = static_cast<int>(number);
        else if (keyIs(fieldSelector, "uvIndex"))
          forecast_.current.uvi = static_cast<float>(number);
        else if (keyIs(fieldSelector, "cloudCover"))
          forecast_.current.clouds = static_cast<int>(number);
      }
      return;
    }
    if (path.getCount() == 2 && keyIs(path.get(0), "timeZone") && keyIs(path.get(1), "id") && value.isString()) {
      forecast_.timezone = value.getString();
      return;
    }
    if (path.getCount() == 2 && keyIs(path.get(0), "weatherCondition") && keyIs(path.get(1), "type") &&
        value.isString()) {
      forecast_.current.weather.condition = GoogleWeatherForecastProvider::mapWeatherCondition(value.getString());
      return;
    }
    if (path.getCount() == 2 && numeric(value)) {
      const char *group = keyAt(path, 0);
      const double number = value.getDouble();
      if (keyIs(group, "temperature") && keyIs(path.get(1), "degrees"))
        forecast_.current.temp = static_cast<float>(number);
      else if (keyIs(group, "feelsLikeTemperature") && keyIs(path.get(1), "degrees"))
        forecast_.current.feels_like = static_cast<float>(number);
      else if (keyIs(group, "dewPoint") && keyIs(path.get(1), "degrees"))
        forecast_.current.dew_point = static_cast<float>(number);
      else if (keyIs(group, "airPressure") && keyIs(path.get(1), "meanSeaLevelMillibars"))
        forecast_.current.pressure = static_cast<int>(number);
      else if (keyIs(group, "visibility") && keyIs(path.get(1), "distance"))
        forecast_.current.visibility = static_cast<int>(metricVisibility(number));
      return;
    }
    if (path.getCount() == 3 && keyIs(path.get(0), "wind")) {
      if (!numeric(value))
        return;
      const char *group = keyAt(path, 1);
      const char *field = keyAt(path, 2);
      const double number = value.getDouble();
      if (keyIs(group, "direction") && keyIs(field, "degrees"))
        forecast_.current.wind_deg = static_cast<int>(number);
      else if (keyIs(group, "speed") && keyIs(field, "value"))
        forecast_.current.wind_speed = metricWindSpeed(number);
      else if (keyIs(group, "gust") && keyIs(field, "value"))
        forecast_.current.wind_gust = metricWindSpeed(number);
      return;
    }
    if (path.getCount() == 3 && keyIs(path.get(0), "precipitation")) {
      const char *group = keyAt(path, 1);
      const char *field = keyAt(path, 2);
      if (keyIs(group, "probability") && keyIs(field, "type") && value.isString())
        currentPrecipitationType_ = value.getString();
      else if (keyIs(group, "qpf") && keyIs(field, "quantity") && numeric(value))
        currentPrecipitationAmount_ = static_cast<float>(value.getDouble());
    }
  }

  void parseHourly(ElementPath path, ElementValue value) {
    if (path.getCount() < 3 || !keyIs(path.get(0), "forecastHours"))
      return;
    const int rawIndex = indexAt(path, 1);
    if (rawIndex < 0 || rawIndex >= NUM_HOURLY)
      return;
    const size_t index = static_cast<size_t>(rawIndex);
    hourly_t &hourly = forecast_.hourly[index];

    if (path.getCount() == 4 && keyIs(path.get(2), "interval") && keyIs(path.get(3), "startTime")) {
      if (parseTimestamp(value, hourly.dt))
        ++timestampCount_;
      return;
    }
    if (path.getCount() == 3) {
      if (!numeric(value)) {
        if (keyIs(path.get(2), "isDaytime") && value.isBool())
          hourly.is_day = value.getBool();
        return;
      }
      const char *field = keyAt(path, 2);
      const double number = value.getDouble();
      if (keyIs(field, "relativeHumidity"))
        hourly.humidity = static_cast<int>(number);
      else if (keyIs(field, "uvIndex"))
        hourly.uvi = static_cast<float>(number);
      else if (keyIs(field, "cloudCover"))
        hourly.clouds = static_cast<int>(number);
      return;
    }
    if (path.getCount() == 4) {
      const char *group = keyAt(path, 2);
      const char *field = keyAt(path, 3);
      if (keyIs(group, "weatherCondition") && keyIs(field, "type") && value.isString()) {
        hourly.weather.condition = GoogleWeatherForecastProvider::mapWeatherCondition(value.getString());
      } else if (numeric(value)) {
        const double number = value.getDouble();
        if (keyIs(group, "temperature") && keyIs(field, "degrees"))
          hourly.temp = static_cast<float>(number);
        else if (keyIs(group, "feelsLikeTemperature") && keyIs(field, "degrees"))
          hourly.feels_like = static_cast<float>(number);
        else if (keyIs(group, "dewPoint") && keyIs(field, "degrees"))
          hourly.dew_point = static_cast<float>(number);
        else if (keyIs(group, "airPressure") && keyIs(field, "meanSeaLevelMillibars"))
          hourly.pressure = static_cast<int>(number);
        else if (keyIs(group, "visibility") && keyIs(field, "distance"))
          hourly.visibility = static_cast<int>(metricVisibility(number));
      }
      return;
    }
    if (path.getCount() == 5) {
      const char *group = keyAt(path, 2);
      const char *subgroup = keyAt(path, 3);
      const char *field = keyAt(path, 4);
      if (keyIs(group, "wind") && numeric(value)) {
        const double number = value.getDouble();
        if (keyIs(subgroup, "direction") && keyIs(field, "degrees"))
          hourly.wind_deg = static_cast<int>(number);
        else if (keyIs(subgroup, "speed") && keyIs(field, "value"))
          hourly.wind_speed = metricWindSpeed(number);
        else if (keyIs(subgroup, "gust") && keyIs(field, "value"))
          hourly.wind_gust = metricWindSpeed(number);
      } else if (keyIs(group, "precipitation")) {
        if (keyIs(subgroup, "probability") && keyIs(field, "percent") && numeric(value))
          hourly.pop = static_cast<int>(value.getDouble());
        else if (keyIs(subgroup, "probability") && keyIs(field, "type") && value.isString())
          hourlyPrecipitationType_[index] = value.getString();
        else if (keyIs(subgroup, "qpf") && keyIs(field, "quantity") && numeric(value)) {
          hourlyPrecipitationAmount_[index] = static_cast<float>(value.getDouble());
          hasHourlyPrecipitation_[index] = true;
        }
      }
    }
  }

  void parseDaily(ElementPath path, ElementValue value) {
    if (path.getCount() < 3 || !keyIs(path.get(0), "forecastDays"))
      return;
    const int rawIndex = indexAt(path, 1);
    if (rawIndex < 0 || rawIndex >= NUM_DAILY)
      return;
    const size_t index = static_cast<size_t>(rawIndex);
    daily_t &daily = forecast_.daily[index];

    if (path.getCount() == 4 && keyIs(path.get(2), "interval") && keyIs(path.get(3), "startTime")) {
      if (parseTimestamp(value, daily.dt))
        ++timestampCount_;
      return;
    }
    if (path.getCount() == 4) {
      const char *group = keyAt(path, 2);
      const char *field = keyAt(path, 3);
      if (numeric(value)) {
        const double number = value.getDouble();
        if (keyIs(group, "maxTemperature") && keyIs(field, "degrees"))
          daily.temp.max = static_cast<float>(number);
        else if (keyIs(group, "minTemperature") && keyIs(field, "degrees"))
          daily.temp.min = static_cast<float>(number);
        else if (keyIs(group, "daytimeForecast"))
          storeDailyPart(index, true, field, value);
        else if (keyIs(group, "nighttimeForecast"))
          storeDailyPart(index, false, field, value);
      } else if (keyIs(group, "daytimeForecast")) {
        storeDailyPart(index, true, field, value);
      } else if (keyIs(group, "nighttimeForecast")) {
        storeDailyPart(index, false, field, value);
      }
      return;
    }
    if (path.getCount() == 5) {
      const char *part = keyAt(path, 2);
      const char *group = keyAt(path, 3);
      const char *field = keyAt(path, 4);
      if (!keyIs(part, "daytimeForecast") && !keyIs(part, "nighttimeForecast"))
        return;
      if (keyIs(group, "weatherCondition") && keyIs(field, "type") && value.isString()) {
        storeDailyCondition(index, keyIs(part, "daytimeForecast"),
                            GoogleWeatherForecastProvider::mapWeatherCondition(value.getString()));
      }
      return;
    }
    if (path.getCount() == 6) {
      const char *part = keyAt(path, 2);
      if (!keyIs(part, "daytimeForecast") && !keyIs(part, "nighttimeForecast"))
        return;
      const bool daytime = keyIs(part, "daytimeForecast");
      const char *group = keyAt(path, 3);
      const char *subgroup = keyAt(path, 4);
      const char *field = keyAt(path, 5);
      if (keyIs(group, "wind") && numeric(value)) {
        const double number = value.getDouble();
        if (keyIs(subgroup, "direction") && keyIs(field, "degrees")) {
          if (daytime || !hasDailyWindDirection_[index]) {
            daily.wind_deg = static_cast<int>(number);
            hasDailyWindDirection_[index] = true;
          }
        } else if (keyIs(subgroup, "speed") && keyIs(field, "value")) {
          const float speed = metricWindSpeed(number);
          if (daytime || !hasDailyWindSpeed_[index] || speed > daily.wind_speed)
            daily.wind_speed = speed;
          hasDailyWindSpeed_[index] = true;
        } else if (keyIs(subgroup, "gust") && keyIs(field, "value")) {
          const float gust = metricWindSpeed(number);
          if (daytime || !hasDailyWindGust_[index] || gust > daily.wind_gust)
            daily.wind_gust = gust;
          hasDailyWindGust_[index] = true;
        }
      } else if (keyIs(group, "precipitation")) {
        if (keyIs(subgroup, "qpf") && keyIs(field, "quantity") && numeric(value)) {
          const size_t partIndex = daytime ? 0 : 1;
          dailyPrecipitationAmount_[index][partIndex] = static_cast<float>(value.getDouble());
          hasDailyPrecipitation_[index][partIndex] = true;
        } else if (keyIs(subgroup, "probability") && keyIs(field, "percent") && numeric(value)) {
          daily.pop = max(daily.pop, static_cast<int>(value.getDouble()));
        } else if (keyIs(subgroup, "probability") && keyIs(field, "type") && value.isString()) {
          const size_t partIndex = daytime ? 0 : 1;
          dailyPrecipitationType_[index][partIndex] = value.getString();
        }
      }
    }
  }

  void storeDailyPart(size_t index, bool daytime, const char *field, ElementValue value) {
    daily_t &daily = forecast_.daily[index];
    if (keyIs(field, "relativeHumidity") && numeric(value)) {
      if (daytime || !hasDailyHumidity_[index]) {
        daily.humidity = static_cast<int>(value.getDouble());
        hasDailyHumidity_[index] = true;
      }
    } else if (keyIs(field, "uvIndex") && numeric(value)) {
      daily.uvi = max(daily.uvi, static_cast<float>(value.getDouble()));
    } else if (keyIs(field, "cloudCover") && numeric(value)) {
      if (daytime || !hasDailyCloudCover_[index]) {
        daily.clouds = static_cast<int>(value.getDouble());
        hasDailyCloudCover_[index] = true;
      }
    } else if (keyIs(field, "weatherCondition") && value.isString()) {
      storeDailyCondition(index, daytime, GoogleWeatherForecastProvider::mapWeatherCondition(value.getString()));
    }
  }

  void storeDailyCondition(size_t index, bool daytime, weather_condition condition) {
    if (daytime || !hasDailyCondition_[index]) {
      forecast_.daily[index].weather.condition = condition;
      hasDailyCondition_[index] = true;
    }
  }

  forecast_t &forecast_;
  ResponseKind kind_;
  bool sawStart_ = false;
  bool documentDone_ = false;
  bool hasCurrentTime_ = false;
  size_t recordCount_ = 0;
  size_t timestampCount_ = 0;
  String currentPrecipitationType_;
  float currentPrecipitationAmount_ = 0.0f;
  String hourlyPrecipitationType_[NUM_HOURLY];
  float hourlyPrecipitationAmount_[NUM_HOURLY] = {};
  bool hasHourlyPrecipitation_[NUM_HOURLY] = {};
  String dailyPrecipitationType_[NUM_DAILY][2];
  float dailyPrecipitationAmount_[NUM_DAILY][2] = {};
  bool hasDailyPrecipitation_[NUM_DAILY][2] = {};
  bool hasDailyHumidity_[NUM_DAILY] = {};
  bool hasDailyCloudCover_[NUM_DAILY] = {};
  bool hasDailyCondition_[NUM_DAILY] = {};
  bool hasDailyWindDirection_[NUM_DAILY] = {};
  bool hasDailyWindSpeed_[NUM_DAILY] = {};
  bool hasDailyWindGust_[NUM_DAILY] = {};
};

struct GoogleWeatherDocumentComplete {
  const GoogleWeatherHandler *handler;
  bool operator()() const { return handler->finishedDocument(); }
};

struct GoogleWeatherDocumentStarted {
  const GoogleWeatherHandler *handler;
  bool operator()() const { return handler->sawStart(); }
};

using GoogleWeatherFeeder = JsonStreamFeeder<GoogleWeatherDocumentComplete, GoogleWeatherDocumentStarted>;

template<typename Handler> static ProviderResult consume(Stream &json, Handler &handler, const char *label) {
  return consumeJsonStream(
      json, handler, [&handler]() { return handler.finishedDocument(); }, [&handler]() { return handler.sawStart(); },
      label, true);
}

static void resetResponse(ResponseKind kind, forecast_t &forecast) {
  switch (kind) {
    case ResponseKind::CURRENT:
      resetCurrent(forecast);
      break;
    case ResponseKind::HOURLY:
      resetHourly(forecast);
      break;
    case ResponseKind::DAILY:
      resetDaily(forecast);
      break;
  }
}

static ProviderResult validateResponse(ResponseKind kind, GoogleWeatherHandler &handler, forecast_t &forecast) {
  bool valid = false;
  switch (kind) {
    case ResponseKind::CURRENT:
      valid = handler.hasCurrentTime();
      break;
    case ResponseKind::HOURLY:
      valid = handler.recordCount() >= NUM_HOURLY && handler.timestampCount() >= NUM_HOURLY;
      if (valid) {
        for (const hourly_t &entry : forecast.hourly) {
          if (entry.dt == 0) {
            valid = false;
            break;
          }
        }
      }
      break;
    case ResponseKind::DAILY:
      valid = handler.recordCount() >= NUM_DAILY && handler.timestampCount() >= NUM_DAILY;
      if (valid) {
        for (const daily_t &entry : forecast.daily) {
          if (entry.dt == 0) {
            valid = false;
            break;
          }
        }
      }
      break;
  }

  if (!valid) {
    resetResponse(kind, forecast);
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  handler.finish();
  return ProviderResult::ok();
}

class GoogleWeatherChunkParser {
 public:
  GoogleWeatherChunkParser(forecast_t &forecast, ResponseKind kind) : forecast_(forecast), kind_(kind) {
    beginAttempt();
  }

  void beginAttempt() {
    resetResponse(kind_, forecast_);
    feeder_.reset();
    handler_.reset();
    handler_ = std::make_unique<GoogleWeatherHandler>(forecast_, kind_);
    feeder_ = std::make_unique<GoogleWeatherFeeder>(*handler_, GoogleWeatherDocumentComplete{handler_.get()},
                                                    GoogleWeatherDocumentStarted{handler_.get()}, label(), true);
  }

  void feed(const uint8_t *data, size_t length) {
    if (feeder_ != nullptr)
      feeder_->feed(data, length);
  }

  ProviderResult finish() {
    if (feeder_ == nullptr || handler_ == nullptr)
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);

    ProviderResult result = feeder_->finish();
    if (!result.isOk()) {
      resetResponse(kind_, forecast_);
      return result;
    }
    return validateResponse(kind_, *handler_, forecast_);
  }

 private:
  const char *label() const {
    switch (kind_) {
      case ResponseKind::CURRENT:
        return "Google Weather current conditions";
      case ResponseKind::HOURLY:
        return "Google Weather hourly forecast";
      case ResponseKind::DAILY:
        return "Google Weather daily forecast";
    }
    return "Google Weather response";
  }

  forecast_t &forecast_;
  ResponseKind kind_;
  std::unique_ptr<GoogleWeatherHandler> handler_;
  std::unique_ptr<GoogleWeatherFeeder> feeder_;
};

static ProviderResult deserializeCurrent(Stream &json, forecast_t &forecast) {
  resetCurrent(forecast);
  GoogleWeatherHandler handler(forecast, ResponseKind::CURRENT);
  ProviderResult result = consume(json, handler, "Google Weather current conditions");
  if (!result.isOk()) {
    resetCurrent(forecast);
    return result;
  }
  return validateResponse(ResponseKind::CURRENT, handler, forecast);
}

static ProviderResult deserializeHourly(Stream &json, forecast_t &forecast) {
  resetHourly(forecast);
  GoogleWeatherHandler handler(forecast, ResponseKind::HOURLY);
  ProviderResult result = consume(json, handler, "Google Weather hourly forecast");
  if (!result.isOk()) {
    resetHourly(forecast);
    return result;
  }
  return validateResponse(ResponseKind::HOURLY, handler, forecast);
}

static ProviderResult deserializeDaily(Stream &json, forecast_t &forecast) {
  resetDaily(forecast);
  GoogleWeatherHandler handler(forecast, ResponseKind::DAILY);
  ProviderResult result = consume(json, handler, "Google Weather daily forecast");
  if (!result.isOk()) {
    resetDaily(forecast);
    return result;
  }
  return validateResponse(ResponseKind::DAILY, handler, forecast);
}

static String requestUrl(const char *path, bool sanitized) {
  String url = String("https://weather.googleapis.com/v1/") + path;
  url += "?key=";
  if (sanitized)
    url += "{API_KEY}";
  else
    url += GOOGLE_WEATHER_API_KEY;
  url += "&location.latitude=";
  url += LAT;
  url += "&location.longitude=";
  url += LON;
  url += "&unitsSystem=METRIC";
  return url;
}

static ProviderResult request(EspHttpClientSession &session, const String &query, const String &sanitizedQuery,
                              GoogleWeatherChunkParser &parser) {
  return session.getWithRetry(
      query, sanitizedQuery, [&parser]() { parser.beginAttempt(); },
      [&parser](const uint8_t *data, size_t length) { parser.feed(data, length); },
      [&parser]() { return parser.finish(); },
      [](esp_http_client_handle_t client) { esp_http_client_set_header(client, "Accept", "application/json"); });
}

}  // namespace

const char *GoogleWeatherForecastProvider::getApiName() const { return "Google Weather API"; }

std::vector<std::unique_ptr<FetchOperation>> GoogleWeatherForecastProvider::createFetchOperations(
    weather_report_t &out) {
  out.resetForecast();
  out.forecast.lat = strtod(LAT.c_str(), nullptr);
  out.forecast.lon = strtod(LON.c_str(), nullptr);

  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<CallbackFetchOperation>(
      getApiName(), true, [this, &out]() { return fetchForecast(out.forecast); }));
  return operations;
}

weather_condition GoogleWeatherForecastProvider::mapWeatherCondition(const char *type) {
  if (type == nullptr)
    return weather_condition::UNKNOWN;
  if (keyIs(type, "CLEAR"))
    return weather_condition::CLEAR;
  if (keyIs(type, "MOSTLY_CLEAR") || keyIs(type, "PARTLY_CLOUDY"))
    return weather_condition::PARTLY_CLOUDY;
  if (keyIs(type, "MOSTLY_CLOUDY"))
    return weather_condition::CLOUDY;
  if (keyIs(type, "CLOUDY"))
    return weather_condition::OVERCAST;
  if (keyIs(type, "LIGHT_RAIN_SHOWERS") || keyIs(type, "CHANCE_OF_SHOWERS") || keyIs(type, "SCATTERED_SHOWERS") ||
      keyIs(type, "RAIN_SHOWERS") || keyIs(type, "HEAVY_RAIN_SHOWERS"))
    return weather_condition::RAIN_SHOWERS;
  if (keyIs(type, "LIGHT_TO_MODERATE_RAIN") || keyIs(type, "MODERATE_TO_HEAVY_RAIN") || keyIs(type, "RAIN") ||
      keyIs(type, "LIGHT_RAIN") || keyIs(type, "HEAVY_RAIN") || keyIs(type, "RAIN_PERIODICALLY_HEAVY") ||
      keyIs(type, "WIND_AND_RAIN"))
    return weather_condition::RAIN;
  if (keyIs(type, "LIGHT_SNOW_SHOWERS") || keyIs(type, "CHANCE_OF_SNOW_SHOWERS") ||
      keyIs(type, "SCATTERED_SNOW_SHOWERS") || keyIs(type, "SNOW_SHOWERS") || keyIs(type, "HEAVY_SNOW_SHOWERS"))
    return weather_condition::SNOW_SHOWERS;
  if (keyIs(type, "RAIN_AND_SNOW"))
    return weather_condition::RAIN_SNOW_MIX;
  if (keyIs(type, "HAIL") || keyIs(type, "HAIL_SHOWERS"))
    return weather_condition::THUNDERSTORM_HAIL;
  if (keyIs(type, "THUNDERSTORM") || keyIs(type, "THUNDERSHOWER") || keyIs(type, "LIGHT_THUNDERSTORM_RAIN") ||
      keyIs(type, "SCATTERED_THUNDERSTORMS") || keyIs(type, "HEAVY_THUNDERSTORM"))
    return weather_condition::THUNDERSTORM;
  if (keyIs(type, "WINDY"))
    return weather_condition::SQUALL;
  if (keyIs(type, "LIGHT_TO_MODERATE_SNOW") || keyIs(type, "MODERATE_TO_HEAVY_SNOW") || keyIs(type, "SNOW") ||
      keyIs(type, "LIGHT_SNOW") || keyIs(type, "HEAVY_SNOW") || keyIs(type, "SNOWSTORM") ||
      keyIs(type, "SNOW_PERIODICALLY_HEAVY") || keyIs(type, "HEAVY_SNOW_STORM") || keyIs(type, "BLOWING_SNOW"))
    return weather_condition::SNOW;
  return weather_condition::UNKNOWN;
}

ProviderResult GoogleWeatherForecastProvider::deserializeCurrent(Stream &json, forecast_t &forecast) {
  return ::deserializeCurrent(json, forecast);
}

ProviderResult GoogleWeatherForecastProvider::deserializeHourly(Stream &json, forecast_t &forecast) {
  return ::deserializeHourly(json, forecast);
}

ProviderResult GoogleWeatherForecastProvider::deserializeDaily(Stream &json, forecast_t &forecast) {
  return ::deserializeDaily(json, forecast);
}

ProviderResult GoogleWeatherForecastProvider::fetchForecast(forecast_t &forecast) {
  esp_http_client_config_t config = {};
  config.timeout_ms = GOOGLE_WEATHER_HTTP_TIMEOUT_MS;
  config.cert_pem = cert_GTS_Root_R4;
  config.disable_auto_redirect = true;
  EspHttpClientSession session(config);

  const String currentUrl = requestUrl("currentConditions:lookup", false);
  const String sanitizedCurrentUrl = requestUrl("currentConditions:lookup", true);
  GoogleWeatherChunkParser currentParser(forecast, ResponseKind::CURRENT);
  ProviderResult result = request(session, currentUrl, sanitizedCurrentUrl, currentParser);
  if (!result.isOk())
    return result;

  const String hourlyUrl =
      requestUrl("forecast/hours:lookup", false) + "&hours=" + String(NUM_HOURLY) + "&pageSize=" + String(NUM_HOURLY);
  const String sanitizedHourlyUrl =
      requestUrl("forecast/hours:lookup", true) + "&hours=" + String(NUM_HOURLY) + "&pageSize=" + String(NUM_HOURLY);
  GoogleWeatherChunkParser hourlyParser(forecast, ResponseKind::HOURLY);
  result = request(session, hourlyUrl, sanitizedHourlyUrl, hourlyParser);
  if (!result.isOk())
    return result;

  const String dailyUrl =
      requestUrl("forecast/days:lookup", false) + "&days=" + String(NUM_DAILY) + "&pageSize=" + String(NUM_DAILY);
  const String sanitizedDailyUrl =
      requestUrl("forecast/days:lookup", true) + "&days=" + String(NUM_DAILY) + "&pageSize=" + String(NUM_DAILY);
  GoogleWeatherChunkParser dailyParser(forecast, ResponseKind::DAILY);
  return request(session, dailyUrl, sanitizedDailyUrl, dailyParser);
}

#endif  // REMOTE_PROVIDER_GOOGLE_WEATHER_FORECAST
