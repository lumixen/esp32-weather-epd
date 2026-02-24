#include "home_assistant_mqtt_client.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include <WiFi.h>
#include <ESP32MQTTClient.h>

ESP32MQTTClient haMqttClient;
SemaphoreHandle_t haMqttConnectSemaphore = NULL;
RTC_DATA_ATTR bool publishedMqttConfig = false;

// Required global callback for connection events
void onMqttConnect(esp_mqtt_client_handle_t client) {
  if (haMqttClient.isMyTurn(client)) {
    if (haMqttConnectSemaphore != NULL) {
      xSemaphoreGive(haMqttConnectSemaphore);
    }
  }
}

// Required global event handler - ESP-IDF version dependent
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  haMqttClient.onEventCallback(event);
  return ESP_OK;
}
#else
void handleMQTT(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
  mqttClient.onEventCallback(event);
}
#endif

std::string formatMQTTString(const char *progmemTemplate, const char *clientId) {
  std::string buffer;
  // Reserve some memory to avoid frequent reallocations.
  buffer.reserve(strlen_P(progmemTemplate) + strlen(clientId) * 4 + 1);

  char c;
  const char *ptr = progmemTemplate;
  const char *placeholder = "${clientId}";
  int placeholderLen = 11;

  while ((c = pgm_read_byte(ptr))) {
    // Check for placeholder match
    if (c == '$' && strncmp_P(ptr, placeholder, placeholderLen) == 0) {
      buffer += clientId;
      ptr += placeholderLen;  // Skip placeholder in template
    } else {
      buffer += c;
      ptr++;
    }
  }
  return buffer;
}

bool publishMQTTSensorDiscovery(const char *sensorName, const char *clientId, const char *discoveryTopicTemplate,
                                const char *discoveryPayloadTemplate) {
  std::string discoveryTopic = formatMQTTString(discoveryTopicTemplate, clientId);
  std::string discoveryPayload = formatMQTTString(discoveryPayloadTemplate, clientId);

  if (haMqttClient.publish(discoveryTopic.c_str(), discoveryPayload.c_str(), 0, true)) {
    Serial.printf("Published %s discovery\n", sensorName);
    return true;
  } else {
    Serial.printf("Warning: Failed to publish %s discovery\n", sensorName);
    return false;
  }
}

bool publishMQTTSensorState(const char *sensorName, const char *clientId, const char *stateTopicTemplate,
                            const char *stateValue) {
  std::string stateTopic = formatMQTTString(stateTopicTemplate, clientId);

  if (haMqttClient.publish(stateTopic.c_str(), stateValue, 0, true)) {
    Serial.printf("Published %s state\n", sensorName);
    return true;
  } else {
    Serial.printf("Warning: Failed to publish %s state\n", sensorName);
    return false;
  }
}

void sendMQTTStatus(const mqtt_status_params_t &params) {
  unsigned long startTime = millis();
  if (haMqttConnectSemaphore == NULL) {
    haMqttConnectSemaphore = xSemaphoreCreateBinary();
  }
  // Generate dynamic client ID based on MAC
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  macAddress.toLowerCase();
  String macSuffix = macAddress.substring(macAddress.length() - 6);
  String clientIdStr = "esp32_weather_display_" + macSuffix;
  const char *clientId = clientIdStr.c_str();
  haMqttClient.setURL(D_HOME_ASSISTANT_MQTT_SERVER, HOME_ASSISTANT_MQTT_PORT, D_HOME_ASSISTANT_MQTT_USERNAME,
                      D_HOME_ASSISTANT_MQTT_PASSWORD);
  haMqttClient.setMaxPacketSize(768);
  haMqttClient.setMqttClientName(clientId);
  haMqttClient.setAutoReconnect(false);
  haMqttClient.loopStart();

  Serial.print("Connecting to MQTT with ID: ");
  Serial.println(clientId);

  if (xSemaphoreTake(haMqttConnectSemaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
    Serial.println("MQTT connected");

    bool publishSuccess = true;
    if (!publishedMqttConfig) {
      Serial.println("Publishing discovery messages...");
      // Publish discovery messages only once per power cycle
      publishSuccess &=
          publishMQTTSensorDiscovery("Battery voltage", clientId, HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC,
                                     HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_PAYLOAD);
      publishSuccess &=
          publishMQTTSensorDiscovery("Battery percent", clientId, HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC,
                                     HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD);
      publishSuccess &= publishMQTTSensorDiscovery("WiFi RSSI", clientId, HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC,
                                                   HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD);
      publishSuccess &=
          publishMQTTSensorDiscovery("API activity duration", clientId, HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_TOPIC,
                                     HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_PAYLOAD);
#ifndef BME_TYPE_NONE
      publishSuccess &= publishMQTTSensorDiscovery("Temperature", clientId, HOME_ASSISTANT_MQTT_TEMPERATURE_TOPIC,
                                                   HOME_ASSISTANT_MQTT_TEMPERATURE_PAYLOAD);
      publishSuccess &= publishMQTTSensorDiscovery("Humidity", clientId, HOME_ASSISTANT_MQTT_HUMIDITY_TOPIC,
                                                   HOME_ASSISTANT_MQTT_HUMIDITY_PAYLOAD);
      publishSuccess &= publishMQTTSensorDiscovery("Pressure", clientId, HOME_ASSISTANT_MQTT_PRESSURE_TOPIC,
                                                   HOME_ASSISTANT_MQTT_PRESSURE_PAYLOAD);
#endif  // BME_TYPE_NONE
      publishedMqttConfig = publishSuccess;

      if (publishedMqttConfig) {
        Serial.println("Publishing sensor states...");
        char valueStr[12];
#if BATTERY_MONITORING
        // 1. Publish Battery Voltage
        snprintf(valueStr, sizeof(valueStr), "%.3f", params.batteryVoltage / 1000.0);
        publishMQTTSensorState("Battery voltage", clientId, MQTT_STATE_TOPIC_VOLTAGE, valueStr);

        // 2. Publish Battery Percent
        snprintf(valueStr, sizeof(valueStr), "%u", params.batteryPercentage);
        publishMQTTSensorState("Battery percent", clientId, MQTT_STATE_TOPIC_PERCENT, valueStr);
#endif

        // 3. Publish WiFi RSSI
        snprintf(valueStr, sizeof(valueStr), "%d", params.wifiRSSI);
        publishMQTTSensorState("WiFi RSSI", clientId, MQTT_STATE_TOPIC_RSSI, valueStr);

        // 4. Publish API Activity Duration
        snprintf(valueStr, sizeof(valueStr), "%lu", params.apiActivityDuration);
        publishMQTTSensorState("API activity duration", clientId, MQTT_STATE_TOPIC_API_ACTIVITY_DURATION, valueStr);
#ifndef BME_TYPE_NONE
        // 5. Publish Temperature
        if (params.temperature.has_value()) {
          snprintf(valueStr, sizeof(valueStr), "%.2f", params.temperature.value());
          publishMQTTSensorState("Temperature", clientId, MQTT_STATE_TOPIC_TEMPERATURE, valueStr);
        }
        // 6. Publish Humidity
        if (params.humidity.has_value()) {
          snprintf(valueStr, sizeof(valueStr), "%.2f", params.humidity.value());
          publishMQTTSensorState("Humidity", clientId, MQTT_STATE_TOPIC_HUMIDITY, valueStr);
        }
        // 7. Publish Pressure
        if (params.pressure.has_value()) {
          snprintf(valueStr, sizeof(valueStr), "%.2f", params.pressure.value());
          publishMQTTSensorState("Pressure", clientId, MQTT_STATE_TOPIC_PRESSURE, valueStr);
        }
#endif
      }
      if (!(haMqttClient.loopStop())) {
        Serial.println("Warning: MQTT loop did not stop cleanly.");
      }
      Serial.println("MQTT publish complete.");
    } else {
      Serial.println("Error: MQTT connection timed out.");
      haMqttClient.loopStop();
    }
    Serial.println("MQTT total time: " + String((millis() - startTime) / 1000.0, 3) + "s");
    vSemaphoreDelete(haMqttConnectSemaphore);
    haMqttConnectSemaphore = NULL;
  }
}
#endif  // HOME_ASSISTANT_MQTT_ENABLED