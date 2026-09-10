#pragma once
#include "control_mode.h"

namespace esphome::grundfos_magna3::protocol {
enum class OperatingMode : uint8_t { NORMAL = 0, STOP = 1, MIN = 2, MAX = 3, UNKNOWN = 0xFF };
// Reuse the class-03 command definition/builder already used by Control Mode.
inline constexpr std::array<ControlMode, 4> OPERATING_MODES{{
    {"Normal", 0x06, 0x00}, {"Stop", 0x05, 0x01},
    {"Min", 0x19, 0x02}, {"Max", 0x1A, 0x03},
}};
inline OperatingMode operating_mode(uint8_t raw) {
  // Only the low two bits encode Operating Mode; preserve higher bits in RX logs.
  return static_cast<OperatingMode>(raw & 0x03);
}
inline const char *operating_mode_name(OperatingMode mode) {
  const auto raw = static_cast<uint8_t>(mode);
  return raw < OPERATING_MODES.size() ? OPERATING_MODES[raw].name : "Unknown";
}
// Unknown must leave the previous boolean unchanged, rather than infer stopped/running.
inline bool operating_running(OperatingMode mode, bool &running) {
  if (mode == OperatingMode::UNKNOWN) return false;
  running = mode != OperatingMode::STOP;
  return true;
}
}  // namespace esphome::grundfos_magna3::protocol
