// USB Audio Class (UAC1) test firmware for the speaker DSP board — 2026-09-15.
//
// PURPOSE: answer one question on real hardware — can this ESP32-S3 present itself to the
// PC as a USB sound card, and at what format? It is NOT the DSP engine: USB frames go
// straight to the LOW DAC (I2S0, GPIO 4/5/6) as stereo, no crossover, no EQ. Once the
// format question is settled the receive path (audio_task below) is what moves into
// dsp_engine.ino in place of the tone generator.
//
// Build (USB-OTG mode, CDC NOT on boot — the audio interface has to be registered before
// USB.begin(), and CDCOnBoot=cdc starts USB before setup() runs):
//   arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default \
//       --build-property "compiler.cpp.extra_flags=-DUAC_RATE=48000 -DUAC_BYTES=3" \
//       --build-property "compiler.c.extra_flags=-DUAC_RATE=48000 -DUAC_BYTES=3" \
//       firmware/uac_test
//
// Why the audio class is compiled here: arduino-esp32 ships TinyUSB as a prebuilt library
// with CDC/MSC/HID/MIDI/Video/DFU/Vendor/NCM — everything except audio. TinyUSB lets an
// application add class drivers through usbd_app_driver_get_cb(), so audio_device.c (the
// exact upstream file for the prebuilt commit) rides along with the sketch.
//
// MEASURED 2026-09-15 (Windows 11, AMD xHCI root port, exclusive-mode WASAPI, 1 kHz tone):
//   24/48  stock TinyUSB          1000 pkt/s, 48000 frames/s to I2S, feedback FIFO steady   OK
//   16/88.2 stock TinyUSB         1000 pkt/s, 88200 frames/s                                OK
//   24/96  stock TinyUSB          Windows streams, device accepts alt 1 + SET_CUR 96000,
//                                 but 0 packets arrive: iso_alloc_fail=1                    FAIL
//   24/96  + UAC_RXFIFO_HACK=1    1000 pkt/s, 96000 frames/s, 0 underruns over 10 s        OK
// The wall is not USB full speed and not Windows: the S3's USB controller has 1024 bytes
// of FIFO shared by every endpoint, and TinyUSB's DWC2 driver insists on an RX FIFO of
// 2 x (largest OUT packet) + 30 words. 582-byte packets need 322 words of the 256 that
// exist, so the allocation fails silently at configuration and the controller drops every
// ISO packet. 1 x the packet (176 words) is enough in practice — the ISR drains a packet
// well inside the 1 ms frame — and enlargeRxFifo() below applies exactly that.
//
// Console: the TinyUSB CDC port (USBSerial), enumerates as a NEW COM number after the
// first flash from the old hardware-CDC build. Flash from then on targets that port.

#include "uac_config.h"          // must precede every TinyUSB header
#include "USB.h"
#include "USBCDC.h"
#include "esp32-hal-tinyusb.h"
#include "class/audio/audio_device.h"
#include "device/usbd_pvt.h"
#include "uac_desc.h"
#include <ESP_I2S.h>

// ---- Pins (LOW DAC, same as dsp_engine.ino) --------------------------------------------
constexpr int8_t I2S0_BCLK = 4, I2S0_LRCK = 5, I2S0_DOUT = 6;
constexpr int8_t PIN_MUTE  = 10;                 // amp mute NPNs, active high = muted

// ---- State -------------------------------------------------------------------------------
static I2SClass  i2s;
static USBCDC    USBSerial(0);                    // the core only instantiates one when CDCOnBoot=cdc
static uint8_t   s_epOut = 0, s_epFb = 0;
static uint8_t   s_itfStreaming = 0xFF;           // AS interface number, learnt from the descriptor cb
static volatile bool     s_streaming = false;     // AS alt setting 1 selected by the host
static volatile uint32_t s_rate      = UAC_RATE;  // last SET_CUR sampling-frequency from the host
static uint8_t   s_mute[UAC_CH + 1];
static int16_t   s_volume[UAC_CH + 1];            // dB, as the host set them (not applied)

// Counters, written from the USB task / ISR and read by loop() for the once-a-second line.
static volatile uint32_t s_pkts = 0, s_bytes = 0, s_short = 0;
static volatile uint16_t s_fifoNow = 0, s_fifoMin = 0xFFFF, s_fifoMax = 0;
static volatile uint32_t s_underruns = 0, s_frames = 0;
static volatile uint32_t s_rateSets = 0, s_altSets = 0;   // control-request activity; printed from loop()
extern "C" volatile uint32_t g_uac_activate_calls, g_uac_activate_fail, g_uac_iso_alloc_fail;   // from audio_device.c
// Nothing prints from USB callbacks: USBSerial.write from inside the usbd task can wait on
// a CDC transfer that only the usbd task itself would complete.

// ---- Configuration descriptor contribution ---------------------------------------------
// Called by the core while it assembles the configuration descriptor. Interfaces are
// numbered in enum order (CDC first), so *itf is already past the console.
static uint16_t loadUacDescriptor(uint8_t* dst, uint8_t* itf) {
  uint8_t str = tinyusb_add_string_descriptor("Speaker DSP");
  s_epOut = tinyusb_get_free_out_endpoint();
  s_epFb  = tinyusb_get_free_in_endpoint();
  s_itfStreaming = *itf + 1;
  uint8_t desc[] = {
    UAC1_SPEAKER_STEREO_FB_DESCRIPTOR(*itf, str, UAC_BYTES, UAC_BITS, s_epOut, UAC_EP_SZ, (uint8_t)(0x80 | s_epFb), UAC_RATE)
  };
  static_assert(sizeof(desc) == UAC1_SPEAKER_STEREO_FB_DESC_LEN(1), "descriptor length mismatch");
  *itf += 2;
  memcpy(dst, desc, sizeof(desc));
  return sizeof(desc);
}

// ---- UAC1 control requests --------------------------------------------------------------
// Windows will not open the endpoint unless mute/volume/sampling-frequency all answer.
extern "C" bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const* req, uint8_t* buf) {
  (void)rhport;
  if (TU_U16_HIGH(req->wValue) == AUDIO10_EP_CTRL_SAMPLING_FREQ && req->bRequest == AUDIO10_CS_REQ_SET_CUR) {
    TU_VERIFY(req->wLength == 3);
    s_rate = tu_unaligned_read32(buf) & 0x00FFFFFF;
    s_rateSets++;
    return true;
  }
  return false;
}

extern "C" bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const* req) {
  if (TU_U16_HIGH(req->wValue) == AUDIO10_EP_CTRL_SAMPLING_FREQ && req->bRequest == AUDIO10_CS_REQ_GET_CUR) {
    uint8_t f[3] = { (uint8_t)s_rate, (uint8_t)(s_rate >> 8), (uint8_t)(s_rate >> 16) };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, f, 3);
  }
  return false;
}

extern "C" bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req, uint8_t* buf) {
  (void)rhport;
  uint8_t ch = TU_U16_LOW(req->wValue), sel = TU_U16_HIGH(req->wValue), ent = TU_U16_HIGH(req->wIndex);
  if (ent != UAC1_ENTITY_FEATURE_UNIT || req->bRequest != AUDIO10_CS_REQ_SET_CUR || ch > UAC_CH) return false;
  if (sel == AUDIO10_FU_CTRL_MUTE)   { TU_VERIFY(req->wLength == 1); s_mute[ch] = buf[0]; return true; }
  if (sel == AUDIO10_FU_CTRL_VOLUME) { TU_VERIFY(req->wLength == 2); s_volume[ch] = (int16_t)tu_unaligned_read16(buf) / 256; return true; }
  return false;
}

extern "C" bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req) {
  uint8_t ch = TU_U16_LOW(req->wValue), sel = TU_U16_HIGH(req->wValue), ent = TU_U16_HIGH(req->wIndex);
  if (ent != UAC1_ENTITY_FEATURE_UNIT || ch > UAC_CH) return false;
  if (sel == AUDIO10_FU_CTRL_MUTE) return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &s_mute[ch], 1);
  if (sel == AUDIO10_FU_CTRL_VOLUME) {
    int16_t v;
    switch (req->bRequest) {
      case AUDIO10_CS_REQ_GET_CUR: v = s_volume[ch] * 256; break;
      case AUDIO10_CS_REQ_GET_MIN: v = -60 * 256;          break;
      case AUDIO10_CS_REQ_GET_MAX: v = 0;                  break;
      case AUDIO10_CS_REQ_GET_RES: v = 256;                break;   // 1 dB steps
      default: return false;
    }
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &v, sizeof v);
  }
  return false;
}

// ---- RX FIFO experiment (UAC_RXFIFO_HACK) ---------------------------------------------
// TinyUSB's DWC2 driver sizes the RX FIFO for 2 x the largest OUT packet + 30 words, and on
// the S3 (256 words total) that refuses 24/96's 582-byte packet at configuration time
// (measured: iso_alloc_fail=1, zero packets received, Windows streams happily into the
// void). The hardware needs only 1 x the packet + status: the ISR drains a packet before
// the next 1 ms frame. This pokes GRXFSIZ up to the bottom of the lowest TX FIFO after
// enumeration — i.e. what a patched driver would do — to measure whether 1x is enough.
#if UAC_RXFIFO_HACK
#include "portable/synopsys/dwc2/dwc2_type.h"
static volatile uint32_t s_rxfifoWords = 0, s_rxfifoApplied = 0;
static void enlargeRxFifo() {
  dwc2_regs_t* dwc2 = (dwc2_regs_t*)0x60080000UL;          // DWC2_FS_REG_BASE on the S3
  // Every TX FIFO sits above its start address; the RX FIFO may use everything below the
  // lowest one. 1 x 582 B needs 146 + ~30 words; the TX FIFOs (EP0 16, CDC 16+2, fb 1)
  // leave 221, so it fits with room.
  uint16_t lowest = dwc2->dieptxf0 & 0xFFFF;
  for (int n = 0; n < 6; n++) {
    uint32_t f = dwc2->dieptxf[n];
    if ((f >> 16) && (f & 0xFFFF) < lowest) lowest = f & 0xFFFF;
  }
  uint16_t want = lowest;
  s_rxfifoWords = want;
  if ((dwc2->grxfsiz & 0xFFFF) >= want) return;
  dwc2->grxfsiz = want;
  dwc2->grstctl = GRSTCTL_RXFFLSH;                          // flush so the new size takes effect
  while (dwc2->grstctl & GRSTCTL_RXFFLSH_Msk) {}
  s_rxfifoApplied++;
}
#endif

extern "C" bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const* req) {
  (void)rhport;
  uint8_t itf = tu_u16_low(req->wIndex), alt = tu_u16_low(req->wValue);
  if (itf == s_itfStreaming) {
#if UAC_RXFIFO_HACK
    if (alt != 0) enlargeRxFifo();                          // endpoint idle: host starts ISO after the status stage
#endif
    s_streaming = (alt != 0);
    if (s_streaming) { s_fifoMin = 0xFFFF; s_fifoMax = 0; }
    s_altSets++;
  }
  return true;
}

extern "C" bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const* req) {
  (void)rhport;
  if (tu_u16_low(req->wIndex) == s_itfStreaming) s_streaming = false;
  return true;
}

// Feedback: let the driver regulate its own FIFO to half full and derive Ff from that.
// No SOF interrupt, no MCLK counter needed — the I2S clock is free-running and the host
// simply follows the fill level.
extern "C" void tud_audio_feedback_params_cb(uint8_t func, uint8_t alt, audio_feedback_params_t* p) {
  (void)func; (void)alt;
  p->method      = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
  p->sample_freq = s_rate;
}

// ISR context: one call per received ISO packet.
extern "C" bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n, uint8_t func, uint8_t ep, uint8_t alt) {
  (void)rhport; (void)func; (void)ep; (void)alt;
  s_pkts++;
  s_bytes += n;
  if (n < (UAC_RATE / 1000) * UAC_BYTES * UAC_CH) s_short++;   // fewer bytes than a nominal frame
  uint16_t a = tud_audio_available();
  s_fifoNow = a;
  if (a < s_fifoMin) s_fifoMin = a;
  if (a > s_fifoMax) s_fifoMax = a;
  return true;
}

// ---- Audio path: USB FIFO -> I2S ---------------------------------------------------------
// Pulls one millisecond of frames at a time and writes them to the DAC as 32-bit slots
// (PCM5102 takes 24-in-32 left-aligned). When the FIFO is empty it writes silence, so
// the DAC keeps clocking and the I2S DMA — not this loop — sets the pace.
static void audioTask(void*) {
  constexpr size_t FRAMES_PER_MS = UAC_RATE / 1000;
  constexpr size_t IN_BYTES      = FRAMES_PER_MS * UAC_BYTES * UAC_CH;
  static uint8_t  in[IN_BYTES];
  static int32_t  out[FRAMES_PER_MS * UAC_CH];

  for (;;) {
    if (s_streaming && tud_audio_available() >= IN_BYTES) {
      tud_audio_read(in, IN_BYTES);
      const uint8_t* p = in;
      for (size_t i = 0; i < FRAMES_PER_MS * UAC_CH; i++, p += UAC_BYTES) {
#if UAC_BYTES == 3
        out[i] = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24);
#else
        out[i] = (int32_t)((uint32_t)p[0] << 16 | (uint32_t)p[1] << 24);
#endif
      }
      s_frames += FRAMES_PER_MS;
    } else {
      if (s_streaming) s_underruns++;
      memset(out, 0, sizeof out);
    }
    i2s.write((uint8_t*)out, sizeof out);      // blocks on DMA: this is the clock
  }
}

// ---- Arduino -----------------------------------------------------------------------------
void setup() {
  pinMode(PIN_MUTE, OUTPUT);
  digitalWrite(PIN_MUTE, HIGH);                // muted until the DAC is clocking

  // One PID per format: Windows keys the audio endpoint's cached properties on the device
  // instance, and a device that re-enumerates with a different format under the SAME id is
  // refused with AUDCLNT_E_UNSUPPORTED_FORMAT at pin creation even though IsFormatSupported
  // says yes (measured 2026-09-15: 24/48 OK first, then 16/48 refused until the PID changed).
  USB.PID(0xA000 | (UAC_BYTES << 8) | (UAC_RATE / 1000));
  static char name[32];
  snprintf(name, sizeof name, "Speaker DSP %u/%lu", UAC_BITS, (unsigned long)UAC_RATE / 1000);
  USB.productName(name);
  USB.manufacturerName("sap");
  USBSerial.begin();
  tinyusb_enable_interface(USB_INTERFACE_CUSTOM, UAC1_SPEAKER_STEREO_FB_DESC_LEN(1), loadUacDescriptor);
  USB.begin();

  i2s.setPins(I2S0_BCLK, I2S0_LRCK, I2S0_DOUT, -1, -1);
  bool ok = i2s.begin(I2S_MODE_STD, UAC_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  digitalWrite(PIN_MUTE, LOW);

  delay(2000);                                 // give the host time to open the console
  USBSerial.printf("\n[uac] test build %s %u-bit/%lu Hz, ep out 0x%02x fb 0x%02x, wMaxPacketSize %u, i2s %s\n",
                   __DATE__ " " __TIME__, UAC_BITS, (unsigned long)UAC_RATE, s_epOut, 0x80 | s_epFb, UAC_EP_SZ, ok ? "ok" : "FAILED");
}

void loop() {
  static uint32_t lastPkts = 0, lastFrames = 0;
  delay(1000);
  uint32_t pk = s_pkts, fr = s_frames;
  USBSerial.printf("[uac] %s mounted=%d stream=%d rate=%lu | pkts/s %lu (short %lu) | fifo now %u min %u max %u of %u | frames/s %lu underrun %lu | mute %u vol %d dB | setrate %lu setalt %lu | iso_alloc_fail %lu activate %lu/%lu fail | rxfifo %lu words (applied %lu)\n",
                   s_streaming ? "RUN " : "idle", tud_audio_mounted(), s_streaming, (unsigned long)s_rate,
                   (unsigned long)(pk - lastPkts), (unsigned long)s_short,
                   s_fifoNow, s_fifoMin == 0xFFFF ? 0 : s_fifoMin, s_fifoMax, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ,
                   (unsigned long)(fr - lastFrames), (unsigned long)s_underruns, s_mute[0], s_volume[0], (unsigned long)s_rateSets, (unsigned long)s_altSets,
                   (unsigned long)g_uac_iso_alloc_fail, (unsigned long)g_uac_activate_calls, (unsigned long)g_uac_activate_fail,
                   (unsigned long)UAC_RXFIFO_WORDS, (unsigned long)UAC_RXFIFO_APPLIED);
  lastPkts = pk; lastFrames = fr;
  if (s_streaming) { s_fifoMin = 0xFFFF; s_fifoMax = 0; }
}
