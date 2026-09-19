// channel_test.ino
// -----------------------------------------------------------------------------
// Single-DAC channel test for the active-crossover project.
//
// Drives ONE PCM5102 (the "LOW" DAC on I2S0) with a sine test tone that cycles:
//     LEFT only  ->  RIGHT only  ->  BOTH  ->  SILENCE  ->  (repeat)
// Serial prints which channel is currently driven.
//
// Use it to bring up one amp + speaker at a time:
//   1. Wire speaker to the amp fed from PCM5102 OUTL. You should hear the tone
//      only during the "LEFT" phase.
//   2. Move it to the OUTR amp; now it should sound only during "RIGHT".
// Once both DAC outputs check out, we scale this to the second DAC (I2S1) and
// all four amps.
//
// REQUIRES: ESP32 Arduino core 3.0.0 or newer (the ESP_I2S library / I2SClass).
//   In Arduino IDE: Boards Manager -> "esp32 by Espressif" >= 3.0.
//   Older 2.x cores use a different driver (driver/i2s.h) and will NOT compile.
//
// PCM5102 board must be strapped: SCK->GND (internal PLL), FMT->GND (I2S),
//   XSMT->3V3 (un-mute!  floating/low = silence), FLT->GND, DEMP->GND.
//
// AMP MUTE: one GPIO drives the base(s) of the mute NPN(s) (BC547/2N2222) that
//   short each TPA3118 2-pin mute header. Held MUTED through boot, un-muted only
//   after I2S is streaming -> pop-free. Set MUTE_ACTIVE_HIGH from your bench test
//   (does shorting the header mute, or un-mute?). See WIRING.md "Amp mute control".
// -----------------------------------------------------------------------------

#include <ESP_I2S.h>
#include <math.h>

// ---- I2S0 pins to the LOW PCM5102 (match WIRING.md; change to your board) ----
constexpr int8_t PIN_BCLK = 4;   // PCM5102 BCK
constexpr int8_t PIN_LRCK = 5;   // PCM5102 LRCK / WS
constexpr int8_t PIN_DOUT = 6;   // PCM5102 DIN  (ESP data out)
// No MCLK line: SCK on the DAC is grounded, so we pass -1 for mclk.

// ---- Amp mute line (shared GPIO -> NPN base(s)) ------------------------------
constexpr int8_t PIN_MUTE = 10;  // any free pin outside the I2S set (GPIO4-9)
// Does driving the transistor (shorting the header) MUTE the amp? Determined by
// your bench test. If "shorted = mute", the transistor conducts to mute -> true.
// If "shorted = un-mute", set false. Either way we hold amps MUTED at boot.
constexpr bool MUTE_ACTIVE_HIGH = true;
constexpr uint32_t UNMUTE_SETTLE_MS = 120;  // let I2S/XSMT settle before un-mute

// ---- Audio parameters --------------------------------------------------------
constexpr uint32_t SAMPLE_RATE = 44100;   // Hz
constexpr float    TONE_HZ     = 440.0f;  // A4 test tone
constexpr float    AMPLITUDE   = 0.25f;   // 0..1 of full scale (keep modest)
constexpr uint32_t PHASE_MS    = 2000;    // how long each phase plays

// Buffer of interleaved stereo samples (L,R,L,R,...) written per loop pass.
constexpr size_t   FRAMES      = 256;     // stereo frames per write
int16_t sampleBuf[FRAMES * 2];

I2SClass i2s;

// Test phases
enum Phase { LEFT_ONLY, RIGHT_ONLY, BOTH, SILENCE, PHASE_COUNT };
const char* phaseName[] = { "LEFT only", "RIGHT only", "BOTH", "SILENCE" };

Phase    phase        = LEFT_ONLY;
uint32_t phaseStart   = 0;
float    phaseAngle   = 0.0f;                       // running sine phase (rad)
const float PHASE_INC = 2.0f * PI * TONE_HZ / SAMPLE_RATE;

// Drive the amp mute line. muted=true silences the amps.
void setAmpsMuted(bool muted) {
  // Level that produces "muted" depends on the board test (MUTE_ACTIVE_HIGH).
  bool level = MUTE_ACTIVE_HIGH ? muted : !muted;
  digitalWrite(PIN_MUTE, level ? HIGH : LOW);
}

void setup() {
  // Force amps MUTED before anything else, while I2S/DAC come up.
  pinMode(PIN_MUTE, OUTPUT);
  setAmpsMuted(true);

  Serial.begin(115200);
  delay(300);
  Serial.println("\n[channel_test] starting (amps muted)");

  i2s.setPins(PIN_BCLK, PIN_LRCK, PIN_DOUT, -1 /*din*/, -1 /*mclk*/);

  // 16-bit stereo standard I2S.
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[channel_test] I2S begin FAILED - check core version / pins");
    while (true) delay(1000);  // stay muted on failure
  }

  // I2S is up; let it and the PCM5102 XSMT settle, then un-mute pop-free.
  delay(UNMUTE_SETTLE_MS);
  setAmpsMuted(false);

  Serial.println("[channel_test] I2S up, amps un-muted. Cycling channels every 2 s.");
  phaseStart = millis();
  Serial.printf("Phase: %s\n", phaseName[phase]);
}

void loop() {
  // Advance to the next phase on the timer.
  if (millis() - phaseStart >= PHASE_MS) {
    phase      = static_cast<Phase>((phase + 1) % PHASE_COUNT);
    phaseStart = millis();
    // Exercise the mute line: hard-mute the amps during the SILENCE phase,
    // un-mute for the tone phases. Confirms the mute wiring end to end.
    setAmpsMuted(phase == SILENCE);
    Serial.printf("Phase: %s%s\n", phaseName[phase],
                  phase == SILENCE ? "  (amps muted)" : "");
  }

  const bool wantLeft  = (phase == LEFT_ONLY || phase == BOTH);
  const bool wantRight = (phase == RIGHT_ONLY || phase == BOTH);

  // Fill one buffer of interleaved L/R samples.
  for (size_t f = 0; f < FRAMES; f++) {
    int16_t s = (int16_t)(sinf(phaseAngle) * AMPLITUDE * 32767.0f);
    phaseAngle += PHASE_INC;
    if (phaseAngle >= 2.0f * PI) phaseAngle -= 2.0f * PI;

    sampleBuf[2 * f]     = wantLeft  ? s : 0;   // left
    sampleBuf[2 * f + 1] = wantRight ? s : 0;   // right
  }

  // Blocking write; paces the loop to the sample rate.
  i2s.write((uint8_t*)sampleBuf, sizeof(sampleBuf));
}
