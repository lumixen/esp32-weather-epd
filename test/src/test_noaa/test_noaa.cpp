/* NOAA/NWS configuration test driver.
 *
 * Copyright (C) 2026  Max Bodaniuk
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>
#include "../test_harness.h"
#include "../iso8601.inc"
#include "noaa_forecast_provider.inc"

void setUp(void) { test_noaa::setUp(); }
void tearDown(void) { test_noaa::tearDown(); }

void setup() {
  delay(200);
  UNITY_BEGIN();
  iso8601_tests::registerTests();
  test_noaa::registerTests();
  UNITY_END();
  for (;;)
    delay(1000);
}

void loop() {}
