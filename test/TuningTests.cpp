// SPDX-License-Identifier: MIT
//
// Is the string in tune, and does it STAY in tune as it is damped?
//
// This is the headline claim of a physically modelled string, and it was
// wrong -- but not for the recorded reason, and the difference matters enough
// to write down.
//
// THERE ARE TWO DEFECTS HERE, and cross-checking the measurement is what
// separated them. They had been recorded as one.
//
// The roadmap quoted +5.1 cents low and -19.3 to +43.0 cents in the top two
// octaves, from an autocorrelation detector, and said to cross-check before
// acting because sibling projects had seen measurement error. Cross-checked by
// FFT, the FUNDAMENTAL turned out to be within about a cent -- which looked
// like the recorded finding was wrong, and it is not. The two detectors
// measure different things and both are right:
//
//   An FFT peak near f0 finds the FUNDAMENTAL.
//   Autocorrelation finds the period of the WHOLE waveform, so stretched
//   partials pull it.
//
// Measuring the partials directly settled it. At 220 Hz they sat within 0.4
// cents of a harmonic series; at 2489 Hz the eighth partial was 21.4 cents
// sharp. The string really was, in the roadmap's words, "not in tune with
// anything, including itself" -- INHARMONICITY from the first-order Thiran's
// frequency-dependent phase delay, exactly as originally diagnosed.
//
// FIXED, by a fifth-order Lagrange fractional delay. What survives is a
// near-Nyquist artefact that is not a tuning error at all, and there is a
// test at the bottom that proves it by moving Nyquist.
//
// The SECOND defect is new, and was hidden underneath the first. The loop
// filter's delay was being compensated with the GROUP delay formula where a
// resonator needs the PHASE delay. They agree at DC and diverge as pitch and
// damping rise, so the fundamental itself walked out of tune as the string was
// damped.
//
//     damping   group delay   phase delay      (worst |cents|, top two octaves)
//       0.3        3.5             2.2
//       0.5       17.7             1.2
//       0.7       55.6             7.0
//       1.0      220.7            63.4
//
// So this file measures the fundamental ACROSS the damping range -- a sweep at
// one damping value cannot see the second defect at all -- and measures the
// partials separately for the first.

#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/physical/WaveguideResonator.h>

#include "Spectrum.h"

#include <cmath>
#include <string>
#include <vector>

using namespace chalkwalk::physical;
using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// The spectral instrument lives in Spectrum.h, calibrated there against
// synthetic signals. This file was where it was written; it is shared now
// because the fractional-delay work reads partials too, and two copies of a
// measurement is how a suite starts disagreeing with itself.

double centsErrorAt(int midi, float damping) {
  const double target = 440.0 * std::pow(2.0, (midi - 69) / 12.0);

  WaveguideResonator wg;
  wg.prepare(kFs, 512);
  wg.setDamping(damping);
  wg.setPitchHz(static_cast<float>(target));
  wg.reset();
  wg.setPitchHz(static_cast<float>(target));

  const int n = 1 << 16;
  std::vector<float> exc(static_cast<std::size_t>(n), 0.0f);
  for (int i = 0; i < 64; ++i)
    exc[static_cast<std::size_t>(i)] = (i % 2) ? -0.5f : 0.5f;
  std::vector<float> l(static_cast<std::size_t>(n), 0.0f);
  std::vector<float> r(static_cast<std::size_t>(n), 0.0f);
  wg.renderReplace(exc.data(), n, l.data(), r.data());

  return centsBetweenHz(target, fundamentalHz(l, kFs, target));
}

double worstOver(int loMidi, int hiMidi, float damping) {
  double worst = 0.0;
  for (int midi = loMidi; midi <= hiMidi; midi += 3)
    worst = std::max(worst, std::abs(centsErrorAt(midi, damping)));
  return worst;
}

}  // namespace

TEST_CASE("the playing range stays in tune at every damping", "[tuning]") {
  // A1 to A5, which is where an instrument is mostly played. This is the
  // assertion that would have caught the bug: at one damping value it passes
  // either way, and across the range it does not.
  for (float damping : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
    const double worst = worstOver(33, 81, damping);
    INFO("damping " << damping << ", worst " << worst << " cents");
    CHECK(worst < 1.5);  // measures 1.12 at the worst damping
  }
}

TEST_CASE("damping does not detune the string", "[tuning]") {
  // The property the compensation exists for, stated directly: turning the
  // damping up must not move the pitch. With the group-delay formula this
  // moved by over two semitones at the top of the range.
  //
  // The bound is still 65 cents, and that is not slack: at A7 and damping 1.0
  // it measures 56.5. See "the top two octaves are usable" for what that
  // residual is -- a resonance peak dragged by a steep loop gain, not a mode
  // in the wrong place.
  for (int midi : {45, 69, 93, 105}) {
    const double dry = centsErrorAt(midi, 0.0f);
    for (float damping : {0.25f, 0.5f, 0.75f, 1.0f}) {
      const double wet = centsErrorAt(midi, damping);
      INFO("midi " << midi << " damping " << damping << ": " << dry << " -> " << wet);
      CHECK(std::abs(wet - dry) < 65.0);
    }
  }
}

TEST_CASE("the top two octaves are usable", "[tuning]") {
  // Looser than the playing range at the heaviest damping, and the reason has
  // changed. It used to be the first-order Thiran's phase-delay error; that
  // is gone, and these three now measure about a tenth of what they did.
  //
  // What is left at damping 1.0 -- 62.5 cents at A7 -- is NOT a tuning error,
  // and saying so cost two wrong explanations. Modelling the loop directly
  // puts the phase condition at 3519.996 Hz for a nominal 3520: the mode is
  // exactly where it was asked to be. What moves is the PEAK. At loopAlpha_
  // 0.8 a thirteen-sample loop has a broad resonance on a steeply falling
  // loop gain, so the spectral maximum is dragged below the mode -- and this
  // detector reads maxima. At A4, same damping, it is 0.10 cents, because the
  // loop is eight times longer and the resonance eight times narrower.
  //
  // A damped resonator's peak genuinely does sit below its undamped
  // frequency, so some of this is right. The rest is the one-pole being a
  // crude model of material damping, which is a body-vocabulary problem and
  // not a tuning one.
  for (float damping : {0.0f, 0.25f, 0.5f}) {
    const double worst = worstOver(84, 105, damping);
    INFO("damping " << damping << ", worst " << worst << " cents");
    CHECK(worst < 2.0);  // measures 1.06 at damping 0.5
  }
  const double heavy = worstOver(84, 105, 1.0f);
  INFO("heaviest damping, worst " << heavy << " cents");
  CHECK(heavy < 70.0);
}


// ===========================================================================
// The OTHER defect: the partials are stretched, and it is not fixed.
// ===========================================================================

namespace {

// A plucked string, rendered long enough to resolve partials.
std::vector<float> pluck(double target, double sampleRate) {
  WaveguideResonator wg;
  wg.prepare(sampleRate, 512);
  wg.setDamping(0.0f);
  wg.setPitchHz(static_cast<float>(target));

  const int n = 1 << 16;
  std::vector<float> exc(static_cast<std::size_t>(n), 0.0f);
  exc[0] = 1.0f;
  std::vector<float> l(static_cast<std::size_t>(n), 0.0f);
  std::vector<float> r(static_cast<std::size_t>(n), 0.0f);
  wg.renderReplace(exc.data(), n, l.data(), r.data());
  return l;
}

// How far the k-th partial of a plucked string sits from k times its
// fundamental. The measurement itself is Spectrum.h's, calibrated there.
double stretchAtRate(double target, int k, double sampleRate) {
  return partialStretchCents(pluck(target, sampleRate), sampleRate, target, k);
}

double stretchAt(double target, int k) { return stretchAtRate(target, k, kFs); }

}  // namespace

TEST_CASE("low notes have a harmonic series", "[tuning]") {
  // Where the instrument is mostly played, the partials really are harmonic.
  for (int k = 2; k <= 8; ++k) {
    const double cents = stretchAt(220.0, k);
    INFO("partial " << k << " of 220 Hz: " << cents << " cents");
    CHECK(std::abs(cents) < 1.5);
  }
}

TEST_CASE("the harmonic series is harmonic across the range", "[tuning]") {
  // Partials below 12 kHz, over four octaves of fundamental. This replaces a
  // characterisation test that held a known defect at arm's length: with a
  // first-order Thiran all-pass the eighth partial of a 2489 Hz string sat
  // 21.4 cents sharp, and a 220 Hz string was already within 0.4 cents, so
  // the series was wrong in a way that got worse with pitch. A fifth-order
  // Lagrange fractional delay closed it -- see the near-Nyquist test below
  // for the part that is NOT closed, and why it cannot be.
  for (double f0 : {220.0, 440.0, 880.0, 1760.0, 2489.0}) {
    for (int k = 2; k <= 8; ++k) {
      if (f0 * k > 12000.0)
        break;
      const double cents = stretchAt(f0, k);
      INFO("partial " << k << " of " << f0 << " Hz (" << (f0 * k / 1000.0)
                      << " kHz): " << cents << " cents");
      CHECK(std::abs(cents) < 2.0);
    }
  }
}

TEST_CASE("what stretch remains is a Nyquist limit, not a tuning error",
          "[tuning]") {
  // RECORDED, BECAUSE IT LOOKS LIKE A DEFECT AND IS NOT.
  //
  // At 48 kHz the eighth partial of a 2489 Hz string sits at 19.9 kHz, which
  // is 0.83 of Nyquist, and it stays several cents sharp however good the
  // fractional delay is. Raising the interpolator order barely moves it:
  // about 10 cents at order 5, 8 at order 9, 4 at order 19 -- twenty taps in
  // a nineteen-sample loop, which is not a delay line any more.
  //
  // The reason it cannot be chased is that it is not a property of the
  // partial. It is a property of the partial's distance from Nyquist, and the
  // test for that is to move Nyquist. THE SAME PARTIAL OF THE SAME NOTE, at
  // 96 kHz, is harmonic -- which is the whole claim, so it is asserted rather
  // than described.
  //
  // Everything below 12 kHz at 48 kHz is covered by the test above; this is
  // the boundary of that claim, stated where someone will find it.
  const double f0 = 2489.0;
  const int k = 8;

  const double at48 = stretchAt(f0, k);
  const double at96 = stretchAtRate(f0, k, 96000.0);
  INFO("partial " << k << " of " << f0 << " Hz = " << (f0 * k / 1000.0)
                  << " kHz: " << at48 << " cents at 48 kHz, " << at96
                  << " cents at 96 kHz");
  CHECK(std::abs(at48) > 2.0);   // the artefact is present at 48 kHz
  CHECK(std::abs(at96) < 1.0);   // and gone when Nyquist moves
}
