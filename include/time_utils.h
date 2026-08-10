#pragma once

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>
#include "config.h"
#include "_locale.h"

bool configureTime(tm *timeInfo);