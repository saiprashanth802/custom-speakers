# 🔊 DIY Audio Engineering — Decisions & Achievements

> A living document of every design decision, engineering tradeoff, and milestone across all audio projects. Built from scratch in a hostel, with a 3D printer and a clear head.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Project 1 — Custom 3-Way Active Studio Monitors](#project-1--custom-3-way-active-studio-monitors)
- [Project 2 — Portable 2.0 Hostel Speaker](#project-2--portable-20-hostel-speaker)
- [EQ Tuning — Peace Equalizer](#eq-tuning--peace-equalizer)
- [Planned Upgrades](#planned-upgrades)
- [Key Learnings & Pitfalls Avoided](#key-learnings--pitfalls-avoided)
- [Tools & Resources](#tools--resources)

---

## Project Overview

| Project | Status | Type | Power |
|---|---|---|---|
| 3-Way Active Studio Monitors | ✅ Completed | 3-way, tri-amplified | AC mains |
| Portable 2.0 Hostel Speaker | 🔧 In Design | 2-way, 3D-printed enclosure | AC plug-in |
| ESP32 Onboard DSP | 📋 Planned | Parametric EQ module | — |
| AUX Input Addition | 📋 Planned | L/R line-level input | — |

---

## Project 1 — Custom 3-Way Active Studio Monitors

### What Was Built

Two self-contained, 3-way active monitor cabinets built entirely from scratch. Each cabinet houses three drivers, three amplifier channels, a passive crossover network, and its own AC power supply — all in a custom wood enclosure with veneer finish.

### Driver Configuration (per cabinet)

| Driver | Model | Role | Power Handling |
|---|---|---|---|
| Woofer | Toyotone TT-660 6.5" | Low frequency | 60W |
| Mid-woofer | 4" driver | Midrange | — |
| Tweeter | 1" dome | High frequency | — |

### Amplification — Tri-Amplified Class D

The decision to tri-amplify (one amp channel per driver) rather than use a single stereo amp with passive crossover only was driven by control. With three separate Class-D modules, each driver's volume can be independently adjusted via a front-mounted potentiometer. This allows on-the-fly balance correction between woofer, mid, and tweeter — something a single passive-only design can't do.

**Why Class D:** Efficiency is critical in a mains-powered but thermally constrained wood enclosure. Class D runs cool enough that no heatsink venting was needed, which simplified the cabinet design.

### Passive Crossover

A passive crossover network was added **downstream** of the amplifier channels to handle high-frequency rolloff and driver protection. The network uses:

- **Toroidal inductors** — hand-wound to calculated target inductances. Toroidal geometry was chosen over air-core for lower leakage flux (less interference with nearby components) and more compact winding.
- **Electrolytic capacitors** — for crossover filter poles.

The passive crossover supplements the independent volume controls to shape the frequency handoff between drivers. Third-order Butterworth filter topology was applied for the crossover slopes.

**Sensitivity matching:** Padding resistors were calculated and added to equalize the sensitivity between drivers. Without this, the higher-sensitivity tweeter would dominate the midrange and woofer, producing a harsh, unbalanced frequency response even with all volumes at maximum.

### Enclosure

- Custom wood enclosure with veneer finish
- Back panel: 2-pin AC socket, illuminated rocker switch, 4-terminal binding posts
- Front panel: three per-driver volume potentiometers on a custom wood bracket
- Internal layout designed around amplifier module placement and driver wiring runs

### Connectivity

- **Current:** Bluetooth-only, 48kHz / 24-bit (standard BT module — not LDAC or aptX HD)
- **Planned:** AUX L/R line-level input, ESP32-based onboard DSP

> **Accuracy note:** Initial documentation assumed the Bluetooth module supported hi-res codecs (LDAC/aptX HD). After physical inspection and chip identification, it was confirmed to be a standard BT module. This was corrected before the GitHub repository was finalized. Accurate component identification matters before documentation or spec sheets are written.

### GitHub Repository

The complete build is documented in a GitHub repository including:

- `README.md` — project overview and specs
- `eq/tight-bass.peace` — the 11-band Peace EQ preset
- `eq/README.md` — filter-by-filter acoustic engineering rationale
- `docs/build.md` — physical construction log
- `docs/electronics.md` — amplifier wiring and crossover schematic notes
- `bill_of_materials.csv` — full component list with sourcing
- `images/` — build photo documentation
- `schematics/` — schematic folder (in progress)

---

## Project 2 — Portable 2.0 Hostel Speaker

### Design Goals

A compact, portable speaker for hostel use — plug-in powered, strong bass, no separate subwoofer. Target budget: ₹3,000–6,000.

### Driver Selection — Dayton DC160-8 6.5"

Multiple drivers were evaluated before settling on the Dayton DC160-8. A FABTEC 6.5" driver sourced from Amazon India was rejected after evaluation:

| Issue | Detail |
|---|---|
| Lower frequency limit | 250 Hz — far too high for a woofer |
| Impedance | 4Ω — incompatible with planned amp |
| Power rating | Misleading peak rating, not RMS |
| Thiele-Small parameters | Not published — enclosure design impossible |

The Dayton DC160-8 was selected because its published Thiele-Small parameters allow proper enclosure simulation, and it has a realistic low-frequency extension suitable for a bass reflex cabinet.

**This decision reflects a key principle:** Never commit to a driver without published T/S parameters. Budget Indian marketplace listings routinely omit or falsify specs. Critical evaluation before purchase avoids expensive mistakes.

### Enclosure Design — Bass Reflex, Tuned

Using the DC160-8's T/S parameters:

| Parameter | Value |
|---|---|
| Internal volume | 7.2 L |
| Port tuning frequency | 42 Hz |
| Port pipe | 50mm PVC |
| Port length | 8.5 cm |
| External dimensions (approx.) | 22 × 28 × 18 cm |

The ported (bass reflex) design was chosen over sealed because the DC160-8's T/S parameters favour it — it yields deeper bass extension with less power and avoids the steep rolloff below resonance that a sealed box would produce.

### Enclosure Construction — 3D Printed PETG

Access to a 3D printer (22×22cm bed, PETG filament) changed the design approach entirely. Rather than MDF woodworking, the enclosure is designed as a four-section printed assembly:

| Section | Description |
|---|---|
| Front baffle | Driver cutout, bolt holes for M3 heat-set inserts |
| Top shell half | Main cabinet volume, internal ribs every 8cm |
| Bottom shell half | Integrated port tube, AC entry |
| Rear cap | Terminal cup, amp board mount |

**Why four sections:** The 22×22cm print bed cannot print a full speaker cabinet in one piece. Each section is designed to fit within the bed constraints while the assembled result is a single rigid, sealed enclosure.

**Print settings:**
- Wall perimeters: 4 (for rigidity and acoustic damping)
- Infill: 40–50%, gyroid pattern (isotropic stiffness)
- Fasteners: M3 heat-set inserts (thread strength in PETG)
- Sealing: TPU gasket between all mating surfaces
- Estimated assembled weight per cabinet: ~1 kg

**Why PETG over PLA:** PETG has better impact resistance and thermal stability — important for an enclosure near an AC power supply and Class-D amp that generates some heat.

**Why 3D printing over wood:** Rounded rectangle profiles and bolt-and-gasket assembly are impractical to cut from MDF in a hostel without power tools. Printing enables custom geometries that would otherwise require a full woodworking shop.

### Status

CAD modeling in Fusion 360 / FreeCAD — in progress. Exact section dimensions prepared and ready for modeling.

---

## EQ Tuning — Peace Equalizer

### Overview

Peace Equalizer (frontend for Equalizer APO) is used for source-side parametric EQ tuning. This applies on the PC/source side, before the signal reaches the Bluetooth module or AUX input.

### tight-bass.peace — 11-Band Preset (Studio Monitors)

| Band | Frequency | Type | Gain | Q | Purpose |
|---|---|---|---|---|---|
| 1 | 26 Hz | High-pass | — | — | Woofer excursion protection — cuts content the driver can't reproduce cleanly |
| 2 | 53 Hz | Low shelf | cut | — | Tightens bass bloom — the woofer's natural resonance peak was making bass sound bloated |
| 3 | 84 Hz | Bell | +8.5 dB | — | Primary bass body — main boost for kick and bass guitar presence |
| 4 | 294 Hz | Bell | cut | — | Surgical low-mid cut — removes boxiness from male vocals and guitar body |
| 5 | 1119 Hz | Bell | cut | — | Nasal/honky midrange correction |
| 6 | 2465 Hz | Bell | cut | — | Upper midrange harshness reduction |
| 7 | 3676 Hz | Bell | boost | — | Presence boost — adds definition to vocals and acoustic instruments |
| 8 | 6488 Hz | Bell | cut | — | Sibilance control — reduces harshness on "s" and "sh" sounds |
| 9 | 10332 Hz | High shelf | +2.5 dB | — | Air and sparkle — extends perceived high-frequency detail |
| PreAmp | — | Gain | −5 dB | — | Headroom — prevents clipping when multiple bands boost simultaneously |

**Stereo settings:** Stereo Expanding +3.5, Stereo Shift −2.047 (widens stereo image for near-field listening)

### Key EQ Principle Learned

When applying bass boosts in Peace EQ, the preamp must be reduced by at least −6 dB or the output clips at the DAC stage before the signal even reaches the amplifier. A +8.5 dB bass boost with 0 dB preamp will distort even at moderate volume. The −5 dB preamp in this preset provides just enough headroom for the boost stack without clipping.

### IEM Tuning — General Approach

For IEMs (in-ear monitors), a separate tuning approach was developed:

- Bass boost: bell filter at ~60 Hz + low shelf at 120–150 Hz
- Treble cut: bell cuts at 5–6 kHz (sibilance) and 8–10 kHz (harshness), high shelf cut at 8 kHz for broad reduction
- Preamp: −6 dB minimum when boosting bass

The optimal values vary by IEM model. A flat-tuned IEM (e.g. 7Hz Salnotes Zero) needs more bass boost than a V-shaped IEM (e.g. KZ sets) that already emphasizes bass.

---

## Planned Upgrades

### AUX Input (L/R Line-Level)

Add a 3.5mm or RCA stereo input to the studio monitors so they can accept wired sources directly, bypassing the Bluetooth module. This makes the monitors usable with a laptop, phone, or audio interface without any wireless link. Planned as a straightforward line-level buffer input on the back panel.

### ESP32-Based Onboard DSP

The most significant planned upgrade. An ESP32 running a parametric EQ firmware would process the audio signal inside the monitor itself — independent of any source-side software like Peace EQ.

**Why this matters:**
- Currently, EQ is source-dependent: if you play from a phone via Bluetooth, Peace EQ on the PC doesn't apply
- An onboard DSP means the tuning is always active regardless of source
- Removes the dependency on a Windows PC running Equalizer APO
- Enables future upgrades: room correction, dynamic EQ, presets switchable from the front panel

**Implementation approach:** ESP32 with I2S output, DSP library (CMSIS-DSP or custom biquad filter chain), baked-in filter coefficients matching the tight-bass.peace preset as a starting point.

---

## Key Learnings & Pitfalls Avoided

### Component Identification First

Assumptions about component capabilities (e.g. Bluetooth codec support) must be verified by actual chip identification before documentation is written. A wrong spec in a README is worse than a missing one.

### T/S Parameters Are Non-Negotiable

Never select a woofer without published Thiele-Small parameters. Without Vas, Qts, and Fs, enclosure volume and port tuning cannot be calculated. Budget drivers on Amazon India routinely omit these — treat that as a disqualification.

### Peak Power Ratings Are Meaningless

Indian marketplace listings (and many budget drivers globally) advertise peak power ratings that are 2–4× the RMS rating. Always look for RMS continuous power handling. Peak ratings describe a brief transient the driver survives, not what it can sustain in actual use.

### Preamp Headroom in EQ

Any EQ preset that boosts frequencies must reduce the preamp by at least the maximum single-band boost, or clipping occurs before the amplifier. This is not optional — it is physics.

### 3D Print Bed Constraints Drive Design

A 22×22cm bed means any enclosure larger than that must be sectioned. This is a design input, not a workaround — the four-section approach was designed around the bed size from the start.

### Sensitivity Matching Is as Important as Crossover Design

A crossover that passes the right frequencies to each driver still sounds wrong if the tweeter is 6 dB more sensitive than the woofer. Padding resistors to equalize driver sensitivity must be part of any multi-way design from the start.

---

## Tools & Resources

### Software

| Tool | Use |
|---|---|
| Peace Equalizer (Equalizer APO) | Source-side parametric EQ |
| Fusion 360 / FreeCAD | Enclosure CAD modeling |
| GitHub | Build documentation |

### Hardware

| Tool | Detail |
|---|---|
| 3D Printer | 22×22cm bed, PETG filament |
| Soldering station | Hand-winding inductors, perfboard work |
| Multimeter | DCR measurement for hand-wound coils |

### Component Sourcing (India)

| Retailer | What to buy |
|---|---|
| DIYAudioCart | Drivers, crossover components |
| Robu.in | Amplifier modules, passive components |
| Evelta | Capacitors, inductors, connectors |

### Reference

- Dayton Audio DC160-8 datasheet (T/S parameters)
- 3rd-order Butterworth crossover design tables
- Thiele-Small enclosure alignment charts (vented/sealed)

---

*Last updated: June 2026 — 2nd year B.Tech ECE, Hyderabad*
