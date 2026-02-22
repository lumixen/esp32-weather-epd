#pragma once

#include "config.h"

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include <pgmspace.h>

// Base topic prefix for state messages
#define MQTT_STATE_BASE_TOPIC "esp32_weather_epd/"

// Configuration (discovery) Topics
static const char HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/${clientId}/battery_voltage/config";
static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/${clientId}/battery_percent/config";
static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/${clientId}/wifi_rssi/config";
static const char HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/${clientId}/api_activity_duration/config";

// State Topics
static const char MQTT_STATE_TOPIC_VOLTAGE[] PROGMEM = MQTT_STATE_BASE_TOPIC "${clientId}/battery_voltage";
static const char MQTT_STATE_TOPIC_PERCENT[] PROGMEM = MQTT_STATE_BASE_TOPIC "${clientId}/battery_percent";
static const char MQTT_STATE_TOPIC_RSSI[] PROGMEM = MQTT_STATE_BASE_TOPIC "${clientId}/wifi_rssi";
static const char MQTT_STATE_TOPIC_API_ACTIVITY_DURATION[] PROGMEM =
    MQTT_STATE_BASE_TOPIC "${clientId}/api_activity_duration";

// Device information (shared across all sensors)
#define MQTT_DEVICE_INFO \
  "\"device\":{" \
  "\"ids\":\"${clientId}\"," \
  "\"name\":\"" D_HOME_ASSISTANT_MQTT_DEVICE_NAME "\"," \
  "\"mf\":\"lumixen\"," \
  "\"mdl\":\"ESP32 Weather EPD\"," \
  "\"sw\":\"" D_BUILD_VERSION "\"" \
  "}"

// Sensor Discovery Payloads
// We inject the state topic dynamically in the C++ code, but the unique_id and device info need the clientId
static const char HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"voltage\","
    "\"unit_of_measurement\":\"V\","
    "\"unique_id\":\"${clientId}_battery_voltage\","
    "\"object_id\":\"${clientId}_battery_voltage\","
    "\"name\":\"Battery Voltage\","
    "\"state_topic\":\"" MQTT_STATE_BASE_TOPIC "${clientId}/battery_voltage\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"battery\","
    "\"unit_of_measurement\":\"%\","
    "\"unique_id\":\"${clientId}_battery_percent\","
    "\"object_id\":\"${clientId}_battery_percent\","
    "\"name\":\"Battery Level\","
    "\"state_topic\":\"" MQTT_STATE_BASE_TOPIC "${clientId}/battery_percent\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"signal_strength\","
    "\"unit_of_measurement\":\"dBm\","
    "\"unique_id\":\"${clientId}_wifi_rssi\","
    "\"object_id\":\"${clientId}_wifi_rssi\","
    "\"name\":\"WiFi Signal Strength\","
    "\"state_topic\":\"" MQTT_STATE_BASE_TOPIC "${clientId}/wifi_rssi\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"duration\","
    "\"unit_of_measurement\":\"ms\","
    "\"unique_id\":\"${clientId}_api_activity_duration\","
    "\"object_id\":\"${clientId}_api_activity_duration\","
    "\"name\":\"API Activity Duration\","
    "\"state_topic\":\"" MQTT_STATE_BASE_TOPIC "${clientId}/api_activity_duration\"," MQTT_DEVICE_INFO "}";

#endif  // HOME_ASSISTANT_MQTT_ENABLED