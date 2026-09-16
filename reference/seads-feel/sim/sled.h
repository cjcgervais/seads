#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "sim/rider_grip.h"
#include "world/snowpack.h"

// ★★ THE SLED KERNEL (WINTER_LAW §3, S3). The SECOND sealed kernel in this
// tree, and INV-7 is the whole point of the file boundary:
//
//     sim/sled.* has its own state, its own step, its own goldens and its own
//     seal. It shares world::HeightField (through world::SnowpackField) and the
//     control transport. It touches the sealed AIRCRAFT kernel NOT AT ALL.
//
// So this header includes NOTHING from sim/ -- no sim/step.h, no sim/aero.h, no
// sim/ground.h, no sim/state.h. That exclusion is an acceptance leg
// (`sled_seal_isolation`), not a convention, because the cheapest way to break
// a seal is to reach across it for one convenient helper.
//
// ★ EVERY FEEL IN §3 IS A CONSEQUENCE OF ONE DECISION: forces are applied at
// THREE PATCH POSITIONS on a rigid body, never at the CG. Weight transfer,
// off-angle tilt, the bank-side ski compressing first, and the rollover are all
// moments about patch offsets. Nothing here computes "tilt", detects "stuck",
// or scripts a launch -- and §2.4c.1 ruled that architecture BEFORE this file
// existed precisely so it would not have to be retrofitted into a sealed
// kernel later.
//
// ★ DEPTH IS THE FIXED INPUT (§2.2a, SIGNED by Chad: 0.77 m median bush). When
// the machine wallows, the dials that move are IN THIS FILE. Lowering
// [snowpack] base_m to make the sled plane is FORBIDDEN -- it silently reopens
// a signed ruling by editing a value nobody would think to re-fly.
//
// PURE: glm + std + world/ only. No raylib, no I/O, no clock.

namespace sim {

// Rider inputs. Separate from sim::Inputs on purpose -- a sled has no pitch,
// roll, yaw, flaps or gear, and sharing the struct would be the first thread of
// the seam INV-7 exists to prevent.
struct SledInputs {
    float throttle = 0.0f;  // [0, 1]
    float brake = 0.0f;     // [0, 1] -- track brake
    // Handlebar. Sign follows the house body-frame table (SPEC §7: +Y up, so a
    // positive yaw rate is nose LEFT): steer +1 = LEFT.
    // ★★ RED-TEAM P0-1 (2026-08-11): at HEAD `slip_ang` subtracted `delta`,
    // which made steer=+1 yaw the nose RIGHT -- documented LEFT, and every
    // prior rollover/sweep leg was sign-blind (magnitude-only). Fixed in
    // sled.cpp; pinned by sled_steer_plus_one_yaws_nose_left.
    float steer = 0.0f;  // [-1, 1]
    // ★ THE RIDER (PACKET_B §9d). Normalized commands -- the KERNEL owns the
    // metre conversion and the reach box, so no caller can teleport mass.
    float lean_lat = 0.0f;  // [-1,1] commanded rider lateral, +1 = LEFT (agrees
                            // with the fixed steer +1 = LEFT)
    float lean_fwd = 0.0f;  // [-1,1] commanded fore-aft, +1 = forward
    float stand = 0.0f;     // [-1 tuck .. 0 seated .. +1 standing]
};

// The three contact patches (§3.2). Ordered so a numeric cast indexes the
// per-patch arrays; SkiLeft and SkiRight are adjacent so the asymmetry legs can
// difference them directly.
enum class Patch : int { SkiLeft = 0, SkiRight, Track, kCount };
constexpr int kPatches = static_cast<int>(Patch::kCount);

// ★★★ K-WS1 / K1 -- THE HONEST AFT CEILING, MEASURED, ONE ROW, FIVE SLICES.
//
// PROVENANCE. These are the animation's OWN delivered rider-CG travel at full
// aft demand, C_s(1), per stand slice s = 0, .25, .5, .75, 1 -- read off the
// load-time bake print `SLED R3-WS(c) BAKE C(1) per stand slice` that
// render/sled_model.cpp emits on every start against the SHIPPED asset. They
// are MEASURED on the real pose path, never typed from a spec table, and
// test/unit/test_rider_pose.cpp cross-checks this row against an independent
// re-derivation of the same pose off the same GLB, so a re-export can never
// silently strand the kernel.
//
// ★★★★ RE-PINNED BY R3-WS(e), 2026-08-21 -- CHAD'S RULING: "not make kneeling
// for longitudinal movement". The kneel LEFT the fore-aft ladder
// (render::kKneelRuntimeGain 1 -> 0), so the ceiling this row measures is a
// DIFFERENT, KNEEL-FREE ladder that ends at the deep seated rear crouch. The
// row was re-baked on the live binary in the SAME commit as the flip:
//
//   R3-WS(d), kneel ON  (SUPERSEDED): 0.2786  0.2668  0.2306  0.2041  0.2021
//   R3-WS(e), kneel-free (SHIPPED)  : 0.2183  0.2146  0.2070  0.1984  0.2021
//
// The seated end loses 60.3 mm. That is not a regression this rung caused --
// it is the honest ceiling of the pose Chad chose, and the whole job of this
// row is to stop the kernel asking for CG the body will not deliver. It also
// falls BELOW the pre-K-WS1 flat clamp of 0.25: the un-curved kernel was
// over-asking by 31.7 mm at zero stand all along, which R3-WS(d)'s kneel had
// briefly masked. The K-A2 cross-check test is what forced this re-pin -- it
// goes red on any bake the row no longer matches, and it did.
//
// WHY A FIVE-POINT ROW AND NOT A TWO-POINT LERP. Still refuted, on the NEW
// row: it is NON-MONOTONE at the top (0.1984 then 0.2021) and concave in the
// middle, so an end-to-end lerp (0.2183 -> 0.2021) promises 0.2102 at s = 0.50
// where the body delivers 0.2070, and 0.20615 at s = 0.75 where the body
// delivers 0.1984 -- MEASURED over-asks of 3.2 mm and 7.8 mm. Smaller than the
// (d) row's 9.8/16.8 mm because the row is flatter, and still a lie. The table
// is carried whole.
//
// ★ THIS IS NOT A DIAL. It is a measurement of the shipped animation. The dial
// is SledParams::aft_ceiling_curve (0-OFF), which blends between the flat
// `lean_aft_max_m` the kernel used before this rung and this row.
inline constexpr int kAftCeilSlices = 5;
inline constexpr double kAftCeilC1[kAftCeilSlices] = {0.2183, 0.2146, 0.2070,
                                                      0.1984, 0.2021};

// ★★★ K-WS1 / K2 -- THE RIDER'S SEATED CG OFFSET FROM THE MACHINE CG, body
// axes [m] (+X right, +Y up, +Z AFT).
//
// MEASURED, not assumed. `render::rider_cg` of the rest-posed Sudburian is at
// model (0.00033, 0.81774, -0.35734) on the shipped GLB, and
// render/sled_model.cpp's SHIPPED mount is R_y(180) after a -(cg_height_m -
// kSagDefaultM) drop -- the DRIVE-2 fix, not the stale "(0, cg_h, 0) lands on
// the kernel CG" line in the same block -- so
//     body = (-m.x, m.y - (cg_height_m - kSagDefaultM), -m.z)
//          = (0.0003, +0.3537, +0.3573).
// He sits 0.354 m ABOVE and 0.357 m BEHIND the machine CG, which is the whole
// moment arm this term has. Cross-checked against the shipped GLB in
// test/unit/test_rider_pose.cpp.
// ⚠ OPEN-KWS1-SAG: the vertical depends on `render::kSagDefaultM` (0.10 m),
// which is a VISUAL stance dial with no kernel owner. It is the number the
// shipped renderer actually draws him at, so it is the honest one -- but K2's
// authority scales LINEARLY with it, so if that dial is ever re-owned this
// constant moves with it. Named, not buried.
//
// WHY IT EXISTS AT ALL, and this is the one place the plan's shorthand had to
// be corrected by arithmetic: K-A4 writes the exchange momentum as
// mu * (r_r x v_rel) with "r_r = (-lat, up, -fwd)". Taken literally that is
// the rider's DISPLACEMENT from his seated pose, and a pure aft throw has
// r parallel to v, so the cross product -- and the whole term -- is
// identically zero. The two-body exchange momentum is taken about the RELATIVE
// POSITION of the two masses, which is the seated offset PLUS the
// displacement. The amendment's own sizing (mu*y*v_rel ~ 40 kg m^2/s) only
// closes with a moment arm of that ORDER, and the measured 0.354 m lands
// the same ~0.2 rad/s transient it predicted -- so the sizing confirms the
// corrected form and refutes the shorthand. Reported, not hidden.
inline constexpr double kRiderSeatOffY_m = 0.3537;
inline constexpr double kRiderSeatOffZ_m = 0.3573;

// Per-surface-class dials (§2.3 coupling). ONE table indexed by
// world::Surface -- never a second surface enum (INV-1's spirit at the
// parameter level).
struct SurfaceDials {
    double mu_kin = 0.05;    // longitudinal sliding friction, skis + track
    double mu_lat = 0.85;    // ★ lateral BITE: almost all the feel is here
    double c_snow_pa = 0.0;  // Mohr-Coulomb cohesion of the running surface
    double phi_deg = 0.0;    // ... and its friction angle
    double rho_eff = 0.0;    // effective density available to plane on / throw
    bool sinkable = false;   // does the patch settle into it at all
    // ★ GROOMING (OPEN-VP2 root cause, 2026-08-11). Multiplies the Bekker
    // pack modulus for THIS surface class. The depth-proxy softening in
    // pack_modulus knows only DEPTH, so it cannot express what a groomer
    // does: mechanical compaction + sinter, which carries load at 1-3 cm of
    // sinkage where the same depth of natural pack takes ~8 cm. Those extra
    // centimetres of immersion were feeding the v^2 displacement drag, and
    // THAT -- not power, not the thrust cap -- was what pinned the trail top
    // speed at 23.8 m/s (85 km/h) against a real 650's ~40+ m/s trail pace.
    // 1.0 = natural pack (Bush is bit-identical by x1.0 multiply); only
    // sinkable rows consume it (pack_modulus is gated by its one caller).
    double pack_k_scale = 1.0;
    // ★ GI S1 (ground_interaction_spec.md §PHASE S1, P1-10): the TOTAL Coulomb
    // brake budget for this surface class, in mu units against the patch's own
    // normal load -- separate from `mu_kin`, which is the UNCONDITIONAL sliding
    // drag already acting on the track every tick regardless of the brake
    // input. The applied brake force is
    // `min(brake_force_n, max(0, (mu_brake - mu_kin_eff) * normal))`: the
    // budget is charged against whatever mu_kin_eff is already spending, never
    // stacked on top of it, so a surface cannot brake harder than its own
    // total friction ceiling permits. 999.0 = sentinel OFF -- so far above any
    // real normal-load product that `min(brake_force_n, huge)` always resolves
    // to the flat `brake_force_n` cap, i.e. the pre-S1 kernel's behaviour
    // exactly (the mu term never binds).
    double mu_brake = 999.0;
};

// ★★ RC ROLL COMFORT (ROLL_COMFORT_HANDOFF.md §0b, Chad's superseding ruling
// 2026-08-12: "make it less honest but dont ruin it... arcadey to a degree so
// that it is fun... just make it more stable"). The honesty law is RELAXED for
// these terms BY RULING — they are arcade-LAWFUL: contact-gated, saturating,
// continuous, and every one tunable to OFF from config ([sled_comfort],
// RC-8). What stays forbidden: a governor (nothing here reads or moves the
// rider's mass — the player's lean is the only thing that moves the rider),
// and any if(angle>X) that scripts an outcome. Mechanism provenance:
// ROLL_COMFORT_CONSULT.md (the Fable consult, intent-fidelity measured).
struct SledComfort {
    // --- D: the rolled readout, gated + persistent (a BUG FIX — the old
    // instantaneous readout latched MID-AIR on legitimate sends and cut
    // throttle at the exact moment a rider would blip it; a jump is not a
    // rollover). rolled becomes true only when the >75 deg attitude has held
    // for persist_s WHILE in ground contact within grace_s. OFF = persist 0.0
    // + grace huge (restores the old instantaneous air-latching readout).
    double rolled_persist_s = 0.30;
    // ⚠⚠ SECOND CONSUMER, ADDED 2026-08-28: this window is no longer the roll
    // readout's alone. render/rider_load.h's `rider_air_free_frac` normalises
    // SledState::air_s by it to decide how fast the rider's body frees up in
    // the air (LADDER 7.3 stage 3). The off-config documented above -- persist
    // 0.0 with grace HUGE -- therefore no longer just disables a readout: it
    // makes the rider's legs NEVER trail, silently, with no red test. Move
    // this number and you are tuning two mechanisms.
    double rolled_grace_s = 0.20;
    // --- R4a SEATED SELF-RIGHT (Chad, 2026-08-26) ---------------------------
    // "IF ON THE SEAT AFTER A ROLLOVER PRESSING THE STAND BUTTON AS IN REGULAR
    //  SEATED OPERATION WILL RIGHT THE SLEIGHT ONTO ITS SKIS ... MACHINE NEEDS
    //  TO BE TIPPING OVER STANDING AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED.
    //  BUT ABOVE A CERTAIN SPEED IT CANNOT HAPPEN ABOVE 5KM/H. YES IT CAN FAIL
    //  TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD"
    //
    // He stayed ON the machine through the rollover, so this is NOT stage 8's
    // on-foot "roll it down like a heavy wheel" -- it is the rider's own weight
    // coming up onto the boards and pushing the machine back over. The control
    // is the STAND button he already uses seated: no new key, no mode, no
    // prompt. Progressive: the torque scales with how hard he is standing, so a
    // half-hearted shift does not do it.
    //
    // ★ SHIPS 0-OFF. `stand` is a TAPED input, so unlike every R0-R3 rung this
    // one CAN move a tape: any recorded moment where he was tipped, slow and
    // standing would replay differently. right_assist_nm = 0.0 makes the whole
    // block a no-op (the torque is multiplied by it), the name-keyed tape param
    // roster carries it, and the tape-absent preset pins it to 0.0 -- so all 31
    // existing tapes replay bit-exactly while the game's config turns it on.
    // Same pattern as assist_hull_frac / roll_stiff_vgain / release_floor_frac.
    double right_assist_nm = 0.0;
    // "IT CANNOT HAPPEN ABOVE 5KM/H" -- his number, exactly, not a derivation.
    // 5 km/h = 1.3888... m/s. Hysteretic like every gate in this kernel: it
    // releases at this speed and re-arms at 0.8x, so a machine hovering at the
    // threshold cannot chatter the assist on and off.
    double right_assist_max_ms = 5.0 / 3.6;
    double right_assist_rearm_frac = 0.8;
    // "MACHINE NEEDS TO BE TIPPING OVER" -- the assist does nothing to an
    // upright machine, so it can never be a free roll-stiffness term in normal
    // riding. 0.35 rad = 20 deg of tilt off local up.
    // ★ v2 (2026-08-26, after Chad drove v1): "no it didnt work at all, never
    // got the standing function to right me, also should work from a full
    // inversion, just takes a few sustained pushes."
    //
    // v1 was a CONSTANT torque shaped by sin(tilt), and it failed three ways:
    //   1. sin(tilt) is ZERO at 180 deg -- the assist applied exactly nothing
    //      at the one attitude he most needed it. An unstable equilibrium, sat
    //      on deliberately by the error signal's own shape.
    //   2. no constant torque can win. The resisting torque is m*g*d about the
    //      CONTACT PIVOT, and the arm is the CG-to-pivot DISTANCE, not
    //      cg_height alone: d_rail = hypot(0.19, 0.564) = 0.595 m, so the peak
    //      resist is 331*9.80665*0.595 = 1932 N m. 900 stalls at
    //      18.6 + asin(900/1932) = 46 deg.
    //   3. and the TESTS PROVED 4000 N m while the config SHIPPED 900 -- they
    //      certified a kernel nobody ran. THE LAW: a constant that describes
    //      the shipped table stops describing it the moment the table moves.
    //
    // "A few sustained pushes" is ROCKING. Nobody statically out-muscles 331 kg
    // at 90 deg; you pump energy in over several swings until it passes the
    // balance point. THE ROCK ITSELF NEEDS NO NEW STATE -- orientation and
    // angular_vel already carry amplitude, phase and momentum, damped honestly
    // by the hull spring-dampers. Nothing counts pushes. The only new memory
    // models the PUSHER: a muscle that tires under a sustained press and
    // recovers when released.
    //
    // right_charge drains with this constant while he pushes. Derived from the
    // machine's own measured rock half-period (~0.93 s about the rail pivot),
    // so one press is about one upswing.
    double right_charge_push_s = 1.0;
    // ... and refills with this while he is off it: one release, one return
    // swing. Pressing in time with the machine's own rhythm is the skill; it is
    // a cadence, never a count, and mashing is strictly WORSE than pacing
    // because an off-phase push torques against the swing.
    double right_charge_rest_s = 0.7;
    // ★ THE DEAD-POINT FIX. -up_body.x keeps its (verified) restoring SIGN but
    // now serves as a DIRECTION, saturated through tanh(e / eps): full
    // authority everywhere except a ~10 deg cone about exact inversion and
    // exact upright. Continuous, so there is no sgn() discontinuity to chatter
    // on at the top, and the machine is always pushed the SHORT way home.
    // 0.1736 = sin(10 deg).
    double right_dir_eps = 0.1736;
    // ★ THE SYMMETRY BREAKER IS HIS LEAN, and it is the honest one. At exactly
    // 180 deg the machine is at a real unstable equilibrium and -up_body.x is
    // 0. The physics ALREADY breaks it: rider_lat_m shifts the composite CG
    // (cg_off) and the hull moment arms move with it, so a leaning rider on an
    // inverted machine tips it toward his lean with the assist entirely OFF.
    // This term makes the assist AGREE with that tip instead of fighting it --
    // "just shift", exactly as he ruled. A full lean counts like 10 deg of
    // attitude error. At exactly 180.000 with ZERO lean the torque is 0 and it
    // sits: that is true physics, and R is the ruled escape hatch. Do NOT
    // "fix" that with a hidden nudge -- a hidden nudge is the RNG-shaped sin
    // this kernel forbids.
    double right_seed_frac = 0.1736;
    // The tilt gate becomes a RAMP rather than a hard edge (a hard gate can
    // chatter at exactly its threshold, and this one had no hysteresis). His
    // 20 deg stays the full-on edge; below 14.3 deg it is identically zero, so
    // it can never act as free roll stiffness in ordinary riding.
    double right_tilt_lo_rad = 0.25;
    double right_tilt_hi_rad = 0.35;
    // ★ THE SPEED GATE READS A LOW-PASSED SPEED, and that is not a nicety. A
    // healthy rock translates the CG at up to omega*d ~ 1.4 m/s -- ABOVE the
    // 1.389 m/s gate -- so on the best pushes the raw reading would DISARM the
    // assist mid-swing. His ruling is about TRAVELLING above 5 km/h; a machine
    // rocking in place is not travelling.
    double right_speed_lp_s = 0.5;
    // ★★ THE PUMP. v2 applied the righting torque whenever he pressed,
    // regardless of which way the machine was already rotating -- so it did
    // POSITIVE work on the upswing and NEGATIVE work on the return, netting
    // ~zero per cycle. MEASURED: a paced press and a continuous hold gave
    // identical results (106.7 vs 106.8 deg from inversion) at every cadence,
    // and a second, third and fourth press achieved NOTHING beyond the first.
    // A mechanism that cannot accumulate energy can never take "a few sustained
    // pushes" -- it can only be under or over the one-shot threshold, which is
    // exactly the cliff the sweep found between 1800 (never) and 2000 (one
    // press does everything).
    //
    // Real rocking adds energy because you push WITH the motion and rest on the
    // return. So the torque is weighted by how well the push aligns with the
    // rotation already happening: full authority pushing with it, ~none pushing
    // against it, and half at rest so the first push can still start the rock.
    // This is what makes cadence a SKILL rather than a ritual -- and it is why
    // mashing is strictly worse than timing, since half a mash lands on the
    // return and takes energy back out.
    double right_pump_omega_eps = 0.35;  // rad/s, the alignment scale
    // ★★★ "STANDING AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED" -- Chad's own
    // words in the original ruling, and the thing v3 did not implement.
    //
    // MEASURED, and it is the whole ballgame: from full inversion the machine
    // rights at lean = 1.0 and NEVER at lean = 0.0, 0.25 or 0.5 -- and the
    // direction signal is nearly IDENTICAL in those arms (tanh saturates to
    // -0.94 vs -1.0). So the lean was never helping as a signal. It helps
    // PHYSICALLY: rider_lat_m shifts the composite CG (cg_off) and that is what
    // actually tips the machine over centre. Every one of v3's outcome legs
    // held lean at FULL, so this was invisible -- the fixture-no-op class in a
    // new dimension: the fixture set the one variable that made it work.
    //
    // A man standing up to heave a machine over does not stay centred; he puts
    // himself on the uphill board and shoves. So STANDING itself commands the
    // shift, toward whichever side the righting is pushing, scaled by how hard
    // he is standing and how much push he has left. His own lean still adds on
    // top, so he can still pick the side -- this only means he no longer HAS to
    // for it to work at all.
    //
    // ★ It is also exactly the state the ANIMATION needs: rider_lat_m is the
    // posed lateral offset, so the leg extending to push the machine over is
    // this number, not a separate clip.
    double right_stand_shift_frac = 1.0;  // x lean_lat_stand_m

    double right_assist_min_tilt_rad = 0.35;
    // --- A: contact-gated saturating roll stiffness ("the sway bar that
    // doesn't exist" — the core §0b fix). Torque about the body roll axis:
    //   -stiff_nm * tanh(phi/ref_rad) * release(|phi|) * w_contact
    // phi measured against the CONTACT PLANE fit through the three patch
    // ground points (so a settled side-hill reads phi ~ 0 and the assist
    // never fights held lean); release smoothsteps 1 -> 0 across
    // [release_lo, release_hi] so past the band the assist LETS GO and abuse
    // still rolls (RC-1); w_contact = clamp01(sum N / (0.5 m g)) is
    // identically 0 airborne, so backflips and the airborne-lean anti-cheat
    // leg stay bit-exact. stiff_nm = 0.0 is OFF.
    // release_hi 1.20 MEASURED on the trip ladder (post red-team, with the
    // righting bias correctly reading lateral slide only): entries <= 50 deg
    // caught, >= 60 deg roll — at 1.00 the catch line sat at 40 (barely
    // above the pre-RC kernel), and the 50/60 catches earlier probes showed
    // were the C2 bias cashing FORWARD speed as "side-slide" (red-team P1-3).
    double roll_stiff_nm = 450.0;
    double roll_ref_rad = 0.14;
    double roll_release_lo_rad = 0.60;
    double roll_release_hi_rad = 1.20;
    // Roll-rate damping. ★ SHIPPED AT ZERO on the consult's do-not list:
    // damping is the dial that kills the flick, the drift's body language
    // and backflip initiation off a lip. Earn every N m s of it from a
    // failing bump leg, never from nerves.
    // ★ GI4 EARNS IT, AND NAMES THE PRICE (awaiting Chad's ruling -- see
    // GI4_RIDE_HANDOFF §5). The evidence is his own 512 s tape: 56 rollovers
    // past 90 deg in 8.5 min, crossing 25 -> 90 deg in a MEDIAN of 0.20 s at
    // up to 654 deg/s, with no roll damping anywhere in the machine to slow
    // them. Felt report: "flips happen too fast and easy".
    // WHY THE CONSULT'S OBJECTION DOES NOT REACH THE AIR: this term is
    // multiplied by `release_eff * w_contact` in the assist, and w_contact is
    // identically 0 with no ground under the patches -- so airborne roll,
    // backflip initiation off a lip, and every air flick are untouched, by
    // construction, not by tuning. What it DOES reach is the consult's other
    // named cost: the drift's body language, on the ground. That is the part
    // that is Chad's call, not measurement's.
    // WHY THE CARVE NEEDS IT ANYWAY: with plane_lat_gain 0.3 the lean-in arc
    // on Bush is 323 m at damp 0, 143 m at 150, 42 m at 400 and 30 m at 800
    // -- undamped, the new lateral plate and the roll axis trade energy and
    // the arc wanders instead of holding. The damping is buying the ARC, not
    // just safety.
    double roll_damp_nms = 800.0;
    // 1.0 = phi against the contact plane, 0.0 = against the radial up.
    // ★ 0.5 MEASURED (probe envelope, blend sweep 1.0/0.5/0.0): pure
    // surface-relative is inert exactly where the terrain rolls live (a
    // machine conformal to a slope reads phi = 0), pure gravity fights every
    // berm and held side-hill in full. The half blend measured best of both:
    // flat ground identical (surface normal == radial up there), ordinary
    // 20 deg side-hill downhill drift halves hands-off (67 -> 34 m over the
    // L3 leg) and turns UPHILL with committed lean (-13 m) — the lean-buys-
    // traverse reward, while the carve/drift legs stay in-band.
    double roll_ref_blend = 0.5;
    // --- B: the lean ENHANCER (RC-2 sharpened: lean is a reward channel,
    // never a survival tax, never a penalty). B1: leaning INTO the current
    // lateral demand multiplies effective bite by (1 + gain * align) where
    // align is clamp01 of the SIGN-MATCHED lean fraction — leaning the wrong
    // way gives exactly 1.0, never less ("bad driving allowed, good riding
    // keeps the benefit" as arithmetic; at lean 0 the tables are
    // bit-identical). B2: lean shifts A's release band outward by
    // sat_gain_rad * lean fraction — a committed rider carries more roll
    // angle before the assist lets go. Both read the SLEWED rider state the
    // player commanded; the kernel never moves the rider.
    double lean_bite_gain = 0.18;
    double lean_sat_gain_rad = 0.15;
    // --- ★ GI3 ROLLFIX (GI3_ROLLFIX_HANDOFF §2, C1+C2; the R3 sink trace of
    // Chad's own roll is the provenance — tape_360_chad, tapedbg window
    // 58.5-60.2 s). THE MEASURED HOLE: at 32-70 deg of roll the patches
    // unload onto the HULL (side_normal_sum thousands of N while normal_sum
    // dies), so the assist's patch-only contact gate zeroed the assist 0.2 s
    // BEFORE the release band expired (w_contact 1.0->0.0 across ticks
    // 7119-7131 with release still 0.96); C2's own rate gate (correctly)
    // sat out the whole window; nothing arrested ~500 J of roll energy
    // against the 320 J barrier. Three dials, all stateless, all inside the
    // existing assist term, each 0.0-OFF bit-identical:
    // F-A: the hull's side load COUNTS as contact for the assist — a machine
    // standing on its running-board is not airborne. Fraction of
    // side_normal_sum added to the contact-gate numerator.
    double assist_hull_frac = 0.5;
    // F-B: authority scales with ground-plane speed — the 450 N m ceiling
    // measured saturated through every pre-roll window at 16+ m/s (his tape
    // AND the gi2 matrix); the same ceiling is fine at walking pace.
    // stiff_eff = roll_stiff_nm * (1 + vgain * g(v)). 0.0 = OFF.
    double roll_stiff_vgain = 1.0;
    // F-C: the release band keeps a speed-gated FLOOR — a carving machine is
    // never hard-abandoned mid-event (the S4 relaxation oscillator's fix);
    // abuse at low speed still rolls because g(v) = 0 below v_lo. The floor
    // has its OWN outer fade (release_floor_hi_rad) so a downed machine past
    // ~83 deg still gets exactly nothing. 0.0 = OFF.
    double release_floor_frac = 0.3;
    double release_floor_hi_rad = 1.45;
    // The shared speed gate for F-B/F-C: smoothstep of horizontal ground-
    // plane speed across [v_lo, v_hi]. Inert while both gains above are 0.
    double assist_v_lo_ms = 6.0;
    double assist_v_hi_ms = 16.0;
    // --- C: the on-side contact story (RC-4: post-roll outcomes are a
    // DISTRIBUTION). C1: four coarse hull points (tunnel top edge L/R, ski
    // outer L/R) as unilateral spring-dampers against the one drive surface —
    // the missing half of the contact geometry; slope + momentum then decide
    // whether a downed machine slides back over its ski line. Engagement
    // blends in over [25, 35] deg of body tilt (a perf cull with a smooth
    // edge — upright, the points sit above the patch plane by construction
    // and hard landings stay on the measured patch path). side_k = 0.0 is
    // OFF. C2: a righting bias that only AMPLIFIES existing side-slide
    // momentum (zero below vmin — a stationary machine NEVER self-rights:
    // the anti-magnetism clause), windowed to the on-side tilt band
    // [40, 140] deg of surface-relative roll so it pushes through the
    // ski-line pivot before handing the machine to the assist. ★ 1400
    // MEASURED (post red-team L4 sweep, with the bias reading LATERAL slide
    // only and the fast-saturating load ramp): a fast side-slide (~15 m/s)
    // pops back onto the skis from every on-side entry while 2-8 m/s tips
    // stay down — momentum decides, "not every time but allow it" (RC-4).
    // Chad's tune dial for MORE/less of the show.
    double side_k = 30000.0;
    double side_c = 2500.0;
    // ★ GI S3.2 (ground_interaction_spec.md, P1-15): 0.30 -> 0.55 GLOBAL.
    // Re-measured against the Bush stop band (docs/gi_measurements.md
    // §Phase S3) rather than assumed -- the packet's "per-surface note" for
    // this dial does not exist in this file.
    double side_mu = 0.55;
    // ★ GI S3.1 (P1-8/P1-9): the hull grows from 6 coarse points (RC1) to 10
    // -- the first 6 literals in the pts[] array below are BYTE-UNCHANGED
    // and in the same order, so side_hull_points = 6 is a structural
    // sub-loop, not a re-derivation (RC1 structural bit-identity). 10 =
    // shipped; 6 = the pre-S3 hull, carried by gi_off().
    int side_hull_points = 10;
    // ★ GI S3.3 (P1-16): cohesion plough on the hull's sliding contacts --
    // `+ d.c_snow_pa * pen * width` on sinkable rows, capturing the
    // SurfaceDials row the hull loop was not previously in scope to read.
    // MEASURED (docs/gi_measurements.md §Phase S3, the Bush stop-band
    // fixture): at Bush c_snow_pa = 1200 Pa this term computes ~11 N against
    // ~300 N of the side_mu friction term it sits beside -- under 5% of it.
    // Per the spec's own instruction ("a decoration is worse than an honest
    // absence") it ships OFF (0.0) with the measurement on the record
    // instead of a cosmetic nonzero default. 0.30 is the spec's own trial
    // value, kept as the field's documented non-shipping magnitude so a
    // future surface with real hull cohesion (ice-crusted crust, say) has a
    // number to start from.
    double hull_shear_width_m = 0.0;
    // ★ GI S3.4 (P1-6): Coulomb-style ROTATIONAL friction about the
    // world-vertical axis for a downed machine resting on the hull -- the
    // missing damping that let a tripped machine spin about vertical
    // forever once side_k gave it somewhere to stand. Gated on
    // SledState::hull_engage_lp (0.1 s low-passed hull load fraction, never
    // the instantaneous graze) and hard-clamped in sled.cpp to the torque
    // that would exactly zero the vertical spin THIS SUBSTEP (h = dt /
    // substeps) -- side_yaw_mu is the FRACTION of that ceiling actually
    // applied, so it can never reverse the spin or ring. 0.0 = OFF
    // (structurally: the whole term is skipped, see sled.cpp).
    double side_yaw_mu = 0.35;
    // ★ GI S3.5 (P1-7): gain + wref SOLVED TOGETHER against the L4 probe
    // (docs/gi_measurements.md §Phase S3, the 9-cell sweep) -- 1400 alone
    // (RC1) had no rotational gate at all; adding one without re-solving the
    // gain left 420 unable to right anything (the recovery's own |w_roll|
    // ~5 rad/s sits close enough to a naive wref that the gate throttles the
    // torque down right where the machine needs the last of it to complete
    // the roll). Shipped pair below is the measured best-of-9 against
    // W_C2/320 J in [1.05, 1.75] AND the 15 m/s self-right AND the 2-8 m/s
    // stay-down invariant, per the leg `sled_onside_l4_sweep_still_rights_
    // with_momentum`.
    // ★★★ STOP CONDITION, reported not forced (docs/gi_measurements.md
    // §Phase S3, the 9-cell L4 sweep, same discipline as S2.4's mu_lat
    // STOP): the ratio target W_C2/320 J in [1.05, 1.75] is NOT reached by
    // ANY of the 9 (gain, wref) candidates under the probe's own
    // measurement (best measured ~0.28, well under the 1.05 floor) --
    // ratio is a REPORTED finding, not gated. Among the 9, several cells
    // that DO cross the 30 deg "on-skis" threshold at 15 m/s do so via
    // violent, repeated near-180 deg oscillation (MEASURED via
    // onside_trace) rather than a clean recovery -- not a behaviour worth
    // shipping just to tick a crude threshold-crossing box. (700, 2.5) is
    // the measured best-of-9 on the OTHER axis: smooth, non-oscillatory
    // settling (stalls ~105-125 deg rather than swinging), and the ONLY
    // reading that also keeps every 2-8 m/s cell down (the anti-magnetism-
    // adjacent half of the requirement). Shipped on that basis; the 15 m/s
    // full recovery is not alive at this pair, which is the honest
    // trade-off reported for the supervising session, not silently forced.
    double side_right_gain_nm = 700.0;
    double side_right_vmin_ms = 1.5;
    double side_right_vref_ms = 6.0;
    // Roll-RATE reference [rad/s] for the C2 gate -- `clamp01(|angular_vel.z|
    // / wref)`: a machine still carrying real roll momentum (the "harvested
    // impulse" the C2 comment already names) gets the full righting bias;
    // one that has nearly stopped spinning (parked-adjacent, the anti-
    // magnetism regime side_right_vmin_ms already guards from the LINEAR
    // side) gets a throttled-down one from THIS gate too, in the angular
    // domain. 1e9 == inert (the pre-S3 kernel had no such gate at all,
    // gi_off()'s OFF value).
    double side_right_wref_rads = 2.5;  // MEASURED alongside gain above
};

struct SledParams {
    // --- mass + geometry ----------------------------------------------------
    // ★ THE MEASURED RETOTAL (Phase R follow-up, 2026-08-11, Chad-authorized
    // "to spec" adoption). The 1991 Polaris factory brochure spec chart
    // prints WEIGHT 486 lb for the base Indy 650 = 220.4 kg dry
    // (MEASURED-primary; the same chart reproduces the verified stance/dims/
    // track cells exactly). Plus RT-5 fluids at the ruled half tank:
    // fuel 17.0 + injection oil 2.9 + coolant 3.0 + chaincase 0.2 = 23.1 kg
    // -> machine 243.5 kg wet. `rider_mass_kg` below is split OUT of this
    // total (machine implicitly 243.5), never added on top of it.
    // (History: 275 sum-preserving -> 340 honest retotal at the estimated
    // 252.5 machine -> 331.0 at the factory-measured weight.)
    double mass_kg = 331.0;      // machine + rider, TOTAL
    double cg_height_m = 0.564;  // CG above the running surface -- ★ the
                                 // rollover's numerator (§2.4c.1)
    double stance_m = 0.927;     // ski centre-to-centre. NOT the rollover's
                                 // denominator any more -- see
                                 // track_rail_half_m: the real tipping arm
                                 // runs ski-outer to RAIL-outer, not stance/2.
    double ski_fwd_m = 0.86;     // skis AHEAD of the CG
    double track_aft_m = 0.52;   // track patch BEHIND the CG
    double ski_len_m = 0.95, ski_width_m = 0.135;
    double track_len_m = 1.14, track_width_m = 0.38;  // ground-contact length;
    // the belt LOOP is a separate field (track_belt_circumference_m below) --
    // the 2.5x trap the packets both measured and named.
    // ★★ THE ROLLOVER ROOT CAUSE (PACKET_B §15). A rear patch on the
    // CENTRELINE contributes ZERO roll-restoring moment, so the support
    // polygon was the TRIANGLE ski-ski-track, not a rectangle -- the real
    // tipping axis runs ski-outer to RAIL-outer, and a real Indy's slide
    // rails sit ~11 in apart (~0.14 m half). Nonzero here means the track's
    // normal reaction is split and applied at TWO points this far outboard
    // (sled.cpp), which is a GEOMETRY correction, not a fudge torque.
    // ★ CARRIED OPEN (PACKET_B §15, red-team P2): the roll arm is still fixed
    // at the MOUNT -- an asymmetric `susp_x` (one rail compressed more than
    // the other) does not move the application point closer to the ground,
    // which a real machine's geometry would. The rail split corrects the
    // WIDTH of the support polygon; it does not correct this second, smaller
    // error, which errs toward MORE stability than the real machine has.
    double track_rail_half_m = 0.19;
    // ★★ THE SAME GEOMETRY DEFECT, IN PITCH (GI S2b, 2026-08-12). PACKET_B
    // §15 found the track's normal reaction applied at ONE point across the
    // track's WIDTH and split it to two rails; the identical error was still
    // standing along the track's LENGTH. A 1.14 m contact patch applying all
    // of its normal reaction at a single point contributes ZERO pitch-
    // restoring moment, so once full throttle unloads the skis there is
    // nothing left holding the nose down but the skis themselves -- and they
    // are in the air. MEASURED (S2, docs/gi_measurements.md): with the honest
    // tangential arm the launch wheelie in the signed 0.77 m Bush ran away to
    // 89.9 deg at t=6 s and never came back; nine planing/dwell/cold legs
    // went red on it. This dial is the §15 correction rotated 90 deg: the
    // track's NORMAL reaction is split and applied at two points this far
    // FORE and AFT of the mount (sled.cpp), composing with the rail split as
    // a 2x2 quartering. When the nose comes up the aft point takes more of
    // the load -- and THAT LOAD SHIFT IS THE RESTORING MOMENT. Pure geometry:
    // no torque term, no angle tested, total normal preserved exactly, so
    // ride height / sinkage / planing are untouched.
    // ★★ 0.20 -- MEASURED, AND IT IS NOT THE GEOMETRIC HALF-LENGTH. The
    // obvious value is "the load-weighted effective half of 1.14 m", i.e.
    // 0.40-0.45. That is the right PHYSICAL half; it is the wrong DIAL,
    // because of the convention this code inherits from the rail split:
    // each of the two split points is given the patch's FULL `susp_k` as its
    // differential rate, so the couple produced is 2*susp_k*half^2 -- twice
    // what an honest two-half-spring decomposition of the same patch gives,
    // and six times what a UNIFORMLY bearing strip of half-length a gives
    // (susp_k*a^2/3). Matching the strip requires half = a/sqrt(6): for
    // a = 0.49 m (a 1.14 m patch bearing mostly over its middle 86 %) that
    // is 0.20. The rail split can carry the literal 0.19 because a slide-
    // rail machine really does have TWO discrete supports there; along the
    // LENGTH there is no such pair, so the dial is a discretisation of a
    // continuum and has to be scaled like one.
    // ★ AND THE SWEEP AGREES WITH THE ALGEBRA, which is why it ships. Full
    // sled suite, one dial moved (docs/gi_measurements.md §Phase S2b):
    //   0.15  5 red   0.18  5 red   0.20  0 red   0.22  7 red
    //   0.25 10 red   0.30 12 red   0.45 15 red   (0.0 = the S2 P0, 9 red)
    // Everything in [0.30, 0.57] DOES settle the launch -- the ladder is in
    // the doc -- but it over-stiffens: at 0.45 the fore-aft weight transfer
    // the rider's lean buys collapses to 0.32x of W/L (the machine's own
    // patch absorbs the moment instead of routing it through the skis) and
    // the deep-snow planing that the nose-up trim was feeding goes with it.
    // 0.20 keeps dN_ski/d(shift) at 0.80x of W/L, inside the ruled band.
    // 0.0 = the old single-point track, BIT-IDENTICAL (a separate code path,
    // asserted by sled_track_pitch_split_off_is_single_point and carried in
    // gi_off()).
    // ★ AIRBORNE-NEUTRAL BY CONSTRUCTION: this acts only through the ground
    // normal reaction, which is identically 0 with the patch in the air, so
    // backflips and jump rotation are untouched -- ASSERTED, not argued, by
    // sled_track_pitch_split_is_airborne_neutral (bit-identical airborne
    // trajectory across the whole ruled band of this dial including 0.0).
    double track_pitch_half_m = 0.20;
    // ★★ GI S2.1 (ground_interaction_spec.md, P1-1): the honest tangential
    // arm. Every TANGENTIAL force (sliding friction, plow drag, lateral
    // bite, track thrust, both brake terms) is a moment about a patch
    // offset (§2.4c.1's whole architecture) -- but until this dial they were
    // ALL applied at the suspension MOUNT, which is `susp_rest_m` above the
    // true contact point even at rest, and further still once the patch has
    // sunk into the pack. `bite_at_contact_frac` blends the application
    // point from the mount (0.0) toward the true contact (1.0), by
    // `g.mount + (0, -max(0, susp_rest_m - susp_x[i] + sink_m[i]) * frac, 0)`
    // (sled.cpp) -- the max() clamp is deliberate: past the bump stop the
    // raw term goes negative and would raise the point ABOVE the mount
    // mid-impact (P2-3). NORMAL forces stay at the mount regardless -- a
    // NAMED APPROXIMATION (sled.cpp, tagged at the rail-split site): vertical
    // force at the mount vs the contact differs by a roll torque ~=
    // L*N*sin(roll) -- ~219 N*m at 20 deg roll, 23% of restoring, erring
    // STABLE; deliberately kept at the mount, same class as the CARRIED-OPEN
    // note above. 1.0 = shipped. 0.0 = the old mount application,
    // BIT-IDENTICAL (asserted by sled_gi_off_is_bit_identical_to_rc1).
    double bite_at_contact_frac = 1.0;
    // Free cleat depth (rig honesty, §8 resolved). This is a burial
    // SATURATION SCALE for the thrust/roost model below (bury = sink/this,
    // clamped [0,1]) -- NOT the physical lug height, which the real machine
    // needs for the rig and for its own sake: track_lug_height_m below. The
    // two used to be the same field and that comment lied about which one it
    // was; 0.0184 is the honest lug.
    //
    // ★★ THE PLANING TUNE, 0.19 -> 0.26 (Phase V, 2026-08-11). WINTER_LAW
    // §2.2a fixes the tuning order: depth 0.77 m is the SIGNED INPUT and the
    // sled's own dials are the free parameter, so this is the end of the
    // chain that was allowed to move.
    //
    // WHAT WAS WRONG. `bury = clamp01(sink / track_clearance_m)` and
    // `avail = ... * (1 - bury)`, so this scale decides at what sinkage the
    // roost/escape term of §3.5a switches OFF. The §1 retotal raised the
    // track's bearing pressure by (340/275) * (1.22/1.14) = 1.32x, and Bekker
    // sinkage goes as p^(1/n) = 1.32^(1/1.55) = 1.20x. Steady sinkage at the
    // signed 0.77 m therefore landed at 0.191 m -- ON TOP of a saturation
    // scale of 0.19 that was calibrated at the OLD mass and track length and
    // carried forward untouched. `avail` was then IDENTICALLY ZERO at exactly
    // the depth Chad signed: the escape thrust deleted by arithmetic, the
    // machine pinned at 8.08 m/s and plane_frac 0.181, roost_flux 0.000.
    // Carrying a legacy scale across a 20 % move of its own operating point
    // is how a threshold ends up sitting on the number it is supposed to
    // bound. The derived floor is 0.19 * 1.20 = 0.228.
    //
    // WHY 0.26 AND NOT 0.23. 0.23 clears the floor but puts the wallow
    // boundary at 0.85-1.00 m -- INSIDE the ordinary spread of a pack whose
    // median is 0.77, i.e. a coin flip on the signed depth itself. 0.26 puts
    // it at 1.14-1.40 m: the signed median clears with ~1.5x margin, p95
    // (1.14 m) still planes but slowest, and the p99 drainage line (1.75 m)
    // still bogs, which §2.2a requires and sled_bogs_in_the_p99_drainage_line
    // pins.
    //
    // ★ 0.26 -> 0.255 AT THE MEASURED-WEIGHT RETOTAL (331 kg, 2026-08-11):
    // the lighter machine sinks ~2% less everywhere, which moved the un-bog
    // cliff at the p99/cold-snap corner BELOW h(t_snap)=1.33 -- the
    // sled_p99_drainage_still_bogs_at_the_snap guardrail tripped (plane_frac
    // 0.659, loudly, exactly its job). Re-measured: 0.25 wallows a shallow
    // emergence-matrix cell (1.14 m reads 0.141); 0.255 restores the whole
    // ruled ordering -- 1.14 m planes 0.67-0.72 and 1.75 m bogs 0.107-0.165
    // across the full hardness band [1.0, 1.33]. Same lesson as the 0.19
    // trap above, caught by the guardrail instead of a drive: a threshold's
    // operating point rides the mass.
    //
    // MEASURED SWEEP (`seads_sled_probe`, terminal state at 40 s full
    // throttle on flat analytic Bush; v [m/s] / plane_frac):
    //   depth ->      0.12         0.37         0.77         1.14      1.75
    //   0.19 (was) 27.68/0.637  21.92/0.690   8.08/0.181   7.06/0.138  bog
    //   0.21       27.68/0.637  21.92/0.690  19.07/0.723   7.06/0.138  bog
    //   0.23       27.68/0.637  21.92/0.690  19.07/0.723   7.06/0.138  bog
    //   0.26 (*)   27.68/0.637  21.92/0.690  19.07/0.723  16.31/0.673  bog
    //   0.30       27.68/0.637  21.92/0.690  19.07/0.723  16.31/0.673  bog
    // Rows 0.12-0.55 do not move: thin snow never sinks far enough for this
    // scale to bind. NOT bit-identical, and the honest number matters -- the
    // scale still shifts `bury` a little at shallow sinkage, so 0.37 m reads
    // 21.519 -> 21.554 m/s (+0.16 %) and 0.12 m and TrailMain 0.25 m (top
    // speed 23.76 m/s) are unchanged to the probe's printed precision. The
    // trail is untouched for practical purposes; it is not untouched exactly.
    // Above the hump the planing operating point is also identical across
    // 0.21..0.30 (sinkage settles at 0.080 m, far below any of them) -- this
    // dial sets WHICH DEPTHS can escape the wallow and nothing else. It is a
    // threshold, not a feel knob, which is why it was chosen over the
    // obvious alternative.
    //
    // ALTERNATIVES MEASURED AND REJECTED.
    //  - `plane_gain` 1.05..4.0: works (1.10 gives 20.53 m/s at 0.77 m) but
    //    it scales the lift EVERYWHERE, so the trail speeds up too
    //    (TrailMain 23.76 -> 24.93 m/s at 1.10, -> 30.1 at 1.50), and the
    //    wallow boundary only moves to 0.85 m -- barely past the signed
    //    median. Above 1.15 it also inverts the depth ordering. It buys a
    //    worse result at the price of a global change.
    //  - `bekker_kphi` 88000 -> 140000: works, but it is a MEASURED soil
    //    constant, not a trim, and it moves sinkage on every surface.
    //  - `pack_soften` 0.90 -> 0.45: DEAD by construction. The softening is
    //    referenced to pack_ref_depth_m = 0.77, so at the signed depth the
    //    scale factor is exactly 1.0 whatever this value is (measured: 0.77 m
    //    row unchanged at 8.08/0.181).
    //  - `roost_ref_depth_m`: dead while bury == 1 -- it multiplies the term
    //    that this scale had already zeroed.
    double track_clearance_m = 0.255;
    double track_lug_height_m = 0.0184;  // ★ the REAL free cleat -- rig +
                                         // honesty; not consumed by burial
    // --- G1: ROTOR GYROSCOPICS (docs/sled_gyro_spec.md) ----------------------
    // ★ THE TRACK IS A GYROSCOPE. Everything in the running gear -- belt, drive
    // axle, idlers, chaincase, jackshaft -- spins about the machine's LATERAL
    // axis, so its angular momentum L_r precesses the chassis: a yaw rate makes
    // a ROLL torque and vice versa. Pitch is immune (a pitch rate is parallel to
    // L_r, so the cross product vanishes) -- a structural prediction, pinned.
    //
    // sim/sled.h:691 deferred exactly this ("would have to come from the track
    // as a reaction wheel, which is not this rung"). Chad ruled it IN,
    // 2026-08-27: "I think we should build it. Because then it has the
    // foundation it needs."
    //
    // ★ THE NUMBERS ARE MEASURED, NOT INVENTED. The belt this kernel already
    // describes is a real Camso track: track_belt_circumference_m 3.0724 m is
    // EXACTLY Camso's own pitch x segments (2.52 in x 48 = 120.96 in), and
    // track_lug_height_m 0.0184 m is 0.7244 in, their 0.725 in / 18 mm class.
    // That identifies it as a 15 in x 121 in 9997R at a manufacturer-published
    // 36 lb = 16.25 kg. drive_radius_m is the PITCH radius of the matching
    // 8-tooth 2.52 in driver, N*p/2pi = 0.0815 m (Wahl Bros application chart).
    // Independent check: this kernel's own 8000 rpm at 46 m/s needs an
    // engine:axle ratio of 1.484; real gearing (0.80 CVT shift-out x 1.857
    // chaincase) gives 1.486 -- 0.1 %.
    //
    // ★ MOMENTUM DOES NOT REFER BY RATIO SQUARED. Inertia does (I*n^2); angular
    // momentum is Sum(I_i * w_i) per shaft. rotor_inertia_track_kgm2 is the
    // momentum-equivalent referred to the DRIVE AXLE -- 0.193 kg m^2, derived as
    // L(46 m/s)/w_axle = 108.9/564.4. It covers the TRACK SIDE ONLY. The crank
    // and clutches are deliberately EXCLUDED: a CVT pins engine rpm, so their
    // momentum is roughly CONSTANT with road speed rather than proportional to
    // it, and folding them in here would be the ratio-squared error. They are
    // their own rung, off engine_rpm.
    //
    // k_gyro 0.0 == OFF and BIT-IDENTICAL. Chad's stick moves it, one dial.
    double k_gyro = 0.0;
    // G2: THE REACTION WHEEL -- the other, LARGER half. Spinning the rotor up
    // or down throws -dL_r/dt straight into PITCH: the machine noses up when
    // you blip the throttle in the air. This is the effect riders actually use
    // ("too much throttle off the lip causes your nose to come up"), and it is
    // exactly the "track as a reaction wheel" this file deferred.
    //
    // Applied as a MOMENTUM DELTA to the rate, never as a torque -- the
    // k_air_shift precedent below, for the same reason it gives: routing a
    // momentum transfer through torque_body would hand it to the gyroscopic
    // term as though it were an external moment. The delta TELESCOPES, so a
    // spin-up/spin-down cycle nets exactly zero and no sustained rate can be
    // manufactured out of throttle noise.
    double k_gyro_react = 0.0;
    double rotor_inertia_track_kgm2 = 0.193;  // momentum-equivalent, at the axle
    double drive_radius_m = 0.0815;           // 8T x 2.52 in pitch radius

    // Principal inertia, body axes (X pitch, Y yaw, Z roll) [kg m^2].
    // DERIVED: z=51 measured at the corrected mass/stance, then all three
    // scaled by 331/340 with the measured-weight retotal (inertia rides the
    // mass it was derived at; a mass move that leaves inertia behind is the
    // silent version of the §1 trap).
    glm::dvec3 inertia{158.7, 186.9, 49.7};

    // --- the rider (PACKET_B §9d, S3 rung) -----------------------------------
    // ★ REAL STATE, per §2.4c.1 -- these are PARAMS (reach limits + rates);
    // the rider's actual displacement lives in SledState below, exactly the
    // susp_x precedent: no separate animation channel to drift from the
    // numbers the renderer and the tests both read.
    double rider_mass_kg = 87.5;     // split OUT of mass_kg, NEVER added --
                                     // 87.5 is INSIDE 331, machine is 243.5
    double lean_lat_seated_m = 0.15;  // reach box, seated
    double lean_lat_stand_m = 0.35;   // ... standing (the box opens with the
                                      // ACTUAL rise, not the command)
    double lean_fwd_max_m = 0.45;
    // ★★★ K-WS1 / K1. = kAftCeilC1[0], the MEASURED seated ceiling of the
    // shipped ladder. It is not a feel dial and it is not a guess: it is the
    // s = 0 end of the honest row above, re-baked whenever the ladder moves.
    //
    // ★★★★ R3-WS(e), 2026-08-21: 0.2786 -> 0.2183. Chad ruled the KNEEL OUT of
    // the longitudinal ladder, so the 60.3 mm the kneel was buying at zero
    // stand is gone with it. This is BELOW the pre-K-WS1 flat 0.25 -- the
    // kernel had been over-asking by 31.7 mm seated since long before this
    // program, and the last ~12.7 % of the player's aft mouse travel bought no
    // pose at all. It buys none now either, because the kernel stops asking.
    // HONEST HEADLINE FOR CHAD: this rung REMOVES aft demand, it does not add
    // authority; the authority ceiling is the pose he chose.
    double lean_aft_max_m = 0.2183;  // bounded by arm reach, not seat length
    // ★★★ K-WS1 / K1, THE DIAL. 0.0 = the pre-rung behaviour EXACTLY: one
    // flat `lean_aft_max_m` at every stand (structurally skipped, bit-
    // identical). 1.0 = the full measured kAftCeilC1 row, piecewise-linear in
    // rise_frac. Anything between blends. The A/B back to today's kernel is
    // therefore two numbers: aft_ceiling_curve 0 + lean_aft_max_m 0.25.
    // SHIPS ON at 1.0 -- it is HONESTY, not feel. R3-WS(e) numbers: against
    // the pre-K-WS1 flat 0.25 the un-curved kernel over-asked 31.7 mm seated
    // and 47.9 mm at full stand; against the re-pinned flat 0.2183 it still
    // over-asks 16.2 mm at s = 0.75 (body 0.1984). The curve is what turns
    // those into zero -- a lie the rider's body pays for with a clamp nobody
    // sees. (Was documented as "77 mm at full stand" while lean_aft_max_m was
    // the kneel-inclusive 0.2786; that era is SUPERSEDED, not deleted.)
    double aft_ceiling_curve = 1.0;
    double stand_rise_m = 0.25;      // rider CG rise seated -> standing
    double tuck_drop_m = 0.10;
    double lean_tau_s = 0.18;        // slew: a body cannot teleport
    // ★ R4a §7.7: the grip law's dials. `capacity` ships at a value that can
    // never be met and `unseat_gain` is calibrated against the drawn chain over
    // his own drives -- see sim/rider_grip.h. Nothing reads the state they
    // produce, so neither can move a golden.
    GripParams grip;
    BuckParams grip_buck;
    double lean_rate_ms = 1.40;
    // ★ OFF by design (§9d.3): side-hilling is HELD for many seconds, and an
    // auto-centring lateral axis cannot side-hill (the mousepad runs out).
    // Present as a structural off-arm, never defaulted on.
    double lean_return_tau_s = 0.0;
    // ★★★ K-WS1 / K2 -- AIRBORNE MOMENTUM EXCHANGE. THE FEEL DIAL, 0-OFF.
    //
    // In flight there is no external torque about the system CG (gravity is
    // torque-free at the CG, drag has no application point in this kernel), so
    // the machine+rider system CONSERVES angular momentum. When the rider
    // moves his 87.5 kg relative to the machine he spends exchange momentum
    //     L_exch = mu * (r_rel x v_rel),   mu = m_r*m_mach/m = 64.4 kg
    // and the chassis takes the opposite: I*omega + L_exch = const, i.e. the
    // per-substep discrete form this kernel applies is
    //     d(omega) = -k * I^-1 * d(L_exch).
    // Because it is the DELTA of a momentum and not a torque, it cannot
    // diverge under a constant slew and it produces NO sustained rate: when
    // the rider stops moving L_exch returns to zero and so does the rate the
    // chassis borrowed. What survives is an ATTITUDE change of a few degrees,
    // which is exactly what body English can honestly buy. A PERSISTING rate
    // would have to come from the track as a reaction wheel, which is not this
    // rung.
    //
    // 1.0 IS THE HONEST SCALE (no fudge). Sizing at full aft throw: mu * y *
    // v_rel ~ 64.4 * 0.44 * 1.40 = 40 kg m^2/s over I.x 158.7 => ~0.25 rad/s
    // of transient pitch. THE SIGN IS MEASURED BY TEST, never typed -- see
    // sled_airborne_lean_is_RESPONSIVE_at_k_nonzero (test_sled.cpp). The
    // name above used to read sled_air_shift_pitches_the_nose_DOWN_on_an_aft
    // _throw, which has never existed -- a red team chasing the citation
    // found it dead. A comment that names a test IS a claim about the tree.
    //
    // DEFAULT 0.0 = OFF, and off is BIT-IDENTICAL (structural branch). Chad's
    // drive decides ON: a real term whose honest sign surprises the hand is
    // exactly how "real" gets ruled backwards.
    double k_air_shift = 0.0;
    double stand_cda_add_m2 = 0.23;  // air drag when risen (0.62 -> 0.85 full
                                     // stand) -- standing costs top speed,
                                     // which is correct and free

    // --- drivetrain (CVT, S3 rung) -------------------------------------------
    double clutch_engage_ms = 3.25;  // below this, closed throttle FREE-COASTS
                                     // on mu alone (full decouple); Chad's
                                     // 50 ft coast emerges from this + mu_kin
    double engine_brake_n = 420.0;   // closed-throttle drag while engaged
    // ★ GI S1 (P0-4a): true = the engine-brake term (above) applies on EVERY
    // tick regardless of the brake pedal, stacked underneath whatever the
    // player's own brake (SurfaceDials::mu_brake-budgeted, above) is doing.
    // false = byte-exact old gate (`brake < 0.05` only -- engine brake fades
    // out the instant the player brakes at all, on the old "don't double-
    // count the coast band" reasoning). true is a real behaviour change, not
    // a continuity nicety: the S1 ordering leg is swept and shipped with it
    // ON.
    bool engine_brake_stacks = true;
    double track_belt_circumference_m = 3.0724;  // the LOOP -- separate from
                                                  // track_len_m's ground
                                                  // contact (the 2.5x trap)
    double steer_rate_per_s = 2.0;   // handlebar slew -- feel, not the
                                     // rollover fix (PACKET_B §15.4)

    // --- suspension (§2.4c.1: REAL STATE, never an animation clip) -----------
    double susp_k = 46000.0;      // N/m per patch
    double susp_c = 3600.0;       // N s/m
    double susp_rest_m = 0.21;    // hang below the mount at zero load
    double susp_travel_m = 0.26;  // bump stop
    double susp_stop_k = 5.0;     // stiffness multiplier past the stop

    // --- pressure-sinkage, Bekker (§3.2) ------------------------------------
    // p = (k_c/b + k_phi) * z^n. Snow values; b is the patch's SHORT dimension.
    double bekker_kc = 3800.0;
    double bekker_kphi = 88000.0;
    double bekker_n = 1.55;
    // ★ DEPTH IS A PROXY FOR CONSOLIDATION -- a named approximation (see
    // sled.cpp). Stiffness scales off the SIGNED median depth, so at 0.77 m the
    // Bekker constants above apply exactly and the deep drainage lines are
    // softer, as §2.2a requires them to be.
    double pack_ref_depth_m = 0.77;  // ★ Chad's signed median. NOT a free dial.
    double pack_soften = 0.90;
    // --- SC1 COLD IS POWER (WINTER_LAW §3.7) -------------------------------
    // ★ PLAIN DOUBLES, not a world::ColdParams pointer/value: the app owns the
    // clock and world::cold's curve, and writes these TWO numbers once per
    // tick before step_sled (SC1_COLD_SPEC.md §3) -- the kernel stays a pure
    // multiply-at-use consumer with no new state and no dependency on
    // world/cold.h, exactly the same seam SledInputs already draws against
    // sim::Environment (this file's header comment, INV-7).
    double snow_hardness = 1.0;  // world::hardness(air_temp_c, cold_params);
                                 // scales pack_modulus (sled.cpp) -- the LIVE
                                 // cold chain: stiffer pack, less sinkage.
    double air_temp_c = -15.0;   // world::air_temp_c(...); consumed by the
                                 // mu_kin warm-drag term and the breathing
                                 // term, both at their point of use.
    // T_ref_K is DERIVED from this at use (never a literal) -- the breathing
    // term's reference temperature. Defaults to [cold] t_ref_c so an unwired
    // caller (e.g. a probe that never touches world::cold) gets the same
    // bit-neutral reference the Bekker constants are tuned at.
    double cold_t_ref_c = -15.0;
    // ★ ALL of warm's performance loss lives on THIS dial (spec §0/§3), on
    // sinkable rows only -- never on snow_hardness. Mirrors [cold]
    // warm_drag_gain's default exactly, so an unwired caller (air_temp_c ==
    // cold_t_ref_c by default) reads bit-neutral regardless of this value.
    double warm_drag_gain = 0.35;
    // ★ THE RATE PROCESS (§3.5a). Sinkage is STATE, in TWO timescales: the
    // Bekker depth arrives at once and is felt at any speed; the creep on top
    // of it is what takes TIME. Dwell buries you, travel sheds -- and it is the
    // second term alone, which is why "hit full throttle" works and why deep
    // snow and thin ice are one mechanism.
    double creep_frac = 2.6;       // dwell excess as a multiple of z_inst
    double tau_sink_s = 0.62;      // time constant of that excess
    double refresh_len_m = 1.15;   // travel length over which the excess sheds

    // --- shear-limited thrust (§3.2, §3.5a) ---------------------------------
    double shear_K_m = 0.055;        // shear-displacement modulus
    double track_speed_max_ms = 46.0;  // track surface speed at full throttle
                                       // (corrected arithmetic, §0 resolved)
    // ★ NOT THE LIMIT IN SNOW -- the snow is (§3.2). It is a CEILING, and the
    // ceiling has a job: P/v falls with speed, so the launch wheelie that
    // thrust-below-the-CG produces settles back onto the skis as the machine
    // gets going. At 34 kW it never settled and the machine drove on its track
    // at 20 deg nose-up forever, skis in the air and no steering at all -- the
    // trace found that; no terminal number showed it.
    // ★ 102.5 hp, the 1991-92 carb hero-year spec (S3 rung, §8 P2 fold --
    // supersedes an earlier 73.6 kW pinned to the 1990 RXL dyno). Spec
    // fidelity, not a top-speed fix: measured DEAD on rollover/backflip
    // (OPEN-VP2).
    double engine_power_w = 76400.0;
    // ★ PEAK DRAWBAR PULL. The traction limit A*(c + sigma*tan phi) is
    // proportional to the INSTANTANEOUS normal load, so a landing spike briefly
    // offers thousands of newtons of grip -- and without this cap the kernel
    // cashed it as thrust and backflipped the machine on the first bump. A real
    // sled cannot pull harder than its engine can turn the track no matter how
    // much grip the snow is offering.
    // ★ 2272 N = 0.70 g measured full-throttle accel AT THE MEASURED 331 kg
    // retotal (0.70 g x 331 kg x g -- the cap is a measured ACCELERATION,
    // 0.70 g from ~500 straight-line tests, so it rides the mass; 2334 was
    // the same 0.70 g at the estimated 340 kg, and 1889 at the old 275).
    // RT-1 ruled DERIVE-from-shear-ceiling; that adoption is the rung-2
    // clutch-causality rework, not this retotal.
    double max_thrust_n = 2272.0;
    // ★★ GI4 §9.7 ITEM 2 — TRACTION-LIMIT THE THRUST (Chad's ruling, 2026-08-24:
    // "at high speeds, getting pulled in and flipping out like 15x is not
    // desirable so yes take care of that"; planing lift itself STAYS — "it is
    // important for traversing atop the snow" and "at slower speeds I think it
    // is good too because [I'd] like to jump the snowbanks").
    //
    // WHAT THE TRACE MEASURED (§9, probe `attractor`, Bush 0.85 WOT full lock):
    // at the settled −28.7° attractor the machine holds 62 N of its 4106 N on
    // the running surfaces — ski_L 0 N, ski_R 7 N, track 55 N — and the
    // drivetrain is still pegged at max_thrust_n. §9.7 item 2, verbatim:
    // "2272 N of drawbar on 55 N of track load is what turns a lean into a
    // runaway. This one stops the loop rather than the lean."
    //
    // WHY THE EXISTING CAP DOES NOT DO THIS. max_thrust_n's own note above says
    // the traction limit A*(c + sigma*tan phi) "is proportional to the
    // INSTANTANEOUS normal load". The sigma term is; the COHESION term is not.
    // `shear` = area*(c_eff + sigma*tan phi)*(...) carries area*c_eff at ANY
    // load, and `roost_thrust` ~ flux*rho_eff*area*v_rel^2 carries none either,
    // so a track with 55 N under it still develops thousands of newtons. Both
    // existing caps are ENGINE ceilings (max_thrust_n, engine_power_w/v); there
    // has never been a CONTACT ceiling.
    //
    // WHY THIS BREAKS THE LOOP AND NOTHING ELSE. §9's law: "every restoring roll
    // term is scaled by contact load ... and every destabilising one is not",
    // giving speed -> lift -> contact down -> restoring down -> speed. Thrust is
    // the term that FEEDS that loop, and it is unscaled. Scaling it by the same
    // contact load every restoring term already pays for closes the loop at its
    // source, WITHOUT touching planing lift (Chad's ruling) and WITHOUT
    // re-shaping a lateral force (§9.7's explicit do-not: "§8.3 has four
    // measurements saying that class does not reach this").
    //
    // WHY IT IS INERT WHERE CHAD WANTS IT INERT. The cap binds only where
    // `normal` has already collapsed — the high-speed planing regime that makes
    // the runaway. In contact (a standing start, a bank jump's approach and its
    // landing, any normal plowing run) `traction_mu * normal` is far above the
    // engine ceiling and this changes nothing, so the low-speed jump behaviour
    // he asked to keep is untouched BY CONSTRUCTION, not by tuning.
    //
    // <= 0.0 = OFF (a BRANCH, not 0.0*normal arithmetic — structurally
    // bit-identical to the pre-item-2 kernel), and OFF is also the TAPE-ABSENT
    // reconstruction for every tape recorded before this dial existed.
    // ★ SHIPS AT 0.0 PENDING MEASUREMENT: the value is NOT invented here. The
    // fence (tip onset / peak / ratio) and the carve matrix are swept and put
    // in front of Chad, per NO GUESSING, before any non-zero default.
    double traction_mu = 0.0;
    // ★ GI S1 (P1-10): 1201 -> 2600. mu_brake is now the TOTAL Coulomb budget
    // that caps the achievable brake force per surface (see SurfaceDials::
    // mu_brake) -- this raw newton figure is only ever the CEILING on top of
    // that per-surface budget, never the sole limiter on a high-mu surface
    // (Road) the way the flat 1201 N figure was. Retuned against
    // `sled_brake_ordering_road_beats_snow_beats_ice` on the post-S2b kernel;
    // see docs/gi_measurements.md §Phase S1 for the per-surface decel table
    // this and the mu_brake table below were swept against together. Target
    // band was originally 0.32-0.42 g at 331 kg on the OLD flat-1201 kernel
    // (superseded once brakes are surface-gated by mu_brake).
    double brake_force_n = 2600.0;
    double roost_ref_depth_m = 0.55;  // depth at which `available_snow` saturates
    // Momentum-flux gain on the ejected snow. ★ This is the ESCAPE thrust and
    // the S5 roost at the same time -- it multiplies `roost_flux`, so a change
    // here changes both, which is the point (§3.5a).
    double roost_gain = 0.55;

    // --- planing (§3.1, the central mechanism) ------------------------------
    double plane_gain = 1.0;      // trim on the flat-plate lift
    // ★ GI4 THE PLANING LATERAL FORCE (GI4_RIDE_HANDOFF §2; provenance =
    // Chad's sled_tape_4, 512 s, and the `carveloads` probe). THE MEASURED
    // HOLE: the lateral bite is `-normal * mu_lat * tanh(slip/slip_ref)` --
    // it reads the BEKKER NORMAL REACTION only. But the planing lift above
    // is what carries the machine once it is up on the snow, and the two are
    // in direct competition: at WOT on Bush at 12 m/s the steady state is
    // ski 78 + 108 N and track 727 N -- 913 N of contact under a machine
    // that weighs 4106 N. The lift carries 78 % of the weight and the
    // steered patches keep 4.5 % of it, so `normal` -- and with it every
    // gram of cornering force -- has already gone to zero by the time the
    // machine is going fast enough to want to turn. Measured consequence:
    // 370 m of steady radius at full lock against a 3.10 m kinematic
    // geometry, and NOT recoverable from mu (a track_lat_mu x
    // lean_bite_gain sweep bottoms out at 252 m).
    // THE PHYSICS THAT WAS MISSING: a planing plate yawed to the flow
    // deflects it SIDEWAYS as well as down. The cornering force of a planing
    // ski comes from the same dynamic pressure as its lift, not from the
    // residual static load underneath it. So this is the lift equation with
    // the attack angle taken in the lateral sense -- same plate, same draft
    // scaling, same rho_eff gate (so it is identically 0 on Road/LakeIce,
    // where rho_eff = 0 and there is no snow to deflect), applied at the
    // same mount so its yaw moment is geometry, not a tuned couple.
    // 0.0 = OFF, bit-identical to the pre-GI4 kernel. 0.3 is the measured
    // choice: on Bush at WOT and full lock it takes the steady radius from
    // 370 m to a dead-flat 33.2/33.5/33.7/33.9 m at v0 = 8/12/16/20 (the
    // speed-invariance IS the "hold a turn" property), at -10 deg of body
    // roll, with lean-in tightening it to 30 m. 0.6 and above roll the
    // machine over at some speed in the sweep; 0.5 clears but with only
    // -20 deg of roll margin left.
    double plane_lat_gain = 0.3;
    // The lean reward on the term above, STEERED PATCHES ONLY. B1's
    // lean_bite_gain is machine-wide and therefore self-defeating for an arc
    // (measured: 0.18 -> 0.45 took the lean-in carve from 30 m to 295-1037 m
    // of radius, because the track out-gains the skis); this one can only
    // ever add ski plate. Inert while plane_lat_gain is 0.
    double plane_lat_lean_gain = 0.0;
    // ★ GI4: how much of the planing lateral plate follows the patch's LOAD
    // SHARE rather than its (saturating) draft. The mu bite is proportional
    // to normal load, so weight transfer quiets the unloaded inside ski for
    // free; the plate had no such term and therefore made a pure roll couple
    // that took the machine's own tip onset from 1.035 g to 0.316 g for only
    // 0.10 of gain. 0.0 = the saturated behaviour, 1.0 = fully load-shared.
    // ★ SHIPS AT 0.0, AND THE REASON IS THE NEXT RUNG'S SPEC. Measured at
    // 1.0: the carve dies with the couple -- Bush steady radius 33 m -> 198 m
    // -- because the ABSOLUTE load is what collapsed (the steered patches
    // hold 186 N of 4106 N at WOT), so any term scaled by it scales to
    // nothing. This scaling removes the FORCE when the intent was to remove
    // only the COUPLE. The correct formulation normalises each steered patch
    // against the MEAN of the steered patches -- preserving the total plate
    // while restoring the inside/outside asymmetry the mu bite gets for free
    // -- not against the machine's weight. IMPLEMENTED that way in the
    // deferred second pass after the patch loop (the mean does not exist
    // until both skis are solved). The pair's shares average to 1 by
    // construction, so the TOTAL plate is preserved and only the couple is
    // removed. 0.0 reproduces the saturated behaviour exactly.
    // ★★ SHIPS AT 0.0 -- THE HYPOTHESIS IS FALSIFIED, NOT UNFINISHED.
    // Implemented properly (deferred second pass, mean over the steered
    // patches, total plate conserved) and MEASURED on the fence: the tip
    // onset moves 0.318 g -> 0.334 g, ~5 % of the 1.035 g it has to recover.
    // The unloaded inside ski's plate is NOT what craters the tip onset. It
    // also costs the ride: 30.6 m at v0 8-12 (better) but the 16 and 20 m/s
    // cells bifurcate into a -28 deg leaned, non-turning, 35 m/s runaway,
    // where load_frac 0.0 holds a dead-flat 33 m at every speed. Kept wired
    // and OFF as the record that this branch was tried and closed.
    double plane_lat_load_frac = 0.0;
    // ★★ GI4 §9.7 ITEM 1 (Chad's ruling, 2026-08-13): THE LIFT'S RAIL/PITCH
    // SPLIT. The §9 attractor trace measured the planing lift applied as ONE
    // point force at `g.mount`, making −216 N·m of DESTABILISING roll moment
    // at −28.7° — the largest single roll term in the machine, from 2845 N of
    // lift on a machine holding 62 N of its 4106 N on the running surfaces.
    //
    // This is the SAME DEFECT GI S2b already found and fixed for the normal
    // reaction, rotated onto lift: "all of a 1.14 m patch's normal reaction
    // applied at one mid-patch point carries NO pitch-restoring moment"
    // (sled.cpp, the normal split's own header) — measured there as a runaway
    // to 89.9°. A single-point lift carries no ROLL-restoring moment for
    // exactly the same reason, and lift is the term still at full strength
    // once contact (and with it every load-scaled restoring term) is gone.
    //
    // THE SPLIT IS BY DRAFT, NOT BY THE NORMAL'S LOAD SHARES. The normal's
    // fr/fl come from `susp_k * dx` against the patch's own normal load, and
    // that ratio DEGENERATES exactly in the regime this fixes (normal → 0 at
    // the attractor). Lift's own physics gives the honest weighting for free:
    // lift ∝ draft, and `dx` is already the extra hang of the outboard rail
    // off the same linearised geometry, so the deeper-riding rail makes more
    // lift. Unilateral like the normal's `max(.,0)`: a rail lifted clear of
    // its draft makes none, and all of it goes to the other rail at its
    // offset.
    //
    // WHERE IT IS INERT, BY CONSTRUCTION. Where `draft` saturates (both rails
    // past plane_draft_m, i.e. any normal plowing run) the shares are 0.5/0.5,
    // which at ±offset is ANALYTICALLY the mount — so this changes nothing in
    // the regime the shipped kernel already handles, and acts only where the
    // machine is skimming shallow, which is the regime that made the
    // attractor. 0.0 keeps the literal single-point `add_at` and is
    // BIT-IDENTICAL (structural: the split is a separate branch, not 0.5/0.5
    // arithmetic through the same adds).
    //
    // ★★ WHAT IT ACTUALLY BOUGHT, MEASURED (§10). On the term it targets it
    // WORKS: at the attractor the lift's roll moment flips from −145 N·m
    // (destabilising) to +289 N·m (restoring), a 434 N·m swing, and the
    // machine climbs off its hull instead of sitting on it. On the FENCE it
    // does not close the gap:
    //
    //     split   tip onset (Bush 0.30)   peak/onset   gate
    //     0.00        0.318 g               2.991      1171/1175  (baseline)
    //     0.25        0.319 g               2.984      1171/1175
    //     0.50        0.334 g               2.855      1170/1175
    //     1.00        0.363 g               2.623      1169/1175
    //                (needs ~1.035 g and a ratio <= 0.90)
    //
    // 1.00 recovers ~6 % of the onset gap — the same order as the
    // mean-normalisation that §8.3 item 4 declared falsified — and its gate
    // cost is three tests, of which TWO are the machine getting MORE
    // tip-resistant than a pinned baseline (1052's ordering goes vacuous when
    // two cg rows stop rolling at all; 3476's `before.rolled` pin breaks) and
    // ONE is a real loss: 2336, lean-in no longer tightens the arc by 10 %
    // (28.640 m vs 28.632 m free) — which is Chad's own felt item 5, rider
    // weight authority. Bush at 0.85 m is UNCHANGED to three decimals across
    // the whole carve matrix, exactly as the saturation argument predicts.
    //
    // ★ SHIPS AT 1.0 — CHAD'S RULING, 2026-08-15 ("ship at 1.0"), taken with
    // the §10.4 cost itemised in front of him. The mechanism is right and the
    // machine no longer rests on its hull; the fence is NOT closed by this
    // alone (§9.6: lift's application point is ONE leg of a three-legged loop,
    // §9.7 items 2 and 3 are the others), so do NOT read a green gate here as
    // the attractor being solved. What the ruling costs, and what is still
    // owed:
    //   - test 2336 (lean-in tightens the arc by 10 %) is RE-PINNED to the
    //     1.0 behaviour and is a REAL REGRESSION, not a stale baseline: it is
    //     Chad's own felt item 5, rider-weight authority. It is carried as an
    //     open debt against §9.7 items 2+3, never as an absorbed cost.
    //   - tests 1052 and 3476 are re-pinned because the machine got MORE
    //     tip-resistant than the baselines asserted (1052's ordering went
    //     vacuous on the never-rolled sentinel; 3476's `before.rolled` pin
    //     stopped rolling). Improvement outrunning the tests.
    // The TAPE-ABSENT RULE keeps this at 0.0 for every tape recorded before
    // the dial existed (test/harness/sled_tape.h) — old goldens still replay
    // the drive that actually happened.
    double plane_lift_split_frac = 1.0;
    // ★ THE DRAFT that makes full planing lift. Lift scales with how deep the
    // patch is riding, which is what makes planing a stable TRIM instead of a
    // launch -- and it is the same draft the plow drag is computed from, so
    // "still plowing some" is structural rather than a tuned floor (§2.2a).
    double plane_draft_m = 0.075;
    // ★ THE MACHINE'S PLAN AREA -- the footprint of the trench it has to climb
    // out of to plane. Charging only the running-surface areas made the
    // planing threshold move ~5 % with depth, which is not enough to express
    // §3.1's "deeper snow, higher planing speed" against everything else that
    // moves with depth. Skis + tunnel + running boards.
    double plan_area_m2 = 1.90;
    double ski_rake_rad = 0.115;  // built-in ski attack angle (~6.6 deg)
    double alpha_max_rad = 0.45;  // clamp on the effective attack angle
    double plow_cd = 1.15;        // displacement drag on the submerged frontal
                                  // area -- ★ NEVER zero in snow ("still
                                  // plowing some", §2.2a)
    double air_cda = 0.62;        // m^2, rider + machine
    double air_rho = 1.29;        // kg/m^3

    // --- steering (§3.2: ski lateral bite under weight transfer) ------------
    double steer_max_rad = 0.42;
    double slip_ref_rad = 0.30;      // slip angle at which bite saturates
    // ★ 0.70 -- graph START-HERE + A's real band 0.65-0.74. Measured NON-
    // BINDING in a steady yaw balance (the track only ever demands mu~0.37,
    // the SKI is the limiter -- PACKET_B §15.2), so 0.70 vs the old 1.55 is
    // behaviour-neutral today; kept as the file-consistent value and rostered
    // (sled_track_lat_mu_is_the_rostered_value) so it cannot drift unnoticed.
    double track_lat_mu = 0.70;      // a track slides sideways poorly
    // ★ GI S2.6 (plane_fit_load_weight, P0-5 restored): the roll-assist
    // reference plane (sled.cpp's `n_surf`, fit through the three patch
    // ground points) by default used every patch's ground point equally --
    // including an UNLOADED one (a ski lifted off the ground beside a bank
    // face still has a sampled ground point there, from the bank's own
    // terrain, even though the patch carries no weight). That reads a
    // spurious tilt into the reference: MEASURED 321.6 N*m of assist torque
    // in the scripted straddle leg with the fit unweighted, vs 45.5 N*m
    // weighted (sled_assist_reference_plane_is_load_weighted). ★ NOT a
    // per-edge Newell reweight (three points are always exactly coplanar, so
    // that is a mathematical no-op -- measured directly, see sled.cpp's
    // comment at the implementation): a RELIABILITY blend toward `up_cg`
    // (the existing fallback's own target) in proportion to how UNEVEN the
    // three patch loads are. 1.0 = shipped. 0.0 = the old unweighted fit,
    // BIT-IDENTICAL (a separate code path, not a weight-1 special case of
    // the new one -- see sled.cpp).
    double plane_fit_load_weight = 1.0;
    // --- integration --------------------------------------------------------
    // ★ MEASURED, NOT GUESSED -- the card's named unknown, closed. Run
    // `seads_sled_probe substep`: the criterion is the smallest count within
    // 0.1 % of a 24-substep reference on BOTH cases, and the second case is the
    // one that decides it. A straight-line launch converges at 3 substeps; the
    // 2 m drop at 20 m/s -- the stiff contact event, which is what a §2.4c
    // snowbank produces -- decides the count. RE-MEASURED at the 331 kg
    // factory-weight retotal (2026-08-11): 8 reads 1.8e-3 on the drop, 12
    // reads 9.2e-4 (launch 5.4e-5). The criterion stayed; the count moved.
    int substeps = 12;

    // ★ RC roll comfort dials (§0b ruling; see SledComfort above).
    SledComfort comfort;

    SurfaceDials dials[static_cast<int>(world::Surface::kCount)];
    SledParams();
};

struct SledState {
    glm::dvec3 position{0.0};                    // world CG [m]
    glm::dvec3 velocity{0.0};                    // world [m/s]
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};  // body->world, unit
    glm::dvec3 angular_vel{0.0};                 // body frame [rad/s]

    // ★ REAL STATE, per §2.4c.1. The renderer, the dash and the tests all read
    // THESE numbers; there is no separate animation channel to drift from them.
    double susp_x[kPatches] = {0.0, 0.0, 0.0};  // compression [m]
    double susp_v[kPatches] = {0.0, 0.0, 0.0};  // compression rate [m/s]
    // ★ THE DWELL VARIABLE (§3.5a). `creep_m` is the integrated state -- the
    // excess sinkage bought by TIME UNDER LOAD; `sink_m` is the total the rest
    // of the model reads (instantaneous Bekker depth + creep, clamped to the
    // snow that is actually there).
    double creep_m[kPatches] = {0.0, 0.0, 0.0};
    double sink_m[kPatches] = {0.0, 0.0, 0.0};

    // ★ THE RIDER, real state (§9d.7): store DISPLACEMENT, derive the CG
    // offset from it. The renderer places the pelvis from these same numbers
    // -- one number, no animation channel to drift from it. Advances per
    // SUBSTEP, slewed (a body cannot teleport).
    // --- R4a seated self-right (Chad 2026-08-26). Both are DERIVED state,
    // deliberately NOT in the positional pin roster: `right_assist_armed` is a
    // pure function of ground speed, and `right_assist_nm_now` is a pure
    // function of this substep's inputs and attitude -- both re-converge from
    // tick 0 on replay, the `ws_exch_l` class. Extending the POSITIONAL pin
    // roster would refuse every existing tape (41 doubles, read by index).
    // ⚠ right_assist_armed is NOT "a pure function of ground speed" -- it is
    // HYSTERETIC, so it carries memory inside the release/re-arm band. The
    // earlier comment here said otherwise and was a lying instrument.
    bool right_assist_armed = true;    // hysteretic speed gate
    double right_assist_nm_now = 0.0;  // readout: what the assist applied
    double right_charge = 1.0;         // push budget [0,1]; drains, refills
    double right_gs_lp = 0.0;          // low-passed ground speed for the gate
    // The brace side commanded by the self-right, carried one substep so the
    // lean slew (which runs earlier in the step) can consume it. Deterministic;
    // one substep is 1/1440 s.
    double right_shift_cmd = 0.0;      // [-1,1], + = brace LEFT
    double rider_lat_m = 0.0;  // + LEFT, to AGREE with steer +1 = LEFT
    double rider_fwd_m = 0.0;  // + forward
    double rider_up_m = 0.0;   // + standing, - tucked
    // ★★★ R4a §7.7 STAGE 1 -- THE GRIP, AND IT IS STRUCTURALLY INERT TODAY.
    //
    // `sim::GripState` carries the arming memory, the kernel's own extension
    // from the pose, the load through his hands (hardness x extension) and the
    // one-way `attached` latch. See `sim/rider_grip.h` for the whole argument,
    // including why the kernel grows its OWN extension (Chad's ruling
    // 2026-08-30) rather than reading the render chain it is modelled on.
    //
    // ⚠ NOTHING IN THIS KERNEL READS IT. It is written every substep and
    // consumed by nobody, so the machine's dynamics are bit-identical with it
    // present -- which is exactly §7.7's shipping rule: the mechanism ships
    // measurable and unable to fire, the goldens are proven unmoved, and only
    // then is the capacity dialled down as a separate re-taped change.
    //
    // ⚠⚠ NOT IN THE POSITIONAL PIN ROSTER, AND IT MUST NOT BE. The tape pins
    // 41 doubles BY INDEX; adding one refuses every existing `.sledtape`, i.e.
    // the 45-drive corpus this rung's capacity is calibrated on. It re-converges
    // from tick 0 on replay from taped inputs alone (the `ws_exch_l` class), so
    // it needs no pin to be reproducible.
    GripState grip;
    // ★★★ K-WS1 / K2. THE ONE NEW PERSISTENT STATE THIS RUNG ADDS: last
    // substep's rider/machine EXCHANGE ANGULAR MOMENTUM, body axes
    // [kg m^2/s]. The airborne term spends its DELTA, so the previous value
    // has to be state -- but it is derived ENTIRELY from the (already-pinned)
    // rider_lat/fwd/up_m slew, so it is deterministic, input-driven and
    // tape-safe. It is written on EVERY substep, airborne or not, which is
    // what makes a takeoff pop impossible (the first airborne delta is taken
    // against the value the ground path already left here, not against a
    // stale one). It is CONSUMED only while airborne and only at
    // k_air_shift > 0, so the dial is bit-identical at 0.
    // NOT in the tape pin roster on purpose: adding it would change the
    // pin41 column count and break every existing .sledtape, and it is a
    // pure function of three fields that ARE pinned.
    glm::dvec3 ws_exch_l{0.0};
    // The rate-limited handlebar -- the RIG reads THIS, not `in.steer`
    // directly, so a render/audio consumer never has to re-slew it.
    double steer_actual = 0.0;  // [-1, 1]
    // Track SURFACE speed (the slip cue) -- the rig reads THIS, never ground
    // speed, because a decoupled coast at 0 belt speed while still moving IS
    // the 100 % slip an observer should see.
    double belt_speed_ms = 0.0;
    // Readout for dash/audio: idle 1700 .. 8000 at full track speed while the
    // clutch is engaged; idle when decoupled at closed throttle.
    double engine_rpm = 1700.0;

    // --- reported, never branched on ---------------------------------------
    // Fraction of the effective weight carried dynamically (§3.1). Continuous:
    // "still plowing some" means this never reaches a plateau where the
    // displacement terms are switched off.
    double plane_frac = 0.0;
    double track_slip = 0.0;
    // ★★ THE ONE NUMBER, TWO CONSUMERS (§3.5a). |slip| x available_snow. S5's
    // roost scales off THIS field and the escape thrust is computed FROM it --
    // fork them and the most legible feedback signal in the game starts lying.
    double roost_flux = 0.0;
    // G1: the rotor's angular momentum, SIGNED, along the body lateral axis
    // [kg m^2/s]. Signed and not a magnitude on purpose: when the effect is
    // reported to feel backwards, the sign of this readout is the first thing
    // the debug session needs, and |L| throws it away.
    double rotor_momentum_kgm2s = 0.0;
    // G2 carry: last substep's rotor momentum, so the reaction is a DELTA.
    // Written every substep in contact or not -- that is what makes the first
    // airborne delta zero instead of a takeoff pop (the ws_exch_l precedent).
    double gyro_prev_L = 0.0;
    double thrust_n = 0.0;
    double ground_speed_ms = 0.0;
    double depth_under_m = 0.0;
    world::Surface surface = world::Surface::Bush;
    // A rollover is EMERGENT (§2.4c.1) -- this is a READOUT of the attitude the
    // dynamics produced, never a trigger. Until S8 the run simply ends.
    // ★ RC (§0b): gated + persistent — true only after the attitude has held
    // rolled_persist_s while in ground contact (rolled_grace_s). A mid-air
    // send NEVER latches it and never cuts throttle (SledComfort, item D).
    bool rolled = false;
    // RC readout state (item D): attitude dwell + time since last contact.
    double rolled_hold_s = 0.0;
    double air_s = 0.0;
    // RC readout, never branched on by the kernel: the roll-assist torque
    // actually applied this step [N m] (dash/debug legibility).
    double assist_nm = 0.0;
    // ★ GI S3.4: the yaw-arrest torque's OWN gate, real state (0.1 s low-
    // passed side-load fraction; §2.4c.1's usual real-state discipline --
    // never re-derived from the instantaneous graze). Inert (reads but is
    // never consumed) when side_yaw_mu == 0.0.
    double hull_engage_lp = 0.0;
};

// ★ GI3/R3 — THE DEBUG SINK (SLED_TAPE_CONSULT_REPLY Q6, ruling (b)). The
// kernel's substep internals, WRITE-ONLY: step_sled fills one record per
// substep when handed a sink and NEVER reads one back — there is no feedback
// path, proven by the null-vs-filled bit-identity leg
// (sled_debug_sink_is_write_only, test/unit/test_sled.cpp), re-run every
// build, never asserted by this comment. Not part of the pin contract, not
// gate-visible state, not tape material (the tape stays inputs + params +
// pins + ground log only, so the sink can evolve without versioning it).
// Consumers: probe/replay only. The caller owns clearing; step_sled APPENDS
// p.substeps records per call.
// ★ GI4/§8.4 THE ROLL-AXIS ATTRIBUTION. Every roll moment the kernel makes
// accumulates anonymously into `torque_body.z` through `add_at`, so "which
// term holds the machine at -28 deg" was unanswerable without opening the
// integrator. These are BUCKETS ON THE SAME SUM: the kernel adds to
// torque_body exactly as before and, only when a sink is attached, ALSO adds
// the same z-component to the bucket named by the block it is in. Write-only,
// like the rest of the sink -- nothing in the kernel reads a bucket back, and
// the null-vs-filled bit-identity leg covers it.
enum RollTerm {
    kRollLift = 0,   // planing lift at the mount
    kRollNormal,     // suspension normal (incl. rail/pitch split)
    kRollFric,       // kinetic friction
    kRollPlow,       // displacement drag
    kRollBite,       // lateral mu bite
    kRollPlate,      // GI4 planing lateral plate (deferred second pass)
    kRollDrive,      // track thrust, engine brake, brake
    kRollHull,       // C1 hull side contact (normal + mu + shear)
    kRollGyro,       // G1 rotor precession (applied OUTSIDE torque_body)
    kRollTermCount
};

struct SledDebugSubstep {
    // The ground story this substep: per-patch normal load + surface class,
    // the assist's contact gate numerator, and the hull's side load.
    double patch_n[kPatches] = {0.0, 0.0, 0.0};
    int patch_surf[kPatches] = {0, 0, 0};
    double normal_sum = 0.0;
    double side_normal_sum = 0.0;
    // The rider story: slewed lean fraction and the B1 sign-matched align.
    double lean_frac = 0.0;
    double align_m = 0.0;
    // The roll story: assist-frame roll (vs the roll_ref_blend reference) and
    // surface-relative roll (the hull/C2 window's angle).
    double phi = 0.0;
    double phi_surf = 0.0;
    // The release band value actually applied [0,1] (after the B2 lean shift
    // and any GI3 floor), the contact gate, and the per-term torques [N m]
    // that accumulate anonymously through add_at/torque_body — the three
    // substep locals the tape consult names as unobservable without this.
    double release = 0.0;
    double w_contact = 0.0;
    double tq_assist = 0.0;
    double tq_c2_right = 0.0;
    double tq_c3_yaw = 0.0;
    // Horizontal ground-plane speed [m/s] — the quantity any speed-aware
    // comfort gate reads, recorded so a tuning session sees the gate's input.
    double v_ground = 0.0;
    // ★★★ R4a §7.7: THE GRIP, AT SUBSTEP RESOLUTION -- and it is here for a
    // reason a red-team had to point out. The replay hands the caller a state
    // once per TICK while the kernel steps the grip every SUBSTEP (12 of them
    // on his tapes), so a capacity read off the tick-level state is reading a
    // 1-in-12 SUBSAMPLE of the quantity the latch actually fires on -- and the
    // latch fires on peaks, which is exactly what a subsample loses. The first
    // capacity this rung published (75.72) was read that way, against the
    // probe's own written warning not to.
    //
    // Same write-only sink, same guarantee: nothing in the kernel reads these
    // back (`sled_debug_sink_is_write_only`).
    double grip_load = 0.0;
    // ★ THE FILTERED LOAD -- THE ONE THE CAPACITY IS ACTUALLY COMPARED TO.
    // Without it an instrument measures the raw product while the kernel
    // triggers on the filtered one, i.e. it reports a distribution of a
    // quantity that decides nothing. That is the class of error that produced
    // 75.72 (a tick subsample of a peak quantity) and it is not repeated here.
    double grip_load_lp = 0.0;
    double grip_extension_m = 0.0;
    // ★ GI4/§8.4: the roll-axis buckets above, the C3 yaw term's OWN roll
    // spill (it is applied about world up, which is not a body axis once the
    // machine is banked), and the body-axis angular velocity so the trace can
    // separate "a torque is holding it" from "it is still swinging".
    double roll_tq[kRollTermCount] = {0.0, 0.0, 0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0, 0.0};
    double tq_c3_roll = 0.0;
    double omega_body[3] = {0.0, 0.0, 0.0};
    // The GI4 plate's per-patch load share (the mean normalisation's own
    // multiplier, 1.0 when the normalisation is off) and the lateral plate
    // magnitude actually applied [N].
    double plate_share[kPatches] = {0.0, 0.0, 0.0};
    double plate_n[kPatches] = {0.0, 0.0, 0.0};
    // Per-patch planing lift [N] and suspension sink [m]. Lift reads
    // `sink_m`, never the patch's normal load, so a patch carrying nothing
    // can still make full lift -- the readout that makes that visible.
    double lift_n[kPatches] = {0.0, 0.0, 0.0};
    double sink_m[kPatches] = {0.0, 0.0, 0.0};
    // ★ R4a PHASE 0 (docs/R4A_PHASE0_MODEL.md §7.1) -- the two inertial
    // quantities the rider-load instrument needs and that nothing else in
    // this record carries. Both are BODY-AXIS and both belong to THIS
    // substep:
    //   a_body     inertial acceleration of the body-frame ORIGIN (which IS
    //              the system CG) = transpose(R) * (force / mass), taken
    //              AFTER gravity and air drag have entered `force`, i.e. the
    //              acceleration the integrator actually applies.
    //   alpha_body the substep's angular acceleration, the same
    //              (torque - omega x I omega) / I the integrator applies,
    //              read BEFORE angular_vel is advanced (`omega_body` above is
    //              the matching pre-update omega, so the triple
    //              (a_body, omega_body, alpha_body) is one consistent instant).
    // They are filled from a SECOND `if (dbg)` block further down, because at
    // the first sink write `force` still lacks gravity/drag and `alpha_body`
    // does not exist yet. Consumer: tools/sled_probe.cpp `probe_grip_hold`.
    // NOT taped (the pin roster is positional) and never read by the kernel.
    double a_body[3] = {0.0, 0.0, 0.0};
    double alpha_body[3] = {0.0, 0.0, 0.0};
};
struct SledDebugSink {
    std::vector<SledDebugSubstep> substeps;
};

// The plant. PURE. `snow` is the ONE ground (INV-1) -- this kernel makes
// exactly one ground call per patch per substep, through
// SnowpackField::sample_at, and contains no HeightField query of its own.
// `dbg` is the R3 write-only sink above — null in the game, filled only by
// probe/replay; the null path is the shipped path.
SledState step_sled(const SledState& state, const SledInputs& in,
                    const SledParams& p, const world::SnowpackField& snow,
                    double dt, SledDebugSink* dbg = nullptr);

}  // namespace sim
