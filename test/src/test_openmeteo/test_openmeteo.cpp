/* Open-Meteo configuration test driver.
 *
 * All feature modules are registered in one Unity session so PlatformIO can
 * build and boot this configuration only once.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "../test_harness.h"

#include "display_utils.inc"
#include "moon_tools.inc"
#include "meteoalarm.inc"
#include "open_meteo_air_quality_provider.inc"
#include "open_meteo_weather_provider.inc"
#include "rtc_drift_correction.inc"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();

  display_utils_tests::registerTests();
  rtc_drift_correction_tests::registerTests();
  moon_tools_tests::registerTests();
  open_meteo_weather_tests::registerTests();
  open_meteo_air_quality_tests::registerTests();
  meteoalarm_tests::registerTests();

  UNITY_END();
}

void loop() {}
