// USB Audio Class test build — configuration.
//
// Included FIRST by both the sketch and audio_device.c, before any TinyUSB header,
// because tusb_option.h defaults CFG_TUD_AUDIO to 0 the moment it is included and the
// arduino-esp32 prebuilt tusb_config.h never sets it.
//
// Build knobs (override with -DUAC_RATE=... / -DUAC_BYTES=... via --build-property):
//   UAC_RATE   sample rate advertised (one rate per build — UAC1 lists rates in the
//              format descriptor but the ISO endpoint has ONE wMaxPacketSize, and the
//              DWC2 RX FIFO is sized from that at configuration time, so mixing 48 k and
//              96 k in one descriptor would make the 48 k case pay the 96 k FIFO cost).
//   UAC_BYTES  bytes per sample on the wire: 3 = 24-bit packed, 2 = 16-bit.
#pragma once

#ifndef UAC_RATE
#define UAC_RATE   48000
#endif
#ifndef UAC_BYTES
#define UAC_BYTES  3
#endif
#define UAC_BITS   (UAC_BYTES * 8)
#define UAC_CH     2

// wMaxPacketSize for one 1 ms full-speed frame, +1 frame of slack so the host can
// speed up when the feedback endpoint asks it to. 24/48 -> 294, 24/96 -> 582, 16/96 -> 388.
#define UAC_EP_SZ  ((UAC_RATE / 1000 + 1) * UAC_BYTES * UAC_CH)

// ---- TinyUSB audio class driver configuration --------------------------------------
#define CFG_TUD_AUDIO                              1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                 0
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP           1
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX         UAC_EP_SZ
// FIFO_COUNT feedback regulates this buffer to half full; 8 frames deep = ~4 ms of
// latency at the regulation point, enough slack for the 1 ms Arduino tick.
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ      (8 * UAC_EP_SZ)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX          0
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ       0
#define CFG_TUD_AUDIO_CTRL_BUF_SZ                  64

// Experiment: after enumeration, grow the DWC2 RX FIFO to 1 x packet (what a patched
// driver would allocate) instead of TinyUSB's 2 x rule. See enlargeRxFifo() in the sketch.
#ifndef UAC_RXFIFO_HACK
#define UAC_RXFIFO_HACK 0
#endif
#if UAC_RXFIFO_HACK
#define UAC_RXFIFO_WORDS   s_rxfifoWords
#define UAC_RXFIFO_APPLIED s_rxfifoApplied
#else
#define UAC_RXFIFO_WORDS   0
#define UAC_RXFIFO_APPLIED 0
#endif
