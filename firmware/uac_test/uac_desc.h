// UAC1 stereo speaker with asynchronous feedback — descriptor template.
//
// Adapted from TinyUSB examples/device/uac2_speaker_fb/src/usb_descriptors.h (MIT,
// commit 6e891c6dc), which is the full-speed path that example takes. Two changes:
//   * an Interface Association Descriptor in front, because this device is a composite
//     (CDC console + audio) and Windows' usbccgp is happiest grouping AC+AS by IAD;
//   * the feature unit + terminals are left exactly as the example has them — Windows
//     insists on a mute/volume feature unit before it will expose the endpoint.
//
// bRefresh = 0 on the synch endpoint is deliberate (the example documents it): DWC2
// cannot schedule an ISO IN at an interval that differs between hosts, so the feedback
// packet is simply offered every 1 ms frame, matching bInterval.
#pragma once

#include "uac_config.h"
#include "tusb.h"

#define UAC1_ENTITY_INPUT_TERMINAL   0x01
#define UAC1_ENTITY_FEATURE_UNIT     0x02
#define UAC1_ENTITY_OUTPUT_TERMINAL  0x03

#define UAC1_DESC_IAD_LEN 8
#define UAC1_DESC_IAD(_firstitf) \
  UAC1_DESC_IAD_LEN, TUSB_DESC_INTERFACE_ASSOCIATION, _firstitf, 2, TUSB_CLASS_AUDIO, 0x00, 0x00, 0x00

#define UAC1_SPEAKER_STEREO_FB_DESC_LEN(_nfreqs) (\
    UAC1_DESC_IAD_LEN\
  + TUD_AUDIO10_DESC_STD_AC_LEN\
  + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
  + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)\
  + TUD_AUDIO10_DESC_STD_AS_LEN\
  + TUD_AUDIO10_DESC_STD_AS_LEN\
  + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
  + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN\
  + TUD_AUDIO10_DESC_STD_AS_ISO_SYNC_EP_LEN)

#define UAC1_SPEAKER_STEREO_FB_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epoutsize, _epfb, ...) \
  UAC1_DESC_IAD(_itfnum),\
  /* Standard AC Interface Descriptor(4.3.1) */\
  TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
  TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)), /*_itf*/ ((_itfnum)+1)),\
  /* Input Terminal Descriptor(4.3.2.1) */\
  TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_nchannels*/ 0x02, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_LEFT_FRONT | AUDIO10_CHANNEL_CONFIG_RIGHT_FRONT, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.3.2.2) */\
  TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ 0x02, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.3.2.5) */\
  TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch2*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
  /* Standard AS Interface Descriptor(4.5.1) — alternate 0, zero bandwidth */\
  TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.5.1) — alternate 1, streaming */\
  TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.5.2) */\
  TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ 0x01, /*_delay*/ 0x00, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
  /* Type I Format Type Descriptor(2.2.5) */\
  TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ 0x02, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
  TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ _epoutsize, /*_interval*/ 0x01, /*_sync_ep*/ _epfb),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
  TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000),\
  /* Standard AS Isochronous Synch Endpoint Descriptor (4.6.2.1) */\
  TUD_AUDIO10_DESC_STD_AS_ISO_SYNC_EP(/*_ep*/ _epfb, /*_bRefresh*/ 0)
