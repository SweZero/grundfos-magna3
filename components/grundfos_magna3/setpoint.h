#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace esphome::grundfos_magna3::protocol {
struct SetpointModeConfig {
  bool writable;
  float min_value, max_value, step;
  const char *unit;
  bool percent;
};
inline SetpointModeConfig setpoint_config(uint8_t mode) {
  switch (mode) {
    case 0x00: return {true, 1.02f, 7.45f, 0.01f, "m", false};
    case 0x01: return {true, 1.94f, 7.45f, 0.01f, "m", false};
    case 0x02: return {true, 35, 100, 1, "%", true};
    case 0x06: return {true, -10, 130, 1, "°C", false};
    case 0x08: return {true, 1.2f, 10.5f, 0.1f, "m³/h", false};
    case 0x0C: return {true, 0, 140, 1, "°C", false};
    default: return {false, 0, 0, 0, "", false};
  }
}
inline float raw_to_range(uint16_t raw, float min, float max) {
  return min + (raw / 10000.0f) * (max - min);
}
// Caller rejects non-finite values before conversion.
inline uint16_t range_to_raw(float value, float min, float max) {
  value = std::clamp(value, min, max);
  return static_cast<uint16_t>(std::clamp(std::lround((value - min) / (max - min) * 10000.0f), 0L, 10000L));
}
inline bool setpoint_from_raw(uint8_t mode, uint16_t raw, float &value) {
  const auto cfg = setpoint_config(mode);
  if (!cfg.writable || raw > 10000) return false;
  value = cfg.percent ? raw / 100.0f : raw_to_range(raw, cfg.min_value, cfg.max_value);
  return true;
}
inline bool setpoint_to_raw(uint8_t mode, float value, uint16_t &raw) {
  const auto cfg = setpoint_config(mode);
  if (!cfg.writable || !std::isfinite(value)) return false;
  value = std::clamp(value, cfg.min_value, cfg.max_value);
  raw = cfg.percent ? static_cast<uint16_t>(std::lround(value * 100.0f))
                    : range_to_raw(value, cfg.min_value, cfg.max_value);
  return true;
}
// A reply is usable only for a GET issued after the latest observed mode change.
struct SetpointContext {
  uint8_t mode{0xFF}, requested_mode{0xFF};
  bool fresh{false}, needs_read{true};
  void observe(uint8_t value) {
    if (mode != value) { mode = value; fresh = false; needs_read = true; }
  }
  void invalidate() { mode = requested_mode = 0xFF; fresh = false; needs_read = true; }
  void begin_read() { requested_mode = mode; needs_read = false; }
  bool accept() {
    fresh = requested_mode == mode && setpoint_config(mode).writable;
    return fresh;
  }
};
}  // namespace esphome::grundfos_magna3::protocol
