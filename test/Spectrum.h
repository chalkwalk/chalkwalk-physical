#pragma once

// Spectral measurement helpers.
//
// A SECOND OPINION, deliberately not the autocorrelation detector in Signal.h.
// This library separated one recorded defect into two by measuring the same
// thing a second way -- an FFT peak near f0 finds the FUNDAMENTAL, while
// autocorrelation finds the period of the WHOLE waveform and so is pulled by
// stretched partials. Both readings were correct. Keeping the two instruments
// separate is what made that visible, so they stay in separate headers and
// neither is built out of the other.
//
// PRINCIPLES, as in Signal.h: when a test fails, suspect the measurement
// first. Everything here is calibrated against synthetic signals with a known
// answer in SpectrumTests.cpp before it is trusted on a resonator's output.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace chalkwalk::test {

namespace detail {
constexpr double kSpectrumPi = 3.14159265358979323846;
}

// In-place radix-2 FFT. Size must be a power of two.
inline void fft(std::vector<std::complex<double>>& v) {
  const std::size_t n = v.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(v[i], v[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2.0 * detail::kSpectrumPi / static_cast<double>(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (std::size_t k = 0; k < len / 2; ++k) {
        const auto u = v[i + k];
        const auto t = v[i + k + len / 2] * w;
        v[i + k] = u + t;
        v[i + k + len / 2] = u - t;
        w *= wl;
      }
    }
  }
}

// Frequency of the strongest spectral peak within +/- `window` (a fraction)
// of `expect`, refined by parabolic interpolation so the answer is not
// quantised to the bin width -- at 65536 points and 48 kHz that is 0.73 Hz,
// which would swamp the few cents these tests are looking for.
inline double peakNear(const std::vector<float>& x, double sampleRate,
                       double expect, double window) {
  constexpr int order = 16;
  const std::size_t n = std::size_t{1} << order;
  std::vector<std::complex<double>> buf(n, {0.0, 0.0});
  const std::size_t m = std::min(n, x.size());
  for (std::size_t i = 0; i + 1 < m; ++i) {
    const double w = 0.5 - 0.5 * std::cos(2.0 * detail::kSpectrumPi *
                                          static_cast<double>(i) /
                                          static_cast<double>(m - 1));
    buf[i] = {static_cast<double>(x[i]) * w, 0.0};
  }
  fft(buf);

  const double binHz = sampleRate / static_cast<double>(n);
  const int lo = std::max(1, static_cast<int>(std::floor(expect * (1.0 - window) / binHz)));
  const int hi = std::min(static_cast<int>(n) / 2 - 2,
                          static_cast<int>(std::ceil(expect * (1.0 + window) / binHz)));
  if (lo >= hi)
    return 0.0;
  int best = lo;
  double bestMag = 0.0;
  for (int b = lo; b <= hi; ++b) {
    const double mag = std::abs(buf[static_cast<std::size_t>(b)]);
    if (mag > bestMag) {
      bestMag = mag;
      best = b;
    }
  }
  const double a  = std::abs(buf[static_cast<std::size_t>(best) - 1]);
  const double b0 = std::abs(buf[static_cast<std::size_t>(best)]);
  const double c  = std::abs(buf[static_cast<std::size_t>(best) + 1]);
  const double denom = a - 2.0 * b0 + c;
  const double delta = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
  return (static_cast<double>(best) + delta) * binHz;
}

// The fundamental: a WIDE window, because the whole question is how far the
// note has moved.
inline double fundamentalHz(const std::vector<float>& x, double sampleRate,
                            double expect) {
  return peakNear(x, sampleRate, expect, 0.25);
}

// A named partial: a NARROW window, because partial k-1 and k+1 are only 1/k
// away and a wide search will happily lock onto one of them. Getting this
// wrong reported the fourth partial of a 220 Hz string as 498 cents flat.
inline double partialHz(const std::vector<float>& x, double sampleRate,
                        double expect) {
  return peakNear(x, sampleRate, expect, 0.08);
}

inline double centsBetweenHz(double reference, double measured) {
  if (reference <= 0.0 || measured <= 0.0)
    return 0.0;
  return 1200.0 * std::log2(measured / reference);
}

// How far the k-th partial sits from k times the MEASURED fundamental, in
// cents. Measured, not nominal: the question is whether the series is
// harmonic with itself, which is a different question from whether the note
// is in tune, and conflating the two is what hid one defect under another.
//
// Returns 0 when the partial would land above 0.45 * sampleRate, where there
// is nothing left to measure.
inline double partialStretchCents(const std::vector<float>& x, double sampleRate,
                                  double expectF0, int k) {
  const double f0 = fundamentalHz(x, sampleRate, expectF0);
  if (f0 <= 0.0)
    return 0.0;
  const double want = f0 * static_cast<double>(k);
  if (want > 0.45 * sampleRate)
    return 0.0;
  const double got = partialHz(x, sampleRate, want);
  return got > 0.0 ? centsBetweenHz(want, got) : 0.0;
}

}  // namespace chalkwalk::test
