/* OpenWeatherMap configuration test driver.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "../test_harness.h"
#include "owm_provider.inc"
#include "owm_weather_provider.inc"
#include "owm_air_quality_provider.inc"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  owm_provider_tests::registerTests();
  owm_weather_tests::registerTests();
  owm_air_quality_tests::registerTests();
  UNITY_END();
}

void loop() {}
