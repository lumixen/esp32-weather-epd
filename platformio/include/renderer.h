/* Renderer declarations for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
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

#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "defines.h"
#include <vector>
#include <Arduino.h>
#include <time.h>
#ifdef EPD_PANEL_DKE_3C_86BF
#include <GxEPD2_750c_86BF.h>
#endif
#include "api_response.h"
#include "config.h"
#include "moon_tools.h"

#if defined(EPD_PANEL_GENERIC_BW_V2)
  #define DISP_WIDTH  800
  #define DISP_HEIGHT 480
  #include <GxEPD2_BW.h>
  extern GxEPD2_BW<GxEPD2_750_T7,
                   GxEPD2_750_T7::HEIGHT> display;
#elif defined(EPD_PANEL_GENERIC_3C_B)
  #define DISP_WIDTH  800
  #define DISP_HEIGHT 480
  #include <GxEPD2_3C.h>
  extern GxEPD2_3C<GxEPD2_750c_Z08,
                   GxEPD2_750c_Z08::HEIGHT / 2> display;
#elif defined(EPD_PANEL_DKE_3C_86BF)
  #define DISP_WIDTH  800
  #define DISP_HEIGHT 480
  #include <GxEPD2_3C.h>
  extern GxEPD2_3C<GxEPD2_750c_86BF,
                   GxEPD2_750c_86BF::HEIGHT / 2> display;
#elif defined(EPD_PANEL_GENERIC_7C_F)
  #define DISP_WIDTH  800
  #define DISP_HEIGHT 480
  #include <GxEPD2_7C.h>
  extern GxEPD2_7C<GxEPD2_730c_GDEY073D46, 
                   GxEPD2_730c_GDEY073D46::HEIGHT / 4> display;
#elif defined(EPD_PANEL_GENERIC_BW_V1)
  #define DISP_WIDTH  640
  #define DISP_HEIGHT 384
  #include <GxEPD2_BW.h>
  extern GxEPD2_BW<GxEPD2_750,
                   GxEPD2_750::HEIGHT> display;
#else
  #error "No valid EPD_PANEL_* macro defined. Please define exactly one panel type."
#endif

typedef enum alignment
{
  LEFT,
  RIGHT,
  CENTER
} alignment_t;

uint16_t getStringWidth(const String &text);
uint16_t getStringHeight(const String &text);
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color=GxEPD_BLACK);
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color=GxEPD_BLACK);
void initDisplay();
void powerOffDisplay();
void drawCurrentConditions(const current_t &current,
                           const daily_t &today,
                           const air_pollution_t &air_pollution);
void drawForecast(const daily_t *daily, tm timeInfo);
void drawAlerts(std::vector<owm_alerts_t> &alerts,
                const String &city, const String &date);
void drawLocationDate(const String &city, const String &date);
void drawOutlookGraph(const hourly_t *hourly, const daily_t *daily,
                      tm timeInfo);
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage);
void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2="");

#endif
