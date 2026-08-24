/* Open-Meteo weather provider for esp32-weather-epd.
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

#if defined(REMOTE_PROVIDER_OPEN_METEO_FORECAST)

#include <Arduino.h>
#include <WiFiClient.h>
#if !defined(OPEN_METEO_FORECAST_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(OPEN_METEO_FORECAST_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <cstdint>
#include <cstring>
#include <ArduinoStreamParser.h>
#include "_locale.h"
#include "client_utils.h"
#include "open_meteo_weather_provider.h"
#include "provider_fetch_operations.h"

/* SAX event handler: maps the Open-Meteo forecast response directly into
 * the provider-agnostic forecast model as the bytes stream in. Only the
 * `current`, `hourly` and `daily` sections are captured; everything else
 * (metadata, *_units, timezone...) is consumed and discarded. The response
 * is requested with timeformat=unixtime, so every value is a number.
 *
 * json-streaming-parser-2 (a fork of json-streaming-parser2 with 64-bit
 * number support, see platformio.ini) has no Key() event: keys are read
 * from the ElementPath handed to each value() callback instead.
 *   - `current` fields arrive at depth 2: the parent selector carries the
 *     section key ("current"), the current selector the field key.
 *   - `hourly`/`daily` array elements arrive at depth 3: the grandparent
 *     selector carries the section key, the parent selector the field key,
 *     and the current selector the array index.
 * Dispatch below mirrors that shape. Everything else (non-matching depths,
 * unknown keys, string values) is ignored.
 *
 * A payload only counts as a forecast once the three required time keys
 * were actually seen: current.time, plus non-empty hourly.time and
 * daily.time arrays. Any syntactically valid JSON that lacks them (e.g. an
 * Open-Meteo {"error": ...} response) makes isComplete() report false, and
 * deserializeCall() rejects it with InvalidInput so the caller's retry and
 * error handling can engage instead of trusting stale forecast values.
 */
class WeatherHandler : public JsonHandler {
 public:
  explicit WeatherHandler(forecast_t &forecast) : forecast_(forecast) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    // All captured Open-Meteo values are numbers; ignore strings ("units"
    // sections) and the like. getDouble() is exact here: the forked parser
    // stores integers as 64-bit (unixtime timestamps fit well below 2^53).
    if (!value.isInt() && !value.isFloat()) {
      return;
    }
    const double d = value.getDouble();
    const int depth = path.getCount();
    if (depth == 2) {
      const char *section = path.getParent() != nullptr ? path.getParent()->getKey() : nullptr;
      const char *field = path.getCurrent() != nullptr ? path.getCurrent()->getKey() : nullptr;
      if (keyIs(section, "current")) {
        storeCurrent(field, d);
      }
    } else if (depth == 3) {
      const char *section = path.get(-2) != nullptr ? path.get(-2)->getKey() : nullptr;
      const char *field = path.getParent() != nullptr ? path.getParent()->getKey() : nullptr;
      const int idx = path.getCurrent() != nullptr ? path.getCurrent()->getIndex() : -1;
      if (idx < 0) {
        return;
      }
      if (keyIs(section, "hourly")) {
        storeHourly(field, static_cast<size_t>(idx), d);
      } else if (keyIs(section, "daily")) {
        storeDaily(field, static_cast<size_t>(idx), d);
      }
    }
  }

 public:
  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }
  bool isComplete() const { return sawCurrentTime_ && sawHourlyTime_ && sawDailyTime_; }

 private:
  static bool keyIs(const char *str, const char *key) { return str != nullptr && strcmp(str, key) == 0; }

  void storeCurrent(const char *field, double value) {
    if (keyIs(field, "time")) {
      forecast_.current.dt = static_cast<int64_t>(value);
      sawCurrentTime_ = true;
    } else if (keyIs(field, "temperature_2m")) {
      forecast_.current.temp = static_cast<float>(value);
    } else if (keyIs(field, "apparent_temperature")) {
      forecast_.current.feels_like = static_cast<float>(value);
    } else if (keyIs(field, "relative_humidity_2m")) {
      forecast_.current.humidity = static_cast<int>(value);
    } else if (keyIs(field, "dew_point_2m")) {
      forecast_.current.dew_point = static_cast<float>(value);
    } else if (keyIs(field, "weather_code")) {
      forecast_.current.weather.condition = OpenMeteoForecastProvider::mapWeatherCode(static_cast<int>(value));
    } else if (keyIs(field, "cloud_cover")) {
      forecast_.current.clouds = static_cast<int>(value);
    } else if (keyIs(field, "visibility")) {
      forecast_.current.visibility = static_cast<int>(value);
    } else if (keyIs(field, "surface_pressure")) {
      forecast_.current.pressure = static_cast<int>(value);
    } else if (keyIs(field, "wind_speed_10m")) {
      forecast_.current.wind_speed = static_cast<float>(value);
    } else if (keyIs(field, "wind_direction_10m")) {
      forecast_.current.wind_deg = static_cast<int>(value);
    } else if (keyIs(field, "wind_gusts_10m")) {
      forecast_.current.wind_gust = static_cast<float>(value);
    } else if (keyIs(field, "is_day")) {
      forecast_.current.is_day = value != 0.0;
    }
  }

  void storeHourly(const char *field, size_t idx, double value) {
    if (idx >= NUM_HOURLY) {
      return;
    }
    if (keyIs(field, "time")) {
      forecast_.hourly[idx].dt = static_cast<int64_t>(value);
      if (idx == 0) {
        sawHourlyTime_ = true;
      }
    } else if (keyIs(field, "temperature_2m")) {
      forecast_.hourly[idx].temp = static_cast<float>(value);
    } else if (keyIs(field, "cloud_cover")) {
      forecast_.hourly[idx].clouds = static_cast<int>(value);
    } else if (keyIs(field, "wind_speed_10m")) {
      forecast_.hourly[idx].wind_speed = static_cast<float>(value);
    } else if (keyIs(field, "wind_gusts_10m")) {
      forecast_.hourly[idx].wind_gust = static_cast<float>(value);
    } else if (keyIs(field, "precipitation_probability")) {
      forecast_.hourly[idx].pop = static_cast<int>(value);
    } else if (keyIs(field, "rain")) {
      forecast_.hourly[idx].rain_1h = static_cast<float>(value);
    } else if (keyIs(field, "snowfall")) {
      forecast_.hourly[idx].snow_1h = static_cast<float>(value);
    } else if (keyIs(field, "weather_code")) {
      forecast_.hourly[idx].weather.condition = OpenMeteoForecastProvider::mapWeatherCode(static_cast<int>(value));
    } else if (keyIs(field, "is_day")) {
      forecast_.hourly[idx].is_day = value != 0.0;
    } else if (keyIs(field, "soil_temperature_18cm")) {
      if (idx == 0) {
        forecast_.current.soil_temperature_18cm = static_cast<float>(value);
      }
    }
  }

  void storeDaily(const char *field, size_t idx, double value) {
    if (idx >= NUM_DAILY) {
      return;
    }
    if (keyIs(field, "time")) {
      forecast_.daily[idx].dt = static_cast<int64_t>(value);
      if (idx == 0) {
        sawDailyTime_ = true;
      }
    } else if (keyIs(field, "temperature_2m_max")) {
      forecast_.daily[idx].temp.max = static_cast<float>(value);
    } else if (keyIs(field, "temperature_2m_min")) {
      forecast_.daily[idx].temp.min = static_cast<float>(value);
    } else if (keyIs(field, "sunrise")) {
      if (idx == 0) {
        forecast_.current.sunrise = static_cast<int64_t>(value);
      }
      forecast_.daily[idx].sunrise = static_cast<int64_t>(value);
    } else if (keyIs(field, "sunset")) {
      if (idx == 0) {
        forecast_.current.sunset = static_cast<int64_t>(value);
      }
      forecast_.daily[idx].sunset = static_cast<int64_t>(value);
    } else if (keyIs(field, "uv_index_max")) {
      if (idx == 0) {
        forecast_.current.uvi = static_cast<float>(value);
      }
      forecast_.daily[idx].uvi = static_cast<float>(value);
    } else if (keyIs(field, "rain_sum")) {
      forecast_.daily[idx].rain = static_cast<float>(value);
    } else if (keyIs(field, "snowfall_sum")) {
      forecast_.daily[idx].snow = static_cast<float>(value);
    } else if (keyIs(field, "precipitation_probability_max")) {
      forecast_.daily[idx].pop = static_cast<int>(value);
    } else if (keyIs(field, "wind_speed_10m_max")) {
      forecast_.daily[idx].wind_speed = static_cast<float>(value);
    } else if (keyIs(field, "wind_gusts_10m_max")) {
      forecast_.daily[idx].wind_gust = static_cast<float>(value);
    } else if (keyIs(field, "weather_code")) {
      forecast_.daily[idx].weather.condition = OpenMeteoForecastProvider::mapWeatherCode(static_cast<int>(value));
    } else if (keyIs(field, "shortwave_radiation_sum")) {
      forecast_.daily[idx].shortwave_radiation_sum = static_cast<float>(value);
    }
  }

  forecast_t &forecast_;
  bool sawStart_ = false;
  bool documentDone_ = false;
  bool sawCurrentTime_ = false;
  bool sawHourlyTime_ = false;
  bool sawDailyTime_ = false;
};

const char *OpenMeteoForecastProvider::getApiName() const {
  return "Open Meteo API";
}  // OpenMeteoForecastProvider::getApiName

std::vector<std::unique_ptr<FetchOperation>> OpenMeteoForecastProvider::createFetchOperations(weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), true, [this, &out]() {
    out.resetForecast();
    return fetch(out.forecast);
  }));
  return operations;
}

/* Map a WMO weather interpretation code (WW) onto the unified weather
 * condition enum.
 *
 * References:
 *   https://www.nodc.noaa.gov/archive/arc0021/0002199/1.1/data/0-data/HTML/WMO-CODE/WMO4677.HTM
 */
weather_condition OpenMeteoForecastProvider::mapWeatherCode(int id) {
  switch (id) {
    case 0:  // Clear sky
      return weather_condition::CLEAR;
    case 1:  // Mainly clear
      return weather_condition::PARTLY_CLOUDY;
    case 2:  // Partly cloudy
      return weather_condition::CLOUDY;
    case 3:  // Overcast
      return weather_condition::OVERCAST;
    case 45:  // Fog
    case 48:  // Depositing rime fog
      return weather_condition::FOG;
    case 51:  // Drizzle: light intensity
    case 53:  // Drizzle: moderate intensity
    case 55:  // Drizzle: dense intensity
      return weather_condition::DRIZZLE;
    case 56:  // Freezing drizzle: light intensity
    case 57:  // Freezing drizzle: dense intensity
      return weather_condition::FREEZING_DRIZZLE;
    case 61:  // Rain: slight intensity
    case 63:  // Rain: moderate intensity
    case 65:  // Rain: heavy intensity
      return weather_condition::RAIN;
    case 66:  // Freezing rain: light intensity
    case 67:  // Freezing rain: heavy intensity
      return weather_condition::FREEZING_RAIN;
    case 71:  // Snow fall: slight intensity
    case 73:  // Snow fall: moderate intensity
    case 75:  // Snow fall: heavy intensity
      return weather_condition::SNOW;
    case 77:  // Snow grains
      return weather_condition::SNOW_GRAINS;
    case 80:  // Rain showers: slight
    case 81:  // Rain showers: moderate
    case 82:  // Rain showers: violent
      return weather_condition::RAIN_SHOWERS;
    case 85:  // Snow showers: slight
    case 86:  // Snow showers: heavy
      return weather_condition::SNOW_SHOWERS;
    case 95:  // Thunderstorm: slight or moderate
      return weather_condition::THUNDERSTORM;
    case 96:  // Thunderstorm with slight hail
    case 99:  // Thunderstorm with heavy hail
      return weather_condition::THUNDERSTORM_HAIL;
    default:
      return weather_condition::UNKNOWN;
  }
}  // OpenMeteoForecastProvider::mapWeatherCode

/* Perform an HTTP GET request to Open-Meteo's forecast API and map the
 * response into the generic forecast model.
 */
ProviderResult OpenMeteoForecastProvider::fetch(forecast_t &forecast) {
#if defined(OPEN_METEO_FORECAST_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(OPEN_METEO_FORECAST_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // OPEN_METEO_FORECAST_TRANSPORT_HTTPS_VERIFY
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
}  // OpenMeteoForecastProvider::fetch

/* Map a streamed response of the Open-Meteo forecast API into the generic
 * forecast model directly as the bytes stream in. */
ProviderResult OpenMeteoForecastProvider::deserializeCall(Stream &json, forecast_t &forecast) {
  // The model is long-lived in the caller and shared with the previous fetch:
  // clear it first, so values a response does not carry can never survive.
  // Rejections reset it again, leaving the model clean after any non-Ok.
  forecast.reset();
  WeatherHandler handler(forecast);
  ArduinoStreamParser parser;
  parser.setHandler(&handler);
  // Feed the parser byte by byte until the root document closes or an error
  // is flagged. The Open-Meteo API never sends a Content-Length (HTTP/1.0
  // responses are close-delimited), so there is no declared size to read up
  // to. Reading one byte at a time never blocks waiting for a full buffer:
  // readBytes() only waits for the next byte (or the socket timeout), and the
  // loop exits the moment the document is complete, so there is no trailing
  // read to stall on either.
  uint8_t b;
  while (!parser.hasParseError() && !handler.finishedDocument() && json.readBytes(&b, 1) > 0) {
    parser.write(&b, 1);
  }
  if (parser.hasParseError()) {
    // Genuinely malformed JSON flagged by the parser. Trailing bytes after a
    // completed document never reach this branch: the read loop above exits as
    // soon as endDocument() fires, so anything following the JSON is simply not
    // fed to the parser.
    LOG_WARNING("Open-Meteo JSON parse error: %s", parser.getErrorMessage());
    forecast.reset();
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (handler.finishedDocument()) {
    if (!handler.isComplete()) {
      LOG_WARNING(
          "Open-Meteo response is no forecast: required time keys (current.time, hourly.time, daily.time) missing");
      forecast.reset();
      return ProviderResult::error(String(TXT_DESERIALIZATION_ERROR_INVALID_INPUT) +
                                   " (missing current/hourly/daily time)");
    }
    return ProviderResult::ok();
  }
  forecast.reset();
  // The body never closed the root document: empty responses and truncated
  // bodies are both silent for this parser, so distinguish them by whether
  // any parse event happened at all.
  if (!handler.sawStart()) {
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  }
  return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
}  // OpenMeteoForecastProvider::deserializeCall

#endif  // REMOTE_PROVIDER_OPEN_METEO_FORECAST