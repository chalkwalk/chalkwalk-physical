#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/physical/TranslationMatrix.h>

using namespace chalkwalk::physical;

// The translation matrix is the seam between MIDI/MPE and physics. It is
// currently thin, and these tests exist so that filling it in is a visible
// change rather than a silent one.

TEST_CASE("a note-on opens the gate and carries velocity into force",
          "[translation]") {
  const TranslationMatrix tm;
  const PhysicalState s = tm.onNoteOn(60, 0.75f);

  REQUIRE(s.gate);
  REQUIRE(s.Fe == 0.75f);
  // Archetype A: velocity also drives mallet hardness, so a hard strike is
  // both louder AND brighter. Recorded because it is a design choice, not a
  // coincidence.
  REQUIRE(s.He == 0.75f);
  REQUIRE(s.Xe == 0.5f);
  REQUIRE(s.Ye == 0.5f);
  REQUIRE(s.Fn == 0.0f);
}

TEST_CASE("a fresh physical state is inert", "[translation]") {
  const PhysicalState s;
  REQUIRE_FALSE(s.gate);
  REQUIRE(s.Fe == 0.0f);
  REQUIRE(s.Fn == 0.0f);
}

TEST_CASE("MPE timbre and pressure reach their physical variables",
          "[translation]") {
  const TranslationMatrix tm;
  PhysicalState s = tm.onNoteOn(60, 1.0f);

  tm.applyTimbre(s, 0.25f);
  REQUIRE(s.Ye == 0.25f);

  tm.applyPressure(s, 0.8f);
  REQUIRE(s.Fn == 0.8f);

  // Timbre must not disturb normal force, or a finger slide would change
  // bow pressure.
  tm.applyTimbre(s, 0.9f);
  REQUIRE(s.Fn == 0.8f);
}
