#pragma once
// ★ THE TRAILING-CHAIN SOLVER -- the Sudburian's scarf today, SUPERMAN in R4.
//
// docs/SCARF_SPEC.md §3 is the contract; docs/SUDBURIAN_LADDER.md §5 is the
// scarf ("six-segment bone chain, driven entirely render-side, no authored
// keyframes") and §7.6 is the architectural find that makes this file worth
// its own translation unit:
//
//     "A damped trailing chain anchored at a fixed point is exactly the scarf.
//      Superman is that same solver with the anchor moved to the grips and the
//      body as the chain. Build it once in R2 for the scarf, re-use it in R4
//      for the body. Same code, same fixed iteration count, same fixed dt.
//      It is a trailing-chain solver, NOT a ragdoll."
//
// So this file is PURE (glm + std only): no raylib, no clock, no getenv, no
// asset. It is unit-testable headlessly (test/unit/test_trail_chain.cpp) and
// it lives in seads_render_core, which is the layer gate's word for "may not
// see a GPU". It NEVER writes to sim/ -- the caller owns the state, the state
// lives inside SledModel (render static), and no tape can see it (§0.1).
//
// ★ MEASURED out of assets/sled/indy650.glb, 2026-08-19 (stdlib python, TRS
//   composed armature->root->pelvis->spine_01..03->neck_01->scarf_01):
//     scarf_01 rest WORLD pos  (0.000000, 1.130831, -0.371228) m
//     scarf_01 rest WORLD +Y   (0.000000, -0.893377, -0.449307) (down-and-back)
//     scarf_01 rest WORLD +X   (1.000000,  0.000000, -0.000001)
//     scarf_01 rest WORLD +Z   (-0.000001, 0.449306, -0.893375)  = cross(X, Y)
//     scarf_02..06 each +0.080000 m along the parent's +Y, identity rotation
//     the six bones ARE skin joints 8..13 of the 42-joint `sudburian_rig`
//   Those are the numbers test 9b pins, and they are what a future authored
//   cloth will skin onto: frame k of this solver IS the world matrix of bone
//   scarf_0(k+1), +Y down the segment, +X across the ribbon.
//
// ★ THE STEADY-STATE LAW the drag model implements (and the tests gate):
//     tan(lift) = drag_k * v^2 / g          (lift measured FROM the down axis)
//   45 deg at 8 m/s, 81 deg at 20 m/s, 3.6 deg at 2 m/s with the shipped
//   drag_k = 0.153 /m. A massless chain in a uniform wind is straight, so the
//   six-link chain follows the single-particle law to 0.1 deg above ~6 m/s.

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace render {

// Superman is legs + torso. ★ R4a: 16 was head-room until the body chain was
// MEASURED, and the measured table is EXACTLY 16 links (grips -> toe, 2.2009 m,
// render/body_chain.h). A cap the shipped table saturates is a landmine --
// `clamp_segments` clamps SILENTLY, so the next station added would vanish
// without a word. 20 restores the head-room; nothing else depends on the value.
inline constexpr int kTrailChainMaxSegments = 20;
inline constexpr int kScarfSegments = 6;  // SUDBURIAN_LADDER §5

// An anchor that moves further than this in ONE sub-step is a teleport (a
// respawn, a camera cut, a drive-mode arm), not motion: re-prime instead of
// launching the tail at 120 m/s.
inline constexpr float kTrailChainTeleportM = 1.0f;
// The pinned root must never sit INSIDE its own keep-out or every constraint
// pass fights the pin (SCARF_SPEC red-team P2-1). Both keep-outs therefore use
// an EFFECTIVE size derived from where the anchor actually is; the sphere keeps
// this much clear air between the root and its own surface.
inline constexpr float kTrailChainHeadMarginM = 0.02f;

// ★ R4a BODY CHAIN: ONE DRAWN-SURFACE PROBE.
//
// ★★★ THE DECISION THIS RUNG WAS TOLD TO MAKE UP FRONT, AND THE TRAP IT
// AVOIDS. The chain is a CENTRELINE. The scarf could very nearly get away with
// that; the body cannot, and for a reason stronger than thickness: THE DRAWN
// LEGS ARE NOT ON THE CENTRELINE AT ALL. They straddle the machine at
// |x| ~ 0.27-0.30 m while the seat is 0.206 m half-wide, so a midline particle
// is INSIDE the seat box at exactly the stations where the drawn rider is
// clear of it. Grading the centreline would therefore be wrong in BOTH
// directions at once -- it would stop a leg that is nowhere near the seat, and
// it would pass a thigh sitting half inside the pan. That is `back_clr` and
// `surf_clr` for the third time (handoff §7.3, §11.6): a number that cannot
// fail because it grades the wrong thing.
//
// So each station carries TWO probes, one per limb, MEASURED off the shipped
// GLB's own skinned vertices by
// assets/character/sudburian_src/measure_body_chain.py and baked in
// render/body_chain.h.
//
// ★ AND IT IS A BOX, NOT A BALL, and that is measured too. The first cut used
// one radius -- the MAX over the limb's vertices -- and a single radius takes
// the WIDEST half-extent in every direction at once: the shoulder station's
// 0.288 m is the half-SPAN across the shoulders, and a ball of that radius
// holds a man's chest 288 mm above a seat his back is 150 mm thick. So the
// probe is the limb's own oriented bounding box in the station's frame:
// centre (u, v, w) and half-extents (hx, hy, hz) along the frame's +X / +Y
// (down the chain) / +Z. The support in any direction is then
// hx|X.n| + hy|Y.n| + hz|Z.n| -- exact along the axes, conservative at the
// corners, and still one analytic projection per face.
//
// The extents are MAXIMA, not percentiles: a keep-out that lets the drawn
// surface into the machine is the defect this rung exists to prevent, and a
// limb floating slightly proud is the safe failure.
//
// ⚠ AN ALL-ZERO EXTENT DISABLES THAT PROBE, and a chain with
// n_probe_stations = 0 grades the centreline exactly as before (the scarf:
// bit-identical).
struct TrailChainProbe {
    float u_m = 0.0f;   // box centre, along the station frame's +X
    float v_m = 0.0f;   // ... +Y, i.e. down the chain
    float w_m = 0.0f;   // ... +Z
    float hx_m = 0.0f;  // half-extents in the same frame; all zero disables
    float hy_m = 0.0f;
    float hz_m = 0.0f;
};

struct TrailChainParams {
    int segments = kScarfSegments;
    // MEASURED: scarf_02..06 sit at local +Y 0.080000 m. sled_model overwrites
    // this with the mean it reads out of the GLB at bind, so the solver never
    // carries a retyped asset number.
    float seg_len_m = 0.080f;
    // ★ R4a BODY CHAIN: A NON-UNIFORM SEGMENT TABLE. The scarf's links are all
    // 0.080 m; a BODY's are not, and the table is not free to be uniform
    // either -- every link is bounded by the 0.218 m seat pan (the vertex
    // keep-out, see TrailChainInput's seat block) while the anatomy sets where
    // the joints are. The measured body is 16 links from 0.053 to 0.209 m.
    // n_seg_len = 0 uses `seg_len_m` for every link and is BIT-IDENTICAL to
    // the pre-rung solver; link i (1-based, p[i-1] -> p[i]) otherwise takes
    // seg_len_tbl[i - 1].
    int n_seg_len = 0;
    float seg_len_tbl[kTrailChainMaxSegments]{};
    // ★ R4a BODY CHAIN: the drawn-surface probes, station i for particle p[i].
    // 0 = grade the centreline (the scarf, unchanged). See TrailChainProbe.
    int n_probe_stations = 0;
    TrailChainProbe probe[kTrailChainMaxSegments + 1][2]{};
    // ★★★ PER-LIMB GRADING -- RULED BY CHAD 2026-08-28, after the drive that
    // found the legs "going all over erratically".
    //
    // The two probes on a station are not always two halves of one body. On the
    // pelvis and the spine they are; on the LEG stations they are TWO LIMBS
    // THAT STRADDLE THE SEAT, one either side of a solid 0.206 m half-wide.
    //
    // The combined rule scores a face as the DEEPEST push any inside probe
    // needs through it and gives the station the cheapest face. For a straddled
    // station that is a veto: the left limb's cheap exit is -X, the right
    // limb's is +X, and each prices the OTHER'S exit at the full width of the
    // seat. So the only face left cheap is UP, and a leg 20 mm inside the pan
    // gets ejected 0.43 m over it -- which is the metre-scale thrash the spring
    // then fights, every substep, forever.
    //
    // Per-limb: each limb picks ITS OWN cheapest face. The station -- one
    // midline particle -- can only take the COMMON MODE of the two pushes, and
    // that is the honest half of the answer: a translation cannot move two
    // limbs in opposite directions. The DIFFERENTIAL half belongs to the
    // per-side reconstruction (render/body_blend.cpp), which is the only place
    // the two limbs exist as separate geometry. On a symmetric straddle the
    // common mode is ~0, which is exactly right -- the bone between his legs
    // was never the thing inside the seat.
    //
    // ⚠ SET PER STATION BY THE ASSET, never inferred here: whether two probes
    // are two limbs or two halves is a fact about the body, and this file knows
    // no body. All false = the combined rule everywhere, BIT-IDENTICAL to the
    // pre-ruling solver (the scarf never sets a probe at all).
    bool probe_per_limb[kTrailChainMaxSegments + 1]{};
    // ★★★ THE POSE ALLOWANCE -- how deep, per face, THIS STATION'S POSE
    // ALREADY SITS in the seat. See seat_escape_station for why it exists: he
    // is SITTING in the seat, so the seated pose is not a violation to be
    // resolved, and a spring aimed at it must not be something the constraint
    // pass fights. Faces are ordered {up, down, +x, -x, +z, -z}.
    //
    // The caller owns it, because the pose is the caller's (render/body_drive.*
    // fills it from the pin every frame). 0 = no allowance anywhere, and that
    // is BIT-IDENTICAL to the pre-ruling keep-out.
    int n_seat_allow = 0;
    float seat_allow[kTrailChainMaxSegments + 1][6]{};
    // FIXED sub-step. The caller supplies the SAME value every call (the sim
    // dt); render reads no clock (CLAUDE.md house law).
    float dt_s = 1.0f / 120.0f;
    // FIXED constraint passes per sub-step (§7.6). MEASURED: with a pinned root
    // and child-only projection ONE root->tail pass already satisfies every
    // distance constraint exactly (1 vs 30 passes: bit-identical trajectories);
    // the extra passes only arbitrate the keep-outs against the lengths. 4 is
    // plenty and still constant work.
    int iterations = 4;
    float gravity_mps2 = 9.81f;
    // Aerodynamics: dv = w_rel * min(1, drag_k * |w_rel| * dt) -- quadratic
    // drag on a light ribbon, written as a velocity relaxation TOWARD the
    // relative wind so it is unconditionally stable for any |w| and any dt (the
    // min() can never overshoot past the wind). Steady hang angle from the down
    // direction: tan(theta) = drag_k * v^2 / g.
    float drag_k_per_m = 0.153f;  // 45 deg at 8.0 m/s; 81 at 20; 3.6 at 2.
    // Verlet velocity retention per sub-step -- the "light damped-spring lag
    // per segment" of §5. MEASURED on the 6-link prototype (speed step from
    // hang, settle = last time outside +-2 deg of final): 0.990 left a released
    // chain swinging ~20 s; 0.975 -> 4.2/3.8/1.9 s at 4/8/20 m/s, peaks
    // 36/100/162 deg; 0.960 -> ~2.7/2.6/1.6 s, peaks ~24/79/154 deg (SHIPPED: a
    // scarf, not a rope, and the whip on a gust still reads); 0.950
    // -> 1.6/2.1/1.4 s. Steady-state lift is unaffected (v = 0 there). ★ CHAD
    // DROVE 0.960 (2026-08-19): "takes too long to settle". RE-MEASURED on the
    // same 6-link rig, pure solver, 40-line harness: 0.960 -> 2.45/2.62/ 1.69 s
    // at 4/8/20 m/s (peaks 22/78/157 deg); 0.940 -> 1.25/1.65/1.35 s; 0.930 ->
    // 0.93/1.40/1.23 s (peaks 17/56/135 deg) SHIPPED -- half the settle, the
    // whip still overshoots 56 deg on an 8 m/s step so a gust still reads;
    // 0.920 -> 0.56/1.16/1.12 s. ⚠★★★ A PER-STEP DAMPING IS MEANINGLESS WITHOUT
    // THE STEP IT WAS MEASURED AT. Every number above is at dt_s = 1/120 s. The
    // R4a superman rig stepped this chain at the KERNEL SUB-STEP h = 0.694 ms
    // -- 12x faster -- with the same per-step multiplier, giving a velocity tau
    // of ~10 ms instead of ~115 ms. It crushed the chain and produced a
    // confident, entirely false 'no superman' that was one message from being
    // reported as a finding. A caller stepping at any other h MUST
    // rate-convert: damping^(h / (1/120)). This is the trap that cost the
    // superman rung the most.
    float damping = 0.930f;
    // Keep-outs. NOT a contact solver (§7.6 bans one): THREE analytic
    // projections applied inside the constraint loop, after the distance pass,
    // every iteration -- the head sphere and the back half-space here, and the
    // R4a SEAT SOLID whose geometry rides in TrailChainInput. The first two
    // DISABLE cleanly for R4 superman, where the chain IS the body and there is
    // nothing to stay out of; the seat is the one that turns ON there.
    float head_keepout_r_m = 0.20f;  // helmet sphere; 0 disables
    float back_keepout_m = 0.06f;    // half-space behind the TORSO plane (§3b);
                                     // a zero back_normal disables
    // ★ R4a SUPERMAN, Chad 2026-08-25: "his legs will hit the seat." The THIRD
    // analytic projection (see TrailChainInput's seat block) keeps the chain
    // out of the machine's own seat solid. This is the clearance held off the
    // measured surface -- i.e. the LEG'S OWN HALF-THICKNESS, since the chain is
    // a centreline. 0 = the centreline rests exactly ON the drawn surface, and
    // is the shipped default because the leg radius is CHAD'S DIAL, not mine:
    // it is not measured off him yet (handoff §3).
    float seat_keepout_m = 0.0f;
    // ===== R4a: THE RETURN-TO-POSE SPRING ===================================
    // ★★★ CHAD DROVE THE BARE CHAIN 2026-08-27 AND RULED IT OUT: "he is
    // flailing all around ... like a firehose unattended." Asked what the body
    // should do instead he ruled **HOLD HIS SHAPE, TRAIL FROM IT** -- the man
    // keeps a recognisable body and the legs trail behind it, and the dial is
    // HOW FAR A JOLT CAN PULL HIM OUT OF IT.
    //
    // That is not a new mechanism, it is the MISSING one. SUDBURIAN_LADDER
    // §7.6 bans a ragdoll precisely because it "produces exactly the floppy,
    // weightless motion §3 and §0.4 exist to prevent" -- and a trailing chain
    // with no angular stiffness is a ragdoll in that respect. A man's body has
    // muscle; it resists leaving its pose. This is that resistance.
    //
    // A per-particle spring toward a TARGET POSE the caller supplies each step
    // (TrailChainInput::pose_p). For the body that target is free:
    // render/sled_model.cpp already evaluates the drawn man's own R3 pose every
    // frame to PIN the chain with, so the spring pulls him back toward exactly
    // the pose he would be in if he were not being thrown.
    //
    // It is a SPRING WITH ITS OWN DAMPING, not a blend toward the target: a
    // positional blend fights the distance pass that runs after it, and a
    // spring does not. CRITICALLY damped at the stated frequency, because the
    // failure it exists to remove is oscillation and a fix that oscillates is
    // not one.
    //
    // ★ ZERO DISABLES, and zero is EXACTLY the pre-rung integrator -- the term
    // multiplies out to (0,0,0) and `v += 0 * dt` is bit-identical, so the
    // shipped scarf (which passes no pose) does not move.
    //
    // ⚠ THE VALUE IS CHAD'S DIAL. What is MEASURED is the pull-out ladder in
    // docs/SESSION_HANDOFF_20260827_r4a_arming.md -- how far a jolt drags the
    // toe off the pose at each candidate stiffness. He drives it.
    float pose_stiff_hz = 0.0f;
    // ★ The FLOOR the anchor-pin may relax back_keepout_m to. The pin exists so
    // a root authored inside the surface is not yanked out and kinked; it must
    // not license the ribbon THROUGH the surface. Set this to the ribbon's own
    // half-thickness and the fabric rests ON the drawn back instead of grazing
    // it centreline-first. 0 keeps the old behaviour (R4 superman: no ribbon).
    float back_keepout_min_m = 0.0f;
};

struct TrailChainState {
    int n = 0;                                  // segments in use
    glm::vec3 p[kTrailChainMaxSegments + 1]{};  // p[0] = anchor
    glm::vec3 p_prev[kTrailChainMaxSegments + 1]{};
    // Last good ribbon width axis. The frames pass writes it, so a chain that
    // streams exactly ALONG the width axis (a hard side slide) keeps the ribbon
    // it had instead of flipping or going NaN (red-team P1-2).
    glm::vec3 last_x{1.0f, 0.0f, 0.0f};
    bool primed = false;
    // ★ Set by every reset, consumed by the next step: a freshly primed chain
    // is SOLVED against the keep-outs and frozen (p_prev = p) instead of
    // integrated. See the comment block in trail_chain_step -- without it a
    // straight-line prime that starts inside the torso half-space is read by
    // the Verlet integrator as tens of m/s and the chain flies over the top.
    bool needs_solve = false;
};

// Per-step inputs, ALL IN ONE FRAME (the caller's -- model space for the sled).
// ★ R2c-7s(g), the context-free consult's P0-1: ONE chord plane cannot fit a
// bent seated torso (the drawn back runs 126->207->150 mm behind the
// neck-pelvis chord over the hang -- measured), so the caller may instead
// supply the DRAWN BACK ITSELF as a fan of segment planes, one per band of
// the torso parameter t. Still analytic projections inside the constraint
// loop -- NOT a contact solver (§7.6's ban stands); n_back_planes = 0 keeps
// the old single-plane path bit-identical (every existing test, and R4's
// disable rule, unchanged).
inline constexpr int kTrailChainMaxBackPlanes = 6;

// Capacity for the seat's measured top-surface profile, NOT a count: the
// shipped table is render/rider_pose.h's kSeatStations = 33 stations over
// z [-1.005000, +0.307615]. The caller passes n_seat_samples; nothing here is
// pinned to the asset.
inline constexpr int kTrailChainMaxSeatSamples = 33;

struct TrailChainInput {
    glm::vec3 anchor{0.0f};  // p[0] this step
    glm::vec3 wind_mps{
        0.0f};  // air velocity relative to the frame (= -velocity)
    glm::vec3 gravity_dir{0.0f, -1.0f,
                          0.0f};  // unit, points DOWN in this frame
    glm::vec3 width_axis{1.0f, 0.0f,
                         0.0f};   // unit, the ribbon's rest width dir
    glm::vec3 head_center{0.0f};  // keep-out sphere centre
    glm::vec3 back_origin{0.0f};  // a point on the torso line (the neck joint)
    glm::vec3 back_normal{0.0f};  // unit, points BACKWARD, PERPENDICULAR TO THE
                                  // POSED TORSO AXIS (§3b). Zero = disabled.
    // -- the banded drawn-back planes (0 = use the single plane above).
    // Plane k covers torso parameter t in [bp_t[k], bp_t[k+1]] (t measured
    // along torso_axis from torso_origin, clamped at the ends); origin is ON
    // the drawn surface, normal points OUTWARD (backward). The keep-out
    // distance applied off each plane is pr.back_keepout_m.
    int n_back_planes = 0;
    glm::vec3 bp_origin[kTrailChainMaxBackPlanes]{};
    glm::vec3 bp_normal[kTrailChainMaxBackPlanes]{};
    float bp_t[kTrailChainMaxBackPlanes + 1]{};
    // ★ SCARF-DRAPE (2026-09-04, Chad's back-lean report): THE COAT IS A
    // SLAB, NOT AN INFINITE WALL. The banded keep-out was a half-space, and
    // at full back lean -- trunk bent forward, back arched UP -- a scarf
    // physically hangs in FRONT of the torso plane, which the half-space
    // forbade everywhere: the chain wadded up above the arch (measured: all
    // six particles bunched in 0.09 of t, s +0.05..+0.18) and read as
    // "going into the coat". bp_lat_half[b] is the coat's own measured
    // half-width in that band (about the torso's lateral axis,
    // cross(torso_axis, back_normal), through back_origin): a probed box
    // laterally OUTSIDE the slab is beside the torso where there is no coat,
    // and is not pushed; one inside leaves by the CHEAPER of the back face
    // and the near side face. 0 = the old half-space, bit-identical -- and
    // the non-probe path never reads it at all.
    float bp_lat_half[kTrailChainMaxBackPlanes]{};
    // ★ And the slab's FRONT face, per band: the coat's chest surface,
    // measured (min s of the band's torso verts vs bp_origin along the band
    // normal -- always negative when live; 0 disables). Without it, a scarf
    // hanging below the neck of a bent-forward torso -- in FRONT of the back
    // plane but nowhere near the coat -- was forbidden everywhere inside the
    // lateral bound, and the chain wadded above the arch. The slab is also
    // bounded in t by [bp_t[0] - band, bp_t[n] + band]: the coat ENDS at the
    // hem and the collar, and a keep-out that follows the chain past the
    // collar forever blocks the one exit gravity actually wants (past the
    // collar, hanging beside the helmet -- the head sphere owns that space).
    float bp_front[kTrailChainMaxBackPlanes]{};
    glm::vec3 torso_origin{0.0f};
    glm::vec3 torso_axis{0.0f, 1.0f, 0.0f};  // unit, pelvis -> neck

    // ===== R4a: THE POSE THE SPRING PULLS BACK TO ============================
    // Target position for particle i, in THIS input's frame. Read only when
    // pr.pose_stiff_hz > 0 AND n_pose > i; 0 disables and is the default (the
    // scarf has no pose to return to). See TrailChainParams::pose_stiff_hz.
    int n_pose = 0;
    glm::vec3 pose_p[kTrailChainMaxSegments + 1]{};

    // ===== R4a: THE NON-INERTIAL FRAME =======================================
    // ★★★ THE FIND THIS RUNG PAYS OFF (Chad, 2026-08-26: "I dont understand why
    // the physics dosent produce it"). He was right and the mechanism story was
    // sloppy. The physics produces superman fine -- the INTEGRATOR was missing
    // terms. This solver runs in the CALLER'S frame, and for the sled that
    // frame is MODEL SPACE: a frame that is itself accelerating and rotating
    // with the machine. Until this rung the predict loop applied only a uniform
    // constant `gravity_dir * gravity_mps2`. The correct field in such a frame
    // is
    //
    //   g_eff = g - a_frame - alpha x r - omega x (omega x r) - 2 omega x v
    //                (uniform)  (Euler)     (centrifugal)        (Coriolis)
    //
    // and NONE OF THE FOUR were present. Never write "the physics can't"; write
    // WHICH TERM THE INTEGRATOR IS MISSING.
    //
    // ★ MEASURED over 31 replayable tapes of Chad's driving, before a line of
    // this was written (`seads_sled_probe superman`, commit fa94c351e):
    //   - ALL FOUR ARE REQUIRED. The three per-particle terms have a median
    //     effect of +0.00..+0.14 deg on nearly every tape and a max to +164
    //     deg: they do NOTHING in ordinary riding and EVERYTHING at the tail.
    //     Tape 11 is the clincher -- >60 deg occupancy 0.00% with the uniform
    //     term alone, 11.82% with all four.
    //   - SUPERMAN IS EMERGENTLY EXTREME, which is Chad's own requirement and
    //     did NOT have to be imposed by a threshold: >60 deg occupancy is 0.00%
    //     on all ten cruise tapes and 2.0-11.8% on the bush tapes.
    //
    // ALL FOUR DEFAULT TO ZERO, and zero is EXACTLY the pre-rung integrator --
    // v += (0,0,0) * dt is bit-identical, so the shipped scarf (which passes
    // none of these) is untouched. Feeding the SCARF this field would be
    // physically right and would change a look Chad has already signed: that is
    // a one-dial drive question, not this rung's business.
    //
    // ⚠ WHERE THESE COME FROM (structural rule, handoff §2.2): a_frame and
    // alpha exist inside the kernel today ONLY in `SledDebugSubstep`, filled
    // under `if (dbg)` with dbg null in the game. DO NOT start passing a debug
    // sink in the shipped game to get them -- that is a tape-visible change.
    // Finite-difference the ALREADY-SHIPPED `velocity` / `angular_vel`
    // render-side instead (see `trail_frame_field` below), which is untapeable
    // by construction.
    //
    // ⚠ FRAME: the kernel's triple is BODY-AXIS; this solver wants the caller's
    // frame. One `model_from_body` rotate at the call site.
    glm::vec3 frame_accel_mps2{0.0f};  // a_frame, the uniform term
    glm::vec3 frame_omega_rps{0.0f};   // omega
    glm::vec3 frame_alpha_rps2{0.0f};  // alpha = d(omega)/dt
    // ★ The point `r` is measured from. For the sled the body origin IS the
    // system CG (sim/sled.h), so the default 0 is right there -- but say it at
    // the call site anyway: anchoring `r` at the wrong point silently zeroes
    // the lever arm the Euler and centrifugal terms act on, and a zero lever is
    // indistinguishable from "the term does nothing".
    glm::vec3 frame_origin{0.0f};

    // ===== R4a: THE SEAT KEEP-OUT ============================================
    // ★ Chad, 2026-08-25: "He can get thrown off in any direction if he
    // supermans, HIS LEGS WILL HIT THE SEAT." Without something to hit, a chain
    // folds straight up over the bars -- several tapes peak at 178-180 deg of
    // lift, i.e. the body vertical and the legs THROUGH the machine. This is
    // what BOUNDS the extreme, so it ships in the same pass as the terms above.
    //
    // ★ SPEC-LEGAL: SUDBURIAN_LADDER §7.6 bans a CONTACT SOLVER, not the
    // analytic projections this loop already runs for the head sphere and the
    // back planes. Same loop, same fixed iteration count, same constant work.
    //
    // ⚠ TWO BOUNDS IT HAS, MEASURED AND WRITTEN DOWN RATHER THAN DISCOVERED ON
    // A DRIVE (test_trail_chain.cpp cases 18 and 19):
    //   1. It is a VERTEX keep-out, like the sphere and the planes. A chain
    //      whose links are LONGER than the solid is thick lies straight through
    //      it with a vertex either side and nothing inside to find. The
    //      measured pan is 0.218 m thick, so the body chain's segment length is
    //      bounded by that -- a constraint on the segment table, not a defect
    //      to close here (closing it means a segment-vs-solid sweep, which is
    //      the contact solver §7.6 bans).
    //   2. A point already inside leaves by its SHALLOWEST face, which for a
    //      chain primed straight down THROUGH the pan is downward -- it sinks
    //      rather than resting on top. Prime the body in a LEGAL pose, which is
    //      what sled_model already does at bind.
    //
    // The seat is a SOLID, not a half-space (the rule R3-WS(d) already
    // established in render/sled_model.cpp): it spans y [y_bottom, top(z)] over
    // |x| <= x_half and z [z_rear, z_front]. `seat_top_y` is the MEASURED
    // centreline profile -- measure_seat_profile.py's downward raycast off the
    // shipped GLB, the same table render/rider_pose.cpp already carries. The
    // caller hands it over; this file knows no asset.
    //
    // n_seat_samples = 0 disables, and is the default: the scarf never sees it.
    int n_seat_samples = 0;
    float seat_z_rear = 0.0f;
    float seat_z_front = 0.0f;  // must be > seat_z_rear, else disabled
    float seat_x_half = 0.0f;
    float seat_y_bottom = 0.0f;
    float seat_top_y[kTrailChainMaxSeatSamples]{};  // sample k at z_rear + k*dz
};

// ===== THE FRAME FIELD SOURCE ==============================================
// ★ How the three frame quantities above are PRODUCED, and the rule is
// structural: NOT by passing a debug sink into the shipped kernel (handoff
// §2.2 rule 2 -- `SledDebugSubstep` is filled only under `if (dbg)` and dbg is
// null in the game; wiring one in would be a tape-visible change to shipped
// behaviour). They are finite-differenced render-side off the ALREADY-SHIPPED
// `velocity` and `angular_vel`, which is untapeable by construction.
//
// Both inputs must ALREADY be expressed in the solver's frame -- for the sled
// that is one `model_from_body` rotate at the call site. The tracker is the
// caller's (render static, beside the chain state); it holds one step of
// history and nothing else.
struct TrailFrameTracker {
    glm::vec3 vel_prev{0.0f};
    glm::vec3 omega_prev{0.0f};
    bool primed = false;
};

struct TrailFrameField {
    glm::vec3 accel_mps2{0.0f};
    glm::vec3 omega_rps{0.0f};
    glm::vec3 alpha_rps2{0.0f};
};

// One backward difference. The FIRST call after a prime returns ZERO accel and
// ZERO alpha (there is no previous sample, and inventing one out of the current
// value is how a respawn becomes a 300 g spike). Same for dt <= 0: a 0-tick
// frame poses, it does not differentiate. Re-prime the tracker wherever the
// chain itself re-primes -- a teleport, a respawn, a drive-mode arm -- for the
// same reason and at the same moment.
TrailFrameField trail_frame_field(TrailFrameTracker& tr,
                                  const glm::vec3& vel_frame,
                                  const glm::vec3& omega_frame, float dt_s);

// Straight chain from `anchor` along `dir` (unit), at rest. Used at first sight
// (sled_model primes at BIND, so a 0-tick first frame never sees an unprimed
// chain) and after a teleport.
void trail_chain_reset(TrailChainState& st, const TrailChainParams& pr,
                       const glm::vec3& anchor, const glm::vec3& dir);

// ONE fixed sub-step. Constant work: no loop bound depends on the data, there
// is no tolerance loop and no early exit. No NaN for any finite input (zero
// wind, zero dt, 1e3 m/s wind, an anchor teleport).
void trail_chain_step(TrailChainState& st, const TrailChainParams& pr,
                      const TrailChainInput& in);

// Bone frames for the chain: frame i (i in [0, n)) has origin p[i], +Y along
// normalize(p[i+1] - p[i]), and its +X/+Z from `width_axis`
// parallel-transported down the chain (Gram-Schmidt against +Y, each step
// seeded by the previous frame's X). Orthonormal, right-handed (Z = cross(X,
// Y)), continuous, always finite. Writes `n` matrices.
//
// ★ Takes the state by NON-CONST ref (the §3 declaration is const): the width
// fallback has to READ AND WRITE `last_x`, and the spec leaves that call to the
// builder ("frames() therefore takes the state by non-const ref, or the caller
// passes last_x in/out -- builder's call, documented"). Documented here.
void trail_chain_frames(TrailChainState& st, const glm::vec3& width_axis,
                        glm::mat4* out);

// ★ SCARF-DRAPE: the anchor depth pod 0 needs. Bone 0's pod hangs off the
// PINNED root the constraint loop never grades; the caller stands the anchor
// off to the returned depth (measured along the anchor's own band normal).
// Shared by both call sites and both chains. Returns pr.back_keepout_m when
// there is nothing to add (no planes, no probe, unprimed chain).
float trail_chain_anchor_need(const TrailChainState& st,
                              const TrailChainParams& pr,
                              const TrailChainInput& in);

// ★ SCARF-DRAPE: re-apply the banded-plane POD projection to a PRESENTATION
// copy of the chain -- for the caller that displaces a copy with the flutter
// wave and then draws it. The wave is deliberately invisible to the solver;
// this keeps it equally unable to put drawn fabric inside the coat. No-op
// (early return) unless the chain has BOTH banded planes and pod probes.
void trail_chain_present_clamp(TrailChainState& st,
                               const TrailChainParams& pr,
                               const TrailChainInput& in);

// The "lift angle" the tests gate: angle (rad) between the chord (p[n] - p[0])
// and the DOWN direction. 0 = hanging, pi/2 = streaming level.
float trail_chain_lift_rad(const TrailChainState& st,
                           const glm::vec3& gravity_dir);

// ★★★ THE SEAT SOLID, EXPOSED -- R4a rung 2 (the drawn legs).
//
// Returns the push that takes a BOX (centre `p`, half-extents `half`) out of
// the seat solid these params+input describe, or (0,0,0) if it is already
// outside. It is the SAME `effective_seat` + face-scoring the chain's own
// keep-out runs, called through one wrapper, because the alternative is a
// second seat model -- and this program's most expensive recurring defect is a
// second copy of a measured thing.
//
// The blend needs it because a lerp between two individually-legal points
// crosses NON-CONVEX free space: the seated ankle is on the running board and
// the trailing ankle is aft and high, and the straight line between them cuts
// the corner THROUGH the seat and tunnel. The chain's own keep-out cannot see
// that -- it grades the CHAIN, and the blended drawn leg is a third geometry.
//
// PURE: no state, no solver, safe to call on any point at any time.
glm::vec3 trail_seat_escape(const TrailChainParams& pr,
                            const TrailChainInput& in, const glm::vec3& p,
                            const glm::vec3& half);

// ★★★ THE STATION PUSH, EXPOSED -- the whole of the ruling above in one
// gradable call. `q` / `r` are the two probes' box centres and half-extents in
// world (already carried through the station frame); `live` says which of them
// draws anything. `per_limb` selects the ruling.
//
// This is the SAME expression the constraint pass runs, called through one
// wrapper -- not a description of it. The alternative is a second keep-out
// model, and a second model of one thing is this program's most expensive
// recurring defect.
// Per-face penetration depth of a BOX (centre `p`, world half-extents `half`)
// into the seat solid. Positive on EVERY face means the box is inside, and the
// value on face f is how far it would have to move to leave through that face.
// Faces are ordered {up, down, +x, -x, +z, -z} -- the order
// TrailChainParams::seat_allow uses, because it is filled from this.
//
// Returns false (and leaves `d6` untouched) when the seat is disabled.
bool trail_seat_depths(const TrailChainParams& pr, const TrailChainInput& in,
                       const glm::vec3& p, const glm::vec3& half, float* d6);

glm::vec3 trail_seat_station_push(const TrailChainParams& pr,
                                  const TrailChainInput& in,
                                  const glm::vec3 q[2], const glm::vec3 r[2],
                                  const bool live[2], bool per_limb);

}  // namespace render
