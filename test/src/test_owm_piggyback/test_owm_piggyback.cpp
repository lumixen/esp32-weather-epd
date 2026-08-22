/* OpenWeatherMap weather/alerts piggyback configuration test driver.
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "fetch_executor.inc"
#include "test_harness.h"

void setUp(void) { test_harness::dispatchSetUp(); }

void tearDown(void) { test_harness::dispatchTearDown(); }

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  fetch_executor_tests::registerTests();
  UNITY_END();
}

void loop() {}
