#pragma once

#include "config.h"

class EnvSensor {
 public:
  virtual ~EnvSensor() = default;

  // Initialize the sensor, return true on success
  virtual bool begin() = 0;

  virtual std::optional<float> getTemperature() = 0;
  virtual std::optional<float> getHumidity() = 0;
  virtual std::optional<float> getPressure() = 0;
};