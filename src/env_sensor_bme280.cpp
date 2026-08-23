/* BME280 environment sensor implementation for esp32-weather-epd.
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

#include "env_sensor_bme280.h"
#ifdef BME_TYPE_BME280

BME280EnvSensor::BME280EnvSensor() : bme(), i2cAddress(BME_ADDRESS), wire(0), initialized(false) {}

BME280EnvSensor::~BME280EnvSensor() {
  wire.end();
  // Power off BME280
  digitalWrite(BME_PIN_PWR, LOW);
}

bool BME280EnvSensor::begin() {
  pinMode(BME_PIN_PWR, OUTPUT);
  digitalWrite(BME_PIN_PWR, HIGH);
  // Let BME280 stabilize after powering it on before attempting to communicate with it
  delay(300);
  wire.begin(BME_PIN_SDA, BME_PIN_SCL, 100000);  // 100kHz
  initialized = bme.begin(i2cAddress, &wire);
  return initialized;
}

std::optional<float> BME280EnvSensor::getTemperature() {
  if (!initialized)
    return {};
  float t = bme.readTemperature();
  if (isnan(t))
    return {};
  return t;
}

std::optional<float> BME280EnvSensor::getHumidity() {
  if (!initialized)
    return {};
  float h = bme.readHumidity();
  if (isnan(h))
    return {};
  return h;
}

std::optional<float> BME280EnvSensor::getPressure() {
  if (!initialized)
    return {};
  float p = bme.readPressure() / 100.0F;
  if (isnan(p))
    return {};
  return p;
}
#endif