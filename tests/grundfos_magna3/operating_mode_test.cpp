#include "../../components/grundfos_magna3/operating_mode.h"
#include <cassert>
using namespace esphome::grundfos_magna3;
int main() {
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x06,0xE5,0x87};
    assert(protocol::mode_command(protocol::OPERATING_MODES[0]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x05,0xD5,0xE4};
    assert(protocol::mode_command(protocol::OPERATING_MODES[1]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x19,0x06,0x59};
    assert(protocol::mode_command(protocol::OPERATING_MODES[2]) == expected);
  }
  {
    const std::array<uint8_t,9> expected{0x27,0x05,0xE7,0xF8,0x03,0x81,0x1A,0x36,0x3A};
    assert(protocol::mode_command(protocol::OPERATING_MODES[3]) == expected);
  }
  for (unsigned raw=0; raw<256; raw++) {
    const auto mode = protocol::operating_mode(raw);
    for (bool previous : {false, true}) {
      bool running = previous;
      const unsigned decoded = raw & 0x03;
      assert(protocol::operating_running(mode, running));
      assert(running == (decoded != 1));
      assert(std::strcmp(protocol::operating_mode_name(mode), protocol::OPERATING_MODES[decoded].name) == 0);
      // Confirmation must match decoded mode, never the full status byte.
      for (unsigned requested = 0; requested < 4; ++requested)
        assert((static_cast<uint8_t>(mode) == requested) == (decoded == requested));
    }
  }
  const uint8_t ack[]{0x24,0x04,0xF8,0xE7,0x03,0x00,0x0D,0x89};
  assert(std::strcmp(protocol::mode_ack(ack, sizeof(ack)), "OK") == 0);
}
