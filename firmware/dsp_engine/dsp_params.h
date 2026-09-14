// dsp_params.h — the parameter model. This struct is the single source of truth
// the audio core reads and (later) the BLE app writes. It IS the app<->firmware
// contract; when the opcode table is frozen, keep `version` in lockstep.
#pragma once
#include <stdint.h>
#include "biquad.h"

constexpr int MAX_VOICING_BANDS = 10;   // pre-crossover; Peace/AutoEQ imports land here
constexpr int MAX_DRIVER_BANDS  = 4;    // post-crossover, per driver
constexpr int NUM_DRIVERS       = 4;    // 0=L-woofer 1=R-woofer 2=L-tweeter 3=R-tweeter

struct EqBand {
  bool       enabled = false;
  FilterType type    = FT_PEAK;
  float      f       = 1000.0f;
  float      Q       = 0.707f;
  float      gainDb  = 0.0f;
};

struct DriverCfg {
  float  levelDb      = 0.0f;   // per-driver trim (tweeters usually negative)
  int    delaySamples = 0;      // time-align
  EqBand eq[MAX_DRIVER_BANDS];
};

struct Params {
  uint16_t  version         = 1;
  uint32_t  sampleRate      = 48000;   // 48000 (Normal) or 96000 (High-Res)
  float     masterGain      = 0.5f;    // linear
  float     voicingPreampDb = 0.0f;    // AutoEQ/Peace "Preamp"
  float     crossoverHz     = 2500.0f; // LR4 acoustic crossover
  EqBand    voicing[MAX_VOICING_BANDS];
  DriverCfg driver[NUM_DRIVERS];
};
