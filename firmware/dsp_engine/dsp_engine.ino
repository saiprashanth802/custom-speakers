// dsp_engine.ino
// -----------------------------------------------------------------------------
// Active-crossover DSP engine for ESP32-S3 + 2x PCM5102 + 4x TPA3118.
//
// STATUS: DSP chain, BLE control, NVS persistence, 48k/96k profile switch and
// presets were all VERIFIED ON HARDWARE 2026-08-23 (see the memory / handoff
// doc for the measurements). The 2026-09-15 additions — boot-profile restore,
// post-switch status push, GET_PARAMS read-back — are COMPILED ONLY until the
// board is next on the bench.
//
// What it does:
//   * Synthesizes a two-tone test input (200 Hz + 5 kHz) as a stand-in for a
//     real USB/WiFi source (still the open item — see INTERFACE.md).
//   * Runs the full chain: Voicing EQ (stereo) -> LR4 crossover -> per-driver
//     EQ / level / delay -> 4 outputs.
//   * LOW band  -> I2S0 -> PCM5102 LOW  (L=L-woofer,  R=R-woofer).
//     HIGH band -> I2S1 -> PCM5102 HIGH (L=L-tweeter, R=R-tweeter).
//   * Audible check: with a 2.5 kHz crossover, the woofers get the 200 Hz tone
//     and the tweeters get the 5 kHz tone. That alone proves the split works.
//   * Built-in PROFILER: measures real cycles/sample and cycles/biquad on the
//     S3 and prints them, so we can replace the 30-cycle estimate in DSP.md and
//     confirm the 96 kHz budget. Flip SAMPLE_RATE to 96000 to see the delta.
//
// Audio runs on core 1 (dedicated); serial/stats on core 0. Amps held muted at
// boot, un-muted after I2S is up (pop-free; see WIRING.md).
//
// REQUIRES: ESP32 Arduino core >= 3.0 (ESP_I2S / I2SClass).
// PCM5102 straps: SCK->GND, FMT->GND, XSMT->3V3, FLT->GND, DEMP->GND.
// -----------------------------------------------------------------------------

#include <ESP_I2S.h>
#include <atomic>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "dsp_params.h"
#include "protocol.h"
#pragma GCC optimize ("O3")   // DSP hot path: optimize for speed, not size

// ---- Pins (match WIRING.md / schematic.html) --------------------------------
constexpr int8_t I2S0_BCLK = 4,  I2S0_LRCK = 5,  I2S0_DOUT = 6;   // LOW DAC
constexpr int8_t I2S1_BCLK = 7,  I2S1_LRCK = 8,  I2S1_DOUT = 9;   // HIGH DAC
constexpr int8_t PIN_MUTE  = 10;                                  // amp mute NPNs
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
SemaphoreHandle_t g_rebuildMux  = nullptr;       // serializes rebuild() across the two cores

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

static inline int16_t f2s16(float x) {
  if (x >  1.0f) x =  1.0f;
  if (x < -1.0f) x = -1.0f;
  return (int16_t)(x * 32767.0f);
}

// ---- Shared profiler stats (core1 writes, core0 prints) ---------------------
volatile float    g_cyclesPerSample = 0;
volatile uint32_t g_activeBiquads   = 0;

void setAmpsMuted(bool m) {
  bool level = MUTE_ACTIVE_HIGH ? m : !m;
  digitalWrite(PIN_MUTE, level ? HIGH : LOW);
}

// ---- Audio task (pinned to core 1) ------------------------------------------
void audioTask(void*) {
  static int16_t lowBuf[FRAMES * 2];    // L=Lw, R=Rw  -> I2S0
  static int16_t highBuf[FRAMES * 2];   // L=Lt, R=Rt  -> I2S1
  static float   xin[FRAMES];           // pre-generated input (kept out of the timed region)
  float phaseLo = 0, phaseHi = 0;
  float incLo = 2.0f * (float)M_PI * TONE_LO_HZ / g_sampleRate;
  float incHi = 2.0f * (float)M_PI * TONE_HI_HZ / g_sampleRate;

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
        i2sLow.begin (I2S_MODE_STD, pend, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        i2sHigh.begin(I2S_MODE_STD, pend, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        g_sampleRate = pend;
        incLo = 2.0f * (float)M_PI * TONE_LO_HZ / pend;
        incHi = 2.0f * (float)M_PI * TONE_HI_HZ / pend;
        delay(80);                          // let the DACs relock
        setAmpsMuted(g_userMuted);          // restore the user's mute intent
        g_rateChanged = true;               // loop() (core 0) notifies the app; no BLE from this core
      }
      g_pendingRate = 0;
    }

    // --- untimed: synthesize the two-tone test input (200 Hz + 5 kHz) ---
    for (size_t f = 0; f < FRAMES; f++) {
      xin[f] = TONE_AMP * (sinf(phaseLo) + sinf(phaseHi));
      phaseLo += incLo; if (phaseLo >= 2 * M_PI) phaseLo -= 2 * M_PI;
      phaseHi += incHi; if (phaseHi >= 2 * M_PI) phaseHi -= 2 * M_PI;
    }

    // Grab the active coefficient buffer once per block (atomic; a BLE param
    // update swaps it between blocks, never mid-block).
    Compiled& C = *Cactive.load(std::memory_order_acquire);

    // --- timed: pure DSP (processFrame + int conversion), no tone-gen ---
    uint32_t t0 = ESP.getCycleCount();
    for (size_t f = 0; f < FRAMES; f++) {
      float out[4];
      processFrame(C, xin[f], xin[f], out);
      lowBuf[2 * f]      = f2s16(out[0]);   // L-woofer
      lowBuf[2 * f + 1]  = f2s16(out[1]);   // R-woofer
      highBuf[2 * f]     = f2s16(out[2]);   // L-tweeter
      highBuf[2 * f + 1] = f2s16(out[3]);   // R-tweeter
    }
    uint32_t dt = ESP.getCycleCount() - t0;   // uint32 wrap is fine
    g_cyclesPerSample = (float)dt / FRAMES;
    g_activeBiquads   = C.activeBiquads;

    // Blocking writes pace the loop to the sample rate.
    i2sLow.write((uint8_t*)lowBuf,  sizeof(lowBuf));
    i2sHigh.write((uint8_t*)highBuf, sizeof(highBuf));
  }
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
static Profile g_profile = PROFILE_NORMAL;

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
      g_profile = (Profile)d[1];
      g_pendingRate = (g_profile == PROFILE_HIRES) ? 96000 : 48000;  // audio task switches it
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
  pinMode(PIN_MUTE, OUTPUT);
  setAmpsMuted(true);                 // muted before anything comes up

  Serial.begin(115200);
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
  g_profile    = (g_sampleRate == 96000) ? PROFILE_HIRES : PROFILE_NORMAL;
  rebuild(params);

  const uint32_t bootRate = g_sampleRate;
  i2sLow.setPins (I2S0_BCLK, I2S0_LRCK, I2S0_DOUT, -1, -1);
  i2sHigh.setPins(I2S1_BCLK, I2S1_LRCK, I2S1_DOUT, -1, -1);
  bool ok = i2sLow.begin (I2S_MODE_STD, bootRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)
         && i2sHigh.begin(I2S_MODE_STD, bootRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  if (!ok) {
    Serial.println("[dsp_engine] I2S begin FAILED (core >=3.0? two I2S ports?)");
    while (true) delay(1000);         // stay muted
  }

  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr,
                          configMAX_PRIORITIES - 1, nullptr, 1);

  delay(150);                         // let I2S/DAC settle
  setAmpsMuted(false);

  setupBle();                         // advertise the DSP control service
  Serial.printf("[dsp_engine] running @ %lu Hz (%s), un-muted, BLE 'SpeakerDSP' advertising\n",
                (unsigned long)bootRate, g_profile == PROFILE_HIRES ? "High-Res" : "Normal");
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

  Serial.printf("[prof] %.1f cyc/sample | %lu biquads | %.1f cyc/biquad | "
                "%.1f%% of one 240MHz core @ %lu Hz\n",
                cps, (unsigned long)nb, cpb, loadPct, (unsigned long)rate);
}
