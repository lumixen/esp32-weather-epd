/* OpenWeatherMap configuration test driver.
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "../test_harness.h"
#include "owm_weather_provider.inc"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  owm_weather_tests::registerTests();
  UNITY_END();
}

void loop() {}
