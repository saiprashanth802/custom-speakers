// biquad.h — Transposed Direct Form II biquad + RBJ coefficient design.
// Single-precision only (the ESP32-S3 FPU has no fast doubles). TDF2 is the
// numerically robust single-precision form — safe for low-freq high-Q bands.
#pragma once
#include <math.h>
#pragma GCC optimize ("O3")   // DSP hot path: optimize for speed, not size

struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;  // normalized, a0 == 1
  float z1 = 0, z2 = 0;                            // state

  inline float process(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
  void reset() { z1 = z2 = 0; }
  void setNormalized(float B0, float B1, float B2,
                     float A0, float A1, float A2) {
    float inv = 1.0f / A0;
    b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv;
    a1 = A1 * inv; a2 = A2 * inv;
  }
};

enum FilterType : uint8_t {
  FT_PEAK, FT_LOWSHELF, FT_HIGHSHELF, FT_LOWPASS, FT_HIGHPASS
};

// RBJ audio-EQ cookbook. gainDb ignored for LP/HP.
inline void designBiquad(Biquad& bq, FilterType type,
                         float fs, float f0, float Q, float gainDb) {
  float A  = powf(10.0f, gainDb / 40.0f);
  float w0 = 2.0f * (float)M_PI * f0 / fs;
  float cw = cosf(w0), sw = sinf(w0);
  float alpha = sw / (2.0f * Q);
  float b0, b1, b2, a0, a1, a2;

  switch (type) {
    case FT_LOWPASS:
      b0 = (1 - cw) * 0.5f; b1 = 1 - cw; b2 = (1 - cw) * 0.5f;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case FT_HIGHPASS:
      b0 = (1 + cw) * 0.5f; b1 = -(1 + cw); b2 = (1 + cw) * 0.5f;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case FT_PEAK:
      b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
      a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
    case FT_LOWSHELF: {
      float s = 2.0f * sqrtf(A) * alpha;
      b0 =     A * ((A + 1) - (A - 1) * cw + s);
      b1 = 2 * A * ((A - 1) - (A + 1) * cw);
      b2 =     A * ((A + 1) - (A - 1) * cw - s);
      a0 =         (A + 1) + (A - 1) * cw + s;
      a1 =    -2 * ((A - 1) + (A + 1) * cw);
      a2 =         (A + 1) + (A - 1) * cw - s; break;
    }
    case FT_HIGHSHELF: {
      float s = 2.0f * sqrtf(A) * alpha;
      b0 =      A * ((A + 1) + (A - 1) * cw + s);
      b1 = -2 * A * ((A - 1) + (A + 1) * cw);
      b2 =      A * ((A + 1) + (A - 1) * cw - s);
      a0 =          (A + 1) - (A - 1) * cw + s;
      a1 =      2 * ((A - 1) - (A + 1) * cw);
      a2 =          (A + 1) - (A - 1) * cw - s; break;
    }
    default:
      b0 = 1; b1 = 0; b2 = 0; a0 = 1; a1 = 0; a2 = 0;
  }
  bq.setNormalized(b0, b1, b2, a0, a1, a2);
}
