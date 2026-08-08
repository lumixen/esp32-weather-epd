/* Unit tests for the MeteoAlarm alert provider (feed parsing).
 *
 * The fixtures are exercised with the real Arduino String/Stream inside the
 * ESP32 QEMU emulator. The primary fixture (feed_ukraine_real.inc) is a
 * verbatim excerpt of the live Ukraine feed captured on 2026-08-07.
 *
 * GPL-3.0, see LICENSE.
 */

#include <unity.h>

#include "data_models.h"
#include "meteoalarm_alert_provider.h"
#include "feed_ukraine_real.inc"
#include "feed_ukraine_latest.inc"

// Fixed clock for deterministic expiry filtering: 2026-08-07 18:00:00 UTC.
static const int64_t kNow = 1786125600LL;
// Epoch of the real fixture timestamps.
static const int64_t kSquallStart = 1786099460LL;   // 2026-08-07T10:44:20+00:00
static const int64_t kSquallEnd = 1786125600LL;     // 2026-08-07T18:00:00+00:00
static const int64_t kRainStart = 1786168800LL;     // 2026-08-08T06:00:00+00:00
static const int64_t kRainEnd = 1786212000LL;       // 2026-08-08T18:00:00+00:00

// Minimal read-only Stream over a String; framework's StreamString does not
// expose String assignment in this Arduino core.
class StringStream : public Stream {
 public:
  explicit StringStream(const String &s) : data_(s), pos_(0), bytesRead_(0) {}
  int read() override {
    if (pos_ < data_.length()) {
      ++bytesRead_;
      return data_[pos_++];
    }
    return -1;
  }
  int peek() override { return pos_ < data_.length() ? data_[pos_] : -1; }
  int available() override { return data_.length() - pos_; }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}
  size_t bytesRead() const { return bytesRead_; }

 private:
  String data_;
  size_t pos_;
  size_t bytesRead_;
};

void setUp(void) {}
void tearDown(void) {}

static DeserializationError parseFeed(const String &xml, std::vector<weather_alert_t> &alerts,
                                      int64_t now, double lat = NAN, double lon = NAN,
                                      size_t expectedLen = 0) {
  StringStream ss(xml);
  return MeteoAlarmAlertProvider::parseFeed(ss, alerts, now, lat, lon, expectedLen);
}

static void test_parse_iso8601(void) {
  TEST_ASSERT_EQUAL_INT64(kSquallStart, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T10:44:20Z"));
  TEST_ASSERT_EQUAL_INT64(kSquallStart, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T10:44:20+00:00"));
  TEST_ASSERT_EQUAL_INT64(kSquallStart, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T12:44:20+02:00"));
  TEST_ASSERT_EQUAL_INT64(kSquallStart, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T05:14:20-05:30"));
  TEST_ASSERT_EQUAL_INT64(1709164800LL, MeteoAlarmAlertProvider::parseIso8601("2024-02-29T00:00:00Z"));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601(""));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601("2026-08-07"));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T10:44:20X"));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T10:44:20+02"));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601("2026-08-07T10:44:20+02:0"));
  TEST_ASSERT_EQUAL_INT64(-1, MeteoAlarmAlertProvider::parseIso8601("abcd-ef-ghTij:kl:mnZ"));
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
  DeserializationError err = parseFeed(kFeedUkraineReal, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  DeserializationError err = parseFeed(kFeedUkraineReal, alerts, kNow + 1);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Heavy rain Warning", alerts[0].event.c_str());

  // Beyond both expiries: nothing left.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, kRainEnd + 1);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());

  // Unsynced clock: expiry filtering is disabled, everything is kept.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, 0);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  DeserializationError err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  DeserializationError err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  DeserializationError err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());

  // Cut inside the second entry: the feed is truncated while parsing.
  alerts.clear();
  cut = kFeedUkraineReal;
  const int secondEntry = cut.indexOf("<cap:event>", cut.indexOf("<cap:event>") + 1);
  cut.remove(secondEntry + 13);
  err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::InvalidInput);

  // Cut right after the first entry: the second entry never opens.
  alerts.clear();
  cut = kFeedUkraineReal;
  cut.remove(cut.indexOf("<entry>", cut.indexOf("<entry>") + 1) - 10);
  err = parseFeed(cut, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::InvalidInput);
}

/* Garbage and empty input parse as valid feeds without alerts. */
static void test_garbage(void) {
  std::vector<weather_alert_t> alerts;

  DeserializationError err = parseFeed("", alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());

  alerts.clear();
  err = parseFeed("this is not xml at all", alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());
}

/* No more than 2 alerts are kept (the renderer displays at most 2), and
 * parsing stops as soon as that many have been collected: the remaining body
 * is not read, cutting the download short. */
static void test_alert_cap(void) {
  String feed = "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:cap=\"urn:oasis:names:tc:emergency:cap:1.2\">";
  for (int i = 0; i < 9; ++i) {
    feed += "<entry><cap:areaDesc>Area " + String(i) + "</cap:areaDesc><cap:event>Wind warning</cap:event>";
    feed += "<cap:effective>2026-08-07T10:44:21+00:00</cap:effective>";
    feed += "<cap:expires>2026-08-07T18:00:00+00:00</cap:expires><cap:severity>Moderate</cap:severity></entry>";
  }
  feed += "</feed>";

  std::vector<weather_alert_t> alerts;
  StringStream ss(feed);
  DeserializationError err = MeteoAlarmAlertProvider::parseFeed(ss, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  for (size_t i = 0; i < alerts.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING("Yellow Wind Warning", alerts[i].event.c_str());
  }
  // The parse stopped as soon as the second entry completed, long before the
  // end of the document.
  TEST_ASSERT(ss.bytesRead() < feed.length());
}

/* Severity colors: with more than 2 alerts only the first two in feed order
 * are kept (the renderer displays at most 2). The full color mapping is
 * covered by test_color_from_severity. */
static void test_severity_colors(void) {
  String feed = "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:cap=\"urn:oasis:names:tc:emergency:cap:1.2\">";
  const char *sevs[] = {"Severe", "Extreme", "Unknown", ""};
  for (const char *sev : sevs) {
    feed += "<entry><cap:areaDesc>Area</cap:areaDesc><cap:event>Thunderstormwarning</cap:event>";
    feed += "<cap:effective>2026-08-07T10:44:21+00:00</cap:effective>";
    feed += "<cap:expires>2026-08-07T18:00:00+00:00</cap:expires>";
    if (sev[0] != '\0') {
      feed += "<cap:severity>";
      feed += sev;
      feed += "</cap:severity>";
    }
    feed += "</entry>";
  }
  feed += "</feed>";

  std::vector<weather_alert_t> alerts;
  DeserializationError err = parseFeed(feed, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Orange Thunderstorm Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_STRING("Red Thunderstorm Warning", alerts[1].event.c_str());
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
  DeserializationError err = parseFeed(kFeedUkraineReal, alerts, kNow, 51.1, 24.85);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());

  // Berlin (52.52, 13.40) is outside both polygons.
  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, kNow, 52.52, 13.40);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  DeserializationError err = parseFeed(feed, alerts, kNow, 52.52, 13.40);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
}

/* Fresh real feed excerpt (2026-08-08, trimmed from 131 entries): of the
 * six entries, four cover a point in western Ukraine (51.1, 24.85) — Squall,
 * Hail and two Thunderstorm warnings — and two do not. Parsing stops at the
 * 2nd matching warning (the renderer displays at most 2). */
static void test_latest_feed_polygon_filter(void) {
  std::vector<weather_alert_t> alerts;

  DeserializationError err = parseFeed(kFeedUkraineLatest, alerts, kNow, 51.1, 24.85);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(0, alerts.size());
}

/* Expiry at the boundary of the fresh fixture: three entries expire exactly
 * at kNow and drop at kNow+1. At kNow the first two entries in feed order
 * (two Squall warnings) are kept; at kNow+1 the two surviving entries are
 * the Heavy rain and Thunderstorm warnings. */
static void test_latest_feed_expiry(void) {
  std::vector<weather_alert_t> alerts;

  DeserializationError err = parseFeed(kFeedUkraineLatest, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[0].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[0].end);
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[1].event.c_str());
  TEST_ASSERT_EQUAL_INT64(kSquallStart, alerts[1].start);
  TEST_ASSERT_EQUAL_INT64(kNow, alerts[1].end);

  alerts.clear();
  err = parseFeed(kFeedUkraineLatest, alerts, kNow + 1);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
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

/* expectedLen stops the read at the content length: when it matches the
 * document length the result is identical to an unlimited parse (no
 * "unexpected end" error even though the stream is not read to EOF); when it
 * cuts inside an entry, that entry is dropped but the parse still succeeds.
 * The read always stops exactly at expectedLen: never past it (a connection
 * closed right after the body must not be probed), so no byte beyond the
 * announced length is ever consumed. */
static void test_expected_len(void) {
  std::vector<weather_alert_t> alerts;

  String realFeed = kFeedUkraineReal;
  StringStream ss(realFeed);
  DeserializationError err =
      MeteoAlarmAlertProvider::parseFeed(ss, alerts, kNow, NAN, NAN, realFeed.length());
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  TEST_ASSERT_EQUAL_UINT(realFeed.length(), ss.bytesRead());

  alerts.clear();
  size_t cut = realFeed.indexOf("</entry>") + 8;  // end of the first entry
  StringStream ss2(realFeed);
  err = MeteoAlarmAlertProvider::parseFeed(ss2, alerts, kNow, NAN, NAN, cut);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(1, alerts.size());
  TEST_ASSERT_EQUAL_STRING("Yellow Squall Warning", alerts[0].event.c_str());
  // The read stopped exactly at the cut, far short of the end of the document.
  TEST_ASSERT_EQUAL_UINT(cut, ss2.bytesRead());

  alerts.clear();
  err = parseFeed(kFeedUkraineReal, alerts, kNow);
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_UINT(2, alerts.size());
}

void setup() {
  delay(200);  // let the emulated UART settle
  UNITY_BEGIN();
  RUN_TEST(test_parse_iso8601);
  RUN_TEST(test_hazard_from_event);
  RUN_TEST(test_color_from_severity);
  RUN_TEST(test_parse_real_feed);
  RUN_TEST(test_expired_skip);
  RUN_TEST(test_onset_fallback);
  RUN_TEST(test_entities);
  RUN_TEST(test_truncated_feed);
  RUN_TEST(test_garbage);
  RUN_TEST(test_alert_cap);
  RUN_TEST(test_severity_colors);
  RUN_TEST(test_point_in_polygon);
  RUN_TEST(test_polygon_filter_fixture);
  RUN_TEST(test_polygon_missing_kept);
  RUN_TEST(test_latest_feed_polygon_filter);
  RUN_TEST(test_latest_feed_expiry);
  RUN_TEST(test_expected_len);
  UNITY_END();
}

void loop() {}
