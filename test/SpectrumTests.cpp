// SPDX-License-Identifier: MIT
//
// Calibration of the spectral instrument, against signals whose answer is
// known before the measurement runs.
//
// This library's tuning story is that one instrument measured twice
// disagreed and both readings were correct. The lesson it drew was not "use
// two instruments" but "calibrate the instrument first" -- the autocorrelation
// detector in Signal.h reported an octave error until a synthetic sine caught
// it. These are the equivalent tests for the FFT side, and they exist so that
// a partial-stretch figure quoted by a resonator test can be believed.

#include <catch2/catch_test_macros.hpp>

#include "Spectrum.h"

#include <cmath>
#include <vector>

using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double hz, std::size_t n) {
  std::vector<float> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = static_cast<float>(std::sin(2.0 * kPi * hz * static_cast<double>(i) / kFs));
  return x;
}

// A harmonic stack in which partial k is deliberately detuned by
// `stretchCents(k)`, so the measurement has a known right answer.
std::vector<float> stack(double f0, int partials, std::size_t n,
                         double (*stretchCents)(int)) {
  std::vector<float> x(n, 0.0f);
  for (int k = 1; k <= partials; ++k) {
    const double hz = f0 * k * std::pow(2.0, stretchCents(k) / 1200.0);
    if (hz > 0.45 * kFs)
      break;
    const double amp = 1.0 / static_cast<double>(k);
    for (std::size_t i = 0; i < n; ++i)
      x[i] += static_cast<float>(amp * std::sin(2.0 * kPi * hz *
                                                static_cast<double>(i) / kFs));
  }
  return x;
}

double noStretch(int) { return 0.0; }

// +10 cents per partial above the first: a stretch of exactly the shape a
// waveguide's fractional delay produces, at a size a test can assert on.
double tenCentsPerPartial(int k) { return 10.0 * (k - 1); }

}  // namespace

TEST_CASE("the spectral peak reader finds a synthetic sine", "[spectrum]") {
  for (double hz : {110.0, 440.0, 2489.0, 7040.0}) {
    const auto x = sine(hz, 1 << 16);
    const double measured = peakNear(x, kFs, hz, 0.25);
    INFO("expected " << hz << " Hz, measured " << measured);
    CHECK(std::abs(centsBetweenHz(hz, measured)) < 1.0);
  }
}

TEST_CASE("a named partial is not confused with its neighbours", "[spectrum]") {
  // The narrow window earns its keep here: partial k-1 and k+1 are only 1/k
  // away, and a wide search locks onto one of them.
  const auto x = stack(220.0, 8, 1 << 16, noStretch);
  for (int k = 2; k <= 8; ++k) {
    const double want = 220.0 * k;
    const double got  = partialHz(x, kFs, want);
    INFO("partial " << k << ": wanted " << want << " Hz, measured " << got);
    CHECK(std::abs(centsBetweenHz(want, got)) < 1.0);
  }
}

TEST_CASE("partial stretch reads zero on a harmonic series", "[spectrum]") {
  const auto x = stack(220.0, 8, 1 << 16, noStretch);
  for (int k = 2; k <= 8; ++k) {
    const double cents = partialStretchCents(x, kFs, 220.0, k);
    INFO("partial " << k << ": " << cents << " cents");
    CHECK(std::abs(cents) < 1.0);
  }
}

TEST_CASE("partial stretch recovers a known stretch", "[spectrum]") {
  // The calibration that matters: a series stretched by a known amount must
  // read back as that amount. Without this, a resonator test quoting "21.4
  // cents at the eighth partial" is quoting an uncalibrated instrument.
  const auto x = stack(220.0, 8, 1 << 16, tenCentsPerPartial);
  for (int k = 2; k <= 8; ++k) {
    const double cents = partialStretchCents(x, kFs, 220.0, k);
    INFO("partial " << k << ": expected " << tenCentsPerPartial(k)
                    << " cents, measured " << cents);
    CHECK(std::abs(cents - tenCentsPerPartial(k)) < 1.5);
  }
}
