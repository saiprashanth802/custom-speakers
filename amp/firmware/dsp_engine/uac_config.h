// USB Audio Class configuration for the DSP engine.
//
// Included FIRST by dsp_engine.ino, audio_device.c and uac_glue.cpp — before any TinyUSB
// header — because tusb_option.h defaults CFG_TUD_AUDIO to 0 the moment it is included
// and the arduino-esp32 prebuilt tusb_config.h never sets it.
//
// One descriptor, two rates: 48000 and 96000, 24-bit packed. The ISO endpoint has a
// single wMaxPacketSize sized for 96 k (582 B); at 48 k the host simply sends 294-byte
// packets into it. The RX-FIFO enlargement in the sketch is needed for both, since
// TinyUSB sizes the FIFO from the descriptor maximum at configuration time.
//
// audio_device.c / uac_desc.h / uac_glue.cpp are copies of firmware/uac_test's — Arduino
// cannot include a sibling sketch's files. uac_test stays the bench harness for the USB
// path on its own; this is the production configuration.
#pragma once

#define UAC_RATE_MAX   96000
#define UAC_BYTES      3
#define UAC_BITS       (UAC_BYTES * 8)
#define UAC_CH         2
#define UAC_EP_SZ      ((UAC_RATE_MAX / 1000 + 1) * UAC_BYTES * UAC_CH)   // 582
#define UAC_FRAME_BYTES (UAC_BYTES * UAC_CH)                               // 6

#define CFG_TUD_AUDIO                              1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                 0
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP           1
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX         UAC_EP_SZ
// FIFO_COUNT feedback regulates this to half full: 388 frames = 8 ms @ 48 k, 4 ms @ 96 k
// of buffering at the regulation point; the engine reads 128-frame blocks out of it.
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ      (8 * UAC_EP_SZ)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX          0
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ       0
#define CFG_TUD_AUDIO_CTRL_BUF_SZ                  64
