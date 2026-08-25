/* Environment sensor interface for esp32-weather-epd.
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

#include <optional>

#include "config.h"

class EnvSensor {
 public:
  virtual ~EnvSensor() = default;

  // Initialize the sensor, return true on success
  virtual bool begin() = 0;

  // Release active hardware resources and enter the lowest-power state. The
  // default is suitable for sensors that do not need an explicit shutdown.
  virtual void shutdown() {}

  virtual std::optional<float> getTemperature() = 0;
  virtual std::optional<float> getHumidity() = 0;
  virtual std::optional<float> getPressure() = 0;
};