/* Unit tests for the OpenWeatherMap One Call weather and alert provider.
 *
 * The real Zaragoza response is parsed through the public deserializer seams
 * using the same Arduino String/Stream types as the firmware. Synthetic
 * responses cover bounds, optional values, and parser failures. Tests never
 * access the network.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "owm_provider.h"
#include "owm_provider_real.inc"
#include "../string_stream.h"
#include "../test_harness.h"
#include "_locale.h"

namespace owm_provider_tests {

void setUp(void) {}
void tearDown(void) {}

static ProviderResult parseOneCall(const String &json, forecast_t &forecast,
                                   std::vector<weather_alert_t> *alerts = nullptr) {
  StringStream ss(json);
  weather_report_t report;
  ProviderResult result = OWMProvider::deserializeOneCall(ss, report);
  forecast = report.forecast;
  if (alerts != nullptr) {
    alerts->clear();
    if (report.alerts) {
      *alerts = *report.alerts;
    }
  }
  return result;
}

static ProviderResult parseAlerts(const String &json, std::vector<weather_alert_t> &alerts) {
  StringStream ss(json);
  weather_report_t report;
  ProviderResult result = OWMProvider::deserializeOneCall(ss, report);
  alerts.clear();
  if (report.alerts) {
    alerts = *report.alerts;
  }
  return result;
}

static String makeOneCallJson(size_t hours, size_t days) {
  String json = "{\"lat\":41.6488,\"lon\":-0.8891,\"timezone\":\"Europe/Madrid\",\"timezone_offset\":7200,"
                "\"current\":{\"dt\":1700000000,\"sunrise\":1699990000,\"sunset\":1700030000,"
                "\"temp\":20.5,\"feels_like\":20.0,\"pressure\":1012,\"humidity\":55,\"dew_point\":11.0,"
                "\"clouds\":25,\"uvi\":2.5,\"visibility\":10000,\"wind_speed\":3.5,\"wind_gust\":5.5,"
                "\"wind_deg\":180,\"rain\":{\"1h\":1.25},\"snow\":{\"1h\":0.0},"
                "\"weather\":[{\"id\":801,\"icon\":\"02d\"}]},\"hourly\":[";
  for (size_t i = 0; i < hours; ++i) {
    if (i > 0)
      json += ",";
    json += "{\"dt\":" + String(static_cast<int64_t>(1800000000LL + i * 3600)) +
            ",\"temp\":" + String(10.0f + static_cast<float>(i)) +
            ",\"feels_like\":12,\"pressure\":1000,\"humidity\":50,\"dew_point\":5,"
            "\"clouds\":10,\"uvi\":1,\"visibility\":9000,\"wind_speed\":2,\"wind_gust\":3,"
            "\"wind_deg\":90,\"pop\":0.25,\"rain\":{\"1h\":0.5},\"snow\":{\"1h\":0},"
            "\"weather\":[{\"id\":500,\"icon\":\"10n\"}]}";
  }
  json += "],\"daily\":[";
  for (size_t i = 0; i < days; ++i) {
    if (i > 0)
      json += ",";
    json += "{\"dt\":" + String(static_cast<int64_t>(1900000000LL + i * 86400)) +
            ",\"sunrise\":1900001000,\"sunset\":1900040000,\"temp\":{\"morn\":10,\"day\":20,"
            "\"eve\":18,\"night\":12,\"min\":" +
            String(5.0f + static_cast<float>(i)) + ",\"max\":" + String(25.0f + static_cast<float>(i)) +
            "},\"pressure\":1005,\"humidity\":60,\"dew_point\":8,\"clouds\":20,\"uvi\":3,"
            "\"visibility\":10000,\"wind_speed\":4,\"wind_gust\":6,\"wind_deg\":200,\"pop\":0.5,"
            "\"rain\":2.5,\"snow\":0,\"weather\":[{\"id\":804,\"icon\":\"04d\"}]}";
  }
  json += "]}";
  return json;
}

static String makeAlertsJson(size_t count, bool includeTags = true) {
  String json = "{\"alerts\":[";
  for (size_t i = 0; i < count; ++i) {
    if (i > 0)
      json += ",";
    json += "{\"sender_name\":\"AEMET\",\"event\":\"Warning " + String(static_cast<int>(i)) +
            "\",\"start\":" + String(static_cast<int64_t>(1800000000LL + i)) +
            ",\"end\":" + String(static_cast<int64_t>(1800003600LL + i)) + ",\"description\":\"ignored\"";
    if (includeTags)
      json += ",\"tags\":[\"Tag " + String(static_cast<int>(i)) + "\"]";
    json += "}";
  }
  json += "]}";
  return json;
}

static void test_get_api_name(void) {
  OWMProvider provider;
  TEST_ASSERT_EQUAL_STRING("One Call API", provider.getApiName());
}

static void test_zaragoza_forecast_fixture(void) {
  static forecast_t forecast;
  ProviderResult result = parseOneCall(kOwmZaragozaReal, forecast);
  TEST_ASSERT_TRUE(result.isOk());

  TEST_ASSERT_EQUAL_FLOAT(41.6488f, forecast.lat);
  TEST_ASSERT_EQUAL_FLOAT(-0.8891f, forecast.lon);
  TEST_ASSERT_EQUAL_STRING("Europe/Madrid", forecast.timezone.c_str());
  TEST_ASSERT_EQUAL_INT(7200, forecast.timezone_offset);

  TEST_ASSERT_EQUAL_INT64(1787482013LL, forecast.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(28.74f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(29.11f, forecast.current.feels_like);
  TEST_ASSERT_EQUAL_INT(1016, forecast.current.pressure);
  TEST_ASSERT_EQUAL_INT(48, forecast.current.humidity);
  TEST_ASSERT_EQUAL_FLOAT(16.65f, forecast.current.dew_point);
  TEST_ASSERT_EQUAL_INT(14, forecast.current.clouds);
  TEST_ASSERT_EQUAL_FLOAT(6.82f, forecast.current.uvi);
  TEST_ASSERT_EQUAL_INT(10000, forecast.current.visibility);
  TEST_ASSERT_EQUAL_FLOAT(4.12f, forecast.current.wind_speed);
  TEST_ASSERT_EQUAL_INT(90, forecast.current.wind_deg);
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.current.weather.condition);
  TEST_ASSERT_TRUE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.rain_1h);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.snow_1h);

  TEST_ASSERT_EQUAL_INT64(1787479200LL, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(28.95f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT(126, forecast.hourly[0].wind_deg);
  TEST_ASSERT_EQUAL_INT(0, forecast.hourly[0].pop);
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.hourly[0].weather.condition);
  TEST_ASSERT_TRUE(forecast.hourly[0].is_day);

  TEST_ASSERT_EQUAL_INT64(1787508000LL, forecast.hourly[8].dt);
  TEST_ASSERT_EQUAL_FLOAT(25.77f, forecast.hourly[8].temp);
  TEST_ASSERT_EQUAL_INT(100, forecast.hourly[8].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.55f, forecast.hourly[8].rain_1h);
  TEST_ASSERT_EQUAL(weather_condition::RAIN, forecast.hourly[8].weather.condition);

  // The model stores 24 hours although the API returned 48.
  TEST_ASSERT_EQUAL_INT64(1787562000LL, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL_FLOAT(29.36f, forecast.hourly[23].temp);
  TEST_ASSERT_EQUAL_INT(71, forecast.hourly[23].pop);
  TEST_ASSERT_TRUE(forecast.hourly[23].is_day);

  TEST_ASSERT_EQUAL_INT64(1787486400LL, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(22.33f, forecast.daily[0].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(32.57f, forecast.daily[0].temp.max);
  TEST_ASSERT_EQUAL_INT(100, forecast.daily[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(4.51f, forecast.daily[0].rain);
  TEST_ASSERT_EQUAL(weather_condition::RAIN, forecast.daily[0].weather.condition);

  TEST_ASSERT_EQUAL_INT64(1787832000LL, forecast.daily[4].dt);
  TEST_ASSERT_EQUAL_FLOAT(20.75f, forecast.daily[4].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(30.3f, forecast.daily[4].temp.max);
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.daily[4].weather.condition);
}

static void test_report_parser_engages_embedded_alerts(void) {
  StringStream ss(kOwmZaragozaReal);
  weather_report_t report;
  ProviderResult result = OWMProvider::deserializeOneCall(ss, report);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_TRUE(report.forecast.current.dt != 0);
  TEST_ASSERT_TRUE(report.alerts.has_value());
  TEST_ASSERT_EQUAL(3, report.alerts->size());
}

static void test_zaragoza_alert_fixture(void) {
  std::vector<weather_alert_t> alerts;
  ProviderResult result = parseAlerts(kOwmZaragozaReal, alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL(3, alerts.size());

  TEST_ASSERT_EQUAL_STRING("Moderate thunderstorm warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_INT64(1787565600LL, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(1787608799LL, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", alerts[0].tags.c_str());

  TEST_ASSERT_EQUAL_STRING("Severe thunderstorm warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_INT64(1787486400LL, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(1787515199LL, alerts[1].end);
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", alerts[1].tags.c_str());

  TEST_ASSERT_EQUAL_STRING("Moderate rain warning", alerts[2].event.c_str());
  TEST_ASSERT_EQUAL_INT64(1787565600LL, alerts[2].start);
  TEST_ASSERT_EQUAL_INT64(1787608799LL, alerts[2].end);
  TEST_ASSERT_EQUAL_STRING("Rain", alerts[2].tags.c_str());
}

static void test_zaragoza_embedded_alerts(void) {
  static forecast_t forecast;
  std::vector<weather_alert_t> alerts;
  ProviderResult result = parseOneCall(kOwmZaragozaReal, forecast, &alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL(3, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Moderate thunderstorm warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_STRING("Moderate rain warning", alerts[2].event.c_str());
  TEST_ASSERT_EQUAL_STRING("Rain", alerts[2].tags.c_str());
}

static void test_forecast_arrays_are_capped(void) {
  static forecast_t forecast;
  ProviderResult result = parseOneCall(makeOneCallJson(NUM_HOURLY + 6, NUM_DAILY + 3), forecast);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1800000000LL + (NUM_HOURLY - 1) * 3600LL, forecast.hourly[NUM_HOURLY - 1].dt);
  TEST_ASSERT_EQUAL_FLOAT(10.0f + NUM_HOURLY - 1, forecast.hourly[NUM_HOURLY - 1].temp);
  TEST_ASSERT_EQUAL_INT64(1900000000LL + (NUM_DAILY - 1) * 86400LL, forecast.daily[NUM_DAILY - 1].dt);
  TEST_ASSERT_EQUAL_FLOAT(5.0f + NUM_DAILY - 1, forecast.daily[NUM_DAILY - 1].temp.min);
}

static void test_missing_optional_forecast_fields_default_safely(void) {
  static forecast_t forecast;
  forecast.lat = 99.0f;
  forecast.current.temp = 99.0f;
  forecast.hourly[0].rain_1h = 99.0f;
  forecast.daily[0].temp.min = 99.0f;
  ProviderResult result =
      parseOneCall("{\"current\":{\"dt\":1},\"hourly\":[{\"dt\":2}],\"daily\":[{\"dt\":3}]}", forecast);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1, forecast.current.dt);
  TEST_ASSERT_EQUAL_INT64(2, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_INT64(3, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.lat);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].temp.min);
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, forecast.current.weather.condition);
}

static void test_alerts_are_capped_at_provider_limit(void) {
  std::vector<weather_alert_t> alerts;
  ProviderResult result = parseAlerts(makeAlertsJson(11), alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL(8, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Warning 0", alerts.front().event.c_str());
  TEST_ASSERT_EQUAL_STRING("Warning 7", alerts.back().event.c_str());
  TEST_ASSERT_EQUAL_STRING("Tag 7", alerts.back().tags.c_str());
}

static void test_empty_alerts_are_successful(void) {
  std::vector<weather_alert_t> alerts;
  String no_alerts_json = makeOneCallJson(1, 1);
  ProviderResult result = parseAlerts(no_alerts_json, alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_TRUE(alerts.empty());

  StringStream missing_alerts_stream(no_alerts_json);
  weather_report_t missing_alerts_report;
  result = OWMProvider::deserializeOneCall(missing_alerts_stream, missing_alerts_report);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_TRUE(missing_alerts_report.alerts.has_value());
  TEST_ASSERT_TRUE(missing_alerts_report.alerts->empty());

  String empty_alerts_json = makeOneCallJson(1, 1);
  empty_alerts_json.remove(empty_alerts_json.length() - 1);
  empty_alerts_json += ",\"alerts\":[]}";
  result = parseAlerts(empty_alerts_json, alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_TRUE(alerts.empty());
}

static void test_alerts_without_tags_are_safe(void) {
  std::vector<weather_alert_t> alerts;
  ProviderResult result = parseAlerts(makeAlertsJson(1, false), alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Warning 0", alerts[0].event.c_str());
  TEST_ASSERT_TRUE(alerts[0].tags.isEmpty());

  result = parseAlerts("{\"alerts\":[{\"event\":\"No tag\",\"start\":1,\"end\":2,\"tags\":[]}]}", alerts);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL(1, alerts.size());
  TEST_ASSERT_TRUE(alerts[0].tags.isEmpty());
}

static void test_invalid_empty_and_truncated_forecast_json(void) {
  static forecast_t forecast;
  ProviderResult result = parseOneCall("this is not json", forecast);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_TRUE(result.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));

  result = parseOneCall("", forecast);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT, result.detail().c_str());

  result = parseOneCall("{\"current\":{\"dt\":1}", forecast);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT, result.detail().c_str());
}

static void test_invalid_empty_and_truncated_alert_json(void) {
  std::vector<weather_alert_t> alerts;
  ProviderResult result = parseAlerts("not json", alerts);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_TRUE(result.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));

  result = parseAlerts("", alerts);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT, result.detail().c_str());

  result = parseAlerts("{\"alerts\":[{\"event\":\"cut", alerts);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT, result.detail().c_str());
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(owm_provider_tests::test_get_api_name);
  RUN_TEST(owm_provider_tests::test_zaragoza_forecast_fixture);
  RUN_TEST(owm_provider_tests::test_report_parser_engages_embedded_alerts);
  RUN_TEST(owm_provider_tests::test_zaragoza_alert_fixture);
  RUN_TEST(owm_provider_tests::test_zaragoza_embedded_alerts);
  RUN_TEST(owm_provider_tests::test_forecast_arrays_are_capped);
  RUN_TEST(owm_provider_tests::test_missing_optional_forecast_fields_default_safely);
  RUN_TEST(owm_provider_tests::test_alerts_are_capped_at_provider_limit);
  RUN_TEST(owm_provider_tests::test_empty_alerts_are_successful);
  RUN_TEST(owm_provider_tests::test_alerts_without_tags_are_safe);
  RUN_TEST(owm_provider_tests::test_invalid_empty_and_truncated_forecast_json);
  RUN_TEST(owm_provider_tests::test_invalid_empty_and_truncated_alert_json);
}

}  // namespace owm_provider_tests
