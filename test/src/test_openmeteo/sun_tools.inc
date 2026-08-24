/* Unit tests for the local sun-state helpers.
 *
 * The example date is fixed to 2026-08-24 (the date used when this feature
 * was planned) so the tests remain reproducible. Runtime code always derives
 * the date from the synchronized clock.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <unity.h>

#include "sun_tools.h"
#include "weather_report.h"
#include "../test_harness.h"

namespace sun_tools_tests {

void setUp(void) {}
void tearDown(void) {
  unsetenv("TZ");
  tzset();
}

static constexpr float kLondonLatitude = 51.5072f;
static constexpr float kLondonLongitude = -0.1276f;
static constexpr time_t kExampleDate = 1787572800LL;      // 2026-08-24 12:00 UTC
static constexpr time_t kExpectedSunrise = 1787547641LL;  // 05:00 UTC
static constexpr time_t kExpectedSunset = 1787598296LL;   // 19:04 UTC

static void useLondonTimezone() {
  // The QEMU image does not ship IANA zoneinfo files. This POSIX TZ string
  // is the equivalent of Europe/London for the example date and exercises
  // the same DST conversion used by the firmware.
  setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1);
  tzset();
}

void test_today_london(void) {
  useLondonTimezone();
  const sun_state_t sun = getSunState(kLondonLatitude, kLondonLongitude, kExampleDate);

  TEST_ASSERT_TRUE(sun.sunrise != 0);
  TEST_ASSERT_TRUE(sun.sunset != 0);
  TEST_ASSERT_TRUE(sun.sunrise < sun.sunset);
  TEST_ASSERT_INT32_WITHIN(120, kExpectedSunrise, sun.sunrise);
  TEST_ASSERT_INT32_WITHIN(120, kExpectedSunset, sun.sunset);
}

void test_separate_report_state(void) {
  weather_report_t report{};
  report.forecast.current.dt = kExampleDate;
  report.sun = sun_state_t{1234, 5678};
  report.moon = moon_state_t{9012, 3456, 0.5f};

  TEST_ASSERT_EQUAL_INT32(1234, report.sun.sunrise);
  TEST_ASSERT_EQUAL_INT32(5678, report.sun.sunset);
  TEST_ASSERT_EQUAL_INT32(9012, report.moon.moonrise);
  TEST_ASSERT_EQUAL_INT32(3456, report.moon.moonset);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, report.moon.phase);
  TEST_ASSERT_EQUAL_INT64(kExampleDate, report.forecast.current.dt);
}

void test_timezone_conversion(void) {
  useLondonTimezone();
  const sun_state_t sun = getSunState(kLondonLatitude, kLondonLongitude, kExampleDate);
  tm sunriseLocal{};
  tm sunsetLocal{};
  TEST_ASSERT_NOT_NULL(localtime_r(&sun.sunrise, &sunriseLocal));
  TEST_ASSERT_NOT_NULL(localtime_r(&sun.sunset, &sunsetLocal));

  // The example date is in British Summer Time. The returned timestamps are
  // UTC, while localtime() applies the configured +1 hour display offset.
  TEST_ASSERT_EQUAL_INT(6, sunriseLocal.tm_hour);
  TEST_ASSERT_EQUAL_INT(0, sunriseLocal.tm_min);
  TEST_ASSERT_EQUAL_INT(20, sunsetLocal.tm_hour);
  TEST_ASSERT_EQUAL_INT(4, sunsetLocal.tm_min);
  TEST_ASSERT_EQUAL_INT(1, sunriseLocal.tm_isdst);
  TEST_ASSERT_EQUAL_INT(1, sunsetLocal.tm_isdst);
}

void test_polar_no_events(void) {
  useLondonTimezone();
  const sun_state_t sun = getSunState(70.0f, 0.0f, 1782043200LL);  // 2026-06-21 12:00 UTC
  TEST_ASSERT_EQUAL_INT32(0, sun.sunrise);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunset);
}

void test_invalid_coordinates(void) {
  useLondonTimezone();
  sun_state_t sun = getSunState(91.0f, kLondonLongitude, kExampleDate);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunrise);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunset);

  sun = getSunState(kLondonLatitude, -181.0f, kExampleDate);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunrise);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunset);

  sun = getSunState(NAN, kLondonLongitude, kExampleDate);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunrise);
  TEST_ASSERT_EQUAL_INT32(0, sun.sunset);
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(sun_tools_tests::test_today_london);
  RUN_TEST(sun_tools_tests::test_separate_report_state);
  RUN_TEST(sun_tools_tests::test_timezone_conversion);
  RUN_TEST(sun_tools_tests::test_polar_no_events);
  RUN_TEST(sun_tools_tests::test_invalid_coordinates);
}

}  // namespace sun_tools_tests
