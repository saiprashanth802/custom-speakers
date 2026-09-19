# Enclosure — 3-way → 2-way conversion

Cabinet woodwork for the v2 active-crossover speakers (see [`../amp/`](../amp/)).
**Nothing cut yet** (as of 2026-09-19). All dimensions in **mm**, measured
**from the bottom edge of the cabinet** — that is the reference the user
measured from and the bottom section is the one that keeps its full geometry.

> Earlier draft of this file (commit 595060d) treated this as a fresh build of
> a three-hole cabinet with a panel cut list and sheet nesting. Wrong premise:
> the cabinets already exist. Superseded by everything below.

## Problem

Two existing **3-way** cabinets, 10 mm stock, **230 × 500 × 280** external,
baffle holes 6.5″ / 4″ / 2″. The electronics are a 2-way (two amp channels
per cabinet). Wanted: a **2-way box with the 6.5″ woofer and 2″ tweeter**, with
the 4″ mid removed — and not merely plugged, because with the mid gone the
tweeter sits ~270 mm above the woofer (centre-to-centre), which at a 2.5–3 kHz
LR4 crossover (λ = 115–137 mm) lobes badly off-axis.

## Plan

**Section the box:** two parallel cuts through all four walls bracketing the
4″ hole, lift out the band, glue the top section back onto the bottom. The
tweeter comes down to the woofer and the box gets shorter.

```
   500 ┌────────┐                    ┌────────┐ 380
       │  (2")  │ 385–430            │  (2")  │ 265–310
   350 ├────────┤ cut B  ─┐          ├────────┤ 230  seam
       │ ( 4" ) │ 235–330 │ removed  │        │
   230 ├────────┤ cut A  ─┘          │ (6.5") │ 65–210
       │        │                    │        │
       │ (6.5") │ 65–210             └────────┘ 0
       │        │
     0 └────────┘
```

## Measured baffle (user, 2026-09-19)

| Feature | From bottom | ⌀ |
|---|---|---|
| Woofer hole | 65 – 210 (centre 137.5) | 145 |
| gap | 210 – 235 | — |
| Mid hole | 235 – 330 (centre 282.5) | 95 |
| gap | 330 – 385 | — |
| Tweeter hole | 385 – 430 (centre 407.5) | 45 |
| top margin | 430 – 500 | — |

Sums to 500. **Frame ODs measured 2026-09-19: woofer 170, tweeter 73.**
Woofer frame spans 52.5–222.5 (12.5 past the hole), 30 mm to each side edge.
After the rebuild the tweeter frame spans 251–324 (14 past the hole), 56 mm
below the new top edge. The cut lines are set by the frames, not the holes.

## Cut lines

| | Cut A | Cut B | Band | New height | Woofer hole → seam | Seam → tweeter hole | C-to-C | Net volume |
|---|---|---|---|---|---|---|---|---|
| **Chosen** | **230** | **350** | **120** | **380** | 20 (frame 7.5) | 35 (frame 21) | **150** | ≈18.5 L |
| Tightest sane | 230 | 365 | 135 | 365 | 20 (frame 7.5) | 20 (frame 6) | 135 | ≈17.7 L |

- Cut A **cannot go above 235** (the mid hole's lower edge) or an arc of the
  hole survives on the bottom section. 230 leaves 5 mm of solid wood under the
  hole so the blade is not running into a void.
- Cut B has room up to ~375 (tweeter frame). 350 was chosen over 365: the last
  15 mm of centre spacing buys almost nothing (150 → 135 at λ≈137) and leaves
  the tweeter frame touching the seam.
- After rejoin, everything above the seam moves down 120: tweeter hole
  **265 – 310, centre 287.5**; top edge at **380**.
- Volume: interior 210 × 260, height 480 → 360: **19.7 L gross**, ≈18.5 L net
  after drivers/splices. Fine for a 6.5″ sealed or ported; the DSP voicing EQ
  ([`../amp/DSP.md`](../amp/DSP.md)) absorbs the low-end shift either way.

## Rejoining 10 mm stock

Edge-to-edge butt joint = 10 mm glue face. Not enough on its own. **Internal
splice strips** across the seam on all four walls:

| Wall | Strip | Notes |
|---|---|---|
| Sides (×2) | 10 × 60 × 260 | centred on the seam, 30 each side |
| Back | 10 × 60 × 210 | same |
| Baffle | 10 × 35 × 210 | **asymmetric**: 20 below the seam (stops at the woofer hole edge, 210), 15 above — cannot be wider without intruding into the woofer hole. Under the two drivers' flanges it also acts as a doubler. |

Strips go in **through the woofer hole** after the glue-up (145 mm — hand and
clamp fit). Glue the seam first with the two sections clamped square on a flat
surface, then fit the strips from inside while the seam is still wet or after
it has cured — either works; wet lets you pull the seam closed with the strips.

Alternative: dowels or a spline in the seam. Not worth it in 10 mm stock;
the strips are stronger and need no precision.

## Cutting

- Circular saw on a **clamped straight edge** (or track saw). Blade depth just
  through one wall (12–13 mm) so it does not touch the opposite wall or
  anything inside.
- **Wrap the line around the box from the bottom edge** with a square, not off
  the previous line — the two cuts must be parallel and each must be square to
  the box's axis or the sections will not mate. Mark 230 and 350 on all four
  faces, check the diagonal across the removed band is equal on both sides.
- One cut per setup, all four walls, then move the straight edge. Do the
  **top cut first** (350): if it wanders, there is still stock to correct
  with the second cut. Cut A at 230 is the unforgiving one (the woofer frame
  is 7 mm away).
- 3 mm kerf: the band actually removed is 120 + kerf on both cuts is *not*
  the case — the kerf is inside the band. Mark the lines at 230 and 350 and
  cut with the blade **inside** the band on both.
- Sand both sawn edges flat and coplanar before glue-up; a rocking joint
  cannot be pulled straight by the splices.

## Confirmed 2026-09-19

- Box is **sealed**; **nothing in the band** on the back or sides (no port,
  brace, cleat or terminal cup between 230 and 350). Cuts are clear.
- Electrically the system already runs as a 2-way: woofer on the combined
  woofer+mid channel, tweeter on its own, **4″ left in place unpowered**.
  Sounds good to the user — but note the unpowered 4″ in a sealed box is an
  untuned **passive radiator**, contributing bass that disappears when the band
  is removed. **Do not judge sealed-vs-ported on today's sound**; judge after
  the cut. (Shorting the mid's terminals damps it — a quick way to hear its
  contribution now.)

## Port — decision deferred to after the cut

User wants a port if it makes a real difference. It can (+3–6 dB around
tuning, far less excursion above it, and the DSP high-pass protects below it),
**but only tuned to the woofer's T/S parameters, which are not known**. A
guessed port is worse than sealed. Nothing about the cut depends on the
choice: the port goes in the **back panel of the bottom section**, untouched by
the cuts, and can be added or plugged later.

Order: (1) cut and rejoin sealed, listen; (2) get Fs/Qts/Vas — model number
off the woofer magnet, or a REW impedance sweep with a single-resistor jig;
(3) size the port for ≈18.5 L net, cut one hole in the back, listen; plug it
if it is not better.

### Measuring T/S with REW (recipe given 2026-09-19, not yet done)

Needs a **stereo** line input (laptop mic jack is mono — a UCA202-class USB
interface is the cheap answer), one ~100 Ω sense resistor (DMM its real value),
a DMM for Re, and 10–20 g of weighed plasticine. Jig: OUT → sense R → driver →
ground; **IN-L across the output (reference), IN-R across the driver**. Low
output level — a loud sweep pulls Fs and Q down. REW: Soundcard prefs stereo
input, ref = Left; Measure → Impedance, enter sense R, **calibrate open /
short / known resistor**; measure free air; add mass on the cone around the
dust cap (Fs must drop 20–35 %); measure again; TS Parameters → Re from the
DMM, Added mass, Sd from the measured cone + ⅓ surround (6.5″ ≈ 135–145 cm²).
Record **Fs, Qts, Vas, Re** here:

| Fs | Qts | Qes | Qms | Vas | Re | Sd |
|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — |

Qts > ~0.6 → sealed regardless; 0.3–0.5 → port worth doing.

**The 6.5″ is a long-throw sub driver: 60 W RMS, 8 Ω, ~70 mm magnet, ~1.5 cm
excursion** (user, 2026-09-19 — p-p vs one-way unconfirmed; Xmax ≥ ~7 mm
either way). Excursion is *not* the bass limit; **amp voltage is**: TPA3118 on
a 24 V rail into 8 Ω ≈ 25–30 W, half the driver's rating (paralleling outputs
does not help — 8 Ω is voltage-limited, not current-limited). The 70 mm
magnet is a modest motor for a sub → expect **Qts on the high side
(0.5–0.7)**, Fs 35–55 Hz, small Vas, Re ≈ 6–7 Ω. **Use 30–50 g added mass**
for the T/S measurement, not 10–20.

Decision rule once measured: **Qts ≤ 0.45 → port** (the efficiency is the
only free bass headroom with these amps); **Qts ≥ 0.55 → sealed** (ported
would boom); between → either.

**Bigger woofer amp? Decided 2026-09-19: not yet.** A 60 W amp = +3 dB over
the TPA3118, barely audible; not worth it. If the TPA3118s audibly clip on
bass at real listening levels after the rebuild, the step is a **TPA3255
stereo board on 36–48 V** for both woofers (+5–6 dB, new PSU, mute rework) —
not a single 60 W channel. Free first: the low band already gets all the
woofer amp's watts (active crossover), and a **soft limiter on the LOW band in
the DSP** would stop hard clipping — firmware task, in [`../amp/TASKS.md`](../amp/TASKS.md).

**Expected gain, revised for this driver:** port = **+3–6 dB at 40–60 Hz per
watt** (efficiency, ≈ a 2–4× bigger amp in the bass) and F3 ~55 → ~40 Hz
without EQ. Sealed + a Linkwitz-transform low shelf in the voicing EQ reaches
the same response at 2–4× the power, with lower group delay and no tuning
risk — a good alignment for a high-Xmax driver, not a compromise. Nothing
changes above ~120 Hz. Ported needs a DSP high-pass ≈ 30 Hz below tuning.
Lean **sealed** unless Qts < ~0.4 and the amps prove to be the bottleneck.

Watch after the rebuild: a sub cone's Le/mass usually fades by 1–2 kHz and a
2″ tweeter rarely reaches below ~2 kHz — check the 1.5–3 kHz region in REW;
the current voicing EQ may be covering a dip there.

The conversion itself buys integration, not bass: tweeter–woofer spacing
270 → 150 mm widens the vertical lobe at the crossover.

## Ready to mark out (2026-09-19)

All inputs for the cut are measured. Assumes the same 6.5″ and 2″ units go
back in (the existing holes then stay right). The woofer's top mounting screw
(~78 mm radius → ~215 from the bottom) lands inside the baffle splice strip
(210–230) and gets 20 mm of wood.

## Open

- Driver model numbers / T/S parameters — not recorded anywhere; needed for
  the port.
- Tweeter delay in the DSP changes when it moves 120 mm closer to the woofer —
  re-do time alignment after the rebuild.
- Where the amps, PSU and ESP32-S3 physically live. If inside the cabinet the
  volume above changes.
- Whether the second cabinet is dimensioned identically — measure it, do not
  assume; cut both from their own marks.
