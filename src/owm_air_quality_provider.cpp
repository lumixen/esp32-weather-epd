/* OpenWeatherMap air-quality provider for esp32-weather-epd.
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

#if defined(REMOTE_PROVIDER_OPENWEATHERMAP_AIR_QUALITY)

#include <Arduino.h>
#if defined(OPENWEATHERMAP_AIR_QUALITY_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <cstdint>
#include <cstring>
#include <time.h>
#include "_locale.h"
#include "esp_http_client_stream.h"
#include "esp_http_client_utils.h"
#include "json_stream_utils.h"
#include "owm_air_quality_provider.h"
#include "provider_fetch_operations.h"

/* Perform an HTTP GET request to OpenWeatherMap's "Air Pollution" API and map
 * the response into the generic air quality model.
 */
ProviderResult OpenWeatherMapAirQualityProvider::fetch(air_quality_t &airQuality) {
#if defined(OPENWEATHERMAP_AIR_QUALITY_TRANSPORT_HTTP)
  const char *scheme = "http://";
#else  // OPENWEATHERMAP_AIR_QUALITY_TRANSPORT_HTTPS_*
  const char *scheme = "https://";
#endif
  int64_t end = time(nullptr);
  // minus 1 is important here, otherwise we could get an extra hour of history
  int64_t start = end - ((3600 * NUM_AIR_POLLUTION) - 1);
  char endStr[22];
  char startStr[22];
  sprintf(endStr, "%lld", end);
  sprintf(startStr, "%lld", start);
  String uri = "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON + "&start=" + startStr + "&end=" + endStr +
               "&appid=" + OPENWEATHERMAP_AIR_QUALITY_API_KEY;
  String sanitizedUrl = String(scheme) + OWM_ENDPOINT + "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON +
                        "&start=" + startStr + "&end=" + endStr + "&appid={API key}";
  String url = String(scheme) + OWM_ENDPOINT + uri;

  esp_http_client_config_t config = {};
  config.timeout_ms = HTTP_CLIENT_TCP_TIMEOUT;
#if defined(OPENWEATHERMAP_AIR_QUALITY_TRANSPORT_HTTPS_VERIFY)
  config.cert_pem = cert_USERTrust_RSA_Certification_Authority;
#endif

  return espHttpGetWithRetry(url, sanitizedUrl, config, [&airQuality](esp_http_client_handle_t client) {
    EspHttpClientStream stream(client);
    ProviderResult result = deserializeAirQuality(stream, airQuality);
    if (stream.hadReadError())
      return espHttpErrorResult(stream.readError());
    return result;
  });
}  // OpenWeatherMapAirQualityProvider::fetch

std::vector<std::unique_ptr<FetchOperation>> OpenWeatherMapAirQualityProvider::createFetchOperations(
    weather_report_t &out) {
  std::vector<std::unique_ptr<FetchOperation>> operations;
  operations.push_back(std::make_unique<CallbackFetchOperation>(getApiName(), false, [this, &out]() {
    out.resetAirQuality();
    air_quality_t &airQuality = out.engageAirQuality();
    ProviderResult result = fetch(airQuality);
    if (!result.isOk() || airQuality.dt[0] == 0) {
      out.resetAirQuality();
    }
    return result;
  }));
  return operations;
}

/* SAX event handler: maps an OpenWeatherMap Air Pollution response directly
 * into the generic air quality model as the bytes stream in. The response has
 * the shape `list[index].dt` and
 * `list[index].components.<pollutant>`; all other fields are ignored.
 */
class OWMAirQualityHandler : public JsonHandler {
 public:
  explicit OWMAirQualityHandler(air_quality_t &airQuality) : airQuality_(airQuality) {}

  void startDocument() override { sawStart_ = true; }
  void endDocument() override { documentDone_ = true; }
  void startObject(ElementPath) override {}
  void endObject(ElementPath) override {}
  void startArray(ElementPath) override {}
  void endArray(ElementPath) override {}
  void whitespace(char) override {}

  void value(ElementPath path, ElementValue value) override {
    if (path.getCount() == 3 && keyIs(path.get(0), "list")) {
      ElementSelector *indexSelector = path.get(1);
      ElementSelector *fieldSelector = path.getCurrent();
      if (indexSelector == nullptr || indexSelector->isObject() || fieldSelector == nullptr) {
        return;
      }
      const int index = indexSelector->getIndex();
      if (index < 0 || !keyIs(fieldSelector, "dt") || index >= NUM_AIR_POLLUTION) {
        return;
      }

      if (value.isInt() || value.isFloat()) {
        airQuality_.dt[index] = static_cast<int64_t>(value.getDouble());
      } else if (value.isNull()) {
        // ArduinoJson's .as<int64_t>() converted null to zero.
        airQuality_.dt[index] = 0;
      }
      return;
    }

    if (path.getCount() != 4 || !keyIs(path.get(0), "list") || !keyIs(path.get(2), "components")) {
      return;
    }

    ElementSelector *indexSelector = path.get(1);
    if (indexSelector == nullptr || indexSelector->isObject()) {
      return;
    }
    const int index = indexSelector->getIndex();
    if (index < 0 || index >= NUM_AIR_POLLUTION) {
      return;
    }

    const Column field = column(path.getCurrent());
    if (field == Column::NONE) {
      return;
    }
    if (value.isInt() || value.isFloat()) {
      storeComponent(field, index, static_cast<float>(value.getDouble()));
    } else if (value.isNull()) {
      // ArduinoJson's .as<float>() converted null to zero.
      storeComponent(field, index, 0.0f);
    }
  }

  bool sawStart() const { return sawStart_; }
  bool finishedDocument() const { return documentDone_; }

 private:
  enum class Column : uint8_t { NONE, CO, NO, NO2, O3, SO2, PM2_5, PM10, NH3 };

  static bool keyIs(ElementSelector *selector, const char *key) {
    return selector != nullptr && selector->isObject() && keyIs(selector->getKey(), key);
  }

  static bool keyIs(const char *str, const char *key) { return str != nullptr && strcmp(str, key) == 0; }

  static Column column(ElementSelector *selector) {
    if (selector == nullptr || !selector->isObject()) {
      return Column::NONE;
    }
    const char *key = selector->getKey();
    if (keyIs(key, "co"))
      return Column::CO;
    if (keyIs(key, "no"))
      return Column::NO;
    if (keyIs(key, "no2"))
      return Column::NO2;
    if (keyIs(key, "o3"))
      return Column::O3;
    if (keyIs(key, "so2"))
      return Column::SO2;
    if (keyIs(key, "pm2_5"))
      return Column::PM2_5;
    if (keyIs(key, "pm10"))
      return Column::PM10;
    if (keyIs(key, "nh3"))
      return Column::NH3;
    return Column::NONE;
  }

  void storeComponent(Column field, int index, float value) {
    switch (field) {
      case Column::CO:
        airQuality_.components.co[index] = value;
        break;
      case Column::NO:
        airQuality_.components.no[index] = value;
        break;
      case Column::NO2:
        airQuality_.components.no2[index] = value;
        break;
      case Column::O3:
        airQuality_.components.o3[index] = value;
        break;
      case Column::SO2:
        airQuality_.components.so2[index] = value;
        break;
      case Column::PM2_5:
        airQuality_.components.pm2_5[index] = value;
        break;
      case Column::PM10:
        airQuality_.components.pm10[index] = value;
        break;
      case Column::NH3:
        airQuality_.components.nh3[index] = value;
        break;
      case Column::NONE:
        break;
    }
  }

  air_quality_t &airQuality_;
  bool sawStart_ = false;
  bool documentDone_ = false;
};

ProviderResult OpenWeatherMapAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  /* The destination is long-lived in the application. Clear it before every
   * parse so short or malformed responses cannot retain stale readings. */
  memset(&airQuality, 0, sizeof(air_quality_t));

  OWMAirQualityHandler handler(airQuality);
  ProviderResult result = consumeJsonStream(
      json, handler, [&handler]() { return handler.finishedDocument(); }, [&handler]() { return handler.sawStart(); },
      "OpenWeatherMap air quality", true);
  if (!result.isOk())
    memset(&airQuality, 0, sizeof(air_quality_t));
  return result;
}  // OpenWeatherMapAirQualityProvider::deserializeAirQuality

#endif  // REMOTE_PROVIDER_OPENWEATHERMAP_AIR_QUALITY
