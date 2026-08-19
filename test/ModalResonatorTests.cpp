#include <catch2/catch_test_macros.hpp>

#include "Signal.h"
#include <chalkwalk/physical/ModalResonator.h>

#include <algorithm>
#include <vector>

using namespace chalkwalk::physical;
using namespace anviltest;

namespace {
constexpr double kFs = 48000.0;
}

// ModalResonator is a documented SKELETON: a one-pole low-pass passthrough,
// standing in for the eventual biquad bank (DESIGN §4.2). These tests pin the
// contract that survives the rewrite -- output shape, replace-not-add,
// finiteness -- and deliberately assert nothing about its timbre, which is
// meant to change.

TEST_CASE("the modal resonator replaces its output", "[modal]") {
  ModalResonator mr;
  mr.prepare(kFs, 512);

  const int n = 256;
  std::vector<float> exc(size_t(n), 0.0f);
  std::vector<float> outL(size_t(n), 9.0f), outR(size_t(n), 9.0f);
  mr.renderReplace(exc.data(), n, outL.data(), outR.data());

  REQUIRE(peakAbs(outL) < 1.0e-6f);
  REQUIRE(peakAbs(outR) < 1.0e-6f);
}

TEST_CASE("the modal resonator passes excitation through", "[modal]") {
  ModalResonator mr;
  mr.prepare(kFs, 512);

  const int n = 4096;
  std::vector<float> exc(size_t(n), 0.0f);
  for (int i = 0; i < n; ++i) exc[size_t(i)] = (i % 64 < 32) ? 0.5f : -0.5f;
  std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
  mr.renderReplace(exc.data(), n, outL.data(), outR.data());

  REQUIRE(allFinite(outL));
  REQUIRE(allFinite(outR));
  REQUIRE(rms(outL) > 0.0f);
}

TEST_CASE("damping darkens the modal resonator", "[modal]") {
  auto brightness = [](float damping) {
    ModalResonator mr;
    mr.prepare(kFs, 512);
    mr.setDamping(damping);

    const int n = 8192;
    std::vector<float> exc(size_t(n), 0.0f);
    unsigned int seed = 99u;
    for (size_t i = 0; i < exc.size(); ++i) {
      seed = seed * 1664525u + 1013904223u;
      exc[i] = float(int(seed)) / 2147483648.0f;
    }
    std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
    mr.renderReplace(exc.data(), n, outL.data(), outR.data());
    return highFraction(outL, kFs, 2000.0f);
  };

  INFO("bright " << brightness(0.0f) << " dark " << brightness(1.0f));
  REQUIRE(brightness(1.0f) < brightness(0.0f));
}

TEST_CASE("reset silences the modal resonator", "[modal]") {
  ModalResonator mr;
  mr.prepare(kFs, 512);

  const int n = 512;
  std::vector<float> exc(size_t(n), 1.0f);
  std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
  mr.renderReplace(exc.data(), n, outL.data(), outR.data());
  mr.reset();

  std::fill(exc.begin(), exc.end(), 0.0f);
  mr.renderReplace(exc.data(), n, outL.data(), outR.data());
  REQUIRE(peakAbs(outL) < 1.0e-9f);
  REQUIRE(peakAbs(outR) < 1.0e-9f);
}
