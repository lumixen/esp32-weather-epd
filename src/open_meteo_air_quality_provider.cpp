/* Open-Meteo air-quality provider for esp32-weather-epd.
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

#if defined(REMOTE_PROVIDER_OPEN_METEO_AIR_QUALITY)

#include <Arduino.h>
#if defined(OPEN_METEO_AIR_QUALITY_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <ArduinoStreamParser.h>
#include <cstdint>
#include <cstring>
#include <time.h>
#include <vector>
#include "_locale.h"
#include "esp_http_client_utils.h"
#include "open_meteo_air_quality_provider.h"
#include "provider_fetch_operations.h"

static ProviderResult parseAirQualityResponse(esp_http_client_handle_t client, air_quality_t &airQuality);

/* Perform an HTTP GET request to Open-Meteo's air quality API and map the
 * response into the generic air quality model.
 */
ProviderResult OpenMeteoAirQualityProvider::fetch(air_quality_t &airQuality) {
  const String uri =
      "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON +
      "&hourly=pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ammonia,nitrogen_monoxide,ozone,pm10&"
      "past_days=1&forecast_days=1&timeformat=unixtime";
#if defined(OPEN_METEO_AIR_QUALITY_TRANSPORT_HTTP)
  const String url = "http://" + OM_AIR_QUALITY_ENDPOINT + uri;
#else  // OPEN_METEO_AIR_QUALITY_TRANSPORT_HTTPS_*
  const String url = "https://" + OM_AIR_QUALITY_ENDPOINT + uri;
#endif
  const String sanitizedUri = OM_AIR_QUALITY_ENDPOINT + uri;

  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
#if defined(OPEN_METEO_AIR_QUALITY_TRANSPORT_HTTPS_VERIFY)
  config.cert_pem = cert_ISRG_Root_X1;
#endif

  return espHttpGetWithRetry(url, sanitizedUri, config, [&airQuality](esp_http_client_handle_t client) {
    return parseAirQualityResponse(client, airQuality);
  });
}  // OpenMeteoAirQualityProvider::fetch

std::vector<std::unique_ptr<FetchOperation>> OpenMeteoAirQualityProvider::createFetchOperations(weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), false, [this, &out]() {
    out.resetAirQuality();
    air_quality_t &airQuality = out.engageAirQuality();
    ProviderResult result = fetch(airQuality);
    // A valid response with no timestamp at or before now contains no
    // usable readings and must not render as zero-filled data.
    if (!result.isOk() || airQuality.dt[0] == 0) {
      out.resetAirQuality();
    }
    return result;
  }));
  return operations;
}

/* SAX event handler: maps the Open-Meteo air quality response directly into
 * the generic air quality model as the bytes stream in. Only arrays below the
 * `hourly` object are consumed; metadata, units, and unknown fields are
 * ignored.
 *
 * The request uses timeformat=unixtime, so timestamps are integers and
 * concentrations are numbers or null. The `time` array is emitted before the
 * pollutant arrays by the Open-Meteo API. Once it closes, the handler knows
 * the last timestamp at or before `now` and can write every subsequent
 * pollutant array directly into the selected 24-entry window.
 */
class AirQualityHandler : public JsonHandler {
 public:
  explicit AirQualityHandler(air_quality_t &airQuality) : airQuality_(airQuality), now_(time(nullptr)) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath path) override {
    isTimeArray_ = path.getCount() == 2 && keyIs(path.get(0), "hourly") && keyIs(path.getCurrent(), "time");
  }
  void endArray(ElementPath) override {
    if (isTimeArray_) {
      finalizeWindow();
    }
    isTimeArray_ = false;
  }
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() != 3 || !keyIs(path.get(0), "hourly")) {
      return;
    }

    ElementSelector *indexSelector = path.getCurrent();
    ElementSelector *fieldSelector = path.getParent();
    if (indexSelector == nullptr || fieldSelector == nullptr) {
      return;
    }
    const int index = indexSelector->getIndex();
    if (index < 0) {
      return;
    }
    const char *field = fieldSelector->getKey();

    if (keyIs(field, "time")) {
      if (value.isInt() || value.isFloat()) {
        storeTime(static_cast<int64_t>(value.getDouble()), index);
      } else if (value.isNull()) {
        // ArduinoJson's .as<int64_t>() converted null to zero.
        storeTime(0, index);
      }
      return;
    }

    if (value.isInt() || value.isFloat()) {
      storeComponent(field, index, static_cast<float>(value.getDouble()));
    } else if (value.isNull()) {
      // ArduinoJson's .as<float>() converted null and absent values to zero.
      storeComponent(field, index, 0.0f);
    }
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }

 private:
  static bool keyIs(ElementSelector *selector, const char *key) {
    return selector != nullptr && selector->isObject() && keyIs(selector->getKey(), key);
  }

  static bool keyIs(const char *str, const char *key) { return str != nullptr && strcmp(str, key) == 0; }

  void storeTime(int64_t timestamp, int index) {
    sawTime_ = true;
    if (timestamp <= now_) {
      closestIndex_ = index;
      if (timeCount_ < NUM_AIR_POLLUTION) {
        airQuality_.dt[timeCount_++] = timestamp;
      } else {
        memmove(airQuality_.dt, airQuality_.dt + 1, (NUM_AIR_POLLUTION - 1) * sizeof(int64_t));
        airQuality_.dt[NUM_AIR_POLLUTION - 1] = timestamp;
      }
    }
  }

  void finalizeWindow() {
    if (!sawTime_ || closestIndex_ < 0) {
      return;
    }
    selectedStart_ = closestIndex_ - NUM_AIR_POLLUTION + 1;
    if (selectedStart_ < 0) {
      selectedStart_ = 0;
    }
    hasWindow_ = true;
  }

  void storeComponent(const char *field, int index, float value) {
    if (!hasWindow_ || index < selectedStart_ || index > closestIndex_) {
      return;
    }
    const int destination = index - selectedStart_;
    if (destination < 0 || destination >= NUM_AIR_POLLUTION) {
      return;
    }

    if (keyIs(field, "pm2_5")) {
      airQuality_.components.pm2_5[destination] = value;
    } else if (keyIs(field, "pm10")) {
      airQuality_.components.pm10[destination] = value;
    } else if (keyIs(field, "carbon_monoxide")) {
      airQuality_.components.co[destination] = value;
    } else if (keyIs(field, "nitrogen_monoxide")) {
      airQuality_.components.no[destination] = value;
    } else if (keyIs(field, "nitrogen_dioxide")) {
      airQuality_.components.no2[destination] = value;
    } else if (keyIs(field, "ozone")) {
      airQuality_.components.o3[destination] = value;
    } else if (keyIs(field, "sulphur_dioxide")) {
      airQuality_.components.so2[destination] = value;
    } else if (keyIs(field, "ammonia")) {
      airQuality_.components.nh3[destination] = value;
    }
  }

  air_quality_t &airQuality_;
  const int64_t now_;
  bool sawStart_ = false;
  bool documentDone_ = false;
  bool sawTime_ = false;
  bool hasWindow_ = false;
  int closestIndex_ = -1;
  int selectedStart_ = 0;
  size_t timeCount_ = 0;
  bool isTimeArray_ = false;
};

/* Adapter around the SAX parser that can be fed either from an Arduino
 * Stream (the unit-test interface) or directly from esp_http_client_read().
 * A new instance is created for every retry so a failed attempt can never
 * contaminate the next response. */
class AirQualityResponseParser {
 public:
  explicit AirQualityResponseParser(air_quality_t &airQuality) : airQuality_(airQuality), handler_(airQuality) {
    memset(&airQuality_, 0, sizeof(air_quality_t));
    parser_.setHandler(&handler_);
  }

  void feed(const char *data, size_t length) {
    for (size_t i = 0; i < length && !hasParseError() && !finishedDocument(); ++i) {
      uint8_t b = static_cast<uint8_t>(data[i]);
      parser_.write(&b, 1);
    }
  }

  bool hasParseError() const { return parser_.hasParseError(); }
  bool finishedDocument() const { return handler_.finishedDocument(); }
  void discard() { clearModel(); }

  ProviderResult finish() {
    if (hasParseError()) {
      LOG_WARNING("Open-Meteo air quality JSON parse error: %s", parser_.getErrorMessage());
      clearModel();
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
    }
    if (finishedDocument()) {
      // A syntactically valid response without hourly data is still accepted,
      // matching the old DOM parser's behavior for API error payloads and {}.
      return ProviderResult::ok();
    }

    clearModel();
    if (!handler_.sawStart()) {
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
    }
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
  }

 private:
  void clearModel() { memset(&airQuality_, 0, sizeof(air_quality_t)); }

  air_quality_t &airQuality_;
  AirQualityHandler handler_;
  ArduinoStreamParser parser_;
};

/* Read one response using bounded storage and feed the JSON parser directly.
 * The HTTP client does not expose an Arduino Stream, so this is the network
 * counterpart of deserializeAirQuality(). */
static ProviderResult parseAirQualityResponse(esp_http_client_handle_t client, air_quality_t &airQuality) {
  AirQualityResponseParser parser(airQuality);
  std::vector<char> buffer(1024);

  while (!parser.hasParseError() && !parser.finishedDocument()) {
    const int n = esp_http_client_read(client, buffer.data(), buffer.size());
    if (n > 0) {
      parser.feed(buffer.data(), static_cast<size_t>(n));
    } else if (n == 0) {
      break;
    } else {
      if (n == -ESP_ERR_HTTP_EAGAIN) {
        // A read timeout can leave a valid prefix in the parser. Let finish()
        // classify it as empty or incomplete input using the existing
        // localized deserialization errors.
        break;
      }
      parser.discard();
      return espHttpErrorResult(espHttpReadError(n));
    }
  }

  return parser.finish();
}

ProviderResult OpenMeteoAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  AirQualityResponseParser parser(airQuality);
  uint8_t b;
  while (!parser.hasParseError() && !parser.finishedDocument() && json.readBytes(&b, 1) > 0) {
    parser.feed(reinterpret_cast<const char *>(&b), 1);
  }
  return parser.finish();
}  // OpenMeteoAirQualityProvider::deserializeAirQuality

#endif  // REMOTE_PROVIDER_OPEN_METEO_AIR_QUALITY
