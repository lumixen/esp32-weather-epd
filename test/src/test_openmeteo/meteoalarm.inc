/* Unit tests for the MeteoAlarm alert provider (feed parsing).
 *
 * The fixtures are exercised with the real Arduino String inside the ESP32
 * QEMU emulator. The primary fixture (feed_ukraine_real.inc) is a verbatim
 * excerpt of the live Ukraine feed captured on 2026-08-07.
 *
 * Copyright (C) 2026  Max Bodaniuk
 *
 * GPL-3.0, see LICENSE.
 */

#include <cstdio>
#include <cstring>
#include <unity.h>

#include "_locale.h"
#include "data_models.h"
#include "meteoalarm_alert_provider.h"
#include "feed_ukraine_real.inc"
#include "feed_ukraine_latest.inc"
#include "../test_harness.h"

namespace meteoalarm_tests {

// Fixed clock for deterministic expiry filtering: 2026-08-07 18:00:00 UTC.
static const int64_t kNow = 1786125600LL;
// Epoch of the real fixture timestamps.
static const int64_t kSquallStart = 1786099460LL;  // 2026-08-07T10:44:20+00:00
static const int64_t kSquallEnd = 1786125600LL;    // 2026-08-07T18:00:00+00:00
static const int64_t kRainStart = 1786168800LL;    // 2026-08-08T06:00:00+00:00
static const int64_t kRainEnd = 1786212000LL;      // 2026-08-08T18:00:00+00:00

void setUp(void) {}
void tearDown(void) {}

static ProviderResult parseFeed(const String &xml, std::vector<weather_alert_t> &alerts, int64_t now, double lat = NAN,
                                double lon = NAN) {
  MeteoAlarmAlertProvider::FeedParser parser(alerts, now, lat, lon);
  parser.feed(xml.c_str(), xml.length());
  return parser.finish();
}

static void test_hazard_from_event(void) {
  TEST_ASSERT_EQUAL_STRING("Squall", MeteoAlarmAlertProvider::hazardFromEvent("Squall warning").c_str());
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", MeteoAlarmAlertProvider::hazardFromEvent("Thunderstormwarning").c_str());
  TEST_ASSERT_EQUAL_STRING("Rain", MeteoAlarmAlertProvider::hazardFromEvent("Yellow Rain Warning").c_str());
  TEST_ASSERT_EQUAL_STRING("High-temperature",
                           MeteoAlarmAlertProvider::hazardFromEvent("Orange High-temperature Warning").c_str());
  TEST_ASSERT_EQUAL_STRING("Wind", MeteoAlarmAlertProvider::hazardFromEvent("Red Wind Warning").c_str());
  TEST_ASSERT_EQUAL_STRING("Heavy rain", MeteoAlarmAlertProvider::hazardFromEvent("Heavy rain warning").c_str());
  TEST_ASSERT_EQUAL_STRING("Wind", MeteoAlarmAlertProvider::hazardFromEvent("  wind  warning  ").c_str());
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::hazardFromEvent("Yellow Warning").isEmpty());
}

static void test_color_from_severity(void) {
  TEST_ASSERT_EQUAL_STRING("Yellow", MeteoAlarmAlertProvider::colorFromSeverity("Minor").c_str());
  TEST_ASSERT_EQUAL_STRING("Yellow", MeteoAlarmAlertProvider::colorFromSeverity("Moderate").c_str());
  TEST_ASSERT_EQUAL_STRING("Orange", MeteoAlarmAlertProvider::colorFromSeverity("Severe").c_str());
  TEST_ASSERT_EQUAL_STRING("Red", MeteoAlarmAlertProvider::colorFromSeverity("Extreme").c_str());
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::colorFromSeverity("").isEmpty());
}

/* Parse the real Ukraine feed excerpt: two entries (Squall covering Kyiv
 * oblast, Heavy rain covering Kharkiv oblast). The entry <title>s (e.g.
 * "Yellow Wind Warning issued for Ukraine - ...") must never leak into the
 * alert events, and the huge <cap:polygon> elements are captured without
 * disturbing the other fields.
 */
static void test_parse_real_feed(void) {
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(kFeedUkraineReal, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());

  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(kSquallEnd, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("squall", alerts[0].tags.c_str());

  TEST_ASSERT_EQUAL_STRING("Yellow Heavy rain Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_INT64(kRainStart, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(kRainEnd, alerts[1].end);
  TEST_ASSERT_EQUAL_STRING("heavy rain", alerts[1].tags.c_str());
}

/* Warnings whose expiry lies in the past are dropped, unless the clock was
 * never synchronized (epoch < 2021). */
static void test_expired_skip(void) {
  std::vector<weather_alert_t> alerts;

  // kNow is before the Heavy rain expiry, after the Squall expiry.
  ProviderResult err = parseFeed(kFeedUkraineReal, alerts, kNow + 1);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Heavy rain Warning", alerts[0].event.c_str());

  // Beyond both expiries: nothing left.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, kRainEnd + 1);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());

  // Unsynced clock: expiry filtering is disabled, everything is kept.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, 0);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
}

/* When the onset element is missing, the effective time is used instead. */
static void test_onset_fallback(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Wind warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_INT64(kSquallStart + 1, alerts[0].start);  // effective time
  TEST_ASSERT_EQUAL_INT64(kSquallEnd, alerts[0].end);
}

/* XML entities in captured elements are decoded into the event text. */
static void test_entities(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Rivne &amp; Volyn oblast</cap:areaDesc>
    <cap:event>Wind &amp; wave warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Wind & wave Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("wind & wave", alerts[0].tags.c_str());
}

/* A feed that ends inside an entry (truncated by a timeout) is reported as
 * invalid instead of silently yielding partial results. */
static void test_truncated_feed(void) {
  std::vector<weather_alert_t> alerts;

  String cut = kFeedUkraineReal;
  cut.remove(cut.length() - 8);  // drop "</feed>\n" but keep the closed entries
  ProviderResult err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());

  // Cut inside the second entry: the feed is truncated while parsing.
  alerts.clear();
  cut = kFeedUkraineReal;
  const int secondEntry = cut.indexOf("<cap:event>", cut.indexOf("<cap:event>") + 1);
  cut.remove(secondEntry + 13);
  err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_FALSE(err.isOk());
  TEST_ASSERT_EQUAL_STRING(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT, err.detail().c_str());

  // Cut right after the first entry: the second entry never opens.
  alerts.clear();
  cut = kFeedUkraineReal;
  cut.remove(cut.indexOf("<entry>", cut.indexOf("<entry>") + 1) - 10);
  err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_FALSE(err.isOk());
}

/* Garbage and empty input parse as valid feeds without alerts. */
static void test_garbage(void) {
  std::vector<weather_alert_t> alerts;

  ProviderResult err = parseFeed("", alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());

  alerts.clear();
  err = parseFeed("this is not xml at all", alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());
}

/* No more than 2 alerts are kept (the renderer displays at most 2): feed()
 * stops updating any state as soon as that many distinct hazards have been
 * collected, so entries fed afterwards (there are 64 in this fixture) are
 * ignored rather than parsed. */
static void test_alert_cap(void) {
  String feed = "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:cap=\"urn:oasis:names:tc:emergency:cap:1.2\">";
  const char *hazards[] = {"Wind warning", "Rain warning", "Thunderstorm warning", "Hail warning"};
  for (int i = 0; i < 64; ++i) {
    feed += "<entry><cap:areaDesc>Area " + String(i) + "</cap:areaDesc><cap:event>" + hazards[i % 4] + "</cap:event>";
    feed += "<cap:effective>2026-08-07T10:44:21+00:00</cap:effective>";
    feed += "<cap:expires>2026-08-07T18:00:00+00:00</cap:expires><cap:severity>Moderate</cap:severity></entry>";
  }
  feed += "</feed>";

  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Wind Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("Yellow Rain Warning", alerts[1].event.c_str());
}

/* Severity colors: with more than 2 alerts only the first two distinct
 * hazards in feed order are kept (the renderer displays at most 2). The full
 * color mapping is covered by test_color_from_severity. */
static void test_severity_colors(void) {
  String feed = "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:cap=\"urn:oasis:names:tc:emergency:cap:1.2\">";
  struct {
    const char *event;
    const char *severity;
  } sevs[] = {{"Thunderstormwarning", "Severe"},
              {"Squall warning", "Extreme"},
              {"Hail warning", "Unknown"},
              {"Wind warning", ""}};
  for (const auto &s : sevs) {
    feed += "<entry><cap:areaDesc>Area</cap:areaDesc><cap:event>";
    feed += s.event;
    feed += "</cap:event>";
    feed += "<cap:effective>2026-08-07T10:44:21+00:00</cap:effective>";
    feed += "<cap:expires>2026-08-07T18:00:00+00:00</cap:expires>";
    if (s.severity[0] != '\0') {
      feed += "<cap:severity>";
      feed += s.severity;
      feed += "</cap:severity>";
    }
    feed += "</entry>";
  }
  feed += "</feed>";

  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Orange Thunderstorm Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("Red Squall Warning", alerts[1].event.c_str());
}

/* Point-in-polygon: ray casting on a square ring. The ring is closed in the
 * CAP style (last point repeats the first). Points on the boundary count as
 * inside; degenerate polygons keep the alert (true). */
static void test_point_in_polygon(void) {
  const String square = "0,0 0,4 4,4 4,0 0,0";
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 2.0, square));
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(0.1, 0.1, square));
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 0.0, square));  // on the edge
  TEST_ASSERT_FALSE(MeteoAlarmAlertProvider::pointInPolygon(6.0, 2.0, square));
  TEST_ASSERT_FALSE(MeteoAlarmAlertProvider::pointInPolygon(-2.0, 2.0, square));
  TEST_ASSERT_FALSE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 6.0, square));

  // Open ring: the closing edge is synthesized.
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 2.0, "0,0 0,4 4,4 4,0"));

  // Degenerate input can not be evaluated and keeps the alert.
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 2.0, ""));
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 2.0, "0,0 1,1"));
  TEST_ASSERT_TRUE(MeteoAlarmAlertProvider::pointInPolygon(2.0, 2.0, "not a polygon"));
}

/* With a location, only warnings whose polygon contains it survive. The
 * Squall polygon covers western Ukraine, the Heavy rain polygon covers
 * central-east Ukraine. */
static void test_polygon_filter_fixture(void) {
  std::vector<weather_alert_t> alerts;

  // (51.1, 24.85) lies inside the Squall polygon but not the Heavy rain one.
  ProviderResult err = parseFeed(kFeedUkraineReal, alerts, kNow, 51.1, 24.85);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());

  // Berlin (52.52, 13.40) is outside both polygons.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, kNow, 52.52, 13.40);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());
}

/* Warnings without a polygon can not be located and are kept even when a
 * location is configured. */
static void test_polygon_missing_kept(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Wind warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow, 52.52, 13.40);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
}

/* Same-hazard warnings with different time ranges (e.g. a squall today and
 * another one tomorrow) are merged into a single alert whose validity span
 * is the union of both windows. */
static void test_merge_union_span(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Wind warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T12:00:00+00:00</cap:onset>
    <cap:expires>2026-08-07T21:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Wind warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-08T06:00:00+00:00</cap:onset>
    <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Wind Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("wind", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(1786104000LL, alerts[0].start);  // 2026-08-07T12:00:00Z
  TEST_ASSERT_EQUAL_INT64(1786212000LL, alerts[0].end);    // 2026-08-08T18:00:00Z
}

/* A duplicated hazard must not consume one of the 2 alert slots: Squall
 * (x2, different time windows) + Hail collect the same two slots as Squall
 * + Hail, and parsing stops after the second distinct hazard. */
static void test_merge_distinct_hazards(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Squall warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T12:00:00+00:00</cap:onset>
    <cap:expires>2026-08-07T21:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Squall warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-08T06:00:00+00:00</cap:onset>
    <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Hail warning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T19:00:00+00:00</cap:onset>
    <cap:expires>2026-08-08T06:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("squall", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(1786104000LL, alerts[0].start);  // union of both windows
  TEST_ASSERT_EQUAL_INT64(1786212000LL, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("Yellow Hail Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_STRING("hail", alerts[1].tags.c_str());
}

/* When a later entry of the same hazard is more urgent, the merged alert
 * adopts the higher severity color (span stays the union). */
static void test_merge_severity_upgrade(void) {
  const char *feed = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Thunderstormwarning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T12:00:00+00:00</cap:onset>
    <cap:expires>2026-08-07T21:00:00+00:00</cap:expires>
    <cap:severity>Moderate</cap:severity>
  </entry>
  <entry>
    <cap:areaDesc>Test region</cap:areaDesc>
    <cap:event>Thunderstormwarning</cap:event>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-08T06:00:00+00:00</cap:onset>
    <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>
    <cap:severity>Severe</cap:severity>
  </entry>
</feed>
)FEED";
  std::vector<weather_alert_t> alerts;
  ProviderResult err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Orange Thunderstorm Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("thunderstorm", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(1786104000LL, alerts[0].start);  // union of both windows
  TEST_ASSERT_EQUAL_INT64(1786212000LL, alerts[0].end);
}

/* Fresh real feed excerpt (2026-08-08, trimmed from 131 entries): of the
 * six entries, four cover a point in western Ukraine (51.1, 24.85) — Squall,
 * Hail and two Thunderstorm warnings — and two do not. Parsing stops at the
 * 2nd matching warning (the renderer displays at most 2). */
static void test_latest_feed_polygon_filter(void) {
  std::vector<weather_alert_t> alerts;

  ProviderResult err = parseFeed(kFeedUkraineLatest, alerts, kNow, 51.1, 24.85);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());

  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("squall", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[0].end);

  TEST_ASSERT_EQUAL_STRING("Yellow Hail Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_STRING("hail", alerts[1].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[1].end);

  // Berlin is outside all six polygons.
  alerts.clear();
  err = parseFeed(kFeedUkraineLatest, alerts, kNow, 52.52, 13.40);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());
}

/* Expiry at the boundary of the fresh fixture: three entries expire exactly
 * at kNow and drop at kNow+1. At kNow the two Squall duplicates merge into
 * the first slot (union of identical spans) and Hail fills the second; at
 * kNow+1 the two surviving entries are the Heavy rain and Thunderstorm
 * warnings. */
static void test_latest_feed_expiry(void) {
  std::vector<weather_alert_t> alerts;

  ProviderResult err = parseFeed(kFeedUkraineLatest, alerts, kNow);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("squall", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("Yellow Hail Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_STRING("hail", alerts[1].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[1].end);

  alerts.clear();
  err = parseFeed(kFeedUkraineLatest, alerts, kNow + 1);
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Heavy rain Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("heavy rain", alerts[0].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kRainStart, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(kRainEnd, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("Yellow Thunderstorm Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_STRING("thunderstorm", alerts[1].tags.c_str());
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(kRainStart, alerts[1].end);
}

/* Synthesizer for a feed the size of the live Ukraine document (~450 KB).
 * The document is generated on demand in small chunks: a ~450 KB String does
 * not fit into the 320 KB RAM of the device, while esp_http_client delivers
 * the real body to FeedParser::feed() in bounded chunks anyway. The bulk of
 * the size comes from long <title> texts (skipped byte-by-byte by the
 * parser, like the real feed's long entries), not from oversized polygons:
 * point-in-polygon has to run for every entry and would dominate the
 * emulated runtime otherwise. The total length is computed up front so the
 * test can assert that every generated chunk was actually fed to the
 * parser. */
static const size_t kSynthChunk = 128;
static const size_t kSynthFillerReps = 80;

/* Generates the synthetic feed in small chunks on demand, mirroring how
 * esp_http_client's HTTP_EVENT_ON_DATA callback delivers the real feed to
 * FeedParser::feed() -- one bounded chunk at a time, never the whole
 * document at once. */
class SyntheticFeedGenerator {
 public:
  explicit SyntheticFeedGenerator(size_t entries, double lat, double lon)
      : entries_(entries),
        lat_(lat),
        lon_(lon),
        filler_("Vulnerability: strong winds with gusts up to 90 km/h. The wind may cause damage "
                "to roofs, trees and outdoor structures. Secure loose objects and avoid forested "
                "areas. This warning is issued for informational purposes and does not imply that "
                "any particular event is likely to occur. ") {
    // Dry run: compute the total generated length before any chunk is handed out.
    while (fillChunk()) {
    }
    dryRun_ = false;
    stage_ = Stage::kHeader;
    entry_ = 0;
    fillReps_ = 0;
    fillOff_ = 0;
    entryStage_ = EntryStage::kOpen;
    chunk_ = "";
  }

  // Fills `out` with the next chunk and returns true, or returns false once
  // the whole document has been generated.
  bool nextChunk(String &out) {
    if (!fillChunk()) {
      return false;
    }
    out = chunk_;
    return true;
  }
  size_t totalLength() const { return total_; }

 private:
  enum class Stage { kHeader, kEntry, kFooter, kDone };
  enum class EntryStage { kOpen, kFiller };
  Stage stage_ = Stage::kHeader;
  EntryStage entryStage_ = EntryStage::kOpen;
  size_t entries_ = 0;
  double lat_ = 0;
  double lon_ = 0;
  size_t entry_ = 0;
  size_t fillReps_ = 0;
  size_t fillOff_ = 0;
  bool dryRun_ = true;
  size_t total_ = 0;
  String chunk_;
  String filler_;

  bool fillChunk() {
    chunk_ = "";
    while (chunk_.length() < kSynthChunk && stage_ != Stage::kDone) {
      if (stage_ == Stage::kHeader) {
        chunk_ += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        chunk_ += "<feed xmlns=\"http://www.w3.org/2005/Atom\" "
                  "xmlns:cap=\"urn:oasis:names:tc:emergency:cap:1.2\">\n";
        stage_ = entries_ > 0 ? Stage::kEntry : Stage::kFooter;
      } else if (stage_ == Stage::kEntry) {
        emitEntry();
      } else if (stage_ == Stage::kFooter) {
        chunk_ += "</feed>\n";
        stage_ = Stage::kDone;
      }
    }
    if (dryRun_) {
      total_ += chunk_.length();
    }
    return chunk_.length() > 0;
  }

  void emitEntry() {
    if (entryStage_ == EntryStage::kOpen) {
      if (entry_ + 1 == entries_) {
        // The only warning covering the test location, placed last so the
        // whole document has to be read before it is found.
        char ring[160];
        snprintf(ring, sizeof(ring), "<cap:polygon>%.2f,%.2f %.2f,%.2f %.2f,%.2f %.2f,%.2f %.2f,%.2f</cap:polygon>\n",
                 lat_ - 0.01, lon_ - 0.01, lat_ - 0.01, lon_ + 0.01, lat_ + 0.01, lon_ + 0.01, lat_ + 0.01, lon_ - 0.01,
                 lat_ - 0.01, lon_ - 0.01);
        chunk_ += "<entry>\n  <cap:areaDesc>Kyiv oblast</cap:areaDesc>\n";
        chunk_ += "  <cap:event>Thunderstormwarning</cap:event>\n";
        chunk_ += "  <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>\n";
        chunk_ += "  <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>\n";
        chunk_ += "  <cap:severity>Moderate</cap:severity>\n";
        chunk_ += ring;
      } else {
        chunk_ += "<entry>\n  <cap:areaDesc>Distant region " + String(entry_) + "</cap:areaDesc>\n";
        chunk_ += "  <cap:event>Wind warning</cap:event>\n";
        chunk_ += "  <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>\n";
        chunk_ += "  <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>\n";
        chunk_ += "  <cap:severity>Moderate</cap:severity>\n";
        // A small ring far away from the test location.
        chunk_ += "<cap:polygon>40.00,10.00 40.01,10.00 40.01,10.01 40.00,10.01 "
                  "40.00,10.00</cap:polygon>\n";
      }
      chunk_ += "<title>";
      fillReps_ = kSynthFillerReps;
      entryStage_ = EntryStage::kFiller;
    }
    if (entryStage_ == EntryStage::kFiller) {
      while (fillReps_ > 0 && chunk_.length() < kSynthChunk) {
        // A rep is one full copy of the filler text: the copy may span
        // several chunks, so only a completed copy counts.
        const size_t take = min(kSynthChunk - chunk_.length(), filler_.length() - fillOff_);
        chunk_.concat(filler_.c_str() + fillOff_, take);
        fillOff_ += take;
        if (fillOff_ == filler_.length()) {
          fillOff_ = 0;
          --fillReps_;
        }
      }
      if (fillReps_ == 0) {
        chunk_ += "</title>\n</entry>\n";
        ++entry_;
        entryStage_ = EntryStage::kOpen;
        if (entry_ == entries_) {
          stage_ = Stage::kFooter;
        }
      }
    }
  }
};

/* A feed the size of the live Ukraine document (539 KB) that contains no
 * matching warning until the very last entry: the alert cap never fires and
 * the parser has to consume the whole body, fed in small chunks exactly as
 * esp_http_client's event handler would deliver them. On the device this is
 * exactly the scenario that outran the old 2 s read window
 * (HTTP_CLIENT_TCP_TIMEOUT) and surfaced as -259 InvalidInput. */
static void test_large_feed_full_consumption(void) {
  SyntheticFeedGenerator gen(20, 50.45, 30.52);
  TEST_ASSERT(gen.totalLength() > 400 * 1024);

  std::vector<weather_alert_t> alerts;
  MeteoAlarmAlertProvider::FeedParser parser(alerts, kNow, 50.45, 30.52);
  size_t fed = 0;
  String chunk;
  while (gen.nextChunk(chunk)) {
    parser.feed(chunk.c_str(), chunk.length());
    fed += chunk.length();
  }
  ProviderResult err = parser.finish();
  TEST_ASSERT_TRUE(err.isOk());
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Thunderstorm Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("thunderstorm", alerts[0].tags.c_str());
  // The whole body was fed and consumed: the last entry (containing the only
  // matching warning) is reached only once every chunk has been processed.
  TEST_ASSERT_EQUAL_UINT(gen.totalLength(), fed);
}

void registerTests() {
  test_harness::selectCallbacks(setUp, tearDown);
  RUN_TEST(meteoalarm_tests::test_hazard_from_event);
  RUN_TEST(meteoalarm_tests::test_color_from_severity);
  RUN_TEST(meteoalarm_tests::test_parse_real_feed);
  RUN_TEST(meteoalarm_tests::test_expired_skip);
  RUN_TEST(meteoalarm_tests::test_onset_fallback);
  RUN_TEST(meteoalarm_tests::test_entities);
  RUN_TEST(meteoalarm_tests::test_truncated_feed);
  RUN_TEST(meteoalarm_tests::test_garbage);
  RUN_TEST(meteoalarm_tests::test_alert_cap);
  RUN_TEST(meteoalarm_tests::test_severity_colors);
  RUN_TEST(meteoalarm_tests::test_point_in_polygon);
  RUN_TEST(meteoalarm_tests::test_polygon_filter_fixture);
  RUN_TEST(meteoalarm_tests::test_polygon_missing_kept);
  RUN_TEST(meteoalarm_tests::test_merge_union_span);
  RUN_TEST(meteoalarm_tests::test_merge_distinct_hazards);
  RUN_TEST(meteoalarm_tests::test_merge_severity_upgrade);
  RUN_TEST(meteoalarm_tests::test_latest_feed_polygon_filter);
  RUN_TEST(meteoalarm_tests::test_latest_feed_expiry);
  RUN_TEST(meteoalarm_tests::test_large_feed_full_consumption);
}

}  // namespace meteoalarm_tests
