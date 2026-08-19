#include <chalkwalk/physical/ModalResonator.h>

#include <algorithm>

namespace chalkwalk::physical {

void ModalResonator::prepare(double, int) { reset(); }

void ModalResonator::reset() {
  lpStateL_ = 0.0f;
  lpStateR_ = 0.0f;
}

void ModalResonator::setDamping(float d) {
  lpAlpha_ = std::clamp(0.05f + 0.5f * (1.0f - d), 0.01f, 0.99f);
}

void ModalResonator::renderReplace(const float* excitation,
                                   int numSamples, float* outL,
                                   float* outR) {
  for (int i = 0; i < numSamples; ++i) {
    const float in = excitation[i];
    lpStateL_ += lpAlpha_ * (in - lpStateL_);
    lpStateR_ += lpAlpha_ * (in - lpStateR_);
    outL[i] = lpStateL_;
    outR[i] = lpStateR_;
  }
}

}  // namespace chalkwalk::physical
