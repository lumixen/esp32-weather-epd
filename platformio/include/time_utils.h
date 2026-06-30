#pragma once

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>
#include <soc/rtc.h>
#include "esp_private/esp_clk.h"
#include "driver/rtc_io.h"
#include "defines.h"
#include "_locale.h"

bool configureTime(tm *timeInfo);
void logTimeBeforeSleep();