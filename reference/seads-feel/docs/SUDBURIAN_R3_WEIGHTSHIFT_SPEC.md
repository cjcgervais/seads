# SUDBURIAN R3-WS — FORE-AFT WEIGHT-SHIFT ANIMATION LADDER

**Status:** SPEC v1, authored by Fable 2026-08-20 from Chad's verbatim ask + the scout map.
**Worktree:** D:\seads_sandboxes\winter-gi, branch `sandbox/gi4-ride`, base `20d4cc107`.
**Scope:** POSE/ANIMATION ONLY. `sim/` is untouchable this rung (§9 deferred kernel note).
**Attempt budget:** 2. Red-team consult mandatory before the gate run (project law).

---

## 0. Chad's ask (the spec of record, paraphrase kept tight)

Aft weight shift must be as graceful as the legacy rider *appeared*: the rider's **feet
move front-to-back** on the running boards, he **slides his butt back on the seat**;
mid-aft = a **regular seated crouch at the back of the seat**, back bent over, hands
reaching for the bars, weight low and rearward; **standing aft** = butt pushed far back,
still gripping, visibly **pulling on the bars**; **full aft** = **kneeling on the back of
the seat** (knees on the rear seat surface, sitting back toward the heels — NOT upright
on his knees), everything committed rearward. Feet must never stay locked in place so
that straightening legs pulls the arms off the bars. Kernel/wheelie authority work is
**deferred by ruling** — animations only, note filed (§9).

## 1. Architecture (what changes, what must not)

- All shape math lives in `render/rider_pose.h/.cpp` (the pure, unit-testable TU).
  `render/sled_model.cpp` only *wires* it: replaces the rigid boot `socket_weld` with the
  new parametric foot/knee targets **only when the aft ladder is active**, and feeds the
  reach-solved hinge. No raylib types in the math TU.
- **0-OFF LAW:** for `rider_fwd_m >= 0` (neutral & forward lean) and any stand value, the
  pose must be **bit-identical** to HEAD `20d4cc107`. The forward-lean path (R1a/R1c
  hinge + Newton) is SIGNED and frozen. New code activates only for `rider_fwd_m < 0`.
- The R1c CG-honesty law stands: posed CG_z ≡ `lean_fwd_m`, CG_y ≡ `lean_up_m`, cleaned
  by the existing seed + 2-step Newton in `pose_and_solve_lean`. The ladder is designed
  so the Newton residual stays mm-scale (§5) — the solver must never visibly drag the
  designed pose.
- MEASURE, NEVER RETYPE: every geometric constant below marked (M) is measured off the
  shipped `assets/sled/indy650.glb` bytes (offline stdlib script → baked table with
  provenance comment, same pattern as `rider_rig.cpp` rest tables). Known traps: seat
  contact surface under the pelvis is **0.5983 not the 0.663 bbox top**; deck sheet is
  **y=0.2520 not the 0.277 lip**; deck z extents **[-1.0100, +0.3100]**; model frame is
  +Y up, +Z forward, aft = −Z; `_L` sockets are model −X; hand↔grip crossover is written
  ONCE in the chain table. Names, never indices.

## 2. Parameterization

- Aft demand `a_m = max(0, -rig.lean_fwd_m)` (metres of demanded rider-CG aft offset;
  kernel clamps at `lean_aft_max_m = 0.25` today).
- Stand fraction `s = clamp(rig.lean_up_m / 0.25, 0, 1)` (existing channel).
- **Ladder coordinate** `u ∈ [0,1]`: obtained by inverting the baked CG curve,
  `u = C_s^{-1}(a_m)` (§5). `u=1` = the full-kneel / full-butt-back pose. With today's
  0.25 m kernel clamp the runtime reaches only partial `u`; when the deferred kernel rung
  raises `lean_aft_max_m`, the ladder extends **automatically** — do not hardcode 0.25
  anywhere in the shape math.
- The pose remains a pure function of one tick of `SledRig` (determinism law §0.1 —
  no statics, no clock, no accumulators). All blends are C¹ (`smoothstep`), so motion is
  pop-free because the *inputs* (`rider_fwd_m`, `lean_up_m`) are already slewed by the
  kernel (`lean_tau_s = 0.18`).

## 3. The shape family S(u, s)

Three branches blended by C¹ weights; ALL keep both hands welded to the grips (existing
hand pin) and preserve the R2c-5 throttle/brake wrist-roll about the live bar axis.

Branch weights:
- `w_kneel = smoothstep(0.78, 0.95, u) * (1 - smoothstep(0.20, 0.40, s))`
- `w_stand = s * (1 - w_kneel)`
- `w_seat  = 1 - w_stand - w_kneel`

### 3a. SEATED SLIDE + REAR CROUCH (w_seat)
- Pelvis follows the **measured seat-top profile** `y_seat(z)` (M): a 1-D table sampled
  at ≥33 stations along the seat centreline from the GLB seat mesh (top-surface raycast,
  not bbox).
  `z_P(u) = z_P0 - u * dZ_pelvis_max`, `y_P(u) = y_seat(z_P(u)) + h_sit`,
  where `h_sit = y_P0 - y_seat(z_P0)` (M, rest sit height) and
  `dZ_pelvis_max = z_P0 - (z_seat_rear + kKneelMarginM)` (M; `kKneelMarginM ≈ 0.06`).
- The "crouched down, back bent over" look at high-u seated is EMERGENT from the reach
  solve (§4): as the pelvis retreats the torso must pitch forward to keep the hands on
  the bars, which lowers the chest — that *is* the rear crouch. Do not add a separate
  crouch dial.

### 3b. STANDING BUTT-BACK (w_stand)
- Vertical placement stays with the existing R1c lean_up solve (untouched).
- Aft: pelvis gets the same `-u * dZ_pelvis_max_stand` retreat with
  `dZ_pelvis_max_stand = min(dZ_pelvis_max, reach limit of §4 at s=1)`.
- Feet stay ON THE DECK (never kneel while standing), slid aft per §3d.
- Arms read as "really pulling": the arm-stretch schedule §4 tops out here.

### 3c. KNEELING ON THE REAR SEAT (w_kneel)
Construction order (per side, x = ±x_K with `x_K = kHipHalfSpan = 0.095` matched to the
thigh head, NOT the deck x):
1. Pelvis at the rear station: `z_P = z_seat_rear + kKneelMarginM`,
   `y_P = y_seat(z_P) + h_kneel_sit` with `h_kneel_sit` solved (§5), start ≈ `h_sit·0.85`
   (sitting back toward the heels, weight low — Chad: *not* upright on the knees).
2. **Knee ON the seat**: K lies on the intersection of the sphere
   `|P - K| = L_thigh = 0.453` (rigid bone — this is an equality, not a clearance) with
   the seat surface curve `y = y_seat(z) + r_knee` at that x (`r_knee ≈ 0.05`, pad
   radius, (M)-checked against the mesh). Take the FORWARD (+z) intersection so knees
   are ahead of the hips. Assert the intersection exists; if the sphere misses the seat,
   the pelvis station or h_kneel_sit is wrong — fail loud, never clamp silently.
3. **Foot behind the knee along the seat**: `F = K - L_shin * t̂`, `L_shin = 0.455`,
   `t̂ = normalize(seat rearward tangent at z_K + 0.15 * ŷ)` (heels slightly up). The
   foot may hang past the seat rear — that is "feet all the way to the back" and is
   correct.
4. Feed F to the existing 2-bone leg IK with the captured bend normal. Because P, K, F
   were constructed as a consistent triangle, the analytic solve must reproduce K —
   **gate: |solved knee − K| ≤ 1 mm** (this doubles as the bend-normal sign check;
   measured sign tables, never retyped).

### 3d. FOOT SLIDE ON THE DECK (seated + standing branches)
- Target on the deck contact plane: `y_F = y_deck + h_boot` with `y_deck = 0.2520` (M)
  and `h_boot = y_F0 - y_deck` (M, rest boot offset ≈ 0.0038).
- `z_F(u) = z_F0 - kFootTrack * u * dZ_pelvis_max`, `kFootTrack = 1.0` (feet track the
  hips — the legacy "grace"), clamped to
  `z_F ≥ z_deck_rear + kBootHalfLen + 0.02` (deck rear −1.0100 (M), boot 0.320 long).
- `x_F = x_F0` (M, rest).
- Blend rigid-weld → parametric target with `smoothstep(0, 0.15, u)` so u=0 is exactly
  the weld (0-OFF).
- Kneel blend: `F_final = mix(F_deck, F_kneel, w_kneel)` — C¹, no pops.

## 4. Torso reach solve (the arm-separation killer)

- Let `G` = midpoint of the two LIVE grip sockets (read per frame like `bar_axis()`),
  `L_ps` (M) = rest pelvis→shoulder-mid distance along the spine chain (≈ 0.533 + clav),
  `L_arm = arm chain dmax = 0.664662` (captured, not retyped).
- **Arm-stretch schedule**: `ρ(u,s) = lerp(ρ0, 0.965, smoothstep(0,1,max(u, 0.8*s)))`
  where `ρ0` = the measured rest arm ratio (0.7301, captured at load). Near-straight
  arms at full aft/stand = "really pulling on the bars".
- Solve the hinge pitch `θ_r ≥ 0` (forward, about model +X through the pelvis — reuse
  `capture_hinge_model`) so that `‖shoulder(P, θ_r) − G‖ = ρ · L_arm`: law of cosines on
  triangle (P, S, G) with sides `L_ps`, `ρ·L_arm`, `‖P−G‖`; θ_r = the deviation from the
  rest shoulder direction. Clamp `θ_r ∈ [0, kReachHingeMaxRad = 65°]`.
- **Hard ladder limit:** `dZ_pelvis_max` (both branches) must satisfy
  `‖P−G‖ ≤ L_ps + 0.985·L_arm` for every (u,s) — if the measured seat rear violates it,
  shorten the pelvis travel and SAY SO in the report; never let the arm clamp.
- This hinge composes with (does not replace) the R1a forward hinge, which stays
  inactive for aft demand (`kHingeAftMaxRad = 0` untouched).
- **Headline gate:** across the full sweep (§7) the arm-chain ratio ≤ **0.985** — the
  clamp at `sled_model.cpp:878-880` must never fire, so the hand pin never separates
  from the forearm. (Today's worst cell is 1.5154 with 23.69 % clamping. That whole
  pathology must read 0.)

## 5. CG honesty — baked two-DOF solve

At load, over a grid `u × s = 33 × 5`:
- Free scalars per cell: (1) `kFootTrack`-scaled foot aft position (leg mass ≈ 32 % of
  87.5 kg — real CG_z authority), (2) the arm-stretch slack ρ within [0.90, 0.985]
  (moves the trunk via θ_r — CG_y and CG_z authority). Hard constraints stay hard:
  pelvis on seat / stand height, hands on grips, feet on deck or kneel triangle.
- 2×2 Newton (reuse the fixed-Jacobian pattern of `rider_pose.h:640-656`) drives the
  posed CG (Winter/Dempster 14-segment table, `rider_pose.cpp:140-172`) to
  `CG_z = -a_m(u)`, `CG_y = lean_up demand`. Bake `C_s(u) = a_m` per s-slice; runtime
  inverts by monotone table lookup + lerp (assert monotone at bake).
- **Gates:** baked residual ≤ 5 mm every cell; runtime end-to-end Newton residual ≤ 2 mm
  (matches R1c-era numbers); the runtime Newton must move the root ≤ 10 mm from the
  designed pose (i.e. the design is honest by construction, the solver only polishes).

## 6. Wiring notes for the builder (traps already paid for)

- The hand/foot nodes are `world_override = true` — the settle sweep skips them; keep
  that invariant for any node you place directly.
- Bend-normal and sign tables are MEASURED at load (`sled_model.cpp:825-873`); if the
  kneel needs a different knee bend side, measure it, add a case to the sign capture,
  and pin it with a test — never type a sign.
- `sled_model.cpp` has ZERO test coverage by construction; every formula must live in
  the math TU and be exercised by `test/unit/test_rider_pose.cpp` additions.
- Two riders: 42-bone `sudburian_rig` only. The 19-bone legacy rider is FROZEN; nothing
  here touches Blender or the .blend (this rung is pure runtime — no Blender session).
- Scarf agent owns D:\seads_sandboxes\scarf; in winter-gi it may touch
  `sudburian_proxy.py` / `rider_rig.*` — this rung must not edit `sudburian_proxy.py`.
- Regenerate `generated/graph/` in the same commit as any structural change;
  `graph_query.py check` green.

## 7. Gates (add to test_rider_pose.cpp, tag [rider_pose])

Sweep grid: u ∈ {0, .125, …, 1} × s ∈ {0, .25, .5, .75, 1} × steer ∈ {−1,0,1} ×
lat ∈ {−1,0,1} (fwd axis at the aft demand implied by u; forward-lean cells covered by
the frozen existing tests).
1. 0-OFF: `rider_fwd_m = 0` ⇒ every joint world bit-identical to a pose computed with
   the ladder compiled out (or vs pinned goldens of HEAD).
2. Arm ratio ≤ 0.985 in EVERY cell (headline). Leg ratio ≤ 0.985.
3. Foot targets within deck extents (phase A); kneel: |solved knee − K| ≤ 1 mm and K on
   the seat surface within 5 mm.
4. CG residuals per §5. Non-vacuity: pelvis aft travel at u=1 ≥ 0.30 m AND the kneel
   branch actually engages (w_kneel > 0.9 at u=1, s=0) — no vacuous-green gates.
   ⚠ **R3-WS(b) correction:** that kneel clause is a claim about `ws_weights`, which is
   the PRE-GAIN branch weight. `kKneelRuntimeGain` ships at 0, so w_kneel at the rider
   is 0 at every (u, s) and nothing a player can see "engages". The test now asserts
   BOTH halves and says so in its name. Turning the gain on is blocked on Chad's ruling
   (hands-on-the-bars vs the kneel — they cannot both be true on this machine) plus
   mesh work at the anatomical knee fold; do NOT flip it to make the gate louder.
5. Continuity: **stepped in `a`, not in `u`** — R3-WS(b) correction. Adjacent demand
   cells (Δa = `lean_aft_max_m`/64 = 0.0039 m) max joint world delta ≤ 0.06 m, feet and
   shins INCLUDED, at s ∈ {0, 0.5, 1}. Stepping in `u` cannot see a jump in `u` itself,
   which is exactly how the stand-entry pop shipped green. The Δu = 1/32 form is kept
   as the bake's own cell measurement, but it is not the gate.
5b. **Curve shape (R3-WS(b), new):** every baked `C_s(u)` slice STRICTLY RISES, and the
   pose delta it buys per demand step stays inside the 0.06 m of gate 5. Hard-gated at
   load on the shipped bake (`LOG_FATAL`), and in ctest on an independently re-derived
   curve. A flat spot in `C` is an infinite jump in `u`.
6. Named constants keep shipped values (extend the existing rename-never-retune test).
7. Full suite: 1259 discovered (1254 at R3-WS + 5 R3-WS(b) cases); the ONLY allowed failures are the 4 pre-existing GI4
   `test_sled` lines — `sled_slides_before_it_tips_on_flat_snow` (1074),
   `sled_grip_ceiling_stays_below_the_tip_threshold` (1253),
   `sled_assist_reference_plane_is_load_weighted` (2766) and
   `sled_debug_sink_is_write_only` (3120). Debug gate via `.claude/hooks/gate.sh`.

## 8. Visual deliverable for Chad's eye (fly checklist goes IN the reply)

Build `build-play` (RelWithDebInfo, raylib 5.5 source dir flag). Smoke shots via
`SEADS_SLED_RIG_SMOKE` at: u=0 (must match HEAD), u≈0.4 seated slide, u≈0.7 rear
crouch, u=1.0 kneel, and stand s=1 × max-aft "pulling" — plus one throttle=1 cell to
show R2c-5 still composes. In-game: mouse back = aft (300 px full), LEFT_SHIFT stand,
C recentre.

## 9. DEFERRED KERNEL NOTE — ★ NO LONGER DEFERRED: BUILT AS K-WS1, 2026-08-21

> ★★★ **THIS SECTION IS SUPERSEDED BY `docs/KERNEL_WS1_SPEC.md`'s BUILD RECORD.** The
> kernel rung LANDED on sandbox/gi4-ride: `lean_aft_max_m` 0.25 → **0.2786** with a
> stand-dependent 5-slice ceiling (`sim::kAftCeilC1`, cross-checked against this asset
> at ctest time), and airborne authority is REAL but **built OFF** (`k_air_shift`).
> Items 1 and 2 below are ANSWERED; item 3's "wheelie rise should strengthen" was
> MEASURED and is a QUESTION for Chad, not a change. Read the build record first.

## 9. DEFERRED KERNEL NOTE (filed, not built — Chad's ruling)

For the future kernel rung, in one place:
1. `lean_aft_max_m` 0.25 → raise toward the **measured** full-ladder CG travel. The
   spec's ≈ 0.45–0.55 m was an expectation and it is WRONG; what binds is the REACH,
   measured in closed form off the shipped GLB (render/rider_pose.h):

   | binding cell | pelvis travel the arms allow |
   |---|---|
   | steer 0, ρ ≤ 0.985, hip rise 0.00 | **0.3364 m** |
   | steer 0, ρ ≤ 0.985, hip rise 0.35 | **0.3277 m** ← the binding cell, → `kLadderPelvisAftM` 0.320 |
   | steer 0, ρ ≤ 0.965, hip rise 0.35 | 0.3148 m |
   | steer −1, ρ ≤ 0.985, hip rise 0.00 | **0.1436 m** ← OPEN-R3WS-STEERREACH |

   ⚠ **RE-FILED AT R3-WS(c) (A2 — the old figures are STALE).** The reach table above
   still describes `kLadderPelvisAftM`, which is unchanged and is now only the
   kneel/reach anchor. What the RUNTIME ladder does is different: the seated retreat
   is the eye-dial `kSeatRetreatAftM = 0.20` and the CG is carried mostly by the
   1.0054 m full runner foot slide. Re-measured off the shipped bake (printed at
   every load, `SLED R3-WS(c) BAKE C(1) per stand slice`):

   | stand s | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
   |---|---|---|---|---|---|
   | R3-WS(b) C(1) | 0.2251 | — | — | — | 0.2164 |
   | R3-WS(c) C(1) | 0.2183 | 0.2146 | 0.2083 | 0.1990 | 0.2021 |
   | R3-WS(d) C(1) — **SUPERSEDED** | 0.2786 | 0.2668 | 0.2306 | 0.2041 | 0.2021 |
   | **R3-WS(e) C(1) — SHIPPED** | **0.2183** | **0.2146** | **0.2070** | **0.1984** | **0.2021** |

   ⚠⚠ **RE-FILED AGAIN AT R3-WS(e) (2026-08-21) — THE KNEEL LEFT THE LADDER
   (§14), SO THE ROW BELOW THIS ONE IS HISTORY.** The kneel-free bake is the
   row the kernel is sized against now: `sim::kAftCeilC1 = {0.2183, 0.2146,
   0.2070, 0.1984, 0.2021}`, `lean_aft_max_m = 0.2183`. The seated end loses
   60.3 mm and lands BELOW the pre-K-WS1 flat 0.25 — see §15. Everything from
   here to the end of §9 describes the R3-WS(d) era and is kept as history.

   ⚠ **RE-FILED AT R3-WS(d) — THIS WAS THE ROW THE KERNEL RUNG SIZED AGAINST.**
   The kneel is ON, and it buys real seated authority: **C_0(1) = 0.2786 m**,
   +60.3 mm over (c). D2 predicted 0.25 ± 0.02 and set 0.255 as the line at
   which the K1 "more seated authority" claim dies — the bake lands ABOVE it,
   so **K1 SURVIVES**, measured, not assumed. Two consequences for the kernel
   rung: (1) at zero stand the ladder now delivers MORE than the kernel's
   `lean_aft_max_m = 0.25` asks for, so the seated CG_z clamp lie at s = 0 is
   GONE and the last of the mouse travel stops at u ≈ 0.93 instead of 1.0 —
   the full kneel is currently UNREACHABLE IN PLAY without raising the clamp;
   (2) the STANDING slices are unchanged (the kneel is dead there by
   construction), so the stand still asks for 48 mm more than it can deliver.
   Measured CG_z solver residual over the shipping range: **27.4 mm** (c: 38.7).
   The ladder's declared CG_y departure is **144.5 mm**, unchanged — D7's
   "grows to 0.26–0.27" is REFUTED: the kneel's own declared CG_y rise is
   **+83.7 mm** at s = 0, and the worst departure is still the STANDING
   crouch's −144.5 mm, which is the number Chad already ruled on.

   So the ladder's ceiling is ≈ **0.218 m** of rider-CG travel, and the kernel's
   `lean_aft_max_m = 0.25` already asks for more than the animation can honestly
   deliver: the measured CG_z clamp lie over the shipping range is **38.7 mm**
   (R3-WS(b): 25.2 mm — it GREW, exactly as A2 predicted, and it is reported rather
   than solved away). A raised `lean_aft_max_m` buys nothing without a rig or bar
   change; a LOWERED one to ≈ 0.218 would make the pose honest to the millimetre.
   That is a kernel ruling for Chad, not an animation dial. The ladder's declared
   CG_y departure from the R1c law is now **144.5 mm** (was 165 mm — it shrank, as
   §11.3 expected).
2. **Airborne authority is currently ZERO**: `cg_off` only moves the contact-patch
   mounts (`sim/sled.cpp:313-317`); with skis in the air weight shift does nothing.
   For catwalk authority the big man needs cg_off (and ideally the rider inertia — the
   named 16 % approximation at `sim/sled.cpp:302-312`) in the rigid-body/gravity terms.
3. Mass context: rider 87.5 of 331 kg — scale felt authority accordingly; wheelie rise
   "the sled rises to him" should strengthen with aft shift.

## 10. Process

Opus builds to this spec. Fresh-context Fable red-team audits the DECISION and the
artefact (not the report) before the gate run burns the attempt. Verify = gates + smoke
shots + this checklist; then AWAIT CHAD'S EYE/DRIVE. Commit on `sandbox/gi4-ride`,
NO PUSH.

---

## 11. R3-WS(c) AMENDMENT — Chad's drive feedback #1 (2026-08-20, VERBATIM RULING)

Chad's words, distilled: feet only go back a little — they must slide **the full length of
the foot runners**, "all the way back", with the front end = "bracing in the wells up
front" (= the rest/neutral stance; forward-lean foot work is DEFERRED by his ruling).
The butt currently "sticks back so far it looks funny"; instead **his butt can then
[come] down a little as he bends his knees some**, and **his head [comes] up** — today he
goes "full face down even when standing at the back" and "does not look like he is
pulling on the bars". Forward lean pose: "looks good so far" — FROZEN, do not touch.

### 11.1 The mechanism of the defect (why face-down)
The v1 ladder buys aft CG mostly with PELVIS RETREAT (dZ_pelvis_max 0.320, reach-limited),
and the §4 reach solve then must lay the trunk over (~50°) to keep the hands welded —
face-down is the *solution* to an over-retreated pelvis. The fix is a re-allocation of CG
authority: legs are ≈32% of rider mass; the full runner slide moves that mass ~0.6–0.9 m,
buying ~0.19–0.26 m of rider-CG shift, so the pelvis retreat (58% trunk share) can shrink
to a natural amount and the trunk pitch can be CAPPED.

### 11.2 New shape law (aft ladder v2 — replaces §3d foot law + re-times §3a/3b pelvis)
1. **Full foot slide, decoupled from the pelvis** (kFootTrack retired):
   `z_F(u) = lerp(z_F0, z_rear_limit, smoothstep(0,1,u))` with
   `z_rear_limit = z_deck_rear + kBootHalfLenM + 0.02` (measured: −1.0100 + 0.1678 +
   0.02 = −0.8222). Non-vacuity gate: at u=1 the foot target IS z_rear_limit (±1 mm).
   `y_F` stays on the deck contact plane; `x_F = x_F0`. Front end of the range is the
   rest weld (u=0, bit-identical 0-OFF unchanged).
2. **Trunk pitch cap — the head-up law**: the §4 reach hinge is clamped
   `θ_r ≤ kHeadUpTrunkMaxRad` (start 38°, one named dial for Chad's eye). The pelvis
   retreat is now DERIVED from the cap, not chosen: dZ_pelvis_max(s) = the largest
   retreat for which the reach solve satisfies ‖shoulder−G‖ = ρ·L_arm with θ_r at or
   below the cap and ρ ≤ 0.975. (Predict: pelvis retreat drops to roughly 0.15–0.22 m —
   the "butt back" reads natural, not funny.)
3. **Butt down as the knees bend**: standing branch adds
   `y_P -= kAftSquatDropM * smoothstep(0.4, 1, u)` (start 0.10 m) — the stand-pull at
   full aft is a knees-bent brace, hips slightly dropped, chest up. Seated branch: pelvis
   stays on the seat profile (unchanged law), which with the reduced retreat keeps him on
   the pan, not the crest.
4. **Head up**: counter-rotate `neck_01` by `min(θ_r, kNeckCounterMaxRad)` (start = 30°)
   so the helmet faces the horizon whenever the trunk pitches — composes with the
   existing cam look channels; at θ_r = 0 it is exactly zero (0-OFF safe).
5. **Knee bend is emergent** (foot far aft + pelvis moderate + pelvis drop) — no knee
   dial. Gate leg ratio ≤ 0.985 on ladder cells (now satisfiable: hip→foot distance
   shrinks in this geometry) and keep the kneel branch OFF as ruled.
6. **CG accounting**: re-bake C_s(u) with the v2 shape. Expected C(1) ≥ 0.25 (legs do
   the work) — the kernel clamp becomes the binding limit, which should shrink or kill
   the 25.2 mm CG_z clamp lie. Report the new C(1) and residuals. All (b) gates stay:
   monotone bake, continuity-in-a at every s slice, curve-shape, stand-entry.
7. Forward lean (`rider_fwd_m >= 0`): bit-identical, as before. Forward foot-slide /
   front-well bracing work: DEFERRED (Chad: "there may be more about the weight shifting
   up front but we will defer that for now").

### 11.3 Open interactions to watch
- Steer at full aft: the outboard-arm bind (OPEN-R3WS-STEERREACH) gets *easier* with a
  more upright trunk (shoulders closer to the bars), but re-measure the worst steer cell.
- The 165 mm declared CG_y departure should SHRINK (less crouch) — report the new value.
- Boot orientation on the long slide: keep soles flat on the deck; no toe-drag through
  the tunnel sides (feet stay at x_F0, deck x span |0.21..0.375| — assert inside).

### 11.4 RED-TEAM AMENDMENTS (2026-08-20 plan audit — BINDING, override §11.1-11.3 where they conflict)

Audit calibrated a replica against the shipped bake (reproduced C_0(1)=0.2251 exactly).
Verdict EXECUTE WITH AMENDMENTS; these are the amendments:

A1 (P0-1) **Retreat is an explicit eye-dial, NOT derived from the trunk cap.** New named
constant `kSeatRetreatAftM = 0.20` (pelvis z −0.785, seat rise +4 mm, θ_solve ≈ 19°).
`kHeadUpTrunkMaxRad` (38°) remains a hard CLAMP only. The judged quantity is
trunk-from-vertical = θ_r + 28.6° rest tilt — print it in the sweep report. New gate:
seat-follow rise at u=1 ≤ 10 mm above the pan (via `seat_top_y`, pure TU) — the butt
stays OFF the rear crest. `kLadderPelvisAftM = 0.320` is NOT reused for the seated
retreat — it stays frozen as the kneel/reach anchor (kneel branch numbers must not move).

A2 (P0-2) §11.2.6's "C(1) ≥ 0.25" prediction is DELETED (it conflated foot travel with
leg-CG travel: legs buy ≈ +0.066 m net over v1). Honest expectations: seated C_0(1) ≈
0.23–0.25 at retreat 0.20; stand C_1(1) ≈ 0.18 — BELOW the 0.25 kernel clamp, so the
CG_z clamp lie GROWS at stand (≈72 mm shortfall on the Newton floor). Bake must REPORT
C per s-slice; §9.1's kernel-note table (0.3277/0.320/0.2251) must be RE-MEASURED and
re-filed in the same commit — the coming aft-clamp-raise rung sizes against it.

A3 (P0-3) **Re-time the foot slide against the continuity budget.** Raw full slide is
0.92 m ≈ 2.9× v1; measured worst joint delta per demand step at s=1 would be 0.091 m
> the 0.06 LOG_FATAL bound → load death. Widen the foot z-schedule band with s the same
way ws_reach_gate widened flexion: `z_F(u,s) = lerp(z_F0, z_rear_limit,
smoothstep(0, hi_f(s), u))` with hi_f(0)=1.0 and hi_f(1) sized by the bake sweep to meet
the 0.06 bound (s=0 and 0.5 already fit: 0.016/0.024). Keep the LOG_FATAL backstop.

A4 (P0-3b) **Re-pin enumeration** (every one, no silent edits): `static_assert
(kLadderPelvisAftM > 0.30f)` stays (constant unchanged); rename-never-retune list gains
`kSeatRetreatAftM`, `kHeadUpTrunkMaxRad`, `kAftSquatDropM`, `kNeckCounterMaxRad` at
shipped values; `kFootTrack` is retired — remove its pin with a comment naming this
amendment; non-vacuity anchor MOVES from pelvis travel to the FOOT: travel ≥ 0.90 m·
(band permitting per A3, assert ≥ 0.60 m at s=1) and foot ≡ z_rear_limit ±1 mm at u=1,
s=0; gate 7.4's "pelvis aft ≥ 0.30" retargets to the kneel anchor, not the seated
retreat; the deck-foot stepping test re-forms to the (u,s) schedule.

A5 (P1-4) **True rear limit uses the HEEL, not the boot centre**: measured heel offset
0.0866 m behind the ankle → `z_rear_limit = z_deck_rear + kBootHeelBackM + 0.02 =
−0.9034` (+81 mm vs the naive half-length figure). Measure `kBootHeelBackM` with
measure_seat_profile.py (it already prints boot extents) and bake with provenance.

A6 (P1-5) The leg gate takes the arms' no-regression ceiling form:
`ratio ≤ max(0.985, same-cell ladder-free ratio)` — HEAD's stand legs already read
1.107–1.1246 and are frozen by 0-OFF; an absolute 0.985 is red on day one.

A7 (P1-6) **Knee/shin clearance gate** (pure TU, per sweep cell): knee x ∈
[seat half-width 0.206 + kKneePadRM 0.0918, deck outer 0.375 + 0.03], and the shin
segment clear of the seat prism. (Measured: the rest bend normal splays knees to
x 0.47–0.54 at u=1 — outboard, legal; the gate exists to catch any capture change that
would fold them INTO the seat.) Chord floor: assert hip→ankle chord ≥ 0.25 everywhere
(shard chord is 0.197; v2 min is 0.42 — margin, cheap insurance).

A8 (P1-7) Well-posedness pinned: the trunk cap applies AFTER ws_reach_gate
(`min(θ_solve, cap) · gate` is WRONG; use `min(θ_solve · gate, cap)` — a ceiling, not a
re-timing); any capped-out reach shortfall flows through ws_solve_station's existing
shorten bisection so want/got stay converged; retreat-related quantities are captured
per s-slice at load in capture_weight_shift (steer 0, closed-form stand reference) and
interpolated in ladder_state. Keep the bake monotone gate; expect thin dC/cell
(~0.0008) at s=1 — if it goes non-monotone, re-time hi_f(s), don't relax the gate.

A9 (P1-8) **Neck counter in the pure TU**: `ws_neck_counter(theta_r)` returns the
rotation about MODEL +X conjugated into neck_01's parent frame (the q_hinge pattern);
sign MEASURED at load per the standing rule, never typed; SledModel gains an n_neck
capture; unit test pins 0 at θ_r=0 (0-OFF) and monotone counter up to
kNeckCounterMaxRad. Head cam channels compose after (child node) — unchanged.

A10 (gates added): head-up gate θ_r ≤ kHeadUpTrunkMaxRad across the whole sweep
(headline of this amendment — it had NO gate); crest gate (A1); heel gate (A5).
Doc fix: §4's "L_arm = 0.664662" is stale — shipped capture is dmax 0.613000.


---

## 12. R3-WS(c) — WHAT SHIPPED, AND THE THREE PLACES IT DEVIATES FROM §11.4

Built 2026-08-20 to §11 + §11.4. `sim/` untouched, no Blender, no `.glb` edit (the
measurement script only READS the file). Forward lean bit-identical (0-OFF gated
joint-by-joint). Kneel branch still OFF at `kKneelRuntimeGain = 0` and
`kLadderPelvisAftM` still 0.320.

### 12.1 The shipped dials

| constant | value | provenance |
|---|---|---|
| `kBootHeelBackM` | 0.086631 | MEASURED (A5), `measure_seat_profile.py`, CPU-skinned boot: ankle 0.086631 ahead of the heel, 0.248964 behind the toe; worse of the two sides |
| `kFootZRearLimitM` | −0.903369 | `kDeckZRearM + kBootHeelBackM + 0.020` — **+81 mm** aft of the naive half-length figure |
| foot travel | **1.0054 m**, at every stand slice | rest ankle z +0.10204 → the rear limit |
| `kSeatRetreatAftM` | 0.20 | A1 eye-dial; seat under the pelvis rises **9.0 mm** (crest gate 10 mm) |
| `kHeadUpTrunkMaxRad` | 38° | A1/A8 ceiling, applied `min(θ_solve·gate, cap)`; it BINDS (sweep worst θ_r = 38.00°) |
| `kRestTrunkTiltRad` | 28.6173° | MEASURED pelvis→spine_03 — the judged quantity is the sum: **66.62° from vertical** at the worst cell (v1: ~79°) |
| `kAftSquatDropM` | 0.10 | §11.2.3, standing branch only, `smoothstep(0.40, 1.00, u)` |
| `kNeckCounterMaxRad` | 30° | A9; sign MEASURED at load (`neck_up_sign` → **−1** on this asset), conjugated into neck_01's parent frame |
| `kFootSlideStandHiU` | **1.00** | swept — see 12.2 |
| `kReachGateStandHiU` | **1.00 → 1.40** | swept — see 12.2 |
| `kFootTrack` | **RETIRED** | A4; the feet no longer track the pelvis |

### 12.2 DEVIATION 1 (A3): the foot band is NOT the continuity lever — the flexion band is

A3 predicted the fix was to widen `hi_f(s)` (the FOOT slide band). Measured on the
shipped bake, no foot band passes at any value:

| foot hi_f(1) | reach hi(1) | s=0 | s=.25 | s=.5 | s=.75 | s=1 | |
|---|---|---|---|---|---|---|---|
| 1.00 | 1.00 | 0.0242 | 0.0262 | 0.0316 | 0.0516 | **0.2727** | FAIL |
| 1.20 | 1.00 | 0.0242 | 0.0254 | 0.0304 | **0.0694** | **0.1162** | FAIL |
| 1.45 | 1.00 | 0.0242 | 0.0246 | 0.0289 | **0.1009** | **0.2355** | FAIL |
| 1.80 | 1.00 | 0.0242 | 0.0263 | 0.0304 | **0.0860** | **0.2695** | FAIL |
| **1.00** | **1.40** | 0.0242 | 0.0264 | 0.0287 | 0.0389 | 0.0276 | **SHIPPED** |

(worst joint travel per 0.0039 m step of aft demand, bound 0.060.) The reason is
that the gate is a RATIO: slowing the foot cuts the joint travel AND the aft CG it
earns, while the trunk flexion keeps spending CG forward — so at high stand `dC`
collapses faster than `dj` and the number gets WORSE. The lever is the flexion, as
it was in R3-WS(b). `kReachGateStandHiU` therefore moves 1.00 → 1.40 (re-pinned in
the rename-never-retune test, mid-plateau: 1.35/1.40/1.45 → 0.0380/0.0389/0.0401,
clear of the cliff between 1.25 → 0.0580 and 1.30 → 0.0301), and the FOOT band ships
at 1.00 — meaning **the feet go all the way back at every stand fraction**, which is
what Chad asked for. No gate was relaxed; the bake's LOG_FATAL is untouched.

### 12.3 DEVIATION 2 (A7): the knee clearance floor is a NO-REGRESSION form, not absolute

A7's absolute band (knee |x| ≥ seat half 0.2060 + pad 0.0918 = 0.2978) is not this
asset. MEASURED with the ladder OFF: the REST knee is at |x| 0.268404 (29 mm inboard
of the floor) and at stand s = 0.75 it reads **0.1973 — inside the seat prism
itself**. Both are HEAD's own pose, frozen by the 0-OFF law. Filed as
**OPEN-R3WS-STANDKNEE**. What ships is the same no-regression form A6 uses for the
legs: the ladder may never walk the knee more than 20 mm inboard of the same cell's
ladder-free knee (measured worst is millimetres, and it is the leg straightening as
the foot goes aft). Chord floor (A7) holds absolutely: min hip→ankle 0.3762 m
against the 0.25 floor.

### 12.4 DEVIATION 3: the ctest pose model needed the runtime's stand reference

The R3-WS(b) re-derived pose model modelled the stand as a pure +Y rise. The runtime
resolves against `lean_seed(hinge, 0, 0, lean_up_m)`, whose inverse Jacobian mixes
both axes. That approximation was fine while the feet moved 0.32 m; with 1.0054 m
under it the two models' `C_s(u)` diverged at full stand and the test failed a pose
the shipped bake measures as clean. The model now re-derives the hinge by probing
ITSELF (exactly as `capture_hinge` probes the shipped path) and uses the same
`lean_seed`. It also carries the neck counter — head+neck is 8.1 % of the body.

### 12.5 The measured result

| quantity | R3-WS(b) | R3-WS(c) |
|---|---|---|
| foot travel | 0.32 m (pelvis-tracked) | **1.0054 m**, every stand slice |
| most aft heel | — | z −0.9900 vs deck rear −1.0100 (20 mm clear) |
| pelvis retreat (seated) | 0.320 m | **0.20 m**; seat rise 9.0 mm |
| worst θ_r | ~50.8° (kneel cell) | **38.00° = the cap**; 66.62° from vertical |
| neck counter | none | 30.00° at the cap, sign −1 measured |
| bake continuity-in-a, by slice | — | 0.0242 / 0.0264 / 0.0287 / 0.0389 / 0.0276 (bound 0.060) |
| runtime sweep continuity-in-a | 0.0209 / 0.0170 / 0.0186 | worst **0.0198**; shin/foot 0.0408 |
| worst arm ratio, ladder cells | 1.4287 | **1.4290** (HEAD's frozen cell is 1.4292) |
| arm no-regression | +0.0270 | **+0.0349** (gate ≤ 0.05) |
| worst leg ratio | 1.1737 | **1.1197** — better than HEAD's own 1.1246 |
| C(1) s=0 / s=1 | 0.2251 / 0.2164 | **0.2183 / 0.2021** |
| CG_z clamp lie (shipping range) | 25.2 mm | **38.7 mm** (A2 predicted the growth) |
| declared CG_y departure | 165 mm | **144.5 mm** |

### 12.6 OPEN FOR CHAD

1. **`lean_aft_max_m` 0.25 vs the ladder's honest 0.218** — the kernel asks for 32 mm
   more aft CG than the animation can deliver, so the last of the mouse travel is a
   38.7 mm lie. Lower the kernel dial, or accept it until the aft-clamp rung.
2. **The four eye-dials are his**: retreat 0.20, trunk cap 38°, squat drop 0.10,
   neck counter 30°. Every one is a single named constant.
3. **OPEN-R3WS-STANDKNEE** (12.3) — HEAD's standing knees are inside the seat prism.
4. The kneel branch and OPEN-R3WS-STEERREACH are unchanged and still his rulings.

---

## 13. R3-WS(d) — KNEEL ON (Chad's ruling 2026-08-20 night: "Turn on the kneel! yes :)")

Chad ruled on the (c) report's open questions: (1) the kernel clamp question is CARRIED
to the kernel rung and that rung is GREEN-LIT ("make it real and better" — see
docs/KERNEL_WS1_SPEC.md); (2) THE KNEEL TURNS ON; (3) OPEN-R3WS-STANDKNEE and
OPEN-R3WS-STEERREACH are DEFERRED with a note. He did not separately answer
"hands on bars vs release" — turning the kneel on with the hand welds in place IS the
implicit answer (hands stay welded; some trunk layover at full kneel is accepted).
Surface the resulting trunk angle in the report so he judges it with eyes, not numbers.

### 13.1 The change
- `kKneelRuntimeGain` 0 → 1. The (b) gate that pinned gain==0 flips to pin gain==1 with
  a comment citing this ruling. The 7.4 non-vacuity gate becomes REAL: runtime
  w_kneel > 0.9 at u=1, s=0 (previously asserted pre-gain only).

### 13.2 Reconciliations forced by the (c) laws (the reasons it was held OFF)
1. **Shard guard (the knee-fold tear):** the v1 kneel construction reached hip→ankle
   chord 0.197 m — below the measured skin-shard chord. The A7 chord floor (≥ 0.25) now
   applies to the KNEEL branch too: solve `h_kneel_sit` (and if needed the heel-tangent
   pitch) so every kneel cell keeps chord ≥ 0.25 + 0.01 margin — a shallower heel-sit,
   knees on the seat, butt toward but not ON the heels. If the floor and the seat
   surface constraint conflict, the seat wins and the report says by how much the
   heel-sit had to lift. NO skin shards may ship — judge the screenshot.
2. **Trunk cap exception in the kneel band:** effective cap =
   `lerp(kHeadUpTrunkMaxRad, kKneelTrunkMaxRad, w_kneel)` with `kKneelTrunkMaxRad = 55°`
   (new eye-dial, re-pin). The head-up neck counter keeps running (cap its input at
   kNeckCounterMaxRad as shipped) so even laid over he looks AT the bars, not the tank.
3. **ρ in the kneel band** may run to 0.985 (the arms genuinely stretch at full kneel).
   The arm no-regression gate (≤ +0.05) stays LAW — if it trips, the kneel pelvis
   station moves forward (it is NOT frozen; only kLadderPelvisAftM the ANCHOR is).
4. **Re-bake:** kneel ON changes C_s(u) at low s — re-bake, keep every (b)/(c) gate:
   monotone per slice, continuity-in-a (all slices, 0.06 bound — the deck→kneel ankle
   swing is **0.405 m** post-(c), NOT the 0.731 m this line first carried (D5); the
   [0.60, 1.00] band SURVIVES on that number and was not widened chasing the stale
   one — measured bake continuity-in-a at s = 0 is 0.0245. If the bound trips, widen the
   kneel blend inside the band, never the gate), stand-entry, crest (seated non-kneel cells),
   knee/shin clearance (kneel cells: knee ON the seat is the target, so the clearance
   gate takes its kneel form: |knee_y − (seat+pad)| ≤ 5 mm, knee x within the seat
   half-width — the OPPOSITE sense of the deck-branch gate).
5. **Declared CG_y** will grow again (kneel lifts the hips ~0.17). Report the new
   worst-case number; it is covered by the standing CG_y ruling Chad already took.
6. **New C_0(1) is the number the kernel rung needs** — report it prominently; expect
   ≈ 0.27–0.30. Re-file §9.1 again in the same commit.

### 13.3 Deliverables
Commit "R3-WS(d): kneel ON (Chad's ruling) — shard-guarded, cap-blended" + smoke shots
r3ws_d_shot_1_kneel_engage.png (u≈0.7, s=0), _2_kneel_full.png (u=1, s=0, side),
_3_kneel_full_rear34.png (rear three-quarter — the shard check view), judged. Gates
green (1260+/1264+ tally, only the 4 known GI4 names). No push.

### 13.4 RED-TEAM AMENDMENTS TO §13 (2026-08-20 plan audit — BINDING)

Replica calibrated (reproduces shipped h_kneel 0.2549, v1 kneel θ ±1°, A1 θ_solve ±0.7°).

D1 (A-P0-1, BLOCKING) **The kneel×stand kill band is an ungated pop.** w_kneel dies
across s ∈ [0.20, 0.40]; the up-slew crosses that in ~2–3 ticks at 60 Hz, forcing
~0.14–0.18 m of joint travel PER FRAME (knee 0.42 m, ankle 0.405 m, pelvis 0.265 m
across the band) — 3× the 0.06 bound, on the player's own stand key. Every existing
continuity gate steps in `a` at fixed s and misses it. REQUIRED: (a) a continuity-in-s
gate — worst joint delta per tick of stand slew at u ∈ {0.8, 1}, same 0.06 m bound;
(b) size the fix by WIDENING the kneel s-band (start ≈ [0.20, 0.70], derived the same
way the u band was) — or slew-limit w_kneel; never the gate.

D2 (A-P0-2) §13.2.6's "expect ≈ 0.27–0.30" is WRONG (double-counts the pelvis, ignores
the legs folding FORWARD onto the seat). Replica: **C_0(1) ≈ 0.25 ± 0.02**
(+0.082 pelvis·riding-mass − 0.037 trunk-forward − 0.012 legs). If the bake lands
< 0.255, the K1 "more seated authority" claim DIES and goes to Chad as a question.

D3 (A-P0-3) The chord-guarded kneel EXISTS: floor 0.26 → flexion 146.7° (< 155),
h_kneel_sit 0.2549 → **0.2902**, knee ON the pan (z −0.520), ankle on the rear rise.
BUT trunk-from-vertical at full kneel = **79–84°** (θ_solve 50–54.6°; the 55° "cap"
is decoration). Chad rejected 79° in v1 seated — this MUST be the HEADLINE of the (d)
report with the side shot, plus the pre-computed lever: kneel station forward
(0.320 → ~0.24–0.26, kneel branch only) trades ~10–15 mm of C for ~10–15° of trunk.
Build as specified; the lever is Chad's dial, offered not applied.

D4 (A-P1-4) Blend-region holes: (a) kneel-form gates fire only at w_kneel > 0.99 and
the deck-form no-regression knee gate goes red on kneel cells — gate EVERY active cell
against the BLENDED reference (|knee − mix(knee_deck_ref, K, w_kneel)| ≤ tol); no cell
ungated. (b) The linear foot mix passes ~(0.19, 0.50, −0.94) — INSIDE the seat prism —
for ~a third of the band: route the blended foot OVER the seat (blend y on a faster
schedule than x/z), or ship the clip only with an explicit judged-screenshot
disposition in the report. Silence is not an option.

D5 (A-P1-5) The "0.731 m ankle swing" figure is STALE — post-(c) the deck foot already
sits at −0.9034 at the kneel band; true deck→kneel swing is **0.405 m** and the
[0.60, 1.00] u-band SURVIVES (est. worst 0.02–0.04 vs 0.06). Fix the §13.2.4 text; do
not widen the u-band chasing the stale number. Monotone: net ΔC ∝ +0.033·w_kneel, no
dip predicted — the LOG_FATAL confirms.

D6 (A-P2-6/7) TWO gain==0 pins exist (test_rider_pose.cpp ~:1658 AND ~:2174) — flip
both, cite the ruling. ws_kneel_h is solved at the rear station; SPEC-4 shortening at
steer cells keeps that h (direction shard-safe, chord opens) — REPORT the realized
flexion at the worst steer cell, don't be surprised by it.

D7 Declared CG_y at full kneel grows to ≈ **0.26–0.27 m** (hips lift 0.236 with the
chord floor, not "~0.17") — covered by the standing ruling; print the number.

---

## 14. R3-WS(d) — WHAT SHIPPED, THE FOUR PLACES IT DEVIATES FROM §13.4, AND THE
##     ONE THING MY OWN EYES SAY IS NOT DONE

Built 2026-08-20 to §13 + the binding §13.4 amendments D1–D7. `sim/` untouched,
no Blender, no `.glb` edit. Forward lean and every `rider_fwd_m >= 0` cell stay
bit-identical (0-OFF gated joint by joint, and the neutral smoke shot reads
HEAD-identical to my eye). Debug gate **1262/1266**, the only failures the four
known GI4 `test_sled` names.

### 14.1 The change, and the dials it added

| constant | value | provenance |
|---|---|---|
| `kKneelRuntimeGain` | **0 → 1** | CHAD'S RULING 2026-08-20: "Turn on the kneel! yes :)" |
| `kKneelChordFloorM` | **0.26** | §13.2.1 shard guard = A7's 0.25 floor + 10 mm. It BINDS: the anatomical chord is 0.1965 (155°), so `h_kneel_sit` re-solves **0.2549 → 0.290215** and the flexion opens to **146.7°**. Solved, not typed. |
| `kKneelTrunkMaxRad` | **55°** | §13.2.2, blended by `w_kneel` from `kHeadUpTrunkMaxRad`. It **BINDS** (sweep worst θ_r = 55.00°) — D3 called it "decoration"; on the shipped reach solve it is a real ceiling and SPEC 4's shortening pays the shortfall. |
| `kKneelSLo` / `kKneelSHi` | **0.00 / 1.00** | D1, sized by the load-time band sweep — see 14.2 |
| `kWsKneelSlewAddM` | **0.030** | D1's gate, in the no-regression form — see 14.2 |
| `ws_blend_bend_normal` | new pure fn | the tear fix — see 14.3 |

Solved kneel geometry, printed at every load: hip y +0.9289 (0.2902 above the
seat at station z −0.9048), knee (z −0.5199, y +0.6901) — **91.8 mm above the
pan, i.e. ON it through the measured knee pad** — ankle (z −0.9747, y +0.6785),
sole exactly on the rear rise. Heel lift the guard had to spend at full kneel:
**19.9 mm** (§13.2.1 asks for this number; the seat keeps the KNEE, the heel is
the contact that leaves).

### 14.2 DEVIATION 1 (D1): the band is the lever, but the bound cannot be met — and NOT because of the kneel

D1 is right that the kill band was an ungated pop and right that the band is the
only lever (the pose is a pure function; a slew limiter needs state). Both gates
were built: a continuity-in-`s` measurement on the real pose path at
u ∈ {0.8, 1.0}, stepped by the fraction of `s` one 60 Hz tick of the kernel's
own lean slew can deliver (`stand_slew_ds_per_tick()` = **0.0884**, reading
BOTH `lean_tau_s` and `lean_rate_ms`, neither retyped), plus its ctest twin.

The sweep, measured at load (`SEADS_SLED_WS_BAND=1`), worst joint delta per tick:

| `kKneelSHi` (lo = 0.00) | 0.40 | 0.60 | 0.70 | 0.80 | **1.00** |
|---|---|---|---|---|---|
| worst m/tick | 0.1552 | 0.1057 | 0.0912 | 0.0803 | **0.0643** |
| **KNEEL BRANCH SUPPRESSED** | | | | | **0.0574** |

D1's starting guess [0.20, 0.70] measures **0.1577**; the widest band `s` allows
measures 0.0643. **Neither meets 0.060 — and neither does the pose with no
kneel in it at all.** R3-WS(c), which Chad has already driven, spends **0.0574 m**
of joint travel on one tick of stand slew at (u = 1, s = 0.63): 96 % of SPEC
7.5's budget before the kneel exists. Filed as **OPEN-R3WS-STANDSLEW**.

So an absolute gate here would be red on day one and would be gating the STAND,
not this rung. The shipped gate is the same NO-REGRESSION form A6 gave the legs
and §12.3 gave the knees: **the kneel may add at most `kWsKneelSlewAddM` = 0.030 m**
to the kneel-free floor. Measured addition: **+0.0069 m**. `kWsContinuityBoundM`
was not touched, the absolute number is printed next to its floor at every load,
and the band ships at the value the sweep bottoms out at.

### 14.3 DEVIATION 2 (D4a/D4b): the tear was an INTERPOLATION bug, not a target bug — and the boot clip ships DISCLOSED

**(a) The bend-normal lerp.** With the kneel first turned on, the legs rendered
as a fan of intersecting flat plates — after the chord floor held at 0.26 m
everywhere. The chord was never the whole story. At REST the ankle is 0.687 m
FORWARD of the hip; at the KNEEL it is 0.070 m BEHIND it, so the two bend-plane
normals are near-**antiparallel**, and the wiring blended them with `mix()`.
A lerp between antiparallel unit vectors **passes through zero**: mid-blend the
normal collapses and flips, and `solve_chain`'s `cross(bend_n, dir)` hands the
thigh and shin a bone frame that spins about their own axes. That is exactly the
roll R3-WS filed as "no amount of target arithmetic can fix from the runtime
side" — correct that the TARGETS could not fix it, because the defect was in the
interpolation. Fixed by rotating the POLE about the chain
(`ws_blend_bend_normal`, pure TU, the same mechanism R2c-5 already uses for the
elbow): unit length at every w, exactly `n0` at w = 0, so every deck cell and
all of 0-OFF are untouched. The boot's orientation `slerp` had the same class of
bug one line away — antipodal quaternions taking the LONG arc — and is now
sign-corrected.

**(b) The boot through the seat, and why it is disclosed rather than routed.**
D4(b) predicted the linear foot mix crosses the seat. It does: measured worst
penetration **0.1298 m at w_kneel 0.830**. Three routes were measured on the
shipped bake against both continuity gates (bound 0.060; the s figure is the
kneel's own addition, allowance 0.030):

| route | cont-in-a | kneel add, in-s | penetration |
|---|---|---|---|
| **straight mix (SHIPPED)** | 0.0245 | 0.0069 | 0.1298 |
| lead the vertical (band 0.35) | **0.4357 FAIL** | — | — |
| lag the lateral (band 0.45) | 0.0242 | 0.0190 | 0.1299 |
| lead y+z, lag x (band 0.55) | 0.0262 | **0.0672 FAIL** | — |

Leading the vertical fails by 7× for the §12.2 reason (the gate is a RATIO;
lifting the foot early triples the travel AND spends the CG it is measured
against). Leading y and z together fails the stand-slew gate instead. And
lagging the lateral — the one nearly-free route — **does not move the number at
all** (0.1298 → 0.1299), because the worst cell is a LATERAL one: at
`lean_lat_m = ±0.15` the inboard boot is inside the seat's x span for the whole
blend whatever the schedule does. It is also arithmetically unwinnable: the
kneeling sole RESTS on the seat (y 0.5997 = `seat_top_y(−0.9747)` exactly) while
the rear rise CRESTS 43 mm above that on the way, so no monotone path stays
clear. **Disposition: the clip ships, disclosed, judged in
`r3ws_d_shot_3_kneel_full_rear34.png`.** It is not silent.

**(c) Blend-region gates.** Every active cell is now gated, and the deck-form
no-regression knee gate is SKIPPED on kneel/blend cells — a category fix, not a
softening: on the deck "inboard" means into the seat, and the kneel's whole job
is to put the knee inboard and ON the seat. It measured a legitimate 0.025 m as
a violation. Those cells are covered instead by a new ctest case gating the knee
against the blended reference `mix(deck-branch knee, K, w_kneel)` (worst ≤ 0.12,
non-vacuity asserted on both ends of the blend) and by the absolute chord floor,
which is checked on ALL of them.

### 14.4 DEVIATION 3 (D5): the u-band survived, and the deck slide no longer completes at low stand

D5 was right — the 0.731 m figure was stale, the true deck→kneel ankle swing is
0.405 m and `[0.60, 1.00]` holds (bake continuity-in-a at s = 0: **0.0245**).
The stale text is fixed in the header. But a consequence D5 did not name: with
the kneel live, the realized DECK foot travel at low stand is **0.63 m of the
1.0054 m runner**, because the kneel captures the foot at u = 0.60 before the
slide finishes. The full 1.0054 m is still realized at s = 1 (where the kneel is
dead). That is a visible change to what Chad signed in (c) — flagged for his eye,
not hidden in a gate.

### 14.5 The measured result

| quantity | R3-WS(c) | **R3-WS(d)** |
|---|---|---|
| `kKneelRuntimeGain` | 0 | **1** |
| kneel hip height above the seat | 0.2549 (155°) | **0.290215 (146.7°)** |
| worst θ_r | 38.00° = the cap | **55.00° = the kneel cap** |
| trunk from vertical, worst | 66.62° | **83.62°** ← THE HEADLINE |
| realized flexion, rest / worst steer | — | **142.7° / 146.7°** (limit 155°) |
| min hip→ankle chord, all cells | 0.3762 | **0.2600** (floor 0.26, shard 0.197) |
| \|solved knee − K\| / \|K − seat\| | — | **1.52 mm / 8.72 mm** |
| bake continuity-in-a by slice | .0242/.0264/.0287/.0389/.0276 | **.0245/.0264/.0287/.0389/.0276** |
| runtime sweep continuity-in-a | worst 0.0198 | **worst 0.0553** (s = 0, the kneel band) |
| continuity-in-s (NEW) | — | **0.0643, floor 0.0574, kneel adds +0.0069** |
| worst arm ratio, ladder cells | 1.4290 | **1.4289** (HEAD's frozen cell 1.4292) |
| arm no-regression | +0.0349 | **+0.0431** (gate ≤ 0.05) |
| worst leg ratio | 1.1197 | **1.1185** (HEAD's own 1.1246) |
| **C_0(1)** | 0.2183 | **0.2786** ← the kernel rung's number |
| CG_z clamp lie, shipping range | 38.7 mm | **27.4 mm** |
| declared CG_y departure | 144.5 mm | **144.5 mm** (kneel's own share +83.7) |
| seated crest rise | 9.0 mm | **1.2 mm** (gate 10) |

### 14.6 ★ THE ARTEFACT VERDICT — MY EYES, AND THEY SAY NOT DONE

The rung's own law is VERIFY THE ARTEFACT, NOT THE PROCESS, so this is stated
first and plainly: **the gates are green and the kneel still does not read as a
kneel.**

- `_5_u0_neutral` — HEAD-identical to my eye. Seated, hands on bars, head up. ✅
- `_1_kneel_engage` (u ≈ 0.70) — **good**: back bent over, feet well aft on the
  runners, head up, arms reaching. This is the pose Chad asked for. ✅
- `_2_kneel_full` (u = 1, side) — the man is PRONE along the tank (83.62° from
  vertical) with his hips high and behind. That much is D3's expected layover.
  The legs behind him read as a **jumbled, interpenetrating mass of slabs**, not
  as a man sitting back on his heels. ❌
- `_3_kneel_full_rear34` — same verdict from behind, plus the disclosed boot/seat
  clip. ❌
- `_4_stand_cross` (u = 1, s ≈ 0.45, the D1 pop cell) — head up and arms pulling,
  which is right, but the mid-blend legs are the same jumble. ❌

The bend-normal fix (14.3a) removed a large part of the tear and is a real
root-caused bug, so this is materially better than the first render — but it is
not clean, and I am not calling it clean. What is left is the shipped
`sudburian_proxy` at extreme hip flexion: rigid per-bone boxes that
interpenetrate each other and the pelvis when the thigh folds up against a prone
trunk. No target arithmetic reaches that; it is the mesh blocker R3-WS filed,
and it is now the only one left.

### 14.7 ★ THE D3 LEVER — MEASURED, RENDERED, AND OFFERED

D3 asked for the kneel-station lever pre-computed. It is, on the shipped reach
solve, printed at every load (`SLED WS(d) D3 LEVER`):

| kneel station | θ_r | trunk from vertical | hip lift |
|---|---|---|---|
| **0.320 (SHIPPED)** | 49.6° | **78.3°** | 0.205 |
| 0.280 | 43.2° | 71.9° | 0.238 |
| 0.240 | 38.4° | **67.0°** | 0.261 |

(These are the steer-0 closed-form cells; the sweep's worst cell reads 83.62°.)
D3 said the trade is ~10–15 mm of C for ~10–15° of trunk — confirmed: 80 mm of
station buys **11.3°**. `kLadderPelvisAftM` is NOT what moves; the kneel's own
`ws_want_aft` term is.

**And it was RENDERED, because a table is not an eye.**
`r3ws_d_shot_6_lever_offer_station0p24.png` is the same cell with the kneel
station at 0.24 (a probe, REVERTED — the shipped code is 0.320). It reads
**noticeably better**: head up, trunk far less prone, the fold less extreme. It
does not fix the leg interpenetration, but it is the difference between "prone
superman" and "crouched rider". **Recommendation to Chad: pull this lever.** It
is his dial and it is not applied.

### 14.8 OPEN FOR CHAD

1. **THE KNEEL RENDERS BADLY AT FULL AFT** (14.6). His ruling turned it on; my
   eyes say the artefact is not there yet. The remaining cause is proxy-mesh
   interpenetration at extreme hip flexion — a MESH rung, not a runtime one.
2. **The D3 lever** (14.7): 0.320 → 0.24 costs ~10–15 mm of ladder CG and buys
   11.3° of trunk. Offered, rendered, not applied.
3. **`lean_aft_max_m` vs the ladder** has FLIPPED at zero stand: the ladder now
   carries 0.2786 m and the kernel asks for 0.25, so **the full kneel is not
   reachable in play** (the mouse tops out at u ≈ 0.93). Standing still asks for
   48 mm more than it delivers. That is the kernel rung's ruling
   (`docs/KERNEL_WS1_SPEC.md`).
4. **OPEN-R3WS-STANDSLEW** (14.2): the R3-WS(c) ladder already spends 0.0574 m of
   joint travel per tick of stand slew, against SPEC 7.5's 0.060.
5. **The disclosed boot/seat clip** (14.3b) and the shortened deck slide at low
   stand (14.4).
6. OPEN-R3WS-STANDKNEE and OPEN-R3WS-STEERREACH are unchanged and still deferred.

---

## 14. CHAD'S RULING 2026-08-21 — KNEEL LEAVES THE LONGITUDINAL LADDER

"defer the kneeling fit for now, what we will do is not make kneeling for longitudinal
movement… eventually [the] pose activated for a hanging to a side lean… a hotkey so you
can pull a side / unstick it… key is going to be P for pull."

**Full note: `docs/SUDBURIAN_LADDER.md` → "R4-SIDE — THE 'P' KEY (PULL)".**

Binding consequences for THIS spec:
1. `kKneelRuntimeGain` returns to **0 for the fore-aft ladder** — the aft ladder ends at
   the deep seated rear crouch (§11/§12, the (c) pose Chad liked). The kneel branch,
   its constants and its gates STAY IN THE TREE, shard-guarded and tested, reserved for
   the R4-SIDE rung. Flip the two gain pins back with a comment citing THIS ruling —
   they are now "held for R4-SIDE", not "held pending a kneel ruling" (that ruling came).
2. The aft-CG ceiling drops 0.2786 → re-measure (expect ≈0.245 at s=0 — ⚠ **MEASURED
   0.2183**, the expectation was 27 mm high; see §15). **K-WS1's pinned
   C row and `lean_aft_max_m` must be re-sized to the kneel-free bake** — the K-A2
   cross-check test is designed to go red until they are.
3. §13/§13.4 (kneel ON) are SUPERSEDED for the longitudinal ladder but remain the
   authority for the kneel POSE itself (chord floor, trunk cap, s-band, the antiparallel
   bend-normal fix) when R4-SIDE builds on it.
4. The full-kneel mesh interpenetration and the D3 station lever are **both DEFERRED by
   ruling** — do not spend on them; the station may move forward at builder discretion
   when R4-SIDE lands.


---

## 15. R3-WS(e) — WHAT SHIPPED FOR THE §14 RULING (2026-08-21)

Small and surgical, exactly as ruled. **Nothing was deleted, nothing redesigned.**

### 15.1 The change

| | what | where |
|---|---|---|
| the switch | `kKneelRuntimeGain` **1 → 0**, "held for R4-SIDE" | `render/rider_pose.h` |
| the two pins | both flipped to `== 0.0f`, citing THIS ruling | `test/unit/test_rider_pose.cpp` |
| the row | `kAftCeilC1` re-pinned to the kneel-free live bake | `sim/sled.h` |
| the clamp | `lean_aft_max_m` **0.2786 → 0.2183** | `sim/sled.h` |
| non-vacuity | new `ws_test_pose_kneel` drives the kneel cases at gain 1 | `test/unit/test_rider_pose.cpp` |

**The kneel branch, `kKneelChordFloorM` (0.26), `kKneelTrunkMaxRad` (55°), the
`kKneelSLo/SHi` band, `kneel_construct` and `ws_blend_bend_normal` all STAY,
wired and gated.** They are the R4-SIDE inheritance. What changed is the
kneel's *address*, not its correctness.

**The kneel tests stay NON-VACUOUS.** With the gain at 0 they would have gone
green about a pose nothing computes, so the test-side pose helper's last
parameter changed from a *scale on top of* the shipped gain to *the gain
itself*: `ws_test_pose` uses the shipped constant (and therefore re-derives the
kneel-free ladder, which is what makes the K-A2 cross-check bite),
`ws_test_pose_deck` passes 0, and the new `ws_test_pose_kneel` passes 1. The
D1 stand-slew case and the whole D4(a) blend/chord case now run through it.

⚠ **ONE GATE GOT STRICTLY STRONGER BY ACCIDENT AND IT IS WORTH NAMING.** §14.3(c)
had to SKIP the deck no-regression knee gate on kneel and blend cells (a
category fix). With the gain at 0 that skip never fires, so every cell of the
ladder is now covered by the strict deck gate again — and it is green. The skip
is left in place, written against the shipped gain, so it returns automatically
if R4-SIDE ever turns the kneel on for this axis.

### 15.2 The measured consequences

**The new terminal aft pose (the ladder's top, s = 0, u = 1.0):** trunk **48.1°
from vertical** (it was 79–84° with the kneel in), `theta_r` 19.5°, full runner
slide 1.000, CG_z residual **−0.35 mm**. That is a deep seated rear crouch —
butt back on the pan, feet far aft, knees bent, head up, arms pulling. **§14.8
item 1 — "the kneel renders badly at full aft" — is CLOSED by this ruling**, not
by a mesh fix: the cell is no longer in the reachable range. Judged by eye in
`scratch/r3ws_e_shot_1_fullaft_seated.png` and
`scratch/r3ws_e_shot_3_fullaft_rear34.png`; standing full aft
(`_shot_2_stand_fullaft.png`) is unchanged from (c)/(d).

**The bake, re-run in the same commit:**

- C(1) row: **0.2183 / 0.2146 / 0.2070 / 0.1984 / 0.2021**, `monotone 1`,
  `min dC/du 0.00041`.
- demand step **0.0044 → 0.0034 m**; worst pose delta per step by slice
  **0.0211 / 0.0230 / 0.0250 / 0.0340 / 0.0241** against SPEC 7.5's 0.060 —
  every slice IMPROVED, worst now 56.7 % of budget (was 72 %).
- continuity-in-`s`: worst **0.0576** vs the kneel-free floor **0.0574**
  (add **+0.0002**, allowance 0.030). `OPEN-R3WS-STANDSLEW` is unmoved and
  still belongs to the STAND key.
- ⚠ The +0.0002 is no longer "the kneel's contribution" — with the gain at 0 it
  is the s-BAND's effect on the seat/stand weight split, which survives the
  gain because `ws_weights` redistributes before the gain is applied. The gate
  still fires on the right quantity for R4-SIDE; the print's wording is now
  slightly ahead of its meaning and is left alone rather than re-tuned.

**Also measured, and NOT a regression of this rung:** the kneel-free row is not
identical to (c)'s at s = 0.50 / 0.75 (0.2070 / 0.1984 vs 0.2083 / 0.1990).
R3-WS(d) changed more than the gain. Re-baked, not reverted.

### 15.3 What this costs, stated before Chad drives

The fore-aft ladder's seated ceiling is **0.2183 m** of rider-CG travel. That is
**below the flat 0.25 the kernel asked for from the very beginning** — so this
rung REMOVES 31.7 mm of demand that never had a pose behind it, and it ADDS no
authority. The catwalk lever shrinks with it: 0.2183 m of body on 87.5 of 331 kg
is 0.0577 m of system CG on a 1.38 m wheelbase, **4.2 % of the wheelbase**
(K-WS1 measured 5.3 %). K3's question for Chad is unchanged and, if anything,
sharper.

### 15.4 Still open

1. **§14.8 item 2, the D3 station lever** — DEFERRED by the §14 ruling.
2. **The deep-flexion proxy interpenetration** — DEFERRED by the §14 ruling;
   it travels with R4-SIDE, where the kneeling leg reappears.
3. `OPEN-R3WS-STANDSLEW`, `OPEN-R3WS-STANDKNEE`, `OPEN-R3WS-STEERREACH` —
   unchanged.
4. **R4-SIDE itself is a NOTE, not a rung.** `docs/SUDBURIAN_LADDER.md`
   → "R4-SIDE — THE 'P' KEY (PULL)". Two rulings are needed before it starts:
   `P` hold-vs-toggle, and whether the inboard hand may leave the bar.
