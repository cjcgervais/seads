# KERNEL v5 — the small-adjustment wobble round (feel/kernel-v5)

Director: Fable (math analysis, review, gate, commit). Writers: Opus (instrument),
Sonnet (mechanism to explicit spec). No writer commits; no writer touches goldens.
Baseline: main @ d68de7d91 (v4 approved). Branch: feel/kernel-v5, worktree seads-feel.

## Chad's report (2026-07-23, flying main)
Small, smooth mouse adjustments (sustained slow yaw; careful aim pulls) make the
rudder/elevator flap and the nose OVERSHOOT the aim circle on the far side by a
little, then bounce — "at those small deflections it is treating them like they
are big deflections." Requirements: keep the universal magnetic arrival and the
one dead blow to the opposite side of the ring **"(or near it)"**; at very slow
adjustments, NO wobble. carry=0 remains the kill switch.

## Measured attribution (this sandbox, 2026-07-23)
1. `track <axis> sine f V` (10 deg amplitude): carry=1 fires the capture 8–28x/20 s
   (once per turnaround, worse with freq) while the carry=0 twin's mean/peak error
   is near-identical — on a moving aim the events buy nothing and each is a
   full-authority surface transient (the flapping).
2. **`track <axis> reversal 3..6 V`: `kick_dps` = 14.2–16.9 with carry=1 vs 0.00
   with carry=0.** After a slow smooth reversal, CARRY captures the residual
   closing rate and actively drives the nose the OLD way toward the rim. The
   machinery-only artifact, cleanly separated.
3. Static small steps (1–2 deg) mostly slip under the wall-glance; 3–5 deg steps
   show the designed ~0.5 deg rim bounce. The felt "small adjustments bounce"
   is the mid-track engage (2), not the parked-aim step.
4. ROOT CAUSE (code): `rim_frac * circle` is a FIXED apex target (0.55 deg). The
   v3 rim-targeted servo law (CARRY brake surface + RETURN outbound leg) lands the
   apex ON the wall regardless of the event's momentum — a 3 deg/s arrival whose
   honest full-brake stopping depth is ~0.01 deg is still driven 0.55 deg past the
   (moving) aim, then dead-blown back. Size-free target == size-free overshoot.

## MEASURED REFINEMENT (nudge instrument, 2026-07-23): TWO sub-mechanisms
The nudge probe (repeated 1-5 deg smoothstep eases, 0.7 s move + 1.5 s rest) at
V250 splits the symptom:
- carry=0 twin: flips exactly 11 (one per ease — the motion itself), apex past
  aim 0.000 at EVERY amplitude, rest_settle ~0.03 deg.
- carry=1: every event takes the DIRECT-SEEK path (RETURN, w_rel ~10 dps at
  engage), apex 0.054-0.107 deg PAST the aim, and flips 25-92 — 3-5 extra
  settle lobes per nudge AT REST (the elevator bounce, on an instrument).
- The CARRY fixed-rim path fires on slow smooth REVERSALS (the reversal probe's
  kick_dps 14-17 vs 0.00) — the rudder's wrong-way flap.
So kernel v5 = TWO rungs, each with its own dial + kill value, measured
independently on the reversal/nudge/step grids:

RUNG A — S-truedepth (the ONE mechanism this round): momentum-earned glance
depth (below). CONFIRMED as the felt symptom: nudge amp 5 at V100 routes into
CARRY and lands apex 0.543 deg = 0.987 rim off a 5.2 deg/s closing rate whose
honest stopping depth is ~0.01 deg — a manufactured wall-deep bounce on a small
adjustment, visible ("overshoots the aim circle on the other side"). The
reversal kick is the same path (cap_far_rim 0.63-1.27).

RUNG B — STRUCK (director): the DIRECT-SEEK release-rate lobes measure
apex 0.05-0.11 deg (sub-pixel at 935 px/rad — likely not the felt symptom),
and controller.cpp's completion banner RECORDS that a rate-window completion
(err AND |w| small) was already tried and measured as a 5-tick limit cycle
with 20 reversal flips. Do NOT re-attempt that shape. Watch item only: if
Chad's fly still shows elevator tick at rest AFTER rung A, re-attribute fresh
— do not reach for the rate window.

## RUNG A — S-truedepth (one dial family, one mechanism)
The glance depth an event EARNS is its own stopping distance under full trackable
braking, never deeper than the wall:

    glance   = w_ev^2 / (2 * alpha_u)          # already computed at ENGAGE (H1)
    rim_live = min(rim_frac * circle, depth_frac * glance)

- `depth_frac` NEW dial in [capture], SHIPPED 1.5 (the sweep: 1.0 sagged the
  committed 45 deg flick to 0.60-0.64 rim, violating the flown Q2 wall; 1.5
  restores 0.94 rim at V140 while the symptom case stays 0.42 rim). Loader
  asserts FINITE only — depth_frac <= 0 is the LOAD-BEARING v4 fixed-rim OFF
  arm (the fly fallback); do NOT "fix" the loader to reject it.
- Fast arrivals (glance >= rim): rim_live == rim_frac*circle — v4 bit-behavior,
  the wall glance and uncapped dead blow preserved.
- Slow adjustments: apex = the momentum's own depth — the bounce shrinks
  CONTINUOUSLY to nothing as w_ev -> 0. The magnetic sqrt approach, DIRECT-SEEK
  classifier, dead-blow arrest, refractory, yank/hand-back exits all unchanged.
- Universality preserved: no aim_moved read, no size gate, fires always —
  honoring the v4 "always, even mid-track" ruling AND the new no-wobble ask.

### Implementation sites (controller.cpp; the event stores its own reference)
- NEW `Internal` field `cap_rim_t` (the event's apex target, rad), set at ENGAGE
  in BOTH branches (CARRY: rim_live above; DIRECT-SEEK: keep current semantics),
  zeroed by reset(). Every site that today reads `cp.capture_rim_frac *
  cp.capture_circle` INSIDE an event (wall-reachability clamp w_wall, the CARRY
  predictive brake surface, RETURN's outbound rim-targeted alpha_req, d_allow
  composition) reads `ns.cap_rim_t` instead. ENGAGE-time capture (like cap_err0/
  cap_d_allow) — never recomputed from live w mid-event (the reference must not
  chase the decaying rate).
- w_wall = sqrt(2*alpha_u*rim_live): for a momentum-earned rim this is >= w_ev by
  construction at depth_frac >= 1 — the clamp binds only at the wall cap. Verify.
- d_allow = max(rim_live + glance, dlen) at the CARRY branch (was rim + glance).
  Yank/handback exits then scale with the event honestly.
- kCapAxisMix / ENGAGE ratio / w_eps / glance_frac / refractory: UNTOUCHED.

### MEASURED AFTER GRID (2026-07-23, depth 1.5, honest binary — reconciles the
### predictions below; red-team verdict SOUND-WITH-FIXES, folds applied)
- nudge pitch V100 amp5 (THE symptom): apex 0.543 -> 0.229 deg (0.99 -> 0.42 rim).
- nudge amp3 V140/V250: apex 0.087/0.071 deg (~1.3 px — sub-visible), unchanged.
- step pitch 5 deg V140: rebound 0.52 -> 0.18 deg. Steps 20/30/45 V140:
  0.47/0.41/0.49 vs v4 baseline 0.47/0.43/0.51 — the flown wall preserved there.
- reversal kick (pitch/yaw V250 @4 deg/s): 14-17 -> 7.1/3.4 dps (HALVED, not
  killed — the few-tick CARRY hold until the hand-back abandon; carry=0 twin is
  0.00, still 100% machine. FLY-CARD item: slow smooth reversals.)
- ** NAMED TRADE (red-team P1): the wall is now V-DEPENDENT.** 45 deg flick:
  0.94 rim @V140 / 0.92 @V100 / **0.76 @V220** (v4: 0.97-0.98 everywhere).
  Intrinsic to momentum-honesty (alpha ~ V^2 outruns the engage rate; no
  reparameterization escapes it). CHAD RULES on the stick: if the high-speed
  glance must be wall-deep, the dial is depth_frac up (costs small-adjustment
  bounce back: 0.229 @1.5 / 0.308 @2.0 / 0.464 @3.0 deg on the symptom case).
  After his ruling, add the V220 45-deg row as an executable regression leg.
- T5 yaw leg added (eased 12 deg @V80 lateral arrival, CARRY, depth arm binds
  at 0.39 rim) — the "every axis" trap closed executably; yaw nudge/reversal
  probed healthy (apex 0.05-0.07 deg, kick 3.4).
- The v4-era traverse REQUIRE now carries a NAMED-COUPLING banner (passes at
  depth 1.4, fails at 1.3 — a trip reads as premise drift, not breakage).

### Predicted numbers (pre-implementation; superseded by the grid above)
- reversal kick_dps at 3–6 deg/s: 14–17 -> low single digits (the honest arrest).
- sine cap_far_rim gap vs carry=0 twin: ~0 at all freqs; engages may stay (they
  become invisible — apex ~ stopping depth).
- step 20 deg: rim 0.9–1.1 UNCHANGED (the v4 grid is the regression surface).
- step 1–2 deg: overshoot <= the honest stopping depth (< 0.1 deg).

## Writer tasks
1. OPUS — instrument (test/harness only): `track` gains an amplitude arg for sine
   (`track axis sine f V ticks gain carry AMP_DEG`, default 10 preserved) and a
   NEW `nudge` pattern: repeated smoothstep eases of AMP deg over T s then REST
   (the real hand: ease 1 deg over 0.7 s, rest 1.5 s, N cycles), reporting
   per-event rows: engage tick, w_ev (deg/s), glance depth (deg), path
   (CARRY/DIRECT), apex-past-aim (deg + rim units), plus the summary counters.
   Existing outputs bit-stable at default args.
2. SONNET — mechanism per the sites above, after the instrument lands and the
   BEFORE grid is recorded. One mechanism, no opportunistic edits.
3. FABLE — review every diff, gate, BEFORE/AFTER grids, fresh-context red-team,
   goldens intent-check (a moved golden must show the slow-arrival signature and
   nothing else), commit by explicit paths, inline fly checklist to Chad.

## Watch items (NOT this rung)
- Near-center elevator tick from RETURN's sqrt stiffness at tiny d: harness steps
  look clean (1–2 deg overshoot 0.02–0.08); if Chad's fly still jitters at rest,
  the next dial is a d-floor on the sqrt ceiling — separate rung, his call.
- The engage churn count itself (events firing per turnaround) — invisible if
  depth is honest; only revisit on his stick.

## RUNG A2 — the sub-wall CURVE (Chad's fly-1 verdict, 2026-07-23)
Chad flew rung A: "better, just needs a little more" — small/fine adjustments
still pushed past the circle. Fix: rim_live = rim * s^depth_pow on the
saturation s = min(1, depth_frac*glance/rim). s = 1 is a fixed point at every
pow, so wall-earning committed arrivals are bit-untouched; pow = 2.0 (shipped)
presses sub-wall events quadratically shallower. pow = 1.0 = rung A
bit-identically (the fly fallback; loader rejects pow <= 0 — s^0 = 1 would
weld small events back to the wall).
DESIGN CONSEQUENCE (deliberate): for s < 1/depth_frac the wall-reachability
clamp w_wall = w_ev*sqrt(depth_frac*s) now cuts the hold BELOW the arriving
rate — a small adjustment sheds speed INTO the crossing (the literal "soften
the approach" ask). Rate-continuous entry (leg 9) is now SCOPED to
wall-earning engages; its fixture moved into the wall-earning-yet-unclamped
window [w_wall/sqrt(depth), w_wall] with a cap_rim_t premise REQUIRE.
MEASURED (pow 2 vs pow 1): nudge V100 amp5 apex 0.229 -> 0.102 deg (0.19 rim);
eased yaw 12@V80 0.39 -> 0.23-0.28 rim; steps V140 10/20/30/45: 0.45/0.47/
0.34/0.46 (wall held); yaw amp16 V80 0.71-0.79 rim (committed lateral still
deep); V220 45-deg 0.53 rim (the V-sag deepens with the curve — still Chad's
open ruling, dial pair depth_frac/depth_pow). Reversal kick unchanged 15.7
(separate site, next rung if felt). Gate 364/364, T6 added, pow-ignored
mutant killed, golden bit-identical.

## RUNG C — HOLD THE LINE (Chad's ruling, 2026-07-23 fly-2)
"It should hold the line of my mouse inputs and try to get to my mouse until
full stall — it might sink a bit as I begin the stall but the nose should stay
where my mouse is asking. It's default clamping to nose-down attitude before it
ever should, even as it dives and gains energy; I will crash to the deck before
my elevator and bank/yaw get me around the turn."

MEASURED ATTRIBUTION (ctrl_fly sustained-lateral trace, this sandbox):
1. EARLY SURRENDER — the AoA pushback is PROPORTIONAL: pitch <= K_aoa*(aoa_max
   - alpha_filtered), so it settles with droop = demand/K_aoa. At K_aoa = 5.0
   and demand ~0.36 rad/s the ride pins at 15.7-16.4 deg vs aoa_max 20 (stall
   20.6) — 4.2 deg of lift abandoned, elevator limp at 0.14. The nose sags out
   of the turn with half the wing unspent.
2. DEAD ELEVATOR IN THE SAG — as the nose falls, bank_error unwinds (the aim
   migrates ABOVE the sagging nose; point-at-aim geometry rolls the bank OUT
   65->0 deg), and align = cos(bank_eff)^6 zeroes the pull: measured 12+ s of
   in_pitch = 0.00 at alpha ~10 (HALF margin unused) with the aim 85 deg away.
3. THE WINDMILL — nose ~77 deg below horizon + lateral aim = bank_error
   ill-conditioned; roll demand SATURATED (-260 deg/s, in_roll -1.00) for 12 s
   without converging; align never satisfies; spiral dive, -2800 m / 20 s.

MECHANISMS (in order; measure the probe between each — 3 may not be needed):
C1 — K_aoa 5.0 -> (measured pick, target ride alpha >= ~19 at full pull:
   droop <= ~1 deg needs K_aoa ~ 20-25). Watch: ring near stall through the
   aoa filter (aoa_tau) — step + lathold probes at low V certify; the plant
   stall at 20.6 is the mush Chad accepts ("might sink a bit").
C2 — S-holdline pull FLOOR, sag-gated (the align gate may fade the pull ONLY
   while the nose holds the line): sag = aim_elev - nose_elev (both world
   elevations, local_up-referenced — the repo's re-reference doctrine; both
   already extracted at the push-gate block). floor_gate = smoothstep(0,
   sag_band, sag) gated to nose BELOW the aim's line; align_f = max(align,
   pull_floor * floor_gate); pitch = max(w_push*((1-blend) + blend*align_f),
   w_min_pitch). pull_floor = 0.0 is the bit-identical OFF arm (strict
   superset — golden untouched at 0). Preserves Chad's flown
   bank_align_power = 6 no-climb roll-in EXACTLY when there is no sag (a
   level/climbing flick has sag <= 0 -> floor_gate 0 -> legacy align). Ship
   pull_floor ~0.5, sag_band ~ (measured pick) — the elevator always fights
   gravity's theft of the line, aligned or not.
C3 — ONLY IF the windmill survives C1+C2 on the probe: hysteretic roll-rate
   relief near the nose-vertical degeneracy. Do NOT build preemptively — C2's
   floor pitches the nose back toward the horizon, which re-conditions
   bank_error and should starve the windmill.

INSTRUMENT (Opus, before any mechanism): harness mode `lathold V offset_deg
ticks` — WORLD-FIXED aim captured offset_deg left of the initial heading AT
THE HORIZON (elevation 0), parallel-transported per tick (aim_moved only on
tick 0). Columns each ~0.5 s: t, V, alt, alpha, phi, nose_elev_deg, err,
in_pitch, in_roll. Summary: t_capture (first err < 5 deg), min_nose_elev_deg,
alt_loss_m, alpha_ride_p95, windmill_ticks (longest |in_roll| > 0.95 run),
crashed. THE rung-C regression surface; BEFORE grid at V 100/140/200/250 x
offset 60/90/120.

## RUNG C — LANDED (2026-07-23, gate 370/370, golden deliberately re-recorded)
C2 = S-holdline sag servo (4 design iterations, each measured):
  C2a align-floor REJECTED (fraction of w_push slingshotted over the top,
  n = -2.7 in AT-12); C2b proportional sqrt_law servo (slingshot dead,
  n_min_sus 5.28) but UNSCOPED (stomped the near-astern latch sign, fired in
  FINE); C2c blend-composed + fwd_gate (astern green) but still out-muscled
  the flown pursuit_step shaping on aligned approaches (re-seeded the jink
  test's bank_error ~ pi knife-edge); C2d = FINAL: min(blend * fwd_gate *
  pull_floor * sqrt_law(sag, K_theta, aB_pitch, w_max_pitch), w_push) —
  bit-inert for aligned approaches, recovers only what align faded, never
  above the budget. pull_floor 1.0 shipped, 0 = OFF fly fallback.
C1 = K_aoa 5 -> 10 (ladder-measured: 15 tripped AT-11/AT-8/AT-12 envelope
  bounds + hot-arrival blow-through 1.4 deg; 10 rides alpha p95 15.8 -> 17.7
  with the low-V capture rim restored 0.84 -> 0.97 and every envelope leg
  green; the hotter 15 is a Chad-ruled step AFTER the fly).
C3 windmill relief: NOT BUILT — windmill_ticks unchanged (29-72) but bounded;
  re-attribute after Chad flies C1+C2 (the sag never gets deep enough on the
  lathold grid to trigger the ctrl_fly-class spiral anymore; the fleeing-aim
  sustained case is the fly's judgment).
Test re-derivations (all banner-documented): yaw-band wall floor 0.85 -> 0.75
  (named coupling: the servo lifts the banked-lateral approach, wall 0.80 at
  pull_floor 1); AT-12 anti-glide re-shaped DURATION-wise (soft-tick fraction
  < 5% + hard unload floor 0.5 + n_mean > 4.0 — the bare 1.5 floor was welded
  to the pre-rung breathing amplitude; measured beat: 26 ticks at n 1.00 in a
  6.5g-mean turn).
FINAL GRID (K_aoa 10, pull_floor 1.0 vs pre-rung): lathold t_capture -11..13%
  (V100/90: 3.57 -> 3.19 s), alpha_p95 +2 deg (15.8 -> 17.7), alt_loss -3..-6%,
  min_nose_elev ~ -12 (sag persists but shallower; the spiral-dive degeneracy
  no longer reachable on the grid); 20/45-deg walls 0.47/0.47 (unchanged);
  A2 small-adjustment apex 0.102 (untouched); 40 dps chase lag 1575 -> 1278 ms
  (the residual is the LIFT ceiling — physics, more only via airframe dials);
  reversal kick 15.7 unchanged (separate site, open watch item).

## RUNG C — red-team round (fresh context, on f523177e9): SOUND-WITH-FIXES
No P0/P1. Verified by probe: inverted-sign inertness (the C2d w_push min
carries the body-frame sign — a wrong-way floor is unrepresentable), push-gate
non-starvation, sag~0 continuity, AT-9 purity, the golden signature BOTH
directions (K_aoa alone reproduces old t300; servo alone reproduces new t300),
and the two bound re-derivations hold at the walk-back dials (not welded).
FOLDED: P2-1 fwd_gate was wholly unpinned (mutant fwd_gate:=1 passed all 370;
TC5's banner was false — the astern sign is owned by the min) -> TC5 tightened
to the floor-off-twin equality + NEW TC7 (positive-w_push near-astern window,
the unique fwd_gate discriminator, mutation-verified sole red of 370); P3-1
TC3 theater deleted; P3-2 TC1 upper bound (servo never exceeds its own law).
WATCH ITEMS (not folded): P3-3 lathold windmill_ticks conflates the benign
roll-in (~29 ticks) with a true windmill — add a start-tick / post-alignment
filter next instrument pass; P3-5 a one-tick inverted pin (cosPhiTheta < 0,
w_push < 0 -> bit-inert) would close that class executably; P3-4 FLY CARD:
at V70/amp20 raw alpha transiently grazes 20.86 (past the 20.6 stall) for a
few ticks before decaying — the honest mush edge, Chad judges.

## RUNG D — THE ARCADE ENERGY MODEL (Chad's ruling 2026-07-23: "give me the
## power — I had been intuiting all along that the airframe is being
## underserved"; supersedes his own 2026-07-08 T_max 9000 envelope ruling)
Attribution at his reported case (148 m/s full-left dive + massive turn bleed
+ slow climb): k_induced 0.05 drove hard-turn Cd to 0.187 = 36.7 kN drag vs a
9 kN engine (T/W 0.31) — the turn out-spent thrust 4:1 (~9 m/s^2 bleed,
measured 2609 m altitude paid in a 20 s sustained turn = the ground-seeking);
[g_limits] n_max 16 capped pitch ~57 dps at 148 and binds harder with V (his
founding wing-rip objection, confirmed); the C2 servo's forward fade
(0.3, 0.7) faded exactly over his err 90-130 deg case (director-owned scope
error). DIALS: k_induced 0.015, T_max 18000 (T/W 0.61), n_max 32, D4 fade
edges (0.85, 0.95) — full servo to ~148 deg off-nose, only true-astern ceded.
MEASURED: 10 s max-rate turn retention 108.7% -> 137.8%; lathold V148/90 alt
loss 142 -> 106 m, sag -13.1 -> -11.5 (grid min_elev now -10.9..-13.1 vs the
pre-C -12..-25); 40 dps chase lag 1278 -> 1001 ms (residual = lift, honest);
climb arithmetic ~20 -> ~53 m/s; A2 small-adjustment apex actually improved
(nudge V100 amp5 apex ~0.0, sub-rim); service ceiling rises to ~9.5 km
(taper_sigma is the dial if the world needs 8 back). Six envelope
anchors/oracles re-derived config-relative with banners (AT-6's oracle was
plain-linear and stale since seek_law; AT-17's exit-velocity leg measured
non-monotonic at 70 s — 0.9x taken with the measurement documented; the 45
deg traverse's 2nd "reversal" characterized as a 0.98 dps wall-local wobble
at e 0.04 deg post-IDLE, never re-crossing — revs <= 2 with numbers). BOTH
goldens deliberately re-recorded (plant golden = dials only, thrust-driven
divergence; controller golden = dials + D4). Gate 370/370. NO dedicated
fresh-context red-team this rung (dials + parameter change inside the
already-red-teamed servo + banner-documented re-derivations, each measured)
— honest ledger; the next kernel round should sweep it in.
KNOWN OPEN: reversal kick ~16 dps (the CARRY hold pre-abandon — unchanged
through A/C/D, next rung if felt); windmill metric conflates roll-in (P3-3);
inverted one-tick pin (P3-5); "bouncy at low deflection / inconsistent" =
re-judge on THIS build (energy-state-dependence removed).

## RUNG E — THE KNIFE EDGE + THE RED ARROW (Chad's fly-4 ruling, 2026-07-23)
His report: past a certain deflection it still noses down (power now present,
so not energy); "I think it's the knife edge set too high on the horizon...
nose-down with the bank over should need my down input too, past like 45
degrees"; plus the off-screen aim suspicion + the red-arrow ask. HE WAS RIGHT:
[push_gate] horizon_enter was 1.0 DEG below horizon — and the side cone that
confines push to straight-down aims DEGENERATES near-astern (the vertical
plane contains the astern direction), so big deflections whose aim drifted a
hair low were push-eligible precisely when the bank was far over (bank_hi
120). A long off-screen lateral drag also genuinely curves below the horizon
(the aim moves on the aim-frame arc, not the horizon) — invisibly.
DIALS: horizon_enter 1.0 -> 45.0, horizon_exit -1.5 -> 40.0 (5-deg band).
RENDER: off-screen aim ARROW (red, screen-edge, points at the true aim via
the shared camera_screen_basis — a pure display ADDITION; the reticle stays
raw per the S-retclamp lesson). No test surface (no ctest runs the app);
visual confirm on Chad's fly.
FIXTURES: four push-gate tests were welded to the 1-deg edge — re-derived
(closed-loop push probe now 55 deg below; the bank-exit fixture's 180-deg
convention proven geometrically impossible at the new depth and moved to 140
with a documented trig argument; the dither fixture re-seeded at depth with
the ripple phase restored; the cascade 6-deg sample -> 50 deg). Bank-collapse
mutant re-verified (8 switches vs 1). Gate 370/370, both goldens untouched.
MEASURED SANITY: latflick 148/70 lateral hold = carve, push == 0 every row;
latflick down=50 = ROLLS THROUGH past inverted (the split-S on real down
input; bankErr 118 < the 120 bank guard so the roll-through owns it);
lathold unchanged (never enters push territory).
