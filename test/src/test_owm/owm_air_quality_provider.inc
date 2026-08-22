/* Unit tests for the OpenWeatherMap Air Pollution provider.
 *
 * The deserializer is exercised with the real Arduino String/Stream inside the
 * ESP32 QEMU emulator. The captured fixture is a response from the OWM
 * history endpoint; synthetic responses cover bounds, missing values, and
 * parser failures. Tests never access the network.
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "owm_air_quality_provider.h"
#include "owm_air_quality_real.inc"
#include "../string_stream.h"
#include "../test_harness.h"
#include "_locale.h"

namespace owm_air_quality_tests {

void setUp(void) {}
void tearDown(void) {}

static ProviderResult parseJson(const String &json, air_quality_t &airQuality) {
  StringStream ss(json);
  return OWMAirQualityProvider::deserializeAirQuality(ss, airQuality);
}

/* Minimal OWM-shaped response with distinguishable values for every
 * component. The OWM `main.aqi` value is intentionally present but is not
 * part of the provider-agnostic air-quality model. */
static String makeSyntheticJson(size_t count) {
  static const char *componentNames[] = {"co", "no", "no2", "o3", "so2", "pm2_5", "pm10", "nh3"};
  static const int componentMultipliers[] = {2, 3, 5, 7, 11, 13, 17, 19};

  String json = "{\"coord\":{\"lon\":30.5,\"lat\":50.5},\"list\":[";
  for (size_t i = 0; i < count; ++i) {
    if (i > 0)
      json += ",";
    json += "{\"main\":{\"aqi\":2},\"components\":{";
    for (size_t c = 0; c < sizeof(componentNames) / sizeof(componentNames[0]); ++c) {
      if (c > 0)
        json += ",";
      json += "\"";
      json += componentNames[c];
      json += "\":";
      json += String(componentMultipliers[c] * static_cast<int>(i));
    }
    json += "},\"dt\":";
    json += String(static_cast<int64_t>(1700000000LL + i * 3600));
    json += "}";
  }
  json += "]}";
  return json;
}

static void test_real_fixture_mapping(void) {
  air_quality_t airQuality = {};
  ProviderResult result = parseJson(kOwmAirQualityReal, airQuality);
  TEST_ASSERT_TRUE(result.isOk());

  TEST_ASSERT_EQUAL_INT64(1787346000LL, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(1787428800LL, airQuality.dt[NUM_AIR_POLLUTION - 1]);
  for (int i = 0; i < NUM_AIR_POLLUTION - 1; ++i) {
    TEST_ASSERT_EQUAL_INT64(airQuality.dt[i] + 3600, airQuality.dt[i + 1]);
  }

  // First entry: verify every pollutant field from the live-shaped response.
  TEST_ASSERT_EQUAL_FLOAT(113.26f, airQuality.components.co[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.no[0]);
  TEST_ASSERT_EQUAL_FLOAT(1.45f, airQuality.components.no2[0]);
  TEST_ASSERT_EQUAL_FLOAT(80.97f, airQuality.components.o3[0]);
  TEST_ASSERT_EQUAL_FLOAT(2.89f, airQuality.components.so2[0]);
  TEST_ASSERT_EQUAL_FLOAT(4.29f, airQuality.components.pm2_5[0]);
  TEST_ASSERT_EQUAL_FLOAT(5.21f, airQuality.components.pm10[0]);
  TEST_ASSERT_EQUAL_FLOAT(2.72f, airQuality.components.nh3[0]);

  // A later entry verifies that list order is retained and values are not
  // accidentally taken from only the first response object.
  TEST_ASSERT_EQUAL_INT64(1787392800LL, airQuality.dt[13]);
  TEST_ASSERT_EQUAL_FLOAT(101.82f, airQuality.components.co[13]);
  TEST_ASSERT_EQUAL_FLOAT(0.1f, airQuality.components.no[13]);
  TEST_ASSERT_EQUAL_FLOAT(0.56f, airQuality.components.no2[13]);
  TEST_ASSERT_EQUAL_FLOAT(100.85f, airQuality.components.o3[13]);
  TEST_ASSERT_EQUAL_FLOAT(3.46f, airQuality.components.so2[13]);
  TEST_ASSERT_EQUAL_FLOAT(3.6f, airQuality.components.pm2_5[13]);
  TEST_ASSERT_EQUAL_FLOAT(4.27f, airQuality.components.pm10[13]);
  TEST_ASSERT_EQUAL_FLOAT(0.89f, airQuality.components.nh3[13]);
}

static void test_more_entries_than_model(void) {
  air_quality_t airQuality = {};
  ProviderResult result = parseJson(makeSyntheticJson(NUM_AIR_POLLUTION + 6), airQuality);
  TEST_ASSERT_TRUE(result.isOk());

  TEST_ASSERT_EQUAL_INT64(1700000000LL, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(1700000000LL + (NUM_AIR_POLLUTION - 1) * 3600LL, airQuality.dt[NUM_AIR_POLLUTION - 1]);
  TEST_ASSERT_EQUAL_FLOAT(2.0f * (NUM_AIR_POLLUTION - 1), airQuality.components.co[NUM_AIR_POLLUTION - 1]);
  TEST_ASSERT_EQUAL_FLOAT(19.0f * (NUM_AIR_POLLUTION - 1), airQuality.components.nh3[NUM_AIR_POLLUTION - 1]);
}

static void test_fewer_entries_clear_remaining_slots(void) {
  air_quality_t airQuality = {};
  for (int i = 0; i < NUM_AIR_POLLUTION; ++i) {
    airQuality.dt[i] = 123456;
    airQuality.components.pm2_5[i] = 99.0f;
  }

  ProviderResult result = parseJson(makeSyntheticJson(3), airQuality);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1700000000LL, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_INT64(1700007200LL, airQuality.dt[2]);
  TEST_ASSERT_EQUAL_FLOAT(26.0f, airQuality.components.pm2_5[2]);
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[3]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[3]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.co[3]);
}

static void test_missing_and_null_components(void) {
  air_quality_t airQuality = {};
  const char *json =
      "{\"list\":["
      "{\"main\":{\"aqi\":1},\"components\":{"
      "\"co\":1.5,\"no\":null,\"no2\":2.5,\"o3\":3.5,\"so2\":4.5,\"pm2_5\":5.5,\"pm10\":6.5,\"nh3\":7.5},\"dt\":1000},"
      "{\"components\":{\"pm2_5\":null,\"o3\":9.0},\"dt\":2000}]}";

  ProviderResult result = parseJson(json, airQuality);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(1.5f, airQuality.components.co[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.no[0]);
  TEST_ASSERT_EQUAL_FLOAT(7.5f, airQuality.components.nh3[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.co[1]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[1]);
  TEST_ASSERT_EQUAL_FLOAT(9.0f, airQuality.components.o3[1]);
}

static void test_unknown_fields(void) {
  air_quality_t airQuality = {};
  const char *json = "{\"metadata\":{\"list\":[999]},\"list\":["
                     "{\"unknown\":true,\"components\":{\"co\":12.5,\"future_pollutant\":88},\"dt\":1000}]}";

  ProviderResult result = parseJson(json, airQuality);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1000, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, airQuality.components.co[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.no2[0]);
}

static void test_empty_or_missing_list_clears_model(void) {
  air_quality_t airQuality = {};
  airQuality.dt[0] = 123456;
  airQuality.components.o3[0] = 42.0f;

  ProviderResult result = parseJson("{}", airQuality);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[0]);

  airQuality.dt[0] = 123456;
  airQuality.components.o3[0] = 42.0f;
  result = parseJson("{\"list\":[]}", airQuality);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[0]);
}

static void test_invalid_json_clears_model(void) {
  air_quality_t airQuality = {};
  airQuality.dt[0] = 123456;
  airQuality.components.pm2_5[0] = 42.0f;

  ProviderResult result = parseJson("this is not json", airQuality);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_TRUE(result.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.pm2_5[0]);
}

static void test_truncated_json_clears_model(void) {
  air_quality_t airQuality = {};
  airQuality.dt[0] = 123456;
  airQuality.components.o3[0] = 42.0f;

  ProviderResult result = parseJson("{\"list\":[{\"components\":{\"o3\":17.5},\"dt\":1000}", airQuality);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT, result.detail().c_str());
  TEST_ASSERT_EQUAL_INT64(0, airQuality.dt[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, airQuality.components.o3[0]);
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(owm_air_quality_tests::test_real_fixture_mapping);
  RUN_TEST(owm_air_quality_tests::test_more_entries_than_model);
  RUN_TEST(owm_air_quality_tests::test_fewer_entries_clear_remaining_slots);
  RUN_TEST(owm_air_quality_tests::test_missing_and_null_components);
  RUN_TEST(owm_air_quality_tests::test_unknown_fields);
  RUN_TEST(owm_air_quality_tests::test_empty_or_missing_list_clears_model);
  RUN_TEST(owm_air_quality_tests::test_invalid_json_clears_model);
  RUN_TEST(owm_air_quality_tests::test_truncated_json_clears_model);
}

}  // namespace owm_air_quality_tests
