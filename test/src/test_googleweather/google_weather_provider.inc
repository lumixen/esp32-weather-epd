/* Offline streaming tests for the Google Weather API provider.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <unity.h>

#include <cstdio>
#include <memory>
#include <vector>

#include "data_models.h"
#include "fetch_operation.h"
#include "google_weather_provider.h"
#include "google_weather_provider_real.inc"
#include "../string_stream.h"
#include "../test_harness.h"

namespace google_weather_tests {

void setUp(void) {}
void tearDown(void) {}

static String hourlyJson(size_t count) {
  String json = R"json({"timeZone":{"id":"Europe/Stockholm"},"forecastHours":[)json";
  for (size_t i = 0; i < count; ++i) {
    if (i != 0)
      json += ",";
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "2026-02-05T%02u:00:00Z", static_cast<unsigned>(i));
    json += String(R"json({"interval":{"startTime":")json") + timestamp + R"json("},"isDaytime":true,)json";
    json += R"json("weatherCondition":{"type":"RAIN_SHOWERS"},"temperature":{"degrees":12.5},)json";
    json += R"json("feelsLikeTemperature":{"degrees":10.5},"dewPoint":{"degrees":5.5},)json";
    json += R"json("relativeHumidity":65,"uvIndex":2,"cloudCover":75,"thunderstormProbability":10,)json";
    json += R"json("precipitation":{"probability":{"percent":40,"type":"RAIN"},)json";
    json += R"json("qpf":{"quantity":1.25,"unit":"MILLIMETERS"}},)json";
    json += R"json("airPressure":{"meanSeaLevelMillibars":1008},)json";
    json += R"json("wind":{"direction":{"degrees":220},"speed":{"value":36},"gust":{"value":54}},)json";
    json += R"json("visibility":{"distance":8,"unit":"KILOMETERS"}})json";
  }
  json += "]}";
  return json;
}

static String dailyJson(size_t count) {
  String json = R"json({"timeZone":{"id":"Europe/Stockholm"},"forecastDays":[)json";
  for (size_t i = 0; i < count; ++i) {
    if (i != 0)
      json += ",";
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "2026-02-%02uT07:00:00Z", static_cast<unsigned>(10 + i));
    json += String(R"json({"interval":{"startTime":")json") + timestamp + R"json("},)json";
    json += R"json("daytimeForecast":{"weatherCondition":{"type":"PARTLY_CLOUDY"},)json";
    json += R"json("relativeHumidity":55,"uvIndex":4,"cloudCover":30,)json";
    json += R"json("precipitation":{"probability":{"percent":25,"type":"RAIN"},)json";
    json += R"json("qpf":{"quantity":2.5,"unit":"MILLIMETERS"}},)json";
    json += R"json("wind":{"direction":{"degrees":180},"speed":{"value":18},"gust":{"value":36}}},)json";
    json += R"json("nighttimeForecast":{"weatherCondition":{"type":"SNOW_SHOWERS"},)json";
    json += R"json("relativeHumidity":85,"uvIndex":0,"cloudCover":90,)json";
    json += R"json("precipitation":{"probability":{"percent":60,"type":"SNOW"},)json";
    json += R"json("qpf":{"quantity":1.5,"unit":"MILLIMETERS"}},)json";
    json += R"json("wind":{"direction":{"degrees":200},"speed":{"value":9},"gust":{"value":27}}},)json";
    json += R"json("maxTemperature":{"degrees":15.5},"minTemperature":{"degrees":4.5}})json";
  }
  json += "]}";
  return json;
}

static void test_captured_current_response(void) {
  StringStream stream(kGoogleWeatherCurrentReal);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeCurrent(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1791227492, forecast.current.dt);
  TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", forecast.timezone.c_str());
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.current.weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(15.8f, forecast.current.temp);
  TEST_ASSERT_EQUAL_INT(82, forecast.current.humidity.value());
  TEST_ASSERT_EQUAL_INT(1000, forecast.current.pressure);
  TEST_ASSERT_EQUAL_INT(16000, forecast.current.visibility.value());
  TEST_ASSERT_EQUAL_INT(242, forecast.current.wind_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.0f / 3.6f, forecast.current.wind_speed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 39.0f / 3.6f, forecast.current.wind_gust);
}

static void test_captured_hourly_response(void) {
  StringStream stream(kGoogleWeatherHourlyReal);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeHourly(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1791226800, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_INT64(1791309600, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.hourly[0].weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(15.8f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT(10, forecast.hourly[0].pop);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.0f / 3.6f, forecast.hourly[0].wind_speed);
}

static void test_captured_daily_response(void) {
  StringStream stream(kGoogleWeatherDailyReal);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeDaily(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1791176400, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(16.4f, forecast.daily[0].temp.max.value());
  TEST_ASSERT_EQUAL_FLOAT(11.6f, forecast.daily[0].temp.min.value());
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.daily[0].weather.condition);
  TEST_ASSERT_EQUAL_INT(15, forecast.daily[0].pop);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.16f, forecast.daily[0].rain);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 26.0f / 3.6f, forecast.daily[0].wind_speed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 45.0f / 3.6f, forecast.daily[0].wind_gust);
}

static void test_current_conditions_mapping(void) {
  const String json = R"json({
    "currentTime":"2026-02-05T12:34:56.123Z","timeZone":{"id":"Europe/Stockholm"},
    "isDaytime":true,"weatherCondition":{"type":"MOSTLY_CLOUDY"},
    "temperature":{"degrees":11.5},"feelsLikeTemperature":{"degrees":9.5},
    "dewPoint":{"degrees":4.5},"relativeHumidity":70,"uvIndex":3,"cloudCover":80,
    "precipitation":{"probability":{"percent":35,"type":"RAIN"},
      "qpf":{"quantity":0.75,"unit":"MILLIMETERS"}},
    "airPressure":{"meanSeaLevelMillibars":1012.8},
    "wind":{"direction":{"degrees":245},"speed":{"value":36},"gust":{"value":54}},
    "visibility":{"distance":9,"unit":"KILOMETERS"}
  })json";
  StringStream stream(json);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeCurrent(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1770294896, forecast.current.dt);
  TEST_ASSERT_EQUAL_STRING("Europe/Stockholm", forecast.timezone.c_str());
  TEST_ASSERT_TRUE(forecast.current.is_day);
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, forecast.current.weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(11.5f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(9.5f, forecast.current.feels_like);
  TEST_ASSERT_EQUAL_FLOAT(4.5f, forecast.current.dew_point);
  TEST_ASSERT_EQUAL_INT(70, forecast.current.humidity.value());
  TEST_ASSERT_EQUAL_INT(1012, forecast.current.pressure);
  TEST_ASSERT_EQUAL_INT(9000, forecast.current.visibility.value());
  TEST_ASSERT_EQUAL_INT(245, forecast.current.wind_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, forecast.current.wind_speed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, forecast.current.wind_gust);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, forecast.current.rain_1h);
}

static void test_hourly_forecast_and_metric_unit_conversion(void) {
  String json = hourlyJson(NUM_HOURLY);
  StringStream stream(json);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeHourly(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1770249600, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, forecast.hourly[0].weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT(65, forecast.hourly[0].humidity);
  TEST_ASSERT_EQUAL_INT(40, forecast.hourly[0].pop);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, forecast.hourly[0].wind_speed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, forecast.hourly[0].wind_gust);
  TEST_ASSERT_EQUAL_INT(8000, forecast.hourly[0].visibility.value());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL_INT64(1770332400, forecast.hourly[23].dt);
}

static void test_daily_forecast_combines_day_and_night_precipitation(void) {
  String json = dailyJson(NUM_DAILY);
  StringStream stream(json);
  forecast_t forecast;
  ProviderResult result = GoogleWeatherForecastProvider::deserializeDaily(stream, forecast);

  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1770706800, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(15.5f, forecast.daily[0].temp.max.value());
  TEST_ASSERT_EQUAL_FLOAT(4.5f, forecast.daily[0].temp.min.value());
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.daily[0].weather.condition);
  TEST_ASSERT_EQUAL_INT(55, forecast.daily[0].humidity);
  TEST_ASSERT_EQUAL_INT(60, forecast.daily[0].pop);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, forecast.daily[0].rain);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, forecast.daily[0].snow);
  TEST_ASSERT_EQUAL_INT(180, forecast.daily[0].wind_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, forecast.daily[0].wind_speed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, forecast.daily[0].wind_gust);
}

static void test_incomplete_payloads_are_rejected_and_reset(void) {
  forecast_t forecast;
  forecast.current.temp = 99.0f;
  String invalidCurrent = R"json({"temperature":{"degrees":20}})json";
  StringStream currentStream(invalidCurrent);
  TEST_ASSERT_FALSE(GoogleWeatherForecastProvider::deserializeCurrent(currentStream, forecast).isOk());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);

  String shortHourly = hourlyJson(1);
  StringStream hourlyStream(shortHourly);
  TEST_ASSERT_FALSE(GoogleWeatherForecastProvider::deserializeHourly(hourlyStream, forecast).isOk());
  TEST_ASSERT_EQUAL_INT64(0, forecast.hourly[0].dt);

  String shortDaily = dailyJson(1);
  StringStream dailyStream(shortDaily);
  TEST_ASSERT_FALSE(GoogleWeatherForecastProvider::deserializeDaily(dailyStream, forecast).isOk());
  TEST_ASSERT_EQUAL_INT64(0, forecast.daily[0].dt);
}

static void test_condition_mapping(void) {
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, GoogleWeatherForecastProvider::mapWeatherCondition("CLEAR"));
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY,
                    GoogleWeatherForecastProvider::mapWeatherCondition("MOSTLY_CLEAR"));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS,
                    GoogleWeatherForecastProvider::mapWeatherCondition("SCATTERED_SHOWERS"));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX,
                    GoogleWeatherForecastProvider::mapWeatherCondition("RAIN_AND_SNOW"));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM,
                    GoogleWeatherForecastProvider::mapWeatherCondition("THUNDERSTORM"));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, GoogleWeatherForecastProvider::mapWeatherCondition("FUTURE_TYPE"));
}

static void test_factory_operations(void) {
  weather_report_t report;
  report.forecast.current.temp = 99.0f;
  GoogleWeatherForecastProvider provider;
  std::vector<std::unique_ptr<FetchOperation>> operations = provider.createFetchOperations(report);

  TEST_ASSERT_EQUAL_UINT(3, operations.size());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, report.forecast.current.temp);
  TEST_ASSERT_TRUE(operations[0]->shouldAbortOnFailure());
  TEST_ASSERT_TRUE(operations[1]->shouldAbortOnFailure());
  TEST_ASSERT_TRUE(operations[2]->shouldAbortOnFailure());
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(google_weather_tests::test_captured_current_response);
  RUN_TEST(google_weather_tests::test_captured_hourly_response);
  RUN_TEST(google_weather_tests::test_captured_daily_response);
  RUN_TEST(google_weather_tests::test_current_conditions_mapping);
  RUN_TEST(google_weather_tests::test_hourly_forecast_and_metric_unit_conversion);
  RUN_TEST(google_weather_tests::test_daily_forecast_combines_day_and_night_precipitation);
  RUN_TEST(google_weather_tests::test_incomplete_payloads_are_rejected_and_reset);
  RUN_TEST(google_weather_tests::test_condition_mapping);
  RUN_TEST(google_weather_tests::test_factory_operations);
}

}  // namespace google_weather_tests
