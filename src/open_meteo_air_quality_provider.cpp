#include "config.h"
#include "logger.h"

#if defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)

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
#include "_locale.h"
#include "client_utils.h"
#include "provider_result_utils.h"
#include "open_meteo_air_quality_provider.h"

/* Perform an HTTP GET request to Open-Meteo's air quality API and map the
 * response into the generic air quality model.
 *
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

ProviderResult OpenMeteoAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
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

  JsonObject hourly = doc["hourly"];
  int count = hourly["time"].size();

  // Find index of closest timestamp below 'now'
  int closest_idx = -1;
  int64_t now = time(nullptr);
  for (int i = 0; i < count; i++) {
    int64_t ts = hourly["time"][i].as<int64_t>();
    if (ts <= now) {
      closest_idx = i;
    } else {
      break;
    }
  }

  // Pick 24 entries: closest below and 23 previous
  int start_idx = closest_idx - NUM_AIR_POLLUTION + 1;
  if (start_idx < 0)
    start_idx = 0;
  int actual_count = closest_idx - start_idx + 1;
  if (actual_count > NUM_AIR_POLLUTION)
    actual_count = NUM_AIR_POLLUTION;
  for (int i = 0; i < actual_count; i++) {
    int idx = start_idx + i;
    airQuality.dt[i] = hourly["time"][idx].as<int64_t>();
    airQuality.components.pm2_5[i] = hourly["pm2_5"][idx].as<float>();
    airQuality.components.pm10[i] = hourly["pm10"][idx].as<float>();
    airQuality.components.co[i] = hourly["carbon_monoxide"][idx].as<float>();
    airQuality.components.no[i] = hourly["nitrogen_monoxide"][idx].as<float>();
    airQuality.components.no2[i] = hourly["nitrogen_dioxide"][idx].as<float>();
    airQuality.components.o3[i] = hourly["ozone"][idx].as<float>();
    airQuality.components.so2[i] = hourly["sulphur_dioxide"][idx].as<float>();
    airQuality.components.nh3[i] = hourly["ammonia"][idx].as<float>();
  }
  return mapDeserializationError(error);
}  // OpenMeteoAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_PROVIDER_OPEN_METEO
