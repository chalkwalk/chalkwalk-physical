#include <chalkwalk/physical/WaveguideResonator.h>

#include <algorithm>
#include <chalkwalk/physical/Constants.h>

#include <cmath>

namespace chalkwalk::physical {

namespace {
// Lowest supported pitch; buffer is sized for this at prepare() time.
constexpr float kMinFreqHz = 20.0f;

// Base feedback gain at DC per loop cycle.
// T60 at f Hz with gain g: T60 = -3 / (f * log10(g))
// g = 0.999 → ~15 s at A4, adjusted downward by setDamping().
constexpr float kBaseFeedbackGain = 0.999f;
}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void WaveguideResonator::prepare(double sampleRate, int /*maxBlockSize*/) {
  sampleRate_ = sampleRate;
  const int maxDelay =
      static_cast<int>(std::ceil(sampleRate / static_cast<double>(kMinFreqHz))) + 4;
  delayBuf_.assign(static_cast<size_t>(maxDelay), 0.0f);
  reset();
  retune();
}

void WaveguideResonator::reset() {
  std::fill(delayBuf_.begin(), delayBuf_.end(), 0.0f);
  writePos_     = 0;
  loopState_    = 0.0f;
  dcX1_         = dcY1_ = 0.0f;
  damperActive_ = false;
  damperGain_   = 1.0f;
  damperDecay_  = 1.0f;
}

// ---------------------------------------------------------------------------
// Parameter setters
// ---------------------------------------------------------------------------

void WaveguideResonator::setDamping(float d) {
  baseDamping_ = d;
  applyLoopParams();
}

void WaveguideResonator::setPressure(float z01) {
  pressureAmt_ = z01;
  applyLoopParams();
}

void WaveguideResonator::setTimbre(float y01) {
  timbreAmt_ = y01;
  applyLoopParams();
}

void WaveguideResonator::setExpressionCurve(float c) {
  expressionCurve_ = c;
  applyLoopParams();
}

// Combine base damping, MPE Z (pressure) and MPE Y (timbre) into loop params.
// expressionCurve_ [-1,1] → exponent [0.25,4]: -1=log (sensitive at low values),
// 0=linear, +1=exp (gentle at low — right for Osmose where Z tracks key travel).
// Z gets a small weight (subtle brightness tilt); Y gets the larger weight
// (primary per-note damping — on Osmose Y only fires after Z is at 100%).
void WaveguideResonator::applyLoopParams() {
  const float exponent = std::pow(4.0f, expressionCurve_);
  const float shapedZ  = pressureAmt_ > 0.0f ? std::pow(pressureAmt_, exponent) : 0.0f;
  const float shapedY  = timbreAmt_   > 0.0f ? std::pow(timbreAmt_,   exponent) : 0.0f;
  const float headroom = 1.0f - baseDamping_;
  const float combined = baseDamping_ + headroom * (shapedZ * 0.25f + shapedY * 0.75f);
  loopAlpha_    = std::min(0.9f, combined * 0.8f);
  feedbackGain_ = kBaseFeedbackGain - baseDamping_ * 0.003f;
  retune();  // recompute delay to cancel LPF phase delay at new loopAlpha_
}

void WaveguideResonator::setPitchHz(float hz) {
  pitchHz_ = (hz > 0.0f) ? hz : 440.0f;
  retune();
}

// ---------------------------------------------------------------------------
// Internal: split the loop delay into an integer offset and a fractional
// remainder for the interpolator.
//
// Total loop delay: L = sampleRate / pitchHz, less the loop filter's own
// phase delay (see retune()).
// ---------------------------------------------------------------------------

void WaveguideResonator::engageDamper(float rate) {
  if (rate <= 0.0f) return;
  damperActive_ = true;
  // rate² gives a concave curve: the lower half of the slider is almost
  // inert and the assertive piano-damper region sits in the upper third.
  // T60 is interpolated in log-space between kMaxT60s and kMinT60s:
  //   rate=0.25  →  T60 ≈  265 s  (imperceptible vs. a ~12 s natural decay)
  //   rate=0.50  →  T60 ≈   40 s  (subtle shortening)
  //   rate=0.70  →  T60 ≈  3.5 s  (clear piano-style damper)
  //   rate=0.80  →  T60 ≈  760 ms (assertive damper)
  //   rate=0.90  →  T60 ≈  140 ms (hard damper)
  //   rate=1.00  →  T60 ≈   20 ms (near-instant hard mute)
  constexpr float kMaxT60s = 500.0f;
  constexpr float kMinT60s =   0.02f;
  const float rateExp = rate * rate;
  const float T60_samples = static_cast<float>(sampleRate_) * kMaxT60s
                            * std::pow(kMinT60s / kMaxT60s, rateExp);
  damperDecay_ = std::pow(10.0f, -3.0f / T60_samples);
  // Keep current damperGain_ — no reset, so a double note-off won't click.
}

void WaveguideResonator::releaseDamper() {
  damperActive_ = false;
  damperGain_   = 1.0f;
  damperDecay_  = 1.0f;
}

// Lagrange fractional-delay coefficients: h_k = prod_{j != k} (d - j)/(k - j),
// giving a delay of `d` samples measured from the first tap.
//
// WHY NOT A THIRAN ALL-PASS, which is the textbook choice here. A first-order
// Thiran was the textbook choice here, and it stretched the harmonic series:
// its phase delay is accurate near DC and drifts upward with frequency, so it
// does not delay every partial by the same fraction of a period. The loop
// then tunes each partial slightly differently. Measured at 2489 Hz, the
// eighth partial sat 21.4 cents sharp while the FUNDAMENTAL was within a cent
// -- which is why an FFT peak at f0 called the note in tune and the ear did
// not agree.
//
// Raising the Thiran's order would narrow that drift. Lagrange is chosen over
// it for two reasons beyond accuracy:
//
//   It has NO STATE. retune() runs whenever pitch, damping or expression
//   moves, which is to say under a ringing note, and swapping the
//   coefficients of a recursive all-pass while it rings is a click. An FIR
//   cannot do that.
//
//   It cannot go unstable. A Thiran all-pass is only stable for a bounded
//   fractional delay, which is a constraint the caller has to be trusted to
//   respect; this has none.
//
// The cost is a small magnitude droop approaching Nyquist, where a waveguide
// loop is already losing energy to the damping filter by design.
//
// WHAT ORDER DOES NOT FIX. Raising the order does not make a partial near
// NYQUIST harmonic, and no practical order does: at 48 kHz the eighth partial
// of a 2489 Hz string sits at 19.9 kHz, which is 0.83 of Nyquist, and it
// stays about 10 cents sharp at order 5, 8 cents at order 9, and 4 cents at
// order 19 -- twenty taps in a nineteen-sample loop, which is no longer a
// delay line. The residual is not a property of the partial's frequency; it
// is a property of its distance from Nyquist. The same 19.9 kHz partial of
// the same note, rendered at 96 kHz, is within 0.05 cents. TuningTests.cpp
// asserts exactly that, because a limit that moves when the sample rate moves
// is a sample-rate limit and should be recorded as one rather than chased.
void WaveguideResonator::computeFractionalDelay(float d) {
  for (int k = 0; k <= kFracOrder; ++k) {
    float h = 1.0f;
    for (int j = 0; j <= kFracOrder; ++j) {
      if (j == k) continue;
      h *= (d - static_cast<float>(j)) / static_cast<float>(k - j);
    }
    fracCoeff_[k] = h;
  }
}

void WaveguideResonator::retune() {
  if (delayBuf_.empty()) return;

  const float L = static_cast<float>(sampleRate_) / pitchHz_;

  // The loop LPF delays the signal, which lengthens the effective loop and
  // flattens the pitch. Subtract it, so that changing loopAlpha_ (via damping
  // or expression) does not detune the string.
  //
  // PHASE DELAY, NOT GROUP DELAY, and that distinction was a real bug rather
  // than pedantry. A resonator tunes by a PHASE condition -- the round trip
  // must come back in phase at the fundamental -- so what has to be cancelled
  // is how far the filter shifts that one frequency, not how it delays an
  // envelope. The two agree at DC and diverge as the pitch and the damping
  // rise, which is exactly where the instrument was going out of tune.
  //
  // Measured across chromatic sweeps at 48 kHz, worst error in the top two
  // octaves, before and after:
  //
  //     damping   group delay   phase delay
  //       0.3        3.5             2.2
  //       0.5       17.7             1.2
  //       0.7       55.6             7.0
  //       1.0      220.7            63.4      (loopAlpha_ 0.8, the maximum)
  //
  // and the whole range up to A5 goes from as much as 20.1 cents to 1.1.
  //
  // A RESIDUAL REMAINS at the very top with heavy damping -- 62 cents at A7
  // with damping 1.0 -- and it has outlived two wrong explanations. It was
  // blamed on the first-order Thiran, and the Thiran is gone. It was then
  // blamed on this cancellation being evaluated once rather than solved to a
  // fixed point, and that was wrong too: modelling the loop directly puts the
  // PHASE CONDITION at 3519.996 Hz for a nominal 3520, which is exact. There
  // is no fixed point to solve.
  //
  // What actually moves is the PEAK of the response, not the mode. At
  // loopAlpha_ 0.8 a 13-sample loop has a broad resonance sitting on a loop
  // gain that falls steeply with frequency, so the spectral maximum is dragged
  // below the mode frequency -- modelled at -25 cents, measured by FFT peak at
  // -62, the rest being what an FFT peak does to a broad, fast-decaying
  // resonance. At A4 the same damping costs 0.10 cents, because the loop is
  // eight times longer and the resonance eight times narrower.
  //
  // Some of that is even correct: a damped resonator's peak really does sit
  // below its undamped frequency. The rest is the one-pole being a crude model
  // of material damping. Either way it is not a bug in this function, and the
  // fix if one is wanted is a better damping filter -- not a better
  // cancellation.
  const float omega0    = 2.0f * kPi * pitchHz_ / static_cast<float>(sampleRate_);
  const float a         = loopAlpha_;
  const float lpfDelay  = (omega0 > 1e-6f)
      ? std::atan2(a * std::sin(omega0), 1.0f - a * std::cos(omega0)) / omega0
      : (a < 1.0f ? a / (1.0f - a) : 0.0f);

  // The DC blocker's phase delay, cancelled for the same reason and by the
  // same argument as the loop filter's. It is NEGATIVE -- a differentiator
  // zero at DC is a phase advance -- so this lengthens the loop rather than
  // shortening it. It is a fraction of a sample above 100 Hz and about 1.5
  // samples at the 20 Hz bottom of the range.
  const float dcDelay = (omega0 > 1e-6f)
      ? -(std::atan2(std::sin(omega0), 1.0f - std::cos(omega0)) -
          std::atan2(kDcBlockR * std::sin(omega0),
                     1.0f - kDcBlockR * std::cos(omega0))) / omega0
      : 0.0f;

  const float Lcomp = L - lpfDelay - dcDelay;

  // Split the loop delay into an integer read offset and a fractional part
  // handled by the interpolator.
  //
  // WHERE THE FRACTION LANDS IS THE WHOLE GAME. A Lagrange interpolator is
  // EXACTLY linear phase -- and so exactly harmonic -- when the fractional
  // delay is kFracOrder/2, because the coefficients are then symmetric; the
  // error grows with the distance from it. The pitch can put the fraction
  // anywhere in a one-sample interval and the caller has no say in that, so
  // the interval is centred on kFracOrder/2 and the worst case is half a
  // sample either side rather than a whole one.
  constexpr float kIdeal = 0.5f * static_cast<float>(kFracOrder);
  const int maxBase = static_cast<int>(delayBuf_.size()) - kFracOrder - 1;
  int base = static_cast<int>(std::lround(Lcomp - kIdeal));
  base = std::max(1, std::min(base, maxBase));

  delayLength_ = base;
  loopLength_  = static_cast<int>(std::lround(Lcomp));
  computeFractionalDelay(Lcomp - static_cast<float>(base));

  // The interpolator has no state, so a live retune cannot click through it.
  // The loop filter's state is kept, for the same reason it always was.
}

// ---------------------------------------------------------------------------
// Render
//
// Signal path per sample:
//   read (interpolated) → loop LPF → DC block → ×feedbackGain → (+exc) → write
//
// Stereo pickups:
//   L = bridge (full-length tap, taken after loop filter)
//   R = mid-string (half-length tap, raw from buffer)
// ---------------------------------------------------------------------------

void WaveguideResonator::renderReplace(const float* excitation, int numSamples,
                                       float* outL, float* outR) {
  const int bufSize = static_cast<int>(delayBuf_.size());
  if (bufSize < kFracOrder + 2 || delayLength_ < 1) {
    std::fill_n(outL, numSamples, 0.0f);
    std::fill_n(outR, numSamples, 0.0f);
    return;
  }

  for (int i = 0; i < numSamples; ++i) {
    // 1. Read the loop, interpolated: the integer offset plus a Lagrange FIR
    //    over kFracOrder+1 consecutive taps for the sub-sample remainder.
    float ap = 0.0f;
    for (int k = 0; k <= kFracOrder; ++k) {
      const int readPos = (writePos_ - delayLength_ - k + 2 * bufSize) % bufSize;
      ap += fracCoeff_[k] * delayBuf_[static_cast<size_t>(readPos)];
    }

    // 2. Loop LPF: y[n] = (1-α)*x[n] + α*y[n-1]  (unity DC gain)
    loopState_ = (1.0f - loopAlpha_) * ap + loopAlpha_ * loopState_;

    // 3. DC blocker. A real string has no zero-frequency mode; a delay loop
    //    with feedback gain g has one, with a gain of 1/(1 - g), which at
    //    0.999 is a thousand. Any drive with an offset charges it up -- the
    //    bowed exciter's does, and 99.9996% of a bowed note's energy was DC
    //    riding a ramp before this was here. See PassivityTests.cpp.
    const float dc = loopState_ - dcX1_ + kDcBlockR * dcY1_;
    dcX1_ = loopState_;
    dcY1_ = dc;

    // 4. Feedback gain, plus physical damper if engaged.
    if (damperActive_) damperGain_ *= damperDecay_;
    const float feedback = feedbackGain_ * dc * damperGain_;

    // 5. Mix in excitation and write to delay line.
    const float newSample = excitation[i] + feedback;
    delayBuf_[static_cast<size_t>(writePos_)] = newSample;

    // 6. Stereo pickups.
    //    L = bridge (the loop output — before excitation addition).
    //    R = mid-string (~N/2 from write head).
    const int midPos =
        (writePos_ - loopLength_ / 2 + bufSize) % bufSize;
    outL[i] = feedback;
    outR[i] = delayBuf_[static_cast<size_t>(midPos)];

    writePos_ = (writePos_ + 1) % bufSize;
  }
}

}  // namespace chalkwalk::physical
