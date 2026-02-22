#pragma once
#include "config.h"
#if defined(HOME_ASSISTANT_MQTT_ENABLED) && HOME_ASSISTANT_MQTT_ENABLED
#include "home_assistant_mqtt.h"

void sendMQTTStatus(uint32_t batteryVoltage, uint8_t batteryPercentage, int8_t wifiRSSI,
                    unsigned long apiActivityDuration);
#endif  // HOME_ASSISTANT_MQTT_ENABLED