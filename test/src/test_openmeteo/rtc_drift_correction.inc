/* Unit tests for the RTC slow-clock drift auto-correction (time_utils.cpp).
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <cmath>
#include <cstdint>
#include <unity.h>

#include "time_utils.h"
#include "../test_harness.h"

namespace rtc_drift_correction_tests {

void setUp(void) {}
void tearDown(void) {}

using namespace rtc_drift;

// ------------------------------------------------------------------ helpers

static void assertNear(double actual, double expected, double relTol) {
  char msg[128];
  snprintf(msg, sizeof(msg), "expected %.10f, got %.10f", expected, actual);
  TEST_ASSERT_TRUE_MESSAGE(std::fabs(actual - expected) <= relTol * (std::fabs(expected) + 1.0), msg);
}

/* Emulate one NTP-synchronized sleep cycle: `realUs` real time elapsed while
 * the slow clock ran at trueFactor relative to the awake calibration, so the
 * RTC reported realUs / trueFactor. Returns the learned-factor update. */
static double oneCycle(double k, double trueFactor, double realUs) {
  const double rtcUs = realUs / trueFactor;
  const double rateError = (rtcUs - realUs) / realUs;
  return updateFactor(k, rateError);
}

// --------------------------------------------------------------------- tests

/* Scaling must be a no-op for k == 1, non-positive k and zero duration. */
void test_scale_identity(void) {
  const uint64_t us = 1800ULL * 1000000ULL;  // 30 min
  TEST_ASSERT_EQUAL_UINT64(us, scaleSleepUs(us, 1.0));
  TEST_ASSERT_EQUAL_UINT64(us, scaleSleepUs(us, 0.0));
  TEST_ASSERT_EQUAL_UINT64(us, scaleSleepUs(us, -1.0));
  TEST_ASSERT_EQUAL_UINT64(0, scaleSleepUs(0, 0.99));
  TEST_ASSERT_EQUAL_UINT64(0, scaleSleepUs(0, 1.0));
}

/* Scaling by 1/k restores the real-time duration: scaled * k ~= original. */
void test_scale_ratio(void) {
  const uint64_t us = 1800ULL * 1000000ULL;
  for (int i = 0; i <= 20; ++i) {
    const double k = RTC_DRIFT_MIN_FACTOR + (double) i * (RTC_DRIFT_MAX_FACTOR - RTC_DRIFT_MIN_FACTOR) / 20.0;
    const uint64_t scaled = scaleSleepUs(us, k);
    const double restored = (double) scaled * k;
    assertNear(restored, (double) us, 1e-6);
    TEST_ASSERT_TRUE_MESSAGE(scaled != 0, "scaled duration must stay non-zero");
  }
}

/* Monotonic: slower sleep clock (smaller k) requires a longer request. */
void test_scale_monotonic(void) {
  const uint64_t us = 60ULL * 1000000ULL;
  uint64_t prev = 0;
  for (int i = 20; i >= 0; --i) {
    const double k = RTC_DRIFT_MIN_FACTOR + (double) i * (RTC_DRIFT_MAX_FACTOR - RTC_DRIFT_MIN_FACTOR) / 20.0;
    const uint64_t scaled = scaleSleepUs(us, k);
    TEST_ASSERT_TRUE_MESSAGE(scaled >= prev, "scaled duration must be monotonic in 1/k");
    prev = scaled;
  }
}

/* The post-wake shift must bring the wall clock back to the real elapsed
 * time: claimed + shift == claimed * k == original desired duration. */
void test_wake_shift_consistency(void) {
  const uint64_t us = 10ULL * 3600ULL * 1000000ULL;  // 10 h, critical case
  for (int i = 0; i <= 20; ++i) {
    const double k = RTC_DRIFT_MIN_FACTOR + (double) i * (RTC_DRIFT_MAX_FACTOR - RTC_DRIFT_MIN_FACTOR) / 20.0;
    const uint64_t claimed = scaleSleepUs(us, k);
    const int64_t shift = wakeShiftUs(claimed, k);
    const double corrected = (double) claimed + (double) shift;
    assertNear(corrected, (double) us, 1e-6);
  }
}

/* Absurd factors must not produce absurd shifts (clamp at 10%). */
void test_wake_shift_clamp(void) {
  const uint64_t claimed = 3600ULL * 1000000ULL;  // 1 h
  const int64_t bound = (int64_t) ((double) claimed * RTC_DRIFT_MAX_SHIFT_RATIO);

  const int64_t shiftSlow = wakeShiftUs(claimed, 0.5);
  TEST_ASSERT_TRUE_MESSAGE(shiftSlow < 0 && shiftSlow >= -bound, "slow-clock shift must be clamped");

  const int64_t shiftFast = wakeShiftUs(claimed, 1.5);
  TEST_ASSERT_TRUE_MESSAGE(shiftFast > 0 && shiftFast <= bound, "fast-clock shift must be clamped");

  TEST_ASSERT_EQUAL_INT64(0, wakeShiftUs(claimed, 1.0));
  TEST_ASSERT_EQUAL_INT64(0, wakeShiftUs(0, 0.99));
}

/* Sample factor: k = 1 / (1 + rateError). */
void test_sample_factor(void) {
  assertNear(sampleFactor(0.0), 1.0, 1e-9);
  assertNear(sampleFactor(0.01), 0.9900990099, 1e-9);
  assertNear(sampleFactor(-0.01), 1.0101010101, 1e-9);
}

/* EMA update with alpha = RTC_DRIFT_LEARN_ALPHA. */
void test_update_factor_ema(void) {
  const double sample = sampleFactor(0.01);  // 0.990099...
  const double expected = 1.0 + RTC_DRIFT_LEARN_ALPHA * (sample - 1.0);
  assertNear(updateFactor(1.0, 0.01), expected, 1e-9);

  // k converges towards the sample factor.
  const double next = updateFactor(expected, 0.01);
  assertNear(next, expected + RTC_DRIFT_LEARN_ALPHA * (sample - expected), 1e-9);
  TEST_ASSERT_TRUE_MESSAGE(std::fabs(next - sample) < std::fabs(expected - sample), "EMA must move towards the sample");
}

/* The learned factor is clamped to [RTC_DRIFT_MIN_FACTOR, RTC_DRIFT_MAX_FACTOR]. */
void test_update_factor_clamp(void) {
  assertNear(updateFactor(RTC_DRIFT_MIN_FACTOR - 0.1, 0.0), RTC_DRIFT_MIN_FACTOR, 1e-12);
  assertNear(updateFactor(RTC_DRIFT_MAX_FACTOR + 0.1, 0.0), RTC_DRIFT_MAX_FACTOR, 1e-12);
}

/* Full learning loop: with a constant true slow-clock ratio (e.g. 0.9932,
 * like the reference sketch's hand-calibrated value), the learned factor
 * must converge to it and the scaled sleep durations must wake on time. */
void test_learning_convergence(void) {
  const double kTrue = 0.9932;
  const double realUs = 1800.0 * 1000000.0;  // 30 min real time per cycle
  double k = 1.0;

  for (int i = 0; i < 32; ++i) {
    k = oneCycle(k, kTrue, realUs);
  }
  assertNear(k, kTrue, 1e-3);

  // With the learned factor, the scaled request wakes after the desired real
  // duration and the corrected wall clock shows the desired elapsed time.
  const uint64_t desiredUs = 3600ULL * 1000000ULL;
  const uint64_t claimed = scaleSleepUs(desiredUs, k);
  const int64_t shift = wakeShiftUs(claimed, k);

  const double realElapsed = (double) claimed * kTrue;
  assertNear(realElapsed, (double) desiredUs, 1e-4);
  assertNear((double) claimed + (double) shift, (double) desiredUs, 1e-4);
}

// ------------------------------------------------------------------ driver

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(rtc_drift_correction_tests::test_scale_identity);
  RUN_TEST(rtc_drift_correction_tests::test_scale_ratio);
  RUN_TEST(rtc_drift_correction_tests::test_scale_monotonic);
  RUN_TEST(rtc_drift_correction_tests::test_wake_shift_consistency);
  RUN_TEST(rtc_drift_correction_tests::test_wake_shift_clamp);
  RUN_TEST(rtc_drift_correction_tests::test_sample_factor);
  RUN_TEST(rtc_drift_correction_tests::test_update_factor_ema);
  RUN_TEST(rtc_drift_correction_tests::test_update_factor_clamp);
  RUN_TEST(rtc_drift_correction_tests::test_learning_convergence);
}

}  // namespace rtc_drift_correction_tests
