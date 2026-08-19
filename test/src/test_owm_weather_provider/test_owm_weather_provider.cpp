/* Unit tests for the OpenWeatherMap weather provider (condition mapping).
 *
 * The OWM weather condition ids are mapped onto the unified
 * weather_condition enum by OWMWeatherProvider::mapWeatherCode; every id of
 * the OWM "weather conditions" reference must resolve to the right
 * condition, with unknown ids falling back to the range defaults or UNKNOWN.
 *
 * References:
 *   https://openweathermap.org/weather-conditions
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "owm_weather_provider.h"

void setUp(void) {}
void tearDown(void) {}

/* Group 2xx: Thunderstorm. */
static void test_map_thunderstorm(void) {
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(200));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(201));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(202));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(210));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(211));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(212));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(221));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM_HAIL, OWMWeatherProvider::mapWeatherCode(230));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM_HAIL, OWMWeatherProvider::mapWeatherCode(231));
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM_HAIL, OWMWeatherProvider::mapWeatherCode(232));
}

/* Group 3xx: Drizzle. */
static void test_map_drizzle(void) {
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(300));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(301));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(302));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(310));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(311));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(312));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(313));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(314));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(321));
}

/* Group 5xx: Rain. */
static void test_map_rain(void) {
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(500));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(501));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(502));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(503));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(504));
  TEST_ASSERT_EQUAL(weather_condition::FREEZING_RAIN, OWMWeatherProvider::mapWeatherCode(511));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OWMWeatherProvider::mapWeatherCode(520));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OWMWeatherProvider::mapWeatherCode(521));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OWMWeatherProvider::mapWeatherCode(522));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SHOWERS, OWMWeatherProvider::mapWeatherCode(531));
}

/* Group 6xx: Snow. */
static void test_map_snow(void) {
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OWMWeatherProvider::mapWeatherCode(600));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OWMWeatherProvider::mapWeatherCode(601));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OWMWeatherProvider::mapWeatherCode(602));
  TEST_ASSERT_EQUAL(weather_condition::SLEET, OWMWeatherProvider::mapWeatherCode(611));
  TEST_ASSERT_EQUAL(weather_condition::SLEET, OWMWeatherProvider::mapWeatherCode(612));
  TEST_ASSERT_EQUAL(weather_condition::SLEET, OWMWeatherProvider::mapWeatherCode(613));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OWMWeatherProvider::mapWeatherCode(615));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OWMWeatherProvider::mapWeatherCode(616));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OWMWeatherProvider::mapWeatherCode(620));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OWMWeatherProvider::mapWeatherCode(621));
  TEST_ASSERT_EQUAL(weather_condition::RAIN_SNOW_MIX, OWMWeatherProvider::mapWeatherCode(622));
}

/* Group 7xx: Atmosphere. */
static void test_map_atmosphere(void) {
  TEST_ASSERT_EQUAL(weather_condition::MIST, OWMWeatherProvider::mapWeatherCode(701));
  TEST_ASSERT_EQUAL(weather_condition::SMOKE, OWMWeatherProvider::mapWeatherCode(711));
  TEST_ASSERT_EQUAL(weather_condition::HAZE, OWMWeatherProvider::mapWeatherCode(721));
  TEST_ASSERT_EQUAL(weather_condition::SAND_WHIRLS, OWMWeatherProvider::mapWeatherCode(731));
  TEST_ASSERT_EQUAL(weather_condition::FOG, OWMWeatherProvider::mapWeatherCode(741));
  TEST_ASSERT_EQUAL(weather_condition::SAND, OWMWeatherProvider::mapWeatherCode(751));
  TEST_ASSERT_EQUAL(weather_condition::DUST, OWMWeatherProvider::mapWeatherCode(761));
  TEST_ASSERT_EQUAL(weather_condition::ASH, OWMWeatherProvider::mapWeatherCode(762));
  TEST_ASSERT_EQUAL(weather_condition::SQUALL, OWMWeatherProvider::mapWeatherCode(771));
  TEST_ASSERT_EQUAL(weather_condition::TORNADO, OWMWeatherProvider::mapWeatherCode(781));
}

/* Group 800/80x: Clear sky and clouds. */
static void test_map_clear_and_clouds(void) {
  TEST_ASSERT_EQUAL(weather_condition::CLEAR, OWMWeatherProvider::mapWeatherCode(800));
  TEST_ASSERT_EQUAL(weather_condition::PARTLY_CLOUDY, OWMWeatherProvider::mapWeatherCode(801));
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, OWMWeatherProvider::mapWeatherCode(802));
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, OWMWeatherProvider::mapWeatherCode(803));
  TEST_ASSERT_EQUAL(weather_condition::OVERCAST, OWMWeatherProvider::mapWeatherCode(804));
}

/* Unknown ids: fall back to the group default, or UNKNOWN outside the
 * 2xx-8xx range. */
static void test_map_unknown(void) {
  TEST_ASSERT_EQUAL(weather_condition::THUNDERSTORM, OWMWeatherProvider::mapWeatherCode(299));
  TEST_ASSERT_EQUAL(weather_condition::DRIZZLE, OWMWeatherProvider::mapWeatherCode(399));
  TEST_ASSERT_EQUAL(weather_condition::RAIN, OWMWeatherProvider::mapWeatherCode(599));
  TEST_ASSERT_EQUAL(weather_condition::SNOW, OWMWeatherProvider::mapWeatherCode(699));
  TEST_ASSERT_EQUAL(weather_condition::FOG, OWMWeatherProvider::mapWeatherCode(799));
  TEST_ASSERT_EQUAL(weather_condition::CLOUDY, OWMWeatherProvider::mapWeatherCode(899));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OWMWeatherProvider::mapWeatherCode(0));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OWMWeatherProvider::mapWeatherCode(100));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OWMWeatherProvider::mapWeatherCode(900));
  TEST_ASSERT_EQUAL(weather_condition::UNKNOWN, OWMWeatherProvider::mapWeatherCode(-1));
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_map_thunderstorm);
  RUN_TEST(test_map_drizzle);
  RUN_TEST(test_map_rain);
  RUN_TEST(test_map_snow);
  RUN_TEST(test_map_atmosphere);
  RUN_TEST(test_map_clear_and_clouds);
  RUN_TEST(test_map_unknown);
  UNITY_END();
}

void loop() {}