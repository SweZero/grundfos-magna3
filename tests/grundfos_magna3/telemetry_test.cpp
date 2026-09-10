#include "../../components/grundfos_magna3/telemetry.h"
#include <cassert>
using namespace esphome::grundfos_magna3;
int main() {
  // New aggregate fixtures: CRC independently calculated with Python binascii.crc_hqx.
  const std::array<uint8_t, 24> request{0x27,0x14,0xE7,0xF8,0x02,0x10,0x18,0x19,0x1E,0x22,0x23,0x24,0x25,0x27,0x3A,0x5A,0x98,0x99,0x0B,0x0C,0x70,0x55,0x36,0x80};
  assert(protocol::telemetry_get() == request);
  std::array<uint8_t, 24> reply{0x24,0x14,0xF8,0xE7,0x02,0x10,0x10,0x2D,0x09,0x43,0x0C,0xA0,0x46,0x08,0x0D,0x3F,0x00,0xF4,0x02,0x2F,0x05,0x02,0x0E,0x34};
  protocol::Telemetry result{};
  assert(protocol::read_telemetry(reply.data(), reply.size(), result));
  const std::array<float,12> expected{8282,0.9f,67,3232,70,8,13,63,488,0.07767643f * 70 - 0.02687265f,559,5};
  assert(result.values == expected);
  assert(result.operating_mode_raw == 2);
  for (size_t n=0; n<reply.size(); n++) {
    assert(!protocol::read_telemetry(reply.data(), n, result));
    assert(result.values == expected);
  }
  for (size_t i=0; i<reply.size(); i++) {
    auto corrupted = reply; corrupted[i] ^= 1;
    assert(!protocol::read_telemetry(corrupted.data(), corrupted.size(), result));
    assert(result.values == expected);
  }
  // Header constraints must also reject frames with a freshly correct CRC.
  for (size_t i=0; i<6; i++) {
    auto invalid = reply; invalid[i] ^= 1; protocol::checksum(invalid);
    assert(!protocol::read_telemetry(invalid.data(), invalid.size(), result));
  }
  std::array<uint8_t,25> extra{};
  std::copy(reply.begin(), reply.end(), extra.begin());
  assert(!protocol::read_telemetry(extra.data(), extra.size(), result));
  for (auto temperature : {0x80, 0xFF, 0x7F}) {
    reply[14] = temperature; protocol::checksum(reply);
    assert(protocol::read_telemetry(reply.data(), reply.size(), result));
    assert(result.values[6] == (temperature < 128 ? temperature : temperature - 256));
  }
  for (uint8_t mode : {0x00,0x01,0x02,0x05,0x06,0x08,0x0A,0x0C,0xFF}) {
    reply[20] = mode; protocol::checksum(reply);
    assert(protocol::read_telemetry(reply.data(), reply.size(), result));
    assert(result.values[11] == mode);
    assert(result.values[0] == 8282 && result.values[8] == 488 && result.values[10] == 559);
  }
  // Independent approximation values at the supplied calibration points.
  assert(std::fabs(protocol::head_meters(27) - 2.070391f) < 0.00001f);
  assert(std::fabs(protocol::head_meters(66) - 5.099772f) < 0.00001f);
  assert(std::fabs(protocol::head_meters(70) - 5.410477f) < 0.00001f);
  assert(std::fabs(protocol::head_meters(92) - 7.119359f) < 0.00001f);
  assert(protocol::head_meters(0) == 0);
  assert(protocol::head_meters(255) == 10);
  for (uint16_t starts : {0, 255, 256, 559, 65535}) {
    reply[18] = starts >> 8; reply[19] = starts; protocol::checksum(reply);
    assert(protocol::read_telemetry(reply.data(), reply.size(), result));
    assert(result.values[10] == starts);
    assert(result.values[0] == 8282 && result.values[8] == 488);
  }
  // A delayed response to the old twelve-ID request must not be treated as the new layout.
  const uint8_t old[]{0x24,0x10,0xF8,0xE7,0x02,0x0C,0x10,0x2D,0x09,0x43,
                     0x0C,0xA0,0x46,0x08,0x0D,0x3F,0x00,0xF4,0x1A,0xA9};
  assert(!protocol::read_telemetry(old, sizeof(old), result));

  // Actual readback after class-2 C0 refusal: requested Proportional Pressure is active.
  const uint8_t readback[]{0x24,0x13,0xF8,0xE7,0x02,0x0F,0x10,0x33,0x02,0x0A,
      0x06,0x40,0x11,0x03,0x13,0x3F,0x00,0xF4,0x02,0x2F,0x01,0x2A,0x71};
  assert(!protocol::read_telemetry(readback, sizeof(readback), result));
  // Operating Mode byte must never shift/overwrite Control Mode or other values.
  for (uint8_t op : {0,1,2,3,255}) {
    reply[21] = op; protocol::checksum(reply);
    assert(protocol::read_telemetry(reply.data(), reply.size(), result));
    assert(result.operating_mode_raw == op);
    assert(result.values[0] == 8282 && result.values[8] == 488 && result.values[11] == 255);
    auto bad = reply; bad[21] ^= 1;
    assert(!protocol::read_telemetry(bad.data(), bad.size(), result));
    assert(result.operating_mode_raw == op);
  }
}
