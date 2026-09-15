// dsp_engine.ino
// -----------------------------------------------------------------------------
// Active-crossover DSP engine for ESP32-S3 + 2x PCM5102 + 4x TPA3118.
//
// STATUS: DSP chain, BLE control, NVS persistence, 48k/96k profile switch,
// presets, boot-profile restore and GET_PARAMS read-back were all VERIFIED ON
// HARDWARE (2026-08-23, 2026-09-15). The USB audio source merged in on
// 2026-09-15 was VERIFIED ON HARDWARE the same afternoon: 24/48 and 24/96
// streamed through the full chain with the app connected over BLE, the rate
// followed the host, and 44 biquads @ 96 k measured 75.3-75.9 % with USB
// live (74.9 % without). The four-GPIO amp mute is compiled only until the
// NPNs are wired.
//
// What it does:
//   * Presents itself to the PC as a USB Audio Class 1.0 stereo speaker
//     ("Speaker DSP", 24-bit, 48 k and 96 k, asynchronous with a feedback
//     endpoint) plus a CDC console. The PC's audio stream IS the input.
//   * Runs the full chain: Voicing EQ (stereo) -> LR4 crossover -> per-driver
//     EQ / level / delay -> 4 outputs.
//   * LOW band  -> I2S0 -> PCM5102 LOW  (L=L-woofer,  R=R-woofer).
//     HIGH band -> I2S1 -> PCM5102 HIGH (L=L-tweeter, R=R-tweeter).
//     Both DACs are fed 24-in-32-bit slots.
//   * The SAMPLE RATE FOLLOWS THE HOST: when Windows selects 48 k or 96 k the
//     engine re-clocks I2S and re-designs the biquads for that rate. The app's
//     High-Res toggle still works while nothing is streaming (bench use) and is
//     refused (ACK 3) while a USB stream owns the rate.
//   * Built-in PROFILER: measures real cycles/sample and cycles/biquad on the
//     S3 and prints them alongside the USB stream counters.
//   * IDLE_TEST_TONES: the original two-tone generator (200 Hz + 5 kHz), off by
//     default, for a bench without a PC stream.
//
// Cores: audio on core 1 (dedicated). USB.begin() is called from a task pinned
// to core 0 so the USB interrupt and the usbd task land there, next to BLE.
// Amps held muted at boot, un-muted after I2S is up (pop-free; see WIRING.md).
//
// BUILD (USB-OTG mode, CDC not on boot — the audio interface must be registered
// before USB.begin(), and CDCOnBoot=cdc starts USB before setup() runs):
//   arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default firmware/dsp_engine
// FLASH: the console is a TinyUSB CDC port; esptool's reset dance does not trip
// it reliably. Run tools/uactool/cdcboot.ps1 twice (drops to the ROM bootloader,
// which enumerates as COM3 here), then upload with -p COM3. Or hold BOOT.
//
// REQUIRES: ESP32 Arduino core >= 3.0 (ESP_I2S / I2SClass).
// PCM5102 straps: SCK->GND, FMT->GND, XSMT->3V3, FLT->GND, DEMP->GND.
// -----------------------------------------------------------------------------

#include "uac_config.h"          // must precede every TinyUSB header
#include <ESP_I2S.h>
#include <atomic>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "USB.h"
#include "USBCDC.h"
#include "esp32-hal-tinyusb.h"
#include "class/audio/audio_device.h"
#include "device/usbd_pvt.h"
#include "portable/synopsys/dwc2/dwc2_type.h"
#include "uac_desc.h"
#include "dsp_params.h"
#include "protocol.h"

// Console = the TinyUSB CDC interface. The core only instantiates USBSerial when
// CDCOnBoot=cdc, and with it off `Serial` is the UART on GPIO43/44 that nothing
// listens to; point it at our own CDC object instead. Writes are dropped while
// no terminal holds DTR, so logging never blocks.
USBCDC USBSerial(0);
#undef  Serial
#define Serial USBSerial

#ifndef IDLE_TEST_TONES
#define IDLE_TEST_TONES 0        // 1 = play the two-tone test signal whenever USB is idle
#endif
#pragma GCC optimize ("O3")   // DSP hot path: optimize for speed, not size

// ---- Pins (match WIRING.md / schematic.html) --------------------------------
constexpr int8_t I2S0_BCLK = 4,  I2S0_LRCK = 5,  I2S0_DOUT = 6;   // LOW DAC
constexpr int8_t I2S1_BCLK = 7,  I2S1_LRCK = 8,  I2S1_DOUT = 9;   // HIGH DAC
// Amp mute: one NPN per TPA3118 MUTE header, one GPIO per NPN (WIRING.md, decided
// 2026-09-15) so channels can be muted singly during bring-up; the engine gangs
// them. GPIO high = transistor on = header shorted. Whether "shorted" is mute or
// un-mute is measured per board; MUTE_ACTIVE_HIGH flips the whole set at once.
constexpr int8_t PIN_MUTE[4] = {10, 11, 12, 13};                   // Lw, Rw, Lt, Rt
constexpr bool   MUTE_ACTIVE_HIGH = true;

// ---- Engine config ----------------------------------------------------------
constexpr uint32_t SAMPLE_RATE = 48000;   // 48000 Normal / 96000 High-Res
constexpr size_t   FRAMES      = 128;     // stereo frames per DMA write
constexpr float    TONE_LO_HZ  = 200.0f;
constexpr float    TONE_HI_HZ  = 5000.0f;
constexpr float    TONE_AMP    = 0.20f;   // each tone; sum stays < 1.0

// ---- LR4 constant: two cascaded Butterworth sections, Q = 1/sqrt(2) ---------
constexpr float BUTTER_Q = 0.70710678f;

I2SClass i2sLow;
I2SClass i2sHigh;

Params params;   // live parameters (written by BLE app, restored from NVS)

// Runtime rate/mute state. The profile switch changes the sample rate at
// runtime, so the rate can no longer be the compile-time SAMPLE_RATE alone.
volatile uint32_t g_sampleRate  = SAMPLE_RATE;   // rate the engine is running at now
volatile uint32_t g_pendingRate = 0;             // audio task applies this between blocks (0 = none)
volatile bool     g_userMuted   = false;         // user mute intent (survives a profile switch)
volatile bool     g_rateChanged = false;         // audio task -> loop(): push EVT_STATUS after a switch
volatile bool     g_dumpParams  = false;         // BLE handler -> loop(): stream the param read-back
static Profile    g_profile     = PROFILE_NORMAL; // derived from g_sampleRate; reported in EVT_STATUS
SemaphoreHandle_t g_rebuildMux  = nullptr;       // serializes rebuild() across the two cores

// ---- USB audio source state -------------------------------------------------
// Written from the usbd task / USB ISR (core 0), read by the audio task and loop().
static uint8_t   g_epOut = 0, g_epFb = 0;
static uint8_t   g_itfStreaming = 0xFF;          // AS interface number, learnt from the descriptor cb
volatile bool     g_usbStreaming = false;        // host selected the streaming alt setting
volatile uint32_t g_usbRate      = SAMPLE_RATE;  // last SET_CUR sampling frequency from the host
volatile uint32_t g_usbPkts = 0, g_usbUnderruns = 0, g_usbRateSets = 0;
volatile uint16_t g_usbFifoNow = 0;
static uint8_t    g_usbMute[UAC_CH + 1];
static int16_t    g_usbVolume[UAC_CH + 1];       // dB as the host set them; not applied (master gain is the app's)
extern "C" volatile uint32_t g_uac_iso_alloc_fail;   // from audio_device.c: RX FIFO allocation result at configuration

// ---- Compiled coefficient sets (rebuilt from `params`) ----------------------
struct DelayLine {
  static constexpr int MAXD = 1024;       // ~21 ms @ 48k, ~10 ms @ 96k
  float buf[MAXD];
  int   w = 0, n = 0;
  void set(int samples) { n = samples < 0 ? 0 : (samples >= MAXD ? MAXD - 1 : samples); }
  inline float process(float x) {
    if (n == 0) return x;
    buf[w] = x;
    int r = w - n; if (r < 0) r += MAXD;
    w = (w + 1) % MAXD;
    return buf[r];
  }
  void reset() { for (int i = 0; i < MAXD; i++) buf[i] = 0; w = 0; }
};

struct Compiled {
  // voicing (stereo: separate state per side, shared design)
  Biquad voicingL[MAX_VOICING_BANDS], voicingR[MAX_VOICING_BANDS];
  int    voicingCount = 0;
  float  preampLin = 1.0f;
  // crossover LR4: 2 biquads each, per side
  Biquad lpL[2], hpL[2], lpR[2], hpR[2];
  // per-driver EQ + level + delay (idx 0..3 = Lw,Rw,Lt,Rt)
  Biquad    drvEq[NUM_DRIVERS][MAX_DRIVER_BANDS];
  int       drvEqCount[NUM_DRIVERS] = {0, 0, 0, 0};
  float     drvLevelLin[NUM_DRIVERS] = {1, 1, 1, 1};
  DelayLine drvDelay[NUM_DRIVERS];
  float     masterLin = 0.5f;
  uint32_t  activeBiquads = 0;
};

// Double buffer + atomic pointer. The audio core (core 1) reads the active
// buffer; rebuild() (called only from the BLE task on core 0, or setup) builds
// into the inactive buffer then atomically swaps. No lock, no torn reads.
// Note: a rebuild resets biquad state (z=0), so a param change causes a brief
// transient — acceptable for v1; a later rev can migrate state or crossfade.
Compiled Cbuf[2];
std::atomic<Compiled*> Cactive{ &Cbuf[0] };

static inline float dB2lin(float db) { return powf(10.0f, db / 20.0f); }
// Kept below the first function on purpose: the Arduino prototype generator
// inserts every prototype before the first function definition in the file,
// and those prototypes reference `Compiled`, so the first function must come
// after `struct Compiled`.
static bool validRate(uint32_t r) { return r == 48000 || r == 96000; }

static void buildInto(Compiled& C, const Params& p) {
  C = Compiled{};                 // fresh coefficients + zeroed state
  float fs = (float)p.sampleRate;
  uint32_t bq = 0;

  C.voicingCount = 0;
  for (int i = 0; i < MAX_VOICING_BANDS; i++) {
    if (!p.voicing[i].enabled) continue;
    const EqBand& b = p.voicing[i];
    designBiquad(C.voicingL[C.voicingCount], b.type, fs, b.f, b.Q, b.gainDb);
    designBiquad(C.voicingR[C.voicingCount], b.type, fs, b.f, b.Q, b.gainDb);
    C.voicingCount++;
  }
  C.preampLin = dB2lin(p.voicingPreampDb);
  bq += C.voicingCount * 2;

  for (int s = 0; s < 2; s++) {
    designBiquad(C.lpL[s], FT_LOWPASS,  fs, p.crossoverHz, BUTTER_Q, 0);
    designBiquad(C.hpL[s], FT_HIGHPASS, fs, p.crossoverHz, BUTTER_Q, 0);
    designBiquad(C.lpR[s], FT_LOWPASS,  fs, p.crossoverHz, BUTTER_Q, 0);
    designBiquad(C.hpR[s], FT_HIGHPASS, fs, p.crossoverHz, BUTTER_Q, 0);
  }
  bq += 8;

  for (int d = 0; d < NUM_DRIVERS; d++) {
    C.drvEqCount[d] = 0;
    for (int i = 0; i < MAX_DRIVER_BANDS; i++) {
      if (!p.driver[d].eq[i].enabled) continue;
      const EqBand& b = p.driver[d].eq[i];
      designBiquad(C.drvEq[d][C.drvEqCount[d]], b.type, fs, b.f, b.Q, b.gainDb);
      C.drvEqCount[d]++;
    }
    bq += C.drvEqCount[d];
    C.drvLevelLin[d] = dB2lin(p.driver[d].levelDb);
    C.drvDelay[d].set(p.driver[d].delaySamples);
  }

  C.masterLin     = p.masterGain;
  C.activeBiquads = bq;
}

// Build into the inactive buffer, then publish it atomically. Mutex-guarded so
// the BLE core (param edits) and the audio core (profile rate switch) can't both
// rebuild at once. The hold is ~tens of µs — negligible vs a block period.
void rebuild(const Params& p) {
  if (g_rebuildMux) xSemaphoreTake(g_rebuildMux, portMAX_DELAY);
  Compiled* cur = Cactive.load(std::memory_order_relaxed);
  Compiled* nxt = (cur == &Cbuf[0]) ? &Cbuf[1] : &Cbuf[0];
  buildInto(*nxt, p);
  Cactive.store(nxt, std::memory_order_release);
  if (g_rebuildMux) xSemaphoreGive(g_rebuildMux);
}

// ---- The hot path: one stereo input frame -> 4 driver outputs ---------------
static inline void processFrame(Compiled& C, float xL, float xR, float out[4]) {
  float vl = xL * C.preampLin, vr = xR * C.preampLin;
  for (int i = 0; i < C.voicingCount; i++) {
    vl = C.voicingL[i].process(vl);
    vr = C.voicingR[i].process(vr);
  }
  // LR4 split
  float loL = C.lpL[1].process(C.lpL[0].process(vl));
  float hiL = C.hpL[1].process(C.hpL[0].process(vl));
  float loR = C.lpR[1].process(C.lpR[0].process(vr));
  float hiR = C.hpR[1].process(C.hpR[0].process(vr));
  float d[4] = { loL, loR, hiL, hiR };     // Lw, Rw, Lt, Rt
  for (int k = 0; k < 4; k++) {
    float y = d[k];
    for (int i = 0; i < C.drvEqCount[k]; i++) y = C.drvEq[k][i].process(y);
    y = C.drvDelay[k].process(y);
    out[k] = y * C.drvLevelLin[k] * C.masterLin;
  }
}

// 24-bit sample left-aligned in a 32-bit I2S slot (the PCM5102 uses the top 24).
// Scaled to 2^23-1 and shifted so a float rounding excursion at +1.0 cannot
// overflow the int conversion.
static inline int32_t f2s32(float x) {
  if (x >  1.0f) x =  1.0f;
  if (x < -1.0f) x = -1.0f;
  return (int32_t)(x * 8388607.0f) << 8;
}

// USB 24-bit packed little-endian -> float in [-1, 1).
static inline float s24f(const uint8_t* p) {
  int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24);
  return (float)v * (1.0f / 2147483648.0f);
}

// ---- Shared profiler stats (core1 writes, core0 prints) ---------------------
volatile float    g_cyclesPerSample = 0;
volatile uint32_t g_activeBiquads   = 0;

void setAmpsMuted(bool m) {
  bool level = MUTE_ACTIVE_HIGH ? m : !m;
  for (int8_t pin : PIN_MUTE) digitalWrite(pin, level ? HIGH : LOW);
}

// ---- Audio task (pinned to core 1) ------------------------------------------
void audioTask(void*) {
  static int32_t lowBuf[FRAMES * 2];    // L=Lw, R=Rw  -> I2S0 (24-in-32 slots)
  static int32_t highBuf[FRAMES * 2];   // L=Lt, R=Rt  -> I2S1
  static float   xL[FRAMES], xR[FRAMES];// input block (filled outside the timed region)
  static uint8_t usbIn[FRAMES * UAC_FRAME_BYTES];
#if IDLE_TEST_TONES
  float phaseLo = 0, phaseHi = 0;
  float incLo = 2.0f * (float)M_PI * TONE_LO_HZ / g_sampleRate;
  float incHi = 2.0f * (float)M_PI * TONE_HI_HZ / g_sampleRate;
#endif

  for (;;) {
    // --- apply a pending profile / sample-rate switch (owned by this core) ---
    uint32_t pend = g_pendingRate;
    if (pend) {
      if (pend != g_sampleRate) {
        setAmpsMuted(true);                 // pop-free: mute across the reconfig
        i2sLow.end();  i2sHigh.end();
        params.sampleRate = pend;
        rebuild(params);                    // recompute coeffs for the new fs
        i2sLow.setPins (I2S0_BCLK, I2S0_LRCK, I2S0_DOUT, -1, -1);
        i2sHigh.setPins(I2S1_BCLK, I2S1_LRCK, I2S1_DOUT, -1, -1);
        i2sLow.begin (I2S_MODE_STD, pend, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
        i2sHigh.begin(I2S_MODE_STD, pend, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
        g_sampleRate = pend;
        g_profile    = (pend == 96000) ? PROFILE_HIRES : PROFILE_NORMAL;
#if IDLE_TEST_TONES
        incLo = 2.0f * (float)M_PI * TONE_LO_HZ / pend;
        incHi = 2.0f * (float)M_PI * TONE_HI_HZ / pend;
#endif
        delay(80);                          // let the DACs relock
        setAmpsMuted(g_userMuted);          // restore the user's mute intent
        g_rateChanged = true;               // loop() (core 0) notifies the app; no BLE from this core
      }
      g_pendingRate = 0;
    }

    // --- untimed: fetch one block of input ---
    // USB frames come out of the class driver's software FIFO, which the feedback
    // endpoint keeps regulated to half full, so a full block is normally waiting.
    // Anything else (no stream, or a momentary shortfall) is silence — never a
    // partial block, which would shift the stream against the DAC clock.
    bool haveUsb = g_usbStreaming && g_usbRate == g_sampleRate
                && tud_audio_available() >= sizeof(usbIn);
    if (haveUsb) {
      tud_audio_read(usbIn, sizeof(usbIn));
      const uint8_t* p = usbIn;
      for (size_t f = 0; f < FRAMES; f++, p += UAC_FRAME_BYTES) {
        xL[f] = s24f(p);
        xR[f] = s24f(p + UAC_BYTES);
      }
    } else {
      if (g_usbStreaming) g_usbUnderruns++;
#if IDLE_TEST_TONES
      for (size_t f = 0; f < FRAMES; f++) {
        xL[f] = xR[f] = TONE_AMP * (sinf(phaseLo) + sinf(phaseHi));
        phaseLo += incLo; if (phaseLo >= 2 * M_PI) phaseLo -= 2 * M_PI;
        phaseHi += incHi; if (phaseHi >= 2 * M_PI) phaseHi -= 2 * M_PI;
      }
#else
      memset(xL, 0, sizeof xL);
      memset(xR, 0, sizeof xR);
#endif
    }

    // Grab the active coefficient buffer once per block (atomic; a BLE param
    // update swaps it between blocks, never mid-block).
    Compiled& C = *Cactive.load(std::memory_order_acquire);

    // --- timed: pure DSP (processFrame + int conversion), no input handling ---
    uint32_t t0 = ESP.getCycleCount();
    for (size_t f = 0; f < FRAMES; f++) {
      float out[4];
      processFrame(C, xL[f], xR[f], out);
      lowBuf[2 * f]      = f2s32(out[0]);   // L-woofer
      lowBuf[2 * f + 1]  = f2s32(out[1]);   // R-woofer
      highBuf[2 * f]     = f2s32(out[2]);   // L-tweeter
      highBuf[2 * f + 1] = f2s32(out[3]);   // R-tweeter
    }
    uint32_t dt = ESP.getCycleCount() - t0;   // uint32 wrap is fine
    g_cyclesPerSample = (float)dt / FRAMES;
    g_activeBiquads   = C.activeBiquads;

    // Blocking writes pace the loop to the sample rate.
    i2sLow.write((uint8_t*)lowBuf,  sizeof(lowBuf));
    i2sHigh.write((uint8_t*)highBuf, sizeof(highBuf));
  }
}

// =============================================================================
// USB audio source (UAC 1.0) — descriptor, control requests, feedback, RX FIFO
// =============================================================================
// The class driver (audio_device.c) is registered in uac_glue.cpp. Everything
// here runs in the usbd task or the USB ISR on core 0; it only sets flags and
// counters — no Serial, no BLE, no I2S from these callbacks.

// Configuration-descriptor contribution, called while the core assembles the
// descriptor. Interfaces are numbered in enum order (CDC first), so *itf is
// already past the console.
static uint16_t loadUacDescriptor(uint8_t* dst, uint8_t* itf) {
  uint8_t str = tinyusb_add_string_descriptor("Speaker DSP");
  g_epOut = tinyusb_get_free_out_endpoint();
  g_epFb  = tinyusb_get_free_in_endpoint();
  g_itfStreaming = *itf + 1;
  uint8_t desc[] = {
    UAC1_SPEAKER_STEREO_FB_DESCRIPTOR(*itf, str, UAC_BYTES, UAC_BITS, g_epOut, UAC_EP_SZ,
                                      (uint8_t)(0x80 | g_epFb), 48000, 96000)
  };
  static_assert(sizeof(desc) == UAC1_SPEAKER_STEREO_FB_DESC_LEN(2), "descriptor length mismatch");
  *itf += 2;
  memcpy(dst, desc, sizeof(desc));
  return sizeof(desc);
}

// RX FIFO: TinyUSB's DWC2 driver wants 2 x (largest OUT packet) + 30 words and the
// S3 has 256 in total, so the 582-byte endpoint's allocation fails silently at
// configuration and every ISO packet would be dropped (measured 2026-09-15 in
// uac_test: iso_alloc_fail=1, 0 packets). 1 x the packet is enough — the ISR
// drains it well inside the 1 ms frame — so grow GRXFSIZ up to the bottom of
// the lowest TX FIFO when the host opens the stream. Must be re-done after every
// bus reset: the driver re-initialises the register to 62 words. 24/96 verified
// with exactly this on 2026-09-15 (96000 frames/s, 0 underruns over 10 s).
static void enlargeRxFifo() {
  dwc2_regs_t* dwc2 = (dwc2_regs_t*)0x60080000UL;          // DWC2_FS_REG_BASE on the S3
  uint16_t lowest = dwc2->dieptxf0 & 0xFFFF;                // every TX FIFO sits above its start
  for (int n = 0; n < 6; n++) {
    uint32_t f = dwc2->dieptxf[n];
    if ((f >> 16) && (f & 0xFFFF) < lowest) lowest = f & 0xFFFF;
  }
  if ((dwc2->grxfsiz & 0xFFFF) >= lowest) return;
  dwc2->grxfsiz = lowest;
  dwc2->grstctl = GRSTCTL_RXFFLSH;                          // flush so the new size takes effect
  while (dwc2->grstctl & GRSTCTL_RXFFLSH_Msk) {}
}

// Sampling frequency (UAC1: an endpoint control). The host's choice becomes the
// engine's rate: the audio task performs the same mute -> re-clock -> re-design
// switch the app's profile toggle uses. The driver re-prepares the feedback
// parameters right after this callback, so the new rate is what it regulates to.
extern "C" bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const* req, uint8_t* buf) {
  (void)rhport;
  if (TU_U16_HIGH(req->wValue) == AUDIO10_EP_CTRL_SAMPLING_FREQ && req->bRequest == AUDIO10_CS_REQ_SET_CUR) {
    TU_VERIFY(req->wLength == 3);
    uint32_t r = tu_unaligned_read32(buf) & 0x00FFFFFF;
    if (!validRate(r)) return false;
    g_usbRate = r;
    g_usbRateSets++;
    if (r != g_sampleRate) g_pendingRate = r;
    return true;
  }
  return false;
}

extern "C" bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const* req) {
  if (TU_U16_HIGH(req->wValue) == AUDIO10_EP_CTRL_SAMPLING_FREQ && req->bRequest == AUDIO10_CS_REQ_GET_CUR) {
    uint32_t r = g_usbRate;
    uint8_t f[3] = { (uint8_t)r, (uint8_t)(r >> 8), (uint8_t)(r >> 16) };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, f, 3);
  }
  return false;
}

// Feature unit: Windows will not open the endpoint unless mute and volume answer.
// The values are accepted and remembered but not applied — level lives in the
// app's master gain, and Windows attenuates in software in shared mode anyway.
extern "C" bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req, uint8_t* buf) {
  (void)rhport;
  uint8_t ch = TU_U16_LOW(req->wValue), sel = TU_U16_HIGH(req->wValue), ent = TU_U16_HIGH(req->wIndex);
  if (ent != UAC1_ENTITY_FEATURE_UNIT || req->bRequest != AUDIO10_CS_REQ_SET_CUR || ch > UAC_CH) return false;
  if (sel == AUDIO10_FU_CTRL_MUTE)   { TU_VERIFY(req->wLength == 1); g_usbMute[ch] = buf[0]; return true; }
  if (sel == AUDIO10_FU_CTRL_VOLUME) { TU_VERIFY(req->wLength == 2); g_usbVolume[ch] = (int16_t)tu_unaligned_read16(buf) / 256; return true; }
  return false;
}

extern "C" bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req) {
  uint8_t ch = TU_U16_LOW(req->wValue), sel = TU_U16_HIGH(req->wValue), ent = TU_U16_HIGH(req->wIndex);
  if (ent != UAC1_ENTITY_FEATURE_UNIT || ch > UAC_CH) return false;
  if (sel == AUDIO10_FU_CTRL_MUTE) return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &g_usbMute[ch], 1);
  if (sel == AUDIO10_FU_CTRL_VOLUME) {
    int16_t v;
    switch (req->bRequest) {
      case AUDIO10_CS_REQ_GET_CUR: v = g_usbVolume[ch] * 256; break;
      case AUDIO10_CS_REQ_GET_MIN: v = -60 * 256;             break;
      case AUDIO10_CS_REQ_GET_MAX: v = 0;                     break;
      case AUDIO10_CS_REQ_GET_RES: v = 256;                   break;   // 1 dB steps
      default: return false;
    }
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &v, sizeof v);
  }
  return false;
}

extern "C" bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const* req) {
  (void)rhport;
  uint8_t itf = tu_u16_low(req->wIndex), alt = tu_u16_low(req->wValue);
  if (itf == g_itfStreaming) {
    if (alt != 0) enlargeRxFifo();          // endpoint idle: the host starts ISO after the status stage
    g_usbStreaming = (alt != 0);
  }
  return true;
}

extern "C" bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const* req) {
  (void)rhport;
  if (tu_u16_low(req->wIndex) == g_itfStreaming) g_usbStreaming = false;
  return true;
}

// Feedback: the driver regulates its own FIFO to half full and derives Ff from
// that. No SOF interrupt, no MCLK counter — the I2S clock free-runs and the host
// follows the fill level.
extern "C" void tud_audio_feedback_params_cb(uint8_t func, uint8_t alt, audio_feedback_params_t* p) {
  (void)func; (void)alt;
  p->method      = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
  p->sample_freq = g_usbRate;
}

extern "C" bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n, uint8_t func, uint8_t ep, uint8_t alt) {
  (void)rhport; (void)n; (void)func; (void)ep; (void)alt;
  g_usbPkts++;
  g_usbFifoNow = tud_audio_available();
  return true;
}

// USB.begin() from a task pinned to core 0: the USB interrupt is allocated on the
// calling core, and the ISR is what matters — it reads every ISO packet out of
// the FIFO, and that belongs next to BLE on core 0, not on the audio core.
static void usbStartTask(void* done) {
  USB.begin();
  *(volatile bool*)done = true;
  vTaskDelete(nullptr);
}

static void setupUsb() {
  USB.VID(0x303A);                    // Espressif's VID, as the core defaults
  USB.PID(0xA3D5);                    // ours. Windows caches the audio endpoint per
                                      // VID/PID/serial: change the PID if the formats change.
  USB.productName("Speaker DSP");
  USB.manufacturerName("sap");
  USBSerial.begin();
  tinyusb_enable_interface(USB_INTERFACE_CUSTOM, UAC1_SPEAKER_STEREO_FB_DESC_LEN(2), loadUacDescriptor);
  volatile bool done = false;
  xTaskCreatePinnedToCore(usbStartTask, "usbstart", 4096, (void*)&done, 2, nullptr, 0);
  while (!done) delay(5);
}

// ---- Clean default config: crossover only, EQ off (the BLE app adds EQ) ------
void loadDefaultConfig(Params& p) {
  p.sampleRate  = SAMPLE_RATE;
  p.crossoverHz = 2500.0f;
  p.masterGain  = 0.5f;
  // voicing + per-driver EQ all disabled by default (EqBand defaults to off)
  p.driver[2].levelDb = -3.0f;   // tweeters padded down a touch, as in a real system
  p.driver[3].levelDb = -3.0f;
}

// ---- NVS persistence (Preferences) ------------------------------------------
// Params is POD (no pointers), so it stores as a raw blob. A version guard
// rejects a blob from an incompatible struct layout. NOTE: a flash write can
// briefly stall code fetched from flash on the other core -> a short audio blip
// on save is possible; saves are user-initiated and occasional, so acceptable.
Preferences prefs;
constexpr int NUM_PRESETS = 8;

// The blob carries sampleRate, and the coefficients MUST be designed for the rate
// I2S is actually running at:
//   * boot (forceRate = 0): keep the blob's rate — this is how the saved profile
//     is restored; setup() then brings I2S up at that rate directly.
//   * runtime LOAD_PRESET (forceRate = g_sampleRate): a preset saved at 96 k
//     loaded while running at 48 k (or vice versa) must be re-designed for the
//     current rate, otherwise every corner frequency lands at 2x / 0.5x. This
//     was a latent bug before 2026-09-15: the old code forced SAMPLE_RATE (48 k)
//     unconditionally, so a preset loaded in High-Res got 48 k coefficients.
static bool loadParamsBlob(const char* key, Params& p, uint32_t forceRate) {
  prefs.begin("dsp", true);
  bool ok = false;
  if (prefs.isKey(key)) {
    Params tmp;
    size_t n = prefs.getBytes(key, &tmp, sizeof(tmp));
    if (n == sizeof(tmp) && tmp.version == p.version) { p = tmp; ok = true; }
  }
  prefs.end();
  if (forceRate)               p.sampleRate = forceRate;
  else if (!validRate(p.sampleRate)) p.sampleRate = SAMPLE_RATE;   // corrupt/old blob -> safe default
  return ok;
}
// Persist with the *intended* rate: if a SET_PROFILE is still pending in the
// audio task, save that, not the rate we are about to leave.
static void saveParamsBlob(const char* key, const Params& p) {
  Params copy = p;
  uint32_t pend = g_pendingRate;
  copy.sampleRate = pend ? pend : (uint32_t)g_sampleRate;
  prefs.begin("dsp", false);
  prefs.putBytes(key, &copy, sizeof(copy));
  prefs.end();
}
static void presetKey(char* out, size_t n, int slot) { snprintf(out, n, "preset%d", slot); }

// =============================================================================
// BLE GATT server — device side of the frozen protocol (see PROTOCOL.md).
// Runs on core 0 (NimBLE host task). CMD writes mutate `params` and rebuild()
// (double-buffered handoff to the audio core). Emits EVT_HELLO/STATUS/ACK, and
// EVT_PARAM x N + EVT_PARAMS_DONE for the read-back. SET_PROFILE hands the rate
// switch to the audio task; NVS save/load is live (see the Preferences block).
// =============================================================================
static NimBLECharacteristic* g_evt = nullptr;


static float  rdF32(const uint8_t* d, int off) { float f; memcpy(&f, d + off, 4); return f; }
static uint16_t rdU16(const uint8_t* d, int off) { return (uint16_t)(d[off] | (d[off + 1] << 8)); }
static void wrF32(uint8_t* d, int off, float f) { memcpy(d + off, &f, 4); }

static bool sendEvt(const uint8_t* data, size_t len) {
  if (!g_evt) return false;
  g_evt->setValue(data, len);
  return g_evt->notify();
}
static void sendAck(uint8_t op, uint8_t result) {
  uint8_t b[3] = { EVT_ACK, op, result };
  sendEvt(b, sizeof(b));
}
static void sendHello() {
  uint8_t b[9];
  b[0] = EVT_HELLO;
  b[1] = FW_VERSION & 0xFF;       b[2] = FW_VERSION >> 8;
  b[3] = PROTOCOL_VERSION & 0xFF; b[4] = PROTOCOL_VERSION >> 8;
  b[5] = MAX_VOICING_BANDS; b[6] = MAX_DRIVER_BANDS; b[7] = NUM_DRIVERS; b[8] = 0;
  sendEvt(b, 8);
}
static void sendStatus() {
  uint32_t rate = g_sampleRate;
  uint8_t b[8] = { EVT_STATUS, (uint8_t)g_profile, (uint8_t)(g_userMuted ? 1 : 0),
                   (uint8_t)(rate), (uint8_t)(rate >> 8),
                   (uint8_t)(rate >> 16), (uint8_t)(rate >> 24), 0 /*linkFlags*/ };
  sendEvt(b, sizeof(b));
}

// ---- Read-back: stream every parameter as EVT_PARAM frames ------------------
// Called from loop() (core 0 Arduino task), NOT from the GATT write callback:
// 40 back-to-back notifies from inside the host task can overrun NimBLE's tx
// queue. A 3 ms gap per frame keeps it well under the connection interval.
// One retry per frame on a false notify(); a frame that still fails is counted
// as skipped and the app's 3 s timeout falls back to a push.
static uint32_t g_dumpCount = 0;
static void emitParam(const uint8_t* payload, size_t len) {
  uint8_t b[20];
  b[0] = EVT_PARAM;
  memcpy(b + 1, payload, len);
  if (!sendEvt(b, len + 1)) { delay(5); if (!sendEvt(b, len + 1)) return; }
  g_dumpCount++;
  delay(3);
}
static void emitBand(uint8_t op, int drv, int idx, const EqBand& e) {
  uint8_t p[18]; int o = 0;
  p[o++] = op;
  if (op == CMD_SET_DRIVER_EQ_BAND) p[o++] = (uint8_t)drv;
  p[o++] = (uint8_t)idx; p[o++] = e.enabled ? 1 : 0; p[o++] = (uint8_t)e.type;
  wrF32(p, o, e.f); o += 4; wrF32(p, o, e.Q); o += 4; wrF32(p, o, e.gainDb); o += 4;
  emitParam(p, o);
}
static void dumpParams() {
  g_dumpCount = 0;
  uint8_t p[8];
  // Profile first so the app knows the rate before it converts delay samples->ms.
  p[0] = CMD_SET_PROFILE;     p[1] = (uint8_t)g_profile;             emitParam(p, 2);
  p[0] = CMD_SET_MUTE;        p[1] = g_userMuted ? 1 : 0;            emitParam(p, 2);
  p[0] = CMD_SET_MASTER_GAIN; wrF32(p, 1, params.masterGain);        emitParam(p, 5);
  p[0] = CMD_SET_CROSSOVER_HZ;wrF32(p, 1, params.crossoverHz);       emitParam(p, 5);
  p[0] = CMD_SET_VOICING_PREAMP; wrF32(p, 1, params.voicingPreampDb); emitParam(p, 5);
  for (int i = 0; i < MAX_VOICING_BANDS; i++) emitBand(CMD_SET_VOICING_BAND, 0, i, params.voicing[i]);
  for (int d = 0; d < NUM_DRIVERS; d++) {
    p[0] = CMD_SET_DRIVER_LEVEL; p[1] = (uint8_t)d; wrF32(p, 2, params.driver[d].levelDb); emitParam(p, 6);
    uint16_t ds = (uint16_t)params.driver[d].delaySamples;
    p[0] = CMD_SET_DRIVER_DELAY; p[1] = (uint8_t)d; p[2] = ds & 0xFF; p[3] = ds >> 8;   emitParam(p, 4);
    for (int i = 0; i < MAX_DRIVER_BANDS; i++) emitBand(CMD_SET_DRIVER_EQ_BAND, d, i, params.driver[d].eq[i]);
  }
  uint8_t done[2] = { EVT_PARAMS_DONE, (uint8_t)g_dumpCount };
  sendEvt(done, sizeof(done));
  Serial.printf("[ble] params dump: %lu frames\n", (unsigned long)g_dumpCount);
}

// Dispatch one CMD frame. Returns 0 on success.
static uint8_t dispatchCmd(const uint8_t* d, size_t n) {
  if (n < 1) return 1;
  uint8_t op = d[0];
  switch (op) {
    case CMD_HELLO:      sendHello();  return 0;
    case CMD_GET_STATUS: sendStatus(); return 0;
    case CMD_GET_PARAMS: g_dumpParams = true; return 0;   // loop() streams it after the ACK

    case CMD_SET_MASTER_GAIN:  if (n < 5) return 1; params.masterGain = rdF32(d,1); rebuild(params); return 0;
    case CMD_SET_MUTE:         if (n < 2) return 1; g_userMuted = d[1] != 0; setAmpsMuted(g_userMuted); return 0;
    case CMD_SET_PROFILE: {
      if (n < 2) return 1;
      // The USB host owns the rate while it streams; the toggle is for the bench
      // (profiling at 96 k with nothing playing). ACK 3 = rate owned by USB.
      if (g_usbStreaming) { sendStatus(); return 3; }
      g_pendingRate = (d[1] == PROFILE_HIRES) ? 96000 : 48000;  // audio task switches it, sets g_profile
      sendStatus();
      return 0;
    }
    case CMD_SET_CROSSOVER_HZ: if (n < 5) return 1; params.crossoverHz = rdF32(d,1); rebuild(params); return 0;
    case CMD_SET_VOICING_PREAMP:if(n < 5) return 1; params.voicingPreampDb = rdF32(d,1); rebuild(params); return 0;

    case CMD_SET_VOICING_BAND: {
      if (n < 16) return 1;
      int idx = d[1]; if (idx < 0 || idx >= MAX_VOICING_BANDS) return 2;
      EqBand& b = params.voicing[idx];
      b.enabled = d[2] != 0; b.type = (FilterType)d[3];
      b.f = rdF32(d,4); b.Q = rdF32(d,8); b.gainDb = rdF32(d,12);
      rebuild(params); return 0;
    }
    case CMD_CLEAR_VOICING:
      for (int i = 0; i < MAX_VOICING_BANDS; i++) params.voicing[i].enabled = false;
      rebuild(params); return 0;

    case CMD_SET_DRIVER_LEVEL: {
      if (n < 6) return 1; int dv = d[1]; if (dv < 0 || dv >= NUM_DRIVERS) return 2;
      params.driver[dv].levelDb = rdF32(d,2); rebuild(params); return 0;
    }
    case CMD_SET_DRIVER_DELAY: {
      if (n < 4) return 1; int dv = d[1]; if (dv < 0 || dv >= NUM_DRIVERS) return 2;
      params.driver[dv].delaySamples = rdU16(d,2); rebuild(params); return 0;
    }
    case CMD_SET_DRIVER_EQ_BAND: {
      if (n < 17) return 1;
      int dv = d[1]; int idx = d[2];
      if (dv < 0 || dv >= NUM_DRIVERS || idx < 0 || idx >= MAX_DRIVER_BANDS) return 2;
      EqBand& b = params.driver[dv].eq[idx];
      b.enabled = d[3] != 0; b.type = (FilterType)d[4];
      b.f = rdF32(d,5); b.Q = rdF32(d,9); b.gainDb = rdF32(d,13);
      rebuild(params); return 0;
    }

    case CMD_SAVE_TO_NVS:
      saveParamsBlob("params", params); return 0;   // boot default

    case CMD_SAVE_PRESET: {
      if (n < 2) return 1; int slot = d[1]; if (slot < 0 || slot >= NUM_PRESETS) return 2;
      char k[16]; presetKey(k, sizeof(k), slot); saveParamsBlob(k, params); return 0;
    }
    case CMD_LOAD_PRESET: {
      if (n < 2) return 1; int slot = d[1]; if (slot < 0 || slot >= NUM_PRESETS) return 2;
      char k[16]; presetKey(k, sizeof(k), slot);
      // Design for the rate we are running at (or switching to), never the
      // rate the preset happened to be saved at.
      uint32_t pend = g_pendingRate;
      if (!loadParamsBlob(k, params, pend ? pend : (uint32_t)g_sampleRate)) return 3;   // no such preset
      rebuild(params); sendStatus();
      Serial.printf("[nvs] %s loaded, coefficients designed for %lu Hz (running %lu)\n",
                    k, (unsigned long)params.sampleRate, (unsigned long)g_sampleRate);
      return 0;
    }

    default: return 0xFF;
  }
}

class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    uint8_t res = dispatchCmd(v.data(), v.size());
    if (v.size() >= 1) {
      Serial.printf("[ble] cmd 0x%02X (%u bytes) -> %u\n", v.data()[0], (unsigned)v.size(), res);
      sendAck(v.data()[0], res);
    }
  }
};
class SrvCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    Serial.println("[ble] client connected");
    sendHello(); sendStatus();
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo&, int) override {
    Serial.println("[ble] disconnected, re-advertising");
    NimBLEDevice::startAdvertising();
  }
};

void setupBle() {
  NimBLEDevice::init("SpeakerDSP");
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new SrvCallbacks());
  NimBLEService* svc = srv->createService(DSP_SVC_UUID);
  srv->getAdvertising();
  NimBLECharacteristic* cmd = svc->createCharacteristic(
      DSP_CMD_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmd->setCallbacks(new CmdCallbacks());
  g_evt = svc->createCharacteristic(DSP_EVT_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(DSP_SVC_UUID);
  adv->setName("SpeakerDSP");
  adv->enableScanResponse(true);   // name/UUID may not fit one 31-byte packet
  NimBLEDevice::startAdvertising();
}

void setup() {
  for (int8_t pin : PIN_MUTE) pinMode(pin, OUTPUT);
  setAmpsMuted(true);                 // muted before anything comes up

  setupUsb();                         // CDC console + UAC speaker; enumerates while the rest boots
  delay(300);
  Serial.println("\n[dsp_engine] boot (amps muted)");

  g_rebuildMux = xSemaphoreCreateMutex();   // must exist before the first rebuild()
  loadDefaultConfig(params);
  if (loadParamsBlob("params", params, 0))  // restore saved boot default, INCLUDING its rate
    Serial.printf("[dsp_engine] restored params from NVS (saved rate %lu Hz)\n",
                  (unsigned long)params.sampleRate);
  // Boot straight into the saved profile: no mute/switch cycle, I2S comes up at
  // that rate. g_sampleRate must be final before audioTask starts (it derives
  // the tone increments from it once).
  g_sampleRate = params.sampleRate;
  g_usbRate    = g_sampleRate;        // until the host says otherwise
  g_profile    = (g_sampleRate == 96000) ? PROFILE_HIRES : PROFILE_NORMAL;
  rebuild(params);

  const uint32_t bootRate = g_sampleRate;
  i2sLow.setPins (I2S0_BCLK, I2S0_LRCK, I2S0_DOUT, -1, -1);
  i2sHigh.setPins(I2S1_BCLK, I2S1_LRCK, I2S1_DOUT, -1, -1);
  bool ok = i2sLow.begin (I2S_MODE_STD, bootRate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)
         && i2sHigh.begin(I2S_MODE_STD, bootRate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  if (!ok) {
    Serial.println("[dsp_engine] I2S begin FAILED (core >=3.0? two I2S ports?)");
    while (true) delay(1000);         // stay muted
  }

  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr,
                          configMAX_PRIORITIES - 1, nullptr, 1);

  delay(150);                         // let I2S/DAC settle
  setAmpsMuted(false);

  setupBle();                         // advertise the DSP control service
  Serial.printf("[dsp_engine] running @ %lu Hz (%s), un-muted, BLE 'SpeakerDSP' advertising, USB ep out 0x%02x fb 0x%02x\n",
                (unsigned long)bootRate, g_profile == PROFILE_HIRES ? "High-Res" : "Normal", g_epOut, 0x80 | g_epFb);
}

void loop() {
  // BLE work handed off by the other contexts (audio core / GATT callback):
  if (g_rateChanged) { g_rateChanged = false; sendStatus(); }   // rate now final -> app refreshes
  if (g_dumpParams)  { g_dumpParams  = false; dumpParams();  }

  static uint32_t last = 0;
  if (millis() - last < 1000) { delay(20); return; }
  last = millis();

  float cps = g_cyclesPerSample;
  uint32_t nb = g_activeBiquads;
  uint32_t rate = g_sampleRate;
  float coreHz  = 240e6f;
  float loadPct = 100.0f * cps * rate / coreHz;    // % of one core
  float cpb     = nb ? cps / nb : 0;

  static uint32_t lastPkts = 0;
  uint32_t pk = g_usbPkts;
  Serial.printf("[prof] %.1f cyc/sample | %lu biquads | %.1f cyc/biquad | "
                "%.1f%% of one 240MHz core @ %lu Hz | usb %s host %lu Hz pkts/s %lu fifo %u underrun %lu%s\n",
                cps, (unsigned long)nb, cpb, loadPct, (unsigned long)rate,
                g_usbStreaming ? "RUN" : "idle", (unsigned long)g_usbRate, (unsigned long)(pk - lastPkts),
                g_usbFifoNow, (unsigned long)g_usbUnderruns,
                g_uac_iso_alloc_fail ? " (rx fifo enlarged at stream start)" : "");
  lastPkts = pk;
}
