# chalkwalk-physical

Physical modelling: bodies that ring, the things that put energy into them,
and the couplings that let one body load another.
JUCE-free, C++17, MIT.

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

**Inharmonicity — fixed.** The first-order Thiran all-pass had a phase delay
that is accurate near DC and drifts upward with frequency, so it did not delay
every partial by the same fraction of a period. At 220 Hz the partials sat
within 0.4 cents of a harmonic series; at 2489 Hz the eighth was 21.4 cents
sharp. A fifth-order Lagrange fractional delay closed it, and the two
characterisation tests that were holding it at arm's length failed by design
and were deleted:

| chromatic sweep, 48 kHz | before | after |
|---|---|---|
| A1–A4, worst tuning error | +5.1 cents | −0.21 |
| A5–A7, worst tuning error | −19.3 to +43.0 | +0.89 |
| partial stretch below 12 kHz | up to 21.4 | under 1.0 |

What survives is a partial at 0.83 of Nyquist that no practical interpolator
order reaches — and it is not a tuning error, because **the same partial of the
same note at 96 kHz is within 0.05 cents**. A limit that moves when the sample
rate moves is a sample-rate limit, so the suite asserts that rather than
describing it.

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

What looks like a residual — 62 cents at A7 with damping at maximum — turned
out not to be a tuning error at all, and saying so cost two wrong
explanations. Modelling the loop puts the phase condition at 3519.996 Hz for a
nominal 3520: the mode is exactly where it was asked to be. What moves is the
**peak**. A heavily damped thirteen-sample loop has a broad resonance sitting
on a steeply falling loop gain, so the spectral maximum is dragged below the
mode — and a peak detector reads maxima. At A4 the same damping costs 0.10
cents. A damped resonator's peak really does sit below its undamped frequency,
so part of it is correct physics and the rest is the one-pole being a crude
model of material damping.

## Build and test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

## A string has no DC, and a feedback loop does

A delay loop with feedback gain *g* has a zero-frequency mode with a gain of
`1/(1-g)` — a thousand at 0.999. Nothing has to go wrong for that to dominate;
a drive with any offset at all is enough, and the bowed exciter's drive is
almost entirely offset.

It was: **89–93% of a bowed note's energy was DC**, a tone riding a ramp to a
peak of 189 against a nominal 1.0. A DC blocker in the loop, with its phase
delay cancelled alongside the loop filter's, brings that to 1.3–2.4% and a peak
of 7. Plucked notes went from 30% DC to 0.2%.

The blocker's `R` was chosen against the harmonic budget rather than by ear.
The phase cancellation is exact only at the fundamental, so whatever phase
variation is left across the partials is inharmonicity — the thing the
fractional-delay work had just paid for. At `R = 0.9999` that costs 1.4 cents
on the second partial of a 110 Hz string; at `0.99999`, 0.18.

## Hearing it

The numeric oracles are the ones that fail a build, but some of what this
library promises is only checkable by ear. The audition bench renders named
scenarios to WAV; it is built with the tests and run by hand.

```sh
./build/test/chalkwalk_physical_audition [output-dir] [name-filter]
```

`damping-sweep` is the audible form of the tuning story above: the same note
plucked five times at rising damping, which must not move in pitch.
`damper-release` and `expression-sweep` move a parameter under a ringing note,
where a click is the whole finding. Scenarios are deterministic, so two of
them can be diffed as well as heard.

The library itself never learns what a file is (`DESIGN §18`). The bench is
not the library, and links it exactly as a consumer would.

Standalone with nothing else on the machine, and with no JUCE anywhere on the
include path — that is the test of the boundary, not a convenience. A file is a
host concern and this library must not learn what one is.

## Licence

MIT. See [LICENSE](LICENSE).

Part of the [chalkwalk](https://github.com/chalkwalk) plugin ecosystem,
alongside [chalkwalk-music](https://github.com/chalkwalk/chalkwalk-music),
[chalkwalk-dsp](https://github.com/chalkwalk/chalkwalk-dsp) and
[chalkwalk-tape](https://github.com/chalkwalk/chalkwalk-tape).
