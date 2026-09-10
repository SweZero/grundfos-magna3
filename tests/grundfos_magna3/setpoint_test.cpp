#include "../../components/grundfos_magna3/setpoint.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace esphome::grundfos_magna3::protocol;
int main() {
  for (uint8_t mode : {0x00, 0x01, 0x06, 0x08, 0x0C}) {
    auto cfg = setpoint_config(mode);
    float value;
    uint16_t raw;
    assert(setpoint_from_raw(mode, 0, value) && std::fabs(value - cfg.min_value) < 0.0001f);
    assert(setpoint_from_raw(mode, 10000, value) && std::fabs(value - cfg.max_value) < 0.0001f);
    assert(setpoint_to_raw(mode, cfg.min_value - 100, raw) && raw == 0);
    assert(setpoint_to_raw(mode, cfg.max_value + 100, raw) && raw == 10000);
    for (int i = 0; i <= 10000; ++i) {
      assert(setpoint_from_raw(mode, i, value));
      assert(setpoint_to_raw(mode, value, raw) && raw == i);
    }
    assert(!setpoint_to_raw(mode, NAN, raw));
    assert(!setpoint_to_raw(mode, INFINITY, raw));
    assert(!setpoint_from_raw(mode, 10001, value));
  }
  float value; uint16_t raw;
  assert(setpoint_from_raw(2, 3460, value) && std::fabs(value - 34.6f) < 0.001f);
  assert(setpoint_to_raw(2, 35, raw) && raw == 3500);
  assert(setpoint_to_raw(2, 50, raw) && raw == 5000);
  assert(setpoint_to_raw(2, 0, raw) && raw == 3500);
  assert(setpoint_to_raw(2, 101, raw) && raw == 10000);
  assert(setpoint_from_raw(6, 5000, value) && value == 60);
  assert(setpoint_from_raw(12, 5000, value) && value == 70);
  for (int i = 0; i < 256; ++i) {
    if (setpoint_config(i).writable) continue;
    assert(!setpoint_to_raw(i, 4, raw));
    assert(!setpoint_from_raw(i, 5000, value));
  }
  SetpointContext context;
  context.begin_read(); assert(!context.accept());
  context.observe(0); context.begin_read(); assert(context.accept());
  context.observe(6); assert(!context.fresh && context.needs_read);
  assert(!context.accept()); // Old CP reply cannot be interpreted as temperature.
  context.begin_read(); assert(context.accept());
  context.invalidate(); assert(!context.fresh && !context.accept());
  context.observe(5); context.begin_read(); assert(!context.accept());
}
