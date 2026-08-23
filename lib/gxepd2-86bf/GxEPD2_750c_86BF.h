/* GxEPD2 86BF panel driver adaptation declarations for esp32-weather-epd.
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

#include "GxEPD2_EPD.h"

class GxEPD2_750c_86BF : public GxEPD2_EPD {
 public:
  // attributes
  static const uint16_t WIDTH = 800;
  static const uint16_t WIDTH_VISIBLE = WIDTH;
  static const uint16_t HEIGHT = 480;
  static const GxEPD2::Panel panel = GxEPD2::GDEW075Z08;
  static const bool hasColor = true;
  static const bool hasPartialUpdate = false;
  static const bool usePartialUpdate = false;  // set false to get better image (flashes full screen)
  static const bool hasFastPartialUpdate = false;
  static const uint16_t power_on_time = 150;           // ms, e.g. 133421us
  static const uint16_t power_off_time = 30;           // ms, e.g. 25362us
  static const uint16_t full_refresh_time = 30000;     // ms, e.g. 17133490us
  static const uint16_t partial_refresh_time = 30000;  // ms, e.g. 17133490us
  // constructor
  GxEPD2_750c_86BF(int16_t cs, int16_t dc, int16_t rst, int16_t busy);
  // methods (virtual)
  //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
  void clearScreen(uint8_t value = 0xFF);                      // init controller memory and screen (default white)
  void clearScreen(uint8_t black_value, uint8_t color_value);  // init controller memory and screen
  void writeScreenBuffer(uint8_t value = 0xFF);                // init controller memory (default white)
  void writeScreenBuffer(uint8_t black_value, uint8_t color_value);  // init controller memory
  // write to controller memory, without screen refresh; x and w should be multiple of 8
  void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                  bool mirror_y = false, bool pgm = false);
  void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                      int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false,
                      bool pgm = false);
  void writeImage(const uint8_t *black, const uint8_t *color, int16_t x, int16_t y, int16_t w, int16_t h,
                  bool invert = false, bool mirror_y = false, bool pgm = false);
  void writeImagePart(const uint8_t *black, const uint8_t *color, int16_t x_part, int16_t y_part, int16_t w_bitmap,
                      int16_t h_bitmap, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                      bool mirror_y = false, bool pgm = false);
  // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
  void writeNative(const uint8_t *data1, const uint8_t *data2, int16_t x, int16_t y, int16_t w, int16_t h,
                   bool invert = false, bool mirror_y = false, bool pgm = false);
  // write to controller memory, with screen refresh; x and w should be multiple of 8
  void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                 bool mirror_y = false, bool pgm = false);
  void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false,
                     bool pgm = false);
  void drawImage(const uint8_t *black, const uint8_t *color, int16_t x, int16_t y, int16_t w, int16_t h,
                 bool invert = false, bool mirror_y = false, bool pgm = false);
  void drawImagePart(const uint8_t *black, const uint8_t *color, int16_t x_part, int16_t y_part, int16_t w_bitmap,
                     int16_t h_bitmap, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                     bool mirror_y = false, bool pgm = false);
  // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
  void drawNative(const uint8_t *data1, const uint8_t *data2, int16_t x, int16_t y, int16_t w, int16_t h,
                  bool invert = false, bool mirror_y = false, bool pgm = false);
  void refresh(bool partial_update_mode = false);            // screen refresh from controller memory to full screen
  void refresh(int16_t x, int16_t y, int16_t w, int16_t h);  // screen refresh from controller memory, partial screen
  void powerOff();   // turns off generation of panel driving voltages, avoids screen fading over time
  void hibernate();  // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by
                     // RST (rst >= 0)
 private:
  void _writeScreenBuffer(uint8_t value);
  void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void _PowerOn();
  void _PowerOff();
  void _InitDisplay();
  void _Init_Full();
  void _Init_Part();
  void _Update_Full();
  void _Update_Part();
};
