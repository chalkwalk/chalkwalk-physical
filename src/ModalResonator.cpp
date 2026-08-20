#include <chalkwalk/physical/ModalResonator.h>

#include <algorithm>

namespace chalkwalk::physical {

void ModalResonator::prepare(double, int) { reset(); }

void ModalResonator::reset() {
  lpState_ = 0.0f;
}

void ModalResonator::setDamping(float d) {
  lpAlpha_ = std::clamp(0.05f + 0.5f * (1.0f - d), 0.01f, 0.99f);
}

void ModalResonator::renderReplace(const float* excitation,
                                   int numSamples, float* outL,
                                   float* outR) {
  // Skeleton: one single-pole low-pass feeding both channels identically. The
  // modal bank (Phase 5) replaces this with a parallel biquad array, and the
  // per-mode state lands where lpState_ is.
  //
  // One state rather than two: the pair here were fed the same input with the
  // same coefficient, so they held the same number for ever and the second
  // filter was arithmetic performed to reach a foregone conclusion.
  for (int i = 0; i < numSamples; ++i) {
    lpState_ += lpAlpha_ * (excitation[i] - lpState_);
    outL[i] = lpState_;
    outR[i] = lpState_;
  }
}

}  // namespace chalkwalk::physical
