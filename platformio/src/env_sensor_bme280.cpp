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