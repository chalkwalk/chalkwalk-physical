# chalkwalk-physical -- Design

The current, overall design of the library. This file tracks what
chalkwalk-physical **is**; `ROADMAP.md` tracks what it is becoming.

Sections carry stable numbers and are cited as `DESIGN §N` from `ROADMAP.md`,
from consumer repositories, and from source comments. Do not renumber casually.

The consumers' own stances live with them: Anvil's `PRINCIPLES.md` and
`NON-GOALS.md` in `mpe_phys/`, Lockstep's in `seq_play/`. This library serves
those stances and does not restate them.

Shared code across the Chalkwalk plugins is planned in
[`../ECOSYSTEM.md`](../ECOSYSTEM.md). Do not restate that argument here.

---

## 1. Vision

chalkwalk-physical computes the sound of physical objects: bodies that ring,
the things that put energy into them, and the couplings that let one body load
another.

It is a **substrate, not an instrument**. It knows what a bar is and what a
tube is; it does not know what a marimba is, except as a recipe it happens to
ship. Instruments are assembled from its parts by whoever is consuming it, and
the library's job is to make that assembly simple, direct, physically honest,
and bounded in cost.

Three consumers, with different needs, and the differences are the design
pressure that keeps the boundary clean:

| Consumer | On disk | What it needs |
|---|---|---|
| **Anvil** | `mpe_phys/` | The whole graph. Anvil is an instrument builder; its Workbench authors assemblies and its user reaches every part |
| **Lockstep** | `seq_play/` | Fine-tuned assemblies, played from sequencer steps, with no per-note expression layer at all |
| **Antiphon** | `antiphon/` | Not yet a consumer -- see §18 |

Lockstep matters out of proportion to its share of the work: it is the consumer
that has **no MPE**, and so it is the one that stops MPE-shaped assumptions
leaking into the interface. They already had: the `Resonator` base this design
replaces carries `setPressure`, `setTimbre` and `setExpressionCurve`, which are
MPE nouns in a class that should not know what a controller is.

**Strictly JUCE-free.** A file is a host concern; this library must not learn
what one is. That rule is why mode lists are *generated* rather than loaded
(§6.3) and why the analysis path for imported modes belongs to the plugin
(§18).

---

## 2. The port protocol

Everything in this library talks through one interface, and it is two numbers.

A **port** is a point on a body where force can be applied and velocity
observed. Each sample, a port reports:

```cpp
struct PortState {
  float freeVelocity;  // velocity with zero applied force, this sample
  float admittance;    // Y = dv/df
};
```

This is a **Norton equivalent**: a source, and the admittance it is seen
through. That is not an analogy -- it is the same two numbers electrical
network theory uses, and it is the reason everything in this design nests
(§11). A sub-network reduced to its equivalent at a port is indistinguishable,
from outside, from a leaf.

Objects step in two phases:

```cpp
virtual void observe(PortState* out) = 0;      // advance, report
virtual void apply(const float* forces) = 0;   // absorb, commit
```

`observe` advances internal state as though nothing were attached and reports
what each port would do. The graph solves for the interaction forces (§4).
`apply` hands each object the force at each of its ports and commits.

The split is cheap because the arithmetic is already there. A modal bank's free
velocity is its biquads run with zero input; its admittance is the sum of the
per-mode `b0` feedthrough terms, recomputed only when modes change. A
waveguide's free velocity is the delay-line output; its admittance is the
constant `1/R0`.

### 2.1 Why Kirchhoff variables and not wave variables

Digital waveguide networks and Wave Digital Filters carry travelling waves and
scatter them at junctions, and they buy a real prize: **passivity is
structural**. Any topology of passive elements is stable, with no per-patch
analysis.

We carry force and velocity instead, for three reasons.

**Modal banks are natively Kirchhoff**, and modal banks are most of the
vocabulary -- bar, plate, membrane, bell, body. Wave variables would need an
adaptor at every one of them.

**Waveguides are subsumed, not excluded.** A waveguide port is a Kirchhoff port
with constant admittance `1/R0`; the conversion is two lines. Scattering
junctions come out as the special case where every admittance is constant, and
stay exactly as cheap as they are today.

**Nonlinear elements are where the interesting sounds are**, and they are where
wave-variable formulations get hard. Contact, collision and friction -- the
snare wire, the hammer, the bow -- are natural as scalar root-finds on relative
velocity (§5.2), and one of them is already written that way (`BowedExciter`'s
Newton solve).

Passivity is recovered as a **measurement** rather than a structural property:
excitation off, total graph energy non-increasing, swept across topologies
(§17). That fits this repository, where the guarantee for tuning is a number in
cents rather than a proof.

---

## 3. Objects

An `Object` is anything with ports. Bodies, masses, drives and assemblies are
all objects; there is no separate exciter hierarchy, because a hammer is a mass
in a contact junction and a bow is a friction junction.

### 3.1 Admittance is a matrix

Push a surface at one point and its other ports move **in the same sample**,
through the modes they share:

```
Y_ij = sum_k  phi_k(x_i) * phi_k(x_j) * b0_k
```

So a multi-port object reports an admittance **matrix**, not one number per
port. It is symmetric, and the symmetry is structural: the mode-shape product
form makes `Y_ij == Y_ji` by construction. That is reciprocity, and the test
for it (§17) checks something real rather than incidental.

Single-port objects -- masses, most drives -- degenerate to a scalar, and pay
nothing.

Ignoring the cross terms was considered and rejected. It would delay
within-object coupling by one sample, which at 10 kHz is 75 degrees of phase.
That is the same class of error as compensating a loop filter with group delay
where a resonator needs phase delay, which cost this library up to 220 cents
and is recorded in `README.md`. It would have been just as invisible to a test
that swept one variable at a time.

### 3.2 Ports have positions

A port is created on a body at a coordinate, and the object owns the mode-shape
weights that coordinate implies.

```cpp
virtual PortId addPort(Vec2 position) = 0;
```

Three separately-conceived features are the same feature once this is true:

- **Excitation placement** -- a strike coupling's port is at a position.
- **Pickup readout** -- a pickup is a port with nothing attached: observe its
  free velocity, apply zero force (§12).
- **Localised damping** -- a damper is a lossy junction at a position, which is
  what a finger on a string is.

### 3.3 Output variable

A port reports velocity, but a real contact mic senses **acceleration** -- a
differentiator, +6 dB/octave, and a large part of why a piezo sounds like a
piezo. Ports therefore carry an output-variable choice: displacement
(integrate, dark and body-heavy), velocity (flat), acceleration (bright and
clicky). One pole each way.

This is a pickup-character control, not a correctness fix, and it is named as
one.

### 3.4 Pitch tracking

Every object carries a tracking coefficient and a reference pitch:

```
f_object = f_nominal * (f_note / f_ref) ^ track
```

`track = 1` follows the note; `track = 0` stays put. This is filter
key-tracking, and it is load-bearing rather than a refinement: without it a
coupled graph is stuck at one of two useless extremes. Everything tracking is a
sampler pitch-shift, where a snare becomes a chipmunk snare two octaves up.
Nothing tracking is unplayable outside a narrow range.

Real instruments are mixed, and the mixture *is* the instrument. A guitar
string tracks 1, its body 0. A marimba bar tracks 1 and so does its resonator
tube, because that tube is tuned per bar -- but the frame does not. A tom's
head tracks, its shell barely does.

Negative values and values above 1 are free and occasionally wonderful.

Retuning changes admittance, so the graph refactors (§4.2), and retuning while
ringing must be click-free.

---

## 4. The graph and the solve

A `Graph` owns objects, couplings and the linear system that binds them. It
steps per sample.

### 4.1 The shared node

The topology this design blesses is the **star**: N members on one node. That
is not a simplification -- it is what real instruments are. A bridge, a
soundboard, a frame, a shell are all one node with many things attached.

At a shared node velocities are equal and forces sum to zero, which gives an
impedance-weighted average:

```
v = sum(v_free_i * Yhat_i) / sum(Yhat_i)      Yhat_i = 1/Y_i, cached
```

About three multiplies and three adds per member, and **one divide for the
node**. Forty-seven harp strings on a soundboard is a few hundred operations a
sample.

This is the whole answer to sympathetic resonance. Sympathy is not a layer
added on top of coupling; it **is** the coupling, and the coupling is a
weighted average. The alternative anyone reaches for first -- sum the strings,
filter, feed a scaled amount back -- costs the same order of arithmetic and is
worse in three ways: no impedance weighting, so a dead string sympathises as
hard as a free one; a feedback gain that must be hand-tuned per patch not to
blow up; and a loop delay that detunes everything it touches.

**Prefer one shared node with many members over many pairwise couplings.** A
bridge is a node, not forty-seven connections. An assembly that needs a mesh is
making a decision to justify, not taking a default.

### 4.2 The factorisation

Cross-admittance (§3.1) means nodes are not independent: the graph solves one
linear system per sample. Because topology is fixed between edits:

- **Factor once**, at `prepare` or on a control-rate tick when admittances
  change (damping, pitch tracking). Symmetric, so LDL-transpose. Off the audio
  thread.
- **Per sample, a triangular solve** -- forward and back substitution over the
  non-zeros.

For a star the matrix is arrow-shaped, factorisation stays O(N), and the
per-sample cost stays the few multiplies per member quoted above. Meshes cost
more, which is the right pressure.

### 4.3 Block rate

The library today runs per block; the graph runs per sample. For a single
uncoupled object that is strictly slower.

A block fast path for the trivial topology may be needed, and is **not built
speculatively**. If the modal bank dominates, and it should, the per-sample
dispatch vanishes into the noise. Measure, then decide.

---

## 5. Couplings

A coupling joins ports at a node and decides the force between them.

### 5.1 Linear couplings

- **Rigid** -- velocities equal. A bridge, a mount, a weld.
- **Spring/damper** -- compliant, with stiffness and loss. A head to a shell, a
  bar to a frame.
- **Lossy** -- a damper, a finger, felt.

### 5.2 Nonlinear couplings

Each reduces to a scalar root-find on relative velocity, warm-started from the
previous sample.

- **Contact** -- one-sided, `f = K * delta^alpha` with Hunt-Crossley damping.
  Zero force until the surfaces meet. This is a hammer, a stick, a snare wire
  against a head, a rattle, a prepared piano.
- **Friction** -- a bow, a rubbed rim, a superball. An elasto-plastic friction
  state is the specific thing to try for reliable attacks.

Warm-starting is stateful, and that state must be cleared on trigger and
release. This is not hypothetical: `BowedExciter` shipped without it, and a
re-trigger began from the velocity the previous note ended on and converged to
a different root -- the same gesture giving a different attack depending on
what came before it. Recorded in `ROADMAP.md` under *Determinism across voice
steal*.

### 5.3 Acoustic cavity coupling

Enclosed air loads the structure, strongly. A closed tube joining two membranes
is what makes a two-headed tom punch rather than thud, and it is an ordinary
bidirectional coupling to a `Tube` (§7).

Free-field radiation does not load the structure -- air against a solid is an
impedance mismatch of roughly 3000:1 -- and is handled one-way and downstream
(§12.2). The line between the two is physical, not a convenience.

---

## 6. Bodies: two continua, not five geometries

The instinct is one class per body: string, bar, tube, membrane, plate. That is
five classes, five parameter sets and five sets of bugs. The physics says
otherwise.

### 6.1 The dispersion relation

A body's partials come from tension against stiffness:

```
omega^2 = c^2 k^2 + kappa^2 k^4       c from tension, kappa from bending stiffness
```

In one dimension, `kappa -> 0` is an ideal string: harmonic, `1 : 2 : 3 : 4`.
`c -> 0` is a free-free bar: `1 : 2.756 : 5.404 : 8.933`. In between sits every
real string, whose partials stretch as `f_n = n * f0 * sqrt(1 + B n^2)` -- the
piano.

In two dimensions the same relation gives membrane against plate.
Tension-dominant is a drumhead, `1 : 1.594 : 2.136 : 2.296 : 2.653` from the
Bessel zeros. Stiffness-dominant is a plate, `1 : 1.73 : 2.33 : 4.11`. Every
real head has both.

So the vocabulary is **`Fibre` and `Surface`** -- one 1D body and one 2D body,
each spanning its continuum -- plus `Tube` (§7), which is genuinely different
because it resonates air rather than matter.

`Geometry` stops being an enum of four hard-coded cases and becomes boundary
conditions (free / clamped / pinned, circular / rectangular) plus a point on
the continuum. `setStiffness` becomes the knob it is already named for, and
currently does nothing.

The payoff is more than tidiness. You can **morph across the tension/stiffness
line**, which is a gesture nothing sample-based can do.

### 6.2 Mode shapes

Mode shapes matter as much as frequencies: they are what makes striking
position and pickup position work. For a circular membrane they are
`J_m(k r) cos(m theta)`, evaluated once per note.

That single function delivers excitation placement, stereo pickup pairs,
paraphonic localised damping, and the cross-admittance of §3.1. It is the
highest-leverage function in the library.

### 6.3 Modes are generated, not tabled

Mode frequencies are computed analytically from geometry and material.

Tables were the obvious alternative and are rejected. A generator is smaller
than the tables it replaces; it morphs continuously where tables snap; the mode
ratios above are its test oracle; and it keeps the JUCE-free boundary clean,
because a table large enough to be worth having wants to be a file.

Tables return for exactly one case: imported modes, where by definition there
is no formula (§18).

### 6.4 Implementation backends

`Fibre` is modal by default. A **waveguide backend** is selectable, and is the
right choice when the body needs exact harmonic pitch, low latency, or
travelling-wave interaction with a drive -- bow, reed and jet all need the
last. It reaches the stiff-string part of the continuum through an all-pass
dispersion chain, but it cannot reach the bar end.

The choice is explicit rather than automatic. An assembly that wants a bowed
string and an assembly that wants a struck bar are making different requests,
and hiding that behind a heuristic would make the pitch behaviour unpredictable.

Karplus-Strong is not a separate model. It is the waveguide backend with a
two-tap averaging loop filter, and it is documented as such rather than
implemented twice.

### 6.5 Cheap nonlinearity

Tension modulation -- mode frequencies (or delay length) modulated by
instantaneous energy -- is a handful of operations and buys the pitch bend of a
hard-hit tom and the bloom of a struck gong. It is the highest musical value
per cycle in the library, and it does not wait for the mesh tier (§15).

---

## 7. Tubes and air

A `Tube` is an air column: cylindrical or conical sections, with scattering
junctions between them, and toneholes as three-port junctions. That is what
makes a tube a woodwind rather than a pipe, and a flare a brass instrument
rather than a tube.

Closed at both ends and coupled to two surfaces, the same object is a drum
shell's cavity (§5.3).

---

## 8. Banded waveguides

A handful of short waveguide loops, each tuned to one mode of an inharmonic
body, each with a bandpass in the loop.

This is the missing middle, and it earns its place because modal banks handle
struck bars beautifully and bowed bars terribly. Bowing needs per-mode
travelling-wave interaction, which a biquad does not have. Banded waveguides
give bowed marimba, glass harmonica, musical saw, singing bowls and bowed
cymbal, on a method the library mostly has already.

---

## 9. Drives

Drives are objects, usually with one port, joined to a body by a coupling.
There is no `Exciter` base class.

- **Struck** -- a `Mass` in a contact junction. Hardness and contact time fall
  out of stiffness and mass rather than being faked with a burst envelope, and
  multiple strikes and rattles come free.
- **Plucked** -- the same contact, released rather than rebounding.
- **Bowed / rubbed** -- friction (§5.2). The same junction at low velocity is a
  rubbed rim, a superball, a roar.
- **Blown** -- three genuinely different mechanisms, not one. A **reed**
  (pressure-controlled nonlinear reflection into a tube: clarinet, sax), a
  **jet** (delayed jet drive: flute, recorder, where overblowing arrives on its
  own), and a **lip** (a one- or two-mass oscillator against a flared tube:
  brass). The reed is the one to build first.
- **Particles** -- a stochastic particle model in the manner of Cook's PhISEM.
  Maraca, cabasa, shaker, sleighbells, tambourine, guiro, from one small object
  with a particle count and a damping.
- **Audio input** -- an object whose free velocity is a host buffer, which is
  how an FX build drives the graph with external sound.

Every drive's random source must advance deterministically per trigger. A voice
stolen the instant after triggering otherwise replays a byte-identical burst
from the state the stolen one left; this shipped once and is recorded.

---

## 10. Ranks

A `Rank<T>` is **one object** holding N members of one object type, generated
from a tuning function, exposing one port (or a few) to the outside.

```
Rank<Fibre>   49 members, chromatic from C2, mounted on Frame at (0.5, 0.1)
```

One line, one coupling, one object in the graph. Nobody places forty-nine bars
by hand.

It is not a special case for marimbas. The same shape is a piano's strings on a
soundboard, a harp, a sitar's sympathetics, tubular bells, and a snare's wires
against a resonant head. An abstraction that lands on five instruments without
bending is the right one.

A `Rank` is also what paraphony is: N pitched sources on one shared body,
individually placed, interacting through it. There is no paraphonic *mode* --
there is a rank.

Nothing requires the members to be tuned chromatically, to be similar to one
another, or to be bodies of the same kind.

### 10.1 The mode budget

Simulating every member fully is wasteful; skipping silent members is wrong,
because a silent undamped string coupled to a frame **is** the sympathy. Cut it
and a harp stops being a harp.

Instead: **members are always present, and the rank has a fixed total mode
budget allocated by activity.** A struck member gets full modes. A dormant one
drops to its lowest one to three, which is most of what sympathy is anyway,
since the energy arriving through a frame is small and lands near fundamentals.
Members are promoted and demoted smoothly, with the crossfade used for voice
stealing.

Three properties, in the order they matter:

- **It is a hard realtime ceiling.** Total modes is a number you set. An
  88-string piano rank and a 3-string rank cost the same when you say they do,
  so "always realtime" is a configured constant rather than a hope.
- **It degrades to the right thing.** Under budget pressure you get sympathy on
  fundamentals only, which is a mild dulling, not a dropout.
- **No click.** Promotion adds modes at zero amplitude; demotion decays them
  out. There is no moment where a body appears or vanishes.

One knob per rank. Its default is a measurement, not a guess.

---

## 11. Assemblies

An `Assembly` **is** an `Object`. It holds an internal graph and exposes some
of its ports.

This works with no special case because `PortState` is a Norton equivalent
(§2), and Norton equivalents compose: a sub-network reduced at a port is
indistinguishable from a leaf. Multi-port assemblies reduce to an admittance
matrix, which is what §3.1 already requires of every object.

So a bar-and-tube is an assembly with one mounting port. A marimba is a rank of
those on a frame. A piano is an assembly containing a rank of strings on a
soundboard, plus a lid and a case -- and the harp inside it is literally the
harp assembly, mounted differently.

`Rank<T>` takes any object, including an assembly, so a rank of bar-and-tube
assemblies needs no new machinery.

### 11.1 Flattening

An assembly's internal graph is **inlined into its parent at `prepare` time**.
Nesting is an authoring-time concept; the runtime sees one flat graph and one
sparse system.

A marimba flattens to 49 bars, 49 tubes, 49 internal nodes and one frame node
-- still a tree, still O(N). There is no recursion at audio rate, no depth
limit to police, and no per-level overhead. Nest as deeply as the instrument
makes sense; pay only for the leaves.

### 11.2 A component and an instrument are the same type

An assembly with a pickup and no mounting port is a playable instrument. An
assembly with a mounting port is a component. **They are the same type**, and
which one it is depends only on whether it is attached to something.

That is what makes a builder a builder. Someone builds a bar-and-tube, saves
it, ranks 49 of them and mounts the rank on a frame, and has built a marimba
out of an instrument they made an hour earlier.

The library ships factory assemblies. Nothing in the library needs to know the
word "marimba" for the above to work -- the factory ones are simply assemblies
we wrote down.

### 11.3 The consumer split

Anvil consumes the graph directly: its Workbench is an assembly editor, and its
user reaches every part. Lockstep consumes assemblies and never sees a port.

Same code underneath, which is the point. A fine-tuned special case built on
the general model cannot drift from it the way Antiphon's `PluckedString`
drifted from Anvil's (`../ECOSYSTEM.md`).

### 11.4 A worked example: the snare

Six objects, five coupling types, nothing bespoke:

| Part | Object | Coupled by |
|---|---|---|
| Batter head | `Surface`, tension-dominant | contact, from the stick |
| Stick | `Mass` | contact |
| Enclosed air | `Tube`, closed | rigid, to both heads |
| Resonant head | `Surface`, tension-dominant | rigid, to the air |
| Shell | `Fibre` ring | spring/damper, to both heads |
| Wires | `Rank<Fibre>` | contact, one-sided, so they buzz only when the head comes to meet them |

That the example decomposes cleanly is the main evidence the vocabulary is the
right size.

---

## 12. Pickups, mics and radiation

### 12.1 Contact mics

A contact mic is a port with a zero-force junction (§3.2). Place as many as you
like, anywhere on any body, each with an output-variable choice (§3.3) and a
pan.

A real contact mic also **loads** the structure -- its mass damps the spot it
is stuck to, audibly on a thin plate. That is a `Mass` in a rigid junction,
built from parts the library already has, exposed as a mic mass that defaults
to zero.

### 12.2 Room mics and the space stage

A room mic hears radiated sound, which is not a port variable, so it cannot be
a port on a body. It is a position in a downstream space stage, fed from the
structure.

The one-way coupling is **correct, not a compromise**: air against a solid body
is an impedance mismatch of roughly 3000:1, so the room genuinely does not load
the structure in any way that could be heard. Modelling it bidirectionally
would cost real cycles to reproduce an absent effect.

The space stage therefore takes **N inputs at positions**. Structure mics and
room mics are one list; some entries are attached to bodies and some are placed
in the air.

Deferred, and named so it is not mistaken for an oversight: modes radiate with
differing efficiency, and a free plate's high modes largely cancel themselves.
Real, audible, and a per-mode weight when someone wants it.

---

## 13. Realtime safety and threading

`Graph::prepare` allocates everything: nodes, junction workspace, the
factorisation, per-object buffers. The audio thread only steps.

Topology changes build a new graph off-thread and hand it over. That is how a
Workbench edits a live patch, and it is what makes §14 possible.

Each voice owns a graph instance built from one shared description.

---

## 14. Building and playing are one process

The graph is editable while it sounds. Adding a body, moving a mic, re-mounting
a rank -- none of these stops the audio, and none of them clicks.

This is a stance, not a feature, and it belongs to Anvil's `PRINCIPLES.md`
under *There is no build mode and no play mode*. It is recorded here because it
is a **constraint on this library**: off-thread graph construction with
handover (§13), smooth promotion and demotion in ranks (§10.1), click-free
retuning under pitch tracking (§3.4), and prepare-time flattening (§11.1) all
exist so that editing is a performance gesture rather than an interruption.

Lockstep holds the same stance for the sequencer (`seq_play/PRINCIPLES.md` §3:
there is no design mode versus performance mode). It is an ecosystem position,
not a one-off.

---

## 15. The mesh tier

Mass-spring networks and finite-difference schemes are deferred, deliberately,
and share the port protocol so a mesh drops into a slot a modal object vacates.

Their unique claim is genuine collision and genuine nonlinearity: wires buzzing
on a head with the head's own motion in the loop, a gong blooming as energy
cascades upward. Nothing in §6 reaches those.

Their cost is 10 to 100 times a modal body, plus stability guards and an energy
test burden that the linear tier does not carry. That is why the linear tier
comes first, and why the mesh tier arrives with its own voice budget rather
than as a drop-in upgrade.

---

## 16. What is deliberately not modelled

**Commuted synthesis** -- folding a body's impulse response into the excitation
-- is excluded precisely *because* it works. It buys body tone by making the
body un-couplable, which is the one thing this design exists to avoid.

---

## 17. What the tests measure

The measurements are the design. This library found a 220-cent tuning error by
sweeping a variable a previous suite had held fixed, and separated one recorded
defect into two by measuring the same thing a second way; the tests below are
written in that spirit.

| Test | Oracle |
|---|---|
| Mode ratios | Analytic. Free-free bar `1 : 2.756 : 5.404`; membrane Bessel zeros; plate `1 : 1.73 : 2.33`; stiff string `n sqrt(1 + B n^2)` |
| Decay | T60 within tolerance of the requested value, swept over pitch **and** damping |
| Passivity | Excitation off: total graph energy non-increasing over ten seconds, swept across topologies |
| Reciprocity | Force at A giving velocity at B equals force at B giving velocity at A. Catches admittance sign errors, which become instability rather than a wrong note |
| Junction limits | Coupling to a zero-admittance port leaves an object unchanged; so does coupling to an infinite-admittance one. Two exact answers that catch most junction algebra |
| Determinism | Identical trigger sequences give bit-identical output, including across voice steal |
| Budget ceiling | A rank's cost is independent of member count at fixed budget |

Sweeps cover **more than one variable at a time**. Every defect this library
has recorded was invisible to a suite that swept one.

Anvil's `PluckGoldenTest` is the cross-repository regression oracle for the
migration (`ROADMAP.md`, *The substrate, behind the golden test*). A feature
vector captured months earlier against the old DSP is a better witness to "the
rebuild changed nothing" than any test written alongside the rebuild.

---

## 18. Boundaries

**No files.** Modal import is an analysis pipeline over an audio buffer; the
buffer arrives from the plugin, which is the thing that knows what a file is.
`ModalDataImporter` holds the mode-list type and the seam, and nothing else.

**No named instruments in the engine.** Factory assemblies are recipes, and a
recipe is data. Anvil's `NON-GOALS.md` fence #2 rejects engine-level instrument
emulation; this library keeps that possible by making every instrument an
assembly of parts.

**No modulation, no effects, no UI.** Movement comes from gesture and physics.
The space stage is the consumer's; this library provides placed outputs (§12.2)
and stops.

**Antiphon is not a consumer yet.** Its `PluckedString` and `ModalBank` remain
a recorded divergence (`../ECOSYSTEM.md`), and its roadmap argues a sampled kit
serves its band better than modelled percussion. Both positions may change; the
interface is not shaped around either.
