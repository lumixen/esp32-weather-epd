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
#include <HTTPClient.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>

// additional libraries
#include <ArduinoJson.h>

// header files
#include "_locale.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"

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

#ifdef WIFI_STATIC_IP_ENABLED
  // Configure static IP before connecting
  IPAddress local_IP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress primaryDNS;
  IPAddress secondaryDNS;

  local_IP.fromString(WIFI_STATIC_IP_IP);
  gateway.fromString(WIFI_STATIC_IP_GATEWAY);
  subnet.fromString(WIFI_STATIC_IP_SUBNET);

  bool ok;
  if (strlen(WIFI_STATIC_IP_DNS1) > 0 && strlen(WIFI_STATIC_IP_DNS2) > 0) {
    primaryDNS.fromString(WIFI_STATIC_IP_DNS1);
    secondaryDNS.fromString(WIFI_STATIC_IP_DNS2);
    ok = WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  } else if (strlen(WIFI_STATIC_IP_DNS1) > 0) {
    primaryDNS.fromString(WIFI_STATIC_IP_DNS1);
    ok = WiFi.config(local_IP, gateway, subnet, primaryDNS);
  } else {
    ok = WiFi.config(local_IP, gateway, subnet);
  }
  if (!ok) {
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
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 0, bestBSSID);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
#else
#ifdef WIFI_HAS_BSSID
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 0, WIFI_BSSID);
#else
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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

/* Perform an HTTP GET request with retry.
 * The `parse` callback is invoked with the response stream to deserialize
 * and map the provider response into the output model.
 *
 * Returns the HTTP Status Code, or a negative error code. The -512 offset
 * distinguishes WiFi errors from httpClient errors and the -256 offset
 * distinguishes JSON deserialization errors from httpClient errors.
 */
int httpGetWithRetry(WiFiClient &client, const String &host, uint16_t port, const String &uri,
                     const String &sanitizedUri, bool useHttp10, uint32_t timeoutMs,
                     std::function<DeserializationError(Stream &, size_t)> parse) {
  int attempts = 0;
  bool rxSuccess = false;

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
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);
    if (useHttp10) {
      http.useHTTP10(true);
    }
    http.begin(client, host, port, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      // Pass the response content length so parsers can stop reading exactly
      // at the end of the body instead of reading past it into the (already
      // closed) connection.
      const int size = http.getSize();
      const size_t expectedLen = size > 0 ? static_cast<size_t>(size) : 0;
      // Make the parser's per-byte read window match the configured timeout:
      // HTTPClient::setTimeout only forwards to the client while connected,
      // so the Stream the parser reads from would otherwise keep its default
      // 1 s window (too short for large, intermittently delivered bodies).
      http.getStream().setTimeout(timeoutMs);
      DeserializationError jsonErr = parse(http.getStream(), expectedLen);
      if (jsonErr) {
        // -256 offset distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
        Serial.println("  stream: read timeout " + String(http.getStream().getTimeout()) + " ms, advertised size " +
                       String(size) + " B, http.connected()=" + String(http.connected()) +
                       ", live stream ptr=" + String(http.getStreamPtr() != nullptr));
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " + getHttpResponsePhrase(httpResponse));
    ++attempts;
    if (!rxSuccess) {
      delay(100);
    }
  }

  return httpResponse;
}  // httpGetWithRetry

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  Serial.println("[debug] Heap Size       : " + String(ESP.getHeapSize()) + " B");
  Serial.println("[debug] Available Heap  : " + String(ESP.getFreeHeap()) + " B");
  Serial.println("[debug] Min Free Heap   : " + String(ESP.getMinFreeHeap()) + " B");
  Serial.println("[debug] Max Allocatable : " + String(ESP.getMaxAllocHeap()) + " B");
  return;
}