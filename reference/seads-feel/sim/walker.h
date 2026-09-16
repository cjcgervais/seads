#pragma once
// ★★★ R4c — THE SUDBURIAN ON FOOT. LADDER §7.3 stages 4-7, §7.5.
//
// WHY THIS FILE IS IN `sim/` AND NOT IN `render/`.
//
// Stage 2 built the departure in `render/rider_flight.{h,cpp}`, and the
// argument was §7.7's own split: "render-side (no tape exposure): chain solve,
// blend weights, IK, tumble pose." That was right about his POSE and wrong
// about his BODY. A man who takes input, samples the snowpack, decides where
// he is on a sphere and hands control back to the machine when he remounts is
// not an animation — he is a second rigid body, and the moment R4c gave him a
// walk there would have been TWO homes for one man.
//
// So the free body MOVED HERE, verbatim, and render keeps the man it DRAWS.
// This is the same move stage 1 made when the arming memory went from
// `render/body_drive.*` into `sim/rider_grip.*`, for the same reason, and the
// lesson it is paying forward is the one that rung wrote down: one law, one
// home, and a transcribed second copy is how a rig stops describing the code.
//
// ⚠ HE IS NOT IN THE SLED TAPE, AND THAT IS DELIBERATE. The tape is a MACHINE
// tape: it pins `SledState` by index and replays the machine. The walker does
// not move the machine (nothing in `sim/sled.cpp` reads one line of this file),
// so a drive replays bit-exactly whether or not a man was walking beside it.
// The day he can push the machine — §7.8's righting, R4d — that stops being
// true and the tape has to grow. It has not grown yet.
//
// ---------------------------------------------------------------------------
// ★★★ CHAD'S RULING, 2026-08-31, VERBATIM — THIS IS THE SPEC AND NOT A GLOSS:
//
//   "he should dissapear and / reappear in deepest snow in a poof and then get
//    up from being prone or supine to crawl then large stepping with snow
//    coming off walk in deep snow, quick gait on hardpack and then the
//    intermediate midly handicapped by about a foot or two, but slowed."
//
// and, on the same day, re-stating the ladder's own rule so it could not be
// missed: *"Its quality / speed is determined by snow depth."*
//
// ★★★ SO DEPTH IS THE ONE INDEPENDENT VARIABLE, AND IT DRIVES BOTH. Not a
// surface CLASS — depth. §7.5's table reads "Deep / sinkable (Bush)" versus
// "Trail or road (packed)", and it is tempting to switch on `world::Surface`;
// but the corridor override already zeroes the depth on plowed trails and
// roads (§2.3), so the depth field ALREADY carries the class distinction, and
// it carries it CONTINUOUSLY. A class switch would be a binary read of a
// continuous quantity, which is this ladder's own recorded disease — three
// rungs, three times. Every DIAL in this file is continuous in depth for that
// reason.
//
// ⚠ AND THE ONE PLACE THAT IS NOT, NAMED RATHER THAN CLAIMED AWAY: choosing
// between `Buried` and `Down` at the landing is a branch on
// `bury_frac * frac > 0`, and a mode is a discrete thing however continuously
// its DURATION is scaled. The observable consequence is discrete too -- the
// poof count steps 1 -> 2 as depth crosses zero. It is defensible (a man is
// either under the snow or he is not, and the stage lasts exactly zero seconds
// at zero depth, so nothing jumps in TIME) -- but an earlier version of this
// banner said flatly "there is no threshold in this file", and that was
// advocacy. Red-team, 2026-09-01.
//
// ★★★ AND HIS THREE ANCHORS ARE MEASUREMENTS, NOT ENDPOINTS I PICKED:
//
//   hardpack       depth ~0        "quick gait"
//   intermediate   "about a foot or two"  = 0.30-0.61 m, so 0.45 m at centre,
//                                  "midly handicapped ... but slowed"
//   deepest        the SIGNED 0.77 m depth law   "large stepping", and he goes
//                                  UNDER it first
//
// Read out piecewise-linearly between those three, exactly the way
// `sim/sled.h`'s `kAftCeilC1` reads its measured aft-ceiling row out of five
// slices. That idiom already ships here; this is not a new curve family, and
// the two ends are not a lerp somebody drew through the middle.

#include <glm/vec3.hpp>

#include "world/snowpack.h"

namespace sim {

// §7.3's chain, as the states a man can actually be in. The ORDER is the feel
// (§7.3: "stages 0-3 are continuous and reversible ... stage 4 is one-way"),
// so these advance and, apart from the remount, do not go back.
enum class WalkerMode : int {
    Riding = 0,  // on the machine. Nothing in this file runs.
    Falling,     // §7.3 stage 4-5: a free body, in the air
    Buried,      // ★ HIS RULING: gone INTO the deepest snow. Not drawn.
    Down,        // §7.3 stage 6: prone or supine on the surface, not moving
    // ★★★ TWO CRAWLS, AND CHAD RULED THE ORDER, 2026-09-01: "they should
    // start coming out by crawling prone, crawling on knees then walking
    // through deep snow." A single `Crawling` stage ran the WALK cycle on a
    // man pitched face-down -- which is exactly the "legs animated like a
    // wheel" he saw. Getting out of deep snow is three motions, not one.
    CrawlProne,  // on his belly, dragging himself clear
    CrawlKnees,  // up on hands and knees
    Afoot,       // §7.3 stage 7: up and moving, on the gait ladder
};

struct WalkerParams {
    // --- THE DEPARTURE (moved verbatim from render/rider_flight.h; every
    // number and every comment there survives, and the tests moved with it) ---

    // ★ CHAD'S DIAL: m/s of sideways departure per metre of the lateral he
    // already had when the grip broke (Q1, 2026-08-30 — "whichever way he's
    // already leaning out"). DERIVED, not measured: clear a 0.4 m half-width
    // machine inside the ~1 s a landing-height departure gives him, at a
    // typical 0.2 m of lean. §7.7a says only his eye can set it. 0 = off.
    double lat_gain_per_s = 2.0;
    // The same drag the drawn body already flies at, single-sourced by value
    // at the call site. rho Cd A / 2m for an 87.5 kg body — a DERIVATION, not
    // a measurement off him (handoff §5.4).
    double drag_k_per_m = 0.005;
    double gravity_mps2 = 9.80665;
    // ★★★ THE SKID, AND IT IS COULOMB -- MEASURED, NOT A DECAY RATE.
    //
    // The first version bled his speed off with a first-order exponential at
    // 6 /s. Chad drove it: "he needs to maintain some forward velocity / skid
    // upon falling off." A consult then measured what the exponential was
    // actually doing: from 15 m/s it stops him in **2.5 m**, which is very
    // nearly the right answer for DEEP LOOSE POWDER (3.2 m) and an order of
    // magnitude short of the right answer for a PACKED TRAIL (29 m). The
    // powder answer had been applied to every surface.
    //
    // ★ AND THE SHAPE WAS WRONG, WHICH IS THE HALF THAT MATTERS MOST. Sliding
    // friction on snow is DRY friction -- the measurement below says so in as
    // many words -- so it is COULOMB: a constant deceleration, giving
    //     d = v^2 / (2 mu g)
    // which is QUADRATIC in the arrival speed. An exponential's distance is
    // v/lambda, LINEAR in it. So under the old law doubling the crash speed
    // only doubled the skid, when it should very nearly quadruple it -- and
    // "fast crashes do not skid proportionally" is exactly the thing an eye
    // notices. Coulomb also stops him at a definite time and distance, so the
    // rest-speed cutoff the exponential needed to terminate is gone with it.
    //
    // ★★★ THE NUMBERS ARE MEASURED ON A REAL BODY, NOT PICKED. Nachbauer,
    // Moessner, Rohm, Schindelwig & Hasler, "Kinetic Friction of Sport Fabrics
    // on Snow", Lubricants 2016, 4(1), 7 -- a linear tribometer at -4.3 C on
    // 400 kg/m^3 snow, plus that group's WHOLE-BODY slide measurement (a real
    // 80 kg skier, mu back-calculated from video-tracked CoM acceleration):
    //
    //     ski overall on snow      mu = 0.40   <- MEASURED, whole body
    //     downhill racing suit     mu = 0.38
    //     slick shell              mu ~ 0.30
    //     coarse outerwear         mu ~ 0.50
    //
    // ⚠ AND A BODY IS NOT A SKI. `SurfaceDials::mu_kin` in sim/sled.h is the
    // MACHINE's number (TrailMain 0.032) and is 10-15x smaller. Reusing it
    // here would be the anti-fork rule misapplied: these are two different
    // physical contacts, and this table is not a second opinion about the
    // machine's.
    //
    // ⚠ THE ICE AND ROAD ROWS ARE WEAKER EVIDENCE AND ARE LABELLED SO. No
    // published fabric-on-ice coefficient exists; 0.07 for bare ice is derived
    // from a reported person-slides-on-a-rink experiment (93 m in 16.6 s) and
    // its internal consistency is imperfect. Road: 0.70 is well-sourced for
    // BARE DRY ASPHALT (pedestrian-throw reconstruction, Searle; motorcycle
    // reconstruction 0.57-0.85), but a PLOWED WINTER road is sanded and
    // patchy, so 0.45 is an interpolation between that and the ice figure --
    // an inference, not a measurement.
    double mu_bush = 0.55;         // body in/on loose snow (coarse outerwear end)
    double mu_trail = 0.40;        // ★ THE MEASURED ONE
    double mu_road = 0.45;         // inferred, see above
    double mu_ice = 0.15;          // snow-dusted lake ice; bare glare ice ~0.07
    // ★ THE PLOUGH, and it is the SAME PHYSICS THE MACHINE ALREADY USES: the
    // sled's own sinkage drag is 0.5 * rho_eff * v^2 * a_sub * plow_cd
    // (sim/sled.cpp). A body ploughing snow is that expression with the body's
    // submerged frontal area. The consult derived c ~ 30 kg/m from first
    // principles and the sled's own Bush constants give 29.9 -- the same
    // number twice, which is why nothing new is invented here.
    //
    // It is QUADRATIC in speed and gated on immersion, so it dominates the
    // first fraction of a second in powder and is STRUCTURALLY ZERO on a
    // plowed road where there is nothing to plough.
    double plow_rho = 260.0;       // kg/m^3, the Bush rho_eff the sled ships
    double plow_cd = 1.15;         // the sled's own bluff-body coefficient
    double body_width_m = 0.50;    // a man lying down, presented edge-on
    double body_immersion_max_m = 0.40;  // he does not plough deeper than this
    double body_mass_kg = 95.0;    // 87.5 kg of him plus the suit and boots
    // How far his drawn origin rides over the drive surface when he is on it.
    // Set from the machine's own `cg_height_m` at the call site.
    double lie_clearance_m = 0.564;

    // --- THE DEPTH LADDER (his ruling, above) -------------------------------

    // The three anchors, in metres of snow. `mid` is the centre of his own
    // "about a foot or two" (0.30-0.61 m); `deep` is THE SIGNED DEPTH LAW,
    // 0.77 m, which is a number this program has already signed and must not
    // re-derive here.
    double depth_mid_m = 0.45;
    double depth_deep_m = 0.77;
    // What he can do at each. ⚠ THESE THREE ARE HUMAN FIGURES, NOT REPO
    // MEASUREMENTS, and they are labelled so rather than dressed up: postholing
    // in thigh-to-hip-deep snow is a well-known ~0.5 m/s slog; the middle is
    // his "slowed". Chad's eye sets them — SEADS_WALK_SPEEDS="hard,mid,deep".
    //
    // ★★★ CHAD'S RULING, GAIT LADDER G2 (PLAN_20260904_gait_ladder.md §1.1),
    // 2026-09-04, VERBATIM: "RUN = 5-6 m/s on hard pack." The old anchor, 3.5,
    // was a fit adult's JOG, not the "up and running" §7.5 asks for; 5.5 sits
    // at the centre of his stated band. The mid/deep anchors are untouched --
    // `row3`'s piecewise-linear read is continuous at every anchor by
    // construction regardless of an endpoint's value, so this is the ONLY
    // number this ruling moves (walker_stance_frac's own measured curve
    // saturates at its jog-ceiling value, 0.38, for every speed at or past its
    // 3.5 m/s upper anchor -- unchanged and unextended, since no measurement
    // exists past it; 0.38 stance already IS a real flight phase, so nothing
    // here needed inventing a fourth number to reach the ruling).
    double speed_hardpack_mps = 5.5;
    double speed_mid_mps = 1.6;
    double speed_deep_mps = 0.6;
    // ★★★ THE STRIDE IS NOW DERIVED FROM A MEASURED HUMAN INVARIANT, AND THE
    // THREE ANCHORS I HAD PICKED ARE GONE.
    //
    // They were wrong by up to 2.5x and the error was arithmetic, not taste.
    // The header used to reason "3.5 / 0.90 = 3.9 steps/s, a jog" -- but the
    // two legs are half a cycle apart WITHIN one phase cycle, so ONE CYCLE IS
    // TWO STEPS. The real cadence was 7.8 steps/s = 468 steps/min, against a
    // human sprint maximum of ~260. He was pinwheeling at 2.7x human rate,
    // which is "walks like a robot" all by itself and cannot be covered up.
    //
    // ★ THE WALK RATIO. Step length divided by cadence is very nearly constant
    // across speed in adults: 0.0060 +/- 0.0006 m/(steps.min^-1) -- Sekiya &
    // Nagasaki, "Reproducibility of the walk ratio", Gait & Posture 1998;
    // 7:225-227. It is a MEASURED invariant of human locomotion, not a curve
    // family. Solving it for speed gives all three anchors from one constant:
    //
    //     cadence = 100*sqrt(v) steps/min      stride = 1.20*sqrt(v) m
    //
    // Cross-checked against normative gait at free speed: v = 1.4 predicts 118
    // steps/min and a 1.42 m stride, against Perry & Burnfield's measured 113
    // and 1.44 -- inside 4 %.
    double walk_ratio = 0.0060;
    // ★ AND DEEP SNOW STRETCHES THE RATIO RATHER THAN BREAKING THE LAW: each
    // step costs a hole, so he buys a longer step at a disproportionately lower
    // cadence. THIS is where "large stepping ... in deep snow" now comes from,
    // and it is one multiplier on a measured invariant instead of three numbers
    // somebody chose. The multiplier itself is a LOOK-DIAL.
    double walk_ratio_deep_gain = 1.0;
    // ★★★ THE FOOT LIFT IS CAPPED BY WHAT HIS LEG CAN ACTUALLY DO. The first
    // version asked for `0.08 + depth`, i.e. 0.85 m of foot rise at the signed
    // depth law -- but the drawn leg is calf 0.453 + foot 0.455 = 0.908 m from
    // the hip (render/rider_rig.cpp, MEASURED), so that demands the leg fold to
    // a 0.1 m vertical span, knee-to-chest, on every step at 0.6 m/s. What it
    // actually produced is worse than ugly: the render IK CLAMPS its target
    // into the chain's reach, and a clamped two-bone chain is a fully extended
    // STRUT. That is the most literal possible source of "robot".
    //
    // The physical truth is that deep-snow travel is WADING, not stepping over:
    // the foot clears part way and is shoved through, and the body pays the
    // rest by sinking. So the lift saturates at what the leg can reach.
    double lift_base_m = 0.08;
    double lift_max_m = 0.45;

    // --- GETTING UP (§7.5, and the two numbers are the spec's own) ----------
    // §7.5: "the same crash costs you fifteen seconds in the bush and two on
    // the trail, and the player learns that from the snow, not from a UI."
    double rise_s_hardpack = 2.0;
    double rise_s_deep = 15.0;
    // ★ HIS RULING: "dissapear and / reappear in deepest snow in a poof". He
    // goes UNDER when the snow is deep enough to take him, and the fraction of
    // the rise he spends out of sight, and then crawling, are the shape of
    // "prone or supine to crawl". Both are fractions of the rise the depth
    // already set, so the whole sequence stretches with the snow by
    // construction and there is one clock, not three.
    //
    // ⚠ NOT A THRESHOLD ON DEPTH: `bury_frac`/`crawl_frac` are scaled by the
    // SAME continuous depth ratio the speed row uses, so in hardpack he spends
    // exactly zero time buried — structurally, not "almost none".
    double bury_frac = 0.25;
    double crawl_frac = 0.35;
    // How the crawl budget splits between the belly and the knees. The belly
    // comes first and is the slower half of getting out of a hole.
    double crawl_prone_share = 0.55;

    // --- ON FOOT ------------------------------------------------------------
    // A man pivots quickly; this is a feel dial and it is his.
    double turn_rate_rps = 2.5;
    // He is 87.5 kg in boots: he does not reach speed instantly and he does
    // not stop dead. One time constant, the house `slew_toward` idiom.
    double accel_tau_s = 0.35;
};

struct WalkerState {
    WalkerMode mode = WalkerMode::Riding;
    double t_mode_s = 0.0;   // seconds in the current mode
    double rise_s = 0.0;     // the whole get-up budget the snow bought him
    // WORLD, and DOUBLES: the drive surface sits a planet radius (15 km) from
    // the origin and a float there is metres of slop (CLAUDE.md sphere
    // invariants).
    glm::dvec3 pos{0.0};
    glm::dvec3 vel{0.0};
    // Unit, and tangent to the sphere at `pos`. Re-orthogonalised every step,
    // because "forward" on a ball is not a constant.
    glm::dvec3 heading{0.0};
    // ★ ADVANCED BY DISTANCE, NEVER BY TIME. Feet that move on a clock skate
    // whenever the body is not travelling at the speed the clock assumed; feet
    // that move on distance cannot, at any frame rate, on any slope.
    double gait_phase = 0.0;  // [0,1)
    // What the snow under him is doing, published for the drawn man so render
    // never runs a second ground query and never disagrees about the depth.
    double depth_m = 0.0;
    double speed_cap_mps = 0.0;
    double stride_m = 0.0;
    // ★ ONE-SHOT EDGES for the app: true on exactly the step the event
    // happens, so a consumer cannot miss it and cannot fire it twice. `poof`
    // is §7.5's snow burst (reuse the roost/spray path, §0.3 — never a second
    // particle system); `landed` is the fall that dents the helmet.
    bool poof = false;
    bool landed = false;
    // ★ HOW FAST HE ARRIVED, published on the landing step. The burst's size
    // is a function of impact speed, and `vel` is zeroed of its inward
    // component by the very step that raises the flag -- so a consumer reading
    // the velocity AFTER the step gets a number that describes the aftermath,
    // not the impact. That is a silent-zero trap and this field is what closes
    // it.
    double poof_speed_mps = 0.0;
    // ★★★ HOW FAR UNDER THE SNOW HE IS, [0,1]: 0 = fully out, 1 = gone.
    //
    // REPORTED, NEVER DRIVING -- nothing in this file reads it back, exactly
    // like `depth_m` / `speed_cap_mps` / `stride_m` above. It exists because
    // Chad drove the burial and said "im buried in the deep snow": the mode was
    // right and there was nothing to SEE, because the drawn man is pinned at
    // `drive_r + lie_clearance_m` and the kernel never lowered him -- so
    // `Buried` drew as a man lying flat, HOVERING half a metre over the snow.
    //
    // ⚠ IT IS NOT A SECOND CLOCK. It is a shape over the Buried stage whose
    // duration the depth already bought, so it stretches with the snow by
    // construction and cannot drift against the get-up sequence.
    double submerge = 0.0;
};

struct WalkerInputs {
    // +1 forward, -1 back. The SAME keys the machine uses, so his hands do not
    // change when his body does.
    float forward = 0.0f;
    // +1 left, matching the kernel's steer convention (sim/sled.h: steer +1 =
    // LEFT). One sign convention in this program, not two.
    float turn = 0.0f;
};

// ---------------------------------------------------------------------------
// The depth ladder, read out piecewise-linearly over his three anchors. Pure,
// so a test can walk the whole row without a machine or a world.
//
// `depth_m` below 0 clamps to hardpack and above `depth_deep_m` clamps to
// deep: snow deeper than the signed law does not make him slower without
// bound, because the law is what "deep" MEANS here.
double walker_depth_frac(const WalkerParams& p, double depth_m);
double walker_speed_cap(const WalkerParams& p, double depth_m);
double walker_stride(const WalkerParams& p, double depth_m);
// The share of the cycle a foot is on the ground. ★ It is NOT 0.5: at 0.5 the
// exchange is instantaneous and there is no double support, which is the
// topology of a RUN. Measured across the walk-run transition by Nilsson &
// Thorstensson, Acta Physiol Scand 1989; 136:217-227 -- 0.68 at a 0.6 m/s
// trudge (36 % double support), 0.62 at a free walk, 0.50 at the transition,
// 0.38 at a 3.5 m/s jog (24 % flight). Continuous in speed, so the walk
// becomes a run without a threshold anywhere.
double walker_stance_frac(const WalkerParams& p, double speed_mps);
// How high the swing foot clears, capped by what the leg can reach.
double walker_lift(const WalkerParams& p, double depth_m);
double walker_rise_s(const WalkerParams& p, double depth_m);
// The rider's friction against the ground he is on, blended across the class
// boundary the way `sample_at` already publishes it (SK-1a) rather than
// stepped. Pure, so the whole table is walkable from a test.
double walker_mu(const WalkerParams& p,
                 const world::SnowpackField::GroundSample& g);

// Seed him at the instant the grip breaks. Call ONCE per fall: `mode` is the
// guard, and re-seeding a man already in the air would teleport him.
//
//   pos_w      his drawn origin (world)
//   vel_w      the MACHINE's world velocity. He is riding it, so it is his —
//              and NOTHING FORWARD IS ADDED. The measured 8-of-10
//              over-the-bars departure is emergent: the landing is the machine
//              shedding speed into the snowpack while he is a free body.
//   lat_axis_w the machine's own +X (right) in world, unit
//   lat_lean_m how far he is already leaning out, SIGNED in that axis. Chad's
//              Q1, and the only directional input the departure has.
void walker_throw(WalkerState& s, const WalkerParams& p,
                  const glm::dvec3& pos_w, const glm::dvec3& vel_w,
                  const glm::dvec3& lat_axis_w, double lat_lean_m);

// Put him back on the machine (R4e retires the KEY_R scaffold that does this
// today). One-way in the other direction: the state ends with him.
void walker_remount(WalkerState& s);

// ---------------------------------------------------------------------------
// ★★★ THE GAIT, AS A PURE FUNCTION OF THE PHASE THE STEP ADVANCED.
//
// It lives HERE, beside the state that drives it, and not in
// `render/sled_model.cpp` — that TU is compiled only into the `seads`
// executable, so a foot curve living there could not be executed by a test,
// and this ladder has lost two rungs to exactly that. What render owns is
// putting the offsets on the rig; what the offsets ARE is gradeable, and
// graded here.
//
// One leg, one phase, in the man's OWN frame:
//   `fwd_m`  + is ahead of him. Half a stride each way, so a full cycle is one
//            stride for that leg.
//   `up_m`   >= 0, and exactly 0 through the whole of stance. A foot that
//            leaves the ground while it is bearing weight is the floaty
//            weightless motion §0.4 exists to prevent.
//
// `phase` in [0,1): [0, 0.5) is STANCE — the foot is planted and slides
// backwards under him at exactly the rate he travels, which is what makes it
// planted rather than skating — and [0.5, 1) is SWING.
//
// ★ `lift_m` IS WHERE HIS "large stepping" LIVES. The swing arc's height is
// handed in rather than computed here, because the thing it has to clear is
// the SNOW, and how deep that is belongs to the caller that just sampled it.
void walker_foot_offset(double phase, double stride_m, double lift_m,
                        double stance_frac, double* fwd_m, double* up_m);

// One step of the man. `f` is the same snowpack the machine drives on — a foot
// is just another contact patch (§R5+), and `sample_at` is the one ground query
// that already serves one.
WalkerState step_walker(const WalkerState& s, const WalkerInputs& in,
                        const WalkerParams& p, const world::SnowpackField& f,
                        double dt_s);

}  // namespace sim
