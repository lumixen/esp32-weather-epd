#include "env_sensor_bme280.h"

BME280EnvSensor::BME280EnvSensor() : bme(), i2cAddress(BME_ADDRESS), wire(0), initialized(false) {}

BME280EnvSensor::~BME280EnvSensor() {
  wire.end();
  digitalWrite(PIN_BME_PWR, LOW);
}

bool BME280EnvSensor::begin() {
  pinMode(PIN_BME_PWR, OUTPUT);
  digitalWrite(PIN_BME_PWR, HIGH);
  delay(300);
  wire.begin(PIN_BME_SDA, PIN_BME_SCL, 100000);  // 100kHz
  initialized = bme.begin(i2cAddress, &wire);
  return initialized;
}

std::optional<float> BME280EnvSensor::getTemperature() {
  if (!initialized)
    return std::nullopt;
  float t = bme.readTemperature();
  if (isnan(t))
    return std::nullopt;
  return t;
}

std::optional<float> BME280EnvSensor::getHumidity() {
  if (!initialized)
    return std::nullopt;
  float h = bme.readHumidity();
  if (isnan(h))
    return std::nullopt;
  return h;
}

std::optional<float> BME280EnvSensor::getPressure() {
  if (!initialized)
    return std::nullopt;
  float p = bme.readPressure() / 100.0F;
  if (isnan(p))
    return std::nullopt;
  return p;
}