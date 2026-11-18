#pragma once
#include "protocol.h"

namespace esphome::grundfos_magna3::protocol {
// Append starts bytes to preserve the original twelve payload offsets.
inline constexpr std::array<uint8_t, 16> TELEMETRY_IDS{
    0x18, 0x19, 0x1E, 0x22, 0x23, 0x24, 0x25, 0x27, 0x3A, 0x5A, 0x98, 0x99, 0x0B, 0x0C, 0x70, 0x55};
inline std::array<uint8_t, TELEMETRY_IDS.size() + 8> telemetry_get() {
  std::array<uint8_t, TELEMETRY_IDS.size() + 8> frame{};
  frame[0] = 0x27;
  frame[1] = TELEMETRY_IDS.size() + 4;
  frame[2] = 0xF7;
  frame[3] = 0xF8;
  frame[4] = 0x02;
  frame[5] = TELEMETRY_IDS.size();
  for (size_t i = 0; i < TELEMETRY_IDS.size(); i++) frame[6 + i] = TELEMETRY_IDS[i];
  checksum(frame);
  return frame;
}
// Empirical conversion derived from measurements on MAGNA3 25-80 180.
// Not confirmed as the protocol-defined scaling for all MAGNA3 models.
inline float head_meters(uint8_t raw) {
  const float head = 0.07767643f * raw - 0.02687265f;
  return head < 0.0f ? 0.0f : (head > 10.0f ? 10.0f : head);
}
// Same entity order as sensor.py; new derived head and starts follow original sensors.
struct Telemetry { std::array<float, 12> values; uint8_t operating_mode_raw; };
inline bool read_telemetry(const uint8_t *f, size_t n, Telemetry &result) {
  // One complete APDU: 6 header + 16 values + 2 CRC bytes.
  // Reject trailing data as well as truncated frames; reassembly splits frames upstream.
  if (n != TELEMETRY_IDS.size() + 8 || f[0] != 0x24 || f[1] != TELEMETRY_IDS.size() + 4 || f[2] != 0xF8 || f[3] != 0xF7 ||
      f[4] != 0x02 || f[5] != TELEMETRY_IDS.size()) return false;
  if (crc(f + 1, n - 3) != ((uint16_t(f[n - 2]) << 8) | f[n - 1])) return false;
  const auto *d = f + 6;
  const float temperature = d[8] < 128 ? float(d[8]) : float(int(d[8]) - 256);
  result.values = {float((uint16_t(d[0]) << 8) | d[1]) * 2.0f,
      d[2] / 10.0f, float(d[3]), float((uint16_t(d[4]) << 8) | d[5]),
      float(d[6]), float(d[7]), temperature, float(d[9]),
      float((uint16_t(d[10]) << 8) | d[11]) * 2.0f,
      head_meters(d[6]), float((uint16_t(d[12]) << 8) | d[13]), float(d[14])};
  result.operating_mode_raw = d[15];
  return true;
}
}  // namespace esphome::grundfos_magna3::protocol
