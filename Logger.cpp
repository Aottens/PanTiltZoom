#include "Logger.h"

#include <stdarg.h>

Stream* Logger::s_stream = nullptr;

void Logger::begin(Stream* stream) {
  s_stream = stream;
}

void Logger::log(Level level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  vlog(level, format, args);
  va_end(args);
}

void Logger::debug(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vlog(Level::DEBUG, format, args);
  va_end(args);
}

void Logger::info(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vlog(Level::INFO, format, args);
  va_end(args);
}

void Logger::warn(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vlog(Level::WARN, format, args);
  va_end(args);
}

void Logger::error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vlog(Level::ERROR, format, args);
  va_end(args);
}

void Logger::vlog(Level level, const char* format, va_list args) {
  if (!s_stream) {
    return;
  }

  char buffer[192];
  vsnprintf(buffer, sizeof(buffer), format, args);
  s_stream->print('[');
  s_stream->print(levelToLabel(level));
  s_stream->print(F("] "));
  s_stream->println(buffer);
}

const __FlashStringHelper* Logger::levelToLabel(Level level) {
  switch (level) {
    case Level::DEBUG:
      return F("DBG");
    case Level::INFO:
      return F("INF");
    case Level::WARN:
      return F("WRN");
    case Level::ERROR:
    default:
      return F("ERR");
  }
}

