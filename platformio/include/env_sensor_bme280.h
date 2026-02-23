#pragma once

#include "config.h"
#include "env_sensor.h"
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <optional>

class BME280EnvSensor : public EnvSensor {
 public:
  BME280EnvSensor();
  ~BME280EnvSensor() override;
  bool begin() override;
  std::optional<float> getTemperature() override;
  std::optional<float> getHumidity() override;
  std::optional<float> getPressure() override;

 private:
  Adafruit_BME280 bme;
  uint8_t i2cAddress;
  TwoWire wire;
  bool initialized;
};