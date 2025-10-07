#pragma once

#include <Arduino.h>
#include <FastAccelStepper.h>

#include "InputMapper.h"

class MotionController {
 public:
  struct StepperPins {
    uint8_t step;
    uint8_t dir;
    uint8_t enable;
  };

  struct StepperPinBundle {
    StepperPins pan;
    StepperPins tilt;
    StepperPins zoom;
  };

  struct AxisPositions {
    int32_t panSteps = 0;
    int32_t tiltSteps = 0;
    int32_t zoomSteps = 0;
  };

  MotionController();

  bool begin(FastAccelStepperEngine* engine, const StepperPinBundle& pins);

  void update(const InputMapper::InputState& state);

  void stopAll();

  void resetEncoders();

  void refreshEncoderData();

  void dumpDiagnostics() const;

  AxisPositions getCurrentPositions() const;

  void moveToPreset(const AxisPositions& target);

 private:
  void configureStepper(FastAccelStepper* stepper, const StepperPins& pins);
  void applyVelocity(FastAccelStepper* stepper, float velocity,
                     uint32_t maxSpeed, uint32_t accel, const char* name);

  FastAccelStepperEngine* _engine = nullptr;
  StepperPinBundle _pins{};
  FastAccelStepper* _panStepper = nullptr;
  FastAccelStepper* _tiltStepper = nullptr;
  FastAccelStepper* _zoomStepper = nullptr;
  AxisPositions _positions{};
};

