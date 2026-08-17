/* Unit tests for alert icon category selection (getAlertCategory).
 *
 * The terminology lists live in the locale includes; getAlertCategory maps
 * the event text to the icon category used by the renderer. Events reach
 * this function lowercased (drawAlerts runs filterAlerts first), so the
 * fixtures mirror that.
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "display_utils.h"

void setUp(void) {}
void tearDown(void) {}

static weather_alert_t makeAlert(const char *event) {
  weather_alert_t alert = {};
  alert.event = event;
  return alert;
}

/* A squall is a wind hazard: it must resolve to the strong wind icon (the
 * MeteoAlarm feed titles these entries "Wind Warning"). */
static void test_squall_is_strong_wind(void) {
  weather_alert_t alert = makeAlert("yellow squall warning");
  alert.event.toLowerCase();
  TEST_ASSERT_EQUAL(alert_category::STRONG_WIND, getAlertCategory(alert));
}

static void test_wind_is_strong_wind(void) {
  weather_alert_t alert = makeAlert("yellow wind warning");
  alert.event.toLowerCase();
  TEST_ASSERT_EQUAL(alert_category::STRONG_WIND, getAlertCategory(alert));
}

/* A squall line is a line of thunderstorms: it must keep resolving to
 * LIGHTNING, which is checked before STRONG_WIND in getAlertCategory. */
static void test_squall_line_is_lightning(void) {
  weather_alert_t alert = makeAlert("yellow squall line warning");
  alert.event.toLowerCase();
  TEST_ASSERT_EQUAL(alert_category::LIGHTNING, getAlertCategory(alert));
}

/* Unmatched events fall back to the generic warning icon. */
static void test_unmatched_is_not_found(void) {
  weather_alert_t alert = makeAlert("yellow dangerous warning");
  alert.event.toLowerCase();
  TEST_ASSERT_EQUAL(alert_category::NOT_FOUND, getAlertCategory(alert));
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_squall_is_strong_wind);
  RUN_TEST(test_wind_is_strong_wind);
  RUN_TEST(test_squall_line_is_lightning);
  RUN_TEST(test_unmatched_is_not_found);
  UNITY_END();
}

void loop() {}
