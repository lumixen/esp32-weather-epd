/* Renderer declarations for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 * Copyright (C) 2026  Max Bodaniuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "config.h"
#include <vector>
#include <Arduino.h>
#include <time.h>
#include "weather_report.h"
#ifdef EPD_PANEL_DKE_3C_86BF
#include <GxEPD2_750c_86BF.h>
#endif
#include "moon_tools.h"

#ifdef EPD_PANEL_GENERIC_BW_V2
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#define BUSY_LEVEL LOW
#include <GxEPD2_BW.h>
extern GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display;
#endif
#ifdef EPD_PANEL_GENERIC_3C_B
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#define BUSY_LEVEL LOW
#include <GxEPD2_3C.h>
extern GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2> display;
#endif
#ifdef EPD_PANEL_DKE_3C_86BF
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#define BUSY_LEVEL LOW
#include <GxEPD2_3C.h>
extern GxEPD2_3C<GxEPD2_750c_86BF, GxEPD2_750c_86BF::HEIGHT / 2> display;
#endif
#ifdef EPD_PANEL_GENERIC_7C_F
#define DISP_WIDTH 800
#define DISP_HEIGHT 480
#define BUSY_LEVEL LOW
#include <GxEPD2_7C.h>
extern GxEPD2_7C<GxEPD2_730c_GDEY073D46, GxEPD2_730c_GDEY073D46::HEIGHT / 4> display;
#endif
#ifdef EPD_PANEL_GENERIC_BW_V1
#define DISP_WIDTH 640
#define DISP_HEIGHT 384
#define BUSY_LEVEL LOW
#include <GxEPD2_BW.h>
extern GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT> display;
#endif

typedef enum alignment { LEFT, RIGHT, CENTER } alignment_t;

uint16_t getStringWidth(const String &text);
uint16_t getStringHeight(const String &text);
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment, uint16_t color = GxEPD_BLACK);
void drawMultiLnString(int16_t x, int16_t y, const String &text, alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing, uint16_t color = GxEPD_BLACK);
void beginLightSleep(const void *);
void initDisplay();
void powerOffDisplay();
void drawCurrentConditions(const weather_report_t &report);
void drawForecast(const weather_report_t &report, tm timeInfo);
void drawAlerts(weather_report_t &report, const String &city, const String &date);
void drawLocationDate(const String &city, const String &date);
void drawOutlookGraph(const weather_report_t &report, tm timeInfo);
void drawStatusBar(const String &statusStr, const String &refreshTimeStr, int rssi, uint32_t batVoltage);
void drawError(const uint8_t *bitmap_196x196, const String &errMsgLn1, const String &errMsgLn2 = "");
void drawCurrentAirQuality(const air_quality_t &air_quality);
void drawCurrentHumidity(const current_t &current);
void drawCurrentMoonphase(const moon_state_t &moon);
void drawCurrentMoonrise(const moon_state_t &moon);
void drawCurrentMoonset(const moon_state_t &moon);
void drawCurrentPressure(const current_t &current);
void drawCurrentSunrise(const sun_state_t &sun);
void drawCurrentSunset(const sun_state_t &sun);
void drawCurrentUVI(const current_t &current);
void drawCurrentVisibility(const current_t &current);
void drawCurrentWind(const current_t &current);
void drawCurrentDewpoint(const current_t &current);
void drawCurrentInPressure(std::optional<float> inPressure);
