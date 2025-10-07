#pragma once

#include <Arduino.h>

class SafetyManager {
 public:
  void begin();
  void onControllerConnected(uint32_t nowMs);
  void onControllerDisconnected(uint32_t nowMs);
  void onInputActivity(uint32_t nowMs);
  bool shouldForceIdle(uint32_t nowMs) const;
  void updateHeartbeat(uint32_t nowMs);
  void dumpDiagnostics() const;
  uint32_t lastInputTimestamp() const { return _lastInputMs; }

 private:
  static constexpr uint32_t kInputTimeoutMs = 300;
  static constexpr uint32_t kRampDownWindowMs = 150;
  static constexpr uint32_t kHeartbeatIntervalMs = 2000;

  uint32_t _lastInputMs = 0;
  uint32_t _lastHeartbeatMs = 0;
  bool _connected = false;
  bool _forceIdle = false;
};

