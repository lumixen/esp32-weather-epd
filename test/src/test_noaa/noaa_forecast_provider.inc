/* Focused NOAA parser and dependency tests. Network access is deliberately
 * not used; all input is generated in memory. */

#include <unity.h>
#include <memory>
#include <vector>
#include "../string_stream.h"
#include "noaa_forecast_provider.h"
#include "fetch_operation.h"

namespace test_noaa {

void setUp() {}
void tearDown() {}

static ProviderResult parse(const String &text, forecast_t &forecast,
                            ProviderResult (*parser)(Stream &, forecast_t &)) {
  StringStream stream(text);
  return parser(stream, forecast);
}

static String hourlyJson() {
  String json = "{\"properties\":{\"periods\":[";
  for (int i = 0; i < NUM_HOURLY; ++i) {
    if (i) json += ",";
    const int hour = (10 + i) % 24;
    const String hourText = (hour < 10 ? "0" : "") + String(hour);
    json += "{\"startTime\":\"2026-08-" + String(25 + (10 + i) / 24) + "T" + hourText +
            ":00:00-04:00\",\"isDaytime\":true,\"temperature\":" + String(20 + i) +
            ",\"dewpoint\":{\"value\":10},\"relativeHumidity\":{\"value\":50},";
    json += "\"probabilityOfPrecipitation\":{\"value\":40},\"windSpeed\":\"5 to 15 km/h\",\"windDirection\":\"NNE\",\"shortForecast\":\"Partly Cloudy\"}";
  }
  return json + "]}}";
}

static String dailyJson() {
  String json = "{\"properties\":{\"periods\":[";
  for (int i = 0; i < NUM_DAILY * 2; ++i) {
    if (i) json += ",";
    const int day = 25 + i / 2;
    json += "{\"startTime\":\"2026-08-" + String(day) + "T" + (i % 2 ? "22" : "10") +
            ":00:00-04:00\",\"isDaytime\":" + String(i % 2 == 0 ? "true" : "false") +
            ",\"temperature\":" + String(25 - i) +
            ",\"probabilityOfPrecipitation\":{\"value\":" + String(i * 5) +
            "},\"windSpeed\":\"10 km/h\",\"windDirection\":\"S\",\"shortForecast\":\"Sunny\"}";
  }
  return json + "]}}";
}

static void test_api_and_helpers() {
  NoaaForecastProvider provider;
  TEST_ASSERT_EQUAL_STRING("NOAA/NWS API", provider.getApiName());
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, NoaaForecastProvider::mapDescription("Chance Showers And Thunderstorms"));
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, NoaaForecastProvider::mapDescription("Partly Cloudy"));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, NoaaForecastProvider::parseIso8601("2026-08-25T15:00:00-05:00"));
  TEST_ASSERT_EQUAL_INT64(1787688000LL, NoaaForecastProvider::parseIso8601("2026-08-25T20:00:00Z"));
  TEST_ASSERT_EQUAL_INT64(-1, NoaaForecastProvider::parseIso8601("not-a-date"));
  TEST_ASSERT_TRUE(NoaaForecastProvider::isValidApiUrl("https://api.weather.gov/gridpoints/BOX/1,2"));
  TEST_ASSERT_FALSE(NoaaForecastProvider::isValidApiUrl("https://example.com/weather"));
}

static void test_hourly_mapping() {
  forecast_t forecast{};
  ProviderResult result = parse(hourlyJson(), forecast, NoaaForecastProvider::deserializeHourly);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(20.0f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_FLOAT(5.0f / 3.6f, forecast.hourly[0].wind_speed);
  TEST_ASSERT_EQUAL(40, forecast.hourly[0].pop);
  TEST_ASSERT_EQUAL(22, forecast.hourly[0].wind_deg);
}

static void test_daily_mapping() {
  forecast_t forecast{};
  ProviderResult result = parse(dailyJson(), forecast, NoaaForecastProvider::deserializeDaily);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(25.0f, forecast.daily[0].temp.day);
  TEST_ASSERT_EQUAL_FLOAT(24.0f, forecast.daily[0].temp.night);
  TEST_ASSERT_EQUAL(5, forecast.daily[0].pop);
}

static void test_points_and_observation_mapping() {
  String urls[3];
  String points = "{\"properties\":{\"forecast\":\"https://api.weather.gov/gridpoints/BOX/1,2/forecast\",\"forecastHourly\":\"https://api.weather.gov/gridpoints/BOX/1,2/forecast/hourly\",\"observationStations\":\"https://api.weather.gov/gridpoints/BOX/1,2/stations\",\"timeZone\":[\"America/New_York\"]}}";
  StringStream pointsStream(points);
  String timezone;
  ProviderResult result = NoaaForecastProvider::deserializePoints(pointsStream, urls[0], urls[1], urls[2], timezone);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_STRING("America/New_York", timezone.c_str());
  TEST_ASSERT_TRUE(urls[0].endsWith("/forecast"));

  String observation = "{\"properties\":{\"timestamp\":\"2026-08-25T15:00:00-04:00\",\"textDescription\":\"Partly Cloudy\",\"temperature\":{\"value\":22,\"unitCode\":\"wmoUnit:degC\"},\"relativeHumidity\":{\"value\":55},\"seaLevelPressure\":{\"value\":101500,\"unitCode\":\"wmoUnit:Pa\"},\"windSpeed\":{\"value\":18,\"unitCode\":\"wmoUnit:km_h-1\"},\"precipitationLastHour\":{\"value\":0.2},\"cloudLayers\":[{\"amount\":\"FEW\"}]}}";
  current_t current{};
  StringStream observationStream(observation);
  result = NoaaForecastProvider::deserializeObservation(observationStream, current);
  TEST_ASSERT_TRUE(result.isOk());
  TEST_ASSERT_EQUAL_FLOAT(22.0f, current.temp);
  TEST_ASSERT_EQUAL(1015, current.pressure);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, current.wind_speed);
  TEST_ASSERT_EQUAL(25, current.clouds);
}

static void test_dependency_graph() {
  NoaaForecastProvider provider;
  weather_report_t report;
  auto operations = provider.createFetchOperations(report);
  TEST_ASSERT_EQUAL(4, operations.size());
  TEST_ASSERT_EQUAL(0, operations[0]->dependencies().size());
  for (size_t i = 1; i < operations.size(); ++i) {
    TEST_ASSERT_EQUAL(1, operations[i]->dependencies().size());
    TEST_ASSERT_EQUAL(operations[0].get(), operations[i]->dependencies()[0]);
  }
}

void registerTests() {
  RUN_TEST(test_api_and_helpers);
  RUN_TEST(test_hourly_mapping);
  RUN_TEST(test_daily_mapping);
  RUN_TEST(test_points_and_observation_mapping);
  RUN_TEST(test_dependency_graph);
}

}  // namespace test_noaa
