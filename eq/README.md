# EQ Preset — tight-bass.peace

Peace Equalizer (Equalizer APO frontend) preset for the custom 3-way studio monitors.

## How to use

1. Install [Equalizer APO](https://sourceforge.net/projects/equalizerapo/) and [Peace GUI](https://sourceforge.net/projects/peace-equalizer-apo-extension/)
2. Copy `tight-bass.peace` to your Peace presets folder (usually `C:\Program Files\EqualizerAPO\config\`)
3. Open Peace → Load Preset → select `tight-bass.peace`

---

## Filter Breakdown

| # | Frequency | Type | Gain | Q | Acoustic Purpose |
|---|---|---|---|---|---|
| 1 | 26 Hz | High-pass (Butterworth) | — | — | Woofer excursion protection. Cuts sub-bass the driver can't reproduce — prevents mechanical damage at high volumes |
| 2 | 53 Hz | Low shelf | −3 dB | — | Tightens bass bloom. The woofer's natural resonance at this region causes bloated, slow-sounding bass. A shelf cut here firms it up |
| 3 | 84 Hz | Bell (peak) | +8.5 dB | 1.2 | Primary bass body. Adds punch and weight to kick drum and bass guitar. Wide Q (1.2) spreads the boost naturally |
| 4 | 294 Hz | Bell | −3.5 dB | 2.1 | Low-mid boxiness cut. This region causes the "cardboard box" coloration common in wood enclosures — surgical cut removes it |
| 5 | 1119 Hz | Bell | −2.8 dB | 1.8 | Nasal/honky midrange correction. Reduces that phone-speaker quality that appears when this band is elevated |
| 6 | 2465 Hz | Bell | −2.2 dB | 2.0 | Upper midrange harshness. Reduces listener fatigue on long sessions — strings and electric guitar benefit most |
| 7 | 3676 Hz | Bell | +2.0 dB | 2.5 | Presence boost. Adds definition and clarity to vocals and acoustic instruments — makes the mix feel more "forward" |
| 8 | 6488 Hz | Bell | −3.0 dB | 2.2 | Sibilance control. Reduces the harsh edge on "s" and "sh" consonants in vocals and cymbals |
| 9 | 10332 Hz | High shelf | +2.5 dB | — | Air and sparkle. Extends perceived high-frequency detail — makes the top end feel open rather than rolled off |

**PreAmp: −5 dB** — Essential headroom to prevent digital clipping when the +8.5 dB bass boost stacks with other gains.

**Stereo Expanding: +3.5** — Widens the stereo image beyond the physical speaker separation, improving soundstage for near-field listening.

**Stereo Shift: −2.047** — Minor center-image correction for asymmetric room placement.

---

## Notes

- This preset was tuned specifically for the Toyotone TT-660 6.5" woofer in this particular enclosure. It may need adjustment for different drivers or room sizes.
- The −5 dB preamp is non-negotiable when using the +8.5 dB bass boost. Removing it will cause clipping at the DAC stage before the signal reaches the amplifier.
- For IEM use, reduce the bass boost to +4–5 dB and remove the high shelf (IEMs typically have better treble extension than the dome tweeter).
