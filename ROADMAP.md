# chalkwalk-physical -- Roadmap

The forward-looking plan. Work is organised into **named work areas**, grouped
under headings for readability. Sub-tasks are checkboxes, so the state of the
project is visible on every return to the repo.

**Work is referred to by name, not by number.** A decimal milestone id means
nothing when you come back to the repo in three weeks, and it invites scope
drift between huge items and tiny ones. "Ranks and the mode budget" is the
reference; the grouping headings are organisational only and are never cited.

For architecture see `DESIGN.md`, whose sections carry stable numbers and are
cited as `DESIGN §N`.

This library is consumed by more than one application. Where a work area below
is a handoff -- something a consumer used to plan and now expects from here --
it says so by name, so the two sides can be read against each other.

The ecosystem-wide plan for what gets shared and why lives outside this
repository, and is deliberately not a dependency of this one.

---

## Active focus

*(2026-08-20)*

The library is seven source files: a waveguide, a modal skeleton, two exciters,
a geometry stub, a translation matrix and an empty importer. It has one live
consumer -- an instrument-building application -- and one file over there does
the whole adoption.

It is about to become the thing that leads. `DESIGN.md` was written today and
describes a substrate -- ports carrying Norton equivalents, a graph that solves
for coupling forces, bodies expressed as two continua rather than five
geometries, ranks, and assemblies that are themselves objects. The builder's
own plan has been edited to hand its object-graph and resonator-vocabulary work
here rather than restate it.

Next, in order:

0. **Correctness first, and it is done.** The fractional delay and the loop's
   DC are both closed; the heavy-damping residual turned out not to be a
   defect at all. All of it changes rendered output, so the builder's golden
   feature vector needs re-capturing once before the object recast begins.
1. **The port protocol and the graph solve.** Everything else is downstream of
   it, and it is the piece with no partial version -- either coupling is
   bidirectional and solved, or it is not.
2. **The substrate, behind the golden test.** Reimplement the two existing
   resonators as objects with the old classes as adapters, so the builder
   compiles unchanged and its golden test says whether anything moved.
3. **The tension/stiffness continuum.** The first work that produces sounds the
   library cannot currently make.

Deliberately *not* next: the mesh tier, factory assemblies, and anything with a
user interface.

---

## Correctness

### Higher-order fractional delay -- done

*(2026-08-20)* The first-order Thiran all-pass is replaced by a **fifth-order
Lagrange** fractional delay, with the fractional part centred on the delay at
which a Lagrange interpolator is exactly linear phase.

- [x] Replace the first-order Thiran with a higher-order fractional delay.
- [x] Both characterisation tests failed by design and are deleted, folded
      into the sweeps they were carved out of.

Measured over chromatic sweeps at 48 kHz:

| | before | after |
|---|---|---|
| A1-A4, worst tuning error | +5.1 cents at G4 | -0.21 |
| A5-A7, worst tuning error | -19.3 to +43.0 | +0.89 |
| Partial stretch, partials below 12 kHz | up to 21.4 | under 1.0 |

Lagrange rather than Thiran 2-3, for two reasons past accuracy: it has no
state, so the coefficients can be swapped under a ringing note -- and
`retune()` runs on every pitch, damping and expression change, which is to say
constantly -- and it cannot go unstable.

**The stated target was not reachable and should not have been stated.** It
asked for partial stretch under 2 cents through the eighth partial at 2489 Hz;
that partial is at 19.9 kHz, which at 48 kHz is 0.83 of Nyquist. No practical
interpolator order gets there -- about 10 cents at order 5, 8 at order 9, 4 at
order 19, which is twenty taps in a nineteen-sample loop. The residual is not
a property of the partial but of its distance from Nyquist, and the same
partial of the same note at 96 kHz is within 0.05 cents. `TuningTests.cpp`
asserts that rather than describing it, because a limit that moves when the
sample rate moves is a sample-rate limit.

### DC in the loop -- done

*(2026-08-20)* A real string has no zero-frequency mode. A delay loop with
feedback gain *g* has one, with a gain of `1/(1-g)` -- a thousand at 0.999 --
and any drive with an offset charges it up. The bowed exciter's drive is
almost entirely offset, so a bowed note was a tone riding a ramp.

- [x] DC blocker in the loop, `R = 0.99999`, phase delay cancelled in
      `retune()` alongside the loop filter's.
- [x] `R` chosen against the harmonic budget, not by ear: the cancellation is
      exact only at the fundamental, so the blocker's remaining phase
      variation across partials is inharmonicity. At `R = 0.9999` that costs
      1.4 cents on the second partial of a 110 Hz string; at `0.99999` it is
      0.18, and 0.8 at the 20 Hz bottom of the range.

| | before | after |
|---|---|---|
| Bowed note, DC share of energy | 89-93% | 1.3-2.4% |
| Bowed note, absolute peak | 19.9 / 147 / 189 | 1.6 / 7.5 / 6.9 |
| Bowed rms over four seconds | 24.6 -> 192.5 | 5.98 -> 4.55 |
| Plucked note, DC share | 30% | 0.2% |

The bowed peak is still several times nominal. That is a level-scaling
question in the drive, not a runaway, and it is left for the friction
coupling to settle.

### The heavy-damping residual is not a tuning error

**Closed as not-a-defect**, and recorded because it cost two wrong
explanations and someone will find it again.

At A7 with damping 1.0 the fundamental measures 62 cents flat. It was blamed
first on the first-order Thiran, then on `retune()` cancelling the loop
filter's phase delay by evaluation rather than by a fixed-point solve. Both
wrong. Modelling the loop directly puts the phase condition at 3519.996 Hz for
a nominal 3520 -- the mode is exactly where it was asked to be, and there is
no fixed point to solve.

What moves is the **peak**, not the mode. At `loopAlpha_` 0.8 a thirteen-sample
loop has a broad resonance sitting on a steeply falling loop gain, so the
spectral maximum is dragged below the mode frequency: -25 cents modelled, -62
measured, the difference being what an FFT peak does to a broad and
fast-decaying resonance. At A4 the same damping costs 0.10 cents, the loop
being eight times longer and the resonance eight times narrower.

A damped resonator's peak really does sit below its undamped frequency, so
part of this is correct physics. The rest is the one-pole being a crude model
of material damping -- which belongs to the body vocabulary, not to tuning.

- [ ] Revisit only when `Fibre` brings per-mode T60 from material damping.
      There is nothing to fix in `retune()`.

### The audition bench -- done

*(2026-08-20)* `test/Audition.cpp` renders named scenarios to WAV, built
alongside the tests and deliberately outside `ctest`. Several acceptance
criteria on this roadmap are stated as audible claims -- morphing is
"click-free", placement has a "comb-filter signature", a tom "bends downward",
a gong "blooms" -- and an ear has to confirm at least once that the numeric
proxy proxies the right thing.

- [x] `test/Wav.h`, tested by round-trip. `DESIGN §18` keeps files out of the
      library; the bench is not the library and links it as a consumer would.
- [x] `test/Spectrum.h`: the FFT and partial readers, lifted out of
      `TuningTests.cpp` where they were private, and calibrated in
      `SpectrumTests.cpp` against synthetic signals with a known answer --
      including a stretched series, so a stretch figure can be believed.
- [x] Scenarios are deterministic: same build, same bytes, so two of them can
      be diffed as well as heard.
- [ ] Grow a scenario alongside each audible acceptance criterion as it lands.

It earned itself on the first run: see *Friction that catches*.

### Sweeps cover more than one variable

Every defect this library has recorded was invisible to a suite that swept one
variable at a time. The damping-detunes-the-string bug survived a suite that
swept pitch at one damping value; the cross-admittance question in
`DESIGN §3.1` is the same shape.

- [ ] Audit the existing suite for single-variable sweeps and widen them.
- [ ] Make two-variable sweeps the default shape for new resonator tests.

---

## The substrate

### The port protocol and the graph solve

`DESIGN §2`, `§3`, `§4`. The foundation: `PortState` as a Norton equivalent,
two-phase `observe`/`apply`, admittance matrices, and a graph that factors once
and does a triangular solve per sample.

- [ ] `PortState`, `Object`, `PortId`, ports created at positions.
- [ ] Admittance as a symmetric matrix per object; scalar degeneracy for
      single-port objects costs nothing.
- [ ] `Graph`: topology, arrow-aware symmetric factorisation at `prepare`,
      triangular solve per sample.
- [ ] Linear couplings: rigid, spring/damper, lossy.
- [ ] Nonlinear couplings: contact and friction as scalar root-finds on
      relative velocity, warm-started, with state cleared on trigger and
      release.
- [ ] Passivity, reciprocity and junction-limit tests (`DESIGN §17`) before any
      body is ported onto it. These are the tests that make the rest cheap to
      write.

Nothing consumes this while it is being built. The builder is untouched.

### The substrate, behind the golden test

The migration, staged so the builder never breaks.

- [ ] Reimplement `WaveguideResonator` and `ModalResonator` as objects.
- [ ] Keep the old `Resonator` classes as thin adapters: `renderReplace` builds
      a two-port graph internally and steps it. The builder compiles unchanged.
- [ ] Run the builder's golden test against the adapter. A feature vector
      captured months earlier against the old DSP is the right witness for
      "the rebuild changed nothing" -- it already proved itself once, catching
      the extraction drift by 56% at `rms_block50`.
- [ ] The builder switches its one adoption file to the graph API.
- [ ] Delete the adapters.

### Demoting the MPE nouns

`setPressure`, `setTimbre` and `setExpressionCurve` are MPE nouns in a base
class that should not know what a controller is. The sequencer is the consumer
that proves it: no per-note expression at all.

- [ ] Grow `TranslationMatrix` into what its name promises: gesture in --
      MPE, a sequencer step, a bot's note -- per-object parameter targets out.
- [ ] The MPE nouns leave `Resonator` with the adapters above.
- [ ] A test that drives an assembly with no expression source at all and gets
      a musical result.

### Block-rate fast path

The graph steps per sample where the library steps per block. For a single
uncoupled object that is strictly slower.

- [ ] **Measure first.** If the modal bank dominates, the per-sample dispatch
      vanishes into the noise and this work area is closed unbuilt.
- [ ] The builder asks for CPU within 10% of its pre-graph baseline; that is the
      number this is judged against.

---

## The body vocabulary

### The tension/stiffness continuum

`DESIGN §6`. Two body objects, not five geometries. `Fibre` spans ideal string
to free-free bar; `Surface` spans membrane to plate. `setStiffness` becomes the
knob it is already named for.

- [ ] `Fibre`: modal backend over `omega^2 = c^2 k^2 + kappa^2 k^4`, with
      boundary conditions (free / clamped / pinned).
- [ ] `Surface`: circular and rectangular, same relation in two dimensions.
- [ ] `Geometry` becomes boundary conditions plus a point on the continuum,
      replacing the four-case enum.
- [ ] Mode ratios tested against the analytic oracles: bar
      `1 : 2.756 : 5.404`, membrane Bessel zeros, plate `1 : 1.73 : 2.33`,
      stiff string `n sqrt(1 + B n^2)`.
- [ ] Morphing across the tension/stiffness line is click-free and continuous.
      This is the gesture nothing sample-based can do; if it snaps, it is not
      done.

### Mode shapes and placed ports

`DESIGN §6.2`. The highest-leverage function in the library: `J_m(k r)
cos(m theta)` and its rectangular equivalent, evaluated once per note.

- [ ] Mode-shape evaluation per geometry.
- [ ] Ports carry mode-shape weights from their coordinate.
- [ ] Cross-admittance `Y_ij = sum_k phi_k(x_i) phi_k(x_j) b0_k`, and the
      reciprocity test that guards its sign.
- [ ] Excitation placement audibly correct: the comb-filter signature is the
      acceptance test.

### Analytic mode generation

`DESIGN §6.3`. Generated, not tabled -- smaller than the tables, morphs where
tables snap, and keeps a file out of a JUCE-free library.

- [ ] Mode generators per geometry and boundary condition.
- [ ] Stiff-string stretch `f_n = n f0 sqrt(1 + B n^2)`.
- [ ] T60 per mode from material damping, tested by sweeping pitch **and**
      damping.

### Waveguide backend and its dispersion

`DESIGN §6.4`. Selectable, explicit, never heuristic.

- [ ] `Fibre` takes a backend choice; waveguide for exact pitch, low latency,
      or travelling-wave interaction with a drive.
- [ ] All-pass dispersion chain so the waveguide backend reaches the
      stiff-string part of the continuum.
- [ ] Document Karplus-Strong as the two-tap loop filter it is, and do not
      implement it twice.

### Air columns and toneholes

`DESIGN §7`.

- [ ] `Tube`: cylindrical and conical sections with scattering junctions.
- [ ] Toneholes as three-port junctions.
- [ ] Closed-both-ends tube coupling two surfaces -- the two-headed tom that
      punches rather than thuds.

### Banded waveguides

`DESIGN §8`. The missing middle, and cheap given what the library already has.

- [ ] `Banded`: a small set of waveguide loops, one per mode, bandpass in loop.
- [ ] Bowed against it: marimba, glass harmonica, musical saw, singing bowl.
      Modal banks cannot do this and the reason is structural, not a tuning
      problem.

### Tension modulation

`DESIGN §6.5`. The highest musical value per cycle in the library, and it does
not wait for the mesh tier.

- [ ] Mode frequencies (or delay length) modulated by instantaneous energy.
- [ ] Acceptance: a hard-struck tom bends downward as it decays; a struck gong
      blooms.

---

## Drives

### Contact, and the end of the exciter hierarchy

`DESIGN §9`. A hammer is a mass in a contact junction. `Exciter` stops being a
type tree.

- [ ] `Mass` object.
- [ ] Contact coupling: one-sided, `f = K delta^alpha`, Hunt-Crossley damping.
- [ ] `StruckExciter`'s hardness and contact time come out of stiffness and
      mass rather than a burst envelope.
- [ ] Multiple strikes and rattles work without special-casing.

### Friction that catches

`DESIGN §5.2`. Play-testing recorded that the bowed prototype does not
feel intuitive; this is the specific hypothesis.

**The bow cannot feel the string, and that is the whole problem.** A real bow
reaches a steady amplitude within a handful of periods because the friction
force depends on the relative velocity `v_bow - v_string`: the bow does
positive work while the string sticks, and the string dissipates against the
friction while it slips. That negative feedback needs `v_string`.

`BowedExciter::setJunctionVelocity` exists for it and **nothing in the library
ever calls it**, so `vHat_` is permanently zero and the solver's relative
velocity contains the bow's own reflection but not the string's motion. It is
asserted directly in `PassivityTests.cpp`: bowing a 110 Hz string and an 880 Hz
string produces BIT-IDENTICAL excitation, which cannot be true of a friction
junction. That test fails when this is fixed.

Not fixable in place: `renderAdd` produces a signal that is *then* injected,
which is feed-forward, and a friction junction is not a source. It is `§5.2` --
a root-find on relative velocity inside a **coupling**, reaction applied to
both sides.

**Do not wire `setJunctionVelocity` from the resonator as an interim.** It is
block-delayed, and at A3 one period is 218 samples against a 64-sample block,
so the feedback is a third of a period stale -- enough to stop a runaway,
nowhere near enough for stick-slip, and it builds a cross-object feedback path
the graph deletes.

A second finding, and probably part of what "does not feel intuitive" was
reporting: in Rate mode bow velocity comes from `|dFn/dt|`, so **steady
pressure is not a bow stroke at all** and a held bow is silent. Defensible
physics, indefensible playability.

- [x] The runaway itself is closed -- it was DC, not the friction model. See
      *DC in the loop*.
- [ ] Acceptance: at constant bow force and speed the string reaches a steady
      amplitude and holds it. `PassivityTests.cpp` can only assert "does not
      run away" until the junction can see the string.
- [ ] `BowedExciter`'s Newton solve moves into the friction junction.
- [ ] Elasto-plastic friction state, tried against attack reliability.
- [ ] Warm-start state cleared on trigger and release -- this shipped broken
      once and is now a test.
- [ ] Bow placement through port position.
- [ ] The same junction at low velocity: rubbed rim, superball, roar.

### Reed, jet and lip

`DESIGN §9`. Three mechanisms, not one "blown exciter". The consumer plan that
preceded this one named a single blown exciter.

- [ ] **Reed** first: pressure-controlled nonlinear reflection into a `Tube`.
- [ ] **Jet**: delayed jet drive. Overblowing should arrive on its own; if it
      has to be scripted, the model is wrong.
- [ ] **Lip**: one- or two-mass oscillator against a flared tube.
- [ ] Breath as a continuous drive, playable from pressure and from a plain
      controller.

### Particle percussion

`DESIGN §9`. One small object covering a whole family.

- [ ] Stochastic particle model: particle count, damping, resonance.
- [ ] Maraca, cabasa, shaker, sleighbells, tambourine, guiro.
- [ ] Deterministic per trigger.

### Audio input as a drive

- [ ] An object whose free velocity is a host buffer, so an FX build is the
      graph with an input object rather than a bespoke design.

---

## Assemblies

### Ranks and the mode budget

`DESIGN §10`. One object, N members, one coupling. Nobody places forty-nine
bars by hand.

- [ ] `Rank<T>` over any object, generated from a tuning function.
- [ ] Shared-node solve, O(N), with cached reciprocal admittances.
- [ ] Sympathetic resonance falls out of the coupling with no feedback path
      and no gain to tune. If a patch needs a sympathy knob, the node solve is
      wrong.
- [ ] **Mode budget**: fixed total, allocated by member activity, promotion and
      demotion crossfaded. An 88-string rank and a 3-string rank cost the same
      at the same budget.
- [ ] Test the ceiling directly: cost independent of member count at fixed
      budget.

### Assemblies, nesting and flattening

`DESIGN §11`.

- [ ] `Assembly : Object`, exposing a subset of its internal ports.
- [ ] Prepare-time flattening: the runtime sees one flat graph and one sparse
      system. No recursion at audio rate, no depth limit.
- [ ] A component and an instrument are the same type; the difference is
      whether a mounting port is attached.
- [ ] `Rank<Assembly>` works with no new machinery -- the marimba test.

### Factory assemblies

Recipes, and a recipe is data. The library never learns the word "marimba" as
anything but a name for one of these.

- [ ] The snare from `DESIGN §11.4`, as written: six objects, five coupling
      types, nothing bespoke. This is the acceptance test for the whole
      vocabulary.
- [ ] Marimba: `Rank<Assembly>` of bar-and-tube on a frame.
- [ ] A struck body, a bowed body, a blown tube, a plucked string on a
      soundboard.
- [ ] Real and unreal: at least one assembly that no instrument-maker would
      build.

### Pickups, mics and placed outputs

`DESIGN §12`.

- [ ] Pickups as zero-force ports, any number, anywhere.
- [ ] Output variable per port: displacement, velocity, acceleration. This is
      pickup character, and it is named as such.
- [ ] Mic mass loading, defaulting to zero.
- [ ] Placed outputs feeding a consumer's space stage: N inputs at positions,
      structure mics and room mics in one list.

---

## Playability

### Pitch tracking

`DESIGN §3.4`. Load-bearing, not a refinement: without it a coupled graph is a
sampler pitch-shift or unplayable outside a narrow range.

- [ ] Tracking coefficient and reference pitch on `Object`.
- [ ] Admittance recomputed and the graph refactored on change.
- [ ] Click-free retuning while ringing.
- [ ] Acceptance: a guitar assembly whose string tracks and whose body does not
      sounds like one instrument across three octaves.

### Editing while it sounds

`DESIGN §14`. The builder's *There is no build mode and no play mode*.

- [ ] Off-thread graph construction with handover; the audio thread only steps.
- [ ] Add a body, move a mic, re-mount a rank, swap a geometry -- no dropout,
      no click, while notes ring.
- [ ] A test that edits topology continuously under sustained excitation and
      asserts no discontinuity above threshold.

### Determinism across voice steal

Recorded, because it shipped broken. Every drive's random source advances per
trigger, and every warm-started solve is cleared.

- [ ] Identical trigger sequences give bit-identical output.
- [ ] A voice stolen the instant after triggering does not replay the stolen
      voice's burst.

---

## The mesh tier

### Mass-spring and finite-difference bodies

`DESIGN §15`. Deferred deliberately, sharing the port protocol so a mesh drops
into a slot a modal object vacates.

- [ ] 1D mass-spring `Fibre` equivalent, with an explicit stability bound.
- [ ] 2D finite-difference `Surface` equivalent.
- [ ] Genuine collision: snare wires with the head's own motion in the loop.
- [ ] Nonlinear plate: a gong that blooms because energy cascades upward, not
      because something modulates.
- [ ] Its own voice budget. Not a drop-in upgrade, and not a default.
- [ ] Energy tests, which the linear tier does not need and this tier does.

---

## Consumers

### The builder adopts the graph

The builder's own plan owns the adoption; this entry exists so the handoff has
one name on both sides.

- [ ] Its object/port and coupling work areas are closed as handed here.
- [ ] `src/voice/Physical.h` moves to the graph API.
- [ ] The Workbench becomes an assembly editor.

### A physical machine for the sequencer

The second committed consumer, and the one that keeps the interface honest by
having no per-note expression at all.

- [ ] A physical machine alongside its analog, FM and tone machines.
- [ ] It consumes assemblies and never sees a port.
- [ ] Played from sequencer steps: velocity, position, and a small set of
      assembly parameters.

### The third application

Not a consumer. Its plucked-string and modal-bank voices remain a recorded
divergence, and its own plan argues a sampled kit serves it better than
modelled percussion. Both positions may change; the interface is not shaped
around either.

- [ ] Revisit only when its soundfont question is settled, and only with
      evidence.

---

## Housekeeping

### Documentation upkeep

- [ ] `README.md` grows a vocabulary table as objects land; the tuning story
      stays, because it is the best evidence in the repo of how this library
      tests.
- [ ] `DESIGN.md` section numbers stay stable. Add sections; do not renumber.
- [ ] Consumers' own plans record what they now expect from here rather than
      restating it. The builder's does; the sequencer's will when its physical
      machine is scheduled.

### One measuring stick across the ecosystem

*(2026-08-21)* `chalkwalk-dsp` grew a second target, `chalkwalk::dsp::measure`:
peak, rms, crest, dB, brightness, pitch and integrated loudness, calibrated
against synthetic signals with known answers and, for loudness, against
ffmpeg's `ebur128`. It is deliberately separate from the header-only
primitives, so linking it is a choice and a consumer that wants a filter does
not build a loudness meter.

This repository already made the argument locally, in *The audition bench*:
detectors were "lifted out of `TuningTests.cpp` where they were private", on
the grounds that an instrument nobody has calibrated cannot be believed. The
same argument holds one level up, and the count says so -- across these
repositories `peak` and `rms` existed three times over and `fundamentalHz`
twice. Two pitch detectors is two answers to one question. One of them read
294.7 Hz for a 440 Hz tone and the fix was carried all the way through before
anyone suspected the instrument rather than the code under test.

Not urgent, and deliberately not bundled with the substrate work. The risk is
specific and worth naming: `test/Signal.h` and `test/Spectrum.h` have
thresholds tuned against *themselves*, so swapping the instrument underneath an
existing suite can turn a passing test red without anything being wrong with
the library. That is a job that wants its own attention rather than a corner of
another one.

- [ ] Take `chalkwalk::dsp::measure` as a test-only dependency.
- [ ] Retire `test/Signal.h`'s `peakAbs` and `rms` in favour of it, one file at
      a time, re-reading each threshold rather than assuming it survives.
- [ ] Reconcile `test/Spectrum.h`'s `fundamentalHz` with the shared one. If
      they disagree, that disagreement is the finding -- record which is right
      and why before deleting either.
- [ ] Keep `peakNear` and the partial readers here if they stay specific to
      stretched series; push them up if they do not.

### Cross-platform CI

- [ ] Keep the standalone build the test of the boundary: no JUCE anywhere on
      the include path, on every platform.
- [ ] Guard build time as the source count grows from seven files to roughly
      twenty-five.
