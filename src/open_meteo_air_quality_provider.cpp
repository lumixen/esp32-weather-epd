#include "config.h"
#include "logger.h"

#if defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)

#include <Arduino.h>
#include <WiFiClient.h>
#if !defined(AIR_QUALITY_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <ArduinoStreamParser.h>
#include <cstdint>
#include <cstring>
#include <time.h>
#include "_locale.h"
#include "client_utils.h"
#include "open_meteo_air_quality_provider.h"

/* Perform an HTTP GET request to Open-Meteo's air quality API and map the
 * response into the generic air quality model.
 */
ProviderResult OpenMeteoAirQualityProvider::fetch(air_quality_t &airQuality) {
#if defined(AIR_QUALITY_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(AIR_QUALITY_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_ISRG_Root_X1);
  const uint16_t port = 443;
#endif
  String uri = "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON +
               "&hourly=pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ammonia,nitrogen_monoxide,ozone,pm10&"
               "past_days=1&forecast_days=1&timeformat=unixtime";
  String sanitizedUri = OM_AIR_QUALITY_ENDPOINT + uri;

  return httpGetWithRetry(client, OM_AIR_QUALITY_ENDPOINT, port, uri, sanitizedUri, true, HTTP_CLIENT_TCP_TIMEOUT,
                          [&airQuality](Stream &json, size_t) { return deserializeAirQuality(json, airQuality); });
}  // OpenMeteoAirQualityProvider::fetch

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

ProviderResult OpenMeteoAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  // The model may be shared with the previous fetch. Clear it first so a
  // short, malformed, or otherwise incomplete response cannot leave stale
  // readings behind.
  memset(&airQuality, 0, sizeof(air_quality_t));
  AirQualityHandler handler(airQuality);
  ArduinoStreamParser parser;
  parser.setHandler(&handler);

  // Read one byte at a time and stop as soon as the root document closes.
  // This avoids waiting for a trailing read after a close-delimited HTTP/1.0
  // response and matches the streaming pump used by the weather provider.
  uint8_t b;
  while (!parser.hasParseError() && !handler.finishedDocument() && json.readBytes(&b, 1) > 0) {
    parser.write(&b, 1);
  }

  if (parser.hasParseError()) {
    LOG_WARNING("Open-Meteo air quality JSON parse error: %s", parser.getErrorMessage());
    memset(&airQuality, 0, sizeof(air_quality_t));
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  if (handler.finishedDocument()) {
    // A syntactically valid response without hourly data is still accepted,
    // matching the old DOM parser's behavior for API error payloads and {}.
    return ProviderResult::ok();
  }

  memset(&airQuality, 0, sizeof(air_quality_t));
  if (!handler.sawStart()) {
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
  }
  return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
}  // OpenMeteoAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_PROVIDER_OPEN_METEO
