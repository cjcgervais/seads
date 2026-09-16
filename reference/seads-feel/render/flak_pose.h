#pragma once
// ★ F-POSE -- THE GUNNER'S SKELETON SOLVE (pure, render_core).
//
// docs/FLAK_GUN_SPEC.md §7.6: the Sudburian ON the gun. Hands welded to
// st_grip_l/r, shoulders to st_pad_l/r, feet on the deck -- and the P1-9
// RULING (Chad, 2026-08-30): a CROUCH CURVE, refined the same day by
// ★P1-9b: FEET STEP IN. As elevation rises the pads swing down about the
// trunnion, his feet shuffle forward along the deck toward the pedestal,
// and his knees and hips solve the drop OVER HIS OWN FEET. Not a seat
// (round 1 sat him on the snow at +87 -- exactly what P1-9 ruled out),
// not a rigid plank lean.
//
// FRAME. Everything here is solved in the GUN MODEL FRAME (+Z = muzzle
// forward, +X = gunner's right at rest, origin = pedestal base on the
// ground) at train = 0. The whole man is a TRAIN rider -- he walks round the
// pedestal with the gun and rotates rigidly with flak_train -- so the drawer
// maps every joint through train_quat + the mount frame exactly as
// train_station_world() does. Elevation is the only pose parameter; no world
// axis is assumed anywhere (the up = normalize(1,1,1) discipline of
// test_flak_gun applies to the world mapping, and the solve never sees it).
//
// ★ ANTHROPOMETRY ARRIVES, IT IS NEVER TYPED. GunnerAnthro is measured by
// the loader off the shipped rig's rest joints (render/flak_gunner.cpp) --
// the one-number rule. The unit test feeds a synthetic rod-man and grades
// PROPERTIES (welds hold, the stance holds in plan, never a sit, upright
// through the mid elevations, no limb stretches, the helmet off the
// breech), which must hold for ANY admissible anthro.
//
// PURE: glm + cmath only. No raylib, no asset, no getenv, no clock.

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/flak_gun.h"

namespace render::flak {

// Segment lengths + offsets of the man, measured off the rig at rest (bone
// lengths are pose-invariant, so rest is the honest place to take them).
struct GunnerAnthro {
    double torso_m = 0.523;      // pelvis -> shoulder-line midpoint
    double thigh_m = 0.4526;     // hip -> knee
    double calf_m = 0.455;       // knee -> ankle
    double uarm_m = 0.345;       // shoulder -> elbow
    double farm_m = 0.270;       // elbow -> wrist
    double hip_half_m = 0.095;   // pelvis -> hip joint, lateral
    // ★ P1-10: the shoulders are no longer welded to the pads, so the rig's
    // OWN shoulder width sets them (loader-measured off the upperarm joints).
    double shoulder_half_m = 0.21;  // shoulder-line mid -> shoulder joint
    // ★★★ WHICH SIDE THE RIG CALLS LEFT, as a SIGN on model +X, measured off
    // the rig's own upperarm joints. The gun GLB names its stations _l/_r off
    // the MOUNT's convention and the rig names its bones off the MAN's, and
    // THE TWO DISAGREE: st_pad_l sits at x = -0.235 while upperarm_l sits at
    // x = +0.21. Naming cannot resolve that -- only geometry can, so the
    // solve pairs hand to pad by SIDE and never by suffix.
    double left_sign = 1.0;  // +1 = the rig's left limbs are on model +X
    double neck_m = 0.095;       // neck base -> head joint
    double ankle_up_m = 0.10;    // boot sole -> ankle joint
    // ★ How far the DRAWN head reaches forward of the head joint along the
    // face axis -- the helmet front. Measured by the loader over the vertices
    // the rig binds to the head bone; this is the point that ends up in the
    // breech if the crane over-reaches, so the head is aimed by it and not by
    // the joint (Chad, 2026-08-31: "smushing his face into the back of the
    // flak gun"). The declared default is a helmeted head's half-depth.
    double face_fwd_m = 0.12;
};

// The solved skeleton, gun model frame, train = 0. Sides are SOLVED
// INDEPENDENTLY against their own named stations -- sides never mirror.
struct GunnerJoints {
    glm::dvec3 pelvis{0.0};
    glm::dvec3 spine_01{0.0}, spine_02{0.0}, spine_03{0.0};
    glm::dvec3 neck{0.0}, head{0.0};
    glm::dvec3 clavicle_l{0.0}, clavicle_r{0.0};
    glm::dvec3 shoulder_l{0.0}, shoulder_r{0.0};  // upperarm joints
    glm::dvec3 elbow_l{0.0}, elbow_r{0.0};
    glm::dvec3 hand_l{0.0}, hand_r{0.0};
    glm::dvec3 hip_l{0.0}, hip_r{0.0};
    glm::dvec3 knee_l{0.0}, knee_r{0.0};
    glm::dvec3 ankle_l{0.0}, ankle_r{0.0};
    glm::dvec3 ball_l{0.0}, ball_r{0.0};
    glm::dvec3 look_dir{0.0, 0.0, 1.0};  // the sight axis under elevation
};

// ★★★ P1-10 -- THE PADS ARE HANDLES (Chad, 2026-09-01: "JUST HAVE TO LET
// THE MAN STEP BACK AND HOLD THE SHOULER PADS LIKE THEY ARE HANDLES").
//
// THE RULING THAT REPLACED A RUNG. Rounds 1-4 welded his SHOULDERS into
// st_pad_l/r and his hands onto the spade grips, and every problem after
// that came out of one measured fact: HE IS TOO SHORT FOR THAT. His chain
// (ankle 0.10 + legs 0.908 + torso 0.523 = 1.531 m dead straight, and nobody
// stands dead straight) cannot reach a 1.643 m pad line, let alone put his
// eye at the 1.783 m sight. kShoulderPadOff's -0.14 existed ONLY to absorb
// that, which pinned his head at bore height right behind the breech -- the
// 2026-08-31 smush. The hardware answer was the Mk 4 jack-screw: lower the
// mount, re-bake the GLB, move a signed number. ★Chad ruled the other way
// and it is better: he STANDS BACK and GRIPS THE PADS LIKE HANDLEBARS. The
// stature mismatch stops being a defect and becomes the posture.
//
// So the weld moved: HANDS ride the pads, and the shoulders are SOLVED --
// an arm's working reach behind and below the handles, at a shoulder height
// he can actually stand at, crouching only when the handles come down. He is
// clear of the metal by construction, not by a clearance dial.
//
// Palm centre relative to the PAD station, cradle-local: he takes the pad
// from behind and just under its face, the way a hand takes a bar.
inline const glm::dvec3 kHandPadOff{0.0, -0.02, -0.06};
// The arms are never straight-locked at the elbow (IK singularity guard and
// the reason he reads as HOLDING the gun rather than hanging off it): the
// shoulder is placed exactly this fraction of a full arm from the handle, so
// the reach is CONSTANT through the whole elevation curve by construction.
inline constexpr double kArmWorkFrac = 0.88;
// ...and he never stands on top of his own hands. When the handles descend
// past what a crouch can follow, this is the fore-aft standoff that is kept
// and the shoulder drops instead.
inline constexpr double kStandoffMinM = 0.25;
// ★ THE GUNNER'S CROUCH AT THE STAND (Chad, 2026-09-01: "he needs to bend
// his knees and bring his shoulders down too. His shoulders are too high").
// He is not standing to attention holding a bar -- he is BRACED on it, knees
// bent, weight down. This is the fraction of a straight leg the shoulder
// line rides at when the handles are high enough to allow any choice; the
// knee closes to about 120 deg there. Strictly below kLegReachFrac so the
// height it yields is always inside the legs' reach -- the overstretch the
// first run of test_flak_pose caught cannot recur.
inline constexpr double kStandLegFrac = 0.85;
// The ankles sit this far FORWARD of the shoulder line in plan (a standing
// man's feet are under him, a shade ahead when he leans on something). The
// fore-aft of the stance is DERIVED now -- st_foot_l/r keep the stance
// WIDTH, which is theirs to give, but their z was the 0 deg position of a
// man standing IN the pads and that man no longer exists.
inline constexpr double kFootAheadOfShoulderZ = 0.05;
// The pelvis NEVER sits: hard floor above the deck plane, and it stays
// over the stepped feet in plan (both graded by test_flak_pose).
inline constexpr double kPelvisDeckMinM = 0.26;
inline constexpr double kHipOverFootM = 0.12;
// ... and out of the pedestal column (radius 0.19 + half a body).
inline constexpr double kPelvisAftLimitZ = -0.30;
// Legs are never asked for their last millimetre (IK singularity guard).
inline constexpr double kLegReachFrac = 0.985;
// ★★★ EYE RELIEF (Chad, 2026-08-31: "he is smushing his face into the back
// of the flak gun, pressed right up against it"). st_eye is where the EYE
// belongs, and the head was craned so that the JOINT reached for it -- which
// drove the drawn face 0.12 m further forward again, into the aft box of
// flak_gun and inside flak_rest. The crane now aims the point that becomes
// his FACE at a spot this far BEHIND st_eye, so the helmet stays out of the
// metal. A clearance number, not an optical one: the mount is built at
// trunnion 1.543 m and the measured Sudburian's shoulder line tops out near
// 1.53 m, so his eye cannot reach the sight height at all -- see the
// jack-screw finding in the handoff. This keeps him off the gun; it does not
// make him tall enough for it.
inline constexpr double kEyeReliefM = 0.35;
// ★★★ THE HEAD SITS ON THE NECK -- it LEANS toward the sight, it never
// REPLACES the neck's up. Standing back (P1-10), the pulled-back aim point
// can fall BELOW the shoulder line once the gun is up, and the unclamped
// crane then hung his head UNDER his own shoulders -- and, because the rig's
// scarf chains ride neck_01 and neck_01 is aimed at the head, threw the
// scarf straight up out of his helmet (Chad, 2026-09-01: "his head appears
// low compared to shoulders ... the scarf is sticking straight up out of the
// sudburains helmet"). ★ONE UNBOUNDED AIM, THREE SYMPTOMS IN THREE DIFFERENT
// PARTS OF THE BODY. The lean is capped here instead.
inline constexpr double kHeadLeanMaxRad = 25.0 * kDeg;
// ★ THE CEILING THIS CRANE HAS. The head is tethered a.neck_m from the neck,
// so the furthest back the drawn face can ever go is
// (neck.z - neck_m + face_fwd_m): a helmet reaching more than ~0.285 m
// forward of its joint CANNOT be cleared of flak_gun's aft box by ANY aim,
// and would need the mount lowered (the jack-screw) or the head detached
// from the pads. The shipped rig measures 0.2087 m, inside the ceiling with
// ~34 mm to spare, and test_flak_pose grades the whole admissible band.
// At DEPRESSION the pads ride UP and the measured chain runs out -- the
// real gunner goes to his toes. Both heels may rise together up to this
// cap; the feet never leave their stations in plan (xz stays exact).
inline constexpr double kToeRiseMaxM = 0.09;

// A cradle-frame point carried through the elevation (about the trunnion).
inline glm::dvec3 elev_point(const Stations& s, double elev_rad,
                             const glm::dvec3& p) {
    const glm::dvec3 tr(0.0, s.trunnion_h, 0.0);
    return cradle_quat(elev_rad) * (p - tr) + tr;
}

// Closed-form two-bone IK: mid joint between root and target, lengths a/b,
// bend pushed toward `hint` (unit-ish, model frame). Overreach clamps the
// target onto the reachable sphere; the caller's own reach clamp should
// make that branch rare.
inline glm::dvec3 two_bone_mid(const glm::dvec3& root, const glm::dvec3& tgt,
                               double a, double b, const glm::dvec3& hint) {
    glm::dvec3 d = tgt - root;
    double L = glm::length(d);
    const double lmax = (a + b) * 0.99999;
    if (L < 1e-9) return root + hint * a;
    if (L > lmax) L = lmax;
    d /= glm::length(tgt - root);
    // bend plane normal from the hint, orthogonalised against the chain
    glm::dvec3 side = hint - d * glm::dot(hint, d);
    const double sl = glm::length(side);
    side = sl > 1e-9 ? side / sl
                     : glm::dvec3(1, 0, 0) -
                           d * glm::dot(glm::dvec3(1, 0, 0), d);
    side = glm::normalize(side);
    // law of cosines: distance of the mid joint along the chain + off it
    const double x = (a * a - b * b + L * L) / (2.0 * L);
    const double h2 = a * a - x * x;
    const double h = h2 > 0.0 ? std::sqrt(h2) : 0.0;
    return root + d * x + side * h;
}

// THE SOLVE. Elevation is clamped to the mount's own limits first.
inline GunnerJoints gunner_solve(const Stations& s, double elev_rad,
                                 const GunnerAnthro& a) {
    const double e = clamp_elev(elev_rad);
    GunnerJoints g;

    // --- THE WELD (P1-10): his HANDS take the pads, as handlebars. This is
    // the only thing tying the man to the gun now -- the shoulders are free.
    // ★★★ HIS LEFT HAND TAKES THE PAD ON HIS LEFT (Chad, 2026-09-01: "need to
    // swap his hands for the flak gun only"). The suffixes lie -- see
    // GunnerAnthro::left_sign -- so the pairing is decided by which SIDE each
    // pad is on, against the side the rig itself puts its left arm. Get this
    // wrong and his left arm, sleeve and thumbed mitt are drawn across on his
    // right. ⚠This is the FLAK pose only; the sled rider's hands are welded
    // elsewhere and are NOT touched by any of this.
    const bool pads_are_swapped = (s.pad_l.x - s.pad_r.x) * a.left_sign < 0.0;
    const glm::dvec3 pad_for_l = pads_are_swapped ? s.pad_r : s.pad_l;
    const glm::dvec3 pad_for_r = pads_are_swapped ? s.pad_l : s.pad_r;
    g.hand_l = elev_point(s, e, pad_for_l + kHandPadOff);
    g.hand_r = elev_point(s, e, pad_for_r + kHandPadOff);

    // --- the sight axis under this elevation (head aim + a lean cue)
    g.look_dir = glm::normalize(elev_point(s, e, s.sight_front) -
                                elev_point(s, e, s.sight_rear));

    // --- WHERE HE STANDS. The shoulder line is placed a fixed working arm
    // from the handle mid: he would like to be at his own standing height,
    // and he keeps that until the handles descend far enough that a straight
    // stand cannot hold them -- from there the reach is spent going DOWN and
    // the fore-aft standoff bottoms out at kStandoffMinM. The reach itself
    // (kArmWorkFrac of a full arm) never changes, so the elbows never lock
    // and never overreach at any elevation: the arms are a rigid triangle
    // and the ELEVATION CURVE IS A CROUCH, exactly as P1-9 rules.
    const double deck_y = 0.5 * (s.foot_l.y + s.foot_r.y);
    const double stand_y = deck_y + a.ankle_up_m +
                           kStandLegFrac * (a.thigh_m + a.calf_m) + a.torso_m;
    const double reach = kArmWorkFrac * (a.uarm_m + a.farm_m);
    const double dy_max = std::sqrt(
        std::max(0.0, reach * reach - kStandoffMinM * kStandoffMinM));
    const auto stance = [&](double ee) {
        const glm::dvec3 hm =
            0.5 * (elev_point(s, ee, s.pad_l + kHandPadOff) +
                   elev_point(s, ee, s.pad_r + kHandPadOff));
        double d = stand_y - hm.y;
        d = d > dy_max ? dy_max : (d < -dy_max ? -dy_max : d);
        const double so = std::sqrt(std::max(
            kStandoffMinM * kStandoffMinM, reach * reach - d * d));
        return glm::dvec3(hm.x, hm.y + d, hm.z - so);
    };
    const glm::dvec3 sh_mid = stance(e);
    // How deep this elevation has driven him below a straight stand, and the
    // deepest the curve ever goes -- the crouch fraction the pelvis seed and
    // the spine bow read (they used the PAD DROP when the pads were his
    // shoulders; the quantity is the same shape, taken off the new chain).
    const double drop = std::max(0.0, stand_y - sh_mid.y);
    const double drop_full = std::max(1e-6, stand_y - stance(kElevMaxRad).y);
    // Shoulders straddle that line at the rig's own width. The SIDE comes
    // from the hands, never typed: the gun's "l" stations are on -x and the
    // rig's left arm is on +x, and only the data knows which is which.
    const double sxh = g.hand_l.x >= g.hand_r.x ? 1.0 : -1.0;
    g.shoulder_l = sh_mid + glm::dvec3(sxh * a.shoulder_half_m, 0.0, 0.0);
    g.shoulder_r = sh_mid - glm::dvec3(sxh * a.shoulder_half_m, 0.0, 0.0);
    // Feet under him, the stance WIDTH off the stations, the fore-aft
    // derived -- he steps back at 0 deg and follows the handles in as they
    // swing forward and down.
    g.ankle_l = glm::dvec3(s.foot_l.x, s.foot_l.y + a.ankle_up_m,
                           sh_mid.z + kFootAheadOfShoulderZ);
    g.ankle_r = glm::dvec3(s.foot_r.x, s.foot_r.y + a.ankle_up_m,
                           sh_mid.z + kFootAheadOfShoulderZ);

    // --- pelvis: the P1-9b crouch. Seeded OVER the stepped feet, then
    // alternately projected onto the constraint set: hips over the feet in
    // plan, above the deck floor, out of the pedestal column, inside the
    // legs' reach ball -- and the torso sphere LAST and exactly (legs flex,
    // the spine does not stretch). All projections are contractions onto
    // convex sets; the iteration is deterministic. The seed is placed ON
    // the torso sphere on its BELOW-THE-SHOULDERS side, toward a point over
    // the feet -- a seed above the shoulder line converges to the mirror
    // basin (pelvis over his own head; the first probe of this round found
    // exactly that at 45 deg and up).
    glm::dvec3 pelvis;
    {
        // ★ DROP-AWARE seed (verify-round finding): a fixed 0.35 m
        // preferred pelvis height holds the pelvis HIGH under a
        // near-vertical gun (it ROSE 0.24 -> 0.45 from 60 -> 87 deg) and
        // folds the back to ~74 deg forward -- ~24 deg deeper than the
        // constraint set demands (feasible minimum ~50 deg). Past the mid
        // band the preferred point rides DOWN toward the deck floor and
        // FORWARD toward the hips-over-feet window's leading edge, and the
        // tuck bottoms out instead of folding. The below-the-shoulders
        // seeding discipline (the mirror-basin trap) is unchanged.
        const glm::dvec3 amid = 0.5 * (g.ankle_l + g.ankle_r);
        const double dyk = 0.5 * (s.foot_l.y + s.foot_r.y);
        const double frac = std::min(1.0, drop / drop_full);
        const double t =
            std::min(1.0, std::max(0.0, (frac - 0.75) / 0.25));
        const double h_pref =
            (1.0 - t) * 0.35 + t * ((dyk + kPelvisDeckMinM) - amid.y);
        const glm::dvec3 pref =
            amid + glm::dvec3(0.0, h_pref, t * 0.75 * kHipOverFootM);
        pelvis = sh_mid + a.torso_m * glm::normalize(pref - sh_mid);
    }
    // lateral geometry is constant, so fold the hip/ankle spread into one
    // midline reach: reach^2 available fore/aft+up = leg^2 - lateral^2
    const double leg = kLegReachFrac * (a.thigh_m + a.calf_m);
    const double lat = std::abs(0.5 * std::abs(g.ankle_l.x - g.ankle_r.x) -
                                a.hip_half_m);
    const double leg_mid =
        std::sqrt(std::max(1e-6, leg * leg - lat * lat));
    // --- heel rise (depression): if the whole chain cannot span shoulder
    // -> deck, raise both ankles together just enough (capped) -- the toes
    // stay planted in plan. Solve |v - r*up| = R for the small root.
    {
        const double R = a.torso_m + leg_mid;
        const glm::dvec3 v =
            sh_mid - 0.5 * (g.ankle_l + g.ankle_r);
        const double d2 = glm::dot(v, v);
        if (d2 > R * R) {
            const double disc = v.y * v.y - (d2 - R * R);
            double r = disc > 0.0 ? v.y - std::sqrt(disc) : kToeRiseMaxM;
            r = std::min(std::max(r, 0.0), kToeRiseMaxM);
            g.ankle_l.y += r;
            g.ankle_r.y += r;
        }
    }
    const glm::dvec3 ank_mid = 0.5 * (g.ankle_l + g.ankle_r);
    for (int it = 0; it < 24; ++it) {
        // hips over the feet in plan (P1-9b), never a sit, never the column
        pelvis.z = std::min(std::max(pelvis.z, ank_mid.z - kHipOverFootM),
                            std::min(ank_mid.z + kHipOverFootM,
                                     kPelvisAftLimitZ));
        pelvis.y = std::max(pelvis.y, deck_y + kPelvisDeckMinM);
        // the legs' reach ball
        const glm::dvec3 to_p = pelvis - ank_mid;
        const double d = glm::length(to_p);
        if (d > leg_mid) pelvis = ank_mid + to_p * (leg_mid / d);
        // torso sphere LAST and exact
        pelvis = sh_mid + a.torso_m * glm::normalize(pelvis - sh_mid);
    }
    g.pelvis = pelvis;

    // --- legs: hips off the pelvis laterally toward their own ankle side,
    // knees bent toward the gun (+Z) and slightly out.
    const double sxl = g.ankle_l.x >= g.ankle_r.x ? 1.0 : -1.0;
    g.hip_l = pelvis + glm::dvec3(sxl * a.hip_half_m, 0.0, 0.0);
    g.hip_r = pelvis - glm::dvec3(sxl * a.hip_half_m, 0.0, 0.0);
    // knees splay OUT as well as forward -- the deep P1-9b squat folds the
    // legs nearly flat, and a straight-forward knee would drive through the
    // pedestal column; splayed, they pass beside it (the catcher's squat).
    g.knee_l = two_bone_mid(g.hip_l, g.ankle_l, a.thigh_m, a.calf_m,
                            glm::normalize(glm::dvec3(sxl * 0.45, 0.0, 0.9)));
    g.knee_r = two_bone_mid(g.hip_r, g.ankle_r, a.thigh_m, a.calf_m,
                            glm::normalize(glm::dvec3(-sxl * 0.45, 0.0, 0.9)));
    g.ball_l = g.ankle_l + glm::dvec3(0.0, -0.06, 0.13);
    g.ball_r = g.ankle_r + glm::dvec3(0.0, -0.06, 0.13);

    // --- spine: stations along the pelvis->shoulder line at the rest-pose
    // fractions, bowed slightly aft (his back rounds as he squats).
    const glm::dvec3 up_t = glm::normalize(sh_mid - pelvis);
    const glm::dvec3 bow = glm::dvec3(0.0, 0.0, -1.0) * (0.03 + 0.05 * drop);
    g.spine_01 = pelvis + up_t * (0.18 * a.torso_m) + bow * 0.55;
    g.spine_02 = pelvis + up_t * (0.45 * a.torso_m) + bow;
    g.spine_03 = pelvis + up_t * (0.72 * a.torso_m) + bow * 0.75;
    g.neck = sh_mid;
    g.clavicle_l = sh_mid + (g.shoulder_l - sh_mid) * 0.14;
    g.clavicle_r = sh_mid + (g.shoulder_r - sh_mid) * 0.14;
    // head: craned toward the SIGHT EYE STATION -- the target the real
    // posture serves (cheek to the rest, eye behind the peep, spec §2.2).
    // The station is OUTBOARD LEFT of the receiver, so chasing it takes the
    // helmet off the gun's centreline -- which is what clears the breech
    // (round-1's helmet sat at x = 0, inside the receiver at 45 deg).
    // ★ ...but the crane aims the FACE, not the joint: the target is pulled
    // back down the sight axis by the drawn head's own forward reach plus
    // kEyeReliefM, so what arrives near st_eye is his goggles and not the
    // back of his skull. Aiming the joint at the station drove the helmet
    // into the breech (2026-08-31). Everything below the neck is untouched.
    const glm::dvec3 eye_t = elev_point(s, e, s.eye);
    const glm::dvec3 head_aim =
        eye_t - g.look_dir * (a.face_fwd_m + kEyeReliefM);
    // ...and the crane LEANS off the neck's own up, never past
    // kHeadLeanMaxRad. See the banner on that constant: unclamped, this aim
    // put his head below his shoulders and his scarf over his helmet.
    glm::dvec3 want = head_aim - g.neck;
    const double want_len = glm::length(want);
    want = want_len > 1e-9 ? want / want_len : up_t;
    glm::dvec3 lean = want - up_t * glm::dot(want, up_t);
    const double lean_len = glm::length(lean);
    if (lean_len > 1e-9) {
        lean /= lean_len;
        const double ang = std::atan2(lean_len, glm::dot(want, up_t));
        const double lim = ang < kHeadLeanMaxRad ? ang : kHeadLeanMaxRad;
        want = up_t * std::cos(lim) + lean * std::sin(lim);
    } else {
        want = up_t;
    }
    g.head = g.neck + a.neck_m * want;

    // --- elbows: down + out, solved per side. ★ The splay side comes from
    // the HANDS (sxh), not the feet: after the 2026-09-01 hand swap the arms
    // and the legs no longer sit on the same suffix convention, and an elbow
    // hinted off the wrong side folds the arm across his chest.
    g.elbow_l = two_bone_mid(g.shoulder_l, g.hand_l, a.uarm_m, a.farm_m,
                             glm::normalize(glm::dvec3(sxh * 0.7, -0.7, 0.0)));
    g.elbow_r =
        two_bone_mid(g.shoulder_r, g.hand_r, a.uarm_m, a.farm_m,
                     glm::normalize(glm::dvec3(-sxh * 0.7, -0.7, 0.0)));
    return g;
}

}  // namespace render::flak
