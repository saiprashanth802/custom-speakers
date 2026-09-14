# Interface & control architecture

**Status (2026-09-15):** built. The BLE control layer, the companion app
(`companion/SpeakerDspDeck`, "Void Deck" UI with a live response curve) and the
full editor below were **verified on hardware 2026-08-23**; the read-back /
boot-profile additions of 2026-09-15 are compiled only (see PROTOCOL.md). What
is still design-only is the **audio source** — the profile table below still
describes the intended USB/WiFi inputs, and the firmware runs a two-tone test
generator in their place.

## Scope decisions (2026-08-23)

- **Control surface:** BLE companion app **only** for now. A **clean,
  purpose-built app** (not bolted onto MacroPadDeck) — reuse MacroPadDeck's
  BLE/WinRT plumbing and design language, but a fresh DSP-control UI. Physical
  encoder/OLED and a web UI are explicitly deferred to a **possible v2** — keep
  the device compact and screen-/knob-less; the app is the whole UI.
- **EQ import:** the app must import **Peace / AutoEQ / Equalizer APO** presets
  (parametric `.txt`) into the Voicing EQ. See [DSP.md](DSP.md) for the format
  and filter-type mapping.
- **Tuning depth:** full editor — crossover freq/slope, per-driver level trim,
  time-align/delay, parametric EQ, presets.
- **Audio sources:** **USB-C (UAC) + WiFi only.** Bluetooth music is dropped
  on purpose (see the hard constraint below).

## Hard constraint: ESP32-S3 is BLE-only — no A2DP

The **ESP32-S3 has no Classic Bluetooth (BR/EDR)** radio. A2DP is a Classic-BT
profile, so **A2DP music sink is impossible on the S3** — not a config issue,
the hardware can't do the profile. LE Audio (LC3) exists over BLE but is not a
practical phone-music path on the S3 yet (immature stack, spotty phone support).

Consequences:
- BLE **companion app / control** → fine, that's what the S3 does.
- **Phone-over-Bluetooth music → not available** with this chip. Accepted.
- Real audio inputs are **USB-C (UAC, ≤24/48)** and **WiFi (≤24/96+)**.

(If phone-BT music ever becomes a hard requirement it's a chip-level fork:
original ESP32 with Classic BT, or a dedicated A2DP receiver module feeding an
input. Both were considered and declined for compactness on 2026-08-23.)

## The "High-Res / Normal" toggle = bundled Audio Profile

Rate and source are coupled by hardware (WiFi is the only 24/96-capable source;
USB caps at 24/48), so the toggle switches a **profile**, not just a rate:

| | High-Res | Normal |
|---|----------|--------|
| Source | WiFi stream | USB-C (UAC) |
| Rate / depth | 24-bit / 96 kHz | 24-bit / 48 kHz |
| DSP | full crossover + EQ + delay | same, at 48 k (more CPU headroom) |
| Radio | WiFi + BLE both live (coex) | WiFi off → BLE alone, low latency/power |
| Use | serious listening, plugged in | desk/PC use, lowest latency |

- Crossover/EQ/delay **params are identical** across profiles; only sample rate
  and source change.
- On switch, firmware **recomputes biquad coefficients** for the new rate
  (coefficients are sample-rate dependent).
- **Switch is pop-free by reusing the amp mute line:** mute (amp GPIO + PCM5102
  `XSMT`) → stop I2S → reconfigure clock + source (start/stop WiFi or USB) →
  recompute coeffs → un-mute.
- Side effect: Normal mode turns WiFi off, so BLE owns the 2.4 GHz radio →
  snappier app + lower RF floor. High-Res runs both (S3 supports coexistence;
  BLE control traffic is tiny, so it is fine).

## Companion app (BLE) — reuse MacroPadDeck / host-link

Windows companion app ↔ ESP32-S3 over a **custom BLE GATT service**, same design
language and BLE plumbing as MacroPadDeck (mind the documented Windows/WinRT BLE
gotchas). Suggested GATT shape:

- **Command/response characteristic** (write + read) — small binary protocol:
  `opcode + payload` (set-param, get-param, save-preset, load-preset,
  switch-profile, mute, volume). Mirrors the host-link approach already built.
- **Status/telemetry characteristic** (notify) — active profile, source, rate,
  link state, optional level meters.

### App sections

1. **Status / Now-playing** — profile, source, sample rate, link state.
2. **Audio Profile** — High-Res ⇄ Normal toggle.
3. **Volume / Mute** — master digital gain + global mute (also asserts the amp
   mute GPIO).
4. **Crossover** — frequency, slope (LR2 / LR4).
5. **Level trim** — per-driver gain (tweeter attenuation).
6. **Time align** — per-driver delay (µs / mm).
7. **Parametric EQ** — N bands (freq / Q / gain).
8. **Presets** — save / recall named presets (NVS).

## Firmware control architecture

- **Single source of truth:** one **versioned `Params` struct** (crossover, per-
  driver trim/delay/EQ, volume, active profile). Version field so the app and
  firmware can detect a schema mismatch.
- **Edit flow:** app sends a delta over BLE → firmware validates → writes to a
  **double-buffered** param block → the audio core picks up the new block via an
  **atomic pointer swap** (no half-applied values, no zipper noise).
- **Persistence:** NVS holds the param block + named presets + last-active
  profile. Restored on boot. Reuse the macropad NVS pattern.
- **Core split (S3 is dual-core):**
  - **Core 1 — audio engine only:** I2S in/out + crossover biquads. Never blocks.
  - **Core 0 — everything else:** BLE stack, command handling, NVS, WiFi (High-
    Res only).
- **Boot:** amps held **muted** (see WIRING.md mute section) until params are
  restored from NVS and I2S is streaming, then un-mute. Pop-free.

## Open questions / next steps

- [x] Companion app: **clean purpose-built app**, reuse MacroPadDeck BLE/WinRT
      plumbing + design language, fresh DSP-control UI, with Peace/AutoEQ import.
- [~] EQ band counts: proposed **10 voicing (pre-crossover) + 4/driver
      (post-crossover)** — fits 96 kHz on a dedicated core (see DSP.md budget).
      Confirm after measuring real cycles/biquad.
- [ ] WiFi source protocol for High-Res: Snapcast / raw HTTP PCM / custom? (See
      WIRING.md "Audio source options" — streaming decoded PCM saves CPU.)
- [ ] Define the `Params` struct + BLE opcode table before writing either side.
- [ ] Verify WiFi+BLE coexistence latency is acceptable for live EQ tweaks in
      High-Res on real hardware.
