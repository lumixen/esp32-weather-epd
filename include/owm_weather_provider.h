#pragma once

#include <vector>
#include <ArduinoJson.h>
#include "alert_provider.h"
#include "weather_provider.h"

/* OpenWeatherMap "One Call" API weather provider.
 *
 * Also implements AlertProvider: OpenWeatherMap serves national weather
 * alerts in the same OneCall response, so they are extracted during the
 * weather fetch and served from the stored response without an additional
 * HTTP request.
 */
class OWMWeatherProvider : public WeatherProvider, public AlertProvider {
 public:
  const char *getApiName() const override;
  int fetch(forecast_t &forecast) override;
  int fetch(std::vector<weather_alert_t> &alerts) override;

 private:
  static DeserializationError deserializeOneCall(Stream &json, forecast_t &forecast,
                                                 std::vector<weather_alert_t> *alerts);
  std::vector<weather_alert_t> alerts_;
  bool haveAlerts_ = false;
  int fetchStatus_ = -1;
};
