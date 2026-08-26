/* Leveled logging helpers for esp32-weather-epd.
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

/* Leveled logging helpers for esp32-weather-epd.
 */
#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "config.h"

enum class LogLevel : uint8_t {
  TRACE = 0,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  CRITICAL,
};

/* Runtime log level filter. Initialized from the generated LOG_LEVEL config
 * constant; can be changed at runtime with setLogLevel(). */
inline LogLevel g_logLevel = static_cast<LogLevel>(LOG_LEVEL);

inline void setLogLevel(LogLevel level) { g_logLevel = level; }

/* __FILE__ is a full build path; keep only the basename for log output. */
#define LOG_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* printf-style sink, guarded by the runtime level. Arguments are only
 * evaluated when the message is actually emitted. Format the prefix and body
 * before sending the complete line in one UART write; the ESP32 UART driver
 * serializes individual writes from concurrent tasks. */
inline void log_output(LogLevel level, const char *file, int line, const char *fmt, ...) {
  static const char *const LEVEL_NAMES[] = {"trc", "dbg", "inf", "wrn", "err", "crt"};
  if (level < g_logLevel) {
    return;
  }

  char body[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(body, sizeof(body), fmt, args);
  va_end(args);

  char output[384];
  snprintf(output, sizeof(output), "[%s] [%s:%d] %s\n", LEVEL_NAMES[static_cast<uint8_t>(level)], file, line, body);
  Serial.write(reinterpret_cast<const uint8_t *>(output), strlen(output));
  if (level >= LogLevel::CRITICAL) {
    Serial.flush();
  }
}

#define LOG_TRACE(...) \
  do { \
    if (LogLevel::TRACE >= g_logLevel) { \
      log_output(LogLevel::TRACE, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define LOG_DEBUG(...) \
  do { \
    if (LogLevel::DEBUG >= g_logLevel) { \
      log_output(LogLevel::DEBUG, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define LOG_INFO(...) \
  do { \
    if (LogLevel::INFO >= g_logLevel) { \
      log_output(LogLevel::INFO, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define LOG_WARNING(...) \
  do { \
    if (LogLevel::WARNING >= g_logLevel) { \
      log_output(LogLevel::WARNING, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define LOG_ERROR(...) \
  do { \
    if (LogLevel::ERROR >= g_logLevel) { \
      log_output(LogLevel::ERROR, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define LOG_CRITICAL(...) \
  do { \
    if (LogLevel::CRITICAL >= g_logLevel) { \
      log_output(LogLevel::CRITICAL, LOG_FILENAME, __LINE__, __VA_ARGS__); \
    } \
  } while (0)
