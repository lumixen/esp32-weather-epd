#include "home_assistant_mqtt_client.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include <WiFi.h>
#include <ESP32MQTTClient.h>

ESP32MQTTClient mqttClient;
SemaphoreHandle_t mqttConnectSemaphore = NULL;

// Required global callback for connection events
void onMqttConnect(esp_mqtt_client_handle_t client) {
  if (mqttClient.isMyTurn(client)) {
    if (mqttConnectSemaphore != NULL) {
      xSemaphoreGive(mqttConnectSemaphore);
    }
  }
}

// Required global event handler - ESP-IDF version dependent
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
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
  // Guessing length + a bit for the ID replacement.
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

// Helper function to publish discovery and state for mqtt sensors.
// Returns true if both publishes were successful, otherwise false.
bool publishMQTTSensor(ESP32MQTTClient &mqtt, const char *sensorName, const char *clientId,
                       const char *discoveryTopicTemplate, const char *discoveryPayloadTemplate,
                       const char *stateTopicTemplate, const char *stateValue) {
  bool success = false;

  std::string discoveryTopic = formatMQTTString(discoveryTopicTemplate, clientId);
  std::string discoveryPayload = formatMQTTString(discoveryPayloadTemplate, clientId);
  std::string stateTopic = formatMQTTString(stateTopicTemplate, clientId);

  if (mqtt.publish(discoveryTopic.c_str(), discoveryPayload.c_str(), 0, true)) {
    if (mqtt.publish(stateTopic.c_str(), stateValue, 0, true)) {
      Serial.printf("  Published %s\n", sensorName);
      success = true;
    } else {
      Serial.printf("  Warning: Failed to publish %s state\n", sensorName);
    }
  } else {
    Serial.printf("  Warning: Failed to publish %s discovery\n", sensorName);
  }

  return success;
}

void sendMQTTStatus(uint32_t batteryVoltage, uint8_t batteryPercentage, int8_t wifiRSSI,
                    unsigned long apiActivityDuration) {
  if (mqttConnectSemaphore == NULL) {
    mqttConnectSemaphore = xSemaphoreCreateBinary();
  }
  // Generate dynamic client ID based on MAC
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  macAddress.toLowerCase();
  String macSuffix = macAddress.substring(macAddress.length() - 6);
  String clientIdStr = "esp32_weather_display_" + macSuffix;
  const char *clientId = clientIdStr.c_str();
  mqttClient.setURL(D_HOME_ASSISTANT_MQTT_SERVER, HOME_ASSISTANT_MQTT_PORT, D_HOME_ASSISTANT_MQTT_USERNAME,
                    D_HOME_ASSISTANT_MQTT_PASSWORD);
  mqttClient.setMaxPacketSize(768);
  mqttClient.setMqttClientName(clientId);
  mqttClient.setAutoReconnect(false);
  mqttClient.loopStart();

  Serial.print("Connecting to MQTT with ID: ");
  Serial.println(clientId);

  if (xSemaphoreTake(mqttConnectSemaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
    Serial.println("MQTT connected. Now publishing discovery and status.");
#if BATTERY_MONITORING
    // 1. Publish Battery Voltage
    char voltageStr[8];
    snprintf(voltageStr, sizeof(voltageStr), "%.3f", batteryVoltage / 1000.0);

    publishMQTTSensor(mqttClient, "Battery voltage", clientId, HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_TOPIC,
                      HOME_ASSISTANT_MQTT_BATTERY_VOLTAGE_PAYLOAD, MQTT_STATE_TOPIC_VOLTAGE, voltageStr);

    // 2. Publish Battery Percent
    char percentStr[4];
    snprintf(percentStr, sizeof(percentStr), "%u", batteryPercentage);

    publishMQTTSensor(mqttClient, "Battery percent", clientId, HOME_ASSISTANT_MQTT_BATTERY_PERCENT_TOPIC,
                      HOME_ASSISTANT_MQTT_BATTERY_PERCENT_PAYLOAD, MQTT_STATE_TOPIC_PERCENT, percentStr);
#endif

    // 3. Publish WiFi RSSI
    char rssiStr[5];
    snprintf(rssiStr, sizeof(rssiStr), "%d", wifiRSSI);

    publishMQTTSensor(mqttClient, "WiFi RSSI", clientId, HOME_ASSISTANT_MQTT_WIFI_RSSI_TOPIC,
                      HOME_ASSISTANT_MQTT_WIFI_RSSI_PAYLOAD, MQTT_STATE_TOPIC_RSSI, rssiStr);

    // 4. Publish API Activity Duration
    char durationStr[12];
    snprintf(durationStr, sizeof(durationStr), "%lu", apiActivityDuration);

    publishMQTTSensor(mqttClient, "API activity duration", clientId, HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_TOPIC,
                      HOME_ASSISTANT_MQTT_API_ACTIVITY_DURATION_PAYLOAD, MQTT_STATE_TOPIC_API_ACTIVITY_DURATION,
                      durationStr);
    if (!(mqttClient.loopStop())) {
      Serial.println("Warning: MQTT loop did not stop cleanly.");
    }
    Serial.println("MQTT publish complete.");
  } else {
    Serial.println("Error: MQTT connection timed out.");
    mqttClient.loopStop();
  }
}
#endif  // HOME_ASSISTANT_MQTT_ENABLED