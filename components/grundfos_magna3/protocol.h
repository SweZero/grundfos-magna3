#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::grundfos_magna3::protocol {
// CRC-16/GENIBUS: poly 0x1021, init/xorout 0xFFFF, no reflection.
// Covers the length byte and body; excludes start byte and checksum.
inline uint16_t crc(const uint8_t *data, size_t size) {
  uint16_t value = 0xFFFF;
  for (size_t i = 0; i < size; i++) {
    value ^= uint16_t(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      value = (value & 0x8000) ? (value << 1) ^ 0x1021 : value << 1;
  }
  return value ^ 0xFFFF;
}
template<size_t N> inline void checksum(std::array<uint8_t, N> &frame) {
  const auto value = crc(frame.data() + 1, N - 3);
  frame[N - 2] = value >> 8;
  frame[N - 1] = value;
}
inline std::array<uint8_t, 9> get() {
  std::array<uint8_t, 9> frame{0x27, 0x05, 0xE7, 0xF8, 0x0C, 0x01, 0xCA, 0, 0};
  checksum(frame);
  return frame;
}
inline std::array<uint8_t, 11> set(uint16_t raw) {
  std::array<uint8_t, 11> frame{0x27, 0x07, 0xE7, 0xF8, 0x0C, 0x83, 0xCA,
                               uint8_t(raw >> 8), uint8_t(raw), 0, 0};
  checksum(frame);
  return frame;
}
// MAGNA3 captured response omits the item ID: 24 06 F8 E7 0C 02 HI LO CRC CRC.
// Untagged values are only meaningful while awaiting our single-item GET.
inline bool read_value(const uint8_t *f, size_t n, uint16_t &raw, bool awaiting_get = false) {
  if (n != 10 && n != 11) return false;
  if (f[0] != 0x24 || f[1] != n - 4 || f[2] != 0xF8 || f[3] != 0xE7 || f[4] != 0x0C)
    return false;
  if (crc(f + 1, n - 3) != ((uint16_t(f[n - 2]) << 8) | f[n - 1])) return false;
  size_t offset;
  if (n == 10 && f[5] == 2 && awaiting_get) offset = 6;
  else if (n == 11 && f[5] == 3 && f[6] == 0xCA) offset = 7;
  else return false;
  raw = (uint16_t(f[offset]) << 8) | f[offset + 1];
  return raw <= 10000;
}
}  // namespace esphome::grundfos_magna3::protocol
