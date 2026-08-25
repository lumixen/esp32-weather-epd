/* BME280 environment sensor implementation interface for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "config.h"
#ifdef BME_TYPE_BME280
#include "env_sensor.h"
#include <Adafruit_BME280.h>
#include <Wire.h>

class BME280EnvSensor : public EnvSensor {
 public:
  BME280EnvSensor();
  ~BME280EnvSensor() override;
  bool begin() override;
  void shutdown() override;
  std::optional<float> getTemperature() override;
  std::optional<float> getHumidity() override;
  std::optional<float> getPressure() override;

 private:
  Adafruit_BME280 bme;
  uint8_t i2cAddress;
  TwoWire wire;
  bool initialized;
  bool powered;
  bool wireStarted;
};
#endif