#include "config.h"

#if defined(AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#if !defined(AIR_QUALITY_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <time.h>
#include "client_utils.h"
#include "owm_air_quality_provider.h"

/* Perform an HTTP GET request to OpenWeatherMap's "Air Pollution" API and map
 * the response into the generic air quality model.
 *
 * Returns the HTTP Status Code.
 */
int OWMAirQualityProvider::fetch(air_quality_t &airQuality) {
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

  return httpGetWithRetry(client, OWM_ENDPOINT, port, uri, sanitizedUri, false,
                          [&airQuality](Stream &json, size_t) { return deserializeAirQuality(json, airQuality); });
}  // OWMAirQualityProvider::fetch

DeserializationError OWMAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  int i = 0;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
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

  return error;
}  // OWMAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_PROVIDER_OPEN_WEATHER_MAP
