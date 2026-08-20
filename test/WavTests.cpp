// SPDX-License-Identifier: MIT
//
// The WAV writer exists so a human can hear what the numeric oracles are
// asserting about. It is test-only -- DESIGN §18 is that the library must not
// learn what a file is, and this is not the library.
//
// It is tested anyway, because an audition file that is silent, half-length or
// at the wrong rate sends you hunting for a DSP bug that is not there.

#include <catch2/catch_test_macros.hpp>

#include "Wav.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace chalkwalk::test;

namespace {

std::vector<unsigned char> readAll(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(in),
                                    std::istreambuf_iterator<char>());
}

uint32_t le32(const std::vector<unsigned char>& b, std::size_t at) {
  return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) |
         (uint32_t(b[at + 2]) << 16) | (uint32_t(b[at + 3]) << 24);
}

uint16_t le16(const std::vector<unsigned char>& b, std::size_t at) {
  return uint16_t(uint16_t(b[at]) | (uint16_t(b[at + 1]) << 8));
}

}  // namespace

TEST_CASE("a written wav round-trips its header and its samples", "[wav]") {
  const std::string path = "chalkwalk_wav_roundtrip.wav";
  const std::size_t n = 1000;
  std::vector<float> l(n), r(n);
  for (std::size_t i = 0; i < n; ++i) {
    l[i] = 0.5f * std::sin(0.05f * float(i));
    r[i] = -0.5f * std::sin(0.05f * float(i));
  }

  REQUIRE(writeWavStereo(path, l, r, 48000.0));

  const auto b = readAll(path);
  REQUIRE(b.size() == 44 + n * 4);
  CHECK(b[0] == 'R');
  CHECK(b[1] == 'I');
  CHECK(b[2] == 'F');
  CHECK(b[3] == 'F');
  CHECK(le32(b, 4) == uint32_t(b.size() - 8));
  CHECK(le16(b, 22) == 2);            // channels
  CHECK(le32(b, 24) == 48000);        // sample rate
  CHECK(le16(b, 34) == 16);           // bits
  CHECK(le32(b, 40) == uint32_t(n * 4));

  // Samples land where they were put, within 16-bit quantisation. The channels
  // are opposite here so a swap cannot pass.
  for (std::size_t i = 0; i < n; i += 97) {
    const auto sl = int16_t(le16(b, 44 + i * 4));
    const auto sr = int16_t(le16(b, 44 + i * 4 + 2));
    INFO("frame " << i);
    CHECK(std::abs(float(sl) / 32767.0f - l[i]) < 1.0e-3f);
    CHECK(std::abs(float(sr) / 32767.0f - r[i]) < 1.0e-3f);
  }

  std::remove(path.c_str());
}

TEST_CASE("a wav that would clip is not written silently", "[wav]") {
  // Normalisation is the caller's business, but a file whose peak is over 1
  // must not wrap around into noise that sounds like a DSP fault.
  const std::string path = "chalkwalk_wav_clip.wav";
  std::vector<float> l(100, 2.0f), r(100, -2.0f);
  REQUIRE(writeWavStereo(path, l, r, 48000.0));

  const auto b = readAll(path);
  CHECK(int16_t(le16(b, 44)) == 32767);
  CHECK(int16_t(le16(b, 46)) == -32767);

  std::remove(path.c_str());
}
