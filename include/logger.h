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
 * evaluated when the message is actually emitted. */
inline void log_output(LogLevel level, const char *file, int line, const char *fmt, ...) {
  static const char *const LEVEL_NAMES[] = {"trc", "dbg", "inf", "wrn", "err", "crt"};
  if (level < g_logLevel) {
    return;
  }
  Serial.printf("[%s] [%s:%d] ", LEVEL_NAMES[static_cast<uint8_t>(level)], file, line);
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
  if (level >= LogLevel::CRITICAL) {
    Serial.flush();
  }
}

#define LOG_TRACE(...)                                                                             \
  do {                                                                                             \
    if (LogLevel::TRACE >= g_logLevel) {                                                           \
      log_output(LogLevel::TRACE, LOG_FILENAME, __LINE__, __VA_ARGS__);                            \
    }                                                                                              \
  } while (0)

#define LOG_DEBUG(...)                                                                             \
  do {                                                                                             \
    if (LogLevel::DEBUG >= g_logLevel) {                                                           \
      log_output(LogLevel::DEBUG, LOG_FILENAME, __LINE__, __VA_ARGS__);                            \
    }                                                                                              \
  } while (0)

#define LOG_INFO(...)                                                                              \
  do {                                                                                             \
    if (LogLevel::INFO >= g_logLevel) {                                                            \
      log_output(LogLevel::INFO, LOG_FILENAME, __LINE__, __VA_ARGS__);                             \
    }                                                                                              \
  } while (0)

#define LOG_WARNING(...)                                                                           \
  do {                                                                                             \
    if (LogLevel::WARNING >= g_logLevel) {                                                         \
      log_output(LogLevel::WARNING, LOG_FILENAME, __LINE__, __VA_ARGS__);                          \
    }                                                                                              \
  } while (0)

#define LOG_ERROR(...)                                                                             \
  do {                                                                                             \
    if (LogLevel::ERROR >= g_logLevel) {                                                           \
      log_output(LogLevel::ERROR, LOG_FILENAME, __LINE__, __VA_ARGS__);                            \
    }                                                                                              \
  } while (0)

#define LOG_CRITICAL(...)                                                                          \
  do {                                                                                             \
    if (LogLevel::CRITICAL >= g_logLevel) {                                                        \
      log_output(LogLevel::CRITICAL, LOG_FILENAME, __LINE__, __VA_ARGS__);                         \
    }                                                                                              \
  } while (0)
