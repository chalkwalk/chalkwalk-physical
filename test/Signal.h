#pragma once

// Measurement helpers for the Anvil tests.
//
// PRINCIPLES: when a test fails, suspect the measurement first. Every function
// here is deliberately simple enough to reason about, and the pitch detector is
// cross-checked against a synthetic sine in TestWaveguideResonator.cpp before
// it is trusted on a resonator's output.

#include <cmath>
#include <cstddef>
#include <vector>

namespace anviltest {

inline bool allFinite(const std::vector<float>& x) {
  for (float v : x)
    if (!std::isfinite(v)) return false;
  return true;
}

inline float peakAbs(const std::vector<float>& x) {
  float p = 0.0f;
  for (float v : x) p = std::max(p, std::fabs(v));
  return p;
}

inline float rms(const std::vector<float>& x, size_t from = 0, size_t to = 0) {
  if (to == 0 || to > x.size()) to = x.size();
  if (from >= to) return 0.0f;
  double acc = 0.0;
  for (size_t i = from; i < to; ++i) acc += double(x[i]) * double(x[i]);
  return float(std::sqrt(acc / double(to - from)));
}

// Fundamental frequency by normalised autocorrelation over the given window,
// with parabolic interpolation around the peak lag for sub-sample resolution.
// Returns 0 if the window carries no usable periodicity.
inline float estimatePitchHz(const std::vector<float>& x, double sampleRate,
                             size_t from, size_t to,
                             float minHz = 40.0f, float maxHz = 5000.0f) {
  if (to > x.size()) to = x.size();
  if (from >= to) return 0.0f;
  const size_t n = to - from;

  const size_t minLag = size_t(sampleRate / double(maxHz));
  const size_t maxLag = size_t(sampleRate / double(minHz));
  if (maxLag >= n / 2 || minLag < 1) return 0.0f;

  // Remove DC: a waveguide loop can accumulate a small offset.
  double mean = 0.0;
  for (size_t i = from; i < to; ++i) mean += double(x[i]);
  mean /= double(n);

  auto corr = [&](size_t lag) {
    double num = 0.0, ea = 0.0, eb = 0.0;
    for (size_t i = 0; i + lag < n; ++i) {
      const double a = double(x[from + i]) - mean;
      const double b = double(x[from + i + lag]) - mean;
      num += a * b;
      ea  += a * a;
      eb  += b * b;
    }
    const double den = std::sqrt(ea * eb);
    return den > 1e-20 ? num / den : 0.0;
  };

  std::vector<double> acf(maxLag + 2, 0.0);
  double best = -1.0;
  for (size_t lag = minLag; lag <= maxLag; ++lag) {
    acf[lag] = corr(lag);
    if (acf[lag] > best) best = acf[lag];
  }
  if (best < 0.3) return 0.0;

  // Octave correction. A periodic signal correlates just as well at 2T as at
  // T, so "take the highest peak" picks an octave below at the mercy of
  // floating-point noise -- which is exactly what the first version of this
  // function did, and what the synthetic-sine calibration test caught. Take
  // instead the FIRST local maximum that comes within a small margin of the
  // best, which is the true period.
  const double accept = best * 0.90;
  size_t bestLag = 0;
  for (size_t lag = minLag + 1; lag + 1 <= maxLag; ++lag) {
    if (acf[lag] >= accept && acf[lag] >= acf[lag - 1] && acf[lag] >= acf[lag + 1]) {
      bestLag = lag;
      break;
    }
  }
  if (bestLag == 0) {
    for (size_t lag = minLag; lag <= maxLag; ++lag)
      if (acf[lag] == best) { bestLag = lag; break; }
  }
  if (bestLag == 0) return 0.0;
  best = acf[bestLag];

  // Parabolic interpolation on the three points around the peak.
  double refined = double(bestLag);
  if (bestLag > minLag && bestLag < maxLag) {
    const double cm = corr(bestLag - 1);
    const double c0 = best;
    const double cp = corr(bestLag + 1);
    const double denom = cm - 2.0 * c0 + cp;
    if (std::fabs(denom) > 1e-12) refined += 0.5 * (cm - cp) / denom;
  }
  return float(sampleRate / refined);
}

inline float centsBetween(float a, float b) {
  if (a <= 0.0f || b <= 0.0f) return 1.0e9f;
  return 1200.0f * float(std::log2(double(b) / double(a)));
}

// Proportion of energy above `splitHz`, measured with a one-pole high-pass.
// Crude, but monotone in spectral tilt, which is all the brightness tests need.
inline float highFraction(const std::vector<float>& x, double sampleRate,
                          float splitHz) {
  const float dt = 1.0f / float(sampleRate);
  const float rc = 1.0f / (2.0f * 3.14159265f * splitHz);
  const float a  = rc / (rc + dt);
  float yPrev = 0.0f, xPrev = 0.0f;
  double hi = 0.0, all = 0.0;
  for (float v : x) {
    const float y = a * (yPrev + v - xPrev);
    xPrev = v;
    yPrev = y;
    hi  += double(y) * double(y);
    all += double(v) * double(v);
  }
  return all > 1e-20 ? float(hi / all) : 0.0f;
}

// Index of the last sample whose magnitude exceeds `threshold`.
inline size_t lastAbove(const std::vector<float>& x, float threshold) {
  for (size_t i = x.size(); i > 0; --i)
    if (std::fabs(x[i - 1]) > threshold) return i - 1;
  return 0;
}

}  // namespace anviltest
