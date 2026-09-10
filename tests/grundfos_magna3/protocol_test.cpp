#include "../../components/grundfos_magna3/protocol.h"
#include <cassert>
#include <limits>
using namespace esphome::grundfos_magna3;
int main() {
  const uint8_t check[] = "123456789";
  assert(protocol::crc(check, 9) == 0xD64E);
  const std::array<uint8_t, 9> expected{0x27, 0x05, 0xE7, 0xF8, 0x0C, 0x01, 0xCA, 0xCA, 0xEE};
  assert(protocol::get() == expected);
  uint16_t raw;
  const uint8_t captured[]{0x24, 0x06, 0xF8, 0xE7, 0x0C, 0x02, 0x00, 0x00, 0x4E, 0x60};
  assert(protocol::read_value(captured, sizeof(captured), raw, true) && raw == 0);
  assert(!protocol::read_value(captured, sizeof(captured), raw, false));
  const uint8_t maximum[]{0x24,0x06,0xF8,0xE7,0x0C,0x02,0x27,0x10,0xC3,0x20};
  const uint8_t actual[]{0x24,0x06,0xF8,0xE7,0x0C,0x02,0x1B,0x09,0x00,0xC0};
  const uint8_t ack[]{0x24,0x04,0xF8,0xE7,0x0C,0x00,0x1D,0xB7};
  assert(protocol::read_value(maximum, sizeof(maximum), raw, true) && raw == 10000);
  assert(protocol::read_value(actual, sizeof(actual), raw, true) && raw == 6921);
  assert(!protocol::read_value(ack, sizeof(ack), raw, true));
  auto frame = protocol::set(5000);
  assert(frame[5] == 0x83 && frame[7] == 0x13 && frame[8] == 0x88);
  // Synthetic reply: verify parser constraints, not hardware compatibility.
  frame[0] = 0x24; frame[2] = 0xF8; frame[3] = 0xE7; frame[5] = 3;
  protocol::checksum(frame);
  assert(protocol::read_value(frame.data(), frame.size(), raw) && raw == 5000);
  for (size_t n = 0; n < frame.size(); n++) assert(!protocol::read_value(frame.data(), n, raw));
  for (size_t i = 0; i < frame.size(); i++) {
    frame[i] ^= 1;
    assert(!protocol::read_value(frame.data(), frame.size(), raw));
    frame[i] ^= 1;
  }
  frame[5] = 0x83; protocol::checksum(frame);
  assert(!protocol::read_value(frame.data(), frame.size(), raw));
  frame[5] = 3; frame[7] = 0xFF; protocol::checksum(frame);
  assert(!protocol::read_value(frame.data(), frame.size(), raw));
}
