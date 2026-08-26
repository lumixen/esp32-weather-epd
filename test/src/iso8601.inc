/* Unit tests for the shared ISO 8601 timestamp parser.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <cstdint>
#include <unity.h>

#include "iso8601.h"

namespace iso8601_tests {

static void test_common_timestamps() {
  int64_t epoch = 0;
  TEST_ASSERT_TRUE(iso8601::parse("2026-08-25T15:00:00-05:00", epoch));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, epoch);
  TEST_ASSERT_TRUE(iso8601::parse("2026-08-25T20:00:00Z", epoch));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, epoch);
  TEST_ASSERT_TRUE(iso8601::parse("2026-08-25T22:00:00+02:00", epoch));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, epoch);
  TEST_ASSERT_TRUE(iso8601::parse("2026-08-25t20:00:00z", epoch));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, epoch);
  TEST_ASSERT_TRUE(iso8601::parse("2026-08-25T20:00:00.123456Z", epoch));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, epoch);
  TEST_ASSERT_TRUE(iso8601::parse("2024-02-29T00:00:00Z", epoch));
  TEST_ASSERT_EQUAL_INT64(1709164800LL, epoch);
}

static void test_invalid_timestamps() {
  int64_t epoch = 0;
  TEST_ASSERT_FALSE(iso8601::parse(nullptr, epoch));
  TEST_ASSERT_FALSE(iso8601::parse("", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00X", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00+02", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00+02:0", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-02-29T20:00:00Z", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-13-01T20:00:00Z", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T24:00:00Z", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:60:00Z", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00.Z", epoch));
  TEST_ASSERT_FALSE(iso8601::parse("2026-08-25T20:00:00Ztrailing", epoch));
}

void registerTests() {
  RUN_TEST(test_common_timestamps);
  RUN_TEST(test_invalid_timestamps);
}

}  // namespace iso8601_tests
