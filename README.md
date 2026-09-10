# ESPHome Grundfos MAGNA3 BLE

Direct Grundfos MAGNA3 control and telemetry over BLE/GENI for ESPHome and Home Assistant, without a CIM module.

## Status

**Version 0.1.0.** Reverse-engineered and tested on a physical **Grundfos MAGNA3 25-80 180**, using an M5Stack Atom S3 Lite (ESP32-S3). Identical behaviour across other MAGNA3 variants is not yet established.

Licensed under MIT. See [release notes](RELEASE_NOTES_0.1.0.md).

## Features

- Direct BLE connection, notification subscription and reconnect handling.
- Control Mode read/write with eight options.
- Operating Mode readback and class-03 commands; Pump Running switch for Normal/Stop.
- Six mode-specific setpoint numbers with fixed units and ranges.
- Telemetry and diagnostic RAW values.
- Readback-based confirmation, without optimistic control state or replay of pending writes after reconnect.

## Supported Control Modes

Constant Pressure, Proportional Pressure, Constant Curve, AUTOADAPT, Constant Temperature, Constant Flow, FLOWADAPT and Differential Temperature.

**AUTOADAPT and FLOWADAPT have no manually adjustable user setpoint.** They do not have writable number entities.

## Operating Modes

| Operating Mode | Pump Running |
| --- | --- |
| Normal | ON |
| Stop | OFF |
| Min | ON |
| Max | ON |

Pump Running means operating versus stopped, not “Normal only”. Switching ON requests Normal; OFF requests Stop. Min/Max are decoded and have internal command mappings, but there is no separate Min/Max control entity in 0.1.0. Readback of Min/Max never itself sends a command.

## Setpoints

| Entity | Unit | Minimum | Maximum | Step |
| --- | --- | ---: | ---: | ---: |
| Constant Pressure Setpoint | m | 1.02 | 7.45 | 0.01 |
| Proportional Pressure Setpoint | m | 1.94 | 7.45 | 0.01 |
| Constant Curve Setpoint | % | 35 | 100 | 1 |
| Constant Temperature Setpoint | °C | -10 | 130 | 1 |
| Constant Flow Setpoint | m³/h | 1.2 | 10.5 | 0.1 |
| Differential Temperature Setpoint | °C | 0 | 140 | 1 |

These ranges and conversions were established on the tested MAGNA3 25-80 180 and require verification for other models. CP/PP/CT/CF/DT map RAW 0..10000 linearly over their ranges. Constant Curve is different: `percent = raw / 100`; 35% writes RAW 3500, not zero. A readback such as RAW 3460 is shown as 34.6%, even though new user writes are restricted to 35..100%.

## Behaviour

- Only the number matching the current confirmed Control Mode can write. Inactive-number commands are rejected locally and never change Control Mode.
- Control Mode changes invalidate the old setpoint context. A fresh 0xCA GET supplies the new value; values are not transferred between units or numbers.
- Normal: send the setpoint, then GET to read it back.
- Min/Max: retain one pending value, request Normal, wait for positive class-03 ACK and a subsequent Normal readback, recheck Control Mode, then send the setpoint. No blocking delay is used.
- The latest request for the same number replaces the pending value while waiting for Normal. A Control Mode change, failed confirmation or disconnect cancels pending work.
- Stop: reject setpoint writes; changing a number never automatically starts a stopped pump.
- Unknown operating/control state, non-finite inputs and busy transport cause local rejection. Retry once the component is ready.
- Pump readback is the source of truth. Only the active number receives a physical readback; a different value than requested is reported as the actual value, not false confirmation.
- `publish_state()` does not invoke the write callback.

The complete Min/Max → Normal → setpoint sequence has automated host coverage. It is not claimed to have completed a separate end-to-end physical acceptance test for this release candidate.

## Telemetry

The existing batch GET runs every 5 seconds and exposes:

- Head (m), Liquid Temperature (°C).
- Power (W), Motor Current (A), Motor Speed (rpm).
- Energy Consumption (kWh, total increasing), Operating Hours (h), Number of Starts.

Diagnostics: **Head Raw, Flow Raw, Control Source Raw, Control Mode Raw**. Operating Mode is a separate text sensor. The full operating-status byte is retained in DEBUG logs.

**Constant Flow Setpoint is not Flow telemetry.** The setpoint conversion to m³/h is established; the physical scaling of Flow Raw telemetry is still unresolved. There is no physical Flow sensor in this version.

## Installation

Use ESPHome **2026.8.2**, the version used for compile verification, with an ESP32 BLE-capable board and ESP-IDF. The public example targets Atom S3 Lite.

Use the component directly from GitHub:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/grandalos/grundfos-magna3
      ref: main
    components: [grundfos_magna3]
```

There is no dependency on ALPHA HWR at runtime. The example tracks `main`, like the Hayward integration.

## Example configuration

[examples/grundfos-magna3.yaml](examples/grundfos-magna3.yaml) is a complete configuration: board, Wi-Fi placeholders, API, OTA, BLE client, component, all six numbers, sensors, select, switch, text sensor and Web Server v3 groups. It has no private package includes or local device identifiers.

Replace the Wi-Fi credentials and pump address. Configure API encryption and OTA authentication for your deployment. Names and IDs are standard ESPHome YAML options. For example:

```yaml
ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: magna3_ble
    auto_connect: true

grundfos_magna3:
  id: magna3
  ble_client_id: magna3_ble
  request_encryption: true
  update_interval: 30s

number:
  - platform: grundfos_magna3
    grundfos_magna3_id: magna3
    control_mode: constant_flow
    id: magna3_constant_flow_setpoint
    name: Constant Flow Setpoint
```

`control_mode` accepts `constant_pressure`, `proportional_pressure`, `constant_curve`, `constant_temperature`, `constant_flow`, `differential_temperature`. The schema supplies units, bounds and steps and rejects conflicting units. The old single `Setpoint` number is not a second write path.

The BLE Connection switch releases the pump for Grundfos GO: turn it OFF, connect the app, then disconnect the app before turning it ON. The tested pump accepts one BLE connection at a time. This switch represents client enablement, not confirmed connection state.

## Known limitations

- Tested on one model; no all-model support claim.
- **Empirical Head conversion:** fitted to measured values on MAGNA3 25-80 180, not established as universal protocol scaling.
- **Unknown:** Flow Raw physical scaling, the upper bits of 0x55, and the semantic meaning of Control Source Raw 0x5A. No interpretation is guessed.
- All six numbers remain visible. Inactive writes are rejected by the component, not hidden dynamically by the UI.
- External changes become visible on polling. Busy writes are rejected, not placed in an unbounded queue.
- Radio coverage and BLE connection exclusivity affect availability. Diagnostic telemetry can retain its last reading during failures; it is not proof of a live connection.
- Only source distribution is planned. No firmware binaries, private captures or device secrets are included.

## Protocol notes

Confirmed on the tested pump: GENI framing/CRC, fragmented BLE notification reassembly, Control Mode and Operating Mode commands/readback, 0xCA and six setpoint conversions. Characteristic: `859cffd1-036e-432a-aa28-1a0085b87ba9`, Write Without Response plus notifications.

Requests start with 0x27; responses with 0x24. GENIBus CRC uses polynomial 0x1021, init/xorout 0xFFFF and covers LEN through payload, excluding SD and CRC. Class 0x0C / ID 0xCA is the setpoint. Control Mode and Operating Mode writes use class 0x03.

Class 0x02 / 0x70 provides Control Mode readback. Class 0x02 / 0x55 provides operating status; `raw_55 & 0x03` maps to Normal/Stop/Min/Max. SET to 0x70 or 0x55 was rejected by the tested pump and is not emitted here. This is not a claim that all GENI class-02 commands are read-only.

Current batch IDs: `18 19 1E 22 23 24 25 27 3A 5A 98 99 0B 0C 70 55`. Requests/responses are 24 bytes; outgoing BLE writes are split into 20+4 bytes. Complete length, addresses, class/count and CRC are validated before publication.

## Development and verification

```sh
bash scripts/test_host.sh
python3 -m pip install esphome==2026.8.2
python3 scripts/prepare_compile_example.py
esphome compile .ci/grundfos-magna3.yaml
```

The compile script substitutes only the public example's Git external source with the current checkout. It does not require a network checkout or a real pump. Host tests cover CRC/frames, telemetry, all control mappings, operating-status masking, conversions, routing and pending-sequence cancellation. GitHub Actions runs these checks without hardware.

`python3 scripts/prepare_release.py` creates an allowlisted source tree under `release/grundfos-magna3-0.1.0`. The source bundle excludes build artifacts and device secrets.

## Credits / Protocol reference

Protocol/framing implementation was developed with reference to [eman/esphome-alpha-hwr](https://github.com/eman/esphome-alpha-hwr). The audit compared commit `87f81c81187a3373edcf22e1602d17f4fb28d95c`; the original historical base commit is not established. No identical whole source files were found, which does not establish complete independent provenance.

Upstream copyright: **2026 Emmanuel Levijarvi**. This credits the reference project, not authorship of the entire MAGNA3 component. Its MIT notice is preserved in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

The MAGNA3 component is licensed under the [MIT License](LICENSE), copyright 2026 grandalos. Upstream attribution and permission terms are preserved separately in THIRD_PARTY_NOTICES.md.

Source-component releases and prebuilt firmware distributions have different licensing scopes. ESPHome runtime licensing and ESP-IDF/dependency notices must also be considered before distributing binaries. This candidate contains source only; a full firmware SBOM is outside this release preparation.
