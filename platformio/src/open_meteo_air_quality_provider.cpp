#include "config.h"

#if defined(AIR_QUALITY_API_OPEN_METEO)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <time.h>
#include "client_utils.h"
#include "open_meteo_air_quality_provider.h"

/* Perform an HTTP GET request to Open-Meteo's air quality API and map the
 * response into the generic air quality model.
 *
 * Returns the HTTP Status Code.
 */
int OpenMeteoAirQualityProvider::fetch(WiFiClient &client, air_quality_t &airQuality) {
  String uri = "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON +
               "&hourly=pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ammonia,nitrogen_monoxide,ozone,pm10&"
               "past_days=1&forecast_days=1&timeformat=unixtime";
  String sanitizedUri = OM_AIR_QUALITY_ENDPOINT + uri;

  return httpGetWithRetry(client, OM_AIR_QUALITY_ENDPOINT, uri, sanitizedUri, true,
                          [&airQuality](Stream &json) { return deserializeAirQuality(json, airQuality); });
}  // OpenMeteoAirQualityProvider::fetch

DeserializationError OpenMeteoAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
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
  return error;
}  // OpenMeteoAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_OPEN_METEO
