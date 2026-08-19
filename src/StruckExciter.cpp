#include <chalkwalk/physical/StruckExciter.h>

#include <cmath>

namespace chalkwalk::physical {

namespace {
constexpr float kPi = 3.14159265358979f;

// He=0: 20 ms soft mallet; He=1: 4 ms hard pick.
constexpr float kDurationMaxMs = 20.0f;
constexpr float kDurationMinMs =  4.0f;

// He=0: 300 Hz LPF cutoff; He=1: 8 kHz (nearly flat).
constexpr float kCutoffSoftHz = 300.0f;
constexpr float kCutoffHardHz = 8000.0f;

inline float xorshiftFloat(unsigned int& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return static_cast<float>(static_cast<int>(s)) / 2147483648.0f;
}
}  // namespace

void StruckExciter::prepare(double sampleRate, int /*maxBlockSize*/) {
  sampleRate_ = sampleRate;
  reset();
}

void StruckExciter::reset() {
  burstLength_    = 0;
  burstPos_       = 0;
  burstAmplitude_ = 0.0f;
  bqX1_ = bqX2_ = bqY1_ = bqY2_ = 0.0f;
}

void StruckExciter::trigger(const PhysicalState& state) {
  const float he = state.He;
  const float durationMs = kDurationMaxMs - he * (kDurationMaxMs - kDurationMinMs);
  burstLength_    = static_cast<int>(sampleRate_ * static_cast<double>(durationMs) / 1000.0);
  if (burstLength_ < 1) burstLength_ = 1;
  burstPos_       = 0;
  burstAmplitude_ = state.Fe;

  const float cutoffHz = kCutoffSoftHz + he * (kCutoffHardHz - kCutoffSoftHz);
  computeLpf(cutoffHz);
}

void StruckExciter::release() {
  // Struck archetype is impulse-based; release is a no-op.
}

void StruckExciter::renderAdd(float* out, int numSamples,
                              const PhysicalState& /*currentState*/) {
  for (int i = 0; i < numSamples; ++i) {
    if (burstPos_ >= burstLength_)
      break;

    // Descending half-Hann: 1.0 at pos 0, 0.0 at pos burstLength_-1.
    const float phase = (burstLength_ > 1)
        ? static_cast<float>(burstPos_) / static_cast<float>(burstLength_ - 1)
        : 1.0f;
    const float env = 0.5f * (1.0f + std::cos(kPi * phase));

    const float x = xorshiftFloat(rngState_) * burstAmplitude_ * env;

    // Biquad LPF — direct form I.
    const float y = bqB0_ * x  + bqB1_ * bqX1_ + bqB2_ * bqX2_
                  - bqA1_ * bqY1_ - bqA2_ * bqY2_;
    bqX2_ = bqX1_;  bqX1_ = x;
    bqY2_ = bqY1_;  bqY1_ = y;

    out[i] += y;
    ++burstPos_;
  }
}

void StruckExciter::computeLpf(float cutoffHz) {
  const float fs  = static_cast<float>(sampleRate_);
  const float fc  = (cutoffHz < fs * 0.49f) ? cutoffHz : fs * 0.49f;
  const float omega    = 2.0f * kPi * fc / fs;
  const float sinOmega = std::sin(omega);
  const float cosOmega = std::cos(omega);
  // Butterworth (Q = 1/sqrt(2) ≈ 0.7071): alpha = sin(omega) / (2*Q).
  const float alpha = sinOmega * 0.7071f;
  const float a0inv = 1.0f / (1.0f + alpha);

  bqB0_ = (1.0f - cosOmega) * 0.5f * a0inv;
  bqB1_ = (1.0f - cosOmega) * a0inv;
  bqB2_ = bqB0_;
  bqA1_ = -2.0f * cosOmega * a0inv;
  bqA2_ = (1.0f - alpha) * a0inv;

  bqX1_ = bqX2_ = bqY1_ = bqY2_ = 0.0f;
}

}  // namespace chalkwalk::physical
