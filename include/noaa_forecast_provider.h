/* NOAA/National Weather Service forecast provider for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <memory>
#include <vector>
#include <Arduino.h>
#include "provider_result.h"
#include "remote_data_provider.h"

struct NoaaStationCandidate {
  String stationId;
  float distance = 0.0f;
};

/* Forecast provider backed by the api.weather.gov points, forecast, and
 * observation APIs. NOAA responses are parsed incrementally; this class does
 * not own a response buffer. */
class NoaaForecastProvider : public RemoteDataProvider {
 public:
  const char *getApiName() const override;
  std::vector<std::unique_ptr<FetchOperation>> createFetchOperations(weather_report_t &out) override;

  static weather_condition mapDescription(const String &description);
  static int64_t parseIso8601(const String &value);

  /* Validate an absolute URL returned by NWS and copy it to normalized. The
   * endpoint is deliberately restricted to api.weather.gov so a compromised
   * or malformed points response cannot redirect the device elsewhere. */
  static bool normalizeApiUrl(const String &url, String &normalized);
  static String normalizeApiUrl(const String &url);
  static bool isValidApiUrl(const String &url) {
    String ignored;
    return normalizeApiUrl(url, ignored);
  }

  /* Public parser seams used by fixture tests. Each parser resets its
   * destination before parsing and rejects empty, malformed, truncated, or
   * structurally incomplete JSON. */
  static ProviderResult deserializeHourly(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeDaily(Stream &json, forecast_t &forecast);
  static ProviderResult deserializeObservation(Stream &json, current_t &current);
  static ProviderResult deserializeObservationStations(Stream &json, std::vector<NoaaStationCandidate> &stations);
  static ProviderResult deserializePoints(Stream &json, String &forecastUrl, String &hourlyUrl,
                                          String &observationStationsUrl, String &timeZone);

  /* Convenience instance seam: parses points and stores its resolved URLs in
   * provider-owned state. */
  ProviderResult deserializePoints(Stream &json);

 private:
  ProviderResult fetchPoints(forecast_t &forecast);
  ProviderResult fetchDaily(forecast_t &forecast);
  ProviderResult fetchHourly(forecast_t &forecast);
  ProviderResult fetchCurrent(current_t &current);

  String forecastUrl_;
  String hourlyUrl_;
  String observationStationsUrl_;
  String timeZone_;
  std::vector<NoaaStationCandidate> stations_;
};
