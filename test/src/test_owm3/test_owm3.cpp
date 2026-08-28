/* OpenWeatherMap configuration test driver.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "../test_harness.h"
#include "owm_v3_provider.inc"
#include "owm_v3_weather_provider.inc"
#include "owm_air_quality_provider.inc"
#include "fetch_executor.inc"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  // Run executor tests before the large parser fixtures so their FreeRTOS
  // worker tasks are fully reclaimed before the parser-heavy cases.
  fetch_executor_tests::registerTests();
  owm_v3_provider_tests::registerTests();
  owm_weather_tests::registerTests();
  owm_air_quality_tests::registerTests();
  UNITY_END();
  // Keep the test task parked after Unity emits its summary. Returning to a
  // tight empty loop makes QEMU repeatedly reset the task watchdog and can
  // corrupt the serial result stream after the tests have completed.
  for (;;) {
    delay(1000);
  }
}

void loop() {}
