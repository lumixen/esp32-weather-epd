/* Unit tests for the moon rise/set helpers (moon_tools.cpp).
 *
 * MoonRise::calculate() returns the nearest rise and the nearest set to
 * the query time independently: when the query time falls between today's
 * moonset and tomorrow's moonrise, the returned set precedes the rise
 * (roughly every other 3h sample of a 60-day sweep). getMoonState() must
 * always return a moonset that follows the moonrise, pairing the rise with
 * the set of the same lunar period.
 *
 * The sweep covers moderate latitudes (ordering checks) and lat 70
 * (polar stretches where the moon stays up for days at a time).
 *
 * GPL-3.0, see LICENSE.
 */

#include <time.h>
#include <MoonRise.h>
#include <unity.h>

#include "moon_tools.h"

static const float kLatitudes[] = {0.0f, 45.0f, 70.0f};
static const size_t kNumLatitudes = sizeof(kLatitudes) / sizeof(kLatitudes[0]);
static const time_t kDay = 86400;
static const time_t kStart = 1785542400L;  // 2026-08-01 00:00:00 UTC

/* Sweeps latitudes x query times and verifies that getMoonState() never
 * pairs a moonset before its moonrise, and that the pair is not absurdly
 * far apart (longer than the walk cap). */
void test_moonset_after_moonrise(void) {
  size_t checked = 0;
  for (size_t latIdx = 0; latIdx < kNumLatitudes; ++latIdx) {
    float lat = kLatitudes[latIdx];
    for (time_t t = kStart; t < kStart + 60 * kDay; t += 6 * 3600) {
      moon_state_t moon = getMoonState(lat, 0.0f, t);
      if (moon.moonrise == 0 || moon.moonset == 0) {
        continue;  // polar periods without events are legitimate
      }
      ++checked;
      TEST_ASSERT_TRUE_MESSAGE(moon.moonset > moon.moonrise,
                               "moonset must follow moonrise");
      TEST_ASSERT_TRUE_MESSAGE(moon.moonset - moon.moonrise < 16 * kDay,
                               "moonset must be reachable by the search walk");
    }
  }
  // Make sure the sweep actually exercised the ordering guarantee.
  TEST_ASSERT_TRUE_MESSAGE(checked > 500, "sweep found too few rise/set pairs");
}

/* Raw MoonRise::calculate() returns set-before-rise pairs roughly half the
 * time; every such pair must come back correctly ordered (and with the same
 * moonrise) from getMoonState(). Guarded by counting raw inversions so the
 * test fails if the sweep stops exercising the buggy case. */
void test_inverted_pairs_get_fixed(void) {
  MoonRise mr;
  size_t inverted = 0;
  for (size_t latIdx = 0; latIdx < kNumLatitudes; ++latIdx) {
    float lat = kLatitudes[latIdx];
    for (time_t t = kStart; t < kStart + 60 * kDay; t += 6 * 3600) {
      mr.calculate(lat, 0.0, t);
      if (!mr.hasRise || !mr.hasSet || mr.setTime > mr.riseTime) {
        continue;
      }
      ++inverted;
      moon_state_t moon = getMoonState(lat, 0.0f, t);
      TEST_ASSERT_TRUE_MESSAGE(moon.moonrise == mr.riseTime,
                               "moonrise must not be altered");
      TEST_ASSERT_TRUE_MESSAGE(moon.moonset > moon.moonrise,
                               "inverted raw pair must be corrected");
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(inverted > 50, "sweep found too few inverted pairs");
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_moonset_after_moonrise);
  RUN_TEST(test_inverted_pairs_get_fixed);
  UNITY_END();
}

void loop() {}