# CONSULT REPORT — scarf wrap regression (fresh-context adversarial, 2026-09-06)

Re-derived from the artefacts: `glb/prepatch.glb` (cafe8482f) vs `glb/current.glb`
(183da606a), full posed-LBS with the GLB's own IBMs and node hierarchy.
Scripts (all runnable in this scratchpad): `poselbs.py` (rig/LBS core + axis
calibration), `measure_wrap.py`, `measure2.py` (posed-space pairing),
`forensics.py` (per-vertex), `rest_vs_bind.py` (the smoking gun),
`simfix.py` / `simfix2.py` / `simfix3.py` (candidate-fix simulations).

## 0. The finding that reframes everything: REST ≠ BIND in this GLB

`rest_vs_bind.py`: for every joint j, D_j = world_rest(j) × IBM(j) should be
identity if the node-rest pose equals the bind pose. It is nowhere near:

| joint | ‖D·p − p‖ at collar pt (0,1.27,−0.50) | trans | 
|---|---|---|
| spine_03 | 328 mm | 780 mm |
| neck_01 (and head, all scarf bones) | 311 mm | 192 mm |
| upperarm_l | 196 mm | 310 mm |
| root | 0 | 0 |

The node-rest skeleton is a **seated riding snapshot**; the IBMs encode the
Blender authoring pose. This is the already-known "rest skeleton diverged"
that makes full re-export crash the mount — but its corollary was never
applied to weight edits:

> **Under a diverged rest/bind, joint weights are not interchangeable even
> between bones that articulate together.** `|D_spine_03 − D_neck_01|` at the
> collar = **62 mm**; `|D_spine_03 − D_upperarm_l|` = **251 mm**. Moving one
> unit of weight from neck_01 to spine_03 moves the *drawn* vertex tens of mm
> in the engine's driven pose — at EVERY seat station, because the engine
> composes its additive channels (hinge, counter, IK) ON TOP of node rest
> (`sled_model.cpp` pose pass: `nd.local = compose(t,r,s)` from rest), so the
> neck-vs-spine relative pose in-game is the node-rest one, not the bind one,
> whenever ws_neck_counter is identity — i.e. at every non-aft station.

## A. Hypothesis: confirmed in mechanism, wrong in magnitude and in the main term

**What the patch actually did** (`forensics.py`): 516 verts changed; the sum
of weight moved is **209.6 units out of neck_01, 209.0 into spine_03** (plus
0.6 into upperarms — the "k-NN coat resample" claim is false: the coat's
8–17 % arm terms were not reproduced, and many verts were a blunt
`neck_01:1.0 → spine_03:1.0` swap). No changed vert carries chain-bone
weight (that part of the claim holds).

**Effect at the driven neutral** (posed-LBS at node rest): the changed hem
verts moved **mean 34 mm, max 145 mm** (worst verts: pure-neck→pure-spine at
bind Y≈1.23–1.25, displaced +12 cm up / +7 cm forward). The knots
(spine .6/neck .4) are bit-identical and stayed put; the hem no longer sits
where it was authored relative to them or to the coat:

- "hem goes INTO the coat at pretty much ALL seat stations" ✓ — the offense
  is station-independent because the rest/bind neck offset is static.
- "worse with torso pitched forward" ✓ — pitch = larger theta_r ⇒ larger
  counter and larger IK deviation; the OLD hem's neck term tracked part of
  that, the NEW hem tracks none.
- "hem looks DETACHED / knots appear moved up" ✓ — hem-vs-knot-rim gap
  metric: OLD max −0.6 mm (attached) vs NEW **+6.2 mm at neutral, +9.6 mm at
  aft saturation**, on top of the 34–145 mm sideways re-sculpt.

**Where the brief's hypothesis was off:** the dominant term is NOT the
"accidental lift" of ws_neck_counter articulation, and not the missing
upperarm follow (0.4 weight-units — negligible). It is the **static
rest/bind divergence** making the neck→spine swap re-sculpt the hem
everywhere, counter or no counter. The counter and the arm IK are secondary
amplifiers (they explain why OLD failed aft/flak — see B).

**Why the old wrap failed aft/flak** (consistent, quantified in `measure2.py`
/ `simfix3.py`): under aft saturation the counter arcs the neck-weighted
front hem down/inward toward the chest while the coat (spine+arm) does not
follow — OLD hem clearance drift vs neutral: mean −13.8 mm, worst −77 mm at
aft-sat; −12.1/−63 mm at flak. That is the pre-patch sink Chad reported.

**Why the previous instrument passed the bad patch:** it measured only
*differential* wrap-vs-coat motion under test rotations applied to a common
base — i.e. it implicitly assumed rest == bind and never checked the ABSOLUTE
placement of the drawn hem in the driven pose, nor hem-vs-knot attachment.
A patch that parks the hem 12 cm from where it belongs but co-moves with the
coat thereafter gates 10× "better". Mine reproduced exactly that trap until
the rest/bind check; treat this as the lane's law: **any instrument for this
asset must include a driven-neutral placement gate.**

## B. Fix design

### B.0 What does NOT work (measured, `simfix.py`)

The brief's candidate — restore prim2, then hem copies the FULL
joints+weights of nearest coat verts (arm terms included), Y-ramped —
**re-sculpts the hem exactly like the failed patch**: posed shift vs OLD at
neutral mean 26 mm / max 124 mm, knot-gap +6.2 mm. The coat blend is still
~84 % spine_03; faithful arm terms change ~15 % of the swap. Under a diverged
rest/bind, *no weight-only edit can preserve the hem's driven placement.*
Had it shipped, it would have gated green on the old instrument and failed
Chad's eye a second time, the same way.

### B.1 The design that works: coat-blend resample + per-vert bind-position compensation ("FIX2/FIX3", measured)

Weights say how a vertex MOVES; positions say where it IS. Fix both:

1. **Restore prim2 from `cafe8482f:assets/sled/indy650.glb`** (law 4). Assert
   byte-equality with the restored state before patching (anti-double-blend).
2. **Eligibility:** prim2 verts with bind Y < 1.29, zero chain-bone weight,
   nearest coat (prim3) vert within 90 mm in bind space (eligibility and
   pairing computed from PRE-compensation authored positions). ~513 verts.
3. **Target blend:** k-NN (k=4, inverse-distance) full joints+weights from
   the local coat verts — arm terms included, exact, no dropping.
4. **Ramp:** `r = (1 − smoothstep(1.25, 1.29, bindY)) × (1 − smoothstep(0.045, 0.090, d_coat))`.
   The Y ramp keeps the knots and the band under them neck-driven (Chad's
   ruling: knots wrap the neck); the NEW coat-distance feather removes the
   hard eligibility cliff against the chain-carried bridge verts (the failed
   patch had a step there; without the feather my sim shows ~100 mm
   edge-shear at aft-sat on the hem↔bridge seam).
   `blend_new = normalize(r·coat_kNN + (1−r)·authored)`, top-4 joints.
5. **Position compensation (the load-bearing new step):**
   `p_bind' = M_new(v; D0)⁻¹ · M_old(v; D0) · p_bind`, where M_old/M_new are
   the vert's blended skin matrices under **D0 = the engine's parked-neutral
   joint palette**, and the same rotation applied to the NORMAL. This pins
   the drawn hem at D0 exactly where the accepted pre-patch wrap drew it;
   away from D0 it moves with the coat (LBS linearity, same blend field ⇒
   locally rigid offset shell riding the coat).
6. **D0 source:** capture the palette (44 × world×IBM) from the running game,
   parked, mid seat station — an env-gated one-shot dump in the CPU skinning
   loop (e.g. `SEADS_RIG_PALETTE_DUMP`), instrumentation only, no runtime
   behavior change. Fallback: the GLB node-rest worlds × IBMs (what my sim
   used) — workable but second-best; the IK/hinge deltas between node rest
   and true driven neutral become residual error (see risk R2).
7. **Proud offset: none.** Bind clearance min is already +4.8 mm and the
   compensation preserves the accepted neutral look. (The failed patch's
   2.5 mm "proud" was not even outward-coherent: measured dot(Δ, normal)
   mean 0.047 — 180 verts out, 165 in.) Keep ±2 mm along the posed coat
   normal as a dial only if Chad reports shimmer.
8. Same-count surgical byte patch of POSITION/NORMAL/JOINTS_0/WEIGHTS_0 of
   prim2's accessors only; knots (Y ≥ 1.29) bit-identical; node count 226.

**Measured prediction** (`simfix2.py`/`simfix3.py`, D0 = node rest):

| metric | OLD | failed patch | this fix |
|---|---|---|---|
| driven-neutral placement shift vs OLD | — | mean 26.4 / max 124.5 mm | **0.00 mm (exact)** |
| hem clearance drift, neutral→aft-sat (changed verts) | mean −13.8 / worst −77 mm | n/a (starts displaced) | **mean −3.2 / worst −27 mm** |
| same, flak | −12.1 / −63 mm | n/a | **−3.1 / −23 mm** |
| penetrators < −5 mm at aft-sat (non-chain) | 166 | — | **74, worst −19 mm** |
| knot-gap growth, worst pose | −2.1 mm | **+9.6 mm** | **+0.6 mm** |

The residual worst-case (−19 to −27 mm) lives in the deliberate transition
band (partial-r verts sliding between coat-follow and knot-follow) — the
"real scarf slides there" region, an order of magnitude better than OLD's
aft sink and with the hem attached to the knots, unlike the failed patch.

### B.2 Rejected alternatives, with numbers
- **Retain a fraction of neck in the hem instead of compensating** (brief's
  alternative): every retained unit of neck weight re-sculpts by
  ~62 mm/unit at neutral unless position-compensated anyway; once you
  compensate, retention only re-introduces the aft dive (0.23 neck ⇒
  ~−18 mm at saturation). Compensation + pure coat blend dominates it.
- **Positions-only proud offset:** would need 50–90 mm to cover aft-sat —
  a different garment.
- **Runtime post-pass on prim2:** law 6 track record; not needed.
- **Rewriting node rest to equal bind (root-cause cure):** forbidden —
  R3-WS gates and the whole pose stack are calibrated on this rest.
- **Plain revert only:** the honest floor. It returns Chad to the state he
  signed except the two known sinks (aft-sat, flak). If anything in B.1
  cannot be verified (esp. D0 capture), SHIP THE REVERT FIRST and do the
  rest as its own gated rung.

## C. Verification battery (closes the holes that passed two bad instruments)

Instrument: per-vertex LBS (weights × rigid joint transforms on drawn verts),
posed-space nearest-coat pairing, signed clearance along the POSED coat
normal. Never bind-space pairing for cross-prim distances (my first run
reproduced the "artefact metric" failure that way), never the banded planes.
Chain-carrying verts (11 coat-adjacent) are excluded from gates — the solver
owns them in-game; report them as observations only.

**Poses** (deltas composed on the captured D0):
1. **P0 — D0 exactly.** THE NEW GATE: posed position of every prim2 vert
   within **2 mm** of the pre-patch asset's posed position (mean ≤ 1 mm).
   The failed patch scores mean 26 / max 125 mm here — this single gate
   would have stopped it, and stops any future weight edit that forgets the
   rest/bind divergence.
2. Seat-station sweep: θ ∈ {5,10,15,20,25,30,38}°: pelvis +θ about model X,
   neck_01 −min(θ,30°), upperarms −θ (IK stand-in that holds the hands near
   the grips — verified in `poselbs.py` calibration).
3. Flak grid: neck −10/−20/−30° × arms −30/−45/−60°.
4. Head-cam: head yaw ±45°, pitch ±30° (wrap carries no head weight — must
   be a no-op on prim2; catches accidental head-weight pickup from coat kNN
   near the hood).
5. Arms-only ±35° (continuity with the historical numbers).

**Metrics and gates** (changed + transition verts, chain excluded):
- **Clearance DRIFT vs P0** per vert (robust to pairing artefacts):
  full-blend (r=1) verts: |drift| ≤ 8 mm every pose; partial-r verts:
  ≥ −20 mm floor, mean ≥ −5 mm.
- **Absolute penetration:** no changed vert < −20 mm in any pose; count
  < −5 mm must be ≤ pre-patch neutral count (no new sinkers).
- **Hem-top ↔ knot-rim gap** (48 rim verts, bind-paired intra-prim): growth
  vs P0 mean ≤ +3 mm, max ≤ +8 mm in every pose (failed patch: +9.6).
- **Tear detector:** posed stretch of bind-neighbour pairs (< 12 mm apart):
  max ≤ 1.5× the pre-patch asset's value in the same pose (catches the
  eligibility-boundary shear; the feather is the dial if it trips).
- **Byte gates:** only prim2 POSITION/NORMAL/JOINTS_0/WEIGHTS_0 bytes differ;
  accessor counts unchanged; prims 0,1,4,5 and all knot verts bit-identical;
  node count 226; weights normalized (Σ=1 ±1e-3), max 4 joints.
- Then the only real gate: **Chad drives** — park & look at collar, full-aft,
  crouch forward, flak sweep, head look-around, one fast run for wind.

**Named ways THIS design can still gate green and fail, and the catch:**
- R1 D0 palette captured in the wrong state (moving, wrong station, flak
  mount) ⇒ compensation pins to the wrong pose = re-sculpt again. Catch:
  gate P0 is computed with the SAME captured palette — so also eyeball the
  dump (root/pelvis translation should match parked telemetry) and capture
  twice (two parks), require ≤ 2 mm disagreement between the two pinnings.
- R2 Engine neutral ≠ D0 by the IK/hinge residual (fallback path) ⇒ small
  neutral shift survives. Catch: two-park capture above; if only node-rest
  is available, cap acceptance to Chad's eye and say so in the handoff.
- R3 kNN picks coat verts across a fold (inner collar vs outer chest) ⇒
  blend/normal noise. Catch: tear detector + restrict kNN candidates to
  coat verts whose bind normal has positive dot with the wrap vert's normal.
- R4 Compensated bind positions look wrong in Blender/bind-space tooling
  (they are off-surface by up to ~12 cm) and a future k-NN pass over the
  patched GLB pairs nonsense. Catch: write the provenance into the handoff:
  eligibility/pairing must always be computed from the cafe8482f positions;
  never re-patch without restoring (laws 3–4 already say so).
- R5 My pose model's IK stand-in misses a real engine articulation (e.g.
  clavicle, spine_01/02 hinge distribution). Catch: the design is
  articulation-agnostic (coat-follow is exact for r=1 verts under ANY pose
  — same blend ⇒ same transform), so only partial-r verts are exposed;
  their floor gate covers it.
- R6 Sub-frame effects (mullet wind vertex pass touching prim2?). Check at
  build time: confirm the wind pass keys off scarf-chain weights/material
  and does not read prim2's neck/spine weights; if it does, re-run its gate.

## D. Idle-motion sanity check (render/sled_model.cpp ~5306–5648)

Read in full. Chad's spec: still when parked; subtle at low speed; ramping
with riding wind.

- **Parked = still: MET.** Wind is exactly −velocity (`:5209`); no ambient
  term. The flutter cannot fire below 1.5 m/s (`kScarfFlutterVMinMps`,
  gate at `:5591`), and the old two-frame stand-off oscillator is fixed
  properly at `:5466–5499`: measured on the RAW anchor, converge with 3 mm
  deadband, growth instant, release decay 50 mm/s, applied once along the
  band normal. The short tail gets its own band/stand-off (`:5552`), no
  latch to oscillate. The flak gunner path is stateless. Residual movers the
  handoff names (frame field idle, per-frame plane re-skin) only move the
  chain if the POSE moves — parked with a still pose they are static.
- **Ramp-up: MET in shape.** Flutter frequency = Strouhal·v/hang (capped),
  amplitude A(v) = A0·Vref/(v+Vref) smoothstepped over the first 1 m/s
  above the gate — no discontinuity at onset; drag on the chain grows with
  v² below and above the gate, so perceived motion is monotonic with speed.
- **Minimal change needed: none in this block.** Two footnotes: (1) at
  exactly walking-pace 1.0–1.5 m/s the only motion is chain drag — arguably
  "subtle" as specced; if Chad wants visible life earlier, the dial is
  kScarfFlutterVMinMps (not new code). (2) the 50 mm/s decay means after a
  large transient the root eases back over ~1–1.6 s while parked — visible
  as one slow settle, not "blowing"; within spec, name it in the drive
  checklist so it is not re-reported as wind.
- The WRAP (this consult's subject) is rigid authored geometry and never
  wind-moves; only the chain does. That is consistent with the spec.

## Verdict, one paragraph

The regression is real and the failed patch caused it, but not mainly through
the mechanism the brief hypothesised: with this GLB's node-rest skeleton
diverged ~30 cm from its bind pose, the neck→spine weight swap re-sculpted
the drawn hem by up to 145 mm at every seat station (knots untouched ⇒
detached look), and both the previous instrument and any pure-differential
instrument are blind to it. The brief's own candidate fix (faithful coat
resample) measures near-identical to the failed patch and must not ship as
weights-only. Ship instead: restore prim2, coat-kNN full-blend with Y-ramp
and coat-distance feather, plus per-vert bind position/normal compensation
pinned to the engine's captured parked palette — measured: 0.0 mm neutral
regression, aft/flak sink reduced from −77/−63 mm to −27/−23 mm worst-case
in the sliding band only, knots attached (+0.6 mm vs +9.6). Gate on the
driven-neutral placement check first; it alone would have caught both bad
rounds. The idle-motion side meets Chad's spec as coded; no change needed.
