#pragma once

#include <Arduino.h>
#include <Bluepad32.h>

struct GamepadSnapshot {
  bool connected = false;
  int16_t axisLX = 0;
  int16_t axisLY = 0;
  int16_t axisRX = 0;
  int16_t axisRY = 0;
  uint16_t triggerL2 = 0;
  uint16_t triggerR2 = 0;
  uint32_t buttons = 0;
  bool l3 = false;
  bool r3 = false;
  bool start = false;
  uint8_t batteryLevel = 0;
};

class InputMapper {
 public:
  struct InputState {
    float panVelocity = 0.0f;
    float tiltVelocity = 0.0f;
    float zoomVelocity = 0.0f;
    bool presetSave = false;
    bool presetRecall = false;
    bool resetEncoders = false;
    bool diagnosticsDump = false;
    uint32_t timestampMs = 0;
  };

  InputMapper();

  InputState map(const GamepadSnapshot& snapshot, uint32_t nowMs);

  static GamepadSnapshot createSnapshotFromGamepad(GamepadPtr gamepad);

 private:
  float applyDeadzoneAndExpo(int16_t value, float deadzone, float expo) const;

  uint32_t _previousButtons = 0;
  uint32_t _savePressStartMs = 0;
  bool _savePressActive = false;
  uint32_t _lastBPressMs = 0;
};

