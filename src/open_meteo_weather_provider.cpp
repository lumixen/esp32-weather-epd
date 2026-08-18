#include "config.h"
#include "logger.h"

#if defined(WEATHER_API_PROVIDER_OPEN_METEO)

#include <Arduino.h>
#include <WiFiClient.h>
#if !defined(WEATHER_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(WEATHER_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <cstdint>
#include <cstring>
#include <rapidjson/reader.h>
#include "_locale.h"
#include "client_utils.h"
#include "open_meteo_weather_provider.h"

/* SAX event handler: maps the Open-Meteo forecast response directly into
 * the provider-agnostic forecast model as the bytes stream in. Only the
 * `current`, `hourly` and `daily` sections are captured; everything else
 * (metadata, *_units, timezone...) is consumed and discarded. The response
 * is requested with timeformat=unixtime, so every value is a number; a
 * single double-precision store path is sufficient (unix timestamps are
 * well below 2^53 and round-trip to int64_t exactly).
 *
 * A payload only counts as a forecast once the three required time keys
 * were actually seen: current.time, plus non-empty hourly.time and
 * daily.time arrays. Any syntactically valid JSON that lacks them (e.g. an
 * Open-Meteo {"error": ...} response) makes isComplete() report false, and
 * deserializeCall() rejects it with InvalidInput so the caller's retry and
 * error handling can engage instead of trusting stale forecast values.
 */
class WeatherHandler {
 public:
  explicit WeatherHandler(forecast_t &forecast) : forecast_(forecast) {}

  bool Null() { return true; }
  bool Bool(bool b) {
    storeScalar(b ? 1.0 : 0.0);
    return true;
  }
  bool Int(int v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Uint(unsigned v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Int64(int64_t v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Uint64(uint64_t v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Double(double v) {
    storeScalar(v);
    return true;
  }
  // Not invoked without kParseNumbersAsStringsFlag; return true to be safe.
  bool RawNumber(const char *, rapidjson::SizeType, bool) { return true; }
  bool String(const char *, rapidjson::SizeType, bool) { return true; }

  bool StartObject() {
    ++depth_;
    return true;
  }

  bool Key(const char *str, rapidjson::SizeType length, bool) {
    col_ = Col::NONE;
    if (inArray_ || (depth_ != 1 && depth_ != 2)) {
      return true;
    }
    if (depth_ == 1) {
      // Top-level keys select the section to capture.
      if (keyEquals(str, length, "current")) {
        section_ = Section::CURRENT;
      } else if (keyEquals(str, length, "hourly")) {
        section_ = Section::HOURLY;
      } else if (keyEquals(str, length, "daily")) {
        section_ = Section::DAILY;
      } else {
        section_ = Section::NONE;
      }
      return true;
    }
    // Keys inside the section objects select the value column.
    switch (section_) {
      case Section::CURRENT:
        col_ = currentColumn(str, length);
        break;
      case Section::HOURLY:
        col_ = hourlyColumn(str, length);
        break;
      case Section::DAILY:
        col_ = dailyColumn(str, length);
        break;
      default:
        break;
    }
    return true;
  }

  bool EndObject(rapidjson::SizeType) {
    --depth_;
    return true;
  }

  bool StartArray() {
    ++depth_;
    if (!inArray_ && col_ != Col::NONE) {
      inArray_ = true;
      idx_ = 0;
    }
    return true;
  }

  bool EndArray(rapidjson::SizeType) {
    --depth_;
    inArray_ = false;
    return true;
  }

 private:
  enum class Section : uint8_t { NONE, CURRENT, HOURLY, DAILY };

  enum class Col : uint8_t {
    NONE,
    CUR_TIME,
    CUR_TEMP,
    CUR_FEELS,
    CUR_HUMIDITY,
    CUR_DEW_POINT,
    CUR_WEATHER_CODE,
    CUR_CLOUDS,
    CUR_VISIBILITY,
    CUR_PRESSURE,
    CUR_WIND_SPEED,
    CUR_WIND_DEG,
    CUR_WIND_GUST,
    CUR_IS_DAY,
    HOUR_TIME,
    HOUR_TEMP,
    HOUR_CLOUDS,
    HOUR_WIND_SPEED,
    HOUR_WIND_GUST,
    HOUR_POP,
    HOUR_RAIN,
    HOUR_SNOW,
    HOUR_WEATHER_CODE,
    HOUR_IS_DAY,
    HOUR_SOIL_TEMP,
    DAY_TIME,
    DAY_TEMP_MAX,
    DAY_TEMP_MIN,
    DAY_SUNRISE,
    DAY_SUNSET,
    DAY_UVI,
    DAY_RAIN,
    DAY_SNOW,
    DAY_POP,
    DAY_WIND_SPEED,
    DAY_WIND_GUST,
    DAY_WEATHER_CODE,
    DAY_SR_SUM
  };

  static bool keyEquals(const char *str, rapidjson::SizeType len, const char *key) {
    const size_t keyLen = strlen(key);
    return len == keyLen && strncmp(str, key, keyLen) == 0;
  }

  static Col currentColumn(const char *str, rapidjson::SizeType len) {
    if (keyEquals(str, len, "time")) {
      return Col::CUR_TIME;
    }
    if (keyEquals(str, len, "temperature_2m")) {
      return Col::CUR_TEMP;
    }
    if (keyEquals(str, len, "apparent_temperature")) {
      return Col::CUR_FEELS;
    }
    if (keyEquals(str, len, "relative_humidity_2m")) {
      return Col::CUR_HUMIDITY;
    }
    if (keyEquals(str, len, "dew_point_2m")) {
      return Col::CUR_DEW_POINT;
    }
    if (keyEquals(str, len, "weather_code")) {
      return Col::CUR_WEATHER_CODE;
    }
    if (keyEquals(str, len, "cloud_cover")) {
      return Col::CUR_CLOUDS;
    }
    if (keyEquals(str, len, "visibility")) {
      return Col::CUR_VISIBILITY;
    }
    if (keyEquals(str, len, "surface_pressure")) {
      return Col::CUR_PRESSURE;
    }
    if (keyEquals(str, len, "wind_speed_10m")) {
      return Col::CUR_WIND_SPEED;
    }
    if (keyEquals(str, len, "wind_direction_10m")) {
      return Col::CUR_WIND_DEG;
    }
    if (keyEquals(str, len, "wind_gusts_10m")) {
      return Col::CUR_WIND_GUST;
    }
    if (keyEquals(str, len, "is_day")) {
      return Col::CUR_IS_DAY;
    }
    return Col::NONE;
  }

  static Col hourlyColumn(const char *str, rapidjson::SizeType len) {
    if (keyEquals(str, len, "time")) {
      return Col::HOUR_TIME;
    }
    if (keyEquals(str, len, "temperature_2m")) {
      return Col::HOUR_TEMP;
    }
    if (keyEquals(str, len, "cloud_cover")) {
      return Col::HOUR_CLOUDS;
    }
    if (keyEquals(str, len, "wind_speed_10m")) {
      return Col::HOUR_WIND_SPEED;
    }
    if (keyEquals(str, len, "wind_gusts_10m")) {
      return Col::HOUR_WIND_GUST;
    }
    if (keyEquals(str, len, "precipitation_probability")) {
      return Col::HOUR_POP;
    }
    if (keyEquals(str, len, "rain")) {
      return Col::HOUR_RAIN;
    }
    if (keyEquals(str, len, "snowfall")) {
      return Col::HOUR_SNOW;
    }
    if (keyEquals(str, len, "weather_code")) {
      return Col::HOUR_WEATHER_CODE;
    }
    if (keyEquals(str, len, "is_day")) {
      return Col::HOUR_IS_DAY;
    }
    if (keyEquals(str, len, "soil_temperature_18cm")) {
      return Col::HOUR_SOIL_TEMP;
    }
    return Col::NONE;
  }

  static Col dailyColumn(const char *str, rapidjson::SizeType len) {
    if (keyEquals(str, len, "time")) {
      return Col::DAY_TIME;
    }
    if (keyEquals(str, len, "temperature_2m_max")) {
      return Col::DAY_TEMP_MAX;
    }
    if (keyEquals(str, len, "temperature_2m_min")) {
      return Col::DAY_TEMP_MIN;
    }
    if (keyEquals(str, len, "sunrise")) {
      return Col::DAY_SUNRISE;
    }
    if (keyEquals(str, len, "sunset")) {
      return Col::DAY_SUNSET;
    }
    if (keyEquals(str, len, "uv_index_max")) {
      return Col::DAY_UVI;
    }
    if (keyEquals(str, len, "rain_sum")) {
      return Col::DAY_RAIN;
    }
    if (keyEquals(str, len, "snowfall_sum")) {
      return Col::DAY_SNOW;
    }
    if (keyEquals(str, len, "precipitation_probability_max")) {
      return Col::DAY_POP;
    }
    if (keyEquals(str, len, "wind_speed_10m_max")) {
      return Col::DAY_WIND_SPEED;
    }
    if (keyEquals(str, len, "wind_gusts_10m_max")) {
      return Col::DAY_WIND_GUST;
    }
    if (keyEquals(str, len, "weather_code")) {
      return Col::DAY_WEATHER_CODE;
    }
    if (keyEquals(str, len, "shortwave_radiation_sum")) {
      return Col::DAY_SR_SUM;
    }
    return Col::NONE;
  }

  void storeCurrent(double value) {
    switch (col_) {
      case Col::CUR_TIME:
        forecast_.current.dt = static_cast<int64_t>(value);
        sawCurrentTime_ = true;
        break;
      case Col::CUR_TEMP:
        forecast_.current.temp = static_cast<float>(value);
        break;
      case Col::CUR_FEELS:
        forecast_.current.feels_like = static_cast<float>(value);
        break;
      case Col::CUR_HUMIDITY:
        forecast_.current.humidity = static_cast<int>(value);
        break;
      case Col::CUR_DEW_POINT:
        forecast_.current.dew_point = static_cast<float>(value);
        break;
      case Col::CUR_WEATHER_CODE:
        forecast_.current.weather.id = static_cast<int>(value);
        break;
      case Col::CUR_CLOUDS:
        forecast_.current.clouds = static_cast<int>(value);
        break;
      case Col::CUR_VISIBILITY:
        forecast_.current.visibility = static_cast<int>(value);
        break;
      case Col::CUR_PRESSURE:
        forecast_.current.pressure = static_cast<int>(value);
        break;
      case Col::CUR_WIND_SPEED:
        forecast_.current.wind_speed = static_cast<float>(value);
        break;
      case Col::CUR_WIND_DEG:
        forecast_.current.wind_deg = static_cast<int>(value);
        break;
      case Col::CUR_WIND_GUST:
        forecast_.current.wind_gust = static_cast<float>(value);
        break;
      case Col::CUR_IS_DAY:
        forecast_.current.is_day = value != 0.0;
        break;
      default:
        break;
    }
  }

  void storeHourly(double value) {
    if (idx_ >= NUM_HOURLY) {
      return;
    }
    switch (col_) {
      case Col::HOUR_TIME:
        forecast_.hourly[idx_].dt = static_cast<int64_t>(value);
        sawHourlyTime_ = true;
        break;
      case Col::HOUR_TEMP:
        forecast_.hourly[idx_].temp = static_cast<float>(value);
        break;
      case Col::HOUR_CLOUDS:
        forecast_.hourly[idx_].clouds = static_cast<int>(value);
        break;
      case Col::HOUR_WIND_SPEED:
        forecast_.hourly[idx_].wind_speed = static_cast<float>(value);
        break;
      case Col::HOUR_WIND_GUST:
        forecast_.hourly[idx_].wind_gust = static_cast<float>(value);
        break;
      case Col::HOUR_POP:
        forecast_.hourly[idx_].pop = static_cast<int>(value);
        break;
      case Col::HOUR_RAIN:
        forecast_.hourly[idx_].rain_1h = static_cast<float>(value);
        break;
      case Col::HOUR_SNOW:
        forecast_.hourly[idx_].snow_1h = static_cast<float>(value);
        break;
      case Col::HOUR_WEATHER_CODE:
        forecast_.hourly[idx_].weather.id = static_cast<int>(value);
        break;
      case Col::HOUR_IS_DAY:
        forecast_.hourly[idx_].is_day = value != 0.0;
        break;
      case Col::HOUR_SOIL_TEMP:
        if (idx_ == 0) {
          forecast_.current.soil_temperature_18cm = static_cast<float>(value);
        }
        break;
      default:
        return;
    }
    ++idx_;
  }

  void storeDaily(double value) {
    if (idx_ >= NUM_DAILY) {
      return;
    }
    switch (col_) {
      case Col::DAY_TIME:
        forecast_.daily[idx_].dt = static_cast<int64_t>(value);
        sawDailyTime_ = true;
        break;
      case Col::DAY_TEMP_MAX:
        forecast_.daily[idx_].temp.max = static_cast<float>(value);
        break;
      case Col::DAY_TEMP_MIN:
        forecast_.daily[idx_].temp.min = static_cast<float>(value);
        break;
      case Col::DAY_SUNRISE:
        if (idx_ == 0) {
          forecast_.current.sunrise = static_cast<int64_t>(value);
        }
        forecast_.daily[idx_].sunrise = static_cast<int64_t>(value);
        break;
      case Col::DAY_SUNSET:
        if (idx_ == 0) {
          forecast_.current.sunset = static_cast<int64_t>(value);
        }
        forecast_.daily[idx_].sunset = static_cast<int64_t>(value);
        break;
      case Col::DAY_UVI:
        if (idx_ == 0) {
          forecast_.current.uvi = static_cast<float>(value);
        }
        forecast_.daily[idx_].uvi = static_cast<float>(value);
        break;
      case Col::DAY_RAIN:
        forecast_.daily[idx_].rain = static_cast<float>(value);
        break;
      case Col::DAY_SNOW:
        forecast_.daily[idx_].snow = static_cast<float>(value);
        break;
      case Col::DAY_POP:
        forecast_.daily[idx_].pop = static_cast<int>(value);
        break;
      case Col::DAY_WIND_SPEED:
        forecast_.daily[idx_].wind_speed = static_cast<float>(value);
        break;
      case Col::DAY_WIND_GUST:
        forecast_.daily[idx_].wind_gust = static_cast<float>(value);
        break;
      case Col::DAY_WEATHER_CODE:
        forecast_.daily[idx_].weather.id = static_cast<int>(value);
        break;
      case Col::DAY_SR_SUM:
        forecast_.daily[idx_].shortwave_radiation_sum = static_cast<float>(value);
        break;
      default:
        return;
    }
    ++idx_;
  }

  void storeScalar(double value) {
    if (section_ == Section::CURRENT && !inArray_) {
      storeCurrent(value);
    } else if (section_ == Section::HOURLY && inArray_) {
      storeHourly(value);
    } else if (section_ == Section::DAILY && inArray_) {
      storeDaily(value);
    }
  }

  public:
  bool isComplete() const {
    return sawCurrentTime_ && sawHourlyTime_ && sawDailyTime_;
  }

  forecast_t &forecast_;
  Section section_ = Section::NONE;
  Col col_ = Col::NONE;
  uint8_t depth_ = 0;
  bool inArray_ = false;
  bool sawCurrentTime_ = false;
  bool sawHourlyTime_ = false;
  bool sawDailyTime_ = false;
  size_t idx_ = 0;
};

const char *OpenMeteoWeatherProvider::getApiName() const {
  return "Open Meteo API";
}  // OpenMeteoWeatherProvider::getApiName

/* Perform an HTTP GET request to Open-Meteo's forecast API and map the
 * response into the generic forecast model.
 */
ProviderResult OpenMeteoWeatherProvider::fetch(forecast_t &forecast) {
#if defined(WEATHER_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(WEATHER_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // WEATHER_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_ISRG_Root_X1);
  const uint16_t port = 443;
#endif
  String uri =
      "/v1/forecast?latitude=" + LAT + "&longitude=" + LON + "&" +
      "current=temperature_2m,relative_humidity_2m,dew_point_2m,apparent_temperature,weather_code,cloud_cover,"
      "visibility,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day&" +
      "hourly=temperature_2m,cloud_cover,wind_speed_10m,wind_gusts_10m,precipitation_probability,rain,snowfall,weather_"
      "code,is_day&" +
      "daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,rain_sum,snowfall_sum,"
      "precipitation_probability_max,wind_speed_10m_max,wind_gusts_10m_max&" +
      "wind_speed_unit=ms&timezone=auto&timeformat=unixtime&forecast_days=5&forecast_hours=" + HOURLY_GRAPH_MAX;

  // This string is printed to terminal to help with debugging.
  String sanitizedUri = OM_ENDPOINT + uri;

  return httpGetWithRetry(client, OM_ENDPOINT, port, uri, sanitizedUri, true, HTTP_CLIENT_TCP_TIMEOUT,
                          [&forecast](Stream &json, size_t) { return deserializeCall(json, forecast); });
}  // OpenMeteoWeatherProvider::fetch

ProviderResult OpenMeteoWeatherProvider::deserializeCall(Stream &json, forecast_t &forecast) {
  // The model is long-lived in the caller and shared with the previous fetch:
  // clear it first, so values a response does not carry can never survive.
  // Rejections reset it again, leaving the model clean after any non-Ok.
  forecast.reset();
  LOG_DEBUG("heap before streamed JSON parse: %u B free", ESP.getFreeHeap());
  StreamInput input(json);
  WeatherHandler handler(forecast);
  rapidjson::Reader reader;
  // kParseStopWhenDoneFlag stops right after the root JSON value, so any
  // trailing bytes in the HTTP body do not trip a RootNotSingular error.
  rapidjson::ParseResult result = reader.Parse<rapidjson::kParseStopWhenDoneFlag>(input, handler);
  LOG_DEBUG("heap after streamed JSON parse: %u B free", ESP.getFreeHeap());
  if (result.IsError()) {
    LOG_WARNING("Open-Meteo JSON parse error %u at offset %zu", static_cast<unsigned>(result.Code()), result.Offset());
    forecast.reset();
    // rapidjson reports a truncated body as a syntax error; classify it as
    // IncompleteInput when the stream was exhausted at the failure point.
    if (result.Code() == rapidjson::kParseErrorDocumentEmpty) {
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
    }
    if (input.reachedEof()) {
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
    }
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (!handler.isComplete()) {
    LOG_WARNING("Open-Meteo response is no forecast: required time keys (current.time, hourly.time, daily.time) missing");
    forecast.reset();
    return ProviderResult::error(String(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) + " (missing current/hourly/daily time)");
  }
  return ProviderResult::ok();
}  // OpenMeteoWeatherProvider::deserializeCall

#endif  // WEATHER_API_PROVIDER_OPEN_METEO