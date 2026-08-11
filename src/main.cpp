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
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>

#include "_locale.h"
#include "time_utils.h"
#include "air_quality_provider.h"
#include "alert_provider.h"
#include "client_utils.h"
#include "config.h"
#include "data_models.h"
#include "display_utils.h"
#include "icons/icons_196x196.h"
#include "logger.h"
#include "provider_factory.h"
#include "renderer.h"
#include "moon_tools.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include "home_assistant_mqtt_client.h"
#endif
#ifndef BME_TYPE_NONE
#include "env_sensor.h"
#ifdef BME_TYPE_BME280
#include "env_sensor_bme280.h"
#endif
#endif

// too large to allocate locally on stack
static forecast_t environment_data;
static air_quality_t air_pollution;
static std::vector<weather_alert_t> alerts;

Preferences prefs;

static SemaphoreHandle_t sensorReadingDoneSemaphore = nullptr;

std::optional<float> inTemp = {};
std::optional<float> inHumidity = {};
std::optional<float> inPressure = {};

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

  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  LOG_INFO("%s %ss", TXT_AWAKE_FOR, String((millis() - startTime) / 1000.0, 3).c_str());
  LOG_INFO("%s %llus", TXT_ENTERING_DEEP_SLEEP_FOR, sleepDuration);
  esp_deep_sleep_start();
}  // end beginDeepSleep

sensor_readings getSensorReadings() {
  if (sensorReadingDoneSemaphore == nullptr) {
    return {.temperature = inTemp, .humidity = inHumidity, .pressure = inPressure};
  }
  std::optional<float> inTempSafeCopy = {};
  std::optional<float> inHumiditySafeCopy = {};
  std::optional<float> inPressureSafeCopy = {};
#ifndef BME_TYPE_NONE
  if (xSemaphoreTake(sensorReadingDoneSemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
    inTempSafeCopy = inTemp;
    inHumiditySafeCopy = inHumidity;
    inPressureSafeCopy = inPressure;
    vSemaphoreDelete(sensorReadingDoneSemaphore);
    sensorReadingDoneSemaphore = nullptr;
  } else {
    LOG_CRITICAL("Timeout waiting for sensor reading to complete");
  }
#endif
  return {.temperature = inTempSafeCopy, .humidity = inHumiditySafeCopy, .pressure = inPressureSafeCopy};
}

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
void publishMqtt(uint32_t batteryVoltage, uint8_t batteryPercent, int8_t wifiRSSI, unsigned long apiActivityDuration) {
  sensor_readings sensorReadings = getSensorReadings();
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
                        int8_t wifiRSSI) {
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
  publishMqtt(batteryVoltage, batteryPercent, wifiRSSI, 0);
#endif

  killWiFi();
  initDisplay();
  do {
    drawError(icon, statusStr, tmpStr);
  } while (display.nextPage());
  powerOffDisplay();
  beginDeepSleep(startTime, timeInfo);
}

#ifndef BME_TYPE_NONE
void envSensorReadingTask(void *pvParameters) {
#ifdef BME_TYPE_BME280
  EnvSensor *sensor = new BME280EnvSensor();
#endif
  if (sensor->begin()) {
    inTemp = sensor->getTemperature();
    inHumidity = sensor->getHumidity();
    inPressure = sensor->getPressure();

    LOG_INFO("Temp: %s°C, Humidity: %s%%, Pressure: %s hPa", String(inTemp.value_or(NAN)).c_str(),
             String(inHumidity.value_or(NAN)).c_str(), String(inPressure.value_or(NAN)).c_str());
  } else {
    LOG_CRITICAL("Failed to initialize BME sensor");
  }
  delete sensor;
  xSemaphoreGive(sensorReadingDoneSemaphore);  // Signal completion
  vTaskDelete(NULL);                           // Delete this task when done
}
#endif

#if !defined(PIO_UNIT_TESTING)

/* Program entry point.
 */
void setup() {
  unsigned long startTime = millis();
  Serial.begin(115200);
  toggleBuiltinLED(true);

  printHeapUsage();

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
        esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
        LOG_WARNING("%s", TXT_VERY_LOW_BATTERY_VOLTAGE);
        LOG_WARNING("%s %umin", TXT_ENTERING_DEEP_SLEEP_FOR, VERY_LOW_BATTERY_SLEEP_INTERVAL);
      } else {  // low battery
        esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
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

#ifndef BME_TYPE_NONE
  sensorReadingDoneSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(envSensorReadingTask, "EnvSensorReadingTask",
              4096,  // Stack size
              NULL,  // Parameters
              1,     // Priority
              NULL   // Task handle
  );
#endif

  // START TIMING FOR WIFI + TIME SYNC + API
  unsigned long networkStartTime = millis();

  // START WIFI
  int8_t wifiRSSI = 0;  // “Received Signal Strength Indicator"
  wl_status_t wifiStatus = startWiFi(wifiRSSI);
  if (wifiStatus != WL_CONNECTED) {  // WiFi Connection Failed
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
                       batteryPercent, wifiRSSI);
  }


#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
  unsigned long apiRequestsStartTime = millis();
#endif
// MAKE API REQUESTS
  WeatherProvider *weatherProvider = createWeatherProvider();
  int rxStatus = weatherProvider->fetch(environment_data);
  if (rxStatus != HTTP_CODE_OK) {
    statusStr = weatherProvider->getApiName();
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                       wifiRSSI);
  }
  AlertProvider *alertProvider = createAlertProvider(weatherProvider);
  if (alertProvider != nullptr) {
    // alerts may be served from the weather provider's stored response, in
    // which case no additional HTTP request is made
    rxStatus = alertProvider->fetch(alerts);
    if (rxStatus != HTTP_CODE_OK) {
      // Alerts are a non-essential source: log the failure and continue
      // without them instead of taking over the whole display.
      LOG_ERROR("Alerts API: %d: %s", rxStatus, getHttpResponsePhrase(rxStatus));
      alerts.clear();
    }
  }

  AirQualityProvider *airQualityProvider = createAirQualityProvider();
  rxStatus = airQualityProvider->fetch(air_pollution);
  if (rxStatus != HTTP_CODE_OK) {
    statusStr = "Air Pollution API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    handleNetworkError(wi_cloud_down_196x196, statusStr, tmpStr, startTime, &timeInfo, batteryVoltage, batteryPercent,
                       wifiRSSI);
  }
// SEND MQTT STATUS (success case)
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
  publishMqtt(batteryVoltage, batteryPercent, wifiRSSI, millis() - apiRequestsStartTime);
#endif

  killWiFi();  // WiFi no longer needed
  long networkDuration = millis() - networkStartTime;
  LOG_INFO("Network operations took %ss", String(networkDuration / 1000.0, 3).c_str());

  moon_state_t moon = getMoonState(LAT.toDouble(), LON.toDouble());

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, timeConfigured, &timeInfo);
  String dateStr;
  getDateStr(dateStr, &timeInfo);

  sensor_readings sensorReadings = getSensorReadings();

  // RENDER FULL REFRESH
  initDisplay();
  do {
    drawCurrentConditions(environment_data.current, air_pollution, sensorReadings.pressure, moon);
    LOG_INFO("Drawing current conditions");
    drawOutlookGraph(environment_data.hourly, environment_data.daily, timeInfo, moon);
    LOG_INFO("Drawing outlook graph");
    drawForecast(environment_data.daily, timeInfo);
    LOG_INFO("Drawing forecast");
    drawLocationDate(CITY_STRING, dateStr);
    LOG_INFO("Drawing location and date");
    drawAlerts(alerts, CITY_STRING, dateStr);
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
