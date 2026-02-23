#pragma once
#include "config.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include "home_assistant_mqtt.h"

typedef struct {
  uint32_t batteryVoltage;
  uint8_t batteryPercentage;
  int8_t wifiRSSI;
  unsigned long apiActivityDuration;
  std::optional<float> temperature;
  std::optional<float> humidity;
  std::optional<float> pressure;
} mqtt_status_params_t;

void sendMQTTStatus(const mqtt_status_params_t &params);
#endif  // HOME_ASSISTANT_MQTT_ENABLED