#include "PresetManager.h"

#include "Logger.h"

namespace {
uint32_t simpleCRC(const uint8_t* data, size_t length) {
  uint32_t crc = 0xA5A5A5A5;
  for (size_t i = 0; i < length; ++i) {
    crc = (crc << 5) ^ (crc >> 27) ^ data[i];
  }
  return crc;
}
}

void PresetManager::begin() {
  if (!EEPROM.begin(kEepromSize)) {
    Logger::warn("EEPROM init failed - presets disabled");
  }
}

void PresetManager::saveCurrentPosition(const InputMapper::InputState& state,
                                        const MotionController::AxisPositions& positions) {
  (void)state;  // TODO: map button combinations to preset index
  PresetRecord record{};
  record.positions = positions;
  record.crc = calculateCRC(record);
  writePreset(0, record);
  Logger::info("Saved preset to slot 0 (TODO: dynamic slot selection)");
}

void PresetManager::recallPreset(uint8_t index, MotionController& controller) {
  PresetRecord record{};
  if (!readPreset(index, &record)) {
    Logger::warn("Preset %u invalid or empty", index);
    return;
  }
  controller.moveToPreset(record.positions);
}

void PresetManager::dumpDiagnostics() const {
  for (uint8_t i = 0; i < kMaxPresets; ++i) {
    PresetRecord record{};
    if (readPreset(i, &record)) {
      Logger::info("Preset %u: pan=%ld tilt=%ld zoom=%ld", i,
                   static_cast<long>(record.positions.panSteps),
                   static_cast<long>(record.positions.tiltSteps),
                   static_cast<long>(record.positions.zoomSteps));
    }
  }
}

uint32_t PresetManager::calculateCRC(const PresetRecord& record) const {
  return simpleCRC(reinterpret_cast<const uint8_t*>(&record.positions),
                   sizeof(record.positions));
}

void PresetManager::writePreset(uint8_t index, const PresetRecord& record) {
  if (index >= kMaxPresets) {
    return;
  }
  size_t offset = index * sizeof(PresetRecord);
  EEPROM.put(offset, record);
  if (!EEPROM.commit()) {
    Logger::warn("EEPROM commit failed for preset %u", index);
  }
}

bool PresetManager::readPreset(uint8_t index, PresetRecord* out) const {
  if (!out || index >= kMaxPresets) {
    return false;
  }
  size_t offset = index * sizeof(PresetRecord);
  EEPROM.get(offset, *out);
  if (out->crc == 0) {
    return false;
  }
  uint32_t crc = calculateCRC(*out);
  return crc == out->crc;
}

