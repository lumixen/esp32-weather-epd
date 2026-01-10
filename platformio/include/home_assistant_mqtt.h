#include "config.h"

#ifdef HOME_ASSISTANT_MQTT_ENABLED
// MQTT Device Discovery Payload
static const char HOME_ASSISTANT_MQTT_DEVICE_DISCOVERY_PAYLOAD[] PROGMEM =
"{"
  "\"dev\":{"
    "\"ids\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "\","
    "\"name\":\"" D_HOME_ASSISTANT_MQTT_DEVICE_NAME "\","
    "\"mf\":\"lumixen\","
    "\"mdl\":\"ESP32 Weather EPD\""
  "},"
  "\"o\":{"
    "\"name\":\"" D_HOME_ASSISTANT_MQTT_DEVICE_NAME "\","
    "\"sw\":\"0.1\""
  "},"
  "\"cmps\":{"
    "\"battery_voltage\":{"
      "\"p\":\"sensor\","
      "\"device_class\":\"voltage\","
      "\"unit_of_measurement\":\"mV\","
      "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_battery_voltage\","
      "\"state_topic\":\"esp32_weather_epd/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery/voltage\""
    "},"
    "\"battery_percent\":{"
      "\"p\":\"sensor\","
      "\"device_class\":\"battery\","
      "\"unit_of_measurement\":\"%\","
      "\"unique_id\":\"" D_HOME_ASSISTANT_MQTT_CLIENT_ID "_battery_percent\","
      "\"state_topic\":\"esp32_weather_epd/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery/percent\""
    "}"
  "},"
  "\"qos\":2"
"}";

// Discovery Topic
static const char HOME_ASSISTANT_MQTT_DEVICE_DISCOVERY_TOPIC[] PROGMEM = 
  "homeassistant/device/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/config";

// State Topics
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_VOLTAGE[] PROGMEM = 
 "esp32_weather_epd/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery/voltage";
static const char HOME_ASSISTANT_MQTT_STATE_TOPIC_PERCENT[] PROGMEM = 
  "esp32_weather_epd/" D_HOME_ASSISTANT_MQTT_CLIENT_ID "/battery/percent";

#endif // MQTT_ENABLED