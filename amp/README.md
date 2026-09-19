# DSP / amp unit (v2)

A standalone box that takes USB audio from a PC and drives two passive-free
cabinets over NL4 cables: **active 2-way stereo crossover on an ESP32-S3, two
PCM5102 DACs, four TPA3118 mono class-D amps, 24 V 150 W AC-DC supply**,
controlled from a Windows companion app over BLE.

```
 PC ──USB-C──▶ ESP32-S3 ──I2S0──▶ PCM5102 "LOW"  ──▶ TPA3118 ×2 ──▶ NL4 pin 1 ──▶ L / R woofer
   (UAC 1.0)   crossover  ──I2S1──▶ PCM5102 "HIGH" ──▶ TPA3118 ×2 ──▶ NL4 pin 2 ──▶ L / R tweeter
                 ▲
                 └── BLE ◀── SpeakerDspDeck (Windows)
```

## What is in here

| Path | What |
|---|---|
| `firmware/dsp_engine/` | The firmware: USB audio class input, DSP chain, dual I2S out, BLE GATT server, NVS persistence, amp mute |
| `firmware/uac_test/`, `firmware/channel_test/` | Bench sketches that proved the USB path and the DAC/amp channels before merging |
| `companion/SpeakerDspDeck/` | Windows WPF app (.NET 9): crossover, voicing EQ with Peace/AutoEQ/Equalizer-APO/REW-curve import, per-driver level/delay/EQ, presets, 48/96 k profile |
| `tools/` | `bletool` (raw BLE opcode harness), `uactool` (USB bench) |
| `presets/` | The user's speaker EQ, fitted to 10 bands |
| `WIRING.md` | Signal chain, pin plan, PCM5102 strap settings, power budget, mute transistors, star ground |
| `DSP.md` | Signal chain, biquad implementation, **measured** CPU budget |
| `INTERFACE.md` | Control architecture, profiles, app sections |
| `PROTOCOL.md` | BLE GATT protocol v1 (canonical) |
| `schematic.html` | Block schematic + ground-path diagram (open in a browser) |
| `TASKS.md` | Open work |

## Hardware

| Block | Part | Notes |
|---|---|---|
| DSP / USB / BLE | ESP32-S3 Mini | dual core: core 1 = audio only, core 0 = USB, BLE, NVS |
| DACs | 2× CJMCU-5102 (PCM5102) | split **by band**, not by side: one DAC = both woofers, the other = both tweeters, so the noise-sensitive high path never shares a converter with the high-current low path. Straps: SCK→GND (wire), FMT/FLT/DEMP = L, XSMT = H |
| Amps | 4× TPA3118D2 mono | one per driver; BTL outputs (no leg is ground); mute via one NPN per board on GPIO 10–13 |
| PSU | 24 V / 150 W AC-DC | shared by the four amps; buck to 5 V for logic. 150 W is ~60 % of four amps at full rating — fine for music, not for four simultaneous test tones |
| Output | 2× NL4 chassis sockets | 1± = woofer, 2± = tweeter |
| Enclosure | custom | amps + PSU + logic in one box; power tier above the logic board, **star ground** to the PSU negative |

Into 8 Ω on a 24 V rail the TPA3118 is voltage-limited to **~25–30 W per
channel**. That, not the drivers, is the system's loudness ceiling.

## Status — what has been verified on hardware

| Date | Verified |
|---|---|
| 2026-08-23 | DSP engine (TDF2 biquads, LR4 + voicing + per-driver EQ), BLE control end-to-end, NVS save/restore, 48⇄96 k switch, presets, per-driver delay — tag `amp-v1-verified-2026-08-23` |
| 2026-09-15 | USB audio class input at 24/48 and 24/96 through the full chain; rate follows the host; 44 biquads at 96 k = **75 %** of the audio core; device→app parameter read-back; boot-profile restore |
| 2026-09-16 | **Music heard by ear** through USB → DSP → DAC → amp; Windows volume/mute applied in firmware; app reconnects after a board reboot |

Not yet on hardware: the four amp-mute transistors (designed, GPIOs assigned,
firmware written — **not wired**); the HIGH DAC listened to on its own.

## Measured numbers worth knowing

- **42.7 cycles per biquad** at `-O3`; **70.9** at the Arduino default `-Os`.
  The `#pragma GCC optimize("O3")` in the DSP files is mandatory.
- 44 biquads: 37.6 % of one core at 48 k, 74.9 % at 96 k. Per-sample cost is
  rate-independent.
- USB at 24/96: the ESP32-S3's USB RX FIFO is too small for TinyUSB's default
  2×-packet rule; the firmware resizes it per interface (`UAC_RXFIFO_HACK`).
  Without it, 96 k streams silently drop every packet.
- Windows sends the **same dB** to the USB master and to each channel volume;
  the firmware uses the more attenuated of the two, not the sum.

## Gotchas that cost real time

Collected in `WIRING.md`, `DSP.md` and `INTERFACE.md`; the top ones:

1. `Serial` on the ESP32-S3 goes to the dead UART pins unless `CDCOnBoot` is
   set — the ROM banner shows, the sketch's prints never do.
2. A 128-bit service UUID plus a device name overflows the 31-byte BLE
   advertisement, so a UUID scan filter never matches; match on the name.
3. The device's connect-time notifications race the Windows CCCD subscribe
   and are lost; the app *pulls* HELLO and STATUS after subscribing.
4. A silent PCM5102 with perfect counters was **LCK and DIN swapped**. Flash
   the on-board tone build and swap a known-good module's wires to split
   module-vs-port in two steps.
5. Any helper function above `struct Compiled` in the `.ino` breaks the build
   (Arduino's prototype generator). Keep helpers below `dB2lin`.

## Building

- Firmware: `arduino-cli`, esp32 core 3.3.6, FQBN
  `esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default` for the USB-audio
  build (CDC-on-boot starts USB before `setup()` and the audio interface can't
  be added after).
- App: `dotnet build` in `companion/SpeakerDspDeck` (.NET 9).

## Open

See `TASKS.md`: LOW-band limiter, wire the mute NPNs, listen to the HIGH DAC
alone, post-rebuild re-alignment, woofer amp upgrade only if the TPA3118s
audibly clip.
