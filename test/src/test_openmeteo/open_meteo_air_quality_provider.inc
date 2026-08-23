/* Unit tests for the Open-Meteo air quality provider (response mapping).
 *
 * The fixtures are exercised with the real Arduino String/Stream inside the
 * ESP32 QEMU emulator. The deserializer picks a window of up to
 * NUM_AIR_POLLUTION hourly entries ending at the closest timestamp at or
 * before `now` (time(nullptr)), so every test pins the emulated system clock
 * with settimeofday() first. The primary fixture
 * (open_meteo_air_quality_real.inc) is a verbatim excerpt of the live
 * Open-Meteo air quality API response for Lima, Peru, captured on 2026-08-19
 * (negative lat/lon, ammonia values null in the API response).
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include <sys/time.h>
#include <time.h>

#include "../string_stream.h"
#include "_locale.h"
#include "data_models.h"
#include "open_meteo_air_quality_provider.h"
#include "open_meteo_air_quality_real.inc"
#include "../test_harness.h"

namespace open_meteo_air_quality_tests {

// Frozen epochs of the Lima fixture (2026-08-18/19, hourly stamps in UTC).
// `now` pinned to the fixture's hour 1787119200 (2026-08-19T06:00:00Z, the
// 30th hourly entry) selects the window from the 7th to the 30th entry.
static const int64_t kNow = 1787119200LL;
static const int64_t kWindowStart = 1787036400LL;  // hourly[7]
static const int64_t kWindowEnd = 1787119200LL;    // hourly[30]

// Base epoch for synthetic fixtures (2000-01-01T00:00:00Z); only relative
// distances to `now` matter for the window selection.
static const int64_t kSyntheticBase = 946684800LL;

static void setSystemTime(int64_t epoch) {
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

/* setUp runs before every test: without it, any test that forgot to pin the
 * clock would depend on the emulator's boot time. */
void setUp(void) { setSystemTime(kNow); }
void tearDown(void) {}

static ProviderResult parseJson(const String &json, air_quality_t &airQuality) {
  StringStream ss(json);
  return OpenMeteoAirQualityProvider::deserializeAirQuality(ss, airQuality);
}

/* Minimal synthetic response with `count` hourly entries. Every component is
 * i*multiplier µg/m^3, so each entry and each pollutant is distinguishable
 * from its index alone. */
static String makeSyntheticJson(size_t count) {
  String j = "{\"hourly\":{\"time\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(kSyntheticBase + (int64_t) i * 3600) + (i + 1 < count ? "," : "");
  }
  j += "],\"pm2_5\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(2 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"pm10\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(3 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"carbon_monoxide\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(4 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"nitrogen_monoxide\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(5 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"nitrogen_dioxide\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(6 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"ozone\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(7 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"sulphur_dioxide\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(8 * i) + (i + 1 < count ? "," : "");
  }
  j += "],\"ammonia\":[";
  for (size_t i = 0; i < count; ++i) {
    j += String(9 * i) + (i + 1 < count ? "," : "");
  }
  j += "]}}";
  return j;
}

/* Map the real Lima response with `now` hitting the 30th of 48 hourly
 * entries: the window must start at the 7th entry (closest_idx - 23) and
 * fill all 24 slots with consecutive hourly stamps. The ammonia values are
 * null in the API response and must map to 0. */
static void test_real_fixture_window(void) {
  setSystemTime(kNow);
  air_quality_t airQuality = {};
  ProviderResult err = parseJson(kOpenMeteoAirQualityReal, airQuality);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(kWindowStart, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(kWindowEnd, airQuality.dt[23]);
  for (int i = 0; i < 23; ++i) {
    TEST_ASSERT_EQUAL_INT64(airQuality.dt[i] + 3600, airQuality.dt[i + 1]);
  }

  // Slot 0 = hourly entry 7.
  TEST_ASSERT_EQUAL_FLOAT(12.9f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(18.4f, airQuality.components.pm10[0]);
  TEST_ASSERT_EQUAL_FLOAT(405.0f, airQuality.components.co[0]);
  TEST_ASSERT_EQUAL_FLOAT(11.7f, airQuality.components.no[0]);
  TEST_ASSERT_EQUAL_FLOAT(22.3f, airQuality.components.no2[0]);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, airQuality.components.o3[0]);
  TEST_ASSERT_EQUAL_FLOAT(8.3f, airQuality.components.so2[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.nh3[0]);

  // Slot 7 = hourly entry 14.
  TEST_ASSERT_EQUAL_INT64(1787061600LL, airQuality.dt[7]);
  TEST_ASSERT_EQUAL_FLOAT(203.0f, airQuality.components.co[7]);
  TEST_ASSERT_EQUAL_FLOAT(50.0f, airQuality.components.o3[7]);

  // Slot 12 = hourly entry 19.
  TEST_ASSERT_EQUAL_INT64(1787079600LL, airQuality.dt[12]);
  TEST_ASSERT_EQUAL_FLOAT(12.0f, airQuality.components.pm2_5[12]);
  TEST_ASSERT_EQUAL_FLOAT(76.1f, airQuality.components.pm10[12]);
  TEST_ASSERT_EQUAL_FLOAT(2.5f, airQuality.components.no2[12]);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, airQuality.components.o3[12]);
  TEST_ASSERT_EQUAL_FLOAT(1.7f, airQuality.components.so2[12]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.nh3[12]);

  // Slot 23 = hourly entry 30 = the hour at `now`.
  TEST_ASSERT_EQUAL_FLOAT(9.6f, airQuality.components.pm2_5[23]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.no[23]);
  TEST_ASSERT_EQUAL_FLOAT(15.0f, airQuality.components.no2[23]);
  TEST_ASSERT_EQUAL_FLOAT(38.0f, airQuality.components.o3[23]);
  TEST_ASSERT_EQUAL_FLOAT(4.2f, airQuality.components.so2[23]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.nh3[23]);
}

/* The response can carry more entries than the model holds: with `now` past
 * the last of 30 entries, the window covers the last 24 (closest_idx - 23),
 * never entries 0..5. */
static void test_more_entries_than_model(void) {
  setSystemTime(kSyntheticBase + 29 * 3600 + 100);
  air_quality_t airQuality = {};
  ProviderResult err = parseJson(makeSyntheticJson(30), airQuality);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 6 * 3600, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 29 * 3600, airQuality.dt[23]);
  TEST_ASSERT_EQUAL_FLOAT(12.0f, airQuality.components.pm2_5[0]);  // 2 * 6
  TEST_ASSERT_EQUAL_FLOAT(203.0f, airQuality.components.o3[23]);   // 7 * 29
}

/* An entry whose timestamp equals `now` is the boundary the window ends on
 * (ts <= now is inclusive), so the next entry is excluded. */
static void test_exact_now_boundary(void) {
  setSystemTime(kSyntheticBase + 24 * 3600);
  air_quality_t airQuality = {};
  ProviderResult err = parseJson(makeSyntheticJson(30), airQuality);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 1 * 3600, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 24 * 3600, airQuality.dt[23]);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(48.0f, airQuality.components.pm2_5[23]);
}

/* Fewer entries than the model holds: the remaining slots are reset instead
 * of retaining values from a previous response. */
static void test_fewer_entries_than_model(void) {
  setSystemTime(kSyntheticBase + 5 * 3600);
  air_quality_t airQuality = {};
  for (int i = 0; i < NUM_AIR_POLLUTION; ++i) {
    airQuality.dt[i] = 123456;
    airQuality.components.pm2_5[i] = 99.0f;
  }

  ProviderResult err = parseJson(makeSyntheticJson(5), airQuality);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(kSyntheticBase, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 4 * 3600, airQuality.dt[4]);
  TEST_ASSERT_EQUAL_FLOAT(8.0f, airQuality.components.pm2_5[4]);
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[5]);  // past the end
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[5]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm10[0]);  // absent key
}

/* `now` before every timestamp: no entry qualifies, the model stays empty
 * and the result is still Ok (no data to report). */
static void test_now_before_all_timestamps(void) {
  setSystemTime(kSyntheticBase - 3600);
  air_quality_t airQuality = {};
  ProviderResult err = parseJson(makeSyntheticJson(5), airQuality);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[0]);
}

/* A payload without an `hourly` section leaves the model empty but still
 * parses as Ok. An Open-Meteo {"error": ...} response is such a payload. */
static void test_non_air_quality_payloads(void) {
  air_quality_t airQuality = {};

  const char *errorResponse = "{\"error\":true,\"reason\":\"Latitude must be in range of (-90, 90]\"}";
  ProviderResult err = parseJson(errorResponse, airQuality);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[0]);

  err = parseJson("{}", airQuality);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.co[0]);
}

/* Entries with missing or null component values are accepted; the absent
 * concentrations must not crash and default to zero. */
static void test_missing_components(void) {
  air_quality_t airQuality = {};
  const char *json = "{\"hourly\":{\"time\":[1000,2000,3000],\"pm2_5\":[1.5,null,3.5],\"ozone\":[4.0,5.0]}}";
  ProviderResult err = parseJson(json, airQuality);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(1000, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(1.5f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[1]);  // null
  TEST_ASSERT_EQUAL_FLOAT(3.5f, airQuality.components.pm2_5[2]);
  TEST_ASSERT_EQUAL_FLOAT(4.0f, airQuality.components.o3[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[2]);  // missing
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.co[0]);  // absent key
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.nh3[3]);
}

/* Unknown sections and columns are consumed without affecting the selected
 * pollutant values. Nested arrays are ignored because only direct children of
 * `hourly` are data series. */
static void test_unknown_fields(void) {
  setSystemTime(kSyntheticBase + 2 * 3600 + 100);
  air_quality_t airQuality = {};
  const char *json = "{\"metadata\":{\"time\":[9999]},\"hourly\":{\"time\":[946684800,946688400,946692000],"
                     "\"unknown\":[90,91,92],\"nested\":{\"ozone\":[80,81,82]},"
                     "\"pm2_5\":[1,2,3],\"ozone\":[4,5,6]}}";

  ProviderResult err = parseJson(json, airQuality);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_INT64(kSyntheticBase + 2 * 3600, airQuality.dt[2]);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, airQuality.components.pm2_5[2]);
  TEST_ASSERT_EQUAL_FLOAT(4.0f, airQuality.components.o3[0]);
  TEST_ASSERT_EQUAL_FLOAT(6.0f, airQuality.components.o3[2]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.co[0]);
}

/* Garbage and empty input are reported as deserialization errors, and a
 * failed parse clears a destination that already held readings. */
static void test_invalid_json(void) {
  air_quality_t airQuality = {};
  airQuality.components.pm2_5[0] = 42.0f;
  airQuality.dt[0] = 123456;

  ProviderResult err = parseJson("this is not json", airQuality);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_TRUE(err.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);

  err = parseJson("", airQuality);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT, err.detail().c_str());
}

/* A valid prefix that ends before the root document closes is incomplete, not
 * a successful partial response, and must not leave any parsed values behind. */
static void test_truncated_json(void) {
  air_quality_t airQuality = {};
  airQuality.components.o3[0] = 17.0f;
  airQuality.dt[0] = 987654;

  ProviderResult err = parseJson("{\"hourly\":{\"time\":[1000],\"ozone\":[4.5]", airQuality);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT, err.detail().c_str());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[0]);
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(open_meteo_air_quality_tests::test_real_fixture_window);
  RUN_TEST(open_meteo_air_quality_tests::test_more_entries_than_model);
  RUN_TEST(open_meteo_air_quality_tests::test_exact_now_boundary);
  RUN_TEST(open_meteo_air_quality_tests::test_fewer_entries_than_model);
  RUN_TEST(open_meteo_air_quality_tests::test_now_before_all_timestamps);
  RUN_TEST(open_meteo_air_quality_tests::test_non_air_quality_payloads);
  RUN_TEST(open_meteo_air_quality_tests::test_missing_components);
  RUN_TEST(open_meteo_air_quality_tests::test_unknown_fields);
  RUN_TEST(open_meteo_air_quality_tests::test_invalid_json);
  RUN_TEST(open_meteo_air_quality_tests::test_truncated_json);
}

}  // namespace open_meteo_air_quality_tests
