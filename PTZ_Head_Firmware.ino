#include <Arduino.h>
#include <Bluepad32.h>
#include <FastAccelStepper.h>
#include "InputMapper.h"
#include "MotionController.h"
#include "PresetManager.h"
#include "SafetyManager.h"
#include "Logger.h"

// Stepper pin definitions (TODO: Move to configuration file)
constexpr uint8_t PAN_STEP_PIN = 2;
constexpr uint8_t PAN_DIR_PIN = 3;
constexpr uint8_t PAN_ENABLE_PIN = 4;
constexpr uint8_t TILT_STEP_PIN = 5;
constexpr uint8_t TILT_DIR_PIN = 6;
constexpr uint8_t TILT_ENABLE_PIN = 7;
constexpr uint8_t ZOOM_STEP_PIN = 8;
constexpr uint8_t ZOOM_DIR_PIN = 9;
constexpr uint8_t ZOOM_ENABLE_PIN = 10;

constexpr size_t CONTROL_QUEUE_LENGTH = 16;
constexpr size_t MOTION_QUEUE_LENGTH = 1;

// Event structure forwarded from Bluepad32 callbacks to ControlTask
enum class ControlEventType : uint8_t {
  Connected,
  Disconnected,
  GamepadData
};

struct ControlEvent {
  ControlEventType type;
  GamepadPtr gamepad;
  GamepadSnapshot snapshot;
};

// FreeRTOS handles
TaskHandle_t g_controlTaskHandle = nullptr;
TaskHandle_t g_motionTaskHandle = nullptr;
TaskHandle_t g_encoderTaskHandle = nullptr;

QueueHandle_t g_controlQueue = nullptr;
QueueHandle_t g_motionQueue = nullptr;

FastAccelStepperEngine g_stepperEngine;
MotionController g_motionController;
InputMapper g_inputMapper;
PresetManager g_presetManager;
SafetyManager g_safetyManager;

static GamepadSnapshot g_lastSnapshot{};

// Forward declarations
void ControlTask(void* parameter);
void MotionTask(void* parameter);
void EncoderTask(void* parameter);
void onConnectedGamepad(GamepadPtr gp);
void onDisconnectedGamepad(GamepadPtr gp);
void onGamepadData(GamepadPtr gp);
void onGamepadDataWithContext(GamepadPtr gp, void* context);
static void enqueueEventFromCallback(const ControlEvent& event);
static void processGamepadData(GamepadPtr gp);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  Logger::begin(&Serial);
  Logger::info("PTZ Head Firmware v0.1 booting...");

  if (!g_motionController.begin(&g_stepperEngine, {
          {PAN_STEP_PIN, PAN_DIR_PIN, PAN_ENABLE_PIN},
          {TILT_STEP_PIN, TILT_DIR_PIN, TILT_ENABLE_PIN},
          {ZOOM_STEP_PIN, ZOOM_DIR_PIN, ZOOM_ENABLE_PIN}})) {
    Logger::warn("MotionController failed to initialize steppers");
  }

  g_presetManager.begin();
  g_safetyManager.begin();
  // TODO: Initialize ESPAsyncWebServer for configuration interface (v0.2)

  g_controlQueue = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(ControlEvent));
  g_motionQueue = xQueueCreate(MOTION_QUEUE_LENGTH, sizeof(InputMapper::InputState));

  if (!g_controlQueue || !g_motionQueue) {
    Logger::error("Failed to create FreeRTOS queues");
  }

  BaseType_t controlResult = xTaskCreatePinnedToCore(
      ControlTask, "ControlTask", 8192, nullptr, 4, &g_controlTaskHandle, 1);
  BaseType_t motionResult = xTaskCreatePinnedToCore(
      MotionTask, "MotionTask", 6144, nullptr, 5, &g_motionTaskHandle, 1);
  BaseType_t encoderResult = xTaskCreatePinnedToCore(
      EncoderTask, "EncoderTask", 4096, nullptr, 3, &g_encoderTaskHandle, 1);

  if (controlResult != pdPASS || motionResult != pdPASS || encoderResult != pdPASS) {
    Logger::error("Failed to create one or more tasks");
  }

  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
#if defined(BP32_HAVE_DATA_CALLBACK) || defined(BLUEPAD32_SUPPORTS_GAMEPAD_DATA_CALLBACK)
  BP32.setGamepadDataCallback(onGamepadDataWithContext, nullptr);
#else
  BP32.setGamepadDataCallback(onGamepadData);
#endif
  BP32.enableNewBluetoothConnections(true);
  Logger::info("Bluepad32 initialized, waiting for controllers...");
}

void loop() {
  BP32.update();
  vTaskDelay(pdMS_TO_TICKS(10));
}

static void enqueueEventFromCallback(const ControlEvent& event) {
  if (!g_controlQueue) {
    return;
  }
  if (xQueueSendToBack(g_controlQueue, &event, 0) != pdTRUE) {
    Logger::warn("Control queue full - dropping event");
  }
}

void onConnectedGamepad(GamepadPtr gp) {
  ControlEvent event{};
  event.type = ControlEventType::Connected;
  event.gamepad = gp;
  enqueueEventFromCallback(event);
}

void onDisconnectedGamepad(GamepadPtr gp) {
  ControlEvent event{};
  event.type = ControlEventType::Disconnected;
  event.gamepad = gp;
  enqueueEventFromCallback(event);
}

void onGamepadData(GamepadPtr gp) {
  processGamepadData(gp);
}

void onGamepadDataWithContext(GamepadPtr gp, void* /*context*/) {
  processGamepadData(gp);
}

static void processGamepadData(GamepadPtr gp) {
  ControlEvent event{};
  event.type = ControlEventType::GamepadData;
  event.gamepad = gp;
  event.snapshot = InputMapper::createSnapshotFromGamepad(gp);
  enqueueEventFromCallback(event);
}

void ControlTask(void* /*parameter*/) {
  Logger::info("ControlTask started");
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t frequency = pdMS_TO_TICKS(2);  // 500 Hz

  uint32_t lastLogMs = millis();

  while (true) {
    ControlEvent event{};
    if (xQueueReceive(g_controlQueue, &event, pdMS_TO_TICKS(5)) == pdTRUE) {
      uint32_t nowMs = millis();
      switch (event.type) {
        case ControlEventType::Connected:
          Logger::info("Gamepad connected");
          g_safetyManager.onControllerConnected(nowMs);
          g_lastSnapshot.connected = true;
          break;
        case ControlEventType::Disconnected:
          Logger::warn("Gamepad disconnected");
          g_safetyManager.onControllerDisconnected(nowMs);
          g_motionController.stopAll();
          g_lastSnapshot = {};
          break;
        case ControlEventType::GamepadData: {
          g_lastSnapshot = event.snapshot;
          g_safetyManager.onInputActivity(nowMs);

          InputMapper::InputState mapped =
              g_inputMapper.map(event.snapshot, nowMs);

          if (g_motionQueue) {
            xQueueOverwrite(g_motionQueue, &mapped);
          }

          if (mapped.presetSave) {
            g_presetManager.saveCurrentPosition(mapped, g_motionController.getCurrentPositions());
          }
          if (mapped.presetRecall) {
            g_presetManager.recallPreset(0, g_motionController);
          }
          if (mapped.resetEncoders) {
            g_motionController.resetEncoders();
          }
          if (mapped.diagnosticsDump) {
            g_motionController.dumpDiagnostics();
            g_presetManager.dumpDiagnostics();
            g_safetyManager.dumpDiagnostics();
          }
          break;
        }
      }
    }

    uint32_t nowMs = millis();
    if (g_safetyManager.shouldForceIdle(nowMs)) {
      InputMapper::InputState idleState{};
      idleState.timestampMs = nowMs;
      if (g_motionQueue) {
        xQueueOverwrite(g_motionQueue, &idleState);
      }
      g_motionController.stopAll();
    }

    if (nowMs - lastLogMs >= 5000) {
      Logger::info("Status: connected=%s battery=%u lastInput=%lu ms", g_lastSnapshot.connected ? "yes" : "no", g_lastSnapshot.batteryLevel, nowMs - g_safetyManager.lastInputTimestamp());
      lastLogMs = nowMs;
    }

    vTaskDelayUntil(&lastWakeTime, frequency);
  }
}

void MotionTask(void* /*parameter*/) {
  Logger::info("MotionTask started");
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t frequency = pdMS_TO_TICKS(1);  // 1 kHz
  InputMapper::InputState currentState{};

  while (true) {
    if (g_motionQueue) {
      InputMapper::InputState newState{};
      if (xQueueReceive(g_motionQueue, &newState, 0) == pdTRUE) {
        currentState = newState;
      }
    }

    g_motionController.update(currentState);
    g_safetyManager.updateHeartbeat(millis());
    vTaskDelayUntil(&lastWakeTime, frequency);
  }
}

void EncoderTask(void* /*parameter*/) {
  Logger::info("EncoderTask started");
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t frequency = pdMS_TO_TICKS(5);  // 200 Hz

  while (true) {
    g_motionController.refreshEncoderData();
    vTaskDelayUntil(&lastWakeTime, frequency);
  }
}
