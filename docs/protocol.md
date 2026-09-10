# Protocol reference for MAGNA3 25-80 180

This is a concise implementation reference, not an official Grundfos specification.

- BLE characteristic: `859cffd1-036e-432a-aa28-1a0085b87ba9`.
- Write Without Response and notifications, reassembled by GENI frame length.
- GENI CRC: poly 0x1021, init 0xFFFF, xorout 0xFFFF, excluding SD.
- Control Mode: read class 02 / 70, write class 03.
- Operating Mode: read class 02 / 55, decode `raw & 0x03`; write class 03.
- Setpoint: class 0C / CA; conversion depends on confirmed Control Mode.
- No SET to class 02 / 70 or 55 is emitted.

Head scaling is empirical for the tested model. Flow Raw scaling, upper bits of
55 and Control Source Raw semantics remain unresolved. Constant Flow setpoint
conversion is separate from flow telemetry.

See [README](../README.md) for behaviour and limitations, and
[third-party notices](../THIRD_PARTY_NOTICES.md) for the protocol reference attribution.
