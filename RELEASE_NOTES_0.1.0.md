# 0.1.0 release candidate

Initial source-only public release candidate for direct BLE/GENI integration of
Grundfos MAGNA3 with ESPHome and Home Assistant, without CIM.

## Source release

Main project license: MIT, copyright 2026 grandalos. Reference-project notices
remain in THIRD_PARTY_NOTICES.md. Repository: https://github.com/grandalos/grundfos-magna3.
The example tracks main. No prebuilt firmware is distributed.

## Tested scope

Reverse-engineered on a physical **Grundfos MAGNA3 25-80 180** with M5Stack Atom S3
Lite. Compile target: ESPHome 2026.8.2 / ESP-IDF. Other MAGNA3 variants are not
claimed to behave identically.

Features: BLE connection/reconnect, CRC-checked notifications and batch telemetry,
eight Control Modes, Operating Mode readback, Pump Running Normal/Stop switch,
six typed setpoint numbers and readback-based confirmation. AUTOADAPT/FLOWADAPT
have no writable user setpoint. Min/Max setpoint changes wait for Normal ACK and
readback; Stop setpoint writes are rejected without automatically starting the pump.

## Cleanup

Only the obsolete `protocol::to_meters` and `protocol::to_raw` helpers and their
exclusive tests were removed: **6 helper lines + 9 test lines = 15 lines**.
The earlier audit estimated 16; the actual diff is 15 (the next parser comment
is retained). Two protocol comments were clarified, without changing formulas,
CRC, framing, maps, parser, transport or the state machine. Public YAML API is
unchanged from the current six-number implementation.

REVIEW items deliberately retained: tagged read_value, OperatingMode UNKNOWN,
SetpointModeConfig.step, OPERATING_MODES[].raw, ControlMode structure, numeric
constants and telemetry indices.

## Limitations and evidence

Head is an empirical fit for the tested pump. Flow Raw telemetry scaling, upper
bits of 0x55 and Control Source Raw semantics remain unresolved. Constant Flow
Setpoint scaling is distinct from telemetry flow scaling.

The Min/Max→Normal→SET end-to-end sequence has automated host tests; independent
physical acceptance of this full sequence is not claimed for this candidate.
No firmware was uploaded as part of release preparation.

## Validation

- Five C++ suites with -Wall -Wextra -Werror and ASan/UBSan: PASS.
- Production-method host setpoint sequence tests: PASS.
- Existing private device YAML compile: PASS.
- Public example compile: PASS; same example with only the Git
  external source replaced by the local component checkout.
- GitHub Actions workflow prepared; GitHub run status is available in the repository Actions tab.

## Licensing

THIRD_PARTY_NOTICES.md preserves the full MIT notice of esphome-alpha-hwr,
copyright 2026 Emmanuel Levijarvi, at audit commit
87f81c81187a3373edcf22e1602d17f4fb28d95c. It credits a protocol reference and does not
assert authorship of the entire MAGNA3 component or establish complete genealogy.

Source-component distribution and prebuilt ESPHome firmware have different
licensing scopes. No binaries are included; firmware dependency notices and a
full SBOM are not part of this candidate.
