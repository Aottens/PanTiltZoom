#include "MotionController.h"

#include <algorithm>
#include <cmath>

#include "Logger.h"

namespace {
constexpr uint32_t kPanMaxSpeedHz = 6000;
constexpr uint32_t kTiltMaxSpeedHz = 6000;
constexpr uint32_t kZoomMaxSpeedHz = 4000;
constexpr uint32_t kPanAcceleration = 10000;
constexpr uint32_t kTiltAcceleration = 10000;
constexpr uint32_t kZoomAcceleration = 5000;
constexpr float kVelocityThreshold = 0.01f;
}

MotionController::MotionController() = default;

bool MotionController::begin(FastAccelStepperEngine* engine,
                             const StepperPinBundle& pins) {
  _engine = engine;
  _pins = pins;

  if (!_engine) {
    return false;
  }

  _engine->init();

  _panStepper = _engine->stepperConnectToPin(_pins.pan.step);
  _tiltStepper = _engine->stepperConnectToPin(_pins.tilt.step);
  _zoomStepper = _engine->stepperConnectToPin(_pins.zoom.step);

  if (!_panStepper || !_tiltStepper || !_zoomStepper) {
    Logger::error("Failed to attach one or more steppers");
    return false;
  }

  configureStepper(_panStepper, _pins.pan);
  configureStepper(_tiltStepper, _pins.tilt);
  configureStepper(_zoomStepper, _pins.zoom);

  Logger::info("MotionController initialized steppers");
  return true;
}

void MotionController::configureStepper(FastAccelStepper* stepper,
                                        const StepperPins& pins) {
  if (!stepper) {
    return;
  }
  stepper->setDirectionPin(pins.dir);
  stepper->setEnablePin(pins.enable);
  stepper->setAutoEnable(true);
  stepper->setCurrentPosition(0);
}

void MotionController::update(const InputMapper::InputState& state) {
  applyVelocity(_panStepper, state.panVelocity, kPanMaxSpeedHz,
                kPanAcceleration, "pan");
  applyVelocity(_tiltStepper, state.tiltVelocity, kTiltMaxSpeedHz,
                kTiltAcceleration, "tilt");
  applyVelocity(_zoomStepper, state.zoomVelocity, kZoomMaxSpeedHz,
                kZoomAcceleration, "zoom");

  if (_panStepper) {
    _positions.panSteps = _panStepper->getCurrentPosition();
  }
  if (_tiltStepper) {
    _positions.tiltSteps = _tiltStepper->getCurrentPosition();
  }
  if (_zoomStepper) {
    _positions.zoomSteps = _zoomStepper->getCurrentPosition();
  }
}

void MotionController::applyVelocity(FastAccelStepper* stepper, float velocity,
                                     uint32_t maxSpeed, uint32_t accel,
                                     const char* name) {
  if (!stepper) {
    return;
  }

  (void)name;

  float magnitude = fabsf(velocity);
  if (magnitude < kVelocityThreshold) {
    if (stepper->isRunning()) {
      stepper->stopMove();
    }
    return;
  }

  uint32_t speed = static_cast<uint32_t>(maxSpeed * magnitude);
  speed = std::max<uint32_t>(speed, 200);
  speed = std::min<uint32_t>(speed, maxSpeed);
  stepper->setAcceleration(accel);
  stepper->setSpeedInHz(speed);
  if (velocity > 0) {
    stepper->runForward();
  } else {
    stepper->runBackward();
  }
}

void MotionController::stopAll() {
  if (_panStepper) {
    _panStepper->forceStop();
    _panStepper->disableOutputs();
  }
  if (_tiltStepper) {
    _tiltStepper->forceStop();
    _tiltStepper->disableOutputs();
  }
  if (_zoomStepper) {
    _zoomStepper->forceStop();
    _zoomStepper->disableOutputs();
  }
}

void MotionController::resetEncoders() {
  if (_panStepper) {
    _panStepper->setCurrentPosition(0);
  }
  if (_tiltStepper) {
    _tiltStepper->setCurrentPosition(0);
  }
  if (_zoomStepper) {
    _zoomStepper->setCurrentPosition(0);
  }
  _positions = {};
  Logger::info("Encoder positions reset");
}

void MotionController::refreshEncoderData() {
  // TODO: Integrate AS5600 encoder readings via I2C multiplexer
}

void MotionController::dumpDiagnostics() const {
  Logger::info("Stepper positions pan=%ld tilt=%ld zoom=%ld", static_cast<long>(_positions.panSteps),
               static_cast<long>(_positions.tiltSteps),
               static_cast<long>(_positions.zoomSteps));
}

MotionController::AxisPositions MotionController::getCurrentPositions() const {
  return _positions;
}

void MotionController::moveToPreset(const AxisPositions& target) {
  if (_panStepper) {
    _panStepper->moveTo(target.panSteps);
  }
  if (_tiltStepper) {
    _tiltStepper->moveTo(target.tiltSteps);
  }
  if (_zoomStepper) {
    _zoomStepper->moveTo(target.zoomSteps);
  }
  Logger::info("Recalling preset: pan=%ld tilt=%ld zoom=%ld", static_cast<long>(target.panSteps),
               static_cast<long>(target.tiltSteps), static_cast<long>(target.zoomSteps));
}

