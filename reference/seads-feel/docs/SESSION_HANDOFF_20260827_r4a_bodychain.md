# HANDOFF — START HERE. R4a: THE BODY CHAIN IS MEASURED AND THE GRADING SURFACE IS DECIDED.

> Launch line for the next session:
> **"Read `docs/SESSION_HANDOFF_20260827_r4a_bodychain.md`; do §7."**

Prior handoffs, in order, all still true where this one does not supersede them:
`docs/SESSION_HANDOFF_20260826_r4a_selfright.md` (the self-right, driven and
signed) → `docs/SESSION_HANDOFF_20260827_r4a_superman.md` (the 31-tape
measurement) → `docs/SESSION_HANDOFF_20260827_r4a_plumbing.md` (the four terms,
the seat keep-out, the scarf field Chad drove and signed, and the coat sink,
attributed and parked). Its **§12 set this rung**, and this is that rung.

---

## 0. THE STATE

| branch | state |
|---|---|
| `sandbox/r4a-phase0` | this rung. **Committed, NOT PUSHED.** |
| `main` `fa2cfe3b9` | unchanged. |

**Nothing the game draws has changed.** `render/sled_model.cpp` was not touched.
The scarf passes no probe table and no length table, both default off, and off
is bit-identical to the pre-rung solver — pinned by a three-arm case (§3, case
4) with a LIVE arm beside the identical one.

★ And the bit-identity is proven on real driving data, not argued:
`./build/seads_sled_probe.exe superman sled_tape_31.sledtape` (and 33 — the only
two of the corpus violent enough that the rig accepts them as evidence) still
reads **`fid 0.0e+00`** after this rung, with a LIVE arm beside it in the same
run (`dCA_p50 −1.14`, `dAB_max +0.22`). A bit-identical arm is only evidence
with a live arm beside it.

⚠ **One number in a prior handoff does not reproduce, and it is NOT this rung's
doing.** `docs/SESSION_HANDOFF_20260827_r4a_plumbing.md` §0 quotes
`liftC_max 166.9` vs `liftB_max 100.0` on tape 31; the same command today prints
`liftC_max 2.5` / `liftB_max 3.4`. Arm C is `r4a_superman::step4`, entirely
PRIVATE to `tools/sled_probe.cpp`, which this rung did not touch (`git status`
clean there) and which cannot see any of the solver changes. So the discrepancy
predates this work — most likely a different invocation or a corpus that moved
(see that handoff's own §6, "READ BEFORE QUOTING ANY TAPE NUMBER", and the
standing note that tapes 32/33 are not bit-exact). **Do not quote 166.9 again
until someone reproduces it.** Flagged, not explained.

**Nothing here needs Chad yet.** The body chain is not wired into the game: it
cannot be, until the stage selector exists (§7). What landed is the thing §12
said had to land first — the body, MEASURED, and the answer to the question it
told this rung to decide up front.

---

## 1. ★★★ THE DECISION: WHICH SURFACE THE LEGS ARE GRADED ON

§12's warning, verbatim: *"the body chain will pose DRAWN GEOMETRY against a
keep-out that grades a CENTRELINE — the identical trap §7.3 and §11.6 have now
paid for three times. Decide up front which surface the legs are graded on."*

**RULED, AND BUILT: the legs are graded on their OWN DRAWN SURFACE**, measured
off the shipped GLB. Not the centreline, and not a scalar dial.

And the reason is stronger than thickness, which is what makes this decidable
without Chad. **The drawn legs are not merely fatter than the chain — they are
not ON it.** They straddle the machine at |x| ≈ 0.27–0.30 m while the seat is
0.206 m half-wide. So a centreline keep-out is wrong in BOTH directions at once:

- it would **stop a bone** at stations where the drawn rider is nowhere near the
  seat (the bone runs up the middle, through the seat box);
- and it would **pass a thigh** sitting half inside the pan.

Measured, in the superman scenario the gate flies (§3, case 5): grading the
centreline drives the drawn man **218 mm** into the machine — the full thickness
of the seat pan, which is as deep as a body can be inside it. Grading the drawn
surface: **4.4 mm** worst case over 1140 sub-steps, and that residual is
explained in §4.

### 1.1 How it is implemented, and why it is a BOX and not a ball

Each station carries **two probes**, one per limb, in the chain's own frame:
the limb's **oriented bounding box** — centre `(u, v, w)` and half-extents
`(hx, hy, hz)` — measured as the max over that limb's own skinned vertices. The
solver evaluates the box's support `hx|X·n| + hy|Y·n| + hz|Z·n|` per seat face:
exact along the axes (which is all an axis-aligned solid ever asks), conservative
at the corners, one analytic projection, constant work. §7.6's ban on a contact
solver is untouched — this is the same shape as the head sphere and the back
planes already in that loop.

★ **The first cut used ONE RADIUS and it was wrong in a way worth writing down.**
A single radius takes the widest half-extent in *every* direction at once: the
shoulder station's 0.246 m is the half-SPAN ACROSS THE SHOULDERS, and a ball of
that radius holds a man's chest 246 mm above a seat his back is a third as thick.
Anisotropy is not a refinement here; it is the difference between a rider resting
on his machine and a rider hovering a foot above it.

### 1.2 ⚠ The bind pose is illegal against this keep-out, on purpose

He is **sitting** in the bind pose: his backside is authored INSIDE the seat
solid, because that is what sitting on a seat looks like. Measured penetration of
the drawn vertices at bind — pelvis 52.5 mm, thigh 38.6 mm, shin_1 30.4 mm,
shin_2 21.0 mm, knee 13.9 mm.

The extents are deliberately **NOT capped to make bind legal.** Capping them
would under-protect superman, which is the only state this chain is ever live in
(LADDER §7.3 stage 3 — seat load going to zero is what arms it).

What that costs is a **PRIMING RULE**, and it is the same one `trail_chain.cpp`
already documents for the back plane:

> **PRIME THE BODY CHAIN WHEN IT ARMS, IN THE POSE IT ARMS IN.** Never at bind.
> Primed seated, the least-penetration exit takes his pelvis out through the
> nearest face.

Test case 7 pins this as a MEASUREMENT, not as a pass: if someone later shrinks
the extents until that case flips, they have changed the answer to §1 and the
case says so out loud.

---

## 2. WHAT LANDED

### 2.1 `assets/character/sudburian_src/measure_body_chain.py` — the measuring tape

Pure stdlib, reads `assets/sled/indy650.glb`'s bytes, touches nothing, PRINTS the
C++ block `render/body_chain.cpp` bakes. Same pattern as
`measure_seat_profile.py` → `render/rider_pose.cpp`. It also:

- proves the rider is the **Sudburian** by BONE COUNT (44; 19 would be the frozen
  legacy rider) — CLAUDE.md standing law 3, never by name;
- re-measures the **seat pan** off the same bytes (0.218000 m) and **asserts every
  link against it**, so the bound cannot be broken by a future re-measure in
  silence;
- re-derives the seat solid and reproduces `rider_pose`'s baked constants exactly
  (0.205972 / 0.380322 / −1.005000 / 0.307615) — an independent cross-check that
  came out identical;
- reports the bind-pose penetration of §1.2.

### 2.2 `render/body_chain.{h,cpp}` — the measured body, and nothing else

17 stations, **16 links, 2.200920 m** from the grip socket to the ball of the
boot. Not a round number and not a choice: it is what the rig's joints and the
0.218 m bound together produce.

★ **A bone-per-link table breaks the bound in four places** — upperarm 0.286,
forearm 0.267, thigh 0.418, shin 0.454 m — so the table SUBDIVIDES: at the rig's
OWN twist bones where it has them (`upperarm_twist_01`, `lowerarm_twist_01/02`,
`thigh_twist_01`), in thirds down the shin where it does not. **Longest link
0.209254 m against the 0.218 m pan.** It is tight, and the gate pins it.

`body_chain_fill()` writes a `TrailChainParams`: segment count, the length table
(DERIVED from the station positions, never a second typed table), and the probes.
Everything else on the params — drag, damping, dt, iterations, the keep-out dials
— stays the caller's, because those are feel and they are not in the asset.

### 2.3 `render/trail_chain.{h,cpp}` — three additions, all zero-default

1. **A non-uniform segment table.** `n_seg_len = 0` uses the scalar `seg_len_m`
   and is bit-identical; otherwise link `i` takes `seg_len_tbl[i-1]`.
2. **The drawn-surface probes** (§1.1). `n_probe_stations = 0` grades the
   centreline exactly as before.
3. **`kTrailChainMaxSegments` 16 → 20.** The measured body is EXACTLY 16 links,
   and `clamp_segments` clamps **silently** — a cap the shipped table saturates
   is a landmine where the next station added vanishes without a word.

Plus one unification: `trail_chain_frames` and the probe pass now share ONE
`chain_axes`. A probe offset means what it was measured to mean only if the
keep-out and the DRAW agree on the frame it is carried in. (Honest note: this did
not move any measured number. It closes a divergence that was free to open, not
one that had.)

### 2.4 ★★★ The deadlock the pair fix exists for

A station carries TWO probes — his left limb and his right — and near the middle
of the machine BOTH can be inside the seat with **opposite shallowest faces**
(+x for one, −x for the other). Projected one at a time, each undoes the other
every iteration and **the man sits in the seat with both keep-outs reporting that
they fired.** Measured before the fix: the drawn shoulder **197 mm** inside the
pan on a chain whose keep-out was live.

So the six faces are scored ONCE for the whole station: face *f* costs the
deepest push any inside probe needs through it, and the particle takes the
cheapest face. A probe already outside costs nothing anywhere and cannot veto a
face.

---

## 3. THE GATE — `test/unit/test_body_chain.cpp`, seven cases

| # | what it pins |
|---|---|
| 1 | every link **< the measured pan**, longest 0.209254, and the cap is **not saturated** |
| 2 | 17 stations / 2.200920 m; both sides draw; and the knee probes sit **outside `kSeatXHalfM`** — the measurement the whole decision rests on |
| 3 | the solver **holds the non-uniform table** every step (and the table really is non-uniform, or the case proves nothing) |
| 4 | a uniform table is **bit-identical** to the scalar — **with a live third arm** that must differ |
| 5 | ★★★ the DRAWN body stays out (4.4 mm) while the **centreline arm sinks 218 mm** |
| 6 | the drawn surface **RESTS** on the seat (clearance ~0) and the **bone rides its own half-height above it** |
| 7 | the bind pose is illegal, **recorded not repaired** (§1.2) |

Case 5's liveness arm is the superman rung's own law applied again: a clean arm is
only evidence with a live arm beside it.

### 3.1 The gate, and the mutations

**1594 tests, 5 red, ALL PRE-EXISTING, verified BY NAME** (the count moves as
tests are added — ours added seven — so a number comparison lies):

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

Four mutations, each applied alone against a rebuilt binary, each killed, and the
tree green again after every revert:

| mutation | result |
|---|---|
| the probe branch never taken — grade the centreline | **2 cases fail** (the drawn body sinks; the contact/bone-offset pair) |
| `link_len` ignores the table and returns the scalar | **2 cases fail** (the held lengths; and the three-arm case's live arm goes identical) |
| the station escape reverted to per-probe sequential — the §2.4 deadlock | **1 case fails** (the drawn body sinks) |
| `kTrailChainMaxSegments` back to 16 | **1 case fails** (the cap is saturated) |

★ The third row is worth keeping: the deadlock is a defect that **passes every
per-probe assertion**, because both keep-outs genuinely fire. Only a metric taken
on the drawn geometry, across the whole station, can see it.

★ Case 6 exists because case 5 alone is **passable by holding the man in the
air.** Contact plus the bone's own offset is the pair of statements that says the
thing being graded is the drawn surface and not the line the solver integrates.

---

## 4. ★★★ THE TRAP THIS RUNG PAID FOR — AND IT WAS MY OWN INSTRUMENT, AGAIN

The metric for "how far is the drawn man inside the seat" was wrong twice, and
each wrong version reported a confident number about the solver that the solver
did not deserve. **Fourth instance on this program of the same disease**
(`back_clr`, `surf_clr`, the wrap's back plane, now this).

- **Draft 1** asked *"is the box's bottom below the seat top?"* A body hanging
  BELOW the machine is tall enough to say yes while being nowhere near the seat.
  It read a false **122 mm**.
- **Draft 2** computed the three-axis overlap and then **reported the vertical
  one**. A boot whose box grazes the seat's rear edge by a micron overlaps in z
  by 1e−6 and in y by 113 mm — and the metric shouted **113 mm of leg inside a
  machine the leg was behind.** An hour went into attributing that to the solver
  (frames? iterations? a Gauss-Seidel conflict?) and **the solver was right the
  whole time.**
- **The fix:** the penetration depth is the **SMALLEST** of the three axis
  overlaps — the shallowest face, which is what the solver's own projection uses
  and what the eye sees.

★ The tell was in the data and I walked past it twice: the "worst" station was
always at the seat's **rear edge**, and an over-read that only ever fires at a
boundary is a boundary artefact, not a defect.

★ Second, smaller lesson, from the same hunt: **an iteration sweep that is
non-monotonic is not convergence.** 4 → 0.113, 8 → 0.082, 20 → 0.078, 24 →
0.007 looked like a cliff worth dialling to. It was the artefact moving between
stations. With the metric corrected the sweep is flat — 4.4 / 5.3 / 6.2 mm at
4 / 8 / 20 — so **`iterations` stays at 4** and the residual is a stated bound
rather than a dial turned until a case went green.

### 4.1 What the 4.4 mm actually is

A real, bounded property of a fixed-iteration analytic projection: the distance
pass and the keep-out are solved in sequence, and while the chain is whipping the
whip moves a particle's neighbours after its own frame was taken. It does not
shrink with iterations. It is 4.4 mm on a 2.2 m man.

---

## 5. WHAT IS OPEN, AND WHOSE IT IS

1. **The stage selector does not exist in the game.** `seat_load_frac` /
   `board_load_frac` were **ruled** the stage-1/2/3 selector
   (`docs/R4A_THROW_RULING_20260825.md` §3 item 2) but they live only inside
   `tools/sled_probe.cpp`'s model today. Nothing arms the body chain. **This is
   the next rung** (§7) and it is build work, not a question.
2. **`seat_keepout_m`** — still 0.0, still Chad's dial, and now it means what it
   says: a clearance held off the DRAWN surface, on top of the limb's own box.
   Unchanged from the last handoff's §3.2.
3. **The torso boxes are coarse.** The shoulder station's lateral half-extent is
   0.246 m because its bucket legitimately holds both deltoids. The along-chain
   extent is clamped to the station's own half-link (§6) but the lateral and
   fore-aft ones are not clamped by anything. It is conservative — he rests
   slightly proud — and it is the first thing to look at if the drive reads
   "floating".
4. **`drag_k = 0.005/m` for a body** is still MY derivation, not measured off
   him. Carried over unchanged.
5. **The return-to-pose spring** (§3.4 of the plumbing handoff) — the chain has
   no angular stiffness, so inside superman it is a perfect flail. Still open,
   still Chad's.
6. **The coat sink** is still attributed and PARKED for his ruling (plumbing
   handoff §11). Untouched by this rung.

---

## 6. ONE THING IN THE MEASURER WORTH KNOWING BEFORE RE-RUNNING IT

Vertices are bucketed to the **nearest station in 3D, restricted to their own
limb group** (arm / torso / leg, with the shoulder and the pelvis deliberately in
two groups each — that is where the body actually is continuous). Without the
group restriction the torso's 13847 pelvis-weighted coat vertices land on the arm
stations and every radius is nonsense.

And the **along-chain extent is clamped to the station's own half-link**: a wide
limb drags vertices a third of a metre down the chain into its bucket, and the
raw box then spans four stations at once (the shoulder measured 0.339 m before
the clamp). Each station owns the body within half a link either side; beyond
that is its neighbour's box, and the neighbour covers it.

---

## 7. THE NEXT RUNG — ARM IT

Everything the body chain needs is now measured, baked, tested, and one
`body_chain_fill(pr)` call away. What is missing is the thing that turns it on:

1. **Get `seat_load_frac` / `board_load_frac` render-side.** They exist in
   `tools/sled_probe.cpp`'s R4a model. ⚠ The structural rule from the plumbing
   handoff §1.3 still binds: do NOT start passing a debug sink through the
   shipped kernel to get at intermediate quantities — that is a tape-visible
   change. Derive them the way `trail_frame_field` derives its own inputs.
2. **The stage machine**, LADDER §7.3 stages 0→3, continuous and reversible,
   with the selector Chad already ruled.
3. **Arm the chain at stage 3, priming it IN THE POSE IT ARMS IN** (§1.2 — this
   is the one thing that will look broken if it is skipped), and blend the drawn
   rider from the R3 pose onto the chain frames.
4. **Then build it for him and give him the absolute path** —
   `D:\flight_sim2\seads-recon\build-play\seads.exe`, never `build\` (−O0), and
   the checklist in the reply, not only in a doc.

★ The felt question his drive will answer, and it is worth asking him in exactly
these terms: **does his body rest on the machine, or float over it?** Everything
in §1.1 and §5.3 lands on that one word.
