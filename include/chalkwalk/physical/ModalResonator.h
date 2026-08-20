#pragma once

#include <chalkwalk/physical/Resonator.h>

namespace chalkwalk::physical {

// 2D/3D modal resonator. Spec §4.2: parallel bank of resonating biquad
// bandpass filters; pitch is bypassed in favour of spatial injection
// along a vector path p(η). Sustained keys apply localised impedance
// (paraphonic muting).
//
// Skeleton: 1-pole low-pass passthrough. Same audio shape as
// WaveguideResonator for now — both branches exist so the architecture
// is concrete.
class ModalResonator : public Resonator {
 public:
  void prepare(double sampleRate, int maxBlockSize) override;
  void reset() override;
  void setGeometry(Geometry) override {}
  void setStiffness(float) override {}
  void setDensity(float) override {}
  void setDamping(float d) override;
  void setPitchHz(float) override {}
  void engageDamper(float) override {}  // stub: Δα_k muting lands in M6
  void releaseDamper() override {}
  void renderReplace(const float* excitation, int numSamples,
                     float* outL, float* outR) override;

 private:
  float lpState_ = 0.0f;  // shared L/R; per-mode state lands here in Phase 5
  float lpAlpha_ = 0.3f;
};

}  // namespace chalkwalk::physical
