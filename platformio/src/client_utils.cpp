/* Client side utilities for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// built-in C++ libraries
#include <cstring>
#include <vector>

// arduino/esp32 libraries
#include <Arduino.h>
#include <esp_sntp.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>

// additional libraries
#include <Adafruit_BusIO_Register.h>
#include <ArduinoJson.h>

// header files
#include "_locale.h"
#include "api_response.h"
#include "aqi.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "renderer.h"
#include "home_assistant_mqtt.h"

#ifdef API_PROTOCOL_HTTP
static const uint16_t PORT = 80;
#else
static const uint16_t PORT = 443;
#endif

/* Power-on and connect WiFi.
 * Takes int parameter to store WiFi RSSI, or “Received Signal Strength
 * Indicator"
 *
 * Returns WiFi status.
 */
wl_status_t startWiFi(int8_t &wifiRSSI) {
  // Set hostname with MAC address suffix
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  macAddress.toLowerCase();
  String macSuffix = macAddress.substring(macAddress.length() - 6);
  String hostname = "esp32_weather_display_" + macSuffix;
  WiFi.setHostname(hostname.c_str());

  WiFi.mode(WIFI_STA);
  Serial.printf("%s '%s'", TXT_CONNECTING_TO, WIFI_SSID);

#ifdef D_WIFI_STATIC_IP_IP
  // Configure static IP before connecting
  IPAddress local_IP;
  IPAddress gateway;
  IPAddress subnet;
#ifdef D_WIFI_STATIC_IP_DNS1
  IPAddress primaryDNS;
#ifdef D_WIFI_STATIC_IP_DNS2
  IPAddress secondaryDNS;
#endif
#endif

  local_IP.fromString(D_WIFI_STATIC_IP_IP);
  gateway.fromString(D_WIFI_STATIC_IP_GATEWAY);
  subnet.fromString(D_WIFI_STATIC_IP_SUBNET);

#ifdef D_WIFI_STATIC_IP_DNS1
  primaryDNS.fromString(D_WIFI_STATIC_IP_DNS1);
#ifdef D_WIFI_STATIC_IP_DNS2
  secondaryDNS.fromString(D_WIFI_STATIC_IP_DNS2);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
#else
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
#endif
#else
  if (!WiFi.config(local_IP, gateway, subnet)) {
#endif
    Serial.println("Failed to configure static IP");
  } else {
    Serial.printf("Static IP configured: %s\n", local_IP.toString().c_str());
  }
#endif

#if WIFI_SCAN
  // Scan for networks, if there are multiple with the same SSID, connect to the one
  // with the best RSSI.
  Serial.print("\nScanning for WiFi networks...");
  int numNetworks = WiFi.scanNetworks();
  int bestRSSI = -100;
  uint8_t bestBSSID[6];
  bool foundNetwork = false;

  for (int i = 0; i < numNetworks; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      if (WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        memcpy(bestBSSID, WiFi.BSSID(i), 6);
        Serial.printf("\n  Found SSID '%s', BSSID %02X:%02X:%02X:%02X:%02X:%02X with RSSI %d dBm", WIFI_SSID,
                      bestBSSID[0], bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], WiFi.RSSI(i));
        foundNetwork = true;
      }
    }
  }
  if (foundNetwork) {
    WiFi.begin(WIFI_SSID, D_WIFI_PASSWORD, 0, bestBSSID);
  } else {
    WiFi.begin(WIFI_SSID, D_WIFI_PASSWORD);
  }
#else
#ifdef WIFI_BSSID
  const uint8_t bssid[] = WIFI_BSSID;
  WiFi.begin(WIFI_SSID, D_WIFI_PASSWORD, 0, bssid);
#else
  WiFi.begin(WIFI_SSID, D_WIFI_PASSWORD);
#endif
#endif

  // timeout if WiFi does not connect in WIFI_TIMEOUT ms from now
  unsigned long timeout = millis() + WIFI_TIMEOUT;
  wl_status_t connection_status = WiFi.status();

  while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
    Serial.print(".");
    delay(50);
    connection_status = WiFi.status();
  }
  Serial.println();

  if (connection_status == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI();  // get WiFi signal strength now, because the WiFi
                             // will be turned off to save power!
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.printf("%s '%s'\n", TXT_COULD_NOT_CONNECT_TO, WIFI_SSID);
  }
  return connection_status;
}  // startWiFi

/* Disconnect and power-off WiFi.
 */
void killWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}  // killWiFi

/* Prints the local time to serial monitor.
 *
 * Returns true if getting local time was a success, otherwise false.
 */
bool printLocalTime(tm *timeInfo) {
  int attempts = 0;
  while (!getLocalTime(timeInfo) && attempts++ < 3) {
    Serial.println(TXT_FAILED_TO_GET_TIME);
    return false;
  }
  Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S");
  return true;
}  // printLocalTime

/* Waits for NTP server time sync, adjusted for the time zone specified in
 * config.cpp.
 *
 * Returns true if time was set successfully, otherwise false.
 *
 * Note: Must be connected to WiFi to get time from NTP server.
 */
bool waitForSNTPSync(tm *timeInfo) {
  // Wait for SNTP synchronization to complete
  unsigned long timeout = millis() + NTP_TIMEOUT;
  if ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) && (millis() < timeout)) {
    Serial.print(TXT_WAITING_FOR_SNTP);
    delay(100);  // ms
    while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) && (millis() < timeout)) {
      Serial.print(".");
      delay(100);  // ms
    }
    Serial.println();
  }
  return printLocalTime(timeInfo);
}  // waitForSNTPSync

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API
 * If data is received, it will be parsed and stored in the global variable
 * owm_onecall.
 *
 * Returns the HTTP Status Code.
 */
int getOWMonecall(WiFiClient &client, environment_data_t &r) {
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};
  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=metric&exclude=minutely";
#if !DISPLAY_ALERTS
  // exclude alerts
  uri += ",alerts";
#endif

  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";

  uri += "&appid=" + OWM_APIKEY;

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + sanitizedUri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3) {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED) {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);  // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);         // default 5000ms
    http.begin(client, OWM_ENDPOINT, PORT, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      jsonErr = deserializeOneCall(http.getStream(), r);
      if (jsonErr) {
        // -256 offset distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " + getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
}  // getOWMonecall

/* Perform an HTTP GET request to OpenWeatherMap's "Air Pollution" API
 * If data is received, it will be parsed and stored in the global variable
 * air_pollution.
 *
 * Returns the HTTP Status Code.
 */
int getAirPollution(WiFiClient &client, air_pollution_t &r) {
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

#ifdef AIR_QUALITY_API_OPEN_WEATHER_MAP
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
  String host = OWM_ENDPOINT;
#endif
#ifdef AIR_QUALITY_API_OPEN_METEO
  String uri = "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON +
               "&hourly=pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ammonia,nitrogen_monoxide,ozone,pm10&"
               "past_days=1&forecast_days=1&timeformat=unixtime";
  String sanitizedUri = OM_AIR_QUALITY_ENDPOINT + uri;
  String host = OM_AIR_QUALITY_ENDPOINT;
#endif

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + sanitizedUri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3) {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED) {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);  // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);         // default 5000ms
#ifdef AIR_QUALITY_API_OPEN_METEO
    http.useHTTP10(true);
#endif
    http.begin(client, host, PORT, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
#ifdef AIR_QUALITY_API_OPEN_WEATHER_MAP
      jsonErr = deserializeOWMAirQuality(http.getStream(), r);
#endif
#ifdef AIR_QUALITY_API_OPEN_METEO
      jsonErr = deserializeOpenMeteoAirQuality(http.getStream(), r);
#endif
      if (jsonErr) {
        // -256 offset to distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " + getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
}  // getAirPollution

/* Perform an HTTP GET request to OpenMeteo's API
 * If data is received, it will be parsed and stored in the global environment_data variable
 *
 * Returns the HTTP Status Code.
 */
int getOMCall(WiFiClient &client, environment_data_t &r) {
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

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

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + sanitizedUri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3) {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED) {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);  // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);         // default 5000ms
    http.useHTTP10(true);
    http.begin(client, OM_ENDPOINT, PORT, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      jsonErr = deserializeOpenMeteoCall(http.getStream(), r);  // Convert String to const char*
      if (jsonErr) {
        // -256 offset distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " + getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
}  // getOMcall

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  Serial.println("[debug] Heap Size       : " + String(ESP.getHeapSize()) + " B");
  Serial.println("[debug] Available Heap  : " + String(ESP.getFreeHeap()) + " B");
  Serial.println("[debug] Min Free Heap   : " + String(ESP.getMinFreeHeap()) + " B");
  Serial.println("[debug] Max Allocatable : " + String(ESP.getMaxAllocHeap()) + " B");
  return;
}

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED

// Helper function to publish discovery and state for mqtt sensors.
// Returns true if both publishes were successful, otherwise false.
bool publishMQTTSensor(PubSubClient &mqtt, const char *sensorName, const String &discoveryTopic,
                       const String &discoveryPayload, const String &stateTopic, const char *stateValue) {
  if (mqtt.publish(discoveryTopic.c_str(), discoveryPayload.c_str(), true)) {
    if (mqtt.publish(stateTopic.c_str(), stateValue, true)) {
      Serial.printf("  Published %s\n", sensorName);
      return true;
    } else {
      Serial.printf("  Warning: Failed to publish %s state\n", sensorName);
      return false;
    }
  } else {
    Serial.printf("  Warning: Failed to publish %s discovery\n", sensorName);
    return false;
  }
}

void sendMQTTStatus(uint32_t batteryVoltage, uint8_t batteryPercentage, int8_t wifiRSSI,
                    unsigned long apiActivityDuration) {
  WiFiClient mqttWifi;
  PubSubClient mqtt(mqttWifi);
  mqtt.setBufferSize(512);
  mqtt.setServer(D_HOME_ASSISTANT_MQTT_SERVER, HOME_ASSISTANT_MQTT_PORT);
  Serial.println("Connecting to MQTT...");
  bool connected =
      mqtt.connect(D_HOME_ASSISTANT_MQTT_CLIENT_ID, D_HOME_ASSISTANT_MQTT_USERNAME, D_HOME_ASSISTANT_MQTT_PASSWORD);
  if (connected) {
    Serial.println("MQTT connected. Now publishing discovery and status.");

#if BATTERY_MONITORING
    // 1. Publish Battery Voltage
    char voltageStr[8];
    snprintf(voltageStr, sizeof(voltageStr), "%.3f", batteryVoltage / 1000.0);
    publishMQTTSensor(mqtt, "Battery voltage", FPSTR(HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC),
                      FPSTR(HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_PAYLOAD),
                      FPSTR(HOME_ASSISTANT_MQTT_STATE_TOPIC_VOLTAGE), voltageStr);

    // 2. Publish Battery Percent
    char percentStr[4];
    snprintf(percentStr, sizeof(percentStr), "%u", batteryPercentage);
    publishMQTTSensor(mqtt, "Battery percent", FPSTR(HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC),
                      FPSTR(HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD),
                      FPSTR(HOME_ASSISTANT_MQTT_STATE_TOPIC_PERCENT), percentStr);
#endif

    // 3. Publish WiFi RSSI
    char rssiStr[5];
    snprintf(rssiStr, sizeof(rssiStr), "%d", wifiRSSI);
    publishMQTTSensor(mqtt, "WiFi RSSI", FPSTR(HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC),
                      FPSTR(HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD), FPSTR(HOME_ASSISTANT_MQTT_STATE_TOPIC_RSSI),
                      rssiStr);

    // 4. Publish API Activity Duration
    if (apiActivityDuration > 0) {
      char durationStr[12];
      snprintf(durationStr, sizeof(durationStr), "%lu", apiActivityDuration);
      publishMQTTSensor(mqtt, "API activity duration", FPSTR(HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_TOPIC),
                        FPSTR(HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_PAYLOAD),
                        FPSTR(HOME_ASSISTANT_MQTT_STATE_TOPIC_API_ACTIVITY_DURATION), durationStr);
    }
    mqtt.disconnect();
    delay(300);  // give the MQTT client time to send the outgoing packets before powering off WiFi
    Serial.println("MQTT publish complete.");
  } else {
    Serial.println(mqtt.state());
    Serial.println("MQTT connection failed.");
  }
}
#endif  // HOME_ASSISTANT_MQTT_ENABLED