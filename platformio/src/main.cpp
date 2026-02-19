/* Main program for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
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

#include "config.h"
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>
#if HOME_ASSISTANT_MQTT_ENABLED
#include <PubSubClient.h>
#include <pgmspace.h>
#endif

#include "_locale.h"
#include "api_response.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "icons/icons_196x196.h"
#include "renderer.h"
#include "moon_tools.h"

#ifndef API_PROTOCOL_HTTP
#include <WiFiClientSecure.h>
#endif
#ifdef API_PROTOCOL_HTTPS_VERIFY
#include "cert.h"
#endif

// too large to allocate locally on stack
static environment_data_t environment_data;
static air_pollution_t air_pollution;

Preferences prefs;

// RTC_DATA_ATTR variables survive deep sleep resets, but not power cycles.
// They are used to store data that must persist across deep sleep cycles, such as the wake-up counter.
RTC_DATA_ATTR uint32_t wakeUpCounter = 0;

/* Toggle the built-in LED on or off. */
void toggleBuiltinLED(bool state) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, state ? LOW : HIGH);  // Lolin D32 LED is active low
  return;
}

/* Put esp32 into ultra low-power deep sleep (<11μA).
 * Aligns wake time to the minute. Sleep times defined in config.cpp.
 */
void beginDeepSleep(unsigned long startTime, tm *timeInfo) {
  if (!getLocalTime(timeInfo)) {
    Serial.println(TXT_REFERENCING_OLDER_TIME_NOTICE);
  }

  // To simplify sleep time calculations, the current time stored by timeInfo
  // will be converted to time relative to the WAKE_TIME. This way if a
  // SLEEP_DURATION is not a multiple of 60 minutes it can be more trivially,
  // aligned and it can easily be deterimined whether we must sleep for
  // additional time due to bedtime.
  // i.e. when curHour == 0, then timeInfo->tm_hour == WAKE_TIME
  int bedtimeHour = INT_MAX;
  if (BED_TIME != WAKE_TIME) {
    bedtimeHour = (BED_TIME - WAKE_TIME + 24) % 24;
  }

  // time is relative to wake time
  int curHour = (timeInfo->tm_hour - WAKE_TIME + 24) % 24;
  const int curMinute = curHour * 60 + timeInfo->tm_min;
  const int curSecond = curHour * 3600 + timeInfo->tm_min * 60 + timeInfo->tm_sec;
  const int desiredSleepSeconds = SLEEP_DURATION * 60;
  const int offsetMinutes = curMinute % SLEEP_DURATION;
  const int offsetSeconds = curSecond % desiredSleepSeconds;

  // align wake time to nearest multiple of SLEEP_DURATION
  int sleepMinutes = SLEEP_DURATION - offsetMinutes;
  if (desiredSleepSeconds - offsetSeconds < 120 ||
      offsetSeconds / (float) desiredSleepSeconds >
          0.95f) {  // if we have a sleep time less than 2 minutes OR less 5% SLEEP_DURATION,
    // skip to next alignment
    sleepMinutes += SLEEP_DURATION;
  }

  // estimated wake time, if this falls in a sleep period then sleepDuration
  // must be adjusted
  const int predictedWakeHour = ((curMinute + sleepMinutes) / 60) % 24;

  uint64_t sleepDuration;
  if (predictedWakeHour < bedtimeHour) {
    sleepDuration = sleepMinutes * 60 - timeInfo->tm_sec;
  } else {
    const int hoursUntilWake = 24 - curHour;
    sleepDuration = hoursUntilWake * 3600ULL - (timeInfo->tm_min * 60ULL + timeInfo->tm_sec);
  }

  // add extra delay to compensate for esp32's with fast RTCs.
  sleepDuration += 3ULL;
  sleepDuration *= 1.0015f;

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  toggleBuiltinLED(false);

  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  Serial.print(TXT_AWAKE_FOR);
  Serial.println(" " + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
  Serial.println(" " + String(sleepDuration) + "s");
  esp_deep_sleep_start();
}  // end beginDeepSleep

void enrichWithMoonData(environment_data_t &data) {
  moon_state_t moonState = getMoonState(LAT.toDouble(), LON.toDouble());
  data.daily[0].moonrise = moonState.moonrise;
  data.daily[0].moonset = moonState.moonset;
  data.daily[0].moon_phase = moonState.phase;
}  // end enrichWithMoonData

void handleNetworkError(const unsigned char *icon, const String &statusStr, const String &tmpStr,
                        unsigned long startTime, tm *timeInfo, uint32_t batteryVoltage, uint8_t batteryPercent,
                        int8_t wifiRSSI, unsigned long networkStartTime) {
#if HOME_ASSISTANT_MQTT_ENABLED
  if (WiFi.status() == WL_CONNECTED) {
    sendMQTTStatus(batteryVoltage, batteryPercent, wifiRSSI, millis() - networkStartTime);
  }
#endif

  killWiFi();
  initDisplay();
  do {
    drawError(icon, statusStr, tmpStr);
  } while (display.nextPage());
  powerOffDisplay();
  beginDeepSleep(startTime, timeInfo);
}

/* Program entry point.
 */
void setup() {
  unsigned long startTime = millis();
  Serial.begin(115200);
  toggleBuiltinLED(true);

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  // Open namespace for read/write to non-volatile storage
  prefs.begin(NVS_NAMESPACE, false);

#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
  uint8_t batteryPercent = calcBatPercent(batteryVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
  Serial.print(TXT_BATTERY_VOLTAGE);
  Serial.println(": " + String(batteryVoltage) + "mv");

  // When the battery is low, the display should be updated to reflect that, but
  // only the first time we detect low voltage. The next time the display will
  // refresh is when voltage is no longer low. To keep track of that we will
  // make use of non-volatile storage.
  bool lowBat = prefs.getBool("lowBat", false);

  // low battery, deep sleep now
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    if (lowBat == false) {  // battery is now low for the first time
      prefs.putBool("lowBat", true);
      prefs.end();
      initDisplay();
      do {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
    }

    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE) {  // critically low battery
      // don't set esp_sleep_enable_timer_wakeup();
      // We won't wake up again until someone manually presses the RST button.
      Serial.println(TXT_CRIT_LOW_BATTERY_VOLTAGE);
      Serial.println(TXT_HIBERNATING_INDEFINITELY_NOTICE);
    } else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE) {  // very low battery
      esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
      Serial.println(TXT_VERY_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(VERY_LOW_BATTERY_SLEEP_INTERVAL) + "min");
    } else {  // low battery
      esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
      Serial.println(TXT_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    esp_deep_sleep_start();
  }
  // battery is no longer low, reset variable in non-volatile storage
  if (lowBat == true) {
    prefs.putBool("lowBat", false);
  }
#else
  uint32_t batteryVoltage = UINT32_MAX;
  uint8_t batteryPercent = UINT8_MAX;
#endif

  // All data should have been loaded from NVS. Close filesystem.
  prefs.end();

  String statusStr = {};
  String tmpStr = {};
  tm timeInfo = {};

  // START TIMING FOR WIFI + TIME SYNC + API
  unsigned long networkStartTime = millis();

  // START WIFI
  int8_t wifiRSSI = 0;  // “Received Signal Strength Indicator"
  wl_status_t wifiStatus = startWiFi(wifiRSSI);
  if (wifiStatus != WL_CONNECTED) {  // WiFi Connection Failed
    killWiFi();
    initDisplay();
    if (wifiStatus == WL_NO_SSID_AVAIL) {
      Serial.println(TXT_NETWORK_NOT_AVAILABLE);
      do {
        drawError(wifi_x_196x196, TXT_NETWORK_NOT_AVAILABLE);
      } while (display.nextPage());
    } else {
      Serial.println(TXT_WIFI_CONNECTION_FAILED);
      do {
        drawError(wifi_x_196x196, TXT_WIFI_CONNECTION_FAILED);
      } while (display.nextPage());
    }
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }

  // TIME SYNCHRONIZATION
  // Sync periodically based on configured interval (NTP_SYNC_INTERVAL_HOURS) and wake-up counter.
  // If RTC time is not valid (e.g., after reset or power loss), force an immediate sync.
  setenv("TZ", D_TIMEZONE, 1);
  tzset();

  bool timeConfigured = false;
  getLocalTime(&timeInfo);  // Updates timeInfo with current RTC time

  // Calculate how many cycles represent the sync interval
  // Ensure we perform integer division, defaulting to at least 1 cycle if sleep duration > interval
  unsigned int cyclesPerInterval = (NTP_SYNC_INTERVAL_HOURS * 60) / SLEEP_DURATION;
  if (cyclesPerInterval < 1) {
    cyclesPerInterval = 1;
  }

  bool driftIsHuge = (timeInfo.tm_year < (2020 - 1900));  // RTC lost power or uninitialized
  bool timerTriggered = (wakeUpCounter >= cyclesPerInterval);

  if (driftIsHuge || timerTriggered) {
    configTzTime(D_TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
    timeConfigured = waitForSNTPSync(&timeInfo);
    if (timeConfigured) {
      wakeUpCounter = 0;  // Reset counter after successful sync
    }
  } else {
    Serial.println("Using internal RTC time. (Wake #" + String(wakeUpCounter) + "/" + String(cyclesPerInterval) + ")");
    timeConfigured = true;
  }

  wakeUpCounter++;

  if (!timeConfigured) {
    Serial.println(TXT_TIME_SYNCHRONIZATION_FAILED);
    handleNetworkError(wi_time_4_196x196, TXT_TIME_SYNCHRONIZATION_FAILED, "", startTime, &timeInfo, batteryVoltage,
                       batteryPercent, wifiRSSI, networkStartTime);
  }

// MAKE API REQUESTS
#if defined(API_PROTOCOL_HTTP)
  WiFiClient client;
#elif defined(API_PROTOCOL_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
#elif defined(API_PROTOCOL_HTTPS_VERIFY)
  WiFiClientSecure client;
#ifdef WEATHER_API_OPEN_WEATHER_MAP
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
#endif
#ifdef WEATHER_API_OPEN_METEO
  client.setCACert(cert_ISRG_Root_X1);
#endif
#endif
#ifdef WEATHER_API_OPEN_WEATHER_MAP
  int rxStatus = getOWMonecall(client, environment_data);
  if (rxStatus != HTTP_CODE_OK) {
    statusStr = "One Call " + OWM_ONECALL_VERSION + " API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                       wifiRSSI, networkStartTime);
  }
#endif
#ifdef WEATHER_API_OPEN_METEO
  int rxStatus = getOMCall(client, environment_data);
  if (rxStatus != HTTP_CODE_OK) {
    statusStr = "Open Meteo API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                       wifiRSSI, networkStartTime);
  }
#endif

#if defined(API_PROTOCOL_HTTPS_VERIFY)
#ifdef AIR_QUALITY_API_OPEN_WEATHER_MAP
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
#endif
#ifdef AIR_QUALITY_API_OPEN_METEO
  client.setCACert(cert_ISRG_Root_X1);
#endif
#endif
  rxStatus = getAirPollution(client, air_pollution);
  if (rxStatus != HTTP_CODE_OK) {
    statusStr = "Air Pollution API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                       wifiRSSI, networkStartTime);
  }
  // SEND MQTT STATUS (success case)
#if HOME_ASSISTANT_MQTT_ENABLED
  if (WiFi.status() == WL_CONNECTED) {
    sendMQTTStatus(batteryVoltage, batteryPercent, wifiRSSI, millis() - networkStartTime);
  }
#endif

  killWiFi();  // WiFi no longer needed
  long networkDuration = millis() - networkStartTime;
  Serial.println("Network operations took " + String(networkDuration) + " ms");

  enrichWithMoonData(environment_data);

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, timeConfigured, &timeInfo);
  String dateStr;
  getDateStr(dateStr, &timeInfo);

  // RENDER FULL REFRESH
  initDisplay();
  do {
    drawCurrentConditions(environment_data.current, environment_data.daily[0], air_pollution);
    Serial.println("Drawing current conditions");
    drawOutlookGraph(environment_data.hourly, environment_data.daily, timeInfo);
    Serial.println("Drawing outlook graph");
    drawForecast(environment_data.daily, timeInfo);
    Serial.println("Drawing forecast");
    drawLocationDate(CITY_STRING, dateStr);
    Serial.println("Drawing location and date");
#if DISPLAY_ALERTS
    drawAlerts(environment_data.alerts, CITY_STRING, dateStr);
#endif
    drawStatusBar(statusStr, refreshTimeStr, wifiRSSI, batteryVoltage);
  } while (display.nextPage());
  powerOffDisplay();

  // DEEP SLEEP
  beginDeepSleep(startTime, &timeInfo);
}  // end setup

/* This will never run
 */
void loop() {}  // end loop
