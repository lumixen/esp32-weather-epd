#include "config.h"
#include "logger.h"

#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP) && !defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#if !defined(ALERTS_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(ALERTS_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include "_locale.h"
#include "client_utils.h"
#include "owm_alert_provider.h"

// OpenWeatherMaps does not specify a limit, but if you need more alerts you
// are probably doomed.
#define OWM_NUM_ALERTS 8

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API requesting
 * only alerts, and map the response into the generic alert model.
 *
 * Returns the HTTP Status Code.
 */
int OWMAlertProvider::fetch(std::vector<weather_alert_t> &alerts) {
#if defined(ALERTS_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(ALERTS_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // ALERTS_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
  const uint16_t port = 443;
#endif
  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=metric&exclude=current,minutely,hourly,daily";

  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";

  uri += "&appid=" + OWM_APIKEY;

  return httpGetWithRetry(client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
                          [&alerts](Stream &json, size_t) { return deserializeAlerts(json, alerts); });
}  // OWMAlertProvider::fetch

DeserializationError OWMAlertProvider::deserializeAlerts(Stream &json, std::vector<weather_alert_t> &alerts) {
  int i = 0;

  JsonDocument filter;
  // description can be very long so they are filtered out to save on memory
  // along with sender_name
  for (int i = 0; i < OWM_NUM_ALERTS; ++i) {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"] = true;
    filter["alerts"][i]["start"] = true;
    filter["alerts"][i]["end"] = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"] = true;
  }

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  LOG_DEBUG("doc.overflowed() : %s", doc.overflowed() ? "true" : "false");
  if (LogLevel::TRACE >= g_logLevel) {
    LOG_TRACE("pretty JSON dump:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  if (error) {
    return error;
  }

  for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
    weather_alert_t new_alert = {};
    // new_alert.sender_name = alert["sender_name"].as<const char *>();
    new_alert.event = alert["event"].as<const char *>();
    new_alert.start = alert["start"].as<int64_t>();
    new_alert.end = alert["end"].as<int64_t>();
    // new_alert.description = alert["description"].as<const char *>();
    new_alert.tags = alert["tags"][0].as<const char *>();
    alerts.push_back(new_alert);

    if (i == OWM_NUM_ALERTS - 1) {
      break;
    }
    ++i;
  }

  return error;
}  // OWMAlertProvider::deserializeAlerts

#endif  // ALERTS_API_PROVIDER_OPEN_WEATHER_MAP && !WEATHER_API_PROVIDER_OPEN_WEATHER_MAP
