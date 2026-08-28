/* Client side utilities for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
 * Copyright (C) 2026  Max Bodaniuk
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

// arduino/esp32 libraries
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

// additional libraries

// header files
#include "_locale.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "logger.h"

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
  LOG_INFO("%s '%s'", TXT_CONNECTING_TO, WIFI_SSID);

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
    LOG_WARNING("Failed to configure static IP");
  } else {
    LOG_INFO("Static IP configured: %s", local_IP.toString().c_str());
  }
#endif

#if WIFI_SCAN
  // Scan for networks, if there are multiple with the same SSID, connect to the one
  // with the best RSSI.
  LOG_INFO("Scanning for WiFi networks...");
  int numNetworks = WiFi.scanNetworks();
  int bestRSSI = -100;
  uint8_t bestBSSID[6];
  bool foundNetwork = false;

  for (int i = 0; i < numNetworks; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      if (WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        memcpy(bestBSSID, WiFi.BSSID(i), 6);
        LOG_DEBUG("Found SSID '%s', BSSID %02X:%02X:%02X:%02X:%02X:%02X with RSSI %d dBm", WIFI_SSID, bestBSSID[0],
                  bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], WiFi.RSSI(i));
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

  LOG_DEBUG("Waiting for WiFi connection (timeout %d ms)", WIFI_TIMEOUT);
  while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
    delay(50);
    connection_status = WiFi.status();
  }

  if (connection_status == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI();  // get WiFi signal strength now, because the WiFi
                             // will be turned off to save power!
    LOG_INFO("IP: %s", WiFi.localIP().toString().c_str());
  } else {
    LOG_WARNING("%s '%s'", TXT_COULD_NOT_CONNECT_TO, WIFI_SSID);
  }
  return connection_status;
}  // startWiFi

/* Disconnect and power-off WiFi.
 */
void killWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}  // killWiFi

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  LOG_DEBUG("Heap Size: %u B, Available: %u B, Min Free: %u B, Max Allocatable: %u B", ESP.getHeapSize(),
            ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}