#include "config.h"

#if defined(WEATHER_API_PROVIDER_OPEN_METEO)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#if !defined(WEATHER_API_TRANSPORT_HTTP)
#include <WiFiClientSecure.h>
#endif
#if defined(WEATHER_API_TRANSPORT_HTTPS_VERIFY)
#include "cert.h"
#endif
#include "client_utils.h"
#include "open_meteo_weather_provider.h"

const char *OpenMeteoWeatherProvider::getApiName() const {
  return "Open Meteo API";
}  // OpenMeteoWeatherProvider::getApiName

/* Perform an HTTP GET request to Open-Meteo's forecast API and map the
 * response into the generic forecast model.
 *
 * Returns the HTTP Status Code.
 */
int OpenMeteoWeatherProvider::fetch(forecast_t &forecast) {
#if defined(WEATHER_API_TRANSPORT_HTTP)
  WiFiClient client;
  const uint16_t port = 80;
#elif defined(WEATHER_API_TRANSPORT_HTTPS_NO_VERIFY)
  WiFiClientSecure client;
  client.setInsecure();
  const uint16_t port = 443;
#else  // WEATHER_API_TRANSPORT_HTTPS_VERIFY
  WiFiClientSecure client;
  client.setCACert(cert_ISRG_Root_X1);
  const uint16_t port = 443;
#endif
  String uri =
      "/v1/forecast?latitude=" + LAT + "&longitude=" + LON + "&" +
      "current=temperature_2m,relative_humidity_2m,dew_point_2m,apparent_temperature,weather_code,cloud_cover,"
      "visibility,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day&" +
      "hourly=temperature_2m,cloud_cover,wind_speed_10m,wind_gusts_10m,precipitation_probability,rain,snowfall,weather_"
      "code,is_day&" +
      "daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,rain_sum,snowfall_sum,"
      "precipitation_probability_max,wind_speed_10m_max,wind_gusts_10m_max&" +
      "wind_speed_unit=ms&timezone=auto&timeformat=unixtime&forecast_days=5&forecast_hours=" + HOURLY_GRAPH_MAX;

  // This string is printed to terminal to help with debugging.
  String sanitizedUri = OM_ENDPOINT + uri;

  return httpGetWithRetry(client, OM_ENDPOINT, port, uri, sanitizedUri, true, HTTP_CLIENT_TCP_TIMEOUT,
                          [&forecast](Stream &json, size_t) { return deserializeCall(json, forecast); });
}  // OpenMeteoWeatherProvider::fetch

DeserializationError OpenMeteoWeatherProvider::deserializeCall(Stream &json, forecast_t &forecast) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : " + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  JsonObject current = doc["current"];
  JsonObject daily = doc["daily"];
  JsonObject hourly = doc["hourly"];

  forecast.current.dt = current["time"].as<int64_t>();
  forecast.current.sunrise = daily["sunrise"][0].as<int64_t>();  //
  forecast.current.sunset = daily["sunset"][0].as<int64_t>();    //
  forecast.current.temp = current["temperature_2m"].as<float>();
  forecast.current.feels_like = current["apparent_temperature"].as<float>();
  forecast.current.pressure = current["surface_pressure"].as<int>();  //
  forecast.current.humidity = current["relative_humidity_2m"].as<int>();
  forecast.current.dew_point = current["dew_point_2m"].as<float>();
  forecast.current.clouds = current["cloud_cover"].as<int>();
  forecast.current.uvi = daily["uv_index_max"][0].as<float>();    //
  forecast.current.visibility = current["visibility"].as<int>();  //
  forecast.current.wind_speed = current["wind_speed_10m"].as<float>();
  forecast.current.wind_gust = current["wind_gusts_10m"].as<float>();
  forecast.current.wind_deg = current["wind_direction_10m"].as<int>();  // w
  forecast.current.weather.id = current["weather_code"].as<int>();
  forecast.current.is_day = current["is_day"].as<bool>();
  forecast.current.soil_temperature_18cm = hourly["soil_temperature_18cm"][0].as<float>();

  int hours = doc["hourly"]["time"].size();
  for (size_t i = 0; i < hours; i++) {
    forecast.hourly[i].dt = hourly["time"][i].as<int64_t>();  // dt means
    forecast.hourly[i].temp = hourly["temperature_2m"][i].as<float>();
    forecast.hourly[i].clouds = hourly["cloud_cover"][i].as<int>();
    forecast.hourly[i].wind_speed = hourly["wind_speed_10m"][i].as<float>();
    forecast.hourly[i].wind_gust = hourly["wind_gusts_10m"][i].as<float>();
    forecast.hourly[i].pop = hourly["precipitation_probability"][i].as<int>();
    forecast.hourly[i].rain_1h = hourly["rain"][i].as<float>();
    forecast.hourly[i].snow_1h = hourly["snowfall"][i].as<float>();
    forecast.hourly[i].weather.id = hourly["weather_code"][i].as<int>();
    forecast.hourly[i].is_day = hourly["is_day"][i].as<bool>();

    if (i == NUM_HOURLY - 1) {
      break;
    }
  }

  int days = doc["daily"]["time"].size();
  for (size_t i = 0; i < days; i++) {
    forecast.daily[i].dt = daily["time"][i].as<int64_t>();
    forecast.daily[i].temp.min = daily["temperature_2m_min"][i].as<float>();
    forecast.daily[i].temp.max = daily["temperature_2m_max"][i].as<float>();
    // Cloud cover percentage is not provided by Open-Meteo as daily
    // forecast.daily[i].clouds = daily["cloud_cover"][i].as<int>();
    forecast.daily[i].wind_speed = daily["wind_speed_10m_max"][i].as<float>();
    forecast.daily[i].wind_gust = daily["wind_gusts_10m_max"][i].as<float>();
    forecast.daily[i].pop = daily["precipitation_probability_max"][i].as<int>();
    forecast.daily[i].rain = daily["rain_sum"][i].as<float>();
    forecast.daily[i].snow = daily["snowfall_sum"][i].as<float>();
    forecast.daily[i].weather.id = daily["weather_code"][i].as<int>();
    forecast.daily[i].shortwave_radiation_sum = daily["shortwave_radiation_sum"][i].as<float>();  //

    if (i == NUM_DAILY - 1) {
      break;
    }
  }
  return error;
}  // OpenMeteoWeatherProvider::deserializeCall

#endif  // WEATHER_API_PROVIDER_OPEN_METEO
