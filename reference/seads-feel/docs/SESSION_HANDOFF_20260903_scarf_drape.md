# SESSION HANDOFF 2026-09-03 — SCARF-DRAPE: the scarf rests ON the coat

Lane: `lanes.scarf` (LANES.toml), branch `sandbox/scarf-drape` off main
`d31086201`, worktree `D:\seads_sandboxes\scarf`.

## §1 Chad's report (2026-09-03, verbatim intent)

1. Sled: "as he leans forward / back his scarf goes into his coat" — wants the
   scarf resting on the coat surface **for all articulations**.
2. Flak gun: "part of the scarf distal to the knots go into the coat."

## §2 What was measured BEFORE a line changed

New smoke rig `SEADS_SMOKE_SLED_LEAN="lat,fwd"` (smoke-only, the
`SEADS_SLED_DRIVE` precedent) + the existing `SEADS_SCARF_DEBUG` surf_vtx
instrument (now `=2` prints the CURRENT value every 30 frames, separating the
resting sink from transient latches):

| condition            | drawn fabric vs coat (surf_vtx) |
|----------------------|--------------------------------|
| neutral, parked      | **−24 mm resting**             |
| full forward lean    | **−64 mm resting**             |
| full back lean       | **−21 mm resting**             |
| 11.7 m/s drive       | **−27 mm sustained**           |

The whole time `surf_clr` (the CENTRELINE against the same planes) read
+15 mm: the solver held the chain's centreline legally while the drawn pods —
rigid boxes hung off the bone frames — pitched through the surface. The exact
defect class the R4a seat probes closed, one surface over.

## §3 The fix, three parts

1. **`render/trail_chain.cpp` — the banded back-plane pass honours
   `TrailChainProbe` boxes.** When a station carries a probe, the plane must
   clear the BOX (support = hx|X·n| + hy|Y·n| + hz|Z·n| about the box's own
   centre), against the particle's band AND its two neighbours (a pod is up to
   a segment long; the plane→surface pushes measure 41..83 mm apart, so the
   neighbour test is the convex-hull test on a hunched back). No probe = the
   old centreline test bit-identical; the body chain (which has probes)
   disables the back planes entirely, so nothing shipped moves until a caller
   sets BOTH.
2. **`render/sled_model.cpp` — the scarf wires pod OBBs at bind.** The pods
   are rigid one-joint skins, so each bone's box in bone-local space
   (L = ibm·v) is a constant, and bone k's drawn frame IS probe station k's
   frame. Keep-out drops to the margin alone (the box now carries the fabric's
   thickness). Bone 0's pod hangs off the PINNED root the constraint loop
   never grades — the anchor stand-off is now pod-aware and multi-band. And
   the flutter wave's drawn copy gets `trail_chain_present_clamp` (new public
   fn): the wave stays invisible to the physics but can no longer put fabric
   inside the coat (it was re-injecting −27 mm at speed).
3. **`render/flak_gunner.cpp` — the scarf at the gun is SOLVED, not
   re-aimed.** The old drawer carried the whole authored tail rigid off
   spine_03; rigid authored fabric knows nothing about the posed coat. Now:
   the same trailing-chain solve, banded planes skinned off the POSED coat
   every frame, pod boxes measured at load, in the gun-model frame (down =
   −Y). Fixed sub-steps (240 first frame, 4 after), no clock read.
   Instrument: `SEADS_FLAK_SCARF_DEBUG`.

Shared constants moved verbatim to `render/scarf_drape.h` (one ruling, two
readers — sled bind and flak load must never retype each other's numbers).

## §4 Measured AFTER

- Sled: resting clearance POSITIVE at every lean (+2..+21 mm); at 11.7 m/s
  samples ≥ −1 mm; worst transient −9 mm for under a second on a jolt.
- Flak: pod clearance **+16 mm steady** at elevation 18° and 60°; worst
  one-frame settle transient −2.4 mm.
- Screenshots eyeballed: scarf lies on the coat's back on the sled at full
  forward lean; hangs down the coat at the gun at both elevations.

## §5 Tests

`test/unit/test_trail_chain.cpp` cases 23–24: the pod-box law (probed chain
rests box-face at keep-out; control chain without probes rests the centreline
— the 30 mm gap is what a deleted support term would erase) and the
presentation clamp (restores box clearance on a displaced copy; strict no-op
without probes).

## §6 Open / named compromises

- Transient dips (≤ 9 mm, sub-second) during jolts remain — verlet fabric
  pressing in on a spike; Chad's eye rules whether it reads as cloth or a bug.
- The wrap/knot itself is still rigid authored geometry at the neck (asset
  lane, unchanged — the §11 dead-end note in sled_model.cpp stands).
- The flak scarf keeps chain state across man/unman; a re-man within 1 m
  glides rather than re-primes (teleport guard covers the rest).
- The short tail at the gun anchors off its carried root; its stand-off rides
  the long chain's (same as the sled's rule).

## §7 Fly checklist (also given inline in the session reply)

Open `D:\seads_sandboxes\scarf\build-play\seads.exe`:
1. Ride the sled; lean full forward, full back, and circle the camera — the
   scarf should rest ON the coat everywhere.
2. Drive fast (flutter on) — the tail should whip but never sink in.
3. Walk to the flak gun, man it (O), free-look around him at low and high
   elevation — the tail distal to the knots should lie on the coat.
NOTE 2026-09-03: game-loop lane on STAND-BY; main advanced to 7b0ecf82a (loop landed). This branch is based on d31086201 -- MERGE origin/main + re-diff red set vs fresh nightly STATUS before landing. Loop's dependent ctest subsets (walker surface, untouched by this lane): -R 'player mode|mount seam|dismount|interact|flak walk-up|gun trample|walker'.

## §8 The red-team round (2026-09-03, commit b4a1453f0)

Fresh-context adversarial review of the first commit. Fixed: the short tail's
bone-0 pod had no pod-aware stand-off (the rung's own defect, one chain over,
in the region Chad named) — now `trail_chain_anchor_need`, shared by both
sites and both chains; the g-floor division amplified unfixable violations
5x — skip, don't floor; both pod-OBB binds read joint slot 0 blind — max-
weight scan; the flak no-pod fallback lost the tube-half — restored. Tests
23-24 rebuilt on a staggered-plane rig that pins the neighbour-band rule,
station indexing, and the support term. Confirmed clean by the review: R4a
body-chain and every no-probe path bit-identical; frame/scale assumptions
hold by construction; bounds safe.

Accepted, named: the flak solve runs 4 fixed substeps per draw (settle
position is fps-independent; visible sway pace is not); the `c += dp`
centre-ride is unpinned by tests (equilibrium never fires it twice; failure
direction is proud); SEADS_SMOKE_SLED_LEAN also arms in --probe (argv-gated
headless, no play-mode leak); the flak carry block still rigid-poses the
chains before the solve overwrites them (dead work, load-bearing root).

## §9 Gate

First commit (37313efb3): **1902/1908, red set == the committed baseline
member for member** (the 6 in generated/gate/known_reds.txt; the nightly's
7th, "ballistic truth harness", passed here — consistent with the ai lane's
fresh-checkout-only observation). Final tree (b4a1453f0): full gate rerun
launched detached → gate_scarf_drape.log (UTF-16; `GATE-EXIT` line = the
verdict written by the process that ran it). Re-measured runtime after the
round: steady minima positive everywhere (sled +2.7..+8.2 mm across leans
and a 0.6-throttle drive; flak +16 mm at 18°/60°).

§9 addendum: final tree (b4a1453f0) gate COMPLETE — 1902/1908, and
`tools/gate/gate_baseline.py check` says the red set is EXACTLY the committed
baseline, member for member. Verdict from the instrument, not the caller.

§9 addendum 2: merged origin/main 7b0ecf82a (the loop landing; 9-file delta,
graph regenerated not hand-merged) at 05c575cf5. Post-merge measurements
byte-identical (sled +6.6/+8.2 mm at full fwd/back lean, flak +15.8 mm), and
the full gate on the MERGED tree is 1902/1908 with gate_baseline.py check
saying the red set is EXACTLY the baseline, member for member. The branch is
land-ready on Chad's word.

## §10 The back-lean round (2026-09-04, commit 5be4df29e)

Chad's drive: "just when slid all the way back its still going into the coat
... (and bending forwards)". Root cause found by a per-particle solver dump
(SEADS_SCARF_DEBUG=2 grew it): the back keep-out was a HALF-SPACE, and at
full back lean the trunk bends forward -- a scarf physically hangs in FRONT
of the torso plane there, which the half-space forbade everywhere. The chain
wadded above the arched back (six particles in 0.09 of t, stacked to s
+0.18); the wad's ends read as fabric diving into the coat.

The keep-out is now a BOUNDED SLAB (probe-path only, 0-disabled, so every
non-filling caller including the flak is bit-identical): per-band lateral
half-width and chest front face, both measured per frame off POSED torso
verts (max-weight-joint filtered to the torso chain -- an arm never inflates
a band), and a t extent that ends at the hem and the collar. Beside, in
front of, or past the coat = free; inside = eject via the cheapest face.
Same gates in the presentation clamp and the anchor stand-off. Gate cases
25-26 pin all of it. Smoke rig gains the stand axis
(SEADS_SMOKE_SLED_LEAN="lat,fwd[,stand]").

Steady minima after, all positive: back lean +5.8 mm (alone, with crouch,
with stand), neutral +6.4, full forward +9.3, lateral leans +6.4/+9.2,
0.6-throttle drive +2.7; flak untouched +15.8.

⚠ KNOWN, separate, unchanged: the LOCAL-surface instrument reads the rigid
WRAP at a pose-invariant -0.106 m inside the local coat envelope near the
collar (lat 0.088) at every lean -- the wrap/knot is authored geometry rigid
to neck_01 and only the asset lane can move it (the §11 dead-end note in
sled_model.cpp stands). If Chad's eye still catches sink AT THE KNOT itself,
that is this debt, not the chain.

§10 addendum: slab-round gate COMPLETE on 5be4df29e -- 1904/1910, and
gate_baseline.py check says the red set is EXACTLY the baseline, member for
member. Land-ready on Chad's word.

## §11 The roll-up round (2026-09-04, commit 61a20513b)

Chad: "at the very end (back) it rolls up into the coat ... going through
it, right where a player looks when driving. Is it possible to fix or no?"
Answer: yes -- three finds on a new lean RAMP rig
(SEADS_SMOKE_SLED_LEAN_RAMP): (1) pod 0 dipped -21 mm DURING the pull-back
(pinned root, stand-off a frame late) -- station 1 now steers it out by
rotating segment 0; (2) surf_vtx's slot-0 rigid read placed a blended
junction vert 33 cm from where it is drawn (full 4-weight blend now); (3)
the flutter wave was under-resolved by a single clamp pass -- two fixed
passes now, drawn copy always exits by the back face, solver side-exit only
when decisively cheaper. After: ramp worst +3.0 mm, every static
articulation's WORST positive, flak +12.2; the only remaining negatives are
single-frame flutter flickers at speed (worst -12.8 mm; flutter OFF never
goes negative). The wrap/knot asset debt stands (-0.106 m local, pose-
invariant, asset lane's).

§11 addendum: the roll-up round's first gate caught a REAL regression in
gate case 23 (the pod-0 steering chattered at the stand-off's parked
equilibrium and floated station 1 proud) AND a process failure worth its own
line: the targeted ctest that had said 24/24 ran a STALE seads_tests -- the
verdict of a binary that never compiled the change. The gate rebuilds first;
that order is the whole reason it caught what the shortcut missed. Fixed
with a 3 mm deadband (d23ad731f); the re-gate on that commit is 1904/1910,
red set EXACTLY the baseline, member for member. Land-ready on Chad's word.

## §12 LANDED (2026-09-04)

Chad's verdict on the roll-up round: "that did not fix it but it might be a
little better land it" -- RULED improved-not-closed, landed at his word.
The remaining visible piece is most likely the WRAP/KNOT asset debt (the
one thing no render-side keep-out can move) and/or the single-frame flutter
flickers at speed. NEXT RUNG CANDIDATE: the asset-lane collar/knot pass --
re-author the wrap against the COAT's collar (scarf_geom.py vs the costume
GLB), the measurement already exists (the LOCAL metric under
SEADS_SCARF_DEBUG=2 reads it at -0.106 m).
