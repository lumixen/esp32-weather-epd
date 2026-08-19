#include "config.h"
#include "logger.h"

#if defined(AIR_QUALITY_API_PROVIDER_OPEN_METEO)

#include <Arduino.h>
#include <WiFiClient.h>
#if !defined(AIR_QUALITY_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include <cstdint>
#include <cstring>
#include <rapidjson/reader.h>
#include <time.h>
#include "_locale.h"
#include "client_utils.h"
#include "open_meteo_air_quality_provider.h"

/* Perform an HTTP GET request to Open-Meteo's air quality API and map the
 * response into the generic air quality model.
 *
 */
ProviderResult OpenMeteoAirQualityProvider::fetch(air_quality_t &airQuality) {
#if defined(AIR_QUALITY_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(AIR_QUALITY_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // AIR_QUALITY_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_ISRG_Root_X1);
  const uint16_t port = 443;
#endif
  String uri = "/v1/air-quality?latitude=" + LAT + "&longitude=" + LON +
               "&hourly=pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ammonia,nitrogen_monoxide,ozone,pm10&"
               "past_days=1&forecast_days=1&timeformat=unixtime";
  String sanitizedUri = OM_AIR_QUALITY_ENDPOINT + uri;

  return httpGetWithRetry(client, OM_AIR_QUALITY_ENDPOINT, port, uri, sanitizedUri, true, HTTP_CLIENT_TCP_TIMEOUT,
                          [&airQuality](Stream &json, size_t) { return deserializeAirQuality(json, airQuality); });
}  // OpenMeteoAirQualityProvider::fetch

/* SAX event handler: maps the Open-Meteo air quality response directly into
 * the generic air quality model as the bytes stream in. Only the `hourly`
 * section is captured: the `time` array followed by one array per requested
 * pollutant. The response is requested with timeformat=unixtime, so every
 * value is a number (or null when a station has no reading); a null maps to
 * a zero concentration, matching the DOM implementation's `.as<float>()`.
 *
 * Window semantics are preserved: the LAST NUM_AIR_POLLUTION hourly entries
 * whose timestamp is at or before `now` (time(nullptr)) are kept, in order,
 * older entries shifted out. Timestamps ascend, so parsing the `time` array
 * yields closestIdx_ = the last index with ts <= now; qualifying entries are
 * packed straight into airQuality_.dt as a rolling window (memmove-shift on
 * overflow), and each pollutant array packs the values at indices <=
 * closestIdx_ into its model array the same way. Since the packed window is
 * the model array itself, the surviving entries always land in slots
 * 0..count-1, exactly like the DOM implementation's
 * [max(0, closestIdx_ - NUM_AIR_POLLUTION + 1), closestIdx_] window.
 *
 * This relies on the `time` array preceding the pollutant arrays inside
 * `hourly` (Open-Meteo always emits it first; pinned by the hourly= request
 * parameters in fetch() and the captured fixture). While no time entry has
 * qualified yet, pollutant values are dropped, so a payload without a
 * usable `hourly` (e.g. an Open-Meteo {"error": ...} response) parses to Ok
 * with an empty model.
 */
class AirQualityHandler {
 public:
  explicit AirQualityHandler(air_quality_t &airQuality) : airQuality_(airQuality), now_(time(nullptr)) {}

  bool Null() {
    storeScalar(0.0);
    return true;
  }
  bool Bool(bool) { return true; }
  bool Int(int v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Uint(unsigned v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Int64(int64_t v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Uint64(uint64_t v) {
    storeScalar(static_cast<double>(v));
    return true;
  }
  bool Double(double v) {
    storeScalar(v);
    return true;
  }
  // Not invoked without kParseNumbersAsStringsFlag; return true to be safe.
  bool RawNumber(const char *, rapidjson::SizeType, bool) { return true; }
  bool String(const char *, rapidjson::SizeType, bool) { return true; }

  bool StartObject() {
    ++depth_;
    return true;
  }

  bool Key(const char *str, rapidjson::SizeType length, bool) {
    col_ = Col::NONE;
    if (inArray_ || (depth_ != 1 && depth_ != 2)) {
      return true;
    }
    if (depth_ == 1) {
      // Top-level keys select the section to capture.
      section_ = keyEquals(str, length, "hourly") ? Section::HOURLY : Section::NONE;
      return true;
    }
    // Keys inside `hourly` select the value column.
    if (section_ == Section::HOURLY) {
      col_ = column(str, length);
    }
    return true;
  }

  bool EndObject(rapidjson::SizeType) {
    --depth_;
    return true;
  }

  bool StartArray() {
    ++depth_;
    if (!inArray_ && col_ != Col::NONE) {
      inArray_ = true;
      idx_ = 0;
    }
    return true;
  }

  bool EndArray(rapidjson::SizeType) {
    --depth_;
    inArray_ = false;
    return true;
  }

 private:
  enum class Section : uint8_t { NONE, HOURLY };

  enum class Col : uint8_t { NONE, TIME, PM2_5, PM10, CO, NO, NO2, O3, SO2, NH3 };

  static bool keyEquals(const char *str, rapidjson::SizeType len, const char *key) {
    const size_t keyLen = strlen(key);
    return len == keyLen && strncmp(str, key, keyLen) == 0;
  }

  static Col column(const char *str, rapidjson::SizeType len) {
    if (keyEquals(str, len, "time")) {
      return Col::TIME;
    }
    if (keyEquals(str, len, "pm2_5")) {
      return Col::PM2_5;
    }
    if (keyEquals(str, len, "pm10")) {
      return Col::PM10;
    }
    if (keyEquals(str, len, "carbon_monoxide")) {
      return Col::CO;
    }
    if (keyEquals(str, len, "nitrogen_monoxide")) {
      return Col::NO;
    }
    if (keyEquals(str, len, "nitrogen_dioxide")) {
      return Col::NO2;
    }
    if (keyEquals(str, len, "ozone")) {
      return Col::O3;
    }
    if (keyEquals(str, len, "sulphur_dioxide")) {
      return Col::SO2;
    }
    if (keyEquals(str, len, "ammonia")) {
      return Col::NH3;
    }
    return Col::NONE;
  }

  void storeTime(int64_t ts) {
    if (ts <= now_) {
      closestIdx_ = static_cast<int>(idx_);
      if (timeCount_ < NUM_AIR_POLLUTION) {
        airQuality_.dt[timeCount_++] = ts;
      } else {
        memmove(airQuality_.dt, airQuality_.dt + 1, (NUM_AIR_POLLUTION - 1) * sizeof(int64_t));
        airQuality_.dt[NUM_AIR_POLLUTION - 1] = ts;
      }
    }
  }

  void storeComponent(float *values, size_t &count, float value) {
    if (closestIdx_ < 0 || idx_ > static_cast<size_t>(closestIdx_)) {
      return;
    }
    if (count < NUM_AIR_POLLUTION) {
      values[count++] = value;
    } else {
      memmove(values, values + 1, (NUM_AIR_POLLUTION - 1) * sizeof(float));
      values[NUM_AIR_POLLUTION - 1] = value;
    }
  }

  void storeScalar(double value) {
    if (!inArray_ || col_ == Col::NONE) {
      return;
    }
    switch (col_) {
      case Col::TIME:
        storeTime(static_cast<int64_t>(value));
        break;
      case Col::PM2_5:
        storeComponent(airQuality_.components.pm2_5, componentCount_[0], static_cast<float>(value));
        break;
      case Col::PM10:
        storeComponent(airQuality_.components.pm10, componentCount_[1], static_cast<float>(value));
        break;
      case Col::CO:
        storeComponent(airQuality_.components.co, componentCount_[2], static_cast<float>(value));
        break;
      case Col::NO:
        storeComponent(airQuality_.components.no, componentCount_[3], static_cast<float>(value));
        break;
      case Col::NO2:
        storeComponent(airQuality_.components.no2, componentCount_[4], static_cast<float>(value));
        break;
      case Col::O3:
        storeComponent(airQuality_.components.o3, componentCount_[5], static_cast<float>(value));
        break;
      case Col::SO2:
        storeComponent(airQuality_.components.so2, componentCount_[6], static_cast<float>(value));
        break;
      case Col::NH3:
        storeComponent(airQuality_.components.nh3, componentCount_[7], static_cast<float>(value));
        break;
      default:
        return;
    }
    ++idx_;
  }

 public:
  air_quality_t &airQuality_;
  const int64_t now_;
  Section section_ = Section::NONE;
  Col col_ = Col::NONE;
  uint8_t depth_ = 0;
  bool inArray_ = false;
  size_t idx_ = 0;
  int closestIdx_ = -1;
  size_t timeCount_ = 0;
  size_t componentCount_[8] = {};
};

ProviderResult OpenMeteoAirQualityProvider::deserializeAirQuality(Stream &json, air_quality_t &airQuality) {
  // The model is long-lived in the caller and shared with the previous
  // fetch: zero it first, so values a response does not carry can never
  // survive (stale out-of-window entries, rejected payloads, ...). The
  // streamed values are packed directly into the (zeroed) model arrays.
  memset(&airQuality, 0, sizeof(air_quality_t));
  LOG_DEBUG("heap before streamed JSON parse: %u B free", ESP.getFreeHeap());
  StreamInput input(json);
  AirQualityHandler handler(airQuality);
  rapidjson::Reader reader;
  // kParseStopWhenDoneFlag stops right after the root JSON value, so any
  // trailing bytes in the HTTP body do not trip a RootNotSingular error.
  rapidjson::ParseResult result = reader.Parse<rapidjson::kParseStopWhenDoneFlag>(input, handler);
  LOG_DEBUG("heap after streamed JSON parse: %u B free", ESP.getFreeHeap());
  if (result.IsError()) {
    LOG_WARNING("Open-Meteo JSON parse error %u at offset %zu", static_cast<unsigned>(result.Code()), result.Offset());
    // rapidjson reports a truncated body as a syntax error; classify it as
    // IncompleteInput when the stream was exhausted at the failure point.
    if (result.Code() == rapidjson::kParseErrorDocumentEmpty) {
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_EMPTY_INPUT);
    }
    if (input.reachedEof()) {
      return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT);
    }
    return ProviderResult::error(TXT_DESERIALIZATION_ERROR_INVALID_INPUT);
  }
  return ProviderResult::ok();
}  // OpenMeteoAirQualityProvider::deserializeAirQuality

#endif  // AIR_QUALITY_API_PROVIDER_OPEN_METEO
