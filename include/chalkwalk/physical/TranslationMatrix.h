#pragma once

namespace chalkwalk::physical {

// Universal physical variables produced by the Translation Matrix
// (spec §2.1). Per-voice, audio-rate when MPE expression streams demand it.
struct PhysicalState {
  float Fe = 0.0f;  // Excitation force / amplitude
  float He = 0.0f;  // Mallet hardness / spectral tilt (0..1)
  float Xe = 0.5f;  // Spatial excitation X (0..1, geometry-relative)
  float Ye = 0.5f;  // Spatial excitation Y (0..1)
  float Fn = 0.0f;  // Normal force (continuous pressure for friction)
  bool gate = false;
};

class TranslationMatrix {
 public:
  // Map a freshly-triggered MIDI note to initial PhysicalState.
  PhysicalState onNoteOn(int midiNote, float velocity) const;

  // Update an existing voice's PhysicalState from MPE expression updates.
  void applyPitchBend(PhysicalState& state, float semitones) const;
  void applyTimbre(PhysicalState& state, float y01) const;
  void applyPressure(PhysicalState& state, float z01) const;
};

}  // namespace chalkwalk::physical
