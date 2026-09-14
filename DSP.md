# DSP engine — architecture, EQ, and CPU budget

**Status:** design, nothing built or measured. Started 2026-08-23. All cycle
counts below are **estimates to be verified on hardware** — treat as sizing, not
fact.

Runs on the ESP32-S3, **audio on a dedicated core** (core 1); control/BLE/WiFi
on core 0. See [INTERFACE.md](INTERFACE.md) for the control side and the
High-Res/Normal profile that sets the sample rate this engine runs at.

## Signal chain

```
 USB/WiFi PCM in (stereo)
        │
   ┌────▼────┐  Voicing EQ  (pre-crossover, full-band, stereo)
   │  L / R  │  ← Peace / AutoEQ / Equalizer APO imports land HERE
   └────┬────┘
        │  split each of L,R:
   ┌────▼─────────────────┐  Crossover  (LR4 = 2 biquads LP + 2 HP per side)
   │  L-low  L-high        │
   │  R-low  R-high        │
   └────┬─────────────────┘
        │  per stream:
   ┌────▼────┐  Per-driver EQ + level trim + delay (post-crossover)
   │ 4 driver│
   │ outputs │ → I2S0 (LOW DAC L/R) + I2S1 (HIGH DAC L/R)
   └─────────┘
```

### Two EQ layers — keep them distinct

- **Voicing EQ** — pre-crossover, on full-band L/R. This is the **global tonal
  curve**, and the **only place an imported full-range preset (Peace/AutoEQ)
  belongs** — it must sit *before* the band split to shape the summed acoustic
  output as the preset intends. Stereo-linked by default.
- **Per-driver EQ** — post-crossover, per band/driver. Driver linearization,
  baffle-step, notches, level match. Set by hand in the app, **not** from an
  import. Importing a full-range preset into this stage would be wrong.

## EQ import (Peace / AutoEQ / Equalizer APO)

Config files are plain text:

```
Preamp: -6.0 dB
Filter 1: ON PK  Fc 105 Hz   Gain 5.5 dB  Q 0.70
Filter 2: ON LSC Fc 105 Hz   Gain 3.0 dB  Q 0.70
Filter 3: ON HSC Fc 10000 Hz Gain 2.0 dB  Q 0.70
```

- The **app** parses these and maps each filter to a standard **RBJ biquad**:
  `PK → peaking`, `LS/LSC → low-shelf`, `HS/HSC → high-shelf`; `Preamp → input
  gain`. These + preamp cover ~all AutoEQ output and most Peace presets.
- Parsed result feeds the **Voicing EQ** bands in the `Params` block (see
  INTERFACE.md). Firmware just runs biquads — it never parses text.
- Unknown filter types (LP/HP/AP/etc.) on import: warn + skip, or map the common
  ones later. Start with PK/LS/HS/Preamp.

## Biquad implementation

- **Transposed Direct Form II (TDF2)**, single-precision float.
  - The S3 FPU is **single-precision only** — doubles are software-emulated
    (~10–20× slower); **never use doubles in the audio path**.
  - Naive Direct Form in single precision accumulates noise on **low-frequency
    high-Q** filters (e.g. a 40 Hz Q-3 band from an AutoEQ import). TDF2 is the
    numerically robust single-precision form — no doubles required.
- Coefficient math (RBJ cookbook) can be done app-side or firmware-side;
  coefficients are **sample-rate dependent**, so they are **recomputed on every
  profile switch** (48k ⇄ 96k). See INTERFACE.md profile-switch sequence.

## CPU budget — MEASURED on hardware (2026-08-23)

Measured on the real ESP32-S3 (esp32 Arduino core 3.3.6, TDF2 single-precision,
audio on a dedicated core). Biquads per input sample, **typical** config
(10 voicing + LR4 + 4/driver):

| Stage | Biquads |
|-------|---------|
| Voicing EQ (10 bands × L/R) | 20 |
| Crossover LR4 (LP+HP × L/R) | 8 |
| Per-driver EQ (4 bands × 4 drivers) | 16 |
| **Total** | **44** |

**Measured cost: 42.7 cycles/biquad** (was estimated at 30 — the real figure is
~40% higher because TDF2's serial dependency chain stalls the LX7 float
pipeline). 44-biquad config = **1877 cycles/sample**, and that per-sample cost is
**rate-independent**, so:

| Config | Rate | cyc/sample | % of one 240 MHz core |
|--------|------|-----------|-----------------------|
| Typical (44) | **48 kHz** | 1877 | **37.6 %** ✅ measured, comfortable |
| Typical (44) | **96 kHz** | 1877 | **~75 %** ✅ feasible on dedicated core (arithmetic; confirm real-time) |
| Heavy (~88)  | 48 kHz | ~3750 | ~75 % |
| Heavy (~88)  | 96 kHz | ~3750 | ~150 % ⚠ over one core |

**CRITICAL build flag — `-O3` on the DSP hot path:** the Arduino ESP32 default is
`-Os` (optimize for **size**), which measured **70.9 cyc/biquad = 62% of a core
at 48 kHz** — nearly 2× slower. Adding `#pragma GCC optimize("O3")` to biquad.h
and the engine .ino dropped it to 42.7. **This pragma is mandatory, not
cosmetic.** (fast-math deliberately NOT used — it reassociates and can hurt
biquad numerics.)

**Rules of thumb (measured):**
- **48 kHz (Normal): 37.6%** for the full editor — wide headroom, can roughly
  **double** the biquad count (~88) before ~75%.
- **96 kHz (High-Res): ~75%** for the typical config — **feasible on the
  dedicated core** (this reversed the pre-optimization conclusion). Moderate
  margin; don't also pile on heavy per-driver EQ at 96 kHz.
- Leave margin: the audio core also services I2S DMA and must never miss a
  deadline (a miss = audible click). 75% is the practical ceiling to design to.

Memory is a non-issue: biquad states are tiny; a few-ms time-align delay at 96k
stereo is < 10 KB. PSRAM (if present) is for WiFi buffering, not DSP.

## Open / to verify

- [x] **Measured** real cyc/biquad on the S3: **42.7 with `-O3`** (70.9 at the
      `-Os` default). 48k typical = 37.6%; 96k typical ≈ 75%.
- [ ] Empirically confirm **96 kHz real-time** (dual-I2S streaming keeps up, no
      underrun) — the 75% is arithmetic from the rate-independent per-sample cost.
- [ ] Voicing band count cap (proposed 10; AutoEQ presets are often ≤10 PK).
- [ ] Per-driver EQ band count (proposed 4/driver).
- [ ] Decide coefficient math location (app vs firmware) for imports vs live
      tweaks — live tweaks argue for firmware-side to avoid BLE round-trips.
- [ ] Confirm crossover alignment (LR4 sums flat); consider all-pass phase
      correction only if measurements show a dip at the crossover point.
