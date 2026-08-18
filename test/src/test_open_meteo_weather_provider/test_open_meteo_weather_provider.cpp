/* Unit tests for the Open-Meteo weather provider (response mapping).
 *
 * The fixtures are exercised with the real Arduino String/Stream inside the
 * ESP32 QEMU emulator. The primary fixture (open_meteo_lima_real.inc) is a
 * verbatim excerpt of the live Open-Meteo forecast API response for Lima,
 * Peru, captured on 2026-08-18 (negative lat/lon, UTC-5 timezone).
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "open_meteo_weather_provider.h"
#include "open_meteo_lima_real.inc"

// Frozen epochs of the Lima fixture (2026-08-18, America/Lima = UTC-5).
static const int64_t kCurrentTime = 1787068800LL;  // 2026-08-18T12:00:00+00:00
static const int64_t kSunrise = 1787051986LL;      // daily[0]
static const int64_t kSunset = 1787094257LL;       // daily[0]

// Minimal read-only Stream over a String; framework's StreamString does not
// expose String assignment in this Arduino core. The String is borrowed, not
// copied, so large fixtures do not double peak RAM; read()/peek() return the
// byte as unsigned (0..255, -1 at EOF) so UTF-8 payloads (e.g. the "°" in the
// units fields) never promote signed chars to unexpected negative values.
class StringStream : public Stream {
 public:
  explicit StringStream(const String &s) : data_(s), pos_(0) {}
  int read() override {
    return pos_ < data_.length() ? static_cast<uint8_t>(data_[pos_++]) : -1;
  }
  int peek() override {
    return pos_ < data_.length() ? static_cast<uint8_t>(data_[pos_]) : -1;
  }
  int available() override { return data_.length() - pos_; }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}

 private:
  const String &data_;
  size_t pos_;
};

void setUp(void) {}
void tearDown(void) {}

static DeserializationError parseJson(const String &json, forecast_t &forecast) {
  StringStream ss(json);
  return OpenMeteoWeatherProvider::deserializeCall(ss, forecast);
}

/* Minimal synthetic response with `hours` hourly and `days` daily entries.
 * Hourly temperatures are 10*i, daily minima 100+i, maxima 200+i, so every
 * entry is distinguishable from its index. */
static String makeSyntheticJson(size_t hours, size_t days) {
  String j = "{\"current\":{\"time\":1000,\"temperature_2m\":12.5,\"is_day\":1},\"hourly\":{\"time\":[";
  for (size_t i = 0; i < hours; ++i) {
    j += String(i) + (i + 1 < hours ? "," : "");
  }
  j += "],\"temperature_2m\":[";
  for (size_t i = 0; i < hours; ++i) {
    j += String(10 * i) + (i + 1 < hours ? "," : "");
  }
  j += "]},\"daily\":{\"time\":[";
  for (size_t i = 0; i < days; ++i) {
    j += String(i) + (i + 1 < days ? "," : "");
  }
  j += "],\"temperature_2m_min\":[";
  for (size_t i = 0; i < days; ++i) {
    j += String(100 + i) + (i + 1 < days ? "," : "");
  }
  j += "],\"temperature_2m_max\":[";
  for (size_t i = 0; i < days; ++i) {
    j += String(200 + i) + (i + 1 < days ? "," : "");
  }
  j += "]}}";
  return j;
}

static void test_get_api_name(void) {
  OpenMeteoWeatherProvider provider;
  TEST_ASSERT_EQUAL_STRING("Open Meteo API", provider.getApiName());
}

/* Map the real Lima response: every current field, the sun times taken from
 * the first daily entry, and the soil temperature defaulting to 0 (not
 * requested from the API). */
static void test_lima_current(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);

  TEST_ASSERT_EQUAL_INT64(kCurrentTime, forecast.current.dt);
  TEST_ASSERT_EQUAL_INT64(kSunrise, forecast.current.sunrise);
  TEST_ASSERT_EQUAL_INT64(kSunset, forecast.current.sunset);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(20.6f, forecast.current.feels_like);
  TEST_ASSERT_EQUAL_INT(1001, forecast.current.pressure);   // 1001.7 hPa
  TEST_ASSERT_EQUAL_INT(60, forecast.current.humidity);
  TEST_ASSERT_EQUAL_FLOAT(13.4f, forecast.current.dew_point);
  TEST_ASSERT_EQUAL_INT(45, forecast.current.clouds);
  TEST_ASSERT_EQUAL_FLOAT(8.45f, forecast.current.uvi);     // daily[0] uv_index_max
  TEST_ASSERT_EQUAL_INT(2460, forecast.current.visibility); // 2460.00 m
  TEST_ASSERT_EQUAL_FLOAT(5.81f, forecast.current.wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(14.50f, forecast.current.wind_gust);
  TEST_ASSERT_EQUAL_INT(153, forecast.current.wind_deg);
  TEST_ASSERT_EQUAL_INT(1, forecast.current.weather.id);
  TEST_ASSERT_TRUE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.soil_temperature_18cm);
}

/* Spot checks on the 24 hourly entries: dt, weather, is_day day/night
 * transitions (is_day 1 -> true, 0 -> false). */
static void test_lima_hourly(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);

  // First entry: the current observation hour.
  TEST_ASSERT_EQUAL_INT64(1787068800LL, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT(45, forecast.hourly[0].clouds);
  TEST_ASSERT_EQUAL_FLOAT(5.81f, forecast.hourly[0].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(14.50f, forecast.hourly[0].wind_gust);
  TEST_ASSERT_EQUAL_INT(0, forecast.hourly[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].snow_1h);
  TEST_ASSERT_EQUAL_INT(1, forecast.hourly[0].weather.id);
  TEST_ASSERT_TRUE(forecast.hourly[0].is_day);

  // Night entry (is_day 0 -> false).
  TEST_ASSERT_EQUAL_INT64(1787097600LL, forecast.hourly[8].dt);
  TEST_ASSERT_EQUAL_FLOAT(18.8f, forecast.hourly[8].temp);
  TEST_ASSERT_EQUAL_INT(34, forecast.hourly[8].clouds);
  TEST_ASSERT_EQUAL_INT(1, forecast.hourly[8].weather.id);
  TEST_ASSERT_FALSE(forecast.hourly[8].is_day);

  TEST_ASSERT_EQUAL_INT64(1787112000LL, forecast.hourly[12].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.5f, forecast.hourly[12].temp);
  TEST_ASSERT_EQUAL_INT(44, forecast.hourly[12].clouds);
  TEST_ASSERT_FALSE(forecast.hourly[12].is_day);

  // Late morning entry: weather code 2, back to daytime.
  TEST_ASSERT_EQUAL_INT64(1787140800LL, forecast.hourly[20].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.2f, forecast.hourly[20].temp);
  TEST_ASSERT_EQUAL_INT(2, forecast.hourly[20].weather.id);
  TEST_ASSERT_TRUE(forecast.hourly[20].is_day);

  // Last of the 24 entries.
  TEST_ASSERT_EQUAL_INT64(1787151600LL, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL_FLOAT(19.9f, forecast.hourly[23].temp);
  TEST_ASSERT_EQUAL_INT(29, forecast.hourly[23].clouds);
  TEST_ASSERT_EQUAL_FLOAT(3.82f, forecast.hourly[23].wind_speed);
  TEST_ASSERT_EQUAL_INT(1, forecast.hourly[23].weather.id);
  TEST_ASSERT_TRUE(forecast.hourly[23].is_day);
}

/* All five daily entries, including the 2 % precipitation day; the
 * shortwave radiation sum defaults to 0 (not requested from the API). */
static void test_lima_daily(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);

  TEST_ASSERT_EQUAL_INT64(1787029200LL, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.5f, forecast.daily[0].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, forecast.daily[0].temp.max);
  TEST_ASSERT_EQUAL_FLOAT(6.30f, forecast.daily[0].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(15.20f, forecast.daily[0].wind_gust);
  TEST_ASSERT_EQUAL_INT(0, forecast.daily[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].rain);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].snow);
  TEST_ASSERT_EQUAL_INT(3, forecast.daily[0].weather.id);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].shortwave_radiation_sum);

  TEST_ASSERT_EQUAL_INT64(1787202000LL, forecast.daily[2].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, forecast.daily[2].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.8f, forecast.daily[2].temp.max);
  TEST_ASSERT_EQUAL_INT(3, forecast.daily[2].weather.id);

  // The only day with any precipitation probability.
  TEST_ASSERT_EQUAL_INT64(1787288400LL, forecast.daily[3].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, forecast.daily[3].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.9f, forecast.daily[3].temp.max);
  TEST_ASSERT_EQUAL_INT(2, forecast.daily[3].pop);
  TEST_ASSERT_EQUAL_FLOAT(3.37f, forecast.daily[3].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(10.00f, forecast.daily[3].wind_gust);
  TEST_ASSERT_EQUAL_INT(2, forecast.daily[3].weather.id);

  TEST_ASSERT_EQUAL_INT64(1787374800LL, forecast.daily[4].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.8f, forecast.daily[4].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(22.5f, forecast.daily[4].temp.max);
  TEST_ASSERT_EQUAL_INT(3, forecast.daily[4].weather.id);
}

/* The response can carry more entries than the model holds: only the first
 * NUM_HOURLY hourly and NUM_DAILY daily entries are stored. The hourly loop
 * stops at the model's last slot (NUM_HOURLY - 1), so with 30 entries the
 * last stored hour must be entry 23, never one of entries 24..29. */
static void test_hourly_and_daily_cap(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson(makeSyntheticJson(30, 7), forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);

  TEST_ASSERT_EQUAL_INT64(23, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL_FLOAT(230.0f, forecast.hourly[23].temp);

  // The last slot holds the 5th daily entry, not the 7th.
  TEST_ASSERT_EQUAL_INT64(4, forecast.daily[4].dt);
  TEST_ASSERT_EQUAL_FLOAT(104.0f, forecast.daily[4].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(204.0f, forecast.daily[4].temp.max);
}

/* Fewer entries than the model holds: the remaining slots stay untouched. */
static void test_shorter_arrays(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson(makeSyntheticJson(3, 2), forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);

  TEST_ASSERT_EQUAL_INT64(0, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT64(2, forecast.hourly[2].dt);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, forecast.hourly[2].temp);
  TEST_ASSERT_EQUAL_INT64(0, forecast.hourly[3].dt);  // past the end
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[3].temp);

  TEST_ASSERT_EQUAL_INT64(1, forecast.daily[1].dt);
  TEST_ASSERT_EQUAL_FLOAT(101.0f, forecast.daily[1].temp.min);
  TEST_ASSERT_EQUAL_INT64(0, forecast.daily[2].dt);  // past the end
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[2].temp.min);
}

/* Optional fields that the provider does not request but would parse if the
 * API ever returned them. */
static void test_optional_fields_present(void) {
  const char *json =
      "{\"current\":{\"time\":1},"
      "\"hourly\":{\"time\":[2],\"soil_temperature_18cm\":[17.25]},"
      "\"daily\":{\"time\":[3],\"shortwave_radiation_sum\":[14.5]}}";
  forecast_t forecast = {};
  DeserializationError err = parseJson(json, forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_FLOAT(17.25f, forecast.current.soil_temperature_18cm);
  TEST_ASSERT_EQUAL_FLOAT(14.5f, forecast.daily[0].shortwave_radiation_sum);
}

/* Missing sections or fields must not crash and default to zero. */
static void test_missing_sections(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson("{}", forecast);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_INT64(0, forecast.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);
  TEST_ASSERT_EQUAL_INT(0, forecast.current.weather.id);
  TEST_ASSERT_FALSE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_INT64(0, forecast.daily[0].dt);

  // current without hourly/daily, and is_day 0 -> false.
  forecast_t f2 = {};
  err = parseJson("{\"current\":{\"time\":123,\"temperature_2m\":-2.5,\"is_day\":0}}", f2);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_INT64(123, f2.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(-2.5f, f2.current.temp);
  TEST_ASSERT_FALSE(f2.current.is_day);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f2.current.soil_temperature_18cm);
}

/* Garbage and empty input are reported as deserialization errors. */
static void test_invalid_json(void) {
  forecast_t forecast = {};
  DeserializationError err = parseJson("this is not json", forecast);
  TEST_ASSERT_FALSE(err == DeserializationError::Ok);

  err = parseJson("", forecast);
  TEST_ASSERT_FALSE(err == DeserializationError::Ok);
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_get_api_name);
  RUN_TEST(test_lima_current);
  RUN_TEST(test_lima_hourly);
  RUN_TEST(test_lima_daily);
  RUN_TEST(test_hourly_and_daily_cap);
  RUN_TEST(test_shorter_arrays);
  RUN_TEST(test_optional_fields_present);
  RUN_TEST(test_missing_sections);
  RUN_TEST(test_invalid_json);
  UNITY_END();
}

void loop() {}
