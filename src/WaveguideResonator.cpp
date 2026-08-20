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
  thiranX1_     = thiranY1_ = 0.0f;
  loopState_    = 0.0f;
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
// Internal: compute delay length and Thiran coefficient from pitchHz_.
//
// Total loop delay: L = sampleRate / pitchHz
// Thiran target D kept in [0.5, 1.5) by: N = floor(L - 0.5)
//   → D = L - N ∈ [0.5, 1.5)
// Coefficient: a = (1 - D) / (1 + D)
//   D=0.5 → a≈+0.33, D=1.0 → a=0, D=1.5 → a≈-0.2  (all |a|<1, stable)
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
  // The residual at the very top with heavy damping is the FIRST-ORDER Thiran's
  // own phase-delay error, which is a separate and much smaller problem -- and
  // one that was invisible underneath this.
  const float omega0    = 2.0f * kPi * pitchHz_ / static_cast<float>(sampleRate_);
  const float a         = loopAlpha_;
  const float lpfDelay  = (omega0 > 1e-6f)
      ? std::atan2(a * std::sin(omega0), 1.0f - a * std::cos(omega0)) / omega0
      : (a < 1.0f ? a / (1.0f - a) : 0.0f);

  const float Lcomp = L - lpfDelay;
  const int   N = std::max(2, static_cast<int>(std::floor(Lcomp - 0.5f)));
  const float D = Lcomp - static_cast<float>(N);  // ∈ [0.5, 1.5)

  delayLength_ = std::min(N, static_cast<int>(delayBuf_.size()) - 1);
  thiranA_     = (1.0f - D) / (1.0f + D);

  // Keep filter state; do NOT zero it here so that live changes don't click.
}

// ---------------------------------------------------------------------------
// Render
//
// Signal path per sample:
//   read → Thiran AP → loop LPF → ×feedbackGain → (+excitation) → write
//
// Stereo pickups:
//   L = bridge (full-length tap, taken after loop filter)
//   R = mid-string (half-length tap, raw from buffer)
// ---------------------------------------------------------------------------

void WaveguideResonator::renderReplace(const float* excitation, int numSamples,
                                       float* outL, float* outR) {
  const int bufSize = static_cast<int>(delayBuf_.size());
  if (bufSize < 2 || delayLength_ < 2) {
    std::fill_n(outL, numSamples, 0.0f);
    std::fill_n(outR, numSamples, 0.0f);
    return;
  }

  for (int i = 0; i < numSamples; ++i) {
    // 1. Read delayed sample (integer part of loop delay).
    const int readPos =
        (writePos_ - delayLength_ + bufSize) % bufSize;
    const float delayed = delayBuf_[static_cast<size_t>(readPos)];

    // 2. Thiran 1st-order all-pass: y[n] = a*(x[n] - y[n-1]) + x[n-1]
    const float ap = thiranA_ * (delayed - thiranY1_) + thiranX1_;
    thiranX1_ = delayed;
    thiranY1_ = ap;

    // 3. Loop LPF: y[n] = (1-α)*x[n] + α*y[n-1]  (unity DC gain)
    loopState_ = (1.0f - loopAlpha_) * ap + loopAlpha_ * loopState_;

    // 4. Feedback gain, plus physical damper if engaged.
    if (damperActive_) damperGain_ *= damperDecay_;
    const float feedback = feedbackGain_ * loopState_ * damperGain_;

    // 5. Mix in excitation and write to delay line.
    const float newSample = excitation[i] + feedback;
    delayBuf_[static_cast<size_t>(writePos_)] = newSample;

    // 6. Stereo pickups.
    //    L = bridge (the loop output — before excitation addition).
    //    R = mid-string (~N/2 from write head).
    const int midPos =
        (writePos_ - delayLength_ / 2 + bufSize) % bufSize;
    outL[i] = feedback;
    outR[i] = delayBuf_[static_cast<size_t>(midPos)];

    writePos_ = (writePos_ + 1) % bufSize;
  }
}

}  // namespace chalkwalk::physical
