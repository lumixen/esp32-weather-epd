#include "time_utils.h"

static SemaphoreHandle_t ntpSyncSemaphore = nullptr;

static RTC_DATA_ATTR uint32_t cyclesSinceLastNtpSync = 0;
static RTC_DATA_ATTR double rtcDriftCorrectionCoefficient = 1.0;
static RTC_DATA_ATTR uint64_t lastTimeBeforeDeepSleepUs = 0;

static void timeSyncNotificationCallback(struct timeval *tv) {
  // Can't use tv for RTC calibration as it always shows the time sync since the EPOCH
  // after the deep sleep.
  if (ntpSyncSemaphore) {
    xSemaphoreGive(ntpSyncSemaphore);
  }
}

// Adjust the RTC calibration register based on the calculated drift correction coefficient.
static void adjustRTCReg(double rtcDriftCorrectionCoefficient) {
  //   switch (rtc_clk_slow_freq_get()) {
  //     case RTC_SLOW_FREQ_RTC:
  //       Serial.println("[TIME] Current slow clock source: Internal 150 kHz RC oscillator");
  //       break;
  //     case RTC_SLOW_FREQ_32K_XTAL:
  //       Serial.println("[TIME] Current slow clock source: External 32 kHz XTAL");
  //       break;
  //     case RTC_SLOW_FREQ_8MD256:
  //       Serial.println("[TIME] Current slow clock source: Internal 8 MHz RC oscillator, divided by 256");
  //       break;
  //     default:
  //       Serial.println("[TIME] Unknown slow clock source");
  //       break;
  //   }
  //   uint32_t rtcSlowPeriod = rtc_clk_cal(RTC_CAL_RTC_MUX, 1024);
  uint32_t rtcSlowPeriod = esp_clk_slowclk_cal_get();
  Serial.printf("[TIME] Current slow clock period: %u (Q13.19 format)\n", rtcSlowPeriod);
  rtcSlowPeriod *= rtcDriftCorrectionCoefficient;
  // RTC_CNTL_STORE1_REG is used to determine the rtc timer scale
  //   REG_WRITE(RTC_CNTL_STORE1_REG, rtcSlowPeriod);
  esp_clk_slowclk_cal_set(rtcSlowPeriod);
  Serial.printf("[TIME] Adjusted slow clock period: %u (Q13.19 format)\n", rtcSlowPeriod);
}

bool configureTime(tm *timeInfo) {
  // TIME SYNCHRONIZATION
  // Sync periodically based on configured interval (NTP_SYNC_INTERVAL_WAKEUPS) and wake-up counter.
  // If RTC time is not valid (e.g., after reset or power loss), force an immediate sync.
  setenv("TZ", D_TIMEZONE, 1);
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
    // Serial.println("[TIME] Time before synchronization: " + String(&timeInfo, "%A, %B %d, %Y %H:%M:%S"));
    struct timeval beforeSync, afterSync;
    gettimeofday(&beforeSync, NULL);
    Serial.print("[TIME] Time before synchronization: ");
    Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S,");
    unsigned long syncStart = millis();

    ntpSyncSemaphore = xSemaphoreCreateBinary();
    sntp_set_time_sync_notification_cb(timeSyncNotificationCallback);
    Serial.println("[TIME] Synchronizing time...");
    configTzTime(D_TIMEZONE, D_NTP_SERVER_1, D_NTP_SERVER_2);
    if (xSemaphoreTake(ntpSyncSemaphore, pdMS_TO_TICKS(NTP_TIMEOUT)) == pdTRUE) {
      unsigned long syncEnd = millis();
      gettimeofday(&afterSync, NULL);
      getLocalTime(timeInfo);

      timeConfigured = true;

      // Self-calibrate RTC drift using the time before and after synchronization, and the known sync duration.
      uint64_t before = beforeSync.tv_sec * 1000000LL + beforeSync.tv_usec;
      uint64_t after = afterSync.tv_sec * 1000000LL + afterSync.tv_usec;
      uint64_t syncDuration = (syncEnd - syncStart) * 1000LL;
      int64_t drift = (after - before) - syncDuration;

      // Calculate drift coefficient using the interval since last sleep.
      // We calculate Ratio = (Real Time Elapsed) / (RTC Time Elapsed)
      if (!driftIsHuge && lastTimeBeforeDeepSleepUs > 0) {
        int64_t rtcElapsed = before - lastTimeBeforeDeepSleepUs;
        int64_t realElapsed = (after - syncDuration) - lastTimeBeforeDeepSleepUs;

        if (rtcElapsed > 1000000LL) {  // Ensure at least 1s elapsed to avoid noise
          rtcDriftCorrectionCoefficient = (double) realElapsed / (double) rtcElapsed;
          adjustRTCReg(rtcDriftCorrectionCoefficient);
        }
      }

      Serial.printf("[TIME] Sync duration: %lu ms\n", syncEnd - syncStart);
      Serial.printf("[TIME] Corrected time drift: %lld us\n", drift);
      Serial.printf("[TIME] RTC drift correction coefficient: %.6f\n", rtcDriftCorrectionCoefficient);
    } else {
      Serial.println(TXT_FAILED_TO_GET_TIME);
    }
    if (timeConfigured) {
      cyclesSinceLastNtpSync = 0;  // Reset counter after successful sync
    }
  } else {
    Serial.println("Using internal RTC time. (Wake #" + String(cyclesSinceLastNtpSync) + "/" +
                   String(cyclesPerInterval) + ")");
    timeConfigured = true;
  }

  cyclesSinceLastNtpSync++;
  return timeConfigured;
}

void logTimeBeforeSleep() {
  struct timeval now;
  gettimeofday(&now, NULL);
  lastTimeBeforeDeepSleepUs = (uint64_t) now.tv_sec * 1000000ULL + (uint64_t) now.tv_usec;
}