# Tasks

Open work, grouped by where it lives. State claims are dated; "compiled only"
and "verified on hardware" are kept distinct. Enclosure work is documented in
`hardware-handoffs/speaker-dsp-crossover/ENCLOSURE.md`.

## Firmware (`firmware/dsp_engine`)

- [ ] **Soft limiter on the LOW band.** The woofer amps (TPA3118, 24 V rail,
      8 Ω load) are voltage-limited to ~25–30 W and are the loudness ceiling of
      the system; class-D hard clipping is a harsh crackle. A look-ahead or
      fast-attack/slow-release limiter on each woofer output, threshold set
      just under the amp's clip level, turns that into gentle compression.
      Runs per sample on core 1 — budget it against the 75 % load at 96 k /
      44 biquads. Threshold, ratio/release exposed over BLE (new opcodes,
      additive, PROTOCOL_VERSION stays 1). Added 2026-09-19.
- [ ] **Wire the four mute NPNs and confirm polarity.** `PIN_MUTE[4] =
      {10,11,12,13}` (Lw,Rw,Lt,Rt) per WIRING.md; `MUTE_ACTIVE_HIGH` assumes
      GPIO high = header shorted = muted. Compiled only since 2026-09-15.
- [ ] **Listen to the HIGH DAC** with the full-range check off — hear the
      crossover split (LOW DAC verified by ear 2026-09-16, HIGH not).
- [ ] **Merge `usb-dsp` to `main` and push** — user's call; branch contains
      `readback` + `uac` + host-volume + reconnect (head `386d05a`), all
      verified on hardware 2026-09-15/16.

## After the enclosure rebuild (3-way → 2-way, see ENCLOSURE.md)

- [ ] **Re-do tweeter time alignment.** The tweeter moves 120 mm closer to
      the woofer (C-to-C 270 → 150 mm); the per-driver delay values are stale.
- [ ] **Re-measure with REW and re-fit the voicing EQ.** The unpowered 4″
      passive-radiator bass is gone, the box is 26 → 18.5 L, the baffle is
      shorter. Check the **1.5–3 kHz region** specifically: a heavy sub cone
      fades by 1–2 kHz and a 2″ tweeter rarely reaches below ~2 kHz; the
      current EQ may be covering a dip there. Revisit the crossover frequency
      against that measurement.
- [ ] **Woofer T/S measurement** (REW impedance, 30–50 g added mass — recipe
      in ENCLOSURE.md) → fill the table there → port / sealed decision by the
      Qts rule.
- [ ] **Woofer amp**: only if the TPA3118s audibly clip at real listening
      levels → TPA3255 stereo board on 36–48 V for both woofers. Not before.

## Companion app (`companion/SpeakerDspDeck`)

- [ ] **Carry over MacroPadDeck's dirty-state / fixed-slot patterns**
      (unsaved-changes indicator, slot semantics). Noted 2026-09-16.
- [ ] UI for the limiter parameters once the firmware has them.
