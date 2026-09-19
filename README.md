# Custom speakers

Two generations of a DIY stereo speaker system.

| | v1 (2024) | **v2 (2026, current)** |
|---|---|---|
| Cabinets | 5.25″ glass-fibre woofer + 1″ silk dome, 8 L ported | 6.5″ long-throw woofer + 2″ tweeter, ~18.5 L sealed (converted from a 3-way box) |
| Crossover | passive 3rd-order Butterworth, hand-wound coils | **active DSP** on an ESP32-S3, LR4 + parametric EQ + per-driver delay |
| Amplification | one TPA3255 stereo, 36 V | **4× TPA3118 mono, bi-amped**, 24 V — one amp per driver |
| Source | USB DAC → analogue preamp | USB-C audio class (24-bit, 48/96 kHz) straight into the DSP |
| Control | knobs on a tone preamp | BLE companion app (Windows) with EQ import |
| Speaker cable | 2-wire | **4-wire NL4** per cabinet (woofer pair + tweeter pair) |

v2 splits into two physical units, and this repo mirrors that:

- **[`speaker/`](speaker/)** — the cabinets. No crossover inside: each box is
  two drivers on an NL4 socket. Contains the enclosure conversion plan
  (3-way → 2-way) and the driver data.
- **[`amp/`](amp/)** — the DSP/amp unit: ESP32-S3 crossover firmware, two
  PCM5102 DACs, four TPA3118 amps, 150 W AC-DC supply, in its own enclosure.
  Firmware, Windows companion app, BLE protocol, wiring and DSP design docs.
  Imported from the former `speaker-dsp-crossover` repository with its history.
- **[`v1-passive-bookshelf/`](v1-passive-bookshelf/)** — the 2024 build,
  unchanged (also tagged `v1`).

## Tags

| Tag | Meaning |
|---|---|
| `v1` | The passive bookshelf build as published, before this restructure |
| `amp-v1-verified-2026-08-23` | The DSP/amp firmware + app as first verified on hardware (BLE control, NVS, presets — before USB audio) |

## Status (2026-09-20)

- **Amp unit:** full chain USB → DSP → DACs → amps **heard by ear 2026-09-16**;
  host volume applied; app reconnects across board reboots. Amp mute
  transistors designed but not yet wired. See [`amp/TASKS.md`](amp/TASKS.md).
- **Cabinets:** currently the original 3-way boxes running as a 2-way (4″ mid
  unpowered). Conversion cut plan is dimensioned and ready; **nothing cut yet**.
  See [`speaker/ENCLOSURE.md`](speaker/ENCLOSURE.md).
