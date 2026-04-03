# 🎛️ EQ Preset — tight-bass.peace

Custom Peace Equalizer preset tuned specifically for this 3-way sealed wood enclosure speaker system.

## How to Load

1. Install [Equalizer APO](https://sourceforge.net/projects/equalizerapo/)
2. Install [Peace GUI](https://sourceforge.net/projects/peace-equalizer-apo-extension/)
3. Open Peace → Import → select `tight-bass.peace`
4. Apply to your output device

---

## Global Settings

| Parameter | Value | Purpose |
|-----------|-------|---------|
| PreAmp | -5 dB | Prevents clipping — needed because bass boost adds +8.5 dB |
| Stereo Expanding | +3.5 | Widens stereo image beyond physical speaker distance |
| Stereo Shift | -2.047 ms | Fine corrects imaging balance between L/R |

---

## Filter Breakdown

| Band | Frequency | Gain | Q | Filter Type | Purpose |
|------|-----------|------|---|-------------|---------|
| 1 | 26 Hz | -6 dB | 1.77 | High-pass (type 2) | Rolls off infrasonic content — protects woofer excursion below usable range |
| 2 | 53 Hz | -1.4 dB | 0.70 | Low-shelf (type 16) | Gentle low shelf — tightens up low bass bloom |
| 3 | 360 Hz | 0 dB | 0.25 | Peaking | Neutral — no correction needed in this region |
| 4 | 39 Hz | -4 dB | 2.29 | Peaking | Reduces sealed-box resonance hump at ~39 Hz |
| 5 | 294 Hz | -2 dB | 1.18 | Peaking | Pulls back upper-bass muddiness and box coloration |
| 6 | 1119 Hz | -1 dB | 1.98 | Peaking | Slight reduction in lower-midrange forwardness |
| 7 | 2465 Hz | -2.24 dB | 2.56 | Peaking | Tames presence peak — reduces harshness on vocals |
| 8 | 3676 Hz | +1.79 dB | 5.08 | Peaking | Narrow boost — adds definition and transient clarity |
| 9 | 6488 Hz | -1.86 dB | 5.88 | Peaking | Cuts tweeter resonance / sibilance peak |
| 10 | 10332 Hz | +2.5 dB | 0.70 | High-shelf (type 17) | Lifts air and top-end sparkle — restores high-frequency extension |
| 11 | 84 Hz | +8.5 dB | 0.61 | Peaking | Primary bass boost — adds warmth and punch centred at 84 Hz |

---

## Tuning Philosophy

This preset targets a **tight, punchy bass response** with:

- Infrasonic protection below 30 Hz (sealed enclosure, not infinite baffle)
- Bass presence centred at 84 Hz (+8.5 dB) — felt and heard without being boomy
- Tightened low bass via cuts at 39 Hz and 53 Hz
- Clean, flat midrange via surgical cuts at 294 Hz, 1119 Hz, 2465 Hz
- Air and high-frequency extension restored via +2.5 dB shelf at 10 kHz

The -5 dB PreAmp provides sufficient headroom so the +8.5 dB bass boost does not clip the signal chain at loud volumes.

---

## Notes

- Tuned on sealed wood enclosure with Toyotone TT-660 6.5" sub, 4" mid-woofer, 1" dome tweeter
- Bluetooth input (48kHz/24-bit) — tuned for wireless source
- May need slight adjustment for different rooms or listening distances
