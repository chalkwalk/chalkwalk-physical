# chalkwalk-physical

Physical modelling: bodies that ring, the things that put energy into them,
and the couplings that let one body load another.
JUCE-free, C++20, MIT.

**[`DESIGN.md`](DESIGN.md)** is what this library is; **[`ROADMAP.md`](ROADMAP.md)**
is what it is becoming. The table below is what exists *today*, which is a good
deal smaller than either.

| | |
|---|---|
| `WaveguideResonator.h` | 1D digital waveguide — delay loop, fractional tuning, loop damping |
| `ModalResonator.h` | A bank of resonant modes, for struck bars and plates |
| `Geometry.h` | Shape of the resonating body, and what it does to the modes |
| `StruckExciter.h` | A hammer or mallet: contact time, hardness, position |
| `BowedExciter.h` | Stick-slip friction — bow force, velocity, position |
| `TranslationMatrix.h` | MPE expression to physical parameters |

## Tuning is measured, twice, by different methods

A string that is not in tune is not a string. Getting this right turned out to
need two independent detectors, because one instrument measured twice
disagreed — and both readings were correct.

**An FFT peak near the fundamental finds the fundamental. Autocorrelation finds
the period of the whole waveform**, so stretched partials pull it. When those
two disagree, the note has a fundamental in the right place and a harmonic
series that is not.

That separated one recorded defect into two:

**Inharmonicity — real, and still here.** The first-order Thiran all-pass has a
phase delay that is accurate near DC and drifts upward with frequency, so it
does not delay every partial by the same fraction of a period. At 220 Hz the
partials sit within 0.4 cents of a harmonic series; at 2489 Hz the eighth is
21.4 cents sharp. Characterised by a test that fails when it is fixed. The
remedy is a higher-order fractional delay — Thiran 2–3, or Lagrange.

**Damping detuned the string — fixed.** The loop filter's delay was compensated
with the **group delay** formula where a resonator needs the **phase delay**.
They agree at DC and diverge as pitch and damping rise. Worst error in the top
two octaves:

| damping | group delay | phase delay |
|---|---|---|
| 0.3 | 3.5 cents | 2.2 |
| 0.5 | 17.7 | 1.2 |
| 0.7 | 55.6 | 7.0 |
| 1.0 | 220.7 | 63.4 |

and the whole range up to A5 goes from as much as 20.1 cents to 1.1. A sweep at
a single damping value cannot see this at all, which is why it survived a test
suite that swept pitch.

## Build and test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

Standalone with nothing else on the machine, and with no JUCE anywhere on the
include path — that is the test of the boundary, not a convenience. A file is a
host concern and this library must not learn what one is.

## Licence

MIT. See [LICENSE](LICENSE).

Part of the [chalkwalk](https://github.com/chalkwalk) plugin ecosystem,
alongside [chalkwalk-music](https://github.com/chalkwalk/chalkwalk-music),
[chalkwalk-dsp](https://github.com/chalkwalk/chalkwalk-dsp) and
[chalkwalk-tape](https://github.com/chalkwalk/chalkwalk-tape).
