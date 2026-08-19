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

#include "_locale.h"
#include "client_utils.h"
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

static ProviderResult parseJson(const String &json, forecast_t &forecast) {
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

/* Every WMO weather interpretation code must map onto the unified
 * weather_condition enum. Unknown codes must fall back to UNKNOWN. */
static void test_map_weather_code(void) {
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, OpenMeteoWeatherProvider::mapWeatherCode(0));
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, OpenMeteoWeatherProvider::mapWeatherCode(1));
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, OpenMeteoWeatherProvider::mapWeatherCode(2));
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, OpenMeteoWeatherProvider::mapWeatherCode(3));
  TEST_ASSERT_EQUAL(weather_condition::FOG, OpenMeteoWeatherProvider::mapWeatherCode(45));
  TEST_ASSERT_EQUAL(weather_condition::FOG, OpenMeteoWeatherProvider::mapWeatherCode(48));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OpenMeteoWeatherProvider::mapWeatherCode(51));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OpenMeteoWeatherProvider::mapWeatherCode(53));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OpenMeteoWeatherProvider::mapWeatherCode(55));
  TEST_ASSERT_EQUAL(weather_condition::FREEZING_DRIZZLE, OpenMeteoWeatherProvider::mapWeatherCode(56));
  TEST_ASSERT_EQUAL(weather_condition::FREEZING_DRIZZLE, OpenMeteoWeatherProvider::mapWeatherCode(57));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OpenMeteoWeatherProvider::mapWeatherCode(61));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OpenMeteoWeatherProvider::mapWeatherCode(63));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OpenMeteoWeatherProvider::mapWeatherCode(65));
  TEST_ASSERT_EQUAL(weather_condition::FREEZING_RAIN, OpenMeteoWeatherProvider::mapWeatherCode(66));
  TEST_ASSERT_EQUAL(weather_condition::FREEZING_RAIN, OpenMeteoWeatherProvider::mapWeatherCode(67));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OpenMeteoWeatherProvider::mapWeatherCode(71));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OpenMeteoWeatherProvider::mapWeatherCode(73));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OpenMeteoWeatherProvider::mapWeatherCode(75));
  TEST_ASSERT_EQUAL(weather_condition::SNOW_GRAINS, OpenMeteoWeatherProvider::mapWeatherCode(77));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OpenMeteoWeatherProvider::mapWeatherCode(80));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OpenMeteoWeatherProvider::mapWeatherCode(81));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OpenMeteoWeatherProvider::mapWeatherCode(82));
  TEST_ASSERT_EQUAL(weather_condition::SNOW_SHOWERS, OpenMeteoWeatherProvider::mapWeatherCode(85));
  TEST_ASSERT_EQUAL(weather_condition::SNOW_SHOWERS, OpenMeteoWeatherProvider::mapWeatherCode(86));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OpenMeteoWeatherProvider::mapWeatherCode(95));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM_HAIL, OpenMeteoWeatherProvider::mapWeatherCode(96));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM_HAIL, OpenMeteoWeatherProvider::mapWeatherCode(99));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenMeteoWeatherProvider::mapWeatherCode(4));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenMeteoWeatherProvider::mapWeatherCode(20));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenMeteoWeatherProvider::mapWeatherCode(44));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenMeteoWeatherProvider::mapWeatherCode(90));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OpenMeteoWeatherProvider::mapWeatherCode(401));
}

/* Map the real Lima response: every current field, the sun times taken from
 * the first daily entry, and the soil temperature defaulting to 0 (not
 * requested from the API). */
static void test_lima_current(void) {
  forecast_t forecast = {};
  ProviderResult err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err.isOk());

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
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.current.weather.condition);
  TEST_ASSERT_TRUE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.soil_temperature_18cm);
}

/* Spot checks on the 24 hourly entries: dt, weather, is_day day/night
 * transitions (is_day 1 -> true, 0 -> false). */
static void test_lima_hourly(void) {
  forecast_t forecast = {};
  ProviderResult err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err.isOk());

  // First entry: the current observation hour.
  TEST_ASSERT_EQUAL_INT64(1787068800LL, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, forecast.hourly[0].temp);
  TEST_ASSERT_EQUAL_INT(45, forecast.hourly[0].clouds);
  TEST_ASSERT_EQUAL_FLOAT(5.81f, forecast.hourly[0].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(14.50f, forecast.hourly[0].wind_gust);
  TEST_ASSERT_EQUAL_INT(0, forecast.hourly[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].rain_1h);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.hourly[0].snow_1h);
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.hourly[0].weather.condition);
  TEST_ASSERT_TRUE(forecast.hourly[0].is_day);

  // Night entry (is_day 0 -> false).
  TEST_ASSERT_EQUAL_INT64(1787097600LL, forecast.hourly[8].dt);
  TEST_ASSERT_EQUAL_FLOAT(18.8f, forecast.hourly[8].temp);
  TEST_ASSERT_EQUAL_INT(34, forecast.hourly[8].clouds);
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.hourly[8].weather.condition);
  TEST_ASSERT_FALSE(forecast.hourly[8].is_day);

  TEST_ASSERT_EQUAL_INT64(1787112000LL, forecast.hourly[12].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.5f, forecast.hourly[12].temp);
  TEST_ASSERT_EQUAL_INT(44, forecast.hourly[12].clouds);
  TEST_ASSERT_FALSE(forecast.hourly[12].is_day);

  // Late morning entry: weather code 2, back to daytime.
  TEST_ASSERT_EQUAL_INT64(1787140800LL, forecast.hourly[20].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.2f, forecast.hourly[20].temp);
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, forecast.hourly[20].weather.condition);
  TEST_ASSERT_TRUE(forecast.hourly[20].is_day);

  // Last of the 24 entries.
  TEST_ASSERT_EQUAL_INT64(1787151600LL, forecast.hourly[23].dt);
  TEST_ASSERT_EQUAL_FLOAT(19.9f, forecast.hourly[23].temp);
  TEST_ASSERT_EQUAL_INT(29, forecast.hourly[23].clouds);
  TEST_ASSERT_EQUAL_FLOAT(3.82f, forecast.hourly[23].wind_speed);
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, forecast.hourly[23].weather.condition);
  TEST_ASSERT_TRUE(forecast.hourly[23].is_day);
}

/* All five daily entries, including the 2 % precipitation day; the
 * shortwave radiation sum defaults to 0 (not requested from the API). */
static void test_lima_daily(void) {
  forecast_t forecast = {};
  ProviderResult err = parseJson(kOpenMeteoLimaReal, forecast);
  TEST_ASSERT_TRUE(err.isOk());

  TEST_ASSERT_EQUAL_INT64(1787029200LL, forecast.daily[0].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.5f, forecast.daily[0].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, forecast.daily[0].temp.max);
  TEST_ASSERT_EQUAL_FLOAT(6.30f, forecast.daily[0].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(15.20f, forecast.daily[0].wind_gust);
  TEST_ASSERT_EQUAL_INT(0, forecast.daily[0].pop);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].rain);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].snow);
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.daily[0].weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].shortwave_radiation_sum);

  TEST_ASSERT_EQUAL_INT64(1787202000LL, forecast.daily[2].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, forecast.daily[2].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.8f, forecast.daily[2].temp.max);
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.daily[2].weather.condition);

  // The only day with any precipitation probability.
  TEST_ASSERT_EQUAL_INT64(1787288400LL, forecast.daily[3].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.0f, forecast.daily[3].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(21.9f, forecast.daily[3].temp.max);
  TEST_ASSERT_EQUAL_INT(2, forecast.daily[3].pop);
  TEST_ASSERT_EQUAL_FLOAT(3.37f, forecast.daily[3].wind_speed);
  TEST_ASSERT_EQUAL_FLOAT(10.00f, forecast.daily[3].wind_gust);
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, forecast.daily[3].weather.condition);

  TEST_ASSERT_EQUAL_INT64(1787374800LL, forecast.daily[4].dt);
  TEST_ASSERT_EQUAL_FLOAT(17.8f, forecast.daily[4].temp.min);
  TEST_ASSERT_EQUAL_FLOAT(22.5f, forecast.daily[4].temp.max);
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, forecast.daily[4].weather.condition);
}

/* The response can carry more entries than the model holds: only the first
 * NUM_HOURLY hourly and NUM_DAILY daily entries are stored. The hourly loop
 * stops at the model's last slot (NUM_HOURLY - 1), so with 30 entries the
 * last stored hour must be entry 23, never one of entries 24..29. */
static void test_hourly_and_daily_cap(void) {
  forecast_t forecast = {};
  ProviderResult err = parseJson(makeSyntheticJson(30, 7), forecast);
  TEST_ASSERT_TRUE(err.isOk());

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
  ProviderResult err = parseJson(makeSyntheticJson(3, 2), forecast);
  TEST_ASSERT_TRUE(err.isOk());

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
  ProviderResult err = parseJson(json, forecast);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_FLOAT(17.25f, forecast.current.soil_temperature_18cm);
  TEST_ASSERT_EQUAL_FLOAT(14.5f, forecast.daily[0].shortwave_radiation_sum);
}

/* A syntactically valid JSON root that is not an Open-Meteo forecast must
 * be rejected with InvalidInput (not Ok), so the caller's retry/error path
 * can engage instead of trusting stale forecast values. This covers the
 * Open-Meteo {"error": ...} responses and payloads missing any of the three
 * required time keys (current.time, hourly.time, daily.time). */
static void test_non_forecast_payloads_rejected(void) {
  forecast_t forecast = {};

  const char *errorResponse = "{\"error\":true,\"reason\":\"Latitude must be in range of (-90, 90]\"}";
  ProviderResult err = parseJson(errorResponse, forecast);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_TRUE(err.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));

  // Empty root: no section seen at all.
  err = parseJson("{}", forecast);
  TEST_ASSERT_FALSE(err.isOk());

  // current without hourly/daily time arrays.
  err = parseJson("{\"current\":{\"time\":123,\"temperature_2m\":-2.5,\"is_day\":0}}", forecast);
  TEST_ASSERT_FALSE(err.isOk());

  // All three sections but current.time missing.
  err = parseJson("{\"current\":{\"temperature_2m\":12.5},\"hourly\":{\"time\":[1]},\"daily\":{\"time\":[2]}}",
                  forecast);
  TEST_ASSERT_FALSE(err.isOk());

  // current.time present but the hourly/daily time arrays empty.
  err = parseJson("{\"current\":{\"time\":1},\"hourly\":{\"time\":[]},\"daily\":{\"time\":[]}}", forecast);
  TEST_ASSERT_FALSE(err.isOk());

  // Rejected payloads must leave the caller's forecast untouched.
  TEST_ASSERT_EQUAL_INT64(0, forecast.current.dt);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.soil_temperature_18cm);
}

/* The minimum key set that identifies a payload as a forecast is accepted,
 * even when every other field is absent. */
static void test_minimal_forecast_keys_accepted(void) {
  forecast_t forecast = {};
  ProviderResult err =
      parseJson("{\"current\":{\"time\":1},\"hourly\":{\"time\":[2]},\"daily\":{\"time\":[3]}}", forecast);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_INT64(1, forecast.current.dt);
  TEST_ASSERT_EQUAL_INT64(2, forecast.hourly[0].dt);
  TEST_ASSERT_EQUAL_INT64(3, forecast.daily[0].dt);
}

/* Payloads with the required time keys but missing individual fields are
 * accepted; the absent fields must not crash and default to zero. */
static void test_missing_optional_fields(void) {
  forecast_t forecast = {};
  ProviderResult err =
      parseJson("{\"current\":{\"time\":123,\"is_day\":0},\"hourly\":{\"time\":[1]},\"daily\":{\"time\":[2]}}",
                forecast);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_INT64(123, forecast.current.dt);
  TEST_ASSERT_FALSE(forecast.current.is_day);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.temp);
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, forecast.current.weather.condition);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.current.soil_temperature_18cm);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, forecast.daily[0].temp.min);
}

/* Garbage and empty input are reported as deserialization errors. */
static void test_invalid_json(void) {
  forecast_t forecast = {};
  ProviderResult err = parseJson("this is not json", forecast);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_TRUE(err.detail().startsWith(TXT_DESERIALIZATION_ERROR_INVALID_INPUT));

  err = parseJson("", forecast);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT, err.detail().c_str());
}

/* EOF seen through Peek() must be recorded too: rapidjson signals end of
 * input via '\0' from Peek() before any failing Take(), so a truncated body
 * detected that way still classifies as IncompleteInput. */
static void test_stream_input_eof_flag(void) {
  String data("ab");
  StringStream ss(data);
  StreamInput input(ss);
  TEST_ASSERT_FALSE(input.reachedEof());
  TEST_ASSERT_EQUAL_CHAR('a', input.Peek());
  TEST_ASSERT_EQUAL_CHAR('a', input.Take());
  TEST_ASSERT_EQUAL_CHAR('b', input.Take());
  TEST_ASSERT_FALSE(input.reachedEof());
  TEST_ASSERT_EQUAL_CHAR('\0', input.Peek());
  TEST_ASSERT_TRUE(input.reachedEof());

  // Exhausted stream from the start: Peek() records it immediately.
  String emptyData("");
  StringStream empty(emptyData);
  StreamInput emptyInput(empty);
  TEST_ASSERT_EQUAL_CHAR('\0', emptyInput.Peek());
  TEST_ASSERT_TRUE(emptyInput.reachedEof());
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_get_api_name);
  RUN_TEST(test_map_weather_code);
  RUN_TEST(test_lima_current);
  RUN_TEST(test_lima_hourly);
  RUN_TEST(test_lima_daily);
  RUN_TEST(test_hourly_and_daily_cap);
  RUN_TEST(test_shorter_arrays);
  RUN_TEST(test_optional_fields_present);
  RUN_TEST(test_missing_optional_fields);
  RUN_TEST(test_minimal_forecast_keys_accepted);
  RUN_TEST(test_non_forecast_payloads_rejected);
  RUN_TEST(test_invalid_json);
  RUN_TEST(test_stream_input_eof_flag);
  UNITY_END();
}

void loop() {}
