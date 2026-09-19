# uactool — bench tools for the USB Audio test firmware

`uactool` (.NET 9 console, NAudio) probes and drives the board's USB audio endpoint from
Windows; the two scripts talk to the TinyUSB CDC console.

    dotnet run -c Release -- probe                  # which exclusive-mode formats the driver accepts
    dotnet run -c Release -- play 96000 24 10       # 1 kHz sine at -30 dBFS, exclusive mode, 10 s
    dotnet run -c Release -- shared 10              # through the Windows mixer at its default format

    .\cdcread.ps1 -port COM5 -secs 8                # read the firmware's per-second status line
    .\cdcboot.ps1 -port COM5                        # run it TWICE: kicks the board into the ROM bootloader

## Gotchas (all measured 2026-09-15)

* **Read the console with DTR *and* RTS asserted.** Opening the port with DTR on / RTS off
  makes Windows walk exactly the DTR/RTS pattern arduino-esp32's `USBCDC` treats as
  "esptool wants the bootloader": the board drops into ROM download mode and the audio
  endpoint disappears. `cdcread.ps1` sets both.
* **`cdcboot.ps1` must run twice.** The reboot state machine in `USBCDC::_onLineState` is
  a 4-step sequence that starts only from its idle state; the first pass resets it, the
  second one trips it. Afterwards flash with `arduino-cli upload -p COM3` (the ROM
  bootloader's USB-Serial-JTAG port), and the app comes back on a different COM number.
* **Change the USB PID when the audio format changes.** Windows caches the audio
  endpoint's properties per device instance; a device that re-enumerates with a
  different format under the same VID/PID/serial passes `IsFormatSupported` but fails
  `Initialize` with `AUDCLNT_E_UNSUPPORTED_FORMAT` (0x88890008) and never sends
  SET_INTERFACE. The firmware derives the PID from the format for that reason.
* `probe` asks the Windows driver, not the device: it reflects the descriptor, so a
  format the device cannot actually receive (24/96 without the RX-FIFO workaround)
  still shows "OK". Only the firmware's `pkts/s` counter proves data is arriving.
