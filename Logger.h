#pragma once

#include <Arduino.h>
#include <stdarg.h>

class Logger {
 public:
  enum class Level { DEBUG, INFO, WARN, ERROR };

  static void begin(Stream* stream);

  static void log(Level level, const char* format, ...);

  static void debug(const char* format, ...);
  static void info(const char* format, ...);
  static void warn(const char* format, ...);
  static void error(const char* format, ...);

 private:
  static void vlog(Level level, const char* format, va_list args);
  static const __FlashStringHelper* levelToLabel(Level level);

  static Stream* s_stream;
};

