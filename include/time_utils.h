#pragma once

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>
#include <cstdint>
#include "config.h"
#include "_locale.h"

bool configureTime(tm *timeInfo);

/*
 * RTC slow-clock drift auto-correction.
 *
 * The ESP32 deep-sleep timer and the internal wall clock are driven by the
 * RTC slow clock (internal RC oscillator), whose rate deviates from the
 * value calibrated right before deep sleep (temperature/voltage effects
 * during sleep). A correction factor k (period in deep sleep / period
 * measured while awake) is learned from the deviation observed between two
 * consecutive NTP synchronizations and applied to the sleep duration and
 * to the wall clock after a timer wakeup, so the device wakes up at the
 * correct real time between NTP syncs.
 */
namespace rtc_drift {

// Drift-learning constants.
constexpr double RTC_DRIFT_MIN_FACTOR = 0.90;       // clamp of the learned factor
constexpr double RTC_DRIFT_MAX_FACTOR = 1.10;
constexpr double RTC_DRIFT_LEARN_ALPHA = 0.25;      // EMA smoothing of new samples
constexpr double RTC_DRIFT_MAX_RATE_ERROR = 0.10;   // reject outlier samples (> 10%)
constexpr uint64_t RTC_DRIFT_MIN_LEARN_INTERVAL_US = 45ULL * 60ULL * 1000000ULL;
constexpr double RTC_DRIFT_MAX_SHIFT_RATIO = 0.10;  // clamp of the post-wake shift

// Correction factor implied by a measured relative drift rate (positive when
// the RTC ran fast): k = 1 / (1 + rateError).
inline double sampleFactor(double rateError) { return 1.0 / (1.0 + rateError); }

inline double clampFactor(double k) {
  if (k < RTC_DRIFT_MIN_FACTOR) {
    return RTC_DRIFT_MIN_FACTOR;
  }
  if (k > RTC_DRIFT_MAX_FACTOR) {
    return RTC_DRIFT_MAX_FACTOR;
  }
  return k;
}

// Exponential moving average of the learned correction factor with a new
// sample derived from the observed relative drift rate.
inline double updateFactor(double k, double rateError) {
  return clampFactor(k + RTC_DRIFT_LEARN_ALPHA * (sampleFactor(rateError) - k));
}

// Deep-sleep duration (us) that yields the desired real-time duration for a
// given correction factor. The timer is programmed with the period measured
// while awake, so the real elapsed time is duration * k and the requested
// duration must be scaled by 1/k.
inline uint64_t scaleSleepUs(uint64_t us, double k) {
  if (k <= 0.0 || k == 1.0 || us == 0) {
    return us;
  }
  double scaled = (double)us / k + 0.5;
  if (scaled < 0.0) {
    return 0;
  }
  return (uint64_t)scaled;
}

// Clock adjustment (us) to apply after waking from a deep sleep that lasted
// `claimedUs` according to the RTC: the real elapsed time was claimed * k.
// The shift is clamped to RTC_DRIFT_MAX_SHIFT_RATIO of the claimed duration
// as a safety bound against a corrupt factor.
inline int64_t wakeShiftUs(uint64_t claimedUs, double k) {
  if (k <= 0.0 || k == 1.0 || claimedUs == 0) {
    return 0;
  }
  double shift = (double)claimedUs * (k - 1.0);
  const double bound = (double)claimedUs * RTC_DRIFT_MAX_SHIFT_RATIO;
  if (shift > bound) {
    shift = bound;
  } else if (shift < -bound) {
    shift = -bound;
  }
  return (int64_t)shift;
}

}  // namespace rtc_drift

// True when the slow-clock drift auto-correction is enabled by the config.
inline bool rtcDriftEnabled() {
#if RTC_DRIFT_CORRECTION
  return true;
#else
  return false;
#endif
}

// Learn the correction factor from a successful NTP synchronization:
// `rtcUsNow` (esp_rtc_get_time_us) and `epochNow` (authoritative Unix time)
// must be sampled together, after the sync completed, so that the elapsed
// real time and the elapsed RTC time span the same interval.
void rtcDriftOnNtpSync(uint64_t rtcUsNow, time_t epochNow);

// Apply the wall-clock correction after waking from a deep sleep (timer
// wakeup only). Must be called before the system time is read for display.
void rtcDriftApplyWakeupCorrection();

// Scale a requested deep-sleep duration (us) by the learned correction
// factor. Call with the value passed to esp_sleep_enable_timer_wakeup.
uint64_t rtcDriftScaleSleepUs(uint64_t us);