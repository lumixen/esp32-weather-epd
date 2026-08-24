/* OpenWeatherMap weather/alerts piggyback configuration test driver.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "fetch_executor.inc"
#include "../test_owm/owm_provider.inc"
#include "../test_harness.h"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  owm_provider_tests::registerTests();
  fetch_executor_tests::registerTests();
  UNITY_END();
}

void loop() {}
