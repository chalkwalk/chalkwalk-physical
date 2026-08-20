#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Signal.h"
#include <chalkwalk/physical/WaveguideResonator.h>

#include <cmath>
#include <vector>

using namespace chalkwalk::physical;
using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;

// One-sample impulse into the loop, then free ringing.
std::vector<float> pluck(WaveguideResonator& wg, int numSamples,
                         float amplitude = 1.0f) {
  std::vector<float> exc(size_t(numSamples), 0.0f);
  exc[0] = amplitude;
  std::vector<float> outL(size_t(numSamples), 0.0f);
  std::vector<float> outR(size_t(numSamples), 0.0f);
  wg.renderReplace(exc.data(), numSamples, outL.data(), outR.data());
  return outL;
}

WaveguideResonator makeString(float pitchHz, float damping = 0.0f) {
  WaveguideResonator wg;
  wg.prepare(kFs, 512);
  wg.setDamping(damping);
  wg.setPitchHz(pitchHz);
  return wg;
}

}  // namespace

// ---------------------------------------------------------------------------
// Calibrate the instrument before trusting it on the thing under test.
// A "wrong" pitch reading has been a measurement error more often than a bug.
// ---------------------------------------------------------------------------
TEST_CASE("the pitch detector reads a synthetic sine correctly",
          "[measurement]") {
  for (float f : {55.0f, 110.0f, 220.0f, 440.0f, 1760.0f}) {
    std::vector<float> sine(24000);
    for (size_t i = 0; i < sine.size(); ++i)
      sine[i] = std::sin(2.0f * 3.14159265f * f * float(i) / float(kFs));

    const float measured = estimatePitchHz(sine, kFs, 0, sine.size());
    INFO("target " << f << " Hz, measured " << measured << " Hz");
    REQUIRE(std::fabs(centsBetween(f, measured)) < 1.0f);
  }
}

// ---------------------------------------------------------------------------
// Tuning. This is what the Thiran all-pass is FOR: without sub-sample delay
// the pitch would quantise to sampleRate/N and be badly sharp or flat.
// ---------------------------------------------------------------------------
// SWEEP EVERY SEMITONE, not the octaves.
//
// An octave-only sweep is nearly useless here, and finding that out cost a
// deliberate sabotage: with the Thiran all-pass disabled entirely -- integer
// delay only -- an A1/A2/A3/A4 sweep still passed, because at 48 kHz those
// four pitches happen to land on fractional delays close to a whole sample.
// The residual error is (1 - D), so a test only has teeth if it visits values
// of D across the whole [0.5, 1.5) range, which a chromatic sweep does and
// four octaves do not.
TEST_CASE("a plucked string sounds the pitch it was asked for", "[waveguide]") {
  // A1 to A4: the range a string model is actually played in.
  // Measured worst case over the chromatic sweep at 48 kHz, 2026-08-18:
  // +5.06 cents at G4 (391.995 Hz). That is at the edge of audible for a
  // sustained tone, and it is NOT the intended accuracy -- see the
  // characterisation test below. 7 is a regression guard against it getting
  // worse, not a statement that 5 cents is fine.
  float worst = 0.0f;
  float worstAt = 0.0f;

  for (int semitone = 0; semitone <= 36; ++semitone) {
    const float target = 55.0f * std::pow(2.0f, float(semitone) / 12.0f);
    auto wg = makeString(target);
    const auto out = pluck(wg, 24000);

    REQUIRE(allFinite(out));
    REQUIRE(peakAbs(out) > 1.0e-4f);

    const float measured = estimatePitchHz(out, kFs, 2400, out.size());
    const float error = centsBetween(target, measured);
    if (std::fabs(error) > std::fabs(worst)) { worst = error; worstAt = target; }

    INFO("target " << target << " Hz, measured " << measured
                   << " Hz, error " << error << " cents");
    REQUIRE(std::fabs(error) < 7.0f);
  }
  INFO("worst over the sweep: " << worst << " cents at " << worstAt << " Hz");
  CHECK(std::fabs(worst) < 7.0f);
}

// KNOWN DEFECT, characterised rather than asserted away.
//
// The loop uses a FIRST-ORDER Thiran all-pass for its fractional delay. Its
// phase delay is accurate near DC and drifts as the loop shortens, so the
// tuning error grows with pitch. Its SIGN depends on where the fractional
// part D lands in [0.5, 1.5), so it is not a monotone sharpening -- which is
// why an octaves-only test missed it entirely.
//
// Measured over CHROMATIC sweeps at 48 kHz, 2026-08-18:
//     A1-A4 (55-440 Hz):    worst  +5.1 cents at G4
//     A5-A7 (880-3520 Hz):  range -19.3 to +43.0 cents
//
// Forty-three cents is close to a quarter tone. The top two octaves of this
// instrument are not in tune with anything, including themselves.
//
// The fix is a higher-order fractional-delay filter -- Thiran order 2-3, or
// Lagrange interpolation -- and it belongs on the roadmap, not in this test.
// The bounds below catch it getting worse, and the final CHECK fails if it
// gets FIXED, which is the signal to delete this test and tighten the sweep
// above.
TEST_CASE("high notes are audibly mistuned: first-order Thiran runs out",
          "[waveguide][characterisation]") {
  float worst = 0.0f;
  float worstAt = 0.0f;

  for (int semitone = 0; semitone <= 24; ++semitone) {
    const float target = 880.0f * std::pow(2.0f, float(semitone) / 12.0f);
    auto wg = makeString(target);
    const auto out = pluck(wg, 24000);
    const float measured = estimatePitchHz(out, kFs, 2400, out.size());
    const float error = centsBetween(target, measured);
    if (std::fabs(error) > std::fabs(worst)) { worst = error; worstAt = target; }
    INFO("target " << target << " Hz, measured " << measured
                   << " Hz, error " << error << " cents");
    REQUIRE(std::fabs(error) < 60.0f);  // if this fails, it got WORSE
  }

  INFO("worst over A5-A7: " << worst << " cents at " << worstAt << " Hz");
  // If this fails, the tuning got FIXED -- delete this test and tighten the
  // sweep above.
  CHECK(std::fabs(worst) > 5.0f);
}

// The loop LPF adds group delay that lengthens the effective loop and flattens
// the pitch. retune() subtracts it, so changing damping must NOT detune the
// string -- a player changing timbre does not expect the note to move.
TEST_CASE("damping does not detune the string", "[waveguide]") {
  const float target = 220.0f;

  auto quiet = makeString(target, 0.0f);
  const float pitchUndamped =
      estimatePitchHz(pluck(quiet, 24000), kFs, 2400, 24000);

  for (float damping : {0.3f, 0.6f, 0.9f}) {
    auto wg = makeString(target, damping);
    const float pitchDamped =
        estimatePitchHz(pluck(wg, 24000), kFs, 2400, 24000);
    const float drift = centsBetween(pitchUndamped, pitchDamped);
    INFO("damping " << damping << ": " << pitchUndamped << " -> "
                    << pitchDamped << " Hz (" << drift << " cents)");
    REQUIRE(std::fabs(drift) < 15.0f);
  }
}

// MPE Z and Y feed the same loop parameters as damping, via applyLoopParams().
// They must not detune the note either -- on an Osmose, Y moves continuously
// while a note is held.
TEST_CASE("MPE pressure and timbre do not detune the string", "[waveguide]") {
  const float target = 220.0f;
  auto reference = makeString(target);
  const float pitchNeutral =
      estimatePitchHz(pluck(reference, 24000), kFs, 2400, 24000);

  auto pressed = makeString(target);
  pressed.setPressure(1.0f);
  const float pitchPressed =
      estimatePitchHz(pluck(pressed, 24000), kFs, 2400, 24000);
  INFO("pressure: " << pitchNeutral << " -> " << pitchPressed);
  REQUIRE(std::fabs(centsBetween(pitchNeutral, pitchPressed)) < 15.0f);

  auto timbred = makeString(target);
  timbred.setTimbre(1.0f);
  const float pitchTimbred =
      estimatePitchHz(pluck(timbred, 24000), kFs, 2400, 24000);
  INFO("timbre: " << pitchNeutral << " -> " << pitchTimbred);
  REQUIRE(std::fabs(centsBetween(pitchNeutral, pitchTimbred)) < 15.0f);
}

// ---------------------------------------------------------------------------
// Energy: damping shortens the decay and darkens the tone.
// ---------------------------------------------------------------------------
TEST_CASE("more damping means a shorter decay", "[waveguide]") {
  const int n = 48000;
  float previousTail = 1.0e9f;

  for (float damping : {0.0f, 0.3f, 0.6f, 0.9f}) {
    auto wg = makeString(220.0f, damping);
    const auto out = pluck(wg, n);
    REQUIRE(allFinite(out));

    const float tail = rms(out, size_t(n) - 4800, size_t(n));
    INFO("damping " << damping << " tail rms " << tail);
    REQUIRE(tail < previousTail);
    previousTail = tail;
  }
}

TEST_CASE("more damping means a darker tone", "[waveguide]") {
  auto bright = makeString(220.0f, 0.0f);
  auto dark   = makeString(220.0f, 0.9f);

  const auto brightOut = pluck(bright, 24000);
  const auto darkOut   = pluck(dark, 24000);

  const float brightHigh = highFraction(brightOut, kFs, 2000.0f);
  const float darkHigh   = highFraction(darkOut, kFs, 2000.0f);
  INFO("high-frequency fraction: bright " << brightHigh << " dark " << darkHigh);
  REQUIRE(darkHigh < brightHigh);
}

// ---------------------------------------------------------------------------
// The physical damper. The documented T60 ladder in engageDamper() is the
// specification here; these assert its shape, not its exact numbers.
// ---------------------------------------------------------------------------
// Measured as the TIME the damper takes to silence the string, not as the
// level left at the end: every assertive rate reaches exactly zero within the
// window, so a tail-level comparison cannot order them and reads 0 < 0.
TEST_CASE("the damper mutes faster as its rate rises", "[waveguide][damper]") {
  const int n = 96000;      // 2 s
  const int ringFor = 4800; // let it sound before damping

  auto samplesToSilence = [&](float rate) {
    auto wg = makeString(220.0f);
    std::vector<float> exc(size_t(n), 0.0f);
    exc[0] = 1.0f;
    std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
    wg.renderReplace(exc.data(), ringFor, outL.data(), outR.data());
    wg.engageDamper(rate);
    wg.renderReplace(exc.data() + ringFor, n - ringFor, outL.data() + ringFor,
                     outR.data() + ringFor);
    REQUIRE(allFinite(outL));
    // -60 dB relative to the level it was ringing at when the damper engaged.
    const float reference = rms(outL, 0, size_t(ringFor));
    const float floorLevel = reference * 0.001f;
    return lastAbove(outL, floorLevel);
  };

  size_t previous = size_t(n) + 1;
  for (float rate : {0.5f, 0.7f, 0.9f, 1.0f}) {
    const size_t decay = samplesToSilence(rate);
    INFO("damper rate " << rate << " silenced after " << decay
                        << " samples (" << double(decay) / kFs << " s)");
    REQUIRE(decay < previous);
    previous = decay;
  }
}

TEST_CASE("a damper rate of zero is a no-op", "[waveguide][damper]") {
  const int n = 24000;
  auto free = makeString(220.0f);
  const float freeTail = rms(pluck(free, n), size_t(n) - 2400, size_t(n));

  auto zeroRate = makeString(220.0f);
  zeroRate.engageDamper(0.0f);
  const float zeroTail = rms(pluck(zeroRate, n), size_t(n) - 2400, size_t(n));

  REQUIRE_THAT(zeroTail, Catch::Matchers::WithinRel(freeTail, 1.0e-5f));
}

TEST_CASE("releasing the damper lets the string ring again",
          "[waveguide][damper]") {
  auto wg = makeString(220.0f);
  wg.engageDamper(1.0f);
  wg.releaseDamper();

  auto free = makeString(220.0f);
  const int n = 24000;
  const float released = rms(pluck(wg, n), size_t(n) - 2400, size_t(n));
  const float never    = rms(pluck(free, n), size_t(n) - 2400, size_t(n));

  REQUIRE_THAT(released, Catch::Matchers::WithinRel(never, 1.0e-5f));
}

// ---------------------------------------------------------------------------
// Contract and robustness.
// ---------------------------------------------------------------------------
TEST_CASE("renderReplace replaces its output, it does not add", "[waveguide]") {
  auto wg = makeString(220.0f);
  const int n = 256;
  std::vector<float> exc(size_t(n), 0.0f);
  std::vector<float> outL(size_t(n), 9.0f), outR(size_t(n), 9.0f);

  wg.renderReplace(exc.data(), n, outL.data(), outR.data());

  REQUIRE(peakAbs(outL) < 1.0e-6f);
  REQUIRE(peakAbs(outR) < 1.0e-6f);
}

TEST_CASE("reset silences a ringing string", "[waveguide]") {
  auto wg = makeString(220.0f);
  (void)pluck(wg, 4800);
  wg.reset();

  const int n = 4800;
  std::vector<float> exc(size_t(n), 0.0f);
  std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
  wg.renderReplace(exc.data(), n, outL.data(), outR.data());

  REQUIRE(peakAbs(outL) < 1.0e-9f);
  REQUIRE(peakAbs(outR) < 1.0e-9f);
}

TEST_CASE("output stays finite and bounded across the parameter space",
          "[waveguide][robustness]") {
  const int n = 8192;
  // Sustained noise excitation is the worst case: it keeps pumping energy in.
  std::vector<float> exc(size_t(n), 0.0f);
  unsigned int seed = 12345u;
  for (size_t i = 0; i < exc.size(); ++i) {
    seed = seed * 1664525u + 1013904223u;
    exc[i] = float(int(seed)) / 2147483648.0f;
  }

  for (float pitch : {20.0f, 55.0f, 440.0f, 4000.0f, 12000.0f}) {
    for (float damping : {0.0f, 0.5f, 1.0f}) {
      for (float curve : {-1.0f, 0.0f, 1.0f}) {
        for (float z : {0.0f, 1.0f}) {
          WaveguideResonator wg;
          wg.prepare(kFs, 512);
          wg.setExpressionCurve(curve);
          wg.setDamping(damping);
          wg.setPressure(z);
          wg.setTimbre(z);
          wg.setPitchHz(pitch);

          std::vector<float> outL(size_t(n), 0.0f), outR(size_t(n), 0.0f);
          wg.renderReplace(exc.data(), n, outL.data(), outR.data());

          INFO("pitch " << pitch << " damping " << damping << " curve " << curve
                        << " z " << z);
          REQUIRE(allFinite(outL));
          REQUIRE(allFinite(outR));
          REQUIRE(peakAbs(outL) < 1000.0f);
          REQUIRE(peakAbs(outR) < 1000.0f);
        }
      }
    }
  }
}

TEST_CASE("an unprepared resonator renders silence rather than reading garbage",
          "[waveguide][robustness]") {
  WaveguideResonator wg;  // no prepare()
  const int n = 128;
  std::vector<float> exc(size_t(n), 1.0f);
  std::vector<float> outL(size_t(n), 7.0f), outR(size_t(n), 7.0f);

  wg.renderReplace(exc.data(), n, outL.data(), outR.data());

  REQUIRE(peakAbs(outL) == 0.0f);
  REQUIRE(peakAbs(outR) == 0.0f);
}

TEST_CASE("a zero or negative pitch falls back to A440 rather than dividing by zero",
          "[waveguide][robustness]") {
  auto wg = makeString(0.0f);
  const auto out = pluck(wg, 24000);
  REQUIRE(allFinite(out));
  const float measured = estimatePitchHz(out, kFs, 2400, 24000);
  INFO("measured " << measured << " Hz");
  REQUIRE(std::fabs(centsBetween(440.0f, measured)) < 10.0f);
}

// ---------------------------------------------------------------------------
// The fractional delay, tested directly rather than through worst-case error.
//
// A loop of integer length N can only sound sampleRate/N, so without a working
// fractional delay the pitch is a STAIRCASE: a range of requested frequencies
// all land on the same N and come out identical. The Thiran all-pass exists to
// fill in between those steps, and this is the only test here that actually
// distinguishes it from a plain integer delay -- the error-bound tests above
// do not, because disabling the all-pass entirely costs only about a cent in
// the A1-A4 range.
//
// Sweeping 1000-1030 Hz at 48 kHz spans loop lengths 48.0 down to 46.6, so an
// integer-only loop would produce at most two distinct pitches across the whole
// sweep.
// ---------------------------------------------------------------------------
TEST_CASE("pitch tracks a fine sweep continuously, not in integer steps",
          "[waveguide][tuning]") {
  std::vector<float> measured;
  for (int hz = 1000; hz <= 1030; ++hz) {
    auto wg = makeString(float(hz));
    measured.push_back(estimatePitchHz(pluck(wg, 24000), kFs, 2400, 24000));
  }

  // Count how many genuinely distinct pitches came out. Integer-stepping gives
  // a handful; a working fractional delay gives one per request.
  int distinct = 1;
  for (size_t i = 1; i < measured.size(); ++i)
    if (std::fabs(centsBetween(measured[i - 1], measured[i])) > 0.5f) ++distinct;

  // Measured 2026-08-18: 14 distinct with the all-pass working, 2 with it
  // disabled. It is 14 rather than 31 because of the DETECTOR, not the
  // resonator -- at a 48-sample lag, parabolic interpolation on an integer-lag
  // autocorrelation cannot resolve 1.7 cents reliably. The bound is therefore
  // set on the contrast this measurement can actually see, not on a precision
  // it cannot.
  INFO("distinct pitches over 31 requests: " << distinct);
  REQUIRE(distinct > 8);

  // And the sweep must rise overall.
  REQUIRE(measured.back() > measured.front());
  REQUIRE(centsBetween(measured.front(), measured.back()) > 40.0f);
}
