#include <chalkwalk/physical/BowedExciter.h>

#include <algorithm>
#include <cmath>

namespace chalkwalk::physical {

void BowedExciter::prepare(double sampleRate, int) {
  sampleRate_ = sampleRate;
}

void BowedExciter::reset() {
  gate_      = false;
  fPrev_     = 0.0f;
  vHat_      = 0.0f;
  vBow_      = 0.0f;
  prevFn_    = 0.0f;
  bowPhase_  = 0.0f;
  bowPos_    = 0.5f;
  bowDir_    = 1.0f;
}

void BowedExciter::trigger(const PhysicalState& state) {
  gate_   = true;
  prevFn_ = state.Fn;
  fPrev_  = state.Fn * kMuS;  // assume stick phase at note-on
}

void BowedExciter::release() {
  gate_ = false;
}

void BowedExciter::renderAdd(float* out, int numSamples,
                              const PhysicalState& currentState) {
  if (!gate_) return;

  const float Fn = currentState.Fn;
  updateBowVelocity(Fn, numSamples);

  // Gate Fn by normalised bow speed.  Without the waveguide feedback loop
  // (M4.3), the junction solver settles at a nonzero static-friction force
  // even when the bow is stationary, continuously pumping energy into the
  // resonator.  Tapering effective Fn to zero as vBow_ → 0 prevents that:
  // zero bow movement → zero excitation.  kV0 is the natural scale for this
  // (the characteristic slip velocity), so full Fn applies once |vBow_|≥kV0.
  const float bowSpeedNorm = std::min(1.0f, std::abs(vBow_) / kV0);
  const float effectiveFn  = Fn * bowSpeedNorm;

  for (int i = 0; i < numSamples; ++i)
    out[i] += solveJunction(effectiveFn, vBow_);
}

// ---------------------------------------------------------------------------

void BowedExciter::updateBowVelocity(float currentFn, int numSamples) noexcept {
  const float dt = static_cast<float>(numSamples) / static_cast<float>(sampleRate_);

  switch (mode_) {
    case 0: {
      // Rate mode: signed dFn/dt → bow velocity with inertial decay.
      // Pressing (Fn rising) = positive bow direction; releasing = negative.
      // Momentum lets the tone sustain briefly after the finger stops.
      const float dFnPerSec = (currentFn - prevFn_) / dt;
      const float target    = dFnPerSec * kRateScale;
      const float decay     = std::exp(-dt / kMomentumTau);
      vBow_ = vBow_ * decay + target * (1.0f - decay);
      prevFn_ = currentFn;
      break;
    }
    case 1: {
      // Auto mode: internal oscillator; Z only controls Fn.
      bowPhase_ += kAutoRateHz * dt;
      if (bowPhase_ >= 1.0f) bowPhase_ -= 1.0f;
      vBow_ = std::sin(bowPhase_ * 6.28318530718f) * kAutoAmp;
      break;
    }
    case 2: {
      // Continuous mode: simulated bow sweeping back and forth.
      bowPos_ += kContSpeed * bowDir_ * dt;
      if (bowPos_ >= 1.0f) { bowPos_ = 1.0f; bowDir_ = -1.0f; }
      if (bowPos_ <= 0.0f) { bowPos_ = 0.0f; bowDir_ =  1.0f; }
      vBow_ = bowDir_ * kContSpeed;
      break;
    }
    default:
      break;
  }
}

float BowedExciter::frictionCurve(float vRel) noexcept {
  return kMuD + (kMuS - kMuD) * std::exp(-std::abs(vRel) / kV0);
}

float BowedExciter::solveJunction(float Fn, float vBow) noexcept {
  // Solve F = Fn * f(v_bow - vHat_ - F/(2*Z0)) via two Newton iterations.
  //
  // g(F)  = F - Fn * f(vRel),  where vRel = vBow - vHat_ - F/(2*Z0)
  // g'(F) = 1 + Fn * f'(vRel) / (2*Z0)
  //         f'(vRel) = -(mu_s - mu_d) * sign(vRel) / v0 * exp(-|vRel|/v0)
  float F = fPrev_;

  for (int iter = 0; iter < 2; ++iter) {
    const float vRel    = vBow - vHat_ - F / (2.0f * kZ0);
    const float absVRel = std::abs(vRel);
    const float expTerm = std::exp(-absVRel / kV0);
    const float fVal    = kMuD + (kMuS - kMuD) * expTerm;
    const float signV   = (vRel >= 0.0f) ? 1.0f : -1.0f;
    const float dfDvRel = -(kMuS - kMuD) * signV / kV0 * expTerm;

    const float g    = F - Fn * fVal;
    const float dgDF = 1.0f + Fn * dfDvRel / (2.0f * kZ0);

    if (std::abs(dgDF) > 1e-9f)
      F -= g / dgDF;
  }

  fPrev_ = F;
  return F;
}

}  // namespace chalkwalk::physical
