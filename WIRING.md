# Active crossover — wiring & signal chain

**Status:** planning, nothing built/verified on hardware yet. Started 2026-08-23.

## System summary

- **Source/DSP host:** ESP32-S3-Mini, running the crossover DSP in firmware
  (split L/R input into low-pass "woofer" and high-pass "tweeter" bands,
  output as two independent stereo I2S streams).
- **DACs:** 2x PCM5102 breakout boards, each stereo, split by band rather
  than by side — DAC-LOW carries both woofer channels (L+R), DAC-HIGH
  carries both tweeter channels (L+R). Deliberate: this keeps each DAC's
  two outputs on the same crossover band, so the high-frequency (more
  noise-sensitive) path never shares a converter with the low-frequency
  path.
- **Amps:** 4x TPA3118/TPA3116 mono (single-channel) boards, one per driver.
- **Drivers:** 2-way stereo, 4 drivers total (L-woofer, L-tweeter, R-woofer,
  R-tweeter).
- **PSU:** single 24V / 150W DC supply, shared by all 4 amps; stepped down
  for logic. See the power budget check below — 150W does not cover all
  4 amps at full rated 60W simultaneously.

**Full schematic + dedicated ground-path diagram:** [schematic.html](schematic.html)
(open in a browser). This file covers the block diagram, pin tables, and
config tables in prose; the schematic is the visual reference to wire from.

## Signal path

```
                         ESP32-S3-Mini (crossover DSP)
                         ┌─────────────────────────┐
                  I2S0   │  BCK4 / WS5 / DOUT6      │   I2S1
                 (LOW)   │                          │  (HIGH)
                         └─────────────────────────┘
                    │                                    │
                    ▼                                    ▼
              PCM5102 — LOW                        PCM5102 — HIGH
              stereo line out                       stereo line out
              OUTL / OUTR                            OUTL / OUTR
                 │      │                                │      │
                 ▼      ▼                                ▼      ▼
              Amp1    Amp2                             Amp3    Amp4
            (L-woof)(R-woof)                         (L-tweet)(R-tweet)
                 │      │                                │      │
                 ▼      ▼                                ▼      ▼
            L-Woofer  R-Woofer                     L-Tweeter  R-Tweeter
```

Each PCM5102 is stereo, and each amp is mono — so each DAC's L/R outputs
split to two separate mono amps. That maps cleanly onto "2-way stereo" with
no extra summing or splitting hardware needed.

## ESP32-S3 I2S pin plan

The S3 has two independent I2S peripherals (I2S0, I2S1) and a fully
remappable GPIO matrix, so any free GPIO works — pick pins that avoid
strapping/PSRAM/USB pins. Suggested (verify against your specific board's
silkscreen — "ESP32-S3 Mini" covers several similar clone boards with
slightly different broken-out pins):

| Signal          | I2S0 (→ DAC-LOW) | I2S1 (→ DAC-HIGH) |
|-----------------|-----------------|-----------------|
| BCK (bit clock) | GPIO4           | GPIO7           |
| WS (LRCK)       | GPIO5           | GPIO8           |
| DOUT (data)     | GPIO6           | GPIO9           |

**Avoid:** GPIO0 (boot strap), GPIO3/45/46 (strapping, often not broken out
on mini boards), GPIO19/20 (native USB D-/D+ if you use USB for
programming/power), GPIO35-37 (reserved if your module uses octal PSRAM).

**No MCLK needed:** run each PCM5102 in its internal-PLL clocking mode
(SCK pin grounded) so BCK/WS/DIN (3 wires) per DAC is sufficient — don't
need to route a 4th master-clock line from the ESP32.

## PCM5102 board config pins

Standard PCM5102 breakout modules expose these control pins — tie them as
follows (verify your board's actual pinout/silkscreen, clones vary):

| Pin  | Function              | Setting |
|------|------------------------|---------|
| SCK  | external MCLK in       | tie to GND (use internal PLL) |
| FMT  | audio format select    | tie to GND (I2S standard format) |
| XSMT | soft mute, active-high | tie to **3.3V** (unmuted) — leaving this floating/low is the classic "everything's wired right but it's silent" bug |
| FLT  | filter select           | tie to GND (normal latency filter) |
| DEMP | de-emphasis             | tie to GND (off) |
| VIN  | board supply            | 5V (check your specific board — some want 3.3V direct) |

## Amp input/output

- TPA3118/TPA3116 mono boards take single-ended line-level input
  (IN+ from PCM5102 OUTL or OUTR, IN− to signal ground) — confirm your
  specific board's input is single-ended vs. differential before wiring;
  some TPA3116 boards expect a differential pair.
- Amp outputs are **bridge-tied (BTL)** — neither output leg is ground.
  Do **not** tie either speaker terminal to system ground or short it to
  another channel's output.
- Confirm max input voltage rating of your specific amp board against the
  24V rail before connecting power.

## Power distribution and budget

- **24V/150W PSU → straight to all 4 amp boards' VIN**, heavy gauge, short
  runs, main input fused at ~8A slow-blow. Each amp branch individually
  fused at ~3A.
- **24V → buck converter → 5V rail** for ESP32-S3 and both PCM5102 boards'
  VIN. Logic load is negligible (well under 1A combined) against the
  budget below.
- Decouple each amp board's power input locally (bulk cap close to the
  board) rather than relying on the shared rail alone.

**Budget check:** 150W ÷ 24V = 6.25A the PSU can supply continuously. Each
amp at its full rated 60W draws 2.5A, so 4 channels at full rated power
simultaneously would ask for 10A — over budget by ~60%. In practice music
has enough crest factor that all 4 channels rarely peak together, so this
is a livable soft limit, not a bench-fire risk, but it means: don't expect
sustained full-rated-power on all 4 channels at once (e.g. test tones on
every channel simultaneously will sag the rail), and the main 8A fuse is
there to catch a real fault, not routine headroom use.

## Audio source options & quality ceilings

The DAC and the ESP32-S3 I2S hardware are **not** the quality limit here — the
PCM5102 does up to 32-bit/384 kHz and the S3's I2S clocks 96 kHz fine. What
actually caps quality is the **source path** and the **DSP CPU budget** (the
S3 runs a 4-channel crossover at the same time as decoding/receiving audio, so
a missed DMA deadline = an audible click). Ceilings per path:

| Source | Realistic max | Why |
|--------|---------------|-----|
| **WiFi** (streamed) | 24-bit / 96 kHz+ | Not codec-capped — buffer the stream, clock it locally |
| **USB-C** (UAC) | 24-bit / 48 kHz | Native USB is Full-Speed (12 Mbit/s); 48 k is the comfortable ceiling |
| **A2DP** (Bluetooth) | 44.1 kHz / 16-bit, lossy | ESP32 A2DP sink is **SBC only** |

**WiFi is the high-res path**, but the *protocol* still caps you: AirPlay and
Spotify Connect are 44.1/16; only Snapcast, a raw HTTP FLAC/WAV stream, or a
custom TCP/UDP PCM feed reach 24/96+. And the CPU squeeze is real — WiFi RX +
FLAC decode + 4-channel crossover on one chip is tight. The usual fix is to
stream **already-decoded PCM** (Snapcast-style): trades bandwidth (cheap on
WiFi) for CPU (precious). Running WiFi at 48 kHz also frees crossover headroom.

**USB-C at 24/48** is the simplest solid path — synchronous, clean, from a PC.

### Why Bluetooth is stuck at 16-bit (SBC vs aptX vs LDAC)

| | SBC | aptX / aptX HD | LDAC |
|---|-----|----------------|------|
| Owner | Open (in A2DP spec) | Qualcomm (proprietary) | Sony (proprietary) |
| Bitrate | ~328 kbps | ~352 / ~576 kbps | up to 990 kbps |
| Max quality | 44.1 kHz / 16-bit | HD: 24-bit / 48 kHz | up to 24-bit / 96 kHz |
| License | Royalty-free | Paid Qualcomm license | Sony license |

SBC is the mandatory A2DP baseline — royalty-free, low-CPU, so it's what
Espressif ships. **aptX and LDAC cannot be used on the ESP32-S3**, for three
reasons (the first is the actual wall):

1. **They're closed, licensed codecs, and we need the *decoder* (sink) side.**
   Sony/Qualcomm don't provide free decoders for the ESP32. Note Android
   open-sourced the LDAC *encoder* (the source/phone side) — the *decoder*
   side we'd need stays closed. Same asymmetry for aptX.
2. **Espressif's Bluetooth stack (Bluedroid) A2DP sink implements SBC only.**
   There's no aptX/LDAC codec to even advertise during the codec handshake, so
   a phone always falls back to SBC.
3. **CPU, secondarily** — LDAC at 990 kbps decode + the crossover would be
   tight, but this isn't the blocker; the code simply doesn't exist for the
   platform.

So Bluetooth = **SBC, 44.1/16, lossy, full stop** — not configurable away.
That's precisely why WiFi (which uses no Bluetooth codec) is the hi-res route.

**Design implication:** do the crossover math internally in **float / 32-bit**
and run both DACs at one shared rate. Then the source path only fills a buffer;
USB / WiFi / BT can be swapped without touching the DSP or clocking underneath.

## Amp mute control (TPA3118D2)

The TPA3118D2 has **two** control pins, different jobs:

| Pin | Logic | Effect |
|-----|-------|--------|
| **MUTE** | HIGH = muted, LOW = normal | Amp stays powered, output silenced — **click-free**. Use this for standby/mute. |
| **/SD** (shutdown) | LOW = shutdown, HIGH = enabled | Full power-down, lowest current; re-enable can pop unless sequenced. |

Both are TTL-compatible, so a 3.3V GPIO drives the logic side fine.

**On these breakout boards the "mute" control is a 2-pin jumper header** — two
pins meant to be bridged by a shorting shunt; bridging them sets the mute
state. So it's a *contact to open/close*, not a level to feed. One pin is the
chip's mute/SD control (held at an idle voltage by an onboard resistor), the
other is GND.

### Driving it from the ESP32 — emulate the jumper with an NPN

Replace the shunt with a transistor across the two header pins. **NPN is fine**
(no MOSFET needed) — BC547 / 2N2222 / 2N3904:

    header pin A (control) ──── C (collector)
                                   |
    GPIO ──[1k]── B (base)      [ NPN ]
                                   |
    header pin B (GND) ──────── E (emitter) ── star ground

- GPIO **high** → transistor saturates → shorts the header (= jumper in).
- GPIO **low**  → transistor off → header open (= jumper out).
- **1k base resistor is required** (a BJT base draws current; unlike a MOSFET
  gate). Emitter goes to the header's GND pin, collector to the control pin.
- ~0.1–0.2V collector saturation is irrelevant to the mute logic threshold.

### Two things to measure before wiring (physical jumper, amp powered)

1. **Which state is mute?** Bridge the two pins with a wire while a tone plays:
   sound stops when *shorted* → shorted = mute (GPIO-high = mute); sound stops
   when *removed* → open = mute (GPIO-low = mute). Sets firmware polarity.
2. **Which pin is GND?** Meter black probe on power ground: the header pin at
   ~0V is GND (emitter side); the other is the control pin (collector side).

### Layout & fan-out (see §4 in schematic.html)

Physical plan: `PCM1 | ESP32-S3 | PCM2` across the top, `T1 T2 T3 T4` in a row
below, **power distribution on an elevated tier** above the logic board (keeps
the 24V high-current wiring off the signal layer). Each amp has its own local
header, so **each amp gets its own NPN**, but **all four bases tie to one GPIO**
(`GPIO10`, placeholder — any free pin outside the I2S set GPIO4–9) for a single
global mute line. Split to 4 GPIOs only if per-channel bring-up is wanted.

### Fail-safe / pop-free sequence

ESP32 GPIOs float for a few hundred ms during reset, so wire it such that
**GPIO floating/low at boot = MUTED**. Firmware then:

1. Bring up I2S, let the PCM5102 settle (its `XSMT` un-mutes).
2. **Then** drive the amps un-muted.
3. On shutdown: mute the amps **first**, then stop I2S.

Muting *both* the DAC (`XSMT`) and the amps during transitions = no turn-on/off
pop. If your test showed *shorted = mute*, add a pull that keeps the base
inactive-but-muted at boot (or invert), so floating still means muted.

## Grounding — star topology

**Rule: every board's ground pin gets its own wire straight back to one
point — the PSU's negative terminal. Never chain GND from one board to the
next.** Daisy-chaining (PSU→ESP32→DAC→DAC→Amp→Amp→Amp→Amp in series) makes
each downstream board's ground reference ride on the return current of
every board ahead of it — the class-D amps' switching current ends up
sharing a wire with the DAC's analog ground reference, which is exactly
the coupling path you're trying to avoid by splitting LOW/HIGH across two
DACs in the first place. See the star diagram in
[schematic.html](schematic.html) for the concrete topology: 7 home-run
wires (ESP32, DAC-LOW, DAC-HIGH, Amp1–4) all meeting at that one point.

- Keep I2S wiring (BCK/WS/DOUT) short; if runs get long, twisted-pair
  signal+ground per line helps.
- Keep the high-current 24V amp wiring physically separated from the I2S
  and DAC analog signal wiring to avoid coupling switching noise from the
  class-D amps into the DAC outputs.
- Each PCM5102's analog GND should travel with its OUTL/OUTR wires back to
  the amp's input ground pin (twisted pair per channel) — that's the
  signal reference riding with the audio, distinct from the power-ground
  star above; the two only meet once, at the star point.

## Open items to verify before wiring up

- [ ] Confirm exact ESP32-S3-Mini board variant and its actual broken-out
      GPIOs (some mini boards omit pins listed above).
- [ ] Confirm PCM5102 breakout's VIN voltage requirement (3.3V vs 5V) —
      check board silkscreen/seller listing.
- [ ] Confirm TPA3118/TPA3116 board: single-ended vs differential input,
      and max supply voltage rating vs the 24V rail.
- [ ] If sustained full-power on all 4 channels turns out to matter, revisit
      the 150W PSU — it's sized for typical/dynamic use, not simultaneous
      full-rated draw on every channel.
