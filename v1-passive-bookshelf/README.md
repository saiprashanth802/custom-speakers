# 🔊 DIY 2-Way Bookshelf Speaker

> Complete stereo pair build — designed, wound, printed, and assembled from scratch.  
> Hyderabad, India · 2024

---

## Overview

A fully custom 2-way bookshelf speaker system built from individual components — no kit, no prefab crossover. Each cabinet houses a 5.25" glass fiber woofer and a 1" silk dome tweeter, crossed over with a hand-calculated 3rd-order Butterworth passive network using polypropylene film capacitors and hand-wound air-core inductors. Driven by a TPA3255 Class-D amplifier at 36V (~63W/ch into 8Ω).

| Stat | Value |
|---|---|
| Configuration | 2-way, passive crossover |
| Cabinets | 2 (stereo pair) |
| Net internal volume | 8.0 L per cabinet |
| External dimensions | 175 × 255 × 175 mm |
| Amplifier | TPA3255 Class D, 300W×2 @ 36V |
| Source | ddHiFi CX31993 USB-C DAC (384kHz/32-bit, 105dB SNR) |
| Total build cost | ₹11,320 |
| Commercial equivalent | ₹22,000–28,000 |

---

## Drivers

### Woofer — DS-52G060 (5.25")

| Spec | Value |
|---|---|
| Diameter | 5.25" |
| Impedance | 8Ω (confirm — NOT 4Ω version) |
| Power handling | 60W RMS |
| Cone | Glass fiber |
| Magnet | Double magnet |
| Source | Doogesound |
| Qty | 2 (one per cabinet) |

### Tweeter — DE15-04 (1" silk dome)

| Spec | Value |
|---|---|
| Diameter | 1" |
| Impedance | 8Ω |
| Power handling | 40W |
| Diaphragm | European silk dome |
| Source | Doogesound |
| Qty | 2 (one per cabinet) |
| Sensitivity pad | 4.7Ω series resistor (4dB attenuation) |

> **Tweeter polarity — critical:** Wire the tweeter in **reverse polarity** (swap + and −). This is required for the 3rd-order Butterworth topology — the 270° phase shift makes reverse polarity correct.

---

## Crossover

### Topology: 3rd-Order Butterworth, Passive

Hand-calculated crossover using polypropylene film capacitors and hand-wound air-core inductors. Polypropylene film was chosen over electrolytic for lower ESR, no polarization requirement, and better long-term stability.

### Capacitors (all MKPA-S Polypropylene Film, 250VDC, ±3%)

| Ref | Value | Role | Qty |
|---|---|---|---|
| C1, C3 | 6.8µF | Tweeter high-pass | 4 (×2 per cabinet) |
| C2 | 10µF ∥ 3.3µF | Woofer shunt | 2+2 per cabinet |

> C2 is formed by paralleling a 10µF and 3.3µF cap to hit the required 13.3µF target value — more accurate than a single non-standard value cap.

### Inductors — Hand-Wound Air-Core

| Ref | Value | Wire | Former | Winding | Qty |
|---|---|---|---|---|---|
| L1, L3 | 0.42mH | 0.5mm enamel | 25mm PETG bobbin | 3 layers × 75 turns | 4 total |
| L2 | 0.85mH | 22SWG enamel | 35mm PETG bobbin | 4 layers × 46 turns | 2 total |

PETG bobbins are 3D-printed (custom designed). Wire sourced from Gujarati Gali.

### Sensitivity Pad

The DE15-04 tweeter has higher sensitivity than the DS-52G060 woofer. A 4.7Ω 10W non-inductive wirewound resistor in series attenuates the tweeter by ~4dB to match the woofer's output level.

---

## Amplifier System

### TPA3255 Class-D Amp Board

| Spec | Value |
|---|---|
| Chip | Texas Instruments TPA3255 (PurePath) |
| Output | 300W × 2 |
| THD | 0.006% |
| Operating voltage | 36V (from SMPS) → ~63W/ch into 8Ω |
| Source | Maker Bazar |

> **Do NOT run at 48V.** At 48V the TPA3255 delivers ~115W into 8Ω — well above the DS-52G060's 60W RMS rating. Use the 36V SMPS only.

### 36V 10A SMPS (360W)

Metal case, EMI filtered, overcurrent protected. Powers the TPA3255 only. A buck converter steps this down to 12V for the preamp board.

### XH-A901 Tone Preamp

NE5532 dual op-amp. Provides bass, treble, and volume controls upstream of the amplifier. Powered via a 36V→12V buck converter included in the Maker Bazar cart.

### Source — ddHiFi CX31993 USB-C DAC

| Spec | Value |
|---|---|
| Chip | CX31993 |
| SNR | 105dB |
| Max resolution | 384kHz / 32-bit |
| Status | Already owned |

Signal chain: USB-C DAC → RCA → XH-A901 preamp → TPA3255 → crossover → drivers.

---

## Cabinet

### Enclosure Design

| Parameter | Value |
|---|---|
| Material | 12mm Gurjan BWR plywood |
| Net internal volume | 8.0 L per cabinet |
| External dimensions | 175 × 255 × 175 mm |
| Port type | Slot port (PETG 3D-printed) |
| Port dimensions | 80 × 20mm slot, 93mm deep, 3mm walls |
| Port position | Right side panel only |

### 3D-Printed Parts (PETG)

| Part | Qty | Detail |
|---|---|---|
| Slot port duct | 2 | 60% gyroid infill, sealed with silicone |
| L1/L3 inductor bobbin (25mm) | 4 | For 0.42mH coils |
| L2 inductor bobbin (35mm) | 2 | For 0.85mH coils |

### Acoustic Treatment

- ~0.5m² polyfill per cabinet, lining rear and side walls
- **Do NOT stuff the port area** — polyfill blocking the port defeats bass reflex tuning
- All internal seams sealed with PVA glue + silicone bead — airtight is critical for port tuning accuracy

---

## Bill of Materials

| # | Component | Spec | Source | Qty | Unit (₹) | Total (₹) |
|---|---|---|---|---|---|---|
| 1 | DS-52G060 woofer | 5.25" 8Ω 60W, glass fiber, double magnet | Doogesound | 2 | 1800 | 3600 |
| 2 | DE15-04 tweeter | 1" silk dome, 8Ω, 40W | Doogesound | 2 | 700 | 1400 |
| 3 | MKPA-S 6.8µF cap | Polypropylene, 250VDC, ±3% | Doogesound | 4 | 79 | 316 |
| 4 | MKPA-S 10µF cap | Polypropylene, 250VDC, ±3% | Doogesound | 2 | 119 | 238 |
| 5 | MKPA-S 3.3µF cap | Polypropylene, 250VDC, ±3% | Doogesound | 2 | 59 | 118 |
| 6 | 4.7Ω 10W resistor | Non-inductive wirewound, 5% | Doogesound | 2 | 49 | 98 |
| 7 | L1/L3 inductors (0.42mH) | Air-core, 0.5mm wire, 25mm PETG former | Wind yourself | 4 | — | — |
| 8 | L2 inductor (0.85mH) | Air-core, 22SWG wire, 35mm PETG former | Wind yourself | 2 | — | — |
| 9 | TPA3255 amp board | 300W×2 Class D, 0.006% THD | Maker Bazar | 1 | 2500 | 2500 |
| 10 | 36V 10A SMPS | 360W, EMI filtered, OCP | Maker Bazar | 1 | 1600 | 1600 |
| 11 | XH-A901 preamp | NE5532, bass/treble/volume | Maker Bazar | 1 | — | — |
| 12 | Buck converter 36V→12V | Powers XH-A901 | Maker Bazar | 1 | — | — |
| 13 | 12mm Gurjan BWR ply | Per cabinet: 2×front/rear, 2×top/bot, 2×sides | Local timber | 2 | 175 | 350 |
| 14 | Port duct (PETG) | 80×20mm slot, 93mm deep | 3D print | 2 | 30 | 60 |
| 15 | Inductor bobbins (PETG) | 25mm + 35mm formers | 3D print | 6 | 5 | 30 |
| 16 | Acoustic polyfill | ~0.5m² per cabinet | Local / Amazon | 1 | 100 | 100 |
| 17 | Speaker binding posts | Banana/spade, 2 per cabinet | Gujarati Gali | 4 | 20 | 80 |
| 18 | Wood screws + PVA + silicone | Cabinet assembly and sealing | Local hardware | 1 | 120 | 120 |
| 19 | 22SWG enamel copper wire | L2 winding, 50m spool | Gujarati Gali | 2 | 250 | 500 |
| 20 | 0.5mm enamel wire | L1/L3 winding | Already owned | — | — | — |
| 21 | 16AWG speaker wire | Internal wiring, ~2m per cabinet | Local market | 1 | 80 | 80 |
| 22 | RCA cable | 3.5mm to dual RCA, shielded, 1m | Amazon India | 1 | 80 | 80 |
| 23 | Perfboard | Crossover board, one per cabinet | Gujarati Gali | 2 | 25 | 50 |
| 24 | ddHiFi CX31993 DAC | 105dB SNR, 384kHz/32-bit | Already owned | — | — | — |
| | | | | | **TOTAL** | **₹11,320** |

---

## Critical Build Notes

| | Note | Detail |
|---|---|---|
| ⚠ | Tweeter polarity | Wire tweeter in **reverse polarity** (swap + and −). Required for 3rd-order Butterworth — 270° phase shift. |
| ⚠ | Power supply voltage | Run TPA3255 at **36V only**. 48V → ~115W into 60W drivers. Use included SMPS, not a higher-voltage supply. |
| ⚠ | Woofer impedance | Source the **8Ω version** of DS-52G060. The 4Ω variant changes crossover values entirely. |
| ✓ | Port clearance | Side port needs minimum 80mm wall clearance. Port on right side panel only. |
| ✓ | Inductor winding | L1/L3: 0.5mm wire, 25mm former, 3 layers × 75 turns. L2: 22SWG, 35mm former, 4 layers × 46 turns. |
| ✓ | Cabinet sealing | Seal ALL internal joints with PVA + silicone bead. Any air leak detunes the port and kills bass extension. |
| ✓ | First test | Wire everything before permanently sealing cabinet. Play 30 min, check coils for heat, verify both drivers working. |
| ✓ | Listening setup | Minimum 80cm distance, ideal 1–1.5m. Toe-in 10–15° toward listening position. Tweeter at ear level. |
| ✓ | Polyfill placement | Line rear and side walls only. Do not obstruct or stuff near the port opening. |

---

## Key Design Decisions

**Why 3rd-order Butterworth crossover?**  
Steeper slopes than 2nd-order (−18dB/octave vs −12dB/octave) give better driver protection and cleaner frequency handoff. The trade-off is the 270° phase shift at crossover, which requires reverse tweeter polarity — handled in wiring.

**Why polypropylene film caps over electrolytic?**  
Lower ESR, no polarization, more stable over temperature and time. Worth the cost premium for crossover components that run for thousands of hours.

**Why parallel caps for C2 (10µF ∥ 3.3µF)?**  
The required 13.3µF isn't a standard value. Paralleling two standard values is more accurate and cheaper than sourcing a custom value.

**Why hand-wound inductors?**  
Off-the-shelf air-core inductors at 0.42mH and 0.85mH were unavailable locally at reasonable cost. Winding to exact calculated values also allows DCR matching between stereo channels for identical frequency response left and right.

**Why PETG for printed parts?**  
Better thermal stability and impact resistance than PLA — important near the SMPS and amp board. Port duct integrity is critical for bass tuning; PETG holds geometry better under heat.

**Why 36V for the TPA3255?**  
The DS-52G060 is rated 60W RMS. At 36V the TPA3255 delivers ~63W/ch into 8Ω — right at the driver's limit. At 48V it would deliver ~115W, risking thermal and mechanical driver failure on transients.

---

*Build by Sai Prashanth · B.Tech ECE · Hyderabad, India · 2024*
