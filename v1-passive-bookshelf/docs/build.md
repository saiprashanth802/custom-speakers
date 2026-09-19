# Build Documentation

## Enclosure

Custom wood enclosures with veneer finish — one per channel, housing all electronics internally.

### Construction

- Material: MDF with veneer laminate
- Internal bracing: horizontal shelf brace between woofer and mid section
- All seams sealed with silicone caulk before driver installation
- Driver cutouts routed to driver-specific OD, sealed with foam gasket tape

### Back Panel

Each cabinet back panel carries:

| Component | Detail |
|---|---|
| AC socket | 2-pin mains input |
| Rocker switch | Illuminated, switches mains to all internals |
| Binding posts | 4-terminal (allows bi-wiring or single-wire) |
| Volume pots | Three potentiometers on a custom wood bracket — one per driver |

### Driver Layout (front baffle)

```
┌─────────────────┐
│    [ TWEETER ]  │   1" dome — top center
│   [ MID-WFR ]  │   4" mid-woofer — center
│  [  WOOFER  ]  │   6.5" Toyotone TT-660 — bottom
└─────────────────┘
```

---

## Amplification

### Topology: Tri-Amplified (3 channels per cabinet)

Three independent Class-D amplifier modules per cabinet — one per driver. This allows each driver's output level to be independently trimmed via the front-panel potentiometers, enabling manual sensitivity matching between the woofer, mid, and tweeter.

**Why Class D:** High efficiency means minimal heatsinking needed inside a sealed wood enclosure. No forced air cooling required.

### Per-Driver Volume Control

Each amp module's input is fed through a 10kΩ linear potentiometer. The pots are mounted on a custom-cut wood bracket on the back panel. Adjustment is done with a screwdriver or knob — not intended for frequent use, more for initial calibration.

---

## Passive Crossover

A passive crossover network sits downstream of the amplifier modules to handle frequency routing to the correct drivers and protect the tweeter from low-frequency content.

### Topology

- 3rd-order Butterworth filter slopes
- Toroidal inductor geometry (lower leakage flux vs air-core)
- Inductors hand-wound to calculated target inductance values
- Electrolytic capacitors for crossover poles
- Sensitivity matching via padding resistors (tweeter attenuated to match woofer sensitivity)

### Why Hand-Wound Inductors

Off-the-shelf toroidal inductors at the required values were either unavailable locally or overpriced. Winding to a calculated inductance also allows exact DCR matching between left and right channel crossovers, ensuring identical frequency response at both channels.

### Why Padding Resistors

The 1" dome tweeter has a higher sensitivity rating than the 6.5" woofer. Without attenuation, the tweeter dominates the midrange and upper bass even at crossover-correct volume levels. Padding resistors in series/shunt configuration reduce the tweeter's effective level to match the woofer's sensitivity.

---

## Wiring

- Star grounding topology: all ground returns meet at a single point (the AC earth/neutral junction) to prevent ground loops
- Signal wiring: twisted pairs from pot output to amp input to reduce EMI pickup
- Speaker wiring: 18AWG from amp output to crossover, 22AWG from crossover to tweeter

---

## Power

- Mains: 230V AC (India standard)
- Each cabinet is independently powered — no shared PSU between left and right
- Internal SMPS module converts mains to the amp rail voltage
