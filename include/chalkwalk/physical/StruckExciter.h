#pragma once

#include <chalkwalk/physical/Exciter.h>

namespace chalkwalk::physical {

// Archetype A: Struck / Plucked. Produces a single shaped impulse on trigger.
// Fe (velocity) -> amplitude. He (mallet hardness) -> burst duration (hard =
// shorter) and spectral tilt (hard = brighter). Envelope is a descending
// half-Hann; spectral shaping is a one-pole-per-side biquad LPF.
class StruckExciter : public Exciter {
 public:
  void prepare(double sampleRate, int maxBlockSize) override;
  void reset() override;
  void trigger(const PhysicalState& state) override;
  void release() override;
  void renderAdd(float* out, int numSamples,
                 const PhysicalState& currentState) override;

 private:
  void computeLpf(float cutoffHz);

  double sampleRate_    = 44100.0;
  int    burstLength_   = 0;
  int    burstPos_      = 0;
  float  burstAmplitude_ = 0.0f;

  // Biquad LPF coefficients and state (direct form I).
  float bqB0_ = 1.0f, bqB1_ = 0.0f, bqB2_ = 0.0f;
  float bqA1_ = 0.0f, bqA2_ = 0.0f;
  float bqX1_ = 0.0f, bqX2_ = 0.0f;
  float bqY1_ = 0.0f, bqY2_ = 0.0f;

  unsigned int rngState_ = 0x1234abcdu;
};

}  // namespace chalkwalk::physical
