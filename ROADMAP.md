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
consumer, Anvil, and one file over there does the whole adoption.

It is about to become the thing that leads. `DESIGN.md` was written today and
describes a substrate -- ports carrying Norton equivalents, a graph that solves
for coupling forces, bodies expressed as two continua rather than five
geometries, ranks, and assemblies that are themselves objects. Anvil's roadmap
has been edited to hand its object-graph and resonator-vocabulary work here and
link rather than restate.

Next, in order:

1. **The port protocol and the graph solve.** Everything else is downstream of
   it, and it is the piece with no partial version -- either coupling is
   bidirectional and solved, or it is not.
2. **The substrate, behind the golden test.** Reimplement the two existing
   resonators as objects with the old classes as adapters, so Anvil compiles
   unchanged and `PluckGoldenTest` says whether anything moved.
3. **The tension/stiffness continuum.** The first work that produces sounds the
   library cannot currently make.

Deliberately *not* next: the mesh tier, factory assemblies, and anything with a
user interface.

---

## Correctness

### Higher-order fractional delay

Carried over, and the oldest open item here. The first-order Thiran all-pass
has a phase delay accurate near DC that drifts upward with frequency, so it
does not delay every partial by the same fraction of a period and the harmonic
series stretches. At 220 Hz the partials sit within 0.4 cents of harmonic; at
2489 Hz the eighth is 21.4 cents sharp.

Characterised in `TuningTests.cpp` by a test that measures partial stretch
directly and **fails when the defect is fixed**.

- [ ] Replace the first-order Thiran with a higher-order fractional delay --
      Thiran 2-3, or Lagrange. Both are standard and neither is large.
- [ ] Target: partial stretch under 2 cents through the eighth partial at
      2489 Hz.
- [ ] When it lands the characterisation test fails by design; delete it and
      tighten `low notes have a harmonic series` to cover the whole range.

This should land **before** the waveguide is recast as an object, so the
golden-test comparison in the next section is not measuring two changes at once.

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

Nothing consumes this while it is being built. Anvil is untouched.

### The substrate, behind the golden test

The migration, staged so Anvil never breaks.

- [ ] Reimplement `WaveguideResonator` and `ModalResonator` as objects.
- [ ] Keep the old `Resonator` classes as thin adapters: `renderReplace` builds
      a two-port graph internally and steps it. Anvil compiles unchanged.
- [ ] Run Anvil's `PluckGoldenTest` against the adapter. A feature vector
      captured months earlier against the old DSP is the right witness for
      "the rebuild changed nothing" -- it already proved itself once, catching
      the extraction drift by 56% at `rms_block50`.
- [ ] Anvil switches `src/voice/Physical.h` to the graph API.
- [ ] Delete the adapters.

### Demoting the MPE nouns

`setPressure`, `setTimbre` and `setExpressionCurve` are MPE nouns in a base
class that should not know what a controller is. Lockstep is the consumer that
proves it: no per-note expression at all.

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
- [ ] Anvil asks for CPU within 10% of its pre-graph baseline; that is the
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

`DESIGN §5.2`. Anvil's play-testing recorded that the bowed prototype does not
feel intuitive; this is the specific hypothesis.

- [ ] `BowedExciter`'s Newton solve moves into the friction junction.
- [ ] Elasto-plastic friction state, tried against attack reliability.
- [ ] Warm-start state cleared on trigger and release -- this shipped broken
      once and is now a test.
- [ ] Bow placement through port position.
- [ ] The same junction at low velocity: rubbed rim, superball, roar.

### Reed, jet and lip

`DESIGN §9`. Three mechanisms, not one "blown exciter". Anvil's roadmap
previously named one.

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

`DESIGN §14`. Anvil's *There is no build mode and no play mode*.

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

### Anvil adopts the graph

Anvil's roadmap owns the adoption; this entry exists so the handoff has one
name on both sides.

- [ ] Anvil's object/port and coupling work areas are closed as handed here.
- [ ] `src/voice/Physical.h` moves to the graph API.
- [ ] The Workbench becomes an assembly editor.

### A Lockstep physical machine

The second committed consumer, and the one that keeps the interface honest by
having no per-note expression at all.

- [ ] A physical machine alongside Lockstep's analog, FM and tone machines.
- [ ] It consumes assemblies and never sees a port.
- [ ] Played from sequencer steps: velocity, position, and a small set of
      assembly parameters.

### Antiphon

Not a consumer. `PluckedString` and `ModalBank` remain a recorded divergence,
and Antiphon's roadmap argues a sampled kit serves its band better than
modelled percussion. Both positions may change; the interface is not shaped
around either.

- [ ] Revisit only when Antiphon's soundfont question is settled, and only with
      evidence.

---

## Housekeeping

### Documentation upkeep

- [ ] `README.md` grows a vocabulary table as objects land; the tuning story
      stays, because it is the best evidence in the repo of how this library
      tests.
- [ ] `DESIGN.md` section numbers stay stable. Add sections; do not renumber.
- [ ] Consumers' own plans record what they now expect from here rather than
      restating it. Anvil's does; Lockstep's will when its physical machine is
      scheduled.

### Cross-platform CI

- [ ] Keep the standalone build the test of the boundary: no JUCE anywhere on
      the include path, on every platform.
- [ ] Guard build time as the source count grows from seven files to roughly
      twenty-five.
