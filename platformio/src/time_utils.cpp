#include "time_utils.h"

static SemaphoreHandle_t ntpSyncSemaphore = nullptr;

static RTC_DATA_ATTR uint32_t cyclesSinceLastNtpSync = 0;

static void timeSyncNotificationCallback(struct timeval *tv) {
  // Can't use tv for RTC calibration as it always shows the time sync since the EPOCH
  // after the deep sleep.
  if (ntpSyncSemaphore) {
    xSemaphoreGive(ntpSyncSemaphore);
  }
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
    Serial.print("[TIME] Time before synchronization: ");
    Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S,");
    unsigned long syncStart = millis();

    ntpSyncSemaphore = xSemaphoreCreateBinary();
    sntp_set_time_sync_notification_cb(timeSyncNotificationCallback);
    Serial.println("[TIME] Synchronizing time...");
    configTzTime(D_TIMEZONE, D_NTP_SERVER_1, D_NTP_SERVER_2);
    if (xSemaphoreTake(ntpSyncSemaphore, pdMS_TO_TICKS(NTP_TIMEOUT)) == pdTRUE) {
      unsigned long syncEnd = millis();
      getLocalTime(timeInfo);

      timeConfigured = true;

      Serial.printf("[TIME] Sync duration: %lu ms\n", syncEnd - syncStart);
    } else {
      Serial.println(TXT_FAILED_TO_GET_TIME);
    }
    vSemaphoreDelete(ntpSyncSemaphore);
    ntpSyncSemaphore = nullptr;
    if (timeConfigured) {
      cyclesSinceLastNtpSync = 0;  // Reset counter after successful sync
    }
  } else {
    Serial.println("Using internal RTC time. (Wake #" + String(cyclesSinceLastNtpSync) + "/" +
                   String(cyclesPerInterval) + ")");
    timeConfigured = true;
  }

  cyclesSinceLastNtpSync++;
  if (!timeConfigured) {
    // Sync was attempted but failed; trigger a retry on the next wakeup.
    cyclesSinceLastNtpSync = cyclesPerInterval;
  }
  if (timeConfigured) {
    Serial.print("[TIME] ");
    Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S");
  }
  return timeConfigured;
}
