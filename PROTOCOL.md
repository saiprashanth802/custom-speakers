# BLE control protocol — v1 (FROZEN 2026-08-23)

The app↔firmware contract. Mirrors `firmware/dsp_engine/dsp_params.h` (`Params`)
and `biquad.h` (`FilterType`). Canonical opcode values live in
`firmware/dsp_engine/protocol.h`; the C# app mirrors them in `Protocol/Opcodes.cs`.
**Bump `PROTOCOL_VERSION` on any breaking change** and gate with EVT_HELLO.

## GATT

Custom service, base UUID `9F3E7Axx-5C2B-4D8E-9A1F-6B0C1D2E3F40`:

| Role | UUID | Properties |
|------|------|-----------|
| Service | `9F3E7A00-5C2B-4D8E-9A1F-6B0C1D2E3F40` | — |
| **CMD** (app→device) | `9F3E7A01-5C2B-4D8E-9A1F-6B0C1D2E3F40` | Write / WriteNoResponse |
| **EVT** (device→app) | `9F3E7A02-5C2B-4D8E-9A1F-6B0C1D2E3F40` | Notify |

## Framing

- **CMD:** `[opcode:u8][payload…]`
- **EVT:** `[event:u8][payload…]`
- Multi-byte integers **little-endian**. Floats **IEEE-754 LE, 4 bytes**.
- `driver` index: 0=L-woofer, 1=R-woofer, 2=L-tweeter, 3=R-tweeter.
- `FilterType`: 0=PK, 1=LOWSHELF, 2=HIGHSHELF, 3=LOWPASS, 4=HIGHPASS.
- `profile`: 0=Normal (USB, 48 kHz), 1=HighRes (WiFi, 96 kHz).

Every SET rebuilds the affected coefficients and applies via the double-buffered
handoff (see INTERFACE.md) — no explicit APPLY needed.

## CMD opcodes (app → device)

| Op | Name | Payload |
|----|------|---------|
| `0x01` | HELLO | — (device replies EVT_HELLO) |
| `0x02` | GET_STATUS | — (device replies EVT_STATUS) |
| `0x10` | SET_MASTER_GAIN | f32 linear (0..1) |
| `0x11` | SET_MUTE | u8 (0=unmute, 1=mute) |
| `0x12` | SET_PROFILE | u8 profile |
| `0x13` | SET_CROSSOVER_HZ | f32 |
| `0x14` | SET_VOICING_PREAMP_DB | f32 |
| `0x20` | SET_VOICING_BAND | u8 idx, u8 enabled, u8 type, f32 f, f32 Q, f32 gainDb |
| `0x21` | CLEAR_VOICING | — (disable all voicing bands; used before an import) |
| `0x30` | SET_DRIVER_LEVEL | u8 driver, f32 dB |
| `0x31` | SET_DRIVER_DELAY | u8 driver, u16 samples |
| `0x32` | SET_DRIVER_EQ_BAND | u8 driver, u8 idx, u8 enabled, u8 type, f32 f, f32 Q, f32 gainDb |
| `0x40` | SAVE_PRESET | u8 slot |
| `0x41` | LOAD_PRESET | u8 slot |
| `0x42` | SAVE_TO_NVS | — (persist current params as boot default) |

**EQ import (Peace / AutoEQ / Equalizer APO):** app parses the text, then sends
`CLEAR_VOICING`, `SET_VOICING_PREAMP_DB`, and one `SET_VOICING_BAND` per parsed
filter (PK/LS/HS). Bands beyond MAX are dropped with a UI warning.

## EVT events (device → app)

| Ev | Name | Payload |
|----|------|---------|
| `0x81` | EVT_HELLO | u16 fwVersion, u16 protocolVersion, u8 maxVoicingBands, u8 maxDriverBands, u8 numDrivers |
| `0x82` | EVT_STATUS | u8 profile, u8 muted, u32 sampleRate, u8 linkFlags |
| `0x83` | EVT_ACK | u8 echoedOpcode, u8 result (0=ok, else error) |

`linkFlags` bit0 = WiFi up, bit1 = USB host present (reserved until those sources
are built).

## Versioning rule

On connect the app sends HELLO and checks `protocolVersion == PROTOCOL_VERSION`.
Mismatch → app shows "firmware/app protocol mismatch, update one side" and stays
read-only. Never silently write against a different version.

## v1 + additive read-back (2026-09-15) — COMPILED ONLY, not yet exercised on hardware

`PROTOCOL_VERSION` stays **1**: nothing above changed. `FW_VERSION` → 2. An old app
never sends the new opcode; an old firmware answers it with `EVT_ACK result 0xFF`,
which the new app treats as "no read-back, push instead" — so every pairing of old
and new still works.

| Op | Name | Payload |
|----|------|---------|
| `0x03` | GET_PARAMS | — (device ACKs, then streams the events below from its main loop) |

| Ev | Name | Payload |
|----|------|---------|
| `0x84` | EVT_PARAM | `u8 setOpcode` followed by **that SET opcode's exact CMD payload** |
| `0x85` | EVT_PARAMS_DONE | `u8 count` — number of EVT_PARAM frames the device sent |

Reusing the SET layouts means the app decodes each EVT_PARAM with the same offsets it
encodes with. Largest frame is `0x84 0x32 …` = 18 bytes, inside the 20-byte notify
limit at the default 23-byte MTU. Emission order (39 frames): `0x12` profile, `0x11`
mute, `0x10` master, `0x13` crossover, `0x14` preamp, `0x20` ×10 voicing bands, then
per driver `0x30` level, `0x31` delay (samples), `0x32` ×4 EQ bands. Profile goes first
so the app knows the rate before it converts delay samples to ms. 3 ms gap per frame.

**Connect sequence is now:** subscribe → app sends HELLO → EVT_HELLO → app sends
GET_PARAMS → 39× EVT_PARAM → EVT_PARAMS_DONE. The app applies these silently (no echo)
and only starts sending SETs on user edits. `PushAll` survives as the fallback for the
NAK / 3 s-timeout case.

**Also new in fw 2 (no protocol change):** a second EVT_STATUS is pushed after the
audio task finishes a profile switch, so `sampleRate` in it is the rate actually
running; the immediate reply to SET_PROFILE still reports the old rate. And the saved
`sampleRate` in the NVS blob is now honoured at boot (boot-profile restore) and
LOAD_PRESET re-designs for the running rate instead of the saved one.
