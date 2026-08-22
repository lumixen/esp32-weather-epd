#include "config.h"
#include "logger.h"

#if defined(AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include <WiFiClient.h>
#if !defined(AIR_QUALITY_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <time.h>
#include "_locale.h"
#include "client_utils.h"
#include "provider_result_utils.h"
#include "owm_air_quality_provider.h"

/* Perform an HTTP GET request to OpenWeatherMap's "Air Pollution" API and map
 * the response into the generic air quality model.
 */
ProviderResult OWMAirQualityProvider::fetch(air_quality_t &airQuality) {
#if defined(AIR_QUALITY_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(AIR_QUALITY_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
  const uint16_t port = 443;
#endif
  int64_t end = time(nullptr);
  // minus 1 is important here, otherwise we could get an extra hour of history
  int64_t start = end - ((3600 * NUM_AIR_POLLUTION) - 1);
  char endStr[22];
  char startStr[22];
  sprintf(endStr, "%lld", end);
  sprintf(startStr, "%lld", start);
  String uri = "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON + "&start=" + startStr + "&end=" + endStr +
               "&appid=" + OWM_APIKEY;
  String sanitizedUri = OWM_ENDPOINT + "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON +
                        "&start=" + startStr + "&end=" + endStr + "&appid={API key}";

  return httpGetWithRetry(client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
                          [&airQuality](Stream &json, size_t) { return deserializeAirQuality(json, airQuality); });
}  // OWMAirQualityProvider::fetch

ProviderResult OWMAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  /* The destination is long-lived in the application. Clear it before every
   * parse so short or malformed responses cannot retain stale readings. */
  memset(&airQuality, 0, sizeof(air_quality_t));

  int i = 0;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
  LOG_DEBUG("doc.overflowed() : %s", doc.overflowed() ? "true" : "false");
  if (LogLevel::TRACE >= g_logLevel) {
    LOG_TRACE("pretty JSON dump:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  if (error) {
    return mapDeserializationError(error);
  }

  for (JsonObject list : doc["list"].as<JsonArray>()) {
    JsonObject list_components = list["components"];
    airQuality.components.co[i] = list_components["co"].as<float>();
    airQuality.components.no[i] = list_components["no"].as<float>();
    airQuality.components.no2[i] = list_components["no2"].as<float>();
    airQuality.components.o3[i] = list_components["o3"].as<float>();
    airQuality.components.so2[i] = list_components["so2"].as<float>();
    airQuality.components.pm2_5[i] = list_components["pm2_5"].as<float>();
    airQuality.components.pm10[i] = list_components["pm10"].as<float>();
    airQuality.components.nh3[i] = list_components["nh3"].as<float>();

    airQuality.dt[i] = list["dt"].as<int64_t>();

    if (i == NUM_AIR_POLLUTION - 1) {
      break;
    }
    ++i;
  }

  return mapDeserializationError(error);
}  // OWMAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP
