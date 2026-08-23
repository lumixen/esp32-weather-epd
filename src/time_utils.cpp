/* Time and RTC utilities for esp32-weather-epd.
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "time_utils.h"
#include "logger.h"

#include <esp_rtc_time.h>
#include <esp_sleep.h>

#include <cmath>

static SemaphoreHandle_t ntpSyncSemaphore = nullptr;

static RTC_DATA_ATTR uint32_t cyclesSinceLastNtpSync = 0;

/*
 * RTC slow-clock drift auto-correction state (RTC memory, survives deep
 * sleep; cleared only by a power-on reset, which forces re-learning).
 */
static RTC_DATA_ATTR struct {
  bool valid;              // baseline established
  double k;                // learned correction factor (deep sleep / awake period ratio)
  uint64_t lastSyncRtcUs;  // esp_rtc_get_time_us() at the last NTP sync
  int64_t lastSyncEpoch;   // Unix time (s) at the last NTP sync
} rtcDrift = {};

// Deep-sleep duration (us) requested last time it slept; used to correct the
// wall clock after the timer wakeup.
static RTC_DATA_ATTR uint64_t rtcDriftLastSleepUs = 0;

static void timeSyncNotificationCallback(struct timeval *tv) {
  // Can't use tv for RTC calibration as it always shows the time sync since the EPOCH
  // after the deep sleep.
  if (ntpSyncSemaphore) {
    xSemaphoreGive(ntpSyncSemaphore);
  }
}

void rtcDriftOnNtpSync(uint64_t rtcUsNow, time_t epochNow) {
  if (!rtcDriftEnabled()) {
    return;
  }

  if (!rtcDrift.valid) {
    // First sync (or after a power-on reset): establish the baseline without
    // touching the factor.
    rtcDrift.valid = true;
    rtcDrift.k = 1.0;
    rtcDrift.lastSyncRtcUs = rtcUsNow;
    rtcDrift.lastSyncEpoch = epochNow;
    LOG_INFO("%s", "RTC drift correction: baseline established");
    return;
  }

  const long long elapsedRealUs = ((long long) epochNow - rtcDrift.lastSyncEpoch) * 1000000LL;
  const long long elapsedRtcUs = (long long) (rtcUsNow - rtcDrift.lastSyncRtcUs);
  rtcDrift.lastSyncRtcUs = rtcUsNow;
  rtcDrift.lastSyncEpoch = epochNow;

  if (elapsedRealUs < (long long) rtc_drift::RTC_DRIFT_MIN_LEARN_INTERVAL_US || elapsedRtcUs <= 0) {
    LOG_DEBUG("RTC drift correction: interval too short (%lld s), skipping sample", elapsedRealUs / 1000000LL);
    return;
  }

  const double rateError = ((double) elapsedRtcUs - (double) elapsedRealUs) / (double) elapsedRealUs;
  if (std::fabs(rateError) > rtc_drift::RTC_DRIFT_MAX_RATE_ERROR) {
    // Outlier (NTP hiccup, timezone change, ...): re-baseline, do not learn.
    LOG_WARNING("RTC drift correction: rejecting outlier drift of %+.0f ppm", rateError * 1e6);
    return;
  }

  rtcDrift.k = rtc_drift::updateFactor(rtcDrift.k, rateError);
  LOG_INFO("RTC drift: %+.0f ppm over %lld min, correction factor %.6f", rateError * 1e6, elapsedRealUs / 60000000LL,
           rtcDrift.k);
}  // end rtcDriftOnNtpSync

void rtcDriftApplyWakeupCorrection() {
  if (!rtcDriftEnabled() || rtcDriftLastSleepUs == 0) {
    return;
  }
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    rtcDriftLastSleepUs = 0;
    return;
  }

  const double k = rtcDrift.valid ? rtcDrift.k : 1.0;
  const long long shiftUs = rtc_drift::wakeShiftUs(rtcDriftLastSleepUs, k);
  rtcDriftLastSleepUs = 0;
  if (shiftUs == 0) {
    return;
  }

  // The RTC claimed rtcDriftLastSleepUs elapsed, but the real elapsed time
  // was claimed * k; shift the wall clock accordingly.
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  long long us = (long long) tv.tv_sec * 1000000LL + tv.tv_usec + shiftUs;
  tv.tv_sec = us / 1000000LL;
  tv.tv_usec = us % 1000000LL;
  settimeofday(&tv, nullptr);
  LOG_INFO("RTC drift correction: adjusted clock by %+lld ms (factor %.6f)", shiftUs / 1000LL, k);
}  // end rtcDriftApplyWakeupCorrection

uint64_t rtcDriftScaleSleepUs(uint64_t us) {
  if (!rtcDriftEnabled()) {
    return us;
  }
  const double k = rtcDrift.valid ? rtcDrift.k : 1.0;
  const uint64_t scaled = rtc_drift::scaleSleepUs(us, k);
  rtcDriftLastSleepUs = scaled;
  return scaled;
}  // end rtcDriftScaleSleepUs

bool configureTime(tm *timeInfo) {
  // TIME SYNCHRONIZATION
  // Sync periodically based on configured interval (NTP_SYNC_INTERVAL_WAKEUPS) and wake-up counter.
  // If RTC time is not valid (e.g., after reset or power loss), force an immediate sync.
  setenv("TZ", TIMEZONE, 1);
  tzset();

  bool timeConfigured = false;
  getLocalTime(timeInfo);  // Updates timeInfo with current RTC time

  unsigned int cyclesPerInterval = NTP_SYNC_INTERVAL_WAKEUPS;
  if (cyclesPerInterval < 1) {
    cyclesPerInterval = 1;
  }
  bool driftIsHuge = (timeInfo->tm_year < (2020 - 1900));  // RTC lost power or uninitialized
  bool timerTriggered = cyclesSinceLastNtpSync >= cyclesPerInterval;

  if (driftIsHuge || timerTriggered) {
    char timeBeforeSync[64];
    strftime(timeBeforeSync, sizeof(timeBeforeSync), "%A, %B %d, %Y %H:%M:%S", timeInfo);
    LOG_DEBUG("Time before synchronization: %s", timeBeforeSync);
    unsigned long syncStart = millis();

    ntpSyncSemaphore = xSemaphoreCreateBinary();
    sntp_set_time_sync_notification_cb(timeSyncNotificationCallback);
    LOG_INFO("Synchronizing time...");
    configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
    if (xSemaphoreTake(ntpSyncSemaphore, pdMS_TO_TICKS(NTP_TIMEOUT)) == pdTRUE) {
      unsigned long syncEnd = millis();
      getLocalTime(timeInfo);

      timeConfigured = true;

      if (rtcDriftEnabled()) {
        // Sample the RTC time alongside the authoritative NTP time, both
        // after the sync completed, so changes in sync latency do not show
        // up as measured drift.
        rtcDriftOnNtpSync(esp_rtc_get_time_us(), time(nullptr));
      }

      LOG_INFO("Sync duration: %lu ms", syncEnd - syncStart);
    } else {
      LOG_WARNING("%s", TXT_FAILED_TO_GET_TIME);
    }
    vSemaphoreDelete(ntpSyncSemaphore);
    ntpSyncSemaphore = nullptr;
    if (timeConfigured) {
      cyclesSinceLastNtpSync = 0;  // Reset counter after successful sync
    }
  } else {
    LOG_INFO("Using internal RTC time. (Wake #%u/%u)", cyclesSinceLastNtpSync, cyclesPerInterval);
    timeConfigured = true;
  }

  cyclesSinceLastNtpSync++;
  if (!timeConfigured) {
    // Sync was attempted but failed; trigger a retry on the next wakeup.
    cyclesSinceLastNtpSync = cyclesPerInterval;
  }
  if (timeConfigured) {
    char timeAfterSync[64];
    strftime(timeAfterSync, sizeof(timeAfterSync), "%A, %B %d, %Y %H:%M:%S", timeInfo);
    LOG_INFO("%s", timeAfterSync);
  }
  return timeConfigured;
}  // end configureTime