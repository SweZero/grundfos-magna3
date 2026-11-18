#include "grundfos_magna3.h"
#include "protocol.h"
#include "telemetry.h"
#include "control_mode.h"
#include "operating_mode.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome::grundfos_magna3 {
static const char *const TAG = "grundfos_magna3";
void PumpRunningSwitch::write_state(bool value) { parent_->write_pump_running(value); }
void ControlModeSelect::control(const std::string &value) { parent_->write_control_mode(value); }
void SetpointNumber::control(float value) { parent_->write_number_setpoint(control_mode_, value); }
void Magna3::dump_config() {
  ESP_LOGCONFIG(TAG, "Grundfos MAGNA3: class 0x0C / parameter 0xCA, mode-dependent setpoint");
  LOG_UPDATE_INTERVAL(this);
}
void Magna3::clear_setpoints_() {
  for (auto *number : numbers_) if (number) number->publish_state(NAN);
}
void Magna3::cancel_setpoint_(const char *reason) {
  if (setpoint_write_ != SetpointWrite::IDLE) ESP_LOGW(TAG, "Pending setpoint cancelled: %s", reason);
  setpoint_write_ = SetpointWrite::IDLE;
}
void Magna3::reset_() {
  cancel_setpoint_("BLE disconnect/reset");
  operating_mode_ = 0xFF;
  if (mode_write_ != ModeWrite::IDLE) ESP_LOGW(TAG, "Control mode transaction cancelled by disconnect/reset");
  mode_write_ = ModeWrite::IDLE;
  requested_mode_ = -1;
  cancel_timeout("mode_readback");
  operating_write_ = false;
  setpoint_context_.invalidate();
  ready_ = pending_ = authenticating_ = awaiting_get_ = false;
  telemetry_pending_ = false;
  last_telemetry_poll_ = millis();
  cancel_timeout("telemetry_timeout");
  cancel_timeout("telemetry_tail");
  handle_ = cccd_ = 0;
  failures_ = 0;
  rx_.clear();
  services_.clear();
  cancel_timeout("read_timeout");
  cancel_timeout("readback");
  cancel_timeout("subscribe");
  clear_setpoints_();
}
void Magna3::reconnect_(const char *reason) {
  ESP_LOGW(TAG, "%s; reconnecting", reason);
  reset_();
  status_set_warning();
  parent()->disconnect();
}
bool Magna3::send_(uint8_t *data, size_t size) {
  if (!ready_) return false;
  ESP_LOGD(TAG, "TX %s", format_hex_pretty(data, size).c_str());
  const auto status = esp_ble_gattc_write_char(parent()->get_gattc_if(), parent()->get_conn_id(),
      handle_, size, data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "BLE write failed: %d", status);
    reconnect_("Write failed");
    return false;
  }
  return true;
}
void Magna3::update() {
  if (!ready_ || pending_ || telemetry_pending_ || (mode_write_ == ModeWrite::WAIT_ACK || mode_write_ == ModeWrite::READBACK)) return;
  if (protocol::mode_index(setpoint_context_.mode) < 0) return;
  setpoint_context_.begin_read();
  auto frame = protocol::get();
  if (!send_(frame.data(), frame.size())) return;
  pending_ = awaiting_get_ = true;
  set_timeout("read_timeout", 5000, [this]() {
    pending_ = awaiting_get_ = false;
    status_set_warning();
    cancel_setpoint_("0xCA readback timeout");
    clear_setpoints_();
    ESP_LOGW(TAG, "No valid 0xCA reply in 5 seconds (%u/3)", ++failures_);
    if (failures_ >= 3) reconnect_("Repeated GET timeout");
  });
}
void Magna3::write_number_setpoint(uint8_t mode, float value) {
  const int requested = protocol::mode_index(mode);
  const int active = protocol::mode_index(setpoint_context_.mode);
  if (requested < 0 || active < 0 || mode != setpoint_context_.mode) {
    ESP_LOGW(TAG, "Setpoint write rejected: %s setpoint is inactive while control mode is %s",
             requested < 0 ? "Unknown" : protocol::CONTROL_MODES[requested].name,
             active < 0 ? "Unknown" : protocol::CONTROL_MODES[active].name);
    return;
  }
  const auto cfg = protocol::setpoint_config(mode);
  if (!cfg.writable) {
    ESP_LOGW(TAG, "Setpoint write rejected: %s controls setpoint automatically", protocol::CONTROL_MODES[active].name);
    return;
  }
  if (!std::isfinite(value)) { ESP_LOGW(TAG, "Setpoint must be finite"); return; }
  if (operating_mode_ == 0xFF) { ESP_LOGW(TAG, "Setpoint write rejected: operating mode is not known yet"); return; }
  if (operating_mode_ == 1) { ESP_LOGW(TAG, "Setpoint write rejected: pump is stopped"); return; }
  value = std::clamp(value, cfg.min_value, cfg.max_value);
  if (setpoint_write_ == SetpointWrite::WAIT_NORMAL || setpoint_write_ == SetpointWrite::READY) {
    if (pending_setpoint_mode_ == mode) {
      pending_setpoint_value_ = value;
      ESP_LOGD(TAG, "Pending setpoint updated: %.3f %s", value, cfg.unit);
    }
    return;
  }
  if (!ready_ || pending_ || telemetry_pending_ || mode_write_ != ModeWrite::IDLE ||
      setpoint_write_ != SetpointWrite::IDLE || !setpoint_context_.fresh) {
    ESP_LOGW(TAG, "Setpoint rejected: busy or fresh readback required"); return;
  }
  ESP_LOGD(TAG, "Setpoint request: mode=%s value=%.3f %s", protocol::CONTROL_MODES[active].name, value, cfg.unit);
  pending_setpoint_mode_ = mode;
  pending_setpoint_value_ = value;
  if (operating_mode_ == 2 || operating_mode_ == 3) {
    ESP_LOGD(TAG, "Pump operating mode is %s; requesting Normal first", operating_mode_ == 2 ? "Min" : "Max");
    setpoint_write_ = SetpointWrite::WAIT_NORMAL;
    write_pump_running(true);
  } else {
    write_setpoint_(value);
  }
}
void Magna3::process_setpoint_() {
  if (setpoint_write_ != SetpointWrite::READY) return;
  if (pending_setpoint_mode_ != setpoint_context_.mode || operating_mode_ != 0) {
    cancel_setpoint_("control mode changed or operating mode is no longer Normal"); return;
  }
  if (pending_ || telemetry_pending_ || mode_write_ != ModeWrite::IDLE) return;
  if (!ready_ || !setpoint_context_.fresh) { cancel_setpoint_("fresh readback required"); return; }
  write_setpoint_(pending_setpoint_value_);
}
void Magna3::write_setpoint_(float value) {
  const uint8_t mode = setpoint_context_.mode;
  const int index = protocol::mode_index(mode);
  if (index < 0) {
    ESP_LOGW(TAG, "Cannot set setpoint: unknown control mode 0x%02X", mode);
    return;
  }
  const auto cfg = protocol::setpoint_config(mode);
  if (!cfg.writable) {
    ESP_LOGW(TAG, "Setpoint write rejected: %s controls setpoint automatically", protocol::CONTROL_MODES[index].name);
    return;
  }
  uint16_t raw;
  if (!protocol::setpoint_to_raw(mode, value, raw)) {
    ESP_LOGW(TAG, "Setpoint must be finite");
    return;
  }
  if (!ready_ || pending_ || telemetry_pending_ || mode_write_ != ModeWrite::IDLE || !setpoint_context_.fresh) {
    ESP_LOGW(TAG, "SET rejected: disconnected, transaction pending or fresh setpoint readback required");
    return;
  }
  if (mode != pending_setpoint_mode_ || operating_mode_ != 0) {
    cancel_setpoint_("mode changed before SET"); return;
  }
  auto frame = protocol::set(raw);
  if (!send_(frame.data(), frame.size())) return;
  ESP_LOGD(TAG, "Setpoint %s: %.3f %s -> raw=%u", protocol::CONTROL_MODES[index].name,
           std::clamp(value, cfg.min_value, cfg.max_value), cfg.unit, raw);
  pending_setpoint_raw_ = raw;
  setpoint_write_ = SetpointWrite::WAIT_READBACK;
  pending_ = true;
  set_timeout("readback", 500, [this]() { pending_ = false; update(); });
}
void Magna3::write_control_mode(const std::string &value) {
  const int index = protocol::mode_index(value.c_str());
  if (index < 0) { ESP_LOGW(TAG, "Invalid control mode option: %s", value.c_str()); return; }
  if (!ready_ || mode_write_ != ModeWrite::IDLE) {
    ESP_LOGW(TAG, "Control mode rejected: disconnected or previous mode transaction pending");
    return;
  }
  cancel_setpoint_("Control Mode command");
  operating_write_ = false;
  requested_mode_ = index;
  mode_write_ = ModeWrite::QUEUED;
}
void Magna3::write_pump_running(bool value) {
  if (!ready_ || mode_write_ != ModeWrite::IDLE) {
    ESP_LOGW(TAG, "Operating mode rejected: disconnected or previous mode transaction pending");
    return;
  }
  if (setpoint_write_ != SetpointWrite::WAIT_NORMAL) cancel_setpoint_("Operating Mode command");
  operating_write_ = true;
  requested_mode_ = value ? 0 : 1;  // Only Normal/Stop are writable from the switch.
  mode_write_ = ModeWrite::QUEUED;
}
const char *Magna3::requested_mode_name_() const {
  return operating_write_ ? protocol::OPERATING_MODES[requested_mode_].name
                          : protocol::CONTROL_MODES[requested_mode_].name;
}
void Magna3::process_mode_write_() {
  if (mode_write_ != ModeWrite::QUEUED || !ready_ || pending_ || telemetry_pending_) return;
  const auto &mode = operating_write_ ? protocol::OPERATING_MODES[requested_mode_]
                                      : protocol::CONTROL_MODES[requested_mode_];
  if (operating_write_) {
    ESP_LOGD(TAG, "Setting operating mode to %s", mode.name);
    ESP_LOGD(TAG, "TX operating mode class03 value=0x%02X", mode.command);
  } else {
    ESP_LOGD(TAG, "Setting control mode to %s", mode.name);
    ESP_LOGD(TAG, "TX class03 control mode command: value=0x%02X", mode.command);
  }
  if (!operating_write_) {
    setpoint_context_.invalidate();
    clear_setpoints_();
  }
  mode_write_ = ModeWrite::WAIT_ACK;
  auto frame = protocol::mode_command(mode);
  if (!send_(frame.data(), frame.size())) return;
  // Only class 03 writes the mode. Positive ACK enables confirmation by the existing GET.
  set_timeout("mode_readback", 12000, [this]() {
    if (mode_write_ == ModeWrite::WAIT_ACK || mode_write_ == ModeWrite::READBACK) {
      ESP_LOGW(TAG, "%s mode change not confirmed: %s (%s)",
               operating_write_ ? "Operating" : "Control", requested_mode_name_(),
               mode_write_ == ModeWrite::WAIT_ACK ? "missing class03 ACK" : "missing matching readback");
      cancel_setpoint_("Normal ACK/readback timeout");
      mode_write_ = ModeWrite::IDLE;
      requested_mode_ = -1;
    }
  });
}

void Magna3::publish_control_mode_(uint8_t raw) {
  ESP_LOGD(TAG, "Control mode readback raw=0x%02X", raw);
  if (setpoint_context_.mode != raw) {
    cancel_setpoint_("Control Mode changed in telemetry");
    setpoint_context_.observe(raw);
    clear_setpoints_();
  }
  const int index = protocol::mode_index(raw);
  if (index < 0) { ESP_LOGD(TAG, "Unknown control mode raw: 0x%02X", raw); return; }
  // publish_state does not call control(), so telemetry cannot create a write loop.
  if (mode_select_) mode_select_->publish_state(protocol::CONTROL_MODES[index].name);
  if (!operating_write_ && mode_write_ == ModeWrite::READBACK && index == requested_mode_) {
    ESP_LOGD(TAG, "Control mode confirmed: %s", protocol::CONTROL_MODES[index].name);
    cancel_timeout("mode_readback");
    mode_write_ = ModeWrite::IDLE;
    requested_mode_ = -1;
  }
}
void Magna3::publish_operating_mode_(uint8_t raw) {
  const auto mode = protocol::operating_mode(raw);
  operating_mode_ = static_cast<uint8_t>(mode);
  const char *name = protocol::operating_mode_name(mode);
  ESP_LOGD(TAG, "Operating mode raw=0x%02X decoded=%s", raw, name);
  if (operating_mode_text_) operating_mode_text_->publish_state(name);
  bool running;
  if (!protocol::operating_running(mode, running)) {
    ESP_LOGD(TAG, "Unknown operating mode raw: 0x%02X", raw);
    return;
  }
  // Publishing state never invokes write_state: Min/Max readback cannot send Normal.
  if (running_switch_) running_switch_->publish_state(running);
  ESP_LOGD(TAG, "Operating mode: %s", name);
  if (operating_write_ && mode_write_ == ModeWrite::READBACK && static_cast<uint8_t>(mode) == requested_mode_) {
    ESP_LOGD(TAG, "Operating mode confirmed: %s (raw=0x%02X)", name, raw);
    if (setpoint_write_ == SetpointWrite::WAIT_NORMAL && operating_mode_ == 0)
      setpoint_write_ = SetpointWrite::READY;
    cancel_timeout("mode_readback");
    mode_write_ = ModeWrite::IDLE;
    requested_mode_ = -1;
  }
}
void Magna3::poll_telemetry_() {
  if (!telemetry_enabled_ || !ready_ || pending_ || telemetry_pending_ ||
      mode_write_ == ModeWrite::QUEUED || mode_write_ == ModeWrite::WAIT_ACK) return;
  const uint32_t now = millis();
  if (now - last_telemetry_poll_ < 5000) return;
  last_telemetry_poll_ = now;
  auto frame = protocol::telemetry_get();
  // GENI is a byte stream. Default ATT MTU 23 allows 20 bytes per write.
  // Keep the single bulk request, split into two paced BLE writes.
  if (!send_(frame.data(), 20)) return;
  telemetry_pending_ = true;
  set_timeout("telemetry_tail", 20, [this, frame]() mutable {
    if (ready_ && telemetry_pending_) send_(frame.data() + 20, frame.size() - 20);
  });
  set_timeout("telemetry_timeout", 5000, [this]() {
    telemetry_pending_ = false;
    ESP_LOGW(TAG, "No valid class 0x02 telemetry reply in 5 seconds");
  });
}
void Magna3::loop() {
  process_setpoint_();
  process_mode_write_();
  if (setpoint_context_.needs_read && mode_write_ == ModeWrite::IDLE) update();
  poll_telemetry_();
  if (!rx_.empty() && millis() - received_at_ > 2000) {
    ESP_LOGW(TAG, "Dropping incomplete GENI frame");
    rx_.clear();
  }
}
void Magna3::receive_(const uint8_t *data, size_t size) {
  ESP_LOGD(TAG, "RX %s", format_hex_pretty(data, size).c_str());
  received_at_ = millis();
  // Feed bytes individually so large notifications cannot grow the buffer unbounded.
  for (size_t i = 0; i < size; i++) {
    if (rx_.empty() && data[i] != 0x24 && data[i] != 0x27) continue;
    rx_.push_back(data[i]);
    while (rx_.size() >= 2) {
      if ((rx_[0] != 0x24 && rx_[0] != 0x27) || rx_[1] < 4) {
        rx_.erase(rx_.begin());
        continue;
      }
      const size_t length = size_t(rx_[1]) + 4;
      if (rx_.size() < length) break;
      ESP_LOGD(TAG, "RX frame %s", format_hex_pretty(rx_.data(), length).c_str());
      const auto expected = (uint16_t(rx_[length - 2]) << 8) | rx_[length - 1];
      if (protocol::crc(rx_.data() + 1, length - 3) != expected) {
        ESP_LOGW(TAG, "Invalid GENI CRC");
        rx_.erase(rx_.begin());
        continue;
      }
      const char *mode_ack = protocol::mode_ack(rx_.data(), length);
      protocol::Telemetry telemetry;
      uint16_t raw;
      if (telemetry_pending_ && protocol::read_telemetry(rx_.data(), length, telemetry)) {
        telemetry_pending_ = false;
        cancel_timeout("telemetry_timeout");
        publish_control_mode_(static_cast<uint8_t>(telemetry.values[11]));
        publish_operating_mode_(telemetry.operating_mode_raw);
        ESP_LOGD(TAG, "MAGNA3 head raw=%.0f -> %.1f m", telemetry.values[4], telemetry.values[9]);
        ESP_LOGD(TAG, "MAGNA3 starts=%.0f", telemetry.values[10]);
        for (size_t index = 0; index < telemetry_sensors_.size(); index++)
          if (telemetry_sensors_[index]) telemetry_sensors_[index]->publish_state(telemetry.values[index]);
      } else if (awaiting_get_ && protocol::read_value(rx_.data(), length, raw, awaiting_get_)) {
        pending_ = awaiting_get_ = false;
        failures_ = 0;
        cancel_timeout("read_timeout");
        status_clear_warning();
        float value;
        const uint8_t mode = setpoint_context_.mode;
        if (setpoint_context_.accept() && protocol::setpoint_from_raw(mode, raw, value)) {
          const auto cfg = protocol::setpoint_config(mode);
          ESP_LOGD(TAG, "Setpoint readback: mode=%s raw=0x%04X value=%.3f %s",
                   protocol::CONTROL_MODES[protocol::mode_index(mode)].name, raw, value, cfg.unit);
          // publish_state never invokes control(): only the matching number is updated.
          if (mode < numbers_.size() && numbers_[mode]) numbers_[mode]->publish_state(value);
          if (setpoint_write_ == SetpointWrite::WAIT_READBACK) {
            if (mode == pending_setpoint_mode_ && raw == pending_setpoint_raw_)
              ESP_LOGD(TAG, "Setpoint confirmed: mode=%s raw=%u value=%.3f %s",
                       protocol::CONTROL_MODES[protocol::mode_index(mode)].name, raw, value, cfg.unit);
            else ESP_LOGW(TAG, "Setpoint readback differs from request; publishing actual value");
            setpoint_write_ = SetpointWrite::IDLE;
          }
        } else {
          ESP_LOGD(TAG, "Setpoint raw=0x%04X not interpreted: automatic/unknown/changed mode 0x%02X", raw, mode);
        }
      } else if (rx_[0] == 0x24 && rx_[2] == 0xF8 && rx_[3] == 0xF7 && rx_[4] == 0x03 &&
                 (mode_write_ == ModeWrite::WAIT_ACK || mode_write_ == ModeWrite::READBACK)) {
        if (mode_ack && rx_[5] == 0) {
          if (operating_write_) ESP_LOGD(TAG, "RX operating mode class03 ACK: success");
          else ESP_LOGD(TAG, "RX class03 ACK: success");
          mode_write_ = ModeWrite::READBACK;
        } else {
          ESP_LOGW(TAG, "RX class03 ACK: unexpected response %s (%s)",
                   format_hex_pretty(rx_.data(), length).c_str(), mode_ack ? mode_ack : "invalid ACK format");
          cancel_setpoint_("unexpected class03 ACK");
          ESP_LOGW(TAG, "%s mode change not confirmed", operating_write_ ? "Operating" : "Control");
          cancel_timeout("mode_readback");
          mode_write_ = ModeWrite::IDLE;
          requested_mode_ = -1;
        }
      } else if (length == 8 && rx_[0] == 0x24 && rx_[2] == 0xF8 && rx_[3] == 0xF7 &&
                 rx_[4] == 0x0C && rx_[5] == 0 && pending_ && !awaiting_get_) {
        ESP_LOGD(TAG, "SET acknowledged; awaiting GET readback");
      } else {
        ESP_LOGW(TAG, "Unrecognized reply/ACK (not published): %s",
                 format_hex_pretty(rx_.data(), length).c_str());
      }
      rx_.erase(rx_.begin(), rx_.begin() + length);
    }
  }
}
void Magna3::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT ||
      !parent()->check_addr(param->ble_security.auth_cmpl.bd_addr)) return;
  const auto &auth = param->ble_security.auth_cmpl;
  ESP_LOGI(TAG, "BLE authentication: success=%d reason=0x%02X", auth.success, auth.fail_reason);
  if (!authenticating_) return;
  authenticating_ = false;
  if (!auth.success) { reconnect_("BLE authentication failed"); return; }
  if (handle_ && parent()->register_for_notify(handle_) != ESP_OK)
    reconnect_("Notification registration after authentication failed");
}
void Magna3::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t interface,
                                esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) break;
      reset_();
      ESP_LOGI(TAG, "Connected; discovering GENI characteristic");
      if (request_encryption_) {
        uint8_t auth = ESP_LE_AUTH_REQ_SC_BOND;
        const auto config_status = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, sizeof(auth));
        if (config_status != ESP_OK) { reconnect_("BLE security configuration failed"); break; }
        authenticating_ = true;
        const auto result = esp_ble_set_encryption(parent()->get_remote_bda(), ESP_BLE_SEC_ENCRYPT);
        ESP_LOGI(TAG, "Requesting encrypted BLE connection, result=%d", result);
        if (result != ESP_OK) { reconnect_("Encryption request failed"); break; }
      }
      set_timeout("subscribe", 15000, [this]() { reconnect_("Notification setup timeout"); });
      break;
    case ESP_GATTC_SEARCH_RES_EVT:
      services_.push_back(esp32_ble_tracker::ESPBTUUID::from_uuid(param->search_res.srvc_id.uuid));
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      const auto uuid = esp32_ble_tracker::ESPBTUUID::from_raw("859cffd1-036e-432a-aa28-1a0085b87ba9");
      for (auto service : services_) {
        auto *chr = parent()->get_characteristic(service, uuid);
        if (!chr) continue;
        if (!(chr->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) ||
            !(chr->properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) continue;
        auto *descriptor = chr->get_descriptor(0x2902);
        if (!descriptor) continue;
        handle_ = chr->handle;
        cccd_ = descriptor->handle;
        break;
      }
      if (!handle_) { reconnect_("GENI characteristic or notification CCCD missing"); break; }
      if (!authenticating_ && parent()->register_for_notify(handle_) != ESP_OK)
        reconnect_("Notification registration failed");
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (!handle_ || param->reg_for_notify.handle != handle_) break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        reconnect_("Notification registration rejected"); break;
      }
      // BLEClientBase writes the CCCD after registration. Wait for its completion.
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
      if (!cccd_ || param->write.handle != cccd_) break;
      if (param->write.status != ESP_GATT_OK) { reconnect_("CCCD rejected"); break; }
      cancel_timeout("subscribe");
      node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      ready_ = true;
      ESP_LOGI(TAG, "Notifications enabled; reading 0xCA");
      update();
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (ready_ && param->notify.conn_id == parent()->get_conn_id() && param->notify.handle == handle_)
        receive_(param->notify.value, param->notify.value_len);
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "Disconnected, reason=0x%02X; BLE client will reconnect", param->disconnect.reason);
      reset_();
      status_set_warning();
      break;
    default: break;
  }
}
}  // namespace esphome::grundfos_magna3
