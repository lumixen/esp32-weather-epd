/* OpenWeatherMap One Call v4 QEMU test driver.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <Arduino.h>
#include <unity.h>

#include "owm_v4_provider.inc"

void setup() {
  delay(100);
  UNITY_BEGIN();
  owm_v4_tests::registerTests();
  UNITY_END();
}

void loop() {}
