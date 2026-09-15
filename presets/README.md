# presets

User tunings kept for the record; both import through the app's **Import Peace / AutoEQ…**
button.

- `my-speakers-filtercurve.txt` — the speaker measurement as an Audacity/REW `FilterCurve`
  (50 points, 10 Hz–18.9 kHz). Not parametric: the importer fits it to the ten voicing
  bands (`Eq/FilterCurveFitter.cs`). 2026-09-15 fit: RMS 0.08 dB, worst 0.3 dB at 10.2 kHz
  over 30 Hz–18 kHz, preamp −3.7 dB; loaded on the device and saved as its boot default.
- `punchy-clear.txt` — the Peace preset of the same name exported as Equalizer APO text
  (11 filters incl. an HPQ at 44 Hz, which the importer skips: 10 bands + preamp −7 dB).
- `sennheiser-hd600.txt` — the Peace "Sennheiser HD 600" preset (AutoEQ-style: LSC 105 Hz, HSC 10 kHz, preamp −6.3 dB) converted to Equalizer APO text. Peace `.peace` filter codes seen so far: absent = PK, 14 = LSC, 15 = HSC, 16 = LS, 17 = HS, 2 = HPQ.
