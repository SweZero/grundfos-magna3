#pragma once
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "setpoint.h"
#include <vector>
#include <array>
#include "esphome/components/sensor/sensor.h"

namespace esphome::grundfos_magna3 {
class Magna3;
class SetpointNumber : public number::Number {
 public:
  void set_control_mode(uint8_t mode) { control_mode_ = mode; }
  void set_parent(Magna3 *parent) { parent_ = parent; }
 protected:
  uint8_t control_mode_{0xFF};
  void control(float value) override;
  Magna3 *parent_{};
};
class ControlModeSelect : public select::Select {
 public:
  void set_parent(Magna3 *parent) { parent_ = parent; }
 protected:
  void control(const std::string &value) override;
  Magna3 *parent_{};
};
class PumpRunningSwitch : public switch_::Switch {
 public:
  void set_parent(Magna3 *parent) { parent_ = parent; }
 protected:
  void write_state(bool value) override;
  Magna3 *parent_{};
};
class Magna3 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  void set_mode_select(ControlModeSelect *value) { mode_select_ = value; telemetry_enabled_ = true; }
  void set_running_switch(PumpRunningSwitch *value) { running_switch_ = value; telemetry_enabled_ = true; }
  void set_operating_mode_text(text_sensor::TextSensor *value) { operating_mode_text_ = value; telemetry_enabled_ = true; }
  void write_pump_running(bool value);
  void write_control_mode(const std::string &value);
  void set_number(uint8_t mode, SetpointNumber *number) { numbers_[mode] = number; telemetry_enabled_ = true; }
  void set_request_encryption(bool value) { request_encryption_ = value; }
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void set_telemetry_sensor(size_t index, sensor::Sensor *value) { telemetry_sensors_[index] = value; telemetry_enabled_ = true; }
  void dump_config() override;
  void update() override;
  void loop() override;
  void write_number_setpoint(uint8_t mode, float value);
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t interface,
                           esp_ble_gattc_cb_param_t *param) override;
 protected:
  enum class ModeWrite { IDLE, QUEUED, WAIT_ACK, READBACK };
  ModeWrite mode_write_{ModeWrite::IDLE};
  int requested_mode_{-1};
  bool operating_write_{false};
  PumpRunningSwitch *running_switch_{};
  text_sensor::TextSensor *operating_mode_text_{};
  const char *requested_mode_name_() const;
  void publish_operating_mode_(uint8_t raw);
  ControlModeSelect *mode_select_{};
  void process_mode_write_();
  void publish_control_mode_(uint8_t raw);
  void poll_telemetry_();
  std::array<sensor::Sensor *, 12> telemetry_sensors_{};
  bool telemetry_enabled_{false}, telemetry_pending_{false};
  uint32_t last_telemetry_poll_{0};
  bool send_(uint8_t *data, size_t size);
  void receive_(const uint8_t *data, size_t size);
  void reset_();
  void reconnect_(const char *reason);
  std::array<SetpointNumber *, 13> numbers_{};
  enum class SetpointWrite { IDLE, WAIT_NORMAL, READY, WAIT_READBACK };
  SetpointWrite setpoint_write_{SetpointWrite::IDLE};
  uint8_t pending_setpoint_mode_{0xFF}, operating_mode_{0xFF};
  float pending_setpoint_value_{0};
  uint16_t pending_setpoint_raw_{0};
  void cancel_setpoint_(const char *reason);
  void process_setpoint_();
  void write_setpoint_(float value);
  void clear_setpoints_();
  protocol::SetpointContext setpoint_context_;
  std::vector<esp32_ble_tracker::ESPBTUUID> services_;
  std::vector<uint8_t> rx_;
  uint16_t handle_{0}, cccd_{0};
  bool ready_{false}, pending_{false}, awaiting_get_{false};
  bool request_encryption_{false}, authenticating_{false};
  uint32_t received_at_{0};
  uint8_t failures_{0};
};
}  // namespace esphome::grundfos_magna3
