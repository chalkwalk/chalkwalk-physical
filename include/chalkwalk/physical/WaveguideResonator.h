#pragma once

#include <chalkwalk/physical/Resonator.h>

#include <vector>

namespace chalkwalk::physical {

// 1D waveguide resonator (spec §4.1). Single-loop delay line with:
//   - Lagrange fractional-delay FIR for sub-sample pitch tuning
//   - DC blocker: a string has no zero-frequency mode, and a feedback loop
//     does
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

  // Fills fracCoeff_ for a fractional delay of `d` samples past tap 0.
  void computeFractionalDelay(float d);

  void applyLoopParams();  // recomputes loopAlpha_ and feedbackGain_ from stored values

  // Order of the Lagrange fractional-delay interpolator.
  //
  // Measured across the chromatic range at 48 kHz, worst partial stretch for
  // partials up to the eighth:
  //
  //     partials below   order 1   order 3   order 5   order 7
  //          5 kHz         0.44      0.05      0.00      0.00
  //          8 kHz         1.76      0.43      0.10      0.02
  //         12 kHz         4.84      2.11      0.91      0.40
  //
  // Order 5 clears the whole audible band by a wide margin and costs six
  // multiply-adds. Beyond it the returns are real but tiny, and they are
  // spent entirely on partials near Nyquist -- see computeFractionalDelay().
  static constexpr int kFracOrder = 5;

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
  int delayLength_ = 0;  // integer delay of the interpolator's first tap
  int loopLength_  = 0;  // total loop delay, rounded -- pickup placement only

  // Lagrange fractional-delay coefficients. Stateless by construction, which
  // is why they can be recomputed under a ringing note without a click.
  float fracCoeff_[kFracOrder + 1] = {1.0f};

  // Loop filter state.
  float loopState_ = 0.0f;

  // DC blocker, in the loop: H(z) = (1 - z^-1) / (1 - R z^-1).
  //
  // R sets the corner, and it is this close to 1 for a reason. The blocker's
  // phase delay is compensated in retune() so it cannot detune the string,
  // but the compensation is exact only AT the fundamental -- the delay it
  // adds still varies between partials, which is inharmonicity, which is the
  // thing that was just paid for. At R = 0.9999 that costs 1.4 cents on the
  // second partial of a 110 Hz string. At 0.99999 it costs 0.18, and 0.8 at
  // the 20 Hz bottom of the supported range, which is affordable.
  static constexpr float kDcBlockR = 0.99999f;
  float dcX1_ = 0.0f;
  float dcY1_ = 0.0f;

  // Physical damper state.
  bool  damperActive_ = false;
  float damperGain_   = 1.0f;   // multiplied into feedback; decays toward 0
  float damperDecay_  = 1.0f;   // per-sample decay factor when damper engaged
};

}  // namespace chalkwalk::physical
