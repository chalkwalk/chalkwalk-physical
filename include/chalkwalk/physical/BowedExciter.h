#pragma once

#include <chalkwalk/physical/Exciter.h>

namespace chalkwalk::physical {

// Archetype B: Bowed / Scraped. Continuous friction model with stick-slip
// curve f(v) = mu_d + (mu_s - mu_d) * exp(-|v/v0|). See spec §3.1.
//
// Per-sample Newton solver for the implicit bow-junction equation:
//   F = Fn * f(v_bow - v_hat - F/(2*Z0))
// where v_hat is the incoming traveling-wave velocity at the bow contact
// (provided by WaveguideResonator via setJunctionVelocity, wired in M4.3;
// defaults to zero until then).
//
// Three bow-velocity modes (M4.2):
//   Rate       — |dZ/dt| with momentum decay; both press and release
//                count as a bow stroke, sign determines bow direction.
//   Auto       — internal oscillator at a fixed rate; Z → Fn only.
//   Continuous — simulated bow position integrates at fixed speed,
//                auto-reversing at the ends; Z → Fn only.
// Plain-MIDI fallback (D): CC1 routed to Fn via Voice::applyCC1Fn,
// handled in VoiceManager before MPE events.
class BowedExciter : public Exciter {
 public:
  void prepare(double sampleRate, int maxBlockSize) override;
  void reset() override;
  void trigger(const PhysicalState& state) override;
  void release() override;
  void renderAdd(float* out, int numSamples,
                 const PhysicalState& currentState) override;

  bool suppressResonatorExpression() const override { return true; }
  void setBowMode(int m) override { mode_ = m; }

  // M4.3: called each block by Voice before renderAdd, providing the
  // previous-block junction velocity from the waveguide delay line.
  void setJunctionVelocity(float vHat) { vHat_ = vHat; }

 private:
  static float frictionCurve(float vRel) noexcept;
  float solveJunction(float Fn, float vBow) noexcept;

  // Update vBow_ for the current block according to the active mode.
  void updateBowVelocity(float currentFn, int numSamples) noexcept;

  // Friction model constants (normalised synthesis units).
  static constexpr float kMuS = 0.8f;
  static constexpr float kMuD = 0.3f;
  static constexpr float kV0  = 0.1f;   // characteristic slip velocity
  static constexpr float kZ0  = 1.0f;   // normalised string impedance

  // Rate mode: scale from dFn/dt to bow velocity, and momentum decay tau (s).
  static constexpr float kRateScale    = 0.05f;
  static constexpr float kMomentumTau  = 1.5f;

  // Auto mode: oscillation rate (Hz) and amplitude.
  static constexpr float kAutoRateHz   = 0.5f;
  static constexpr float kAutoAmp      = 0.25f;

  // Continuous mode: bow speed (position units per second) and range.
  static constexpr float kContSpeed    = 0.15f;

  int   mode_  = 0;        // 0=Rate, 1=Auto, 2=Continuous
  bool  gate_  = false;
  float fPrev_ = 0.0f;     // warm-start: previous sample's junction force
  float vHat_  = 0.0f;     // junction velocity from resonator (M4.3)
  float vBow_  = 0.0f;     // current bow velocity fed into solver each block
  double sampleRate_ = 44100.0;

  // Rate mode state
  float prevFn_ = 0.0f;

  // Auto mode state
  float bowPhase_ = 0.0f;   // [0, 1)

  // Continuous mode state
  float bowPos_ = 0.5f;     // normalised position [0, 1]
  float bowDir_ = 1.0f;     // ±1
};

}  // namespace chalkwalk::physical
