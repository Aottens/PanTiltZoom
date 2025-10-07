#include "SafetyManager.h"

#include "Logger.h"

void SafetyManager::begin() {
  _lastInputMs = millis();
  _lastHeartbeatMs = millis();
  _connected = false;
  _forceIdle = true;
}

void SafetyManager::onControllerConnected(uint32_t nowMs) {
  _connected = true;
  _forceIdle = false;
  _lastInputMs = nowMs;
  _lastHeartbeatMs = nowMs;
}

void SafetyManager::onControllerDisconnected(uint32_t nowMs) {
  _connected = false;
  _forceIdle = true;
  _lastInputMs = nowMs;
}

void SafetyManager::onInputActivity(uint32_t nowMs) {
  _lastInputMs = nowMs;
  _forceIdle = false;
}

bool SafetyManager::shouldForceIdle(uint32_t nowMs) const {
  if (_forceIdle) {
    return true;
  }
  if (!_connected) {
    return true;
  }
  if ((nowMs - _lastInputMs) > (kInputTimeoutMs + kRampDownWindowMs)) {
    return true;
  }
  return false;
}

void SafetyManager::updateHeartbeat(uint32_t nowMs) {
  if (!_connected) {
    return;
  }
  if ((nowMs - _lastInputMs) > (kInputTimeoutMs + kRampDownWindowMs)) {
    _forceIdle = true;
  }
  if (nowMs - _lastInputMs > kHeartbeatIntervalMs) {
    Logger::warn("Safety timeout triggered - forcing idle");
    _forceIdle = true;
  }
  if (nowMs - _lastHeartbeatMs >= kHeartbeatIntervalMs) {
    Logger::debug("Heartbeat ok");
    _lastHeartbeatMs = nowMs;
  }
}

void SafetyManager::dumpDiagnostics() const {
  Logger::info("Safety: connected=%s forceIdle=%s lastInput=%lu", _connected ? "yes" : "no",
               _forceIdle ? "yes" : "no", static_cast<unsigned long>(_lastInputMs));
}

