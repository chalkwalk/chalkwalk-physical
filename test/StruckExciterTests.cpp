#include <catch2/catch_test_macros.hpp>

#include "Signal.h"
#include <chalkwalk/physical/StruckExciter.h>

#include <vector>

using namespace chalkwalk::physical;
using namespace anviltest;

namespace {

constexpr double kFs = 48000.0;

PhysicalState strike(float force, float hardness) {
  PhysicalState s;
  s.Fe = force;
  s.He = hardness;
  s.gate = true;
  return s;
}

std::vector<float> render(StruckExciter& ex, const PhysicalState& s, int n) {
  std::vector<float> out(size_t(n), 0.0f);
  ex.renderAdd(out.data(), n, s);
  return out;
}

}  // namespace

TEST_CASE("an untriggered exciter is silent", "[struck]") {
  StruckExciter ex;
  ex.prepare(kFs, 512);
  const auto out = render(ex, strike(1.0f, 0.5f), 2048);
  REQUIRE(peakAbs(out) == 0.0f);
}

// The interface says renderAdd is ADDITIVE and callers must zero the buffer.
// A silent regression to "replace" would erase whatever else fed the resonator.
TEST_CASE("renderAdd adds to the buffer rather than replacing it", "[struck]") {
  StruckExciter ex;
  ex.prepare(kFs, 512);
  const auto s = strike(1.0f, 0.5f);
  ex.trigger(s);

  const int n = 2048;
  std::vector<float> out(size_t(n), 5.0f);
  ex.renderAdd(out.data(), n, s);

  // Past the burst the buffer must be untouched, not zeroed.
  REQUIRE(out[size_t(n) - 1] == 5.0f);
  // Inside the burst it must have moved.
  REQUIRE(out[0] != 5.0f);
}

TEST_CASE("mallet hardness sets the burst length", "[struck]") {
  // He=0 is a 20 ms soft mallet; He=1 a 4 ms hard pick.
  struct Case { float he; double expectedMs; };
  for (Case c : {Case{0.0f, 20.0}, Case{0.5f, 12.0}, Case{1.0f, 4.0}}) {
    StruckExciter ex;
    ex.prepare(kFs, 512);
    const auto s = strike(1.0f, c.he);
    ex.trigger(s);

    const int n = 4096;
    const auto out = render(ex, s, n);
    const double lengthMs = double(lastAbove(out, 0.0f) + 1) / kFs * 1000.0;

    INFO("He " << c.he << " -> " << lengthMs << " ms, expected "
               << c.expectedMs << " ms");
    REQUIRE(lengthMs > c.expectedMs * 0.9);
    REQUIRE(lengthMs <= c.expectedMs * 1.01);
  }
}

TEST_CASE("a harder mallet is brighter", "[struck]") {
  StruckExciter soft;
  soft.prepare(kFs, 512);
  const auto softState = strike(1.0f, 0.0f);
  soft.trigger(softState);
  const auto softOut = render(soft, softState, 4096);

  StruckExciter hard;
  hard.prepare(kFs, 512);
  const auto hardState = strike(1.0f, 1.0f);
  hard.trigger(hardState);
  const auto hardOut = render(hard, hardState, 4096);

  const float softHigh = highFraction(softOut, kFs, 1000.0f);
  const float hardHigh = highFraction(hardOut, kFs, 1000.0f);
  INFO("high fraction: soft " << softHigh << " hard " << hardHigh);
  REQUIRE(hardHigh > softHigh);
}

TEST_CASE("excitation force scales the output", "[struck]") {
  auto renderAt = [](float force) {
    StruckExciter ex;
    ex.prepare(kFs, 512);
    const auto s = strike(force, 0.5f);
    ex.trigger(s);
    std::vector<float> out(4096, 0.0f);
    ex.renderAdd(out.data(), 4096, s);
    return rms(out);
  };

  const float quiet = renderAt(0.25f);
  const float loud  = renderAt(1.0f);
  INFO("rms: quiet " << quiet << " loud " << loud);
  REQUIRE(loud > quiet * 2.0f);
  REQUIRE(renderAt(0.0f) == 0.0f);
}

TEST_CASE("the burst envelope descends", "[struck]") {
  StruckExciter ex;
  ex.prepare(kFs, 512);
  const auto s = strike(1.0f, 0.0f);  // 20 ms, the longest burst
  ex.trigger(s);
  const auto out = render(ex, s, 4096);

  const size_t burst = lastAbove(out, 0.0f) + 1;
  const float head = rms(out, 0, burst / 4);
  const float tail = rms(out, burst - burst / 4, burst);
  INFO("head rms " << head << " tail rms " << tail);
  REQUIRE(head > tail * 3.0f);
}

// Characterisation, not a requirement: the RNG state deliberately survives
// reset(), so two identical strikes are not bit-identical. That is musically
// right -- a repeated strike is not a copy -- and is recorded here so a future
// change to seeding is a visible decision rather than an accident.
TEST_CASE("repeated strikes are not identical", "[struck][characterisation]") {
  StruckExciter ex;
  ex.prepare(kFs, 512);
  const auto s = strike(1.0f, 0.5f);

  ex.trigger(s);
  const auto first = render(ex, s, 4096);
  ex.reset();
  ex.trigger(s);
  const auto second = render(ex, s, 4096);

  REQUIRE(first != second);
  // ...but at the same level.
  REQUIRE(rms(first) > 0.0f);
  REQUIRE(std::fabs(rms(first) - rms(second)) < rms(first) * 0.35f);
}

TEST_CASE("output stays finite across the parameter space",
          "[struck][robustness]") {
  for (float force : {0.0f, 0.5f, 1.0f, 4.0f}) {
    for (float he : {0.0f, 0.5f, 1.0f}) {
      StruckExciter ex;
      ex.prepare(kFs, 512);
      const auto s = strike(force, he);
      ex.trigger(s);
      const auto out = render(ex, s, 4096);
      INFO("force " << force << " He " << he);
      REQUIRE(allFinite(out));
      REQUIRE(peakAbs(out) <= force * 1.5f + 1.0e-6f);
    }
  }
}

TEST_CASE("reset clears a burst in progress", "[struck]") {
  StruckExciter ex;
  ex.prepare(kFs, 512);
  const auto s = strike(1.0f, 0.0f);
  ex.trigger(s);
  (void)render(ex, s, 64);
  ex.reset();

  const auto out = render(ex, s, 4096);
  REQUIRE(peakAbs(out) == 0.0f);
}
