# bletool — raw opcode driver for bench tests

A console harness that reuses the app's `BleLink` / `Frame` / `Opcodes` (copy the three
files from `companion/SpeakerDspDeck` next to `Program.cs`, they are not committed here to
avoid a second copy drifting) and sends a scripted opcode sequence. Written 2026-09-15 to
exercise `CMD_SAVE_PRESET` / `CMD_LOAD_PRESET` across a rate change — the app never sends
those (its presets are app-side), so the firmware path had no other way to be tested.

Close the app first: one BLE central at a time.
