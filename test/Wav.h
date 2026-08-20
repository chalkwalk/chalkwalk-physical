#pragma once

// A minimal 16-bit stereo WAV writer, FOR TESTS ONLY.
//
// DESIGN §18: the library must not learn what a file is. This is not the
// library -- it is the bench, and a bench that can hand you something to
// listen to is worth having, because several of the acceptance criteria on
// the roadmap are stated as audible claims ("click-free", "bends downward as
// it decays", "blooms") and an ear is the instrument that settles those.
//
// The ear is a check on the numeric oracle, not a replacement for it. Nothing
// in ctest depends on this file's output.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace chalkwalk::test {

namespace detail {

inline void put32(std::ofstream& o, uint32_t v) {
  const unsigned char b[4] = {static_cast<unsigned char>(v & 0xff),
                              static_cast<unsigned char>((v >> 8) & 0xff),
                              static_cast<unsigned char>((v >> 16) & 0xff),
                              static_cast<unsigned char>((v >> 24) & 0xff)};
  o.write(reinterpret_cast<const char*>(b), 4);
}

inline void put16(std::ofstream& o, uint16_t v) {
  const unsigned char b[2] = {static_cast<unsigned char>(v & 0xff),
                              static_cast<unsigned char>((v >> 8) & 0xff)};
  o.write(reinterpret_cast<const char*>(b), 2);
}

inline int16_t toPcm(float v) {
  const float clamped = std::max(-1.0f, std::min(1.0f, v));
  return static_cast<int16_t>(std::lround(clamped * 32767.0f));
}

}  // namespace detail

// Writes `l` and `r` interleaved as 16-bit PCM. Samples outside [-1, 1] are
// clamped rather than wrapped: wrapping sounds like a broken resonator, and
// the point of an audition file is that what you hear is what the model did.
inline bool writeWavStereo(const std::string& path, const std::vector<float>& l,
                           const std::vector<float>& r, double sampleRate) {
  const std::size_t frames = std::min(l.size(), r.size());
  const uint32_t dataBytes = static_cast<uint32_t>(frames * 4);

  std::ofstream o(path, std::ios::binary);
  if (!o)
    return false;

  o.write("RIFF", 4);
  detail::put32(o, 36 + dataBytes);
  o.write("WAVE", 4);
  o.write("fmt ", 4);
  detail::put32(o, 16);                                    // PCM chunk size
  detail::put16(o, 1);                                     // PCM
  detail::put16(o, 2);                                     // channels
  detail::put32(o, static_cast<uint32_t>(sampleRate));
  detail::put32(o, static_cast<uint32_t>(sampleRate) * 4); // byte rate
  detail::put16(o, 4);                                     // block align
  detail::put16(o, 16);                                    // bits
  o.write("data", 4);
  detail::put32(o, dataBytes);

  for (std::size_t i = 0; i < frames; ++i) {
    detail::put16(o, static_cast<uint16_t>(detail::toPcm(l[i])));
    detail::put16(o, static_cast<uint16_t>(detail::toPcm(r[i])));
  }
  return static_cast<bool>(o);
}

// Scale so the loudest sample sits at `peak`. Audition files are compared by
// ear across a sweep, and an untouched level makes the quiet one sound worse
// rather than quieter.
inline void normalise(std::vector<float>& l, std::vector<float>& r,
                      float peak = 0.89f) {
  float m = 0.0f;
  for (float v : l) m = std::max(m, std::fabs(v));
  for (float v : r) m = std::max(m, std::fabs(v));
  if (m < 1.0e-9f)
    return;
  const float g = peak / m;
  for (float& v : l) v *= g;
  for (float& v : r) v *= g;
}

}  // namespace chalkwalk::test
