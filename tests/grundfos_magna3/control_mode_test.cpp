#include "../../components/grundfos_magna3/control_mode.h"
#include <cassert>
using namespace esphome::grundfos_magna3;
int main() {
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x18,0x16,0x78};
    assert(protocol::mode_command(protocol::CONTROL_MODES[0]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x17,0xE7,0x97};
    assert(protocol::mode_command(protocol::CONTROL_MODES[1]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x16,0xF7,0xB6};
    assert(protocol::mode_command(protocol::CONTROL_MODES[2]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x34,0xF3,0x96};
    assert(protocol::mode_command(protocol::CONTROL_MODES[3]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x3D,0x62,0xBF};
    assert(protocol::mode_command(protocol::CONTROL_MODES[4]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x3F,0x42,0xFD};
    assert(protocol::mode_command(protocol::CONTROL_MODES[5]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x41,0xDD,0xA4};
    assert(protocol::mode_command(protocol::CONTROL_MODES[6]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x61,0xF9,0xC6};
    assert(protocol::mode_command(protocol::CONTROL_MODES[7]) == expected);
  }
  for (unsigned raw = 0; raw < 256; raw++) {
    const int index = protocol::mode_index(uint8_t(raw));
    if (index >= 0) {
      assert(protocol::CONTROL_MODES[index].raw == raw);
      assert(protocol::mode_index(protocol::CONTROL_MODES[index].name) == index);
    } else {
      for (const auto &mode : protocol::CONTROL_MODES) assert(mode.raw != raw);
    }
  }
  assert(protocol::mode_index("invalid") == -1);
  std::array<uint8_t,8> rejected{0x24,0x04,0xF8,0xE7,0x03,0xC0,0,0};
  protocol::checksum(rejected);
  assert(std::strcmp(protocol::mode_ack(rejected.data(), rejected.size()), "Illegal Operation") == 0);
  const uint8_t unrelated[]{0x24,0x04,0xF8,0xE7,0x02,0xC0,0xE7,0xF4};
  assert(protocol::mode_ack(unrelated, sizeof(unrelated)) == nullptr);
  const std::array<uint8_t,8> accepted{0x24,0x04,0xF8,0xE7,0x03,0x00,0x0D,0x89};
  assert(std::strcmp(protocol::mode_ack(accepted.data(), accepted.size()), "OK") == 0);
  for (size_t n=0; n<8; n++) assert(protocol::mode_ack(rejected.data(), n) == nullptr);
  for (size_t i=0; i<8; i++) {
    auto bad = rejected; bad[i] ^= 1;
    assert(protocol::mode_ack(bad.data(), bad.size()) == nullptr);
  }
  for (size_t i : {size_t(0),size_t(1),size_t(2),size_t(3),size_t(4)}) {
    auto bad = rejected; bad[i] = 0; protocol::checksum(bad);
    assert(protocol::mode_ack(bad.data(), bad.size()) == nullptr);
  }
}
