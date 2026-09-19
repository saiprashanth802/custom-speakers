# Electronics Documentation

## System Signal Chain

```
Bluetooth Module (48kHz/24-bit)
        │
        ▼
  Audio Splitter (1 → 3 per channel)
        │
   ┌────┼────┐
   ▼    ▼    ▼
 Vol  Vol  Vol   ← 10kΩ linear potentiometers (back panel)
 Pot  Pot  Pot
   │    │    │
   ▼    ▼    ▼
Class-D Class-D Class-D   ← one amp module per driver
  Amp   Amp   Amp
   │    │    │
   ▼    ▼    ▼
Passive Crossover Network
   │    │    │
   ▼    ▼    ▼
Woofer  Mid  Tweeter
(6.5") (4") (1" dome)
```

---

## Bluetooth Module

| Parameter | Value |
|---|---|
| Sample rate | 48 kHz |
| Bit depth | 24-bit |
| Codec | Standard BT (SBC) — not LDAC or aptX HD |
| Output | Stereo analog line-level |

> Note: The module was initially assumed to support hi-res codecs. After physical inspection and chip identification, it was confirmed as a standard BT SBC module. Documentation was corrected accordingly. Always identify chips physically before writing specs.

---

## Amplifier Modules

- Type: Class D
- Configuration: 3 modules per cabinet (tri-amplified)
- One channel used per module (mono per driver)
- Input: line-level from potentiometer wiper
- Output: to passive crossover input

---

## Passive Crossover Network

### Component List (per cabinet)

| Component | Value | Purpose |
|---|---|---|
| Inductor L1 | (woofer LPF) | Toroidal, hand-wound |
| Inductor L2 | (mid BPF) | Toroidal, hand-wound |
| Capacitor C1 | (tweeter HPF) | Electrolytic |
| Capacitor C2 | (mid HPF pole) | Electrolytic |
| Padding R | (tweeter attenuation) | Wire-wound resistor |

### Filter Slopes

- 3rd-order Butterworth: −18 dB/octave slopes
- Crossover points: (to be measured and documented after room tuning)

---

## Grounding

Star ground topology:

```
Woofer amp GND ──────┐
Mid amp GND ─────────┼──── Single ground point ──── AC Earth
Tweeter amp GND ─────┘
BT module GND ───────┘
```

No daisy-chain grounding. All returns go directly to the star point to prevent ground loops between amp channels.

---

## Planned Additions

### AUX Input (L/R Line-Level)
- 3.5mm TRS or RCA stereo input on back panel
- Line-level buffer stage before the audio splitter
- Switch or auto-detect between BT and AUX sources

### ESP32-Based Onboard DSP
- ESP32 running biquad filter chain (CMSIS-DSP or custom)
- I2S output to a DAC module feeding the amp inputs
- 11-band parametric EQ matching the `tight-bass.peace` preset baked in
- Source-independent: tuning applied regardless of input (BT, AUX, or future inputs)
- Front-panel preset switching via rotary encoder
