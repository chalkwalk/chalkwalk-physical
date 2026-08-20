#include <chalkwalk/physical/TranslationMatrix.h>

namespace chalkwalk::physical {

PhysicalState TranslationMatrix::onNoteOn(int /*midiNote*/, float velocity) const {
  PhysicalState s;
  s.Fe = velocity;
  s.He = velocity;  // Archetype A: velocity also drives mallet hardness.
  s.Xe = 0.5f;
  s.Ye = 0.5f;
  s.Fn = 0.0f;
  s.gate = true;
  return s;
}

void TranslationMatrix::applyPitchBend(PhysicalState&, float) const {
  // Pitch bypasses the translation matrix by design: MPE pitch-bend arrives as
  // a pre-computed Hz value -- per-note bend combined with the master
  // channel's -- and goes straight to Resonator::setPitchHz. This exists to
  // satisfy the declared interface and is never called by a voice layer.
}

void TranslationMatrix::applyTimbre(PhysicalState& state, float y01) const {
  state.Ye = y01;
}

void TranslationMatrix::applyPressure(PhysicalState& state, float z01) const {
  state.Fn = z01;
}

}  // namespace chalkwalk::physical
