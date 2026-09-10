"""Host-test production transaction methods with BLE/scheduler/entity test doubles."""
from pathlib import Path
import re
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
p = root / 'components/grundfos_magna3'
h = (p / 'grundfos_magna3.h').read_text()
h = re.sub(r'#include "esphome/[^\n]+\n', '', h).replace('#pragma once', '')
h = re.sub(r'  void (?:gap|gattc)_event_handler\([^;]+;', '', h)
cpp = (p / 'grundfos_magna3.cpp').read_text()
cpp = cpp[:cpp.index('void Magna3::gap_event_handler')] + '}\n'
cpp = re.sub(r'#include "(?:grundfos_magna3.h|esphome/[^\"]+)"\n', '', cpp)
a = cpp.index('bool Magna3::send_(')
b = cpp.index('void Magna3::update()', a)
cpp = cpp[:a] + '''bool Magna3::send_(uint8_t *data, size_t size) {
  if (!ready_) return false;
  sent.emplace_back(data, data + size); return true;
}
''' + cpp[b:]
stubs = r'''
#include <map>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <cmath>
std::vector<std::vector<uint8_t>> sent;
template<class... T> void log_stub(T...) {}
#define ESP_LOGD log_stub
#define ESP_LOGI log_stub
#define ESP_LOGW log_stub
#define ESP_LOGCONFIG log_stub
#define LOG_UPDATE_INTERVAL log_stub
namespace esphome {
uint32_t millis() { return 0; }
std::string format_hex_pretty(const uint8_t *, size_t) { return ""; }
struct PollingComponent {
  virtual void update() {} virtual void loop() {} virtual void dump_config() {}
  std::map<std::string, std::function<void()>> timers;
  void set_timeout(const char *name, uint32_t, std::function<void()> f) { timers[name] = f; }
  void cancel_timeout(const char *name) { timers.erase(name); }
  void fire(const char *name) { auto f = timers.at(name); timers.erase(name); f(); }
  void status_set_warning() {} void status_clear_warning() {}
};
namespace number { struct Number { float state = NAN; int updates = 0;
  void publish_state(float v) { state = v; ++updates; }
  virtual void control(float) = 0;
}; }
namespace select { struct Select { void publish_state(const char *) {} virtual void control(const std::string &) = 0; }; }
namespace switch_ { struct Switch { void publish_state(bool) {} virtual void write_state(bool) = 0; }; }
namespace text_sensor { struct TextSensor { void publish_state(const char *) {} }; }
namespace sensor { struct Sensor { void publish_state(float) {} }; }
namespace esp32_ble_tracker { struct ESPBTUUID {}; }
namespace ble_client { struct BLEClientNode { void disconnect() {} BLEClientNode *parent() { return this; } }; }
}
'''
tests = r'''
using namespace esphome::grundfos_magna3;
struct Pump : Magna3 {
  using Magna3::process_mode_write_; using Magna3::process_setpoint_;
  using Magna3::publish_control_mode_; using Magna3::publish_operating_mode_;
  using Magna3::reset_;
  Pump(uint8_t mode = 0, uint8_t op = 0) {
    ready_ = true; setpoint_context_.observe(mode); setpoint_context_.begin_read(); setpoint_context_.accept();
    operating_mode_ = op; sent.clear();
  }
  void ack(uint8_t status = 0, bool valid = true) {
    std::array<uint8_t, 8> f{0x24,4,0xF8,0xE7,3,status,0,0}; protocol::checksum(f);
    if (!valid) f[7] ^= 1;
    receive_(f.data(), f.size());
  }
  void value(uint16_t raw) {
    std::array<uint8_t, 10> f{0x24,6,0xF8,0xE7,0x0C,2,uint8_t(raw>>8),uint8_t(raw),0,0};
    protocol::checksum(f); receive_(f.data(), f.size());
  }
};
int main() {
  // No TX for stopped/unknown operating mode, inactive entity, automatic mode or NaN.
  for (uint8_t op : {1,255}) { Pump p(0,op); p.write_number_setpoint(0,5); assert(sent.empty()); }
  { Pump p; p.write_number_setpoint(8,5); p.write_number_setpoint(0,NAN); assert(sent.empty()); }
  for (uint8_t mode : {5,10}) { Pump p(mode); p.write_number_setpoint(mode,5); assert(sent.empty()); }
  // Six routes: direct SET in Normal, actual readback only to the matching number.
  for (uint8_t mode : {0,1,2,6,8,12}) {
    Pump p(mode); SetpointNumber numbers[13];
    for (uint8_t m : {0,1,2,6,8,12}) p.set_number(m, &numbers[m]);
    auto cfg = protocol::setpoint_config(mode);
    float value = (cfg.min_value + cfg.max_value) / 2;
    p.write_number_setpoint(mode,value); assert(sent.size()==1 && sent[0][4]==0x0C);
    uint16_t raw; assert(protocol::setpoint_to_raw(mode,value,raw));
    assert(sent[0][7]==(raw>>8) && sent[0][8]==(raw&255));
    assert(numbers[mode].updates==0);
    p.fire("readback"); p.value(raw);
    for (uint8_t m : {0,1,2,6,8,12}) assert(numbers[m].updates==(m==mode));
    assert(sent.size()==2); // SET + GET; publish cannot cause a new SET.
  }
  for (uint8_t op : {2,3}) {
    Pump p(0,op); p.write_number_setpoint(0,4); p.process_mode_write_();
    assert(sent.size()==1 && sent[0]==std::vector<uint8_t>({0x27,5,0xE7,0xF8,3,0x81,6,0xE5,0x87}));
    p.write_number_setpoint(0,5); p.write_number_setpoint(8,9);
    p.process_setpoint_(); assert(sent.size()==1);
    p.publish_operating_mode_(0x30); p.process_setpoint_(); assert(sent.size()==1); // No ACK yet.
    p.ack(); p.process_setpoint_(); assert(sent.size()==1); // Need readback after ACK.
    p.publish_operating_mode_(0x30); p.process_setpoint_(); assert(sent.size()==2 && sent[1][4]==0x0C);
    uint16_t raw; protocol::setpoint_to_raw(0,5,raw);
    assert(sent[1][7]==(raw>>8) && sent[1][8]==(raw&255));
    p.process_setpoint_(); assert(sent.size()==2);
  }
  // Cancel before final SET: negative ACK, timeout, disconnect, or Control Mode change.
  for (int failure=0; failure<5; ++failure) {
    Pump p(0,2); p.write_number_setpoint(0,5); p.process_mode_write_();
    if (failure==0) p.ack(0xC0);
    if (failure==1) p.fire("mode_readback");
    if (failure==2) p.reset_();
    if (failure==3) { p.ack(); p.publish_control_mode_(8); }
    if (failure==4) { p.ack(0,false); p.fire("mode_readback"); }
    p.publish_operating_mode_(0x30); p.process_setpoint_(); assert(sent.size()==1);
  }
  { Pump p(0,3); p.write_number_setpoint(0,5); p.process_mode_write_(); p.ack();
    p.publish_operating_mode_(0x33); p.process_setpoint_(); assert(sent.size()==1);
    p.fire("mode_readback"); p.publish_operating_mode_(0x30); p.process_setpoint_(); assert(sent.size()==1);
  }
}
'''
with tempfile.TemporaryDirectory(prefix='magna3-sequence-') as d:
    source = Path(d) / 'test.cpp'
    source.write_text(stubs + h + cpp + tests)
    binary = Path(d) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I', str(p), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Setpoint sequence tests passed')
