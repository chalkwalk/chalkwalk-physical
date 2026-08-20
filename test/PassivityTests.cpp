// SPDX-License-Identifier: MIT
//
// Does the instrument settle, and does it stay settled when the drive stops?
//
// `DESIGN §17` lists passivity as a graph-tier test -- total energy
// non-increasing once excitation is removed, swept across topologies. This
// file starts before the graph, because the property it tests is already
// being violated and a violation is worth a name.
//
// THE PHYSICS THIS ASSERTS. A bow is not an energy source; it is a constraint
// between two velocities. The friction force depends on the RELATIVE velocity
// v_bow - v_string, and that dependence is what makes it self-limiting: during
// stick the bow does positive work, during slip the string slides back against
// the friction and dissipates. Energy in per cycle grows more slowly with
// amplitude than the string's losses do, so a bow at constant force and speed
// reaches a steady amplitude within a handful of periods and stays there.
//
// Remove the string's velocity from that relation and the friction element
// stops being a friction element. It becomes a force source -- and a force
// source into a lossy resonator is an integrator.

#include <catch2/catch_test_macros.hpp>

#include "Signal.h"

#include <chalkwalk/physical/BowedExciter.h>
#include <chalkwalk/physical/WaveguideResonator.h>

#include <cmath>
#include <vector>

using namespace chalkwalk::physical;
using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;
constexpr int kBlock = 64;

struct Bowed {
  std::vector<float> out;
  std::vector<float> windowRms;  // one per 0.5 s
  std::vector<float> windowDc;
};

// Bow a string at CONSTANT force and CONSTANT bow speed, and report the
// energy in successive half-second windows.
//
// Continuous mode is the one that holds the bow speed still: it sweeps a
// simulated bow position at 0.15 units/s and only reverses at the ends, so
// for the first 6.6 seconds the bow velocity is a constant +0.15. Rate mode
// cannot be used for this at all -- its bow velocity is |dFn/dt|, so a
// constant force means a stationary bow and silence.
Bowed bowSteadily(double seconds, float normalForce, int mode = 2) {
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setDamping(0.1f);
  wg.setPitchHz(220.0f);

  BowedExciter ex;
  ex.prepare(kFs, kBlock);
  ex.setBowMode(mode);

  PhysicalState s;
  s.Fn = normalForce;
  s.gate = true;
  ex.trigger(s);

  Bowed b;
  const std::size_t n = std::size_t(seconds * kFs) / kBlock * kBlock;
  b.out.assign(n, 0.0f);
  std::vector<float> exc(kBlock), scratch(kBlock);
  for (std::size_t i = 0; i + kBlock <= n; i += kBlock) {
    std::fill(exc.begin(), exc.end(), 0.0f);
    ex.renderAdd(exc.data(), kBlock, s);
    wg.renderReplace(exc.data(), kBlock, b.out.data() + i, scratch.data());
  }

  const std::size_t w = std::size_t(0.5 * kFs);
  for (std::size_t from = 0; from + w <= n; from += w) {
    b.windowRms.push_back(rms(b.out, from, from + w));
    double mean = 0.0;
    for (std::size_t i = from; i < from + w; ++i) mean += double(b.out[i]);
    b.windowDc.push_back(float(mean / double(w)));
  }
  return b;
}

}  // namespace

TEST_CASE("a bowed string does not run away", "[passivity]") {
  // It did. At constant bow force and speed the string grew from rms 24.6 to
  // 192.5 over four seconds, and 99.9996% of the final window was DC -- the
  // friction solver settling on a near-constant force, and a loop of gain
  // 0.999 turning that into a thousandfold offset. A DC blocker in the loop
  // closed it: the same four seconds now go 5.98 -> 4.55, bounded and roughly
  // stationary.
  //
  // Roughly stationary is not equilibrium, and the next test says why not.
  for (int mode : {1, 2}) {
    const auto b = bowSteadily(4.0, 0.7f, mode);
    REQUIRE(b.windowRms.size() >= 8);
    REQUIRE(allFinite(b.out));

    const float first = b.windowRms.front();
    const float last  = b.windowRms.back();
    for (std::size_t i = 0; i < b.windowRms.size(); ++i)
      INFO("  mode " << mode << " window " << i << ": rms " << b.windowRms[i]
                     << " dc " << b.windowDc[i]);
    INFO("mode " << mode << " window rms: " << first << " -> " << last);
    CHECK(last < first * 1.5f);
    for (float w : b.windowRms)
      CHECK(w < first * 2.0f);
  }
}

TEST_CASE("the bow cannot feel the string, which is why it does not settle",
          "[passivity][characterisation]") {
  // KNOWN DEFECT, and the reason the test above says "roughly stationary"
  // rather than "settled".
  //
  // A real bow reaches a steady amplitude within a handful of periods because
  // the friction force depends on the RELATIVE velocity v_bow - v_string: the
  // bow does positive work while the string sticks and the string dissipates
  // against the friction while it slips, so energy in per cycle grows more
  // slowly with amplitude than the losses do. That is a negative feedback and
  // it needs v_string.
  //
  // BowedExciter has a setter for it, and nothing in the library ever calls
  // it. So vHat_ is permanently zero, and this is directly testable: the
  // exciter's output does not depend on the resonator it is driving. Bow a
  // 110 Hz string and an 880 Hz string and the excitation is BIT-IDENTICAL,
  // which is not a thing that can be true of a friction junction.
  //
  // It is not fixable in place either. renderAdd() produces a signal that is
  // THEN injected into a resonator, and that pipeline is feed-forward; a
  // friction junction is not a source but a constraint between two
  // velocities. `DESIGN §5.2`: a scalar root-find on relative velocity inside
  // a COUPLING, with the reaction applied to both sides.
  //
  // When that lands this test fails, which is the signal to delete it and
  // assert the equilibrium the test above cannot yet assert.
  auto excitationDriving = [](float pitch) {
    WaveguideResonator wg;
    wg.prepare(kFs, kBlock);
    wg.setDamping(0.1f);
    wg.setPitchHz(pitch);

    BowedExciter ex;
    ex.prepare(kFs, kBlock);
    ex.setBowMode(2);

    PhysicalState s;
    s.Fn = 0.7f;
    s.gate = true;
    ex.trigger(s);

    std::vector<float> excitation;
    std::vector<float> exc(kBlock), l(kBlock), r(kBlock);
    for (int b = 0; b < 1500; ++b) {
      std::fill(exc.begin(), exc.end(), 0.0f);
      ex.renderAdd(exc.data(), kBlock, s);
      excitation.insert(excitation.end(), exc.begin(), exc.end());
      wg.renderReplace(exc.data(), kBlock, l.data(), r.data());
    }
    return excitation;
  };

  const auto low  = excitationDriving(110.0f);
  const auto high = excitationDriving(880.0f);
  REQUIRE(low.size() == high.size());
  REQUIRE(peakAbs(low) > 1.0e-3f);

  std::size_t differing = 0;
  for (std::size_t i = 0; i < low.size(); ++i)
    if (low[i] != high[i]) ++differing;

  INFO(differing << " of " << low.size()
                 << " excitation samples differ between a 110 Hz string and "
                    "an 880 Hz one");
  CHECK(differing == 0);  // when this fails, the bow can feel the string
}

TEST_CASE("a string is passive once the drive is removed", "[passivity]") {
  // The property that does hold, and the one `DESIGN §17` will sweep across
  // topologies once there are topologies. A plucked string given no further
  // excitation must not gain energy in any window, ever.
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setDamping(0.1f);

  for (float pitch : {110.0f, 440.0f, 1760.0f}) {
    for (float damping : {0.0f, 0.3f, 0.6f}) {
      wg.reset();
      wg.setDamping(damping);
      wg.setPitchHz(pitch);

      const std::size_t n = std::size_t(3.0 * kFs);
      std::vector<float> l(n, 0.0f), r(n, 0.0f);
      std::vector<float> exc(n, 0.0f);
      exc[0] = 1.0f;  // one impulse, and nothing after it
      wg.renderReplace(exc.data(), int(n), l.data(), r.data());

      const std::size_t w = std::size_t(0.25 * kFs);
      float prev = rms(l, 0, w);
      for (std::size_t from = w; from + w <= n; from += w) {
        const float now = rms(l, from, from + w);
        INFO("pitch " << pitch << " damping " << damping << " window at "
                      << double(from) / kFs << "s: " << prev << " -> " << now);
        CHECK(now <= prev * 1.001f);  // 0.1% for float noise
        prev = now;
      }
    }
  }
}
