/* Offline streaming tests for the OpenWeatherMap One Call v4 provider.
 * The Stockholm payloads are deliberately small synthetic captures: they
 * preserve the v4 response shape while containing no credentials.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include <memory>
#include <vector>

#include "data_models.h"
#include "fetch_operation.h"
#include "owm_v4_provider.h"
#include "../string_stream.h"
#include "../test_harness.h"

namespace owm_v4_tests {

void setUp(void) {}
void tearDown(void) {}

static String currentJson() {
  return R"json({
    "lat":59.329323,"lon":18.068580,"timezone":"Europe/Stockholm","timezone_offset":3600,
    "data":[{"dt":1700000000,"temp":4.5,"feels_like":1.2,"pressure":1012,"humidity":81,
      "dew_point":1.4,"uvi":0.3,"clouds":75,"visibility":9000,"wind_speed":4.1,"wind_deg":210,
      "wind_gust":7.2,"rain":{"1h":0.6},"snow":{"1h":0.0},
      "weather":[{"id":501,"icon":"10n"}]}],"alert_id":[]})json";
}

static String hourlyJson(size_t count, int64_t firstDt) {
  String json = "{\"lat\":59.329323,\"lon\":18.068580,\"data\":[";
  for (size_t i = 0; i < count; ++i) {
    if (i != 0)
      json += ",";
    json += "{\"dt\":" + String(static_cast<long long>(firstDt + i * 3600)) +
            ",\"temp\":4.5,\"feels_like\":2.0,\"pressure\":1000,\"humidity\":80,"
            "\"dew_point\":1.0,\"uvi\":0.5,\"clouds\":60,\"visibility\":8000,"
            "\"wind_speed\":3.0,\"wind_deg\":180,\"wind_gust\":5.0,\"pop\":0.42,"
            "\"rain\":{\"1h\":0.4},\"snow\":{\"1h\":0.0},"
            "\"weather\":[{\"id\":800,\"icon\":\"01d\"}]}";
  }
  json += "]}";
  return json;
}

static String dailyJson(size_t count, int64_t firstDt) {
  String json = "{\"lat\":59.329323,\"lon\":18.068580,\"timezone\":\"Europe/Stockholm\",\"data\":[";
  for (size_t i = 0; i < count; ++i) {
    if (i != 0)
      json += ",";
    json += "{\"dt\":" + String(static_cast<long long>(firstDt + i * 86400)) +
            ",\"temp\":{\"morn\":1,\"day\":5,\"eve\":3,\"night\":0,\"min\":-1,\"max\":6},"
            "\"pressure\":1015,\"humidity\":75,\"dew_point\":0,\"clouds\":40,\"uvi\":1.5,"
            "\"wind_speed\":2.5,\"wind_deg\":190,\"wind_gust\":4.0,\"pop\":0.25,"
            "\"rain\":2.0,\"snow\":0,\"weather\":[{\"id\":601}]}";
  }
  json += "]}";
  return json;
}

static void test_current_mapping_and_root_termination(void) {
  String payload = currentJson() + "TRAILING";
  StringStream stream(payload);
  forecast_t forecast;
  ProviderResult result = OpenWeatherMapOneCallV4Provider::deserializeCurrent(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 59.329323f, forecast.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 18.068580f, forecast.lon);
  TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", forecast.timezone.c_str());
  TEST_ASSERT_EQUAL_INT(3600, forecast.timezone_offset);
  TEST_ASSERT_EQUAL_INT64(1700000000, forecast.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(4.5f, forecast.current.temp);
  TEST_ASSERT_EQUAL_INT(81, forecast.current.humidity.value());
  TEST_ASSERT_EQUAL_FLOAT(0.6f, forecast.current.rain_1h);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.snow_1h);
  TEST_ASSERT_EQUAL(weather_condition::RAIN, forecast.current.weather.condition);
  TEST_ASSERT_FALSE(forecast.current.is_day);
  TEST_ASSERT_TRUE(stream.bytesRead() < payload.length());
}

static void test_hourly_pages_map_and_merge(void) {
  forecast_t forecast;
  String first = hourlyJson(20, 1700000000);
  StringStream firstStream(first);
  TEST_ASSERT_TRUE(OpenWeatherMapOneCallV4Provider::deserializeHourly(firstStream, forecast).isOk());

  String continuation = hourlyJson(4, 1700000000 + 20 * 3600);
  StringStream continuationStream(continuation);
  TEST_ASSERT_TRUE(OpenWeatherMapOneCallV4Provider::deserializeHourly(continuationStream, forecast, 20).isOk());

  TEST_ASSERT_EQUAL_INT64(1700000000 + 23 * 3600, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL_FLOAT(4.5f, forecast.hourly[20].temp);
  TEST_ASSERT_EQUAL_INT(42, forecast.hourly[0].pop);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, forecast.hourly[0].weather.condition);
  TEST_ASSERT_TRUE(forecast.hourly[0].is_day);
  TEST_ASSERT_TRUE(forecast.hourly[0].visibility.has_value());
}

static void test_daily_mapping_and_missing_values_reset(void) {
  forecast_t forecast;
  forecast.daily[0].rain = 99.0f;
  forecast.daily[0].visibility = 123;
  String payload = dailyJson(NUM_DAILY + 3, 1700000000);
  StringStream stream(payload);
  ProviderResult result = OpenWeatherMapOneCallV4Provider::deserializeDaily(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1700000000 + 4 * 86400, forecast.daily[4].dt);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, forecast.daily[0].temp.morn);
  TEST_ASSERT_EQUAL_FLOAT(6.0f, forecast.daily[0].temp.max.value());
  TEST_ASSERT_EQUAL_INT(25, forecast.daily[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, forecast.daily[0].rain);
  TEST_ASSERT_FALSE(forecast.daily[0].visibility.has_value());
  TEST_ASSERT_EQUAL(weather_condition::SNOW, forecast.daily[0].weather.condition);
}

static void test_invalid_empty_truncated_and_missing_data(void) {
  forecast_t forecast;
  forecast.current.temp = 88.0f;
  StringStream empty("");
  TEST_ASSERT_FALSE(OpenWeatherMapOneCallV4Provider::deserializeCurrent(empty, forecast).isOk());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);

  StringStream invalid("not json");
  TEST_ASSERT_FALSE(OpenWeatherMapOneCallV4Provider::deserializeDaily(invalid, forecast).isOk());
  StringStream truncated("{\"data\":[{\"dt\":1}");
  TEST_ASSERT_FALSE(OpenWeatherMapOneCallV4Provider::deserializeHourly(truncated, forecast).isOk());
  StringStream missing("{\"error\":\"bad key\"}");
  TEST_ASSERT_FALSE(OpenWeatherMapOneCallV4Provider::deserializeCurrent(missing, forecast).isOk());
}

static void test_factory_creates_required_operations(void) {
  weather_report_t report;
  report.forecast.current.temp = 77.0f;
  OpenWeatherMapOneCallV4Provider provider;
  std::vector<std::unique_ptr<FetchOperation>> operations = provider.createFetchOperations(report);

  TEST_ASSERT_EQUAL_UINT(3, operations.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, report.forecast.current.temp);
  for (const auto &operation : operations)
    TEST_ASSERT_TRUE(operation->shouldAbortOnFailure());
}

static void test_condition_mapping(void) {
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OpenWeatherMapOneCallV4Provider::mapWeatherCode(211));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OpenWeatherMapOneCallV4Provider::mapWeatherCode(301));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OpenWeatherMapOneCallV4Provider::mapWeatherCode(616));
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, OpenWeatherMapOneCallV4Provider::mapWeatherCode(804));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenWeatherMapOneCallV4Provider::mapWeatherCode(999));
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(owm_v4_tests::test_current_mapping_and_root_termination);
  RUN_TEST(owm_v4_tests::test_hourly_pages_map_and_merge);
  RUN_TEST(owm_v4_tests::test_daily_mapping_and_missing_values_reset);
  RUN_TEST(owm_v4_tests::test_invalid_empty_truncated_and_missing_data);
  RUN_TEST(owm_v4_tests::test_factory_creates_required_operations);
  RUN_TEST(owm_v4_tests::test_condition_mapping);
}

}  // namespace owm_v4_tests
