#pragma once

#include <vector>
#include "alert_provider.h"
#include "provider_result.h"
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
  ProviderResult fetch(forecast_t &forecast) override;
  ProviderResult fetch(std::vector<weather_alert_t> &alerts) override;

 private:
  static ProviderResult deserializeOneCall(Stream &json, forecast_t &forecast,
                                           std::vector<weather_alert_t> *alerts);
  std::vector<weather_alert_t> alerts_;
  bool haveAlerts_ = false;
  ProviderResult fetchStatus_;
};
