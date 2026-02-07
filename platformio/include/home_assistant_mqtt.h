#include "config.h"

#ifdef HOME_ASSISTANT_MQTT_ENABLED

// Configuration (discovery) Topics
static const char HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC[] PROGMEM = 
  D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_voltage/config";
static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC[] PROGMEM = 
  D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_percent/config";
static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC[] PROGMEM = 
  D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/wifi_rssi/config";

// State Topics (defined first so they can be referenced in payloads)
#define MQTT_STATE_TOPIC_VOLTAGE D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_voltage/state"
#define MQTT_STATE_TOPIC_PERCENT D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery_percent/state"
#define MQTT_STATE_TOPIC_RSSI D_HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX "/sensor/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/wifi_rssi/state"

static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_VOLTAGE[] PROGMEM = MQTT_STATE_TOPIC_VOLTAGE;
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_PERCENT[] PROGMEM = MQTT_STATE_TOPIC_PERCENT;
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_RSSI[] PROGMEM = MQTT_STATE_TOPIC_RSSI;

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
  "\"state_topic\":\"" MQTT_STATE_TOPIC_VOLTAGE "\","
  MQTT_DEVICE_INFO
"}";

static const char HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD[] PROGMEM =
"{"
  "\"device_class\":\"battery\","
  "\"unit_of_measurement\":\"%\","
  "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_battery_percent\","
  "\"name\":\"Battery Level\","
  "\"state_topic\":\"" MQTT_STATE_TOPIC_PERCENT "\","
  MQTT_DEVICE_INFO
"}";

static const char HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD[] PROGMEM =
"{"
  "\"device_class\":\"signal_strength\","
  "\"unit_of_measurement\":\"dBm\","
  "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_wifi_rssi\","
  "\"name\":\"WiFi Signal\","
  "\"state_topic\":\"" MQTT_STATE_TOPIC_RSSI "\","
  MQTT_DEVICE_INFO
"}";

#endif // HOME_ASSISTANT_MQTT_ENABLED