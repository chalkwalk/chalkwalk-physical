#pragma once

#include <chalkwalk/physical/Resonator.h>

#include <vector>

namespace chalkwalk::physical {

// 1D waveguide resonator (spec §4.1). Single-loop delay line with:
//   - Thiran 1st-order all-pass for sub-sample pitch tuning
//   - One-pole loop LPF for frequency-dependent material damping
//   - Per-loop feedback gain for overall energy loss
// Stereo output via two pickup positions: L = bridge, R = mid-string.
class WaveguideResonator : public Resonator {
 public:
  void prepare(double sampleRate, int maxBlockSize) override;
  void reset() override;
  void setGeometry(Geometry) override {}
  void setStiffness(float) override {}
  void setDensity(float) override {}
  void setDamping(float d) override;
  void setPressure(float z01) override;
  void setTimbre(float y01) override;
  void setExpressionCurve(float c) override;
  void setPitchHz(float hz) override;
  void engageDamper(float rate) override;
  void releaseDamper() override;
  void renderReplace(const float* excitation, int numSamples,
                     float* outL, float* outR) override;

 private:
  void retune();

  void applyLoopParams();  // recomputes loopAlpha_ and feedbackGain_ from stored values

  double sampleRate_   = 44100.0;
  float  pitchHz_      = 440.0f;
  float  baseDamping_      = 0.0f;  // last value from setDamping()
  float  pressureAmt_      = 0.0f;  // last value from setPressure() / MPE Z [0,1]
  float  timbreAmt_        = 0.0f;  // last value from setTimbre()   / MPE Y [0,1]
  float  expressionCurve_  = 0.0f;  // last value from setExpressionCurve() [-1,1]
  float  feedbackGain_ = 0.999f;
  float  loopAlpha_    = 0.0f;

  // Circular delay buffer.
  std::vector<float> delayBuf_;
  int writePos_    = 0;
  int delayLength_ = 0;  // integer part of total loop delay

  // Thiran 1st-order all-pass state and coefficient.
  float thiranA_  = 0.0f;
  float thiranX1_ = 0.0f;
  float thiranY1_ = 0.0f;

  // Loop filter state.
  float loopState_ = 0.0f;

  // Physical damper state.
  bool  damperActive_ = false;
  float damperGain_   = 1.0f;   // multiplied into feedback; decays toward 0
  float damperDecay_  = 1.0f;   // per-sample decay factor when damper engaged
};

}  // namespace chalkwalk::physical
