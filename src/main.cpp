/* Main program for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
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

#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "_locale.h"
#include "time_utils.h"
#include "client_utils.h"
#include "config.h"
#include "data_models.h"
#include "weather_report.h"
#include "display_utils.h"
#include "icons/icons_196x196.h"
#include "logger.h"
#include "provider_factory.h"
#include "provider_result.h"
#include "fetch_executor.h"
#include "environment_sensor_fetch_operation.h"
#include "renderer.h"
#include "moon_tools.h"
#include "sun_tools.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include "home_assistant_mqtt_client.h"
#endif

// too large to allocate locally on stack
static weather_report_t environment_data;

Preferences prefs;

/* Toggle the built-in LED on or off. */
void toggleBuiltinLED(bool state) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, state ? LOW : HIGH);  // Lolin D32 LED is active low
  return;
}

/* Put esp32 into ultra low-power deep sleep (<11μA).
 * Aligns wake time to the minute. Sleep times defined in config.
 */
void beginDeepSleep(unsigned long startTime, tm *timeInfo) {
  if (!getLocalTime(timeInfo)) {
    LOG_WARNING("%s", TXT_REFERENCING_OLDER_TIME_NOTICE);
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

  printHeapUsage();

  toggleBuiltinLED(false);

  esp_sleep_enable_timer_wakeup(rtcDriftScaleSleepUs(sleepDuration * 1000000ULL));
  LOG_INFO("%s %ss", TXT_AWAKE_FOR, String((millis() - startTime) / 1000.0, 3).c_str());
  LOG_INFO("%s %llus", TXT_ENTERING_DEEP_SLEEP_FOR, sleepDuration);
  esp_deep_sleep_start();
}  // end beginDeepSleep

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
void publishMqtt(uint32_t batteryVoltage, uint8_t batteryPercent, int8_t wifiRSSI, unsigned long apiActivityDuration,
                 const sensor_readings &sensorReadings) {
  if (WiFi.status() == WL_CONNECTED) {
    sendMQTTStatus({.batteryVoltage = batteryVoltage,
                    .batteryPercentage = batteryPercent,
                    .wifiRSSI = wifiRSSI,
                    .apiActivityDuration = apiActivityDuration,
                    .temperature = sensorReadings.temperature,
                    .humidity = sensorReadings.humidity,
                    .pressure = sensorReadings.pressure});
  }
}
#endif

void handleNetworkError(const unsigned char *icon, const String &statusStr, const String &tmpStr,
                        unsigned long startTime, tm *timeInfo, uint32_t batteryVoltage, uint8_t batteryPercent,
                        int8_t wifiRSSI, FetchExecution &sensorExecution, const sensor_readings &sensorReadings) {
  // close() waits for the local producer and releases the sensor before any
  // error status is published or the display enters its error path.
  sensorExecution.close();
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
  publishMqtt(batteryVoltage, batteryPercent, wifiRSSI, 0, sensorReadings);
#endif

  killWiFi();
  initDisplay();
  do {
    drawError(icon, statusStr, tmpStr);
  } while (display.nextPage());
  powerOffDisplay();
  beginDeepSleep(startTime, timeInfo);
}

#if !defined(PIO_UNIT_TESTING)

/* Program entry point.
 */
void setup() {
  unsigned long startTime = millis();
  Serial.begin(115200);
  toggleBuiltinLED(true);

  printHeapUsage();

  // Correct the wall clock for the slow-clock drift accumulated during the
  // previous deep sleep. Must run before any path (low battery, WiFi
  // failure, ...) can enter deep sleep again, or the recorded sleep duration
  // is replaced without its interval ever being corrected.
  rtcDriftApplyWakeupCorrection();

  // Open namespace for read/write to non-volatile storage
  prefs.begin(NVS_NAMESPACE, false);

#if BATTERY_MONITORING
  uint32_t batteryVoltage = 0;
  bool batteryVoltageValid = readBatteryVoltage(batteryVoltage);
  uint8_t batteryPercent;
  if (batteryVoltageValid) {
    batteryPercent = calcBatPercent(batteryVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
    LOG_INFO("%s: %umv", TXT_BATTERY_VOLTAGE, batteryVoltage);

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
        LOG_CRITICAL("%s", TXT_CRIT_LOW_BATTERY_VOLTAGE);
        LOG_CRITICAL("%s", TXT_HIBERNATING_INDEFINITELY_NOTICE);
      } else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE) {  // very low battery
        esp_sleep_enable_timer_wakeup(rtcDriftScaleSleepUs(VERY_LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL));
        LOG_WARNING("%s", TXT_VERY_LOW_BATTERY_VOLTAGE);
        LOG_WARNING("%s %umin", TXT_ENTERING_DEEP_SLEEP_FOR, VERY_LOW_BATTERY_SLEEP_INTERVAL);
      } else {  // low battery
        esp_sleep_enable_timer_wakeup(rtcDriftScaleSleepUs(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL));
        LOG_WARNING("%s", TXT_LOW_BATTERY_VOLTAGE);
        LOG_WARNING("%s %umin", TXT_ENTERING_DEEP_SLEEP_FOR, LOW_BATTERY_SLEEP_INTERVAL);
      }
      esp_deep_sleep_start();
    }
    // battery is no longer low, reset variable in non-volatile storage
    if (lowBat == true) {
      prefs.putBool("lowBat", false);
    }
  } else {
    // No valid reading: a transient ADC failure must not trigger the low-
    // battery shutdown, which would hibernate a charged device indefinitely.
    LOG_ERROR("Failed to read battery voltage, skipping low-battery check");
    batteryVoltage = UINT32_MAX;
    batteryPercent = UINT8_MAX;
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

  // Start local data collection independently of the network phase. The
  // execution handle owns the operation and sensor until the explicit wait
  // before MQTT/rendering.
  auto sensorExecution = executeParallelAsync(createEnvironmentSensorOperations(environment_data));

  // START TIMING FOR WIFI + TIME SYNC + API
  unsigned long networkStartTime = millis();

  // START WIFI
  int8_t wifiRSSI = 0;  // “Received Signal Strength Indicator"
  wl_status_t wifiStatus = startWiFi(wifiRSSI);
  if (wifiStatus != WL_CONNECTED) {  // WiFi Connection Failed
    sensorExecution.close();
    killWiFi();
    initDisplay();
    if (wifiStatus == WL_NO_SSID_AVAIL) {
      LOG_WARNING("%s", TXT_NETWORK_NOT_AVAILABLE);
      do {
        drawError(wifi_x_196x196, TXT_NETWORK_NOT_AVAILABLE);
      } while (display.nextPage());
    } else {
      LOG_WARNING("%s", TXT_WIFI_CONNECTION_FAILED);
      do {
        drawError(wifi_x_196x196, TXT_WIFI_CONNECTION_FAILED);
      } while (display.nextPage());
    }
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }

  bool timeConfigured = configureTime(&timeInfo);

  if (!timeConfigured) {
    LOG_WARNING("%s", TXT_TIME_SYNCHRONIZATION_FAILED);
    handleNetworkError(wi_time_4_196x196, TXT_TIME_SYNCHRONIZATION_FAILED, "", startTime, &timeInfo, batteryVoltage,
                       batteryPercent, wifiRSSI, sensorExecution, environment_data.sensor);
  }

  unsigned long apiRequestsStartTime = millis();
  // MAKE API REQUESTS — parallel with bounded pool (max 2 concurrent)
  auto fetchBundle = createFetchBundle(environment_data);
  auto results = executeParallel(fetchBundle.ops);
  for (size_t i = 0; i < fetchBundle.ops.size(); ++i) {
    if (!results[i].isOk() && fetchBundle.ops[i]->shouldAbortOnFailure()) {
      statusStr = fetchBundle.ops[i]->name();
      tmpStr = results[i].detail();
      handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                         wifiRSSI, sensorExecution, environment_data.sensor);
    }
    // Optional failures are logged and their provider operation has already
    // disengaged its report group; rendering continues.
    if (!results[i].isOk() && !fetchBundle.ops[i]->shouldAbortOnFailure()) {
      LOG_WARNING("Optional provider operation %s failed: %s", fetchBundle.ops[i]->name(), results[i].detail().c_str());
    }
  }
  // Synchronize and release the local producer once, immediately before
  // consumers use the report. The report owns the copied readings after this.
  sensorExecution.close();

// SEND MQTT STATUS (success case)
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
  publishMqtt(batteryVoltage, batteryPercent, wifiRSSI, millis() - apiRequestsStartTime, environment_data.sensor);
#endif

  killWiFi();  // WiFi no longer needed
  long networkDuration = millis() - networkStartTime;
  LOG_INFO("Network operations took %ss", String(networkDuration / 1000.0, 3).c_str());

  environment_data.sun = getSunState(LAT.toDouble(), LON.toDouble());
  environment_data.moon = getMoonState(LAT.toDouble(), LON.toDouble());

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, timeConfigured, &timeInfo);
  String dateStr;
  getDateStr(dateStr, &timeInfo);

  // RENDER FULL REFRESH
  initDisplay();
  do {
    drawCurrentConditions(environment_data);
    LOG_INFO("Drawing current conditions");
    drawOutlookGraph(environment_data, timeInfo);
    LOG_INFO("Drawing outlook graph");
    drawForecast(environment_data, timeInfo);
    LOG_INFO("Drawing forecast");
    drawLocationDate(CITY_STRING, dateStr);
    LOG_INFO("Drawing location and date");
    drawAlerts(environment_data, CITY_STRING, dateStr);
    drawStatusBar(statusStr, refreshTimeStr, wifiRSSI, batteryVoltage);
  } while (display.nextPage());
  powerOffDisplay();

  // DEEP SLEEP
  beginDeepSleep(startTime, &timeInfo);
}  // end setup

/* This will never run
 */
void loop() {}  // end loop

#endif  // !PIO_UNIT_TESTING
