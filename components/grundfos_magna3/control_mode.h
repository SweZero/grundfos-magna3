#pragma once
#include "protocol.h"
#include <cstring>

namespace esphome::grundfos_magna3::protocol {
struct ControlMode { const char *name; uint8_t command; uint8_t raw; };
inline constexpr std::array<ControlMode, 8> CONTROL_MODES{{
    {"Constant Pressure", 0x18, 0x00}, {"Proportional Pressure", 0x17, 0x01},
    {"Constant Curve", 0x16, 0x02}, {"AUTOADAPT", 0x34, 0x05},
    {"Constant Temperature", 0x3D, 0x06}, {"Constant Flow", 0x3F, 0x08},
    {"FLOWADAPT", 0x41, 0x0A}, {"Differential Temperature", 0x61, 0x0C},
}};
inline int mode_index(uint8_t raw) {
  for (size_t i = 0; i < CONTROL_MODES.size(); i++) if (CONTROL_MODES[i].raw == raw) return i;
  return -1;
}
inline int mode_index(const char *name) {
  for (size_t i = 0; i < CONTROL_MODES.size(); i++) if (std::strcmp(CONTROL_MODES[i].name, name) == 0) return i;
  return -1;
}
// Control/Operating Mode writes use class 03 in this implementation.
// Class 02 datapoints 0x70 and 0x55 are readback-only here: SET attempts were
// rejected by the tested pump. This does not describe all GENI class 02 commands.
inline const char *mode_ack(const uint8_t *f, size_t n) {
  if (n != 8 || f[0] != 0x24 || f[1] != 4 || f[2] != 0xF8 || f[3] != 0xF7 ||
      f[4] != 0x03 || (f[5] & 0x3F) != 0) return nullptr;
  if (crc(f + 1, n - 3) != ((uint16_t(f[6]) << 8) | f[7])) return nullptr;
  switch (f[5] >> 6) {
    case 0: return "OK";
    case 1: return "Unknown Class";
    case 2: return "Unknown Data Item";
    case 3: return "Illegal Operation";
  }
  return nullptr;
}
inline std::array<uint8_t, 9> mode_command(const ControlMode &mode) {
  std::array<uint8_t, 9> frame{0x27,0x05,0xF7,0xF8,0x03,0x81,mode.command,0,0};
  checksum(frame);
  return frame;
}
}  // namespace esphome::grundfos_magna3::protocol
