#include <catch2/catch_test_macros.hpp>

#include "Signal.h"
#include <chalkwalk/physical/BowedExciter.h>

#include <vector>

using namespace chalkwalk::physical;
using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;

PhysicalState bowState(float normalForce) {
  PhysicalState s;
  s.Fn = normalForce;
  s.gate = true;
  return s;
}

// Render `blocks` blocks of `blockSize`, calling renderAdd once per block so
// the per-block bow-velocity update runs the way Voice drives it.
std::vector<float> renderBlocks(BowedExciter& ex, PhysicalState s, int blocks,
                                int blockSize, float fnPerBlockDelta = 0.0f) {
  std::vector<float> out(size_t(blocks * blockSize), 0.0f);
  for (int b = 0; b < blocks; ++b) {
    ex.renderAdd(out.data() + b * blockSize, blockSize, s);
    s.Fn += fnPerBlockDelta;
  }
  return out;
}

}  // namespace

TEST_CASE("an ungated bow is silent", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  const auto out = renderBlocks(ex, bowState(1.0f), 16, 512);
  REQUIRE(peakAbs(out) == 0.0f);
}

TEST_CASE("release silences the bow", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(1);  // Auto: sounds without any gesture
  ex.trigger(bowState(1.0f));
  REQUIRE(rms(renderBlocks(ex, bowState(1.0f), 16, 512)) > 0.0f);

  ex.release();
  REQUIRE(peakAbs(renderBlocks(ex, bowState(1.0f), 16, 512)) == 0.0f);
}

TEST_CASE("zero normal force makes no sound", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(1);
  ex.trigger(bowState(0.0f));
  const auto out = renderBlocks(ex, bowState(0.0f), 16, 512);
  REQUIRE(peakAbs(out) < 1.0e-6f);
}

// Rate mode is the whole design idea: the bow is driven by the RATE of change
// of pressure, so a finger held still makes no sound however hard it presses.
// Without the vBow taper this settled at a nonzero static-friction force and
// pumped energy in forever -- the bug the taper in renderAdd() exists to fix.
TEST_CASE("in Rate mode a held pressure eventually goes quiet", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(0);
  ex.trigger(bowState(1.0f));

  const auto out = renderBlocks(ex, bowState(1.0f), 400, 512);
  const size_t n = out.size();
  const float tail = rms(out, n - 4096, n);
  INFO("tail rms " << tail);
  REQUIRE(tail < 1.0e-3f);
}

TEST_CASE("in Rate mode a moving pressure sounds", "[bowed]") {
  BowedExciter still;
  still.prepare(kFs, 512);
  still.setBowMode(0);
  still.trigger(bowState(0.1f));
  const float stillRms = rms(renderBlocks(still, bowState(0.1f), 64, 512));

  BowedExciter moving;
  moving.prepare(kFs, 512);
  moving.setBowMode(0);
  moving.trigger(bowState(0.1f));
  const float movingRms =
      rms(renderBlocks(moving, bowState(0.1f), 64, 512, 0.01f));

  INFO("still " << stillRms << " moving " << movingRms);
  REQUIRE(movingRms > stillRms);
}

TEST_CASE("Auto mode sounds without any gesture", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(1);
  ex.trigger(bowState(1.0f));

  const auto out = renderBlocks(ex, bowState(1.0f), 128, 512);
  REQUIRE(allFinite(out));
  REQUIRE(rms(out) > 1.0e-3f);
}

TEST_CASE("Continuous mode sounds without any gesture", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(2);
  ex.trigger(bowState(1.0f));

  const auto out = renderBlocks(ex, bowState(1.0f), 128, 512);
  REQUIRE(allFinite(out));
  REQUIRE(rms(out) > 1.0e-3f);
}

TEST_CASE("normal force scales the bow's output", "[bowed]") {
  auto level = [](float fn) {
    BowedExciter ex;
    ex.prepare(kFs, 512);
    ex.setBowMode(1);
    ex.trigger(bowState(fn));
    return rms(renderBlocks(ex, bowState(fn), 128, 512));
  };

  const float light = level(0.25f);
  const float heavy = level(1.0f);
  INFO("light " << light << " heavy " << heavy);
  REQUIRE(heavy > light);
}

// The bowed exciter drives the friction model from Z and Y directly, so Voice
// must not ALSO route them to the resonator or they are counted twice.
TEST_CASE("the bow suppresses resonator expression routing", "[bowed]") {
  BowedExciter ex;
  REQUIRE(ex.suppressResonatorExpression());
}

TEST_CASE("the junction solver stays finite across modes and forces",
          "[bowed][robustness]") {
  for (int mode : {0, 1, 2}) {
    for (float fn : {0.0f, 0.5f, 1.0f, 4.0f}) {
      for (float vHat : {-2.0f, 0.0f, 2.0f}) {
        BowedExciter ex;
        ex.prepare(kFs, 512);
        ex.setBowMode(mode);
        ex.trigger(bowState(fn));
        ex.setJunctionVelocity(vHat);

        const auto out = renderBlocks(ex, bowState(fn), 64, 512);
        INFO("mode " << mode << " Fn " << fn << " vHat " << vHat);
        REQUIRE(allFinite(out));
        REQUIRE(peakAbs(out) < 100.0f);
      }
    }
  }
}

TEST_CASE("reset returns the bow to its rest state", "[bowed]") {
  BowedExciter ex;
  ex.prepare(kFs, 512);
  ex.setBowMode(1);
  ex.trigger(bowState(1.0f));
  (void)renderBlocks(ex, bowState(1.0f), 32, 512);
  ex.reset();

  // reset() clears the gate, so nothing sounds until the next trigger.
  REQUIRE(peakAbs(renderBlocks(ex, bowState(1.0f), 32, 512)) == 0.0f);
}

TEST_CASE("a re-trigger does not inherit the last note's junction velocity",
          "[bowed]") {
  // vHat_ is the junction velocity fed back from the resonator. The Newton
  // solve is warm-started, so a trigger that leaves a stale vHat_ in place
  // starts from the velocity the PREVIOUS note ended on and converges
  // elsewhere -- the same gesture then gives a different attack depending on
  // what happened before it, which is the one thing an attack must not do.
  //
  // Auto mode, because the exciter has to be SOUNDING for this to be
  // observable at all: in the default mode with no gesture it renders
  // silence, and two silent buffers agree no matter what the solver did.
  //
  // Regression: extracted into chalkwalk-physical from a checkout that
  // predated the fix in its origin project.
  const auto attackAfter = [](float staleVelocity) {
    BowedExciter ex;
    ex.prepare(kFs, 512);
    ex.setBowMode(2);  // Auto: sounds without a gesture

    PhysicalState s;
    s.Fn = 0.5f;
    s.gate = true;

    // Play a note, leave a junction velocity behind, and release.
    ex.trigger(s);
    ex.setJunctionVelocity(staleVelocity);
    std::vector<float> scratch(512, 0.0f);
    ex.renderAdd(scratch.data(), 512, s);
    ex.release();

    // The next note's attack must not depend on that.
    ex.trigger(s);
    std::vector<float> out(512, 0.0f);
    ex.renderAdd(out.data(), 512, s);
    return out;
  };

  const auto clean = attackAfter(0.0f);
  REQUIRE(peakAbs(clean) > 0.0f);  // the test would be vacuous on silence
  REQUIRE(attackAfter(0.4f) == clean);
}
