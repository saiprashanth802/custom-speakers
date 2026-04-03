# ⚡ Electronics

## Signal Chain

```
Bluetooth RX (48kHz/24-bit)
    └──► Split to 3 amp modules
              ├──► Sub amp   ──► Crossover ──► 6.5" woofer
              ├──► Mid amp   ──► Crossover ──► 4" mid
              └──► HF amp    ──► Crossover ──► 1" tweeter
```

## Components

| Component | Spec | Notes |
|-----------|------|-------|
| Bluetooth module | 48kHz / 24-bit | Wireless input only for now |
| Sub amp | Class-D ~60W | Drives Toyotone TT-660 |
| Mid amp | Class-D ~20W | Drives mid-woofer |
| HF amp | Class-D ~30W | Drives tweeter |
| Passive crossover | PCB with toroidal inductors | Handles band separation |
| Filter caps | 100µF 35V | Power supply smoothing |
| DC-DC converter | Step-down module | Regulated supply for low-power stages |

## Power

- Input: 2-pin AC mains
- Internal conversion to DC for amp modules
- Separate regulated rail for Bluetooth module

## Future: ESP32 DSP

Planned upgrade: ESP32 microcontroller inserted between Bluetooth RX and amp inputs to run onboard parametric EQ / DSP — eliminating dependence on PC-side Peace EQ. Will enable standalone tuning profiles stored on the speaker itself.
