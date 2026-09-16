#pragma once
// ★★★ ST-5 PHASE D -- THE SEAT DEPLOY, AS ARITHMETIC.
//
// docs/PLAN_20260904_sting_st5_art.md §4 and the game-loop lane's
// docs/PACKET_TO_STING_ANIM_from_gameloop_20260904.md §3. Everything in this
// header is PURE: no raylib, no GLB, no clock of its own, no file-scope
// state. It is the half of the deploy animation that can be executed by
// ctest (test/unit/test_sting_pose.cpp) -- the timer's laws and the key
// interpolation. The half that cannot (loading launcher.glb, drawing it)
// lives in render/launcher_model.* and render/draw.cpp.
//
// TWO SEPARATE QUANTITIES, and conflating them is the classic bug:
//   * the RAW blend `t` rises LINEARLY at 1/kDeployRiseS, so "three seconds"
//     is a fact and not an asymptote. `render::blend_toward` -- the flak
//     pullout's exponential, which this timer otherwise mirrors -- only
//     APPROACHES 1, and a pose that never reaches its shouldered key would
//     leave the stock a centimetre off his shoulder forever.
//   * the EASE is applied at the POSE (`ease()`), so the feel is smooth at
//     both ends while the duration stays measurable.
//
// THE CUT IS NOT A FAST BLEND (packet §3.5: "your pose must be able to cut,
// not only blend"). A fall, a mount, a respawn or the X give-up takes the
// stance away, and the man is not holding anything on the next frame -- so
// `cut` drops the blend to 0 in ONE frame, with no frames drawn of a
// launcher easing down out of hands that are no longer there. A VOLUNTARY
// P-stow is a different act and eases over kStowFallS.

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace render {
namespace sting {

// Chad's ask, verbatim: "3 second deployment animation from the seat".
inline constexpr double kDeployRiseS = 3.0;
// The voluntary stow (P while shouldered) is a quick, deliberate put-away --
// not the reverse of the draw. A 3 s ease-down would leave the launcher
// hanging in shot long after the prompt said STOWED.
inline constexpr double kStowFallS = 0.3;
// The seam between the two legs of the sweep: stowed -> pulled -> shouldered.
// 0.45 of the blend buys the pull off the tunnel (the slow, heavy half) and
// leaves 0.55 for the swing up onto the shoulder.
inline constexpr float kPullFrac = 0.45f;

// ---------------------------------------------------------------------------
// THE TIMER
// ---------------------------------------------------------------------------

struct DeployIn {
    // The launcher is in his hands (render::FrameInfo::sting_shouldered).
    bool shouldered = false;
    // The stance is GONE this frame -- fall, mount, respawn, X give-up, a
    // forced mode. Beats everything below: no ease, no frames.
    bool cut = false;
    // The drone went active THIS frame (a rising edge, not a level). Snaps
    // the blend to full so a launch clicked mid-draw does not hand the
    // machine back with a half-raised launcher on the seat.
    bool launched = false;
    double dt = 0.0;  // clamped frame dt [s]
};

// One frame of the deploy blend. Returns the new raw t in [0,1].
//
// ORDER IS THE RULING: cut beats launch beats the ramp. A frame that is both
// a launch and a stance loss is a stance loss -- the launcher left his hands
// either way, and the cut is the one that cannot draw a wrong frame.
inline double deploy_step(double t, const DeployIn& in) {
    if (in.cut) return 0.0;
    if (in.launched) return 1.0;
    const double dt = in.dt > 0.0 ? in.dt : 0.0;
    if (in.shouldered) return std::min(1.0, t + dt / kDeployRiseS);
    return std::max(0.0, t - dt / kStowFallS);
}

// Smootherstep (zero first AND second derivative at both ends) on the raw
// blend. The plain smoothstep still starts with a visible corner in
// acceleration at this duration -- three seconds is long enough to see it.
inline float ease(float u) {
    const float x = std::clamp(u, 0.0f, 1.0f);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// Which leg of the sweep, and how far along it. leg 0 = stowed -> pulled,
// leg 1 = pulled -> shouldered. CONTINUOUS at kPullFrac by construction:
// leg 0 ends at w == 1 (the pulled key) and leg 1 starts at w == 0 (the same
// key), so the two legs agree on the frame they share.
struct KeyBlend {
    int leg = 0;
    float w = 0.0f;
};
inline KeyBlend key_blend(float t_eased) {
    const float x = std::clamp(t_eased, 0.0f, 1.0f);
    KeyBlend kb;
    if (x <= kPullFrac) {
        kb.leg = 0;
        kb.w = kPullFrac > 0.0f ? x / kPullFrac : 1.0f;
    } else {
        kb.leg = 1;
        kb.w = (x - kPullFrac) / (1.0f - kPullFrac);
    }
    return kb;
}

// ---------------------------------------------------------------------------
// THE KEYS
// ---------------------------------------------------------------------------

// A rigid frame for the launcher. `basis` columns are (right, up, +Z) in the
// glTF authoring convention the asset ships in: FIRE DIRECTION IS -Z, UP IS
// +Y, origin at the centre of the two grips. So the aim ray is -basis[2],
// exactly as render::sting_model_draw already reads the drone's own frame.
struct Frame {
    glm::dvec3 pos{0.0};
    glm::dmat3 basis{1.0};
};

// Orthonormal frame from a fire direction and an up hint. Returns identity
// columns unchanged if the two are parallel (nothing to solve, and a
// normalize of a zero cross is a NaN that would poison the whole draw).
inline glm::dmat3 frame_from_aim(const glm::dvec3& fire,
                                 const glm::dvec3& up_hint) {
    const double fl = glm::length(fire);
    if (fl < 1e-9) return glm::dmat3(1.0);
    const glm::dvec3 f = fire / fl;
    glm::dvec3 r = glm::cross(f, up_hint);
    const double rl = glm::length(r);
    if (rl < 1e-9) return glm::dmat3(1.0);
    r /= rl;
    const glm::dvec3 u = glm::cross(r, f);
    return glm::dmat3(r, u, -f);  // column 2 is +Z, i.e. AFT
}

// Blend two rigid frames. Position mixes; rotation SLERPS -- a component-wise
// mix of two bases is not a rotation and shows up as a launcher that shears
// and shrinks through the middle of the sweep.
inline Frame blend_frame(const Frame& a, const Frame& b, float w) {
    const double x = std::clamp(static_cast<double>(w), 0.0, 1.0);
    Frame out;
    out.pos = a.pos * (1.0 - x) + b.pos * x;
    const glm::dquat qa = glm::quat_cast(a.basis);
    glm::dquat qb = glm::quat_cast(b.basis);
    if (glm::dot(qa, qb) < 0.0) qb = -qb;  // shortest arc, never the long way
    out.basis = glm::mat3_cast(glm::normalize(glm::slerp(qa, qb, x)));
    return out;
}

// ---------------------------------------------------------------------------
// THE HANDS
// ---------------------------------------------------------------------------
//
// ★★★ Chad, 2026-09-04, VERBATIM: "onto the hand hook to make exception and
// grasp the stock of the launcher". v1 of the deploy left both hands welded to
// the handlebars while the launcher swung up beside them, because the weld in
// render/sled_model.cpp had no way to be told about a target that did not
// exist when it ran. These two numbers are the OTHER half of that fix: how
// much of each hand belongs to the launcher at this point of the sweep.
//
// THEY ARE PURE FUNCTIONS OF THE DEPLOY BLEND, and that is the whole design.
// Nothing here integrates, latches or remembers, so the stance-loss CUT
// (deploy -> 0 in one frame, `deploy_step`) puts both hands back on the bars
// on the very next frame with no second piece of state to agree with -- the
// same reason the pose itself is a pure function of `t`.
//
// THE RIGHT HAND GOES FIRST, and it is the trigger hand: he lets go of the
// throttle, pulls the launcher off the tunnel by its pistol grip, and only
// then -- once the thing is out and moving up -- does his left hand leave the
// bar to take the forward grip. A man does not release both bars at once at
// speed, and the eye reads it immediately if he does.
inline constexpr float kGripROnT = 0.25f;   // right hand leaves the throttle
inline constexpr float kGripRFullT = 0.50f;  // ...and is on the pistol grip
inline constexpr float kGripLOnT = 0.55f;   // left hand leaves the bar LATER
inline constexpr float kGripLFullT = 0.80f;  // ...onto the forward grip

// Smoothstep across a window, clamped outside it. Zero derivative at both
// ends, so a hand leaves and arrives without a corner in its speed -- the
// weld is a position blend and a linear ramp reads as a snatch.
inline float grip_ramp(float t, float lo, float hi) {
    if (!(hi > lo)) return t >= hi ? 1.0f : 0.0f;
    const float x = std::clamp((t - lo) / (hi - lo), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// [0,1]: 0 = the hand is on the handlebar exactly as it always was, 1 = it is
// on the launcher's grip. `t_eased` is the EASED blend (`ease(t)`), the same
// number the key interpolation runs on, so the hands and the tube they take
// hold of are driven by one clock.
inline float grip_weight_r(float t_eased) {
    return grip_ramp(std::clamp(t_eased, 0.0f, 1.0f), kGripROnT, kGripRFullT);
}
inline float grip_weight_l(float t_eased) {
    return grip_ramp(std::clamp(t_eased, 0.0f, 1.0f), kGripLOnT, kGripLFullT);
}

// ---------------------------------------------------------------------------
// THE STATIONS
// ---------------------------------------------------------------------------

// Rest-pose model-space positions of the launcher's five station empties, in
// the authoring frame above. render::launcher_stations() overwrites these
// from assets/drone/launcher.glb.
//
// ⚠ THE NOMINALS MIRROR THE SHIPPED GLB, DIGIT FOR DIGIT -- they are not a
// designer's sketch. The collapse defect (Chad 2026-09-04: "collapsed over
// ... looked broken") survived a green test suite precisely because the test
// solved on invented nominals (0.57 m grip span) while the runtime ran the
// asset's (0.68 m, which no seated arm reaches). One truth: re-export the
// launcher, re-copy these numbers. st_muzzle is the rail's TAIL-STOP (the
// drone's tail sits on it, the body extends forward along -Z), NOT the rail
// front -- the old nominal had the opposite convention and hid there too.
struct Stations {
    bool from_glb = false;          // true once read off the asset
    glm::dvec3 grip_r{0.0, -0.145, 0.194};   // rear pistol grip (right hand)
    // ⚠ the forward grip was moved 0.29 m AFT of where the launcher was
    // first modelled (z -0.191): the original span put the support hand
    // 1.05 m from a 0.613 m seated arm. A magwell-style hold, one hand just
    // ahead of the other, is what a seated torso that cannot rotate can do.
    glm::dvec3 grip_l{0.0, -0.140, 0.100};   // forward grip (left hand)
    glm::dvec3 shoulder{0.0, -0.115, 0.488};  // the buttplate, against him
    glm::dvec3 sight{-0.118, 0.101, 0.395};   // optic, left cantilever
    glm::dvec3 muzzle{0.0, 0.139, 0.050};     // rail tail-stop: the drone's
                                              // tail, and its departure point
};

// ---------------------------------------------------------------------------
// THE SEATED / AFOOT KEYS, IN THE MAN'S OWN FRAME
// ---------------------------------------------------------------------------
//
// Both are expressed in a right-handed LOCAL frame: +X his right, +Y local
// up, -Z the way he faces. draw.cpp maps that onto the SLED basis (seated) or
// the walker's heading (afoot) -- one frame, one convention, so a sign cannot
// disagree between the two callers.
//
// ⚠ THE SWEEP GOES AROUND THE SILHOUETTE, NOT THROUGH IT. Every key sits
// OUTBOARD of his right shoulder (x >= +0.30) until the last one pulls in to
// the shoulder itself, and the rise happens at that outboard station. The
// obvious lazy path -- lerp a low centred key straight to a shouldered key --
// sweeps the tube through his chest, his chin and the windshield. The unit
// test asserts the clearance rather than trusting this comment.

// ⚠ SEATED AND AFOOT KEYS ARE SEPARATE, AND ONLY THE AFOOT PAIR HANGS OFF
// THE SHOULDER. The old single shoulder-relative pair was calibrated on a
// STANDING man (shoulder 1.55 m over his boots, stow at 0.50 m); hung off the
// real seated shoulder -- 0.51 m over the machine origin in the rider's
// hunched-over-the-bars rest pose, NOT the 1.20 m the fallback constant
// claimed -- the stow key landed 0.02 m off the running surface: the rig
// spent the first ~1.3 s of the deploy INSIDE the track (Chad: "I couldnt
// even see the rpas at deploy"). The seated keys are therefore quoted as
// ABSOLUTE machine-local positions against the seat the machine actually
// has (seat top ~= +0.10 over the frame origin).
inline constexpr double kStowBelowShoulderM = 1.05;
inline constexpr double kPullBelowShoulderM = 0.30;

// STOWED / PULLED, SEATED: beside the tunnel just under seat-top height, then
// out to his right at chest height, rail half swung onto the aim. Outboard
// the whole way (the sweep goes around the silhouette).
inline Frame seated_stowed() {
    Frame f;
    f.pos = glm::dvec3(0.34, 0.06, 0.30);
    f.basis = frame_from_aim(glm::dvec3(0.16, -0.06, -1.0),
                             glm::dvec3(0.88, 0.47, 0.0));
    return f;
}
inline Frame seated_pulled() {
    Frame f;
    f.pos = glm::dvec3(0.46, 0.36, 0.08);
    f.basis = frame_from_aim(glm::dvec3(0.45, 0.34, -1.0),
                             glm::dvec3(0.24, 0.97, 0.0));
    return f;
}

// STOWED, AFOOT: the low carry the standing man raises from. The standing
// shoulder calibration (~1.55 m over the boots) is the one the old constants
// were correct for, so afoot keeps the shoulder-relative form.
inline Frame local_stowed(double shoulder_up_m) {
    Frame f;
    f.pos = glm::dvec3(0.34, shoulder_up_m - kStowBelowShoulderM, 0.26);
    f.basis = frame_from_aim(glm::dvec3(0.16, -0.06, -1.0),
                             glm::dvec3(0.88, 0.47, 0.0));
    return f;
}

// ---------------------------------------------------------------------------
// THE UN-HUNCH -- "the sudburian seems hunched over unnecessarily" (Chad,
// 2026-09-05, on the seated shoulder pose).
// ---------------------------------------------------------------------------
//
// A man riding a snowmachine hard is folded over the bars, and this rider's
// REST SKELETON carries that fold -- ~33 deg of forward trunk tilt, authored
// into the asset, which is why it survived every pose the file already
// applies. It is right for riding and wrong for aiming: nobody shoulders a
// launcher from a tuck. So when the launcher comes up, the TRUNK comes up
// with it.
//
// THE KEY IS THE GRIP WEIGHT AND NOTHING ELSE. `sit` is the larger of the
// two hand-grip weights the hook already carries, so this rung adds no state
// of its own: it rises with the same eased blend that walks the hands off
// the bars, and it is exactly 0 on every frame the launcher is stowed --
// which is what makes "weight 0 is bit-identical" still true. The CUT (the
// blend to 0 in one frame) puts him back over the bars in one frame too,
// free, because there is nothing here to smooth it.
//
// THE FRACTION IS NOT ALL OF IT. He is still on a moving machine with his
// legs under the tunnel, and a ramrod-straight back reads as a parade rest,
// not a firing stance. kSitUpFrac takes out most of the hunch and leaves a
// working forward set.
inline constexpr float kSitUpFrac = 0.72f;

// ...spread down the spine, lumbar-heaviest, the way a seated man actually
// straightens: the hips open first and the shoulders follow. The three
// shares are spine_01, spine_02, spine_03 and they sum to 1, so the TOTAL
// rotation of the trunk above spine_03 is exactly kSitUpFrac of the measured
// hunch however the shares are re-cut.
inline constexpr float kSitShare[3] = {0.45f, 0.35f, 0.20f};

// The rotation to apply at one spine joint, in radians about MODEL +X, given
// the hunch MEASURED off the rest skeleton (positive = tilted forward) and
// the grip weight. NEGATIVE = backward = sitting up. Identically 0 at
// sit <= 0, which is the inertness contract.
inline float sit_up_rad(float hunch_rad, float sit, int seg) {
    if (seg < 0 || seg > 2) return 0.0f;
    const float w = sit < 0.0f ? 0.0f : (sit > 1.0f ? 1.0f : sit);
    if (w <= 0.0f) return 0.0f;
    return -hunch_rad * kSitUpFrac * kSitShare[seg] * w;
}

// The seated rider's NECK in machine-local coordinates (+X right, +Y up,
// +Z back), measured off the indy650 rest skeleton (neck_01 over the kernel
// CG via the mount law; the rest pose already carries the 33 deg riding
// hunch). The fore/aft +0.297 is the term the collapse defect was missing:
// the shoulder anchor was built from a HEIGHT alone, which put it 0.30 m
// ahead of the man and stacked the launcher's grip span on top of that.
// draw.cpp prefers the live posed neck (cached as a full body-frame VECTOR,
// same anti-smear law as the old scalar); this is the fallback.
inline constexpr glm::dvec3 kSeatedNeckLocal{0.0, 0.657, 0.297};

// The right shoulder, relative to the same local frame's origin: outboard far
// enough that the tube clears the side of his head (a helmet is ~0.12 m of
// half-width; 0.22 m leaves a hand's breadth), and one head-drop below the
// eye height the launch origin is quoted at.
inline constexpr double kShoulderOutM = 0.22;
inline constexpr double kShoulderDropM = 0.15;
// Fallback shoulder heights, used when the posed rider is unavailable (the
// hero GLB missing / placeholder forced) and afoot, where nothing publishes a
// posed neck at all. Both are ONE HEAD-DROP below the eye heights the
// game-loop lane's `sting_stance` quotes (1.35 seated, 1.7 afoot), so the
// launcher hangs off the same man the launch origin is measured from.
inline constexpr double kSeatedShoulderUpM = 1.35 - kShoulderDropM;
inline constexpr double kAfootShoulderUpM = 1.70 - kShoulderDropM;

// Where the muzzle ends up, given the shouldered anchor and the aim. This is
// the number the game-loop lane's launch origin has to agree with (packet §2)
// -- "if your deploy animation ends the launcher somewhere else, say so".
// `up` is local up; the return is the height of st_muzzle above the frame
// origin (the machine's frame origin seated, the walker's feet afoot).
inline double muzzle_height_m(const Stations& st, double shoulder_up_m,
                              const glm::dvec3& fire, const glm::dvec3& up) {
    const glm::dmat3 b = frame_from_aim(fire, up);
    // pos places st.shoulder AT the anchor; the muzzle then follows.
    const glm::dvec3 rel = b * (st.muzzle - st.shoulder);
    return shoulder_up_m + glm::dot(rel, glm::normalize(up));
}

// ---------------------------------------------------------------------------
// ★★★ ST-5 THE SWEEP -- "up to 180 degree sweep with head and torso"
// (Chad, 2026-09-05, VERBATIM: "the only things that allows me to aim the gun
// around is that my arms move, the sudburian shall follow the aim, up to 180
// degree sweep with head and torso as well moving with the rpas, they shall
// twist head and torso in addition to the arms. They should only be able to
// manoeuvre it in a 180 degree sweep while seated on the snowmachine, if they
// want to target an enemy behind them they need to turn the snowmachine
// around.")
// ---------------------------------------------------------------------------
//
// TWO RULINGS, AND THEY ARE SEPARATE PIECES OF ARITHMETIC.
//
//  1. THE LIMIT. Seated, the aim azimuth lives in [-90, +90] OFF THE MACHINE'S
//     OWN HEADING -- a 180 degree sweep, the whole of it in front of him. It is
//     a clamp on the ACCUMULATOR and not on the drawn pose: a pose-only clamp
//     would let the mouse wind an invisible number up to 300 degrees and then
//     make him pay 210 degrees of mouse to come back, which reads as a dead
//     stick. Because the accumulator is measured over the STANCE FRAME (the
//     machine's heading -- app/main.cpp builds `h` from
//     `sled.orientation * -Z`), the window TURNS WITH THE MACHINE for free:
//     steering the sled carries the whole 180 degrees around with it, which is
//     exactly the second half of his ruling ("turn the snowmachine around").
//     AFOOT IS NOT CLAMPED -- his sentence names the seat, and the walker
//     turns his whole body.
//
//  2. THE TWIST. The launcher rides his hands, his hands ride his arms, and
//     until this rung the chain above the shoulders did not know the aim
//     existed: he tracked a target 90 degrees off the nose with his arms
//     wrenched across a trunk that still faced forward. The twist is spread
//     the way a human twists -- THORACIC-HEAVY, and the head LEADING -- which
//     is the exact inverse of the un-hunch's lumbar-heavy share above (a man
//     STRAIGHTENS from the hips and TWISTS from the chest, and using one
//     share table for both would be wrong in both directions).

// The seated window: +-90 degrees off the machine's heading.
inline constexpr double kSeatedAzMaxRad = 1.5707963267948966;

// The clamp itself. Applied EVERY frame the stance is the seat, not only on a
// frame the mouse moved: a remount, a forced mode or a stance regained with
// the accumulator already outside the window has to be pulled in, and there
// is no mouse event on that frame to hang it off.
inline double clamp_seated_az(double az) {
    return az < -kSeatedAzMaxRad ? -kSeatedAzMaxRad
                                 : (az > kSeatedAzMaxRad ? kSeatedAzMaxRad : az);
}
inline float clamp_seated_az(float az) {
    return static_cast<float>(clamp_seated_az(static_cast<double>(az)));
}

// The five joints the sweep is spread over, in chain order from the hips up.
// The first three are the SAME nodes the un-hunch rotates, and that is
// deliberate: one captured chain, one set of parent rest rotations, two
// rotations composed at each rung.
enum TwistSeg {
    kTwSpine1 = 0,
    kTwSpine2,
    kTwSpine3,
    kTwNeck,
    kTwHead,
    kTwistCount
};

// ⚠ THORACIC-HEAVY, AND IT SUMS TO 1. The lumbar spine barely rotates in a
// living body -- its facet joints face the wrong way for axial rotation -- so
// a share table that twisted from the waist would look like a mannequin on a
// lazy susan.
// The top two rungs (0.20 + 0.15 = 0.35) are the head LEADING the aim: he
// looks where the sight is before his chest gets there, which is what makes a
// tracking shot read as tracking rather than as a turret.
inline constexpr float kTwistShare[kTwistCount] = {0.15f, 0.25f, 0.25f, 0.20f,
                                                   0.15f};

// How much of the aim's ELEVATION the head takes. He looks UP the sight line;
// the trunk does not follow it (the stock is on his shoulder, and a trunk
// that pitched with the aim would take the shoulder out from under it).
inline constexpr float kHeadElFrac = 0.40f;

// The rotation to apply at one joint, in radians about MODEL +Y (up).
//
// ⚠ THE SIGN. Model +X is the rider's LEFT and model +Z is the nose (the
// coordinate law render/sled_model.cpp steers by), so a POSITIVE rotation
// about model +Y takes the nose toward his left -- and `aim_az` is POSITIVE
// TO HIS RIGHT (app/main.cpp builds the aim as cos(az)*heading - sin(az)*left).
// The negation is that disagreement, written down once. The same sign the
// head-yaw channel two hundred lines below already carries ("orbiting the
// view right (az+) turns the head right = negative model yaw").
//
// Identically 0 at sit <= 0 -- the inertness contract the whole hook ships
// under -- and clamped at 1 so a caller's rounding cannot over-wind him.
inline float twist_rad(float aim_az, float sit, int seg) {
    if (seg < 0 || seg >= kTwistCount) return 0.0f;
    const float w = sit < 0.0f ? 0.0f : (sit > 1.0f ? 1.0f : sit);
    if (w <= 0.0f) return 0.0f;
    return -aim_az * kTwistShare[seg] * w;
}

// The head's share of the ELEVATION, in radians about MODEL +X. NEGATIVE =
// backward = looking up, the same sign convention `sit_up_rad` carries (and
// for the same reason: about model +X, +theta takes the nose down).
inline float head_el_rad(float aim_el, float sit) {
    const float w = sit < 0.0f ? 0.0f : (sit > 1.0f ? 1.0f : sit);
    if (w <= 0.0f) return 0.0f;
    return -aim_el * kHeadElFrac * w;
}

}  // namespace sting
}  // namespace render
