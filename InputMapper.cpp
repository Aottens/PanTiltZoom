#include "InputMapper.h"

#include <cmath>

namespace {
constexpr float kAxisNormalizer = 512.0f;
constexpr float kTriggerNormalizer = 1023.0f;
constexpr float kPanDeadzone = 0.08f;
constexpr float kPanExpo = 0.3f;
constexpr float kTiltDeadzone = 0.08f;
constexpr float kTiltExpo = 0.3f;
constexpr uint32_t kPresetSaveHoldMs = 2000;
}

InputMapper::InputMapper() = default;

InputMapper::InputState InputMapper::map(const GamepadSnapshot& snapshot,
                                         uint32_t nowMs) {
  InputState state{};
  state.timestampMs = nowMs;

  if (!snapshot.connected) {
    _previousButtons = 0;
    _savePressActive = false;
    _lastBPressMs = 0;
    return state;
  }

  float panNormalized = applyDeadzoneAndExpo(snapshot.axisLX, kPanDeadzone, kPanExpo);
  float tiltNormalized = applyDeadzoneAndExpo(snapshot.axisRY, kTiltDeadzone, kTiltExpo);

  state.panVelocity = panNormalized;
  state.tiltVelocity = -tiltNormalized;

  float zoomIn = static_cast<float>(snapshot.triggerR2) / kTriggerNormalizer;
  float zoomOut = static_cast<float>(snapshot.triggerL2) / kTriggerNormalizer;
  state.zoomVelocity = zoomIn - zoomOut;

  bool buttonA = (snapshot.buttons & BUTTON_A) != 0;
  bool buttonB = (snapshot.buttons & BUTTON_B) != 0;
  bool startPressed = (snapshot.buttons & BUTTON_START) != 0;

  if (buttonA) {
    if (!_savePressActive) {
      _savePressActive = true;
      _savePressStartMs = nowMs;
    }
  } else if (_savePressActive) {
    if (nowMs - _savePressStartMs >= kPresetSaveHoldMs) {
      state.presetSave = true;
    }
    _savePressActive = false;
  }

  bool previousB = (_previousButtons & BUTTON_B) != 0;
  if (!previousB && buttonB) {
    _lastBPressMs = nowMs;
  }
  if (previousB && !buttonB) {
    if ((nowMs - _lastBPressMs) <= 1500) {
      state.presetRecall = true;
    }
  }

  bool previousStart = (_previousButtons & BUTTON_START) != 0;
  if (!previousStart && startPressed) {
    state.resetEncoders = true;
  }

  state.diagnosticsDump = snapshot.l3 && snapshot.r3 &&
                          !((_previousButtons & BUTTON_THUMBL) &&
                            (_previousButtons & BUTTON_THUMBR));

  _previousButtons = snapshot.buttons;

  return state;
}

GamepadSnapshot InputMapper::createSnapshotFromGamepad(GamepadPtr gamepad) {
  GamepadSnapshot snapshot{};
  if (!gamepad) {
    return snapshot;
  }

  snapshot.connected = gamepad->isConnected();
  snapshot.axisLX = gamepad->axisX();
  snapshot.axisLY = gamepad->axisY();
  snapshot.axisRX = gamepad->axisRX();
  snapshot.axisRY = gamepad->axisRY();
  snapshot.triggerL2 = gamepad->brake();
  snapshot.triggerR2 = gamepad->throttle();
  snapshot.buttons = gamepad->buttons();
  snapshot.l3 = (snapshot.buttons & BUTTON_THUMBL) != 0;
  snapshot.r3 = (snapshot.buttons & BUTTON_THUMBR) != 0;
  snapshot.start = (snapshot.buttons & BUTTON_START) != 0;
  snapshot.batteryLevel = gamepad->battery();

  return snapshot;
}

float InputMapper::applyDeadzoneAndExpo(int16_t value, float deadzone,
                                        float expo) const {
  float normalized = static_cast<float>(value) / kAxisNormalizer;
  normalized = constrain(normalized, -1.0f, 1.0f);

  if (fabsf(normalized) < deadzone) {
    return 0.0f;
  }

  float sign = normalized >= 0.0f ? 1.0f : -1.0f;
  float magnitude = (fabsf(normalized) - deadzone) / (1.0f - deadzone);
  magnitude = constrain(magnitude, 0.0f, 1.0f);

  float expoValue = magnitude * (1.0f - expo) + powf(magnitude, 3.0f) * expo;
  return sign * expoValue;
}

