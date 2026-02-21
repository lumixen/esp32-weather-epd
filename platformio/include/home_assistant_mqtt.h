#pragma once

#include "config.h"

#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED

// Base topic prefix for state messages
#define MQTT_STATE_BASE_TOPIC "esp32-weather-epd/"

// Configuration (discovery) Topics
static const char HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_voltage/config";
static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_percent/config";
static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/wifi_rssi/config";
static const char HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_TOPIC[] PROGMEM =
    D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/api_activity_duration/config";

// State Topics (separate topic structure)
#define MQTT_STATE_TOPIC_VOLTAGE MQTT_STATE_BASE_TOPIC D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_voltage"
#define MQTT_STATE_TOPIC_PERCENT MQTT_STATE_BASE_TOPIC D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_percent"
#define MQTT_STATE_TOPIC_RSSI MQTT_STATE_BASE_TOPIC D_HOME_ASSISTANT_MQTT_CLIENT_ID "/wifi_rssi"
#define MQTT_STATE_TOPIC_API_ACTIVITY_DURATION MQTT_STATE_BASE_TOPIC D_HOME_ASSISTANT_MQTT_CLIENT_ID "/api_activity_duration"

static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_VOLTAGE[] PROGMEM = MQTT_STATE_TOPIC_VOLTAGE;
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_PERCENT[] PROGMEM = MQTT_STATE_TOPIC_PERCENT;
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_RSSI[] PROGMEM = MQTT_STATE_TOPIC_RSSI;
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_API_ACTIVITY_DURATION[] PROGMEM = MQTT_STATE_TOPIC_API_ACTIVITY_DURATION;

// Device information (shared across all sensors)
#define MQTT_DEVICE_INFO \
  "\"device\":{" \
  "\"ids\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "\"," \
  "\"name\":\"" D_HOME_ASSISTANT_MQTT_DEVICE_NAME "\"," \
  "\"mf\":\"lumixen\"," \
  "\"mdl\":\"ESP32 Weather EPD\"," \
  "\"sw\":\"" D_BUILD_VERSION "\"" \
  "}"

// Sensor Discovery Payloads (with full device information in each)
static const char HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"voltage\","
    "\"unit_of_measurement\":\"V\","
    "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_battery_voltage\","
    "\"name\":\"Battery Voltage\","
    "\"state_topic\":\"" MQTT_STATE_TOPIC_VOLTAGE "\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"battery\","
    "\"unit_of_measurement\":\"%\","
    "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_battery_percent\","
    "\"name\":\"Battery Level\","
    "\"state_topic\":\"" MQTT_STATE_TOPIC_PERCENT "\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"signal_strength\","
    "\"unit_of_measurement\":\"dBm\","
    "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_wifi_rssi\","
    "\"name\":\"WiFi Signal Strength\","
    "\"state_topic\":\"" MQTT_STATE_TOPIC_RSSI "\"," MQTT_DEVICE_INFO "}";

static const char HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_PAYLOAD[] PROGMEM =
    "{"
    "\"device_class\":\"duration\","
    "\"unit_of_measurement\":\"ms\","
    "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_api_activity_duration\","
    "\"name\":\"API Activity Duration\","
    "\"state_topic\":\"" MQTT_STATE_TOPIC_API_ACTIVITY_DURATION "\"," MQTT_DEVICE_INFO "}";

#endif  // HOME_ASSISTANT_MQTT_ENABLED