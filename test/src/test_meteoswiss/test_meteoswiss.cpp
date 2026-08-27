/* MeteoSwiss forecast-provider fixture tests.
 *
 * Copyright (C) 2026  Max Bodaniuk
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>
#include "../test_harness.h"
#include "../string_stream.h"
#include "meteo_swiss_forecast_provider.h"
#include "fetch_operation.h"
#include "meteoswiss_fixtures.inc"

namespace test_meteoswiss {

void setUp() {}
void tearDown() {}

static String forecastJson(bool complete = true) {
  String json = "{\"currentWeather\":{\"time\":1787818200000,\"icon\":1,\"iconV2\":101,\"temperature\":22.7},";
  json += "\"forecast\":[";
  for (int i = 0; i < 5; ++i) {
    if (i)
      json += ",";
    json += "{\"dayDate\":\"2026-08-" + String(27 + i) + "\",\"iconDay\":1,\"iconDayV2\":" + String(i == 1 ? 13 : 1) +
            ",\"temperatureMax\":" + String(25 + i) + ",\"temperatureMin\":" + String(15 + i) +
            ",\"precipitation\":" + String(i * 0.5f) + "}";
  }
  json += "],\"graph\":{\"start\":1787781600000,\"startLowResolution\":1787835600000,";
  const char *fields[] = {"temperatureMean1h", "windSpeed1h", "gustSpeed1h"};
  for (const char *field : fields) {
    json += "\"" + String(field) + "\":[";
    for (int i = 0; i < 40; ++i) {
      if (i)
        json += ",";
      json += String(i + (field[0] == 't' ? 100 : field[0] == 'w' ? 10 : 20));
    }
    json += "],";
  }
  json += "\"windDirection3h\":[";
  for (int i = 0; i < 20; ++i)
    json += String(i ? "," : "") + String(180 + i);
  json += "],\"weatherIcon3hV2\":[";
  for (int i = 0; i < 20; ++i)
    json += String(i ? "," : "") + String(i % 2 ? 1 : 102);
  json += "],\"precipitationProbability3h\":[";
  for (int i = 0; i < 20; ++i)
    json += String(i ? "," : "") + String(i * 5);
  json += "],\"precipitation10m\":[";
  for (int i = 0; i < 90; ++i)
    json += String(i ? "," : "") + String(i == 66 ? 0.1f : 0.0f);
  json += "],\"precipitation1h\":[";
  for (int i = 0; i < 40; ++i)
    json += String(i ? "," : "") + String(i == 11 ? 2.5f : 0.0f);
  json += "]}}";
  if (!complete)
    json.remove(json.length() - 2);
  return json;
}

static void test_provider_and_icon_mapping() {
  MeteoSwissForecastProvider provider;
  TEST_ASSERT_EQUAL_STRING("MeteoSwiss API", provider.getApiName());
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, MeteoSwissForecastProvider::mapWeatherCode(1));
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, MeteoSwissForecastProvider::mapWeatherCode(2));
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, MeteoSwissForecastProvider::mapWeatherCode(4));
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, MeteoSwissForecastProvider::mapWeatherCode(5));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, MeteoSwissForecastProvider::mapWeatherCode(6));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, MeteoSwissForecastProvider::mapWeatherCode(7));
  TEST_ASSERT_EQUAL(weather_condition::SNOW_SHOWERS, MeteoSwissForecastProvider::mapWeatherCode(8));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, MeteoSwissForecastProvider::mapWeatherCode(9));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, MeteoSwissForecastProvider::mapWeatherCode(14));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, MeteoSwissForecastProvider::mapWeatherCode(22));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, MeteoSwissForecastProvider::mapWeatherCode(42));
  TEST_ASSERT_EQUAL(weather_condition::FOG, MeteoSwissForecastProvider::mapWeatherCode(28));
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, MeteoSwissForecastProvider::mapWeatherCode(101));
  TEST_ASSERT_EQUAL(weather_condition::FOG, MeteoSwissForecastProvider::mapWeatherCode(128));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, MeteoSwissForecastProvider::mapWeatherCode(0));
  TEST_ASSERT_TRUE(MeteoSwissForecastProvider::isDayIcon(42));
  TEST_ASSERT_FALSE(MeteoSwissForecastProvider::isDayIcon(142));
}

static void test_forecast_mapping_and_alignment() {
  String json = forecastJson();
  StringStream stream(json);
  forecast_t forecast{};
  ProviderResult result = MeteoSwissForecastProvider::deserializeForecast(stream, forecast);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1787818200, forecast.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(22.7f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(22.7f, forecast.current.feels_like);
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, forecast.current.weather.condition);
  TEST_ASSERT_FALSE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_INT64(1787821200, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(111.0f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_FLOAT(21.0f / 3.6f, forecast.hourly[0].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(31.0f / 3.6f, forecast.hourly[0].wind_gust);
  TEST_ASSERT_EQUAL(183, forecast.hourly[0].wind_deg);
  TEST_ASSERT_EQUAL(15, forecast.hourly[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.1f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, forecast.hourly[0].weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, forecast.daily[0].temp.max.value());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].rain);
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, forecast.daily[1].weather.condition);
}

static void test_captured_fixtures() {
  String forecastText(kMeteoSwissForecastFixture);
  StringStream forecastStream(forecastText);
  forecast_t forecast{};
  TEST_ASSERT_TRUE(MeteoSwissForecastProvider::deserializeForecast(forecastStream, forecast).isOk());
  TEST_ASSERT_EQUAL_FLOAT(31.0f, forecast.daily[0].temp.max.value());

  String csvText(kMeteoSwissObservationFixture);
  StringStream csvStream(csvText);
  current_t current{};
  TEST_ASSERT_TRUE(MeteoSwissForecastProvider::deserializeObservationCsv(csvStream, "KLO", current).isOk());
  TEST_ASSERT_EQUAL_FLOAT(22.4f, current.temp);
}

static void test_station_csv_mapping() {
  String csv =
      "Station/Location;Date;tre200s0;ure200s0;tde200s0;dkl010z0;fu3010z0;fu3010z1;prestas0;pp0qffs0;pp0qnhs0\n";
  csv += "OTHER;202608270810;1;2;3;4;5;6;1000;1001;1002\n";
  csv += "KLO;202608270810;22.20;73.90;17.30;360.00;2.50;6.80;964.80;1013.20;1015.20\n";
  StringStream stream(csv);
  current_t current{};
  ProviderResult result = MeteoSwissForecastProvider::deserializeObservationCsv(stream, "KLO", current);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_INT64(1787818200, current.dt);
  TEST_ASSERT_EQUAL_FLOAT(22.2f, current.temp);
  TEST_ASSERT_EQUAL(73, current.humidity.value());
  TEST_ASSERT_EQUAL_FLOAT(2.5f / 3.6f, current.wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(6.8f / 3.6f, current.wind_gust);
  TEST_ASSERT_EQUAL(360, current.wind_deg);
  TEST_ASSERT_EQUAL(1015, current.pressure);
}

static void test_validation_and_truncated_input() {
  forecast_t forecast{};
  String empty;
  StringStream emptyStream(empty);
  TEST_ASSERT_FALSE(MeteoSwissForecastProvider::deserializeForecast(emptyStream, forecast).isOk());
  String malformed = "{not json";
  StringStream malformedStream(malformed);
  TEST_ASSERT_FALSE(MeteoSwissForecastProvider::deserializeForecast(malformedStream, forecast).isOk());
  String truncated = forecastJson(false);
  StringStream truncatedStream(truncated);
  ProviderResult result = MeteoSwissForecastProvider::deserializeForecast(truncatedStream, forecast);
  TEST_ASSERT_FALSE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);
}

static void test_dependency_graph() {
  MeteoSwissForecastProvider provider;
  weather_report_t report;
  auto operations = provider.createFetchOperations(report);
  TEST_ASSERT_EQUAL(2, operations.size());
  TEST_ASSERT_TRUE(operations[0]->shouldAbortOnFailure());
  TEST_ASSERT_FALSE(operations[1]->shouldAbortOnFailure());
  TEST_ASSERT_EQUAL(1, operations[1]->dependencies().size());
  TEST_ASSERT_EQUAL(operations[0].get(), operations[1]->dependencies()[0]);
}

void registerTests() {
  RUN_TEST(test_provider_and_icon_mapping);
  RUN_TEST(test_forecast_mapping_and_alignment);
  RUN_TEST(test_captured_fixtures);
  RUN_TEST(test_station_csv_mapping);
  RUN_TEST(test_validation_and_truncated_input);
  RUN_TEST(test_dependency_graph);
}

}  // namespace test_meteoswiss

void setUp(void) { test_meteoswiss::setUp(); }
void tearDown(void) { test_meteoswiss::tearDown(); }

void setup() {
  delay(200);
  UNITY_BEGIN();
  test_meteoswiss::registerTests();
  UNITY_END();
  for (;;)
    delay(1000);
}

void loop() {}
