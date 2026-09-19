# Speaker cabinets (v2)

Passive-free cabinets: two drivers each, no crossover inside, one **NL4**
socket on the back. Crossover, EQ, delay and amplification all live in the
[amp unit](../amp/).

## Drivers

| Position | Driver | Known | Unknown |
|---|---|---|---|
| Woofer | 6.5″ long-throw subwoofer, **8 Ω, 60 W RMS**, ~70 mm magnet, ~1.5 cm excursion | cutout ⌀145, frame ⌀170 | model number, T/S parameters |
| Tweeter | 2″, 8 Ω | cutout ⌀45, frame ⌀73 | model number |
| (removed) | 4″ mid | cutout ⌀95 | — |

The woofer is a sub driver with a modest motor: high excursion, probably
high-ish Qts. With the TPA3118 voltage-limited to ~25–30 W into 8 Ω, **amp
voltage, not cone excursion, is the loudness ceiling** — see the port
discussion in `ENCLOSURE.md`.

## Cabinet

| | Now (3-way box) | After conversion |
|---|---|---|
| External W × H × D | 230 × 500 × 280 | 230 × **380** × 280 |
| Stock | 10 mm | — |
| Internal volume | 26.2 L gross | 19.7 L gross, ≈18.5 L net |
| Loading | sealed | sealed; port decision after T/S measurement |
| Tweeter–woofer centre spacing | 270 mm | **150 mm** |

The conversion removes a 120 mm band containing the 4″ hole (cuts at 230 and
350 from the bottom edge through all four walls) and rejoins the box with
internal splice strips. Full plan, measurements and decision log:
**[`ENCLOSURE.md`](ENCLOSURE.md)**.

## NL4 wiring convention

Standard bi-amp assignment, same at both ends of the cable:

| NL4 pin | Signal |
|---|---|
| 1+ / 1− | Woofer (LOW band amp) |
| 2+ / 2− | Tweeter (HIGH band amp) |

TPA3118 outputs are bridge-tied: neither leg is ground, so the NL4 shell and
the cabinet carry no ground — the cable is four floating conductors. Keep
polarity consistent; the DSP's per-driver delay and the LR4 alignment assume
both drivers wired in phase.

The rear panel needs a **round NL4 chassis socket cutout** (⌀ ~24 mm for the
common NL4MP, four M3 screws) — add it to the bottom section's rear panel,
below the seam, when the cabinet is opened for the conversion.

## After the rebuild

Re-measure with REW and redo the tweeter delay and the voicing EQ — the
passive-radiator bass from the unpowered 4″ disappears, the volume drops and
the baffle shortens. Listed in [`../amp/TASKS.md`](../amp/TASKS.md).
