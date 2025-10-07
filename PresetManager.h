#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "InputMapper.h"
#include "MotionController.h"

class PresetManager {
 public:
  struct PresetRecord {
    uint32_t crc = 0;
    MotionController::AxisPositions positions;
  };

  void begin();

  void saveCurrentPosition(const InputMapper::InputState& state,
                           const MotionController::AxisPositions& positions);

  void recallPreset(uint8_t index, MotionController& controller);

  void dumpDiagnostics() const;

 private:
  uint32_t calculateCRC(const PresetRecord& record) const;
  void writePreset(uint8_t index, const PresetRecord& record);
  bool readPreset(uint8_t index, PresetRecord* out) const;

  static constexpr uint8_t kMaxPresets = 6;
  static constexpr size_t kEepromSize = kMaxPresets * sizeof(PresetRecord);
};

