#pragma once

#include <vector>
#include <ArduinoJson.h>
#include "alert_provider.h"

/* OpenWeatherMap "One Call" API alert provider.
 *
 * Used when the weather provider is not OpenWeatherMap, so alerts must be
 * fetched with their own request. When the weather provider is
 * OpenWeatherMap, alerts ride along in the weather response instead and
 * OWMWeatherProvider serves them from cache.
 */
class OWMAlertProvider : public AlertProvider {
 public:
  int fetch(std::vector<weather_alert_t> &alerts) override;

 private:
  static DeserializationError deserializeAlerts(Stream &json, std::vector<weather_alert_t> &alerts);
};
