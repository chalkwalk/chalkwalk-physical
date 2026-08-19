#pragma once

#include <chalkwalk/physical/Geometry.h>

namespace chalkwalk::physical {

// Abstract base for resonators. A resonator accepts a mono excitation
// stream (or audio input, in FX mode) and produces stereo output.
//
// The same Resonator instance can be either per-voice (polyphonic) or
// shared across voices (paraphonic Fuse mode). This is a runtime topology
// decision owned by VoiceManager; the resonator itself does not care.
class Resonator {
 public:
  virtual ~Resonator() = default;

  virtual void prepare(double sampleRate, int maxBlockSize) = 0;
  virtual void reset() = 0;

  virtual void setGeometry(Geometry g) = 0;
  virtual void setStiffness(float s) = 0;
  virtual void setDensity(float d) = 0;
  virtual void setDamping(float d) = 0;

  // Tuning hint for waveguide mode (Hz). Modal mode ignores this and
  // instead routes the MIDI note to a spatial injection coordinate.
  virtual void setPitchHz(float hz) = 0;

  // Continuous per-note pressure (poly aftertouch / MPE Z) in [0,1].
  // Default no-op; waveguide applies a subtle brightness tilt.
  virtual void setPressure(float /*z01*/) {}

  // MPE timbre (Y dimension) in [0,1]. On the Osmose this only activates
  // after Z reaches 100%, making it a natural "finger-on-string" gesture.
  // Default no-op; waveguide uses it as the primary per-note damping target.
  virtual void setTimbre(float /*y01*/) {}

  // Curve shaping applied to both pressure and timbre inputs before they
  // reach the resonator. [-1,1]: -1=log (sensitive low), 0=linear, +1=exp
  // (gentle low — suitable for Osmose where Z tracks the full key travel).
  virtual void setExpressionCurve(float /*c*/) {}

  // Engage / release the physical damper.
  // `rate` in [0,1]: 0 = no damper (guitar, free ring), 1 = hard felt
  // (rapid mute). engageDamper() is called on note-off; releaseDamper()
  // will be called by a held sustain pedal (CC64, wired at M2.5).
  virtual void engageDamper(float rate) = 0;
  virtual void releaseDamper() = 0;

  // Render numSamples of stereo output. The mono `excitation` buffer is
  // injected into the resonator at the configured spatial point. Output
  // is *replaced*, not added.
  virtual void renderReplace(const float* excitation, int numSamples,
                             float* outL, float* outR) = 0;
};

}  // namespace chalkwalk::physical
