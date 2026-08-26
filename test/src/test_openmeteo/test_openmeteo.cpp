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
#include "../iso8601.inc"

#include "display_utils.inc"
#include "environment_sensor.inc"
#include "moon_tools.inc"
#include "sun_tools.inc"
#include "meteoalarm.inc"
#include "open_meteo_air_quality_provider.inc"
#include "open_meteo_weather_provider.inc"
#include "rtc_drift_correction.inc"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();

  iso8601_tests::registerTests();
  display_utils_tests::registerTests();
  environment_sensor_tests::registerTests();
  rtc_drift_correction_tests::registerTests();
  moon_tools_tests::registerTests();
  sun_tools_tests::registerTests();
  open_meteo_weather_tests::registerTests();
  open_meteo_air_quality_tests::registerTests();
  meteoalarm_tests::registerTests();

  UNITY_END();
  // Keep the test task parked after Unity emits its summary. Returning to a
  // tight empty loop makes QEMU repeatedly reset the task watchdog and can
  // corrupt the serial result stream after the tests have completed.
  for (;;) {
    delay(1000);
  }
}

void loop() {}
