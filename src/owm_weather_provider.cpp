#include "config.h"
#include "logger.h"

#if defined(WEATHER_API_PROVIDER_OPEN_WEATHER_MAP)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#if !defined(WEATHER_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(WEATHER_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include "_locale.h"
#include "client_utils.h"
#include "provider_result_utils.h"
#include "owm_weather_provider.h"

// OpenWeatherMaps does not specify a limit, but if you need more alerts you
// are probably doomed.
#define OWM_NUM_ALERTS 8

const char *OWMWeatherProvider::getApiName() const {
  return "One Call API";
}  // OWMWeatherProvider::getApiName

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API and map the
 * response into the generic forecast model.
 */
ProviderResult OWMWeatherProvider::fetch(forecast_t &forecast) {
#if defined(WEATHER_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(WEATHER_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // WEATHER_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_USERTrust_RSA_Certification_Authority);
  const uint16_t port = 443;
#endif
  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT + "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=metric&exclude=minutely";
#if !defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  // exclude alerts
  uri += ",alerts";
#endif

  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";

  uri += "&appid=" + OWM_APIKEY;

  std::vector<weather_alert_t> *alerts = nullptr;
#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  alerts = &alerts_;
#endif

  ProviderResult result = httpGetWithRetry(
      client, OWM_ENDPOINT, port, uri, sanitizedUri, false, HTTP_CLIENT_TCP_TIMEOUT,
      [this, &forecast, alerts](Stream &json, size_t) { return deserializeOneCall(json, forecast, alerts); });

  fetchStatus_ = result;
  haveAlerts_ = result.isOk();
  return result;
}  // OWMWeatherProvider::fetch

/* Serve national weather alerts extracted from the last One Call response.
 * No additional HTTP request is made.
 */
ProviderResult OWMWeatherProvider::fetch(std::vector<weather_alert_t> &alerts) {
  if (haveAlerts_) {
    alerts = alerts_;
    return ProviderResult::ok();
  }
  return fetchStatus_;
}  // OWMWeatherProvider::fetch

/* Map an OpenWeatherMap weather condition id onto the unified weather
 * condition enum.
 *
 * References:
 *   https://openweathermap.org/weather-conditions
 */
weather_condition OWMWeatherProvider::mapWeatherCode(int id) {
  switch (id) {
    // Group 2xx: Thunderstorm
    case 200:  // Thunderstorm with light rain
    case 201:  // Thunderstorm with rain
    case 202:  // Thunderstorm with heavy rain
    case 210:  // Light thunderstorm
    case 211:  // Thunderstorm
    case 212:  // Heavy thunderstorm
    case 221:  // Ragged thunderstorm
      return weather_condition::THUNDERSTORM;
    case 230:  // Thunderstorm with light drizzle
    case 231:  // Thunderstorm with drizzle
    case 232:  // Thunderstorm with heavy drizzle
      return weather_condition::THUNDERSTORM_HAIL;
    // Group 3xx: Drizzle
    case 300:  // Light intensity drizzle
    case 301:  // Drizzle
    case 302:  // Heavy intensity drizzle
    case 310:  // Light intensity drizzle rain
    case 311:  // Drizzle rain
    case 312:  // Heavy intensity drizzle rain
    case 313:  // Shower rain and drizzle
    case 314:  // Heavy shower rain and drizzle
    case 321:  // Shower drizzle
      return weather_condition::DRIZZLE;
    // Group 5xx: Rain
    case 500:  // Light rain
    case 501:  // Moderate rain
    case 502:  // Heavy intensity rain
    case 503:  // Very heavy rain
    case 504:  // Extreme rain
      return weather_condition::RAIN;
    case 511:  // Freezing rain
      return weather_condition::FREEZING_RAIN;
    case 520:  // Light intensity shower rain
    case 521:  // Shower rain
    case 522:  // Heavy intensity shower rain
    case 531:  // Ragged shower rain
      return weather_condition::RAIN_SHOWERS;
    // Group 6xx: Snow
    case 600:  // Light snow
    case 601:  // Snow
    case 602:  // Heavy snow
      return weather_condition::SNOW;
    case 611:  // Sleet
    case 612:  // Light shower sleet
    case 613:  // Shower sleet
      return weather_condition::SLEET;
    case 615:  // Light rain and snow
    case 616:  // Rain and snow
    case 620:  // Light shower snow
    case 621:  // Shower snow
    case 622:  // Heavy shower snow
      return weather_condition::RAIN_SNOW_MIX;
    // Group 7xx: Atmosphere
    case 701:  // Mist
      return weather_condition::MIST;
    case 711:  // Smoke
      return weather_condition::SMOKE;
    case 721:  // Haze
      return weather_condition::HAZE;
    case 731:  // Sand/dust whirls
      return weather_condition::SAND_WHIRLS;
    case 741:  // Fog
      return weather_condition::FOG;
    case 751:  // Sand
      return weather_condition::SAND;
    case 761:  // Dust
      return weather_condition::DUST;
    case 762:  // Volcanic ash
      return weather_condition::ASH;
    case 771:  // Squalls
      return weather_condition::SQUALL;
    case 781:  // Tornado
      return weather_condition::TORNADO;
    // Group 800: Clear
    case 800:  // Clear sky
      return weather_condition::CLEAR;
    // Group 80x: Clouds
    case 801:  // Few clouds: 11-25%
      return weather_condition::PARTLY_CLOUDY;
    case 802:  // Scattered clouds: 25-50%
    case 803:  // Broken clouds: 51-84%
      return weather_condition::CLOUDY;
    case 804:  // Overcast clouds: 85-100%
      return weather_condition::OVERCAST;
    default:
      // maybe this is a new condition id in one of the existing groups
      if (id >= 200 && id < 300) {
        return weather_condition::THUNDERSTORM;
      }
      if (id >= 300 && id < 400) {
        return weather_condition::DRIZZLE;
      }
      if (id >= 500 && id < 600) {
        return weather_condition::RAIN;
      }
      if (id >= 600 && id < 700) {
        return weather_condition::SNOW;
      }
      if (id >= 700 && id < 800) {
        return weather_condition::FOG;
      }
      if (id >= 800 && id < 900) {
        return weather_condition::CLOUDY;
      }
      return weather_condition::UNKNOWN;
  }
}  // OWMWeatherProvider::mapWeatherCode

ProviderResult OWMWeatherProvider::deserializeOneCall(Stream &json, forecast_t &forecast,
                                                      std::vector<weather_alert_t> *alerts) {
  int i;

  JsonDocument filter;
  filter["current"] = true;
  filter["minutely"] = false;
  filter["hourly"] = true;
  filter["daily"] = true;
#if !defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  filter["alerts"] = false;
#else
  // description can be very long so they are filtered out to save on memory
  // along with sender_name
  for (int i = 0; i < OWM_NUM_ALERTS; ++i) {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"] = true;
    filter["alerts"][i]["start"] = true;
    filter["alerts"][i]["end"] = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"] = true;
  }
#endif

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  LOG_DEBUG("doc.overflowed() : %s", doc.overflowed() ? "true" : "false");
  if (LogLevel::TRACE >= g_logLevel) {
    LOG_TRACE("pretty JSON dump:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  if (error) {
    return mapDeserializationError(error);
  }

  forecast.lat = doc["lat"].as<float>();
  forecast.lon = doc["lon"].as<float>();
  forecast.timezone = doc["timezone"].as<const char *>();
  forecast.timezone_offset = doc["timezone_offset"].as<int>();

  JsonObject current = doc["current"];
  forecast.current.dt = current["dt"].as<int64_t>();
  forecast.current.sunrise = current["sunrise"].as<int64_t>();
  forecast.current.sunset = current["sunset"].as<int64_t>();
  forecast.current.temp = current["temp"].as<float>();
  forecast.current.feels_like = current["feels_like"].as<float>();
  forecast.current.pressure = current["pressure"].as<int>();
  forecast.current.humidity = current["humidity"].as<int>();
  forecast.current.dew_point = current["dew_point"].as<float>();
  forecast.current.clouds = current["clouds"].as<int>();
  forecast.current.uvi = current["uvi"].as<float>();
  forecast.current.visibility = current["visibility"].as<int>();
  forecast.current.wind_speed = current["wind_speed"].as<float>();
  forecast.current.wind_gust = current["wind_gust"].as<float>();
  forecast.current.wind_deg = current["wind_deg"].as<int>();
  forecast.current.rain_1h = current["rain"]["1h"].as<float>();
  forecast.current.snow_1h = current["snow"]["1h"].as<float>();
  JsonObject current_weather = current["weather"][0];
  forecast.current.weather.condition = mapWeatherCode(current_weather["id"].as<int>());
  // OpenWeatherMap indicates sun is up with d otherwise n for night
  forecast.current.is_day = current_weather["icon"].as<String>().endsWith("d");

  i = 0;
  for (JsonObject hourly : doc["hourly"].as<JsonArray>()) {
    forecast.hourly[i].dt = hourly["dt"].as<int64_t>();
    forecast.hourly[i].temp = hourly["temp"].as<float>();
    forecast.hourly[i].feels_like = hourly["feels_like"].as<float>();
    forecast.hourly[i].pressure = hourly["pressure"].as<int>();
    forecast.hourly[i].humidity = hourly["humidity"].as<int>();
    forecast.hourly[i].dew_point = hourly["dew_point"].as<float>();
    forecast.hourly[i].clouds = hourly["clouds"].as<int>();
    forecast.hourly[i].uvi = hourly["uvi"].as<float>();
    forecast.hourly[i].visibility = hourly["visibility"].as<int>();
    forecast.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    forecast.hourly[i].wind_gust = hourly["wind_gust"].as<float>();
    forecast.hourly[i].wind_deg = hourly["wind_deg"].as<int>();
    forecast.hourly[i].pop = hourly["pop"].as<float>() * 100;
    forecast.hourly[i].rain_1h = hourly["rain"]["1h"].as<float>();
    forecast.hourly[i].snow_1h = hourly["snow"]["1h"].as<float>();
    JsonObject hourly_weather = hourly["weather"][0];
    forecast.hourly[i].weather.condition = mapWeatherCode(hourly_weather["id"].as<int>());
    // OpenWeatherMap indicates sun is up with d otherwise n for night
    forecast.hourly[i].is_day = hourly_weather["icon"].as<String>().endsWith("d");

    if (i == NUM_HOURLY - 1) {
      break;
    }
    ++i;
  }

  i = 0;
  for (JsonObject daily : doc["daily"].as<JsonArray>()) {
    forecast.daily[i].dt = daily["dt"].as<int64_t>();
    forecast.daily[i].sunrise = daily["sunrise"].as<int64_t>();
    forecast.daily[i].sunset = daily["sunset"].as<int64_t>();
    JsonObject daily_temp = daily["temp"];
    forecast.daily[i].temp.morn = daily_temp["morn"].as<float>();
    forecast.daily[i].temp.day = daily_temp["day"].as<float>();
    forecast.daily[i].temp.eve = daily_temp["eve"].as<float>();
    forecast.daily[i].temp.night = daily_temp["night"].as<float>();
    forecast.daily[i].temp.min = daily_temp["min"].as<float>();
    forecast.daily[i].temp.max = daily_temp["max"].as<float>();
    forecast.daily[i].pressure = daily["pressure"].as<int>();
    forecast.daily[i].humidity = daily["humidity"].as<int>();
    forecast.daily[i].dew_point = daily["dew_point"].as<float>();
    forecast.daily[i].clouds = daily["clouds"].as<int>();
    forecast.daily[i].uvi = daily["uvi"].as<float>();
    forecast.daily[i].visibility = daily["visibility"].as<int>();
    forecast.daily[i].wind_speed = daily["wind_speed"].as<float>();
    forecast.daily[i].wind_gust = daily["wind_gust"].as<float>();
    forecast.daily[i].wind_deg = daily["wind_deg"].as<int>();
    forecast.daily[i].pop = daily["pop"].as<float>() * 100;
    forecast.daily[i].rain = daily["rain"].as<float>();
    forecast.daily[i].snow = daily["snow"].as<float>();
    JsonObject daily_weather = daily["weather"][0];
    forecast.daily[i].weather.condition = mapWeatherCode(daily_weather["id"].as<int>());

    if (i == NUM_DAILY - 1) {
      break;
    }
    ++i;
  }

#if defined(ALERTS_API_PROVIDER_OPEN_WEATHER_MAP)
  if (alerts != nullptr) {
    i = 0;
    for (JsonObject alert : doc["alerts"].as<JsonArray>()) {
      weather_alert_t new_alert = {};
      // new_alert.sender_name = alert["sender_name"].as<const char *>();
      new_alert.event = alert["event"].as<const char *>();
      new_alert.start = alert["start"].as<int64_t>();
      new_alert.end = alert["end"].as<int64_t>();
      // new_alert.description = alert["description"].as<const char *>();
      new_alert.tags = alert["tags"][0].as<const char *>();
      alerts->push_back(new_alert);

      if (i == OWM_NUM_ALERTS - 1) {
        break;
      }
      ++i;
    }
  }
#endif

  return mapDeserializationError(error);
}  // OWMWeatherProvider::deserializeOneCall

#endif  // WEATHER_API_PROVIDER_OPEN_WEATHER_MAP
