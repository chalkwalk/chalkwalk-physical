// SPDX-License-Identifier: MIT
//
// The audition bench: renders named scenarios to WAV so a human can hear
// what the numeric oracles are asserting about.
//
// WHY THIS EXISTS. Several acceptance criteria on the roadmap are stated as
// audible claims -- morphing across the tension/stiffness line is
// "click-free"; excitation placement has a "comb-filter signature"; a
// hard-struck tom "bends downward as it decays"; a gong "blooms". Those need
// an ear at least once, to confirm that the numeric proxy is a proxy for the
// right thing. This library's own history is the argument: a string that
// measured in tune by FFT did not sound in tune, and the ear was right before
// the second instrument was built.
//
// WHAT THIS IS NOT. It is not a test, it runs in nothing automated, and no
// assertion depends on its output. DESIGN §18 keeps files out of the library;
// this is the bench, not the library, and it links the library exactly as a
// consumer would.
//
//   ./chalkwalk_physical_audition [output-dir] [name-filter]
//
// Every scenario is deterministic: the same build renders the same bytes, so
// two of these can be diffed as well as heard.

#include "Wav.h"

#include <chalkwalk/physical/BowedExciter.h>
#include <chalkwalk/physical/ModalResonator.h>
#include <chalkwalk/physical/StruckExciter.h>
#include <chalkwalk/physical/WaveguideResonator.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace chalkwalk::physical;
using namespace chalkwalk::test;

namespace {

constexpr double kFs = 48000.0;
constexpr int kBlock = 64;

struct Take {
  std::vector<float> l, r;
};

Take silence(double seconds) {
  const std::size_t n = static_cast<std::size_t>(seconds * kFs);
  return Take{std::vector<float>(n, 0.0f), std::vector<float>(n, 0.0f)};
}

void append(Take& dst, const Take& src) {
  dst.l.insert(dst.l.end(), src.l.begin(), src.l.end());
  dst.r.insert(dst.r.end(), src.r.begin(), src.r.end());
}

float midiToHz(int midi) {
  return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

// One note through one exciter into one resonator. `onSample` is called at
// block boundaries so a scenario can move a parameter while it sounds --
// which is how the click-free claims will be auditioned.
Take note(Exciter& ex, Resonator& res, PhysicalState state, double seconds,
          const std::function<void(double, PhysicalState&)>& atBlock = {}) {
  Take t = silence(seconds);
  ex.prepare(kFs, kBlock);
  ex.reset();
  ex.trigger(state);

  std::vector<float> exc(kBlock);
  for (std::size_t i = 0; i + kBlock <= t.l.size(); i += kBlock) {
    if (atBlock)
      atBlock(static_cast<double>(i) / kFs, state);
    std::fill(exc.begin(), exc.end(), 0.0f);
    ex.renderAdd(exc.data(), kBlock, state);
    res.renderReplace(exc.data(), kBlock, t.l.data() + i, t.r.data() + i);
  }
  return t;
}

PhysicalState struck(float velocity, float hardness) {
  PhysicalState s;
  s.Fe = velocity;
  s.He = hardness;
  s.gate = true;
  return s;
}

// --- Scenarios -------------------------------------------------------------

// A plucked string at a named pitch. The high one was the inharmonicity
// witness -- its eighth partial measured 21.4 cents sharp under the old
// first-order Thiran, and the top two octaves were, in the roadmap's words,
// not in tune with anything including themselves. Keep it: it is now the
// witness for the other direction, and it is the file to listen to first if
// the fractional delay is ever changed again.
Take pluck(int midi, double seconds = 3.0) {
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setPitchHz(midiToHz(midi));
  wg.setDamping(0.15f);
  StruckExciter ex;
  return note(ex, wg, struck(0.85f, 0.7f), seconds);
}

// Five plucks of the same note at rising damping. The pitch must not move.
// This is the audible form of "damping does not detune the string" -- the
// defect it was written for moved the note by over two semitones, which is
// obvious by ear and was invisible to a suite that swept only pitch.
Take dampingSweep() {
  Take out;
  for (float damping : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
    WaveguideResonator wg;
    wg.prepare(kFs, kBlock);
    wg.setDamping(damping);
    wg.setPitchHz(midiToHz(69));
    StruckExciter ex;
    Take t = note(ex, wg, struck(0.85f, 0.6f), 1.6);
    normalise(t.l, t.r);
    append(out, t);
    append(out, silence(0.25));
  }
  return out;
}

// A pluck, left to ring, then damped. Listen for the damper engaging without
// a click, and for a double note-off not restarting the decay.
Take damperRelease() {
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setDamping(0.05f);
  wg.setPitchHz(midiToHz(57));
  StruckExciter ex;
  bool engaged = false;
  return note(ex, wg, struck(0.9f, 0.5f), 4.0, [&](double t, PhysicalState&) {
    if (!engaged && t >= 2.0) {
      wg.engageDamper(0.8f);
      engaged = true;
    }
  });
}

// Expression moving under a sounding note. loopAlpha_ changes retune the
// delay line every block here, so any click is the compensation clicking.
Take expressionSweep() {
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setDamping(0.0f);
  wg.setPitchHz(midiToHz(64));
  StruckExciter ex;
  return note(ex, wg, struck(0.9f, 0.4f), 4.0, [&](double t, PhysicalState&) {
    wg.setTimbre(static_cast<float>(0.5 - 0.5 * std::cos(2.0 * 3.14159265 * t / 4.0)));
  });
}

// The bow. Play-testing recorded that this does not feel intuitive; the
// roadmap's hypothesis is the friction junction, and this is the file that
// says whether a change to it helped.
//
// The first version of this scenario held Fn constant and rendered SILENCE,
// which is not a bench bug. In Rate mode bow velocity comes from |dFn/dt|, so
// steady pressure is not a bow stroke at all -- leaning on a string does
// nothing until you move. That is defensible physics and indefensible
// playability, and it is very likely part of what "does not feel intuitive"
// was reporting. So the bench plays it as a player would: press in, sustain,
// release.
Take bowed(int midi, int mode) {
  WaveguideResonator wg;
  wg.prepare(kFs, kBlock);
  wg.setDamping(0.1f);
  wg.setPitchHz(midiToHz(midi));
  BowedExciter ex;
  ex.setBowMode(mode);
  PhysicalState s = struck(0.7f, 0.5f);
  s.Fn = 0.0f;
  return note(ex, wg, s, 4.0, [](double t, PhysicalState& st) {
    // Press in over 300 ms, hold, release over the last 500 ms.
    if (t < 0.3)
      st.Fn = static_cast<float>(t / 0.3) * 0.7f;
    else if (t > 3.5)
      st.Fn = static_cast<float>((4.0 - t) / 0.5) * 0.7f;
    else
      st.Fn = 0.7f;
  });
}

// The modal side, which is still a one-pole skeleton. It is here so that the
// day it becomes a real mode bank, there is a before to compare against.
Take struckModal() {
  ModalResonator mr;
  mr.prepare(kFs, kBlock);
  mr.setDamping(0.3f);
  StruckExciter ex;
  return note(ex, mr, struck(0.9f, 0.8f), 2.0);
}

struct Scenario {
  const char* name;
  std::function<Take()> render;
};

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> s = {
      {"pluck-a2", [] { return pluck(45); }},
      {"pluck-a4", [] { return pluck(69); }},
      {"pluck-d7", [] { return pluck(98); }},
      {"damping-sweep", dampingSweep},
      {"damper-release", damperRelease},
      {"expression-sweep", expressionSweep},
      {"bowed-a3-rate", [] { return bowed(57, 0); }},
      {"bowed-a3-auto", [] { return bowed(57, 1); }},
      {"bowed-a3-continuous", [] { return bowed(57, 2); }},
      {"struck-modal", struckModal},
  };
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string outDir = argc > 1 ? argv[1] : "audition";
  const std::string filter = argc > 2 ? argv[2] : "";

  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    std::fprintf(stderr, "cannot create %s: %s\n", outDir.c_str(),
                 ec.message().c_str());
    return 1;
  }

  int written = 0;
  for (const auto& s : scenarios()) {
    if (!filter.empty() && std::string(s.name).find(filter) == std::string::npos)
      continue;
    Take t = s.render();
    float peak = 0.0f;
    for (float v : t.l) peak = std::max(peak, std::fabs(v));
    for (float v : t.r) peak = std::max(peak, std::fabs(v));
    normalise(t.l, t.r);

    const std::string path = outDir + "/" + s.name + ".wav";
    if (!writeWavStereo(path, t.l, t.r, kFs)) {
      std::fprintf(stderr, "cannot write %s\n", path.c_str());
      return 1;
    }
    std::printf("%-22s %6.2f s  peak %7.4f  %s\n", s.name,
                static_cast<double>(t.l.size()) / kFs, static_cast<double>(peak),
                path.c_str());
    ++written;
  }

  if (written == 0) {
    std::fprintf(stderr, "no scenario matched \"%s\". Available:\n", filter.c_str());
    for (const auto& s : scenarios())
      std::fprintf(stderr, "  %s\n", s.name);
    return 1;
  }
  return 0;
}
