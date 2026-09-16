# HANDOFF — START HERE. R4a SUPERMAN: THE FOUR TERMS AND THE SEAT ARE PLUMBED.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws, then `docs/R4A_THROW_RULING_20260825.md`
IN FULL — Chad's ruling doc for this whole rung — then §0–§2 of this file. The
solver work is DONE: `render/trail_chain.*` now carries all four pseudo-force
terms and the seat keep-out, gated by 9 mutation-verified cases, and it is
BIT-IDENTICAL with the fields at their zero defaults. Chad then RULED THE SCARF
GETS THE FIELD, and HE HAS DRIVEN IT AND SIGNED IT (§6) — with ONE open complaint,
the coat sink at the knot, which is ATTRIBUTED in §7 and is the first thing to do.
Read §5.5 before quoting the gate at any of the scarf wiring, because the gate
cannot see caller glue. After §7, the next rung is THE BODY CHAIN ITSELF — anchor
it at the grips, give it a segment table bounded by the 0.218 m pan (§2), and feed
it the same field. Superman is the SCARF SOLVER
re-anchored at the grips (`SUDBURIAN_LADDER.md` §7.6); rigid-body ragdoll stays
BANNED by Chad's own ruling. Do not re-open that. Do not let the LEAN near the
self-right."*

This supersedes `docs/SESSION_HANDOFF_20260827_r4a_superman.md`. Its §4 (the
traps) and §6 (the 31-tape corpus and the absent-dial trap) still stand and are
not repeated here.

---

## 0. THE STATE

| branch | tip | state |
|---|---|---|
| `sandbox/r4a-phase0` | see `git log -1` | this rung. **Committed, NOT PUSHED.** |
| `main` | `fa2cfe3b9` | R3-HANDS merged and driven. Unchanged. |

⚠ **§0.1 SUPERSEDES THE LINE BELOW.** As first committed (`8650b2bee`) nothing
the game drew had changed. Chad then ruled the scarf gets the field (§5), **drove
it, and signed it** (§6) — so the game DOES draw differently now. Everything below
about the zero defaults is still exactly true and is what the
`SEADS_SCARF_FIELD=0` arm restores.

**Nothing the game draws has changed.** `render/sled_model.cpp` was not touched:
the scarf passes none of the new fields, they default to zero, and zero is
exactly the pre-rung integrator. That is deliberate — see §3, question 1.

★ And it is PROVEN on real driving data, not argued: `seads_sled_probe superman`
compares render's SHIPPED solver against the rig's private prototype with the
terms off, and after this change it still reads **`fid 0.0e+00`** — checked on
tapes 3, 11, 20 and 31, tape 11 being the one where the four terms matter most.
A bit-identical arm is only evidence with a LIVE arm beside it, and arm C is
live in the same run (`liftC_max 166.9` vs `liftB_max 100.0` on tape 31).

---

## 1. WHAT LANDED

### 1.1 The four pseudo-force terms, in `trail_chain_step`

`trail_chain` solves in the caller's frame, and for the sled that frame is MODEL
SPACE — itself accelerating and rotating. Until now the predict loop applied
only a uniform constant `gravity_dir * gravity_mps2`. It now applies

```
g_eff = g - a_frame - alpha x r - omega x (omega x r) - 2 omega x v
             (uniform)  (Euler)    (centrifugal)        (Coriolis)
```

taken about `frame_origin` (the sled body origin IS the system CG). New
`TrailChainInput` fields: `frame_accel_mps2`, `frame_omega_rps`,
`frame_alpha_rps2`, `frame_origin`. **All default to zero.**

Applied UNCONDITIONALLY — no branch, no data-dependent bound — so §7.6's
constant-work promise holds verbatim. The term order is the measurement rig's
(drag, uniform field, then these), so the 31-tape measurement still describes
the code that shipped.

### 1.2 The seat keep-out — Chad's §2.3, "his legs will hit the seat"

A THIRD analytic projection in `apply_constraints`, beside the head sphere and
the back planes. §7.6 bans a contact SOLVER, not these. The seat is a SOLID
(the rule `render/sled_model.cpp` R3-WS(d) already established), spanning
y `[y_bottom, top(z)]` over `|x| <= x_half`, z `[z_rear, z_front]`. A point
inside leaves by its SHALLOWEST face.

The geometry is handed in by the caller — this file knows no asset — and the
test copies it out of `render/rider_pose.*`, which carries
`measure_seat_profile.py`'s own emitted 33-station table off the shipped GLB. No
number was retyped.

`TrailChainParams::seat_keepout_m` (default **0.0**) is the clearance held off
the drawn surface, i.e. **the leg's own half-thickness**. It is 0 because that
radius has not been measured off Chad — his dial, §3.

### 1.3 `trail_frame_field` — where the field comes from, and where it must not

A backward difference of the ALREADY-SHIPPED `velocity` / `angular_vel`,
render-side. The structural rule it encodes: `a_body` / `alpha_body` exist in the
kernel today only inside `SledDebugSubstep`, filled under `if (dbg)` with `dbg`
null in the game. **Do not start passing a sink in the shipped game to get
them** — that is a tape-visible change. The finite difference is untapeable by
construction.

The first call after a prime returns ZERO accel and ZERO alpha. Inventing a
previous sample out of the current one reads a respawn as hundreds of g.

### 1.4 The gate

**Gate: 1587 tests, 5 red, ALL PRE-EXISTING, verified BY NAME** (the count moves
as tests are added — ours added nine — so a number comparison lies):

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

★ `sled_debug_sink_is_write_only` was OPENED and CHECKED, not pattern-matched:
14402/14403, the single red is `REQUIRE(saw_release_under_one)` at
`test_sled.cpp:3456`, the fixture-coverage assert. Every bit-identity assert is
green — which is exactly where a regression from this rung would have shown.

Nine new cases in `test/unit/test_trail_chain.cpp` (14–22), **every one
mutation-verified**: deleting the term or rule it names makes that case fail,
checked one mutation at a time against a rebuilt binary.

| mutation | killed by |
|---|---|
| uniform `-a_frame` deleted | 14 (hangs along g − a, 45° analytic) |
| centrifugal deleted | 15 (thrown outward, bracketed by the two ends' own angles) |
| Euler deleted | 16 (swung sideways, direction asserted) |
| Coriolis deleted | 17 (out-of-plane response is ODD in omega) |
| seat projection disabled | 18 (never enters the solid; ungated arm does) |
| seat anchor-pin rule removed | 20 (disabling is bit-exact) |
| frame-field first-sample guard | 21 (no invented derivative) |

★ **Case 17 is the one worth reading.** Centrifugal and Euler are EVEN in omega
and stay in the plane; Coriolis is ODD in omega and is the only term that leaves
it. Reversing the spin must mirror the tail exactly. Delete the term and BOTH
runs sit at x = 0 — so it cannot pass with the mechanism gone.

---

## 2. ★★★ TWO BOUNDS, MEASURED AND WRITTEN DOWN RATHER THAN FOUND ON A DRIVE

Both are in `trail_chain.h` above the seat block and gated by case 19.

1. **The seat is a VERTEX keep-out.** Like the sphere and the planes beside it,
   it is projected per particle. A chain whose links are LONGER than the solid
   is thick lies straight through it with a vertex either side and nothing
   inside to find. **The measured pan is 0.598322 − 0.380322 = 0.218 m thick, so
   the body chain's segment length is bounded by that.** The measurement rig's
   coarse 5 × 0.35 m straddles it; 0.20 m links cannot. This is a CONSTRAINT ON
   THE SEGMENT TABLE the next rung authors, not a defect to close here — closing
   it any other way means a segment-vs-solid sweep, which is the contact solver
   §7.6 bans.
2. **A point already inside leaves by its shallowest face**, which for a chain
   primed straight down THROUGH the pan is DOWNWARD — it sinks rather than
   resting on top. Same class as the straight-line prime the back plane already
   documents. Prime the body in a LEGAL pose, which is what `sled_model` already
   does at bind.

★ Also re-stated in the header where it belongs, because it cost the last
session the most: **a per-step damping is meaningless without the step it was
measured at.** `damping = 0.930` is calibrated at dt = 1/120 s. A caller
stepping at the kernel sub-step must rate-convert `damping^(h / (1/120))`.

---

## 3. WHAT IS OPEN, ALL NEEDING CHAD

1. ~~Does the SCARF get the field too?~~ **★★★ RULED YES, Chad 2026-08-27:
   "yes for the scarf getting the feild then."** BUILT — see §5. AWAITING HIS
   DRIVE.
2. **`seat_keepout_m`** — the leg's half-thickness. 0.0 today (centreline on the
   drawn surface). Not measured off him.
3. **`drag_k = 0.005/m` and the 5 × 0.35 m body** are still MY derivation, not
   measured off him, and §2 above now says 0.35 is too coarse for the seat.
4. **The return-to-pose spring.** The chain has NO angular stiffness — a perfect
   flail, so inside superman it is limp. If his superman should FIGHT back
   toward the seat, that is a new term.
5. **The leg-seat strike**: visual only, or does it feed the throw? Open in §2.3
   of the ruling doc. The projection built here is visual-only.
6. Unchanged from the last handoff: `grip_capacity_n` and the LATERAL load,
   whether `grip_n` becomes 3D, `right_assist_nm = 2400` measured not flown, the
   marker's look, and whether tapes 32/33 get re-driven.

**Nothing here needs a drive yet** — the game is unchanged. The first thing
worth his time is the body chain itself landing on top of this, and then a
drive.

---

## 4. HOW TO RUN THINGS

Unchanged from `docs/SESSION_HANDOFF_20260827_r4a_superman.md` §5. The one
addition worth knowing: `./build/seads_sled_probe.exe superman <tape>` prints a
FIDELITY line comparing render's shipped solver against the rig's private
prototype with the terms off. **That number is the live proof that the zero
defaults are bit-identical** — it must stay `0.0e+00` after any change to the
predict loop.

---

## 5. ★★★ THE SCARF GETS THE FIELD — RULED AND BUILT 2026-08-27

> **Chad: "yes for the scarf getting the feild then."**

### 5.1 What it changes, in his terms

Before this, the scarf knew two things: **how fast am I going** (`wind_mps` =
minus the machine's velocity) and **which way is down** (gravity, rotated into
model space each frame, which is why a roll already swung it). It did NOT know
the frame it hangs in was being thrown around. So: brake hard and it kept
trailing instead of flying forward past his shoulder; land a jump and it was
never slammed down; hold a corner and it hung straight back instead of swinging
outboard; snap a roll on and it never cracked.

It now gets all four terms, exactly as the body chain will.

### 5.2 The wiring, five files

- `render/sled_model.h` — `SledRig` gains `omega_body_rps` and `epoch`.
- `render/draw.h` / `draw.cpp` — the two reads forwarded. ★ `angular_vel` is
  ALREADY body frame (`sim/sled.h:1121`); it must NOT be rotated again.
- `app/main.cpp` — `info.sled_omega_body_rps = sled.angular_vel` (a pure read of
  shipped kernel state, no kernel change, nothing tape-visible), plus the epoch.
- `render/sled_model.cpp` — `trail_frame_field` once per frame, held constant
  across that frame's sub-steps exactly as the wind and gravity direction
  already are. `frame_origin` stays 0: `body_from_model` is a pure rotation so
  the origins coincide, and the body origin IS the system CG.

Both scarf chains get it — `in_s` is copied from `in` after the field is set, so
the short shoulder tail cannot fork from the long one (the "moved consumer" trap
this file has paid for before).

### 5.3 ★★★ THE HAZARD THAT NEEDED A NEW SIGNAL, NOT A THRESHOLD

A finite difference across a **state override** — the mount seed, or KEY_R
autoright — reads a teleport as hundreds of g and would fling the scarf. **The
solver's own teleport test cannot catch it**: `kTrailChainTeleportM` watches the
ANCHOR, and the scarf's anchor is a MODEL-SPACE pose position, so it does not
move a millimetre when the machine is respawned underneath it.

Clamping the magnitude would have been a guess. Instead the discontinuity is
named AT ITS SOURCE: `sled_epoch` increments at the two sites that write `sled`
outside `step_sled` — **the same two that already emit a tape O record** — and
render re-primes the tracker when the epoch changes. `trail_frame_field`'s
first-call rule then returns zero accel and zero alpha for that frame.

### 5.4 The A/B, and the instrument

`SEADS_SCARF_FIELD=0` turns it off, and off is **bit-identical to the pre-rung
scarf** (every term multiplies out to `(0,0,0)`) — same shape as
`SEADS_SCARF_FLUTTER`. The env is read once at startup, so the A/B is two
launches, not a mid-drive toggle.

`SEADS_SCARF_DEBUG` now also prints
`SCARF FIELD: |a| ... |om| ... |al| ... epoch N` — so the mechanism is MEASURED
on a drive rather than assumed, and all three read 0.00 on the OFF arm.

### 5.5 ⚠ WHAT THE GATE CANNOT SEE HERE

This is caller glue in `render/` and `app/`, and **no ctest runs `seads.exe`**
(CLAUDE.md, and it is a standing lesson in this repo). The pure piece
(`trail_frame_field`) is unit-tested and mutation-verified; the frame rotation,
the epoch re-prime and the per-frame difference are certified by CHAD'S DRIVE
and by the `SEADS_SCARF_DEBUG` numbers, not by the green gate. Say so plainly —
do not report the gate as covering it.

### 5.6 What could honestly go wrong, and what to watch

The scarf is a **ribbon** — `drag_k` 0.153 against a body's 0.005, about thirty
times more reactive. Its damping 0.930 was driven and signed by Chad on a scarf
that had never felt a jolt. So the open risks, in order:

1. A crash or tumble is hundreds of g. It will move a LOT. Dramatic, or a mess —
   only the drive says which.
2. Harder accelerations push harder against the back keep-out. Watch `surf_clr`
   in `SEADS_SCARF_DEBUG`: negative means fabric inside the coat, and that is
   the number five earlier rounds were NOT graded on.
3. If it reads too whippy, the honest dial is `damping` (or `drag_k`) — one
   dial, re-driven, not both at once.

---

## 6. ★★★ CHAD DROVE THE SCARF AND SIGNED IT — 2026-08-27

> **"it sinks in at the coat just below the knots a bit if I had to complain
> about something but you did an amazing job of that. Did not check 5 but ill
> let ya know. I did check everything perfect but you got most of it for sure.
> DIdnt notice because I was immersed and the scarf helped that"**

**THE SCARF FIELD IS SIGNED.** Items 1-4 and 6-7 of the drive checklist were
flown: the forward throw under braking, the slam on landing, the outboard swing
in a held corner, the crack on a roll snap, and the crash. His verdict on the
mechanism is "perfect" and "amazing job".

★★★ **THE LINE THAT MATTERS MOST IS THE LAST ONE.** *"Didnt notice because I was
immersed and the scarf helped that."* The scarf was never the point — the point
was that the machine's motion should be legible on the man. It reads. Do not
regress this in pursuit of §7.

### 6.1 ★ CHECKLIST ITEM 5 — THE KEY_R PRESS — CONFIRMED CLEAN

> **Chad, 2026-08-27: "scarf looks good! Pressing R, Good"**

**THE EPOCH GUARD IS CERTIFIED, AND IT IS CERTIFIED WHERE IT HAD TO BE.** §5.3's
`sled_epoch` was the ONLY part of this rung whose correctness rested on a signal
introduced here rather than on a measurement — no test in the tree can see it
(§5.5), and the failure mode was loud and specific: the scarf flung across the
screen on an R press. He pressed R and it is clean.

★ The general form, worth keeping: **a finite difference over shipped state needs
a DISCONTINUITY SIGNAL, and the signal must come from whoever CAUSES the
discontinuity** — not from a magnitude threshold in the consumer, and not from a
guard that watches the wrong quantity (the solver's own teleport test watches the
ANCHOR, which does not move when the machine respawns underneath it).

**THE SCARF FIELD RUNG IS CLOSED.** The only thing outstanding against the scarf
is §7, which is a geometry defect that predates it.

---

## 7. ★★★ THE ONE COMPLAINT: THE COAT SINK JUST BELOW THE KNOT — ATTRIBUTED

> *"it sinks in at the coat just below the knots a bit"*

**ATTRIBUTED TO A MECHANISM, NOT GUESSED AT — but NOT YET MEASURED on the drawn
surface, and NOT FIXED. Read §7.3 before touching a constant.**

### 7.1 The mechanism, in three measured numbers

`render/sled_model.cpp` builds the scarf's back keep-out as two values:

```
back_keepout_m     = kScarfBackMarginM + kScarfTubeHalfM = 0.012 + 0.015 = 0.027
back_keepout_min_m = kScarfTubeHalfM                     =         0.015
```

The first is the normal keep-out: **the fabric's own half-thickness PLUS a 12 mm
visual margin.** The second is the FLOOR the anchor-pin relaxation may collapse
it to — **half-thickness ONLY, zero margin.** Its own comment says exactly what
that buys: *"it grazes instead of resting on it."*

So wherever the pin relaxation bites, the ribbon's back face lands **exactly on**
the coat surface with no breathing room at all. Everywhere else down the tail it
has 12 mm.

★ And the relaxation bites HARDEST RIGHT THERE. `trail_chain.cpp`'s own ★★★
banner already records why: `s_anch` is the anchor's depth measured against
**THIS POINT'S band, not the anchor's own band** — the anchor sits up at the neck,
and against a band further down the back, where the coat stands further out, it
reads deeply negative and drives `d` to the floor. The bands just below the knot
are precisely the ones where that gap opens.

★ The tail's authored half-thickness there is `TAIL_T * k` with `k ≈ 1` near the
knot (`scarf_geom.py::_tail_radius`, k tapers DOWN toward the tip) — i.e. 0.015,
**exactly equal to the floor.** Zero margin is not an approximation here; it is
an identity.

### 7.2 Why it showed up NOW, and why that is not a regression

Before the field the scarf hung slacker and only touched that band in passing.
The field presses it against the back during every acceleration and HOLDS it
there — so a zero-margin graze that was momentary is now sustained, and a
sustained graze reads as a sink. **The geometry defect is almost certainly
pre-existing; the field made it visible.**

★ **CONFIRM THAT FIRST, IT IS ONE RUN:** drive the same ground with
`SEADS_SCARF_FIELD=0` and compare. If it sinks there too, this is old and
independent of the field, and the fix must not be charged to the field.

### 7.3 ⚠⚠⚠ THE TRAP WAITING FOR WHOEVER FIXES THIS

**`surf_clr` CANNOT SEE THIS DEFECT.** The constraint guarantees the CENTRELINE
sits at or beyond `d`, and `d >= back_keepout_min_m` always — so `surf_clr` reads
POSITIVE, around +0.015, **while the fabric is visibly in the coat.** It is
measuring the chain; his complaint is about the SURFACE.

This is the SAME TRAP ONE LEVEL DOWN. `back_clr` stayed positive through five
failed rounds of "its still sinking into the coat", which is why `surf_clr` was
built. `surf_clr` now stays positive through this one. **The number that can fail
here must be measured on the DRAWN, SKINNED VERTICES of the tail against the
coat's banded surface — not on any chain position.** Build that instrument BEFORE
touching a constant, or this becomes round six of the same mistake.

### 7.4 The candidate fix — ONE constant, and its named risk

Raise the floor from half-thickness to **half-thickness + margin**:

```
back_keepout_min_m = kScarfBackMarginM + kScarfTubeHalfM   // 0.015 -> 0.027
```

which makes the floor equal the keep-out and gives the knot band the same 12 mm
every other band already has. ★ Note 0.027 is also the figure the file's own
line-193 comment has always quoted (*"the authored tube is 0.027 m half-thick"*)
while the shipped constant carried 0.015 — a documented number that stopped
describing the constant beside it. THE LAW, again.

⚠ **THE RISK, NAMED:** a floor equal to the keep-out DISABLES the anchor-pin
relaxation for the back planes entirely, and that relaxation exists so a root
authored inside the surface is not yanked out and kinked. The mitigating evidence
is in the code (*"with the banded planes ... the roots are AUTHORED 30 mm off the
back — no anchor stand-off"*), i.e. the root should be well outside and never need
the relaxation. **That is evidence, not proof.** Verify the knot does not kink
before believing it, and expect to need a BAND-LOCAL floor rather than a global
one if it does.

⚠ It is a FEEL change to a signed look: ONE dial, and Chad drives it.

---

## 8. THE ORDER OF WORK FROM HERE

1. **Wait for his item-5 report** (KEY_R). §6.1.
2. **Build the surface instrument** — drawn skinned vertices vs the coat's banded
   planes. §7.3. Without it, step 3 is round six.
3. **Run the `SEADS_SCARF_FIELD=0` A/B** to settle whether the sink is old. §7.2.
4. **Then** the one constant, and his drive. §7.4.
5. **Then the body chain** — anchored at the grips, segment table bounded by the
   0.218 m pan, fed the same field. That is the rung this whole session existed
   to make buildable.

---

## 9. ★★★ THE SURFACE INSTRUMENT IS BUILT, AND IT MOVED THE STORY

`surf_vtx`, commit `983652440`. The scarf's own DRAWN verts, restricted to the
solver-driven bones, posed with the same rigid one-joint skinning the mesh uses,
measured against the coat's banded planes in each vertex's own live band.
Debug-only; the bind capture is unconditional so the bind path never depends on
an env var.

Measured at once (`--smoke` + `SEADS_SLED_DEBUG_MODE` + a pinned
`SEADS_SCARF_VEL`), which is why it was built before any constant was touched:

| v (m/s) | field | `surf_clr` | `surf_vtx` | worst bone |
|---|---|---|---|---|
| 2 | ON | 0.015 | **−0.0114** | scarf_05 |
| 2 | OFF | 0.015 | **−0.0126** | scarf_05 |
| 8 | ON | 0.032 | +0.0085 | scarf_05 |
| 8 | OFF | 0.032 | +0.0041 | scarf_05 |
| 20 | ON | 0.068 | +0.0153 | scarf_01 |

### 9.1 What is now SETTLED

1. **The sink is real and measured.** At 2 m/s the drawn fabric is 11–13 mm
   INSIDE the coat while `surf_clr` reports a healthy **+15 mm**. §7.3's warning
   was right: the old number could not fail on this defect.
2. **★★★ THE FIELD IS EXONERATED.** `field=0` is EQUAL OR WORSE in every arm.
   The sink PREDATES the scarf field. §7.2's "check this first" is checked, and
   the answer is: do not charge this to the field.

### 9.2 ⚠⚠⚠ What it CONTRADICTS — my own §7, on location

§7 attributed the defect to the anchor-pin relaxation *just below the knot*. The
worst point MEASURES at **scarf_05**, the far end of the tail, at 2 and 8 m/s —
moving to scarf_01 only at 20. **Chad says "just below the knots."**

Two readings, and they are not equivalent:
- the visible worst differs from the numeric worst (the knot region is bunched,
  nearer the eye, and an overlap there reads louder than a deeper one down the
  tail), or
- his DRIVING states — accelerating, leaning, standing — put the worst somewhere
  these static smoke poses cannot reach. `SEADS_SCARF_VEL` PINS the velocity, so
  `|a|` is identically 0 in every row above; **a hard brake is exactly the state
  that cannot be synthesised this way.**

**DO NOT TOUCH A CONSTANT UNTIL THIS IS RESOLVED.** Tuning §7.4's floor against
a defect measured at scarf_05, to fix a complaint made about scarf_01, is a
constant tuned against the wrong number — round six wearing a new instrument.

★ The honest next step is ONE DRIVE with `SEADS_SCARF_DEBUG` on: Chad reproduces
the sink he saw, and `surf_vtx` names the bone and the band while he is looking
at it. That converts "just below the knots" into a vertex.

### 9.3 What the numbers DO support, stated carefully

At 2 m/s `surf_clr` sits EXACTLY at `back_keepout_min_m` = `kScarfTubeHalfM` =
0.015 — the pin relaxation is fully active — and the fabric still reaches 26 mm
inboard of the centreline, against a constant labelled *"= scarf_geom.py TAIL_T
(flat fabric)"*.

⚠ **That 26 mm is a PROJECTION ALONG THE PLANE NORMAL, an upper bound, NOT the
half-thickness.** At 20 m/s the same figure reads 53 mm because the streaming
ribbon is tilted relative to the plane. Do not quote it as a thickness, and do
not derive a constant from it without measuring the ribbon's true half-thickness
in its own frame.

---

## 10. ★★★ THE SINK IS INTERMITTENT AND LOW-SPEED — AND THAT RESOLVES §9.2

> **Chad, 2026-08-27, after the second drive: "im sure I did not see the scarf
> sink in on the last drive."**

He saw it on drive 1 and did not see it on drive 2. **Both reports are true, and
the explanation is not code.**

★ **VERIFIED, not assumed:** the only source change between the two builds he
drove was a COMMENT (the raw-`rig.ticks` note in `sled_model.cpp`). Behaviour
was identical across both drives. Nothing was fixed in between, so the
difference is CONDITIONS.

★ **And §9's own table already predicted exactly this.** `surf_vtx` is negative
ONLY at the bottom of the speed range and climbs out of it immediately:

| v (m/s) | `surf_vtx` |
|---|---|
| 2 | **−0.0114** (fabric inside the coat) |
| 8 | +0.0085 |
| 20 | +0.0153 |

**The sink is a LOW-SPEED / CRAWL condition, and it is marginal — about 11 mm.**
Spend a drive mostly moving and it never appears; spend one crawling around and
it does. That is the whole discrepancy, and it also resolves §9.2's
contradiction between where I attributed it and where he saw it: he was not
watching a settled pose, he was watching a slow one.

### 10.1 THE RULING THIS INVITES — and it is Chad's, not mine

⚠ §11 SUPERSEDES THE "PARK IT" RECOMMENDATION BELOW ON ONE POINT ONLY: the
mechanism is now ATTRIBUTED, so parking it no longer means parking a mystery. It
means parking a KNOWN skin-weight divergence whose fix is a graded reweight of a
signed asset — still his ruling, but now a cheap one to make.

The defect is: real, measured, instrumented, ~11 mm, **pre-dating the scarf
field** (§9.1), confined to crawling speed, and invisible on a normal drive. He
raised it as *"if I had to complain about something"* and then did not see it
again.

**RECOMMENDATION: PARK IT.** It is fully documented here, `surf_vtx` is in the
tree, and `drive_scarf_log.bat` converts any future sighting into a vertex in
one double-click. The body chain — the rung this entire session existed to make
buildable — is worth more than 11 mm at 2 m/s.

⚠ Do NOT close it as "not a bug". It IS a bug, it is simply below the waterline.
If a later rung puts the rider at crawling speed for real work (the walk-back,
the millwright lane, the pit stop), it comes back up the list on its own.

---

## 11. ★★★ OVERNIGHT: THE SINK IS ATTRIBUTED. IT IS A SKIN-WEIGHT DIVERGENCE.

Chad went to bed with *"please continue to work on this for a while then, make
the handoff."* This is that work. **The mechanism is found and quantified. It is
NOT fixed, and §11.5 says why that is deliberate.**

New tool, committed: `assets/character/sudburian_src/measure_scarf_vs_coat.py`
(pure stdlib, reads the SHIPPED GLB plus the generators, touches nothing).

### 11.1 The asset lane is CLEAR — it is not an authoring defect

Ray-parity containment of every scarf vertex against the coat's shells in the
AUTHORED pose, **per shell** (a combined tree is wrong: inside two overlapping
shells is an EVEN crossing count, i.e. "outside" — `coverage.py`'s own law):

| piece | verts | inside the coat |
|---|---|---|
| wrap | 360 | **0** |
| bridge_long / bridge_short | 112 / 92 | 0 / 0 |
| tail_long / tail_short | 252 / 92 | 0 / 0 |

**0 of 908.** Nothing is authored inside the coat, the wrap included. Whatever
he sees is made at RUNTIME. (★ `tail_long` = 252 verts exactly matches the 252
solver-driven verts `surf_vtx` captures in-game — the two lanes agree on what
the chain owns.)

### 11.2 ★★★ THE DEFECT: the wrap and the coat carry DIFFERENT BONES

In the band they share, y **1.2210 .. 1.3080 m**:

| surface | skin weights |
|---|---|
| scarf **wrap** | spine_03 60.0%, **neck_01 40.0%** |
| **coat**, same band | spine_03 78.2%, **upperarm_l 10.9%, upperarm_r 10.9%** |

They diverge BOTH WAYS: the wrap carries a bone the coat has none of, and the
coat carries two the wrap has none of. **Two surfaces in the same place
following different bones cannot stay in order.** Whichever bone moves, one
surface goes and the other does not, and the wrap is authored only just outside
the coat — so it takes very little.

### 11.3 How big, in millimetres — and the sentence that was backwards

Rotate one bone, take the linear-blend displacement scaled by each vertex's own
weight for it, and compare against the other surface's (which is zero, because
its weight is zero):

| bone rotated | wrap moves | coat moves |
|---|---|---|
| neck_01, 5° | 3.2 mm mean / 5.4 worst | 0.0 |
| neck_01, 20° | 12.6 / 21.3 | 0.0 |
| **upperarm, 15°** | **0.0** | **21.1 mean / 31.5 worst** |
| **upperarm, 45°** | **0.0** | **61.8 / 92.5** |

⚠★★★ **THE FIRST DRAFT OF THIS ATTRIBUTION WAS BACKWARDS AND THE NUMBERS WERE
FINE.** It said "the head follows the camera, so ordinary looking around is
10–20° of NECK." **It does not.** `render/sled_model.cpp` applies
`cam_yaw`/`cam_pitch` at `sm.n_head` — the HEAD bone, a CHILD of neck_01 — so
looking around turns the head and leaves the neck alone. The **only** thing that
rotates `neck_01` is `ws_neck_counter`, which is **identity unless the rider is
AFT**.

So the neck term is not the cause. **It fires exactly when he slides back —
which is when he says the sink DISAPPEARS. It is the CURE.** A number can be
right while the sentence around it is backwards; only asking "what actually
drives this bone in the shipped code" caught it.

### 11.4 ★ THE WHOLE STORY, and it fits every word he said

- **Riding normally** → the arms are IK-welded to the grips every frame, far
  from bind → the coat's band moves with them (21–92 mm of scale) and the wrap
  does not → the coat crosses the wrap → **"it sinks in at the coat"**.
- The band is the wrap's own band → **"right at knots"**.
- **Slide back** → `ws_neck_counter` fires → the wrap moves 40% of that, away →
  it clears → **"lean back is when it dissapears"**.
- It is small and situational → **"a bit"**, and invisible on the drive where he
  did not crawl (§10).

### 11.5 ⚠ WHY IT IS NOT FIXED, AND WHAT NOT TO DO

The obvious move — strip `neck_01` off the wrap so it follows the coat — has a
**known opposite failure mode**: the 40% IS the 2026-08-24 fix that replaced
100% `neck_01`, which had the scarf riding his SKULL (544/908 buried in the
pose). Trading one for the other is how this ladder has burned rounds before.

The honest shape is a **GRADE**: the part of the wrap that lies ON the coat
follows the coat's bones, the part at the collar keeps some neck. That is a
change to a **SIGNED asset** and it is **CHAD'S RULING**, not a handoff's. It
also wants his eye, in his Blender session, awake.

### 11.6 ⚠⚠⚠ THREE RENDER-SIDE ATTEMPTS TO MEASURE THE WRAP, ALL ARTEFACTS

Recorded so the next agent does not spend the night the same way. All three
tried to measure wrap-vs-coat penetration from the torso's banded back planes:

1. **All wrap verts vs the banded planes** → a constant **−0.3072 m**. The wrap
   goes ALL THE WAY ROUND the neck; its front half is 300 mm inboard of a BACK
   plane by construction.
2. **Restricted to the back half** → a constant **−0.1498 m**. Still meaningless:
   the neck sits well forward of the coat's back surface, so wrap verts are
   inboard of a torso back plane no matter how they are filtered. Neither moved
   by a micron across a full `lean_fwd` sweep.
3. **Nearest-coat-vertex signed distance off that vertex's posed normal**
   (built, measured, and REVERTED tonight) → **−0.0769 m** neutral, **−0.1083 m**
   leaning back. Too large for "a bit" by an order of magnitude, and it moves
   the WRONG WAY against his report. The pairing is fragile exactly where the
   two surfaces are not locally parallel — which at a wrap around a neck is
   everywhere.

**THE RULE THIS PAYS FOR, AGAIN:** a misleading instrument is worse than none.
`back_clr` and then `surf_clr` both stayed positive through rounds of "it is
still sinking"; these three would have shipped a confident wrong number in the
other direction. **None of them is in the tree.**

★ The attribution in §11.2–11.4 does NOT depend on any of them. It is a weights
comparison and a linear-blend displacement — it needs no penetration test at all,
which is precisely why it is trustworthy.

### 11.7 THE NEXT MEASUREMENT, when it is wanted

A posed containment test belongs in **Blender**, where
`verify_costume_live.py` already does ray parity correctly on real meshes, in
the POSE — not on render's torso planes. Per CLAUDE.md standing law 3 that is a
GUI session with Chad awake to see the viewport, never headless. Until then the
offline attribution stands on its own.

---

## 12. WHERE THINGS STAND, AND WHAT IS NEXT

| thread | state |
|---|---|
| The four terms + seat keep-out | **BUILT**, 9 mutation-verified cases, gate 1587/5-pre-existing |
| The scarf field | **DRIVEN AND SIGNED**, KEY_R confirmed, rung CLOSED |
| The coat sink | **ATTRIBUTED** (§11), instrumented, PARKED for his ruling |
| **The body chain** | **NEXT. Not started.** |

**THE NEXT RUNG IS THE BODY CHAIN.** Anchor the chain at the grips
(`M.grip_body` from the shipped GLB), author a segment table **bounded by the
0.218 m pan** (§2), feed it the same four terms, and his legs hit the seat the
way he ruled. That is the rung this whole session existed to make buildable, and
everything it needs is now in the tree and driven.

★ Two things to carry into it from tonight, both earned:
- the seat keep-out is a **VERTEX** keep-out, so the segment table is bounded by
  the solid's thinnest dimension (§2);
- the body chain will pose DRAWN GEOMETRY against a keep-out that grades a
  CENTRELINE — the identical trap §7.3 and §11.6 have now paid for three times.
  Decide up front which surface the legs are graded on.
