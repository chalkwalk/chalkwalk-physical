#pragma once

#include <chalkwalk/physical/TranslationMatrix.h>

namespace chalkwalk::physical {

// Abstract base for all excitation generators. An Exciter produces an
// audio-rate signal that is then injected into a Resonator. It does not
// itself produce a tonal output (per spec §3).
class Exciter {
 public:
  virtual ~Exciter() = default;

  virtual void prepare(double sampleRate, int maxBlockSize) = 0;
  virtual void reset() = 0;

  // Drive the exciter for one note-on event. Subclasses interpret the
  // PhysicalState according to their archetype (impulse vs continuous).
  virtual void trigger(const PhysicalState& state) = 0;
  virtual void release() = 0;

  // Render `numSamples` samples into the supplied mono buffer. The exciter
  // is additive into `out` (callers must zero the buffer if needed).
  virtual void renderAdd(float* out, int numSamples,
                         const PhysicalState& currentState) = 0;

  // When true, Voice skips forwarding MPE Z/Y to resonator->setPressure /
  // setTimbre. Bowed exciter returns true because those dimensions drive the
  // friction model directly; independent resonator routing would double-count.
  virtual bool suppressResonatorExpression() const { return false; }

  // Runtime bow-mode switch (no-op for non-bowed archetypes).
  virtual void setBowMode(int) {}
};

}  // namespace chalkwalk::physical
