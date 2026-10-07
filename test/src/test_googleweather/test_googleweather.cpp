/* QEMU test driver for the Google Weather API provider.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Arduino.h>
#include <unity.h>

#include "google_weather_provider.inc"

void setup() {
  delay(100);
  UNITY_BEGIN();
  google_weather_tests::registerTests();
  UNITY_END();
  for (;;) {
    delay(1000);
  }
}

void loop() {}
