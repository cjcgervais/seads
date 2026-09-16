#pragma once
// THE FLAK GUN -- 20 mm/70 Oerlikon on Mount Mark 4: stations + kinematics.
//
// docs/FLAK_GUN_SPEC.md is the contract. The GLB is assets/flak/oerlikon_mk4.glb
// (authored by assets/flak/flak_src/flak_gun_geom.py in the GUI Blender, every
// dimension sourced -- OP 909 / OP 911 / NavWeaps / the Sudburian's ruled
// table -- and the derived ones flagged there and in the spec §4).
//
// PURE: glm + std only. No raylib, no cgltf, no asset I/O, no clock. Lives in
// seads_render_core and is gated headlessly by test/unit/test_flak_gun.cpp,
// which ALSO opens the shipped GLB (cgltf, test-side) and pins that the node
// names, the station positions and the limits this header assumes are the
// ones the file actually carries -- the ghost-GLB trap
// (docs/SUDBURIAN_LADDER.md §2.2 "THE GHOST GLB") closed at the source.
//
// ★ THE MODEL FRAME (glTF, +Y up): +Z = MUZZLE FORWARD at zero train and
// zero elevation, +X = the gunner's RIGHT, +Y = up. The origin is the centre
// of the pedestal base, on the ground.
//   flak_train  rotates about +Y.  A right-handed rotation about +Y by +t
//               carries +Z toward +X, so POSITIVE TRAIN = MUZZLE SWINGS RIGHT.
//   flak_cradle rotates about +X.  A right-handed rotation about +X by +a
//               carries +Z toward -Y (DOWN), so the node's quaternion for an
//               elevation e (positive = muzzle UP) is angleAxis(-e, +X).
//               cradle_quat() below is the ONE place that sign lives.
// Everything under flak_cradle (gun, drum, sight, grips, pads, the st_*
// stations on the gun) elevates; st_foot_l/r + st_approach live under
// flak_train -- the gunner walks round the pedestal with the gun and does
// not tilt with it.
//
// ★ WHAT THIS HEADER IS NOT. It is not the loader (app-side cgltf, the
// sled_model.cpp precedent), not the fire control, not the camera, not the
// on-foot state (R5+, unspecified). Those are the next rungs (spec §7). It is
// the measured contract they will all read, so the numbers live ONCE.

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace render::flak {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kDeg = kPi / 180.0;

// --- the mount's documented limits [OP 909]. Train is unlimited (360 deg).
inline constexpr double kElevMinRad = -5.0 * kDeg;
inline constexpr double kElevMaxRad = 87.0 * kDeg;   // Mod. 2/3; Mod. 3 = 90
// Trunnion height range of the jack-screw column [OP 909], and the height the
// GLB is built at: the Sudburian's shoulder joint in boots, 0.980 + 0.533 +
// 0.030 [SUDBURIAN_LADDER §3]. The test pins the file agrees.
inline constexpr double kTrunnionMinM = 46.5 * 0.0254;    // 1.181
inline constexpr double kTrunnionMaxM = 62.25 * 0.0254;   // 1.581
inline constexpr double kTrunnionBuiltM = 0.980 + 0.533 + 0.030;  // 1.543
// The gun [OP 911 / NavWeaps]. The pad face is 35.5 in behind the train axis
// with the gun horizontal [OP 909]; the spec §4 flags "trunnion ON the axis".
inline constexpr double kGunOverallM = 87.0 * 0.0254;             // 2.210
inline constexpr double kPadBehindAxisM = 35.5 * 0.0254;          // 0.9017
inline constexpr double kMuzzleSpeedMps = 835.0;                  // 2725 ft/s
inline constexpr double kRofHz = 450.0 / 60.0;                    // cyclic
inline constexpr int kDrumRounds = 60;

// --- the station table. Model-frame positions read from the GLB's st_*
// nodes by the loader (or the test); zero-initialised means "not loaded".
struct Stations {
    glm::dvec3 muzzle{0.0};       // st_muzzle      (cradle)  bore exit
    glm::dvec3 eye{0.0};          // st_eye         (cradle)  the sight camera
    glm::dvec3 sight_rear{0.0};   // st_sight_rear  (cradle)  the peep
    glm::dvec3 sight_front{0.0};  // st_sight_front (cradle)  bead + rings
    glm::dvec3 grip_l{0.0}, grip_r{0.0};  // st_grip_l/r (cradle) hand targets
    glm::dvec3 pad_l{0.0}, pad_r{0.0};    // st_pad_l/r  (cradle) shoulders
    glm::dvec3 eject{0.0};        // st_eject       (cradle)  case chute
    glm::dvec3 drum{0.0};         // st_drum        (cradle)  reload swap
    glm::dvec3 foot_l{0.0}, foot_r{0.0};  // st_foot_l/r (train)  on the pad
    glm::dvec3 approach{0.0};     // st_approach    (train)   walk-up point
    double trunnion_h = 0.0;      // flak_cradle world y at rest
    double train_h = 0.0;         // flak_train  world y at rest
};

// The node names, so the loader and the test share ONE list.
inline constexpr const char* kNodeRoot = "flak_root";
inline constexpr const char* kNodeTrain = "flak_train";
inline constexpr const char* kNodeCradle = "flak_cradle";

// --- pose
struct Pose {
    double train_rad = 0.0;  // + = muzzle right of the mount's +Z
    double elev_rad = 0.0;   // + = muzzle up; clamped [kElevMin, kElevMax]
};

inline double clamp_elev(double e) {
    return e < kElevMinRad ? kElevMinRad : (e > kElevMaxRad ? kElevMaxRad : e);
}

// Wrap to (-pi, pi].
inline double wrap_pi(double a) {
    a = std::fmod(a + kPi, 2.0 * kPi);
    if (a < 0.0) a += 2.0 * kPi;
    return a - kPi;
}

// The two node quaternions, model frame. cradle_quat carries the ONE sign
// that encodes "+X rotation drops the muzzle" (banner above).
inline glm::dquat train_quat(double train_rad) {
    return glm::angleAxis(train_rad, glm::dvec3(0.0, 1.0, 0.0));
}
inline glm::dquat cradle_quat(double elev_rad) {
    return glm::angleAxis(-elev_rad, glm::dvec3(1.0, 0.0, 0.0));
}

// --- the mount frame on the sphere. `up` is local up at the pedestal
// (normalize(position), SPEC §6 -- never a world axis); `fwd0` is where the
// muzzle points at zero train, projected into the tangent plane here so a
// caller passing an un-projected bearing cannot tilt the pedestal.
struct MountFrame {
    glm::dvec3 pos{0.0};    // pedestal base centre, world (on the ground)
    glm::dvec3 up{0.0};
    glm::dvec3 fwd0{0.0};   // tangent, unit
    glm::dvec3 right0{0.0}; // = cross(up, fwd0)... see make_mount_frame
};

inline MountFrame make_mount_frame(const glm::dvec3& pos, const glm::dvec3& up_in,
                                   const glm::dvec3& fwd_hint) {
    MountFrame f;
    f.pos = pos;
    f.up = glm::normalize(up_in);
    glm::dvec3 t = fwd_hint - f.up * glm::dot(fwd_hint, f.up);
    const double n = glm::length(t);
    if (n < 1e-9) {
        // hint parallel to up: pick ANY tangent, deterministically, without a
        // world-axis preference that could coincide with up at a generic point
        const glm::dvec3 a = std::fabs(f.up.x) < 0.9 ? glm::dvec3(1, 0, 0)
                                                     : glm::dvec3(0, 1, 0);
        t = glm::cross(f.up, a);
    }
    f.fwd0 = glm::normalize(t);
    // Model +X must be the gunner's RIGHT when facing +Z with +Y up: in a
    // right-handed frame right = cross(up, fwd)?  cross(+Y,+Z) = +X. Yes.
    f.right0 = glm::cross(f.up, f.fwd0);
    return f;
}

// World direction (unit) of a MODEL-frame direction under the mount frame at
// zero pose -- the frame's rotation only.
inline glm::dvec3 model_dir_to_world(const MountFrame& f, const glm::dvec3& d) {
    return f.right0 * d.x + f.up * d.y + f.fwd0 * d.z;
}

// A world aim direction -> the pose that points the bore at it. Train is the
// azimuth of the aim in the tangent plane (atan2 on the model's own axes, so
// +train = right, matching train_quat); elevation is the angle above the
// tangent plane, CLAMPED to the mount's limits -- the 3 deg dead cone
// overhead is real [OP 909] and wanted (spec §2). Returns false (and leaves
// the pose untouched) if the aim has no tangent component AND no vertical
// component, i.e. a zero vector.
inline bool aim_to_pose(const MountFrame& f, const glm::dvec3& aim_world,
                        Pose& out) {
    const double n = glm::length(aim_world);
    if (!(n > 0.0)) return false;
    const glm::dvec3 a = aim_world / n;
    const double x = glm::dot(a, f.right0);
    const double z = glm::dot(a, f.fwd0);
    const double y = glm::dot(a, f.up);
    if (std::hypot(x, z) < 1e-12) {
        // straight up/down: keep the current train, clamp the elevation
        out.elev_rad = clamp_elev(y >= 0.0 ? kPi / 2 : -kPi / 2);
        return true;
    }
    out.train_rad = std::atan2(x, z);
    out.elev_rad = clamp_elev(std::atan2(y, std::hypot(x, z)));
    return true;
}

// Body-limited slew of the free gun toward a demanded pose (spec §2: the
// DEMAND is unsmoothed, the GUN has a rate cap -- the gun mass, not the
// hand). Train takes the short way round. Rates are FEEL DIALS, not measured
// facts; the defaults are the red-team's derivation (spec §2.3) and Chad
// judges them at the gun. dt <= 0 or a rate <= 0 leaves the pose untouched.
struct SlewRates {
    double train_rad_s = 90.0 * kDeg;
    double elev_rad_s = 60.0 * kDeg;
};

inline Pose slew_toward(const Pose& cur, const Pose& target, const SlewRates& r,
                        double dt) {
    if (!(dt > 0.0)) return cur;
    Pose p = cur;
    if (r.train_rad_s > 0.0) {
        const double d = wrap_pi(target.train_rad - cur.train_rad);
        const double cap = r.train_rad_s * dt;
        p.train_rad = wrap_pi(cur.train_rad +
                              (std::fabs(d) <= cap ? d : (d > 0 ? cap : -cap)));
    }
    if (r.elev_rad_s > 0.0) {
        const double d = clamp_elev(target.elev_rad) - cur.elev_rad;
        const double cap = r.elev_rad_s * dt;
        p.elev_rad = clamp_elev(cur.elev_rad +
                                (std::fabs(d) <= cap ? d : (d > 0 ? cap : -cap)));
    }
    return p;
}

// --- placing a station in the world under a pose.
// A CRADLE station p (model frame, i.e. flak_root coordinates as the GLB
// stores its world position) is: rotate about the trunnion by the elevation,
// then about the train axis, then into the mount frame.
inline glm::dvec3 cradle_station_world(const MountFrame& f, const Stations& s,
                                       const Pose& pose, const glm::dvec3& p) {
    const glm::dvec3 trunnion(0.0, s.trunnion_h, 0.0);
    const glm::dvec3 q = cradle_quat(pose.elev_rad) * (p - trunnion) + trunnion;
    const glm::dvec3 m = train_quat(pose.train_rad) * q;  // train axis = +Y through origin
    return f.pos + model_dir_to_world(f, m);
}
// A TRAIN station (feet, approach): train rotation only.
inline glm::dvec3 train_station_world(const MountFrame& f, const Pose& pose,
                                      const glm::dvec3& p) {
    return f.pos + model_dir_to_world(f, train_quat(pose.train_rad) * p);
}
// The bore direction in the world under a pose (unit).
inline glm::dvec3 bore_dir_world(const MountFrame& f, const Pose& pose) {
    const glm::dvec3 m =
        train_quat(pose.train_rad) * (cradle_quat(pose.elev_rad) * glm::dvec3(0, 0, 1));
    return glm::normalize(model_dir_to_world(f, m));
}

// The sight camera: eye at st_eye, looking along the SIGHT AXIS (rear peep ->
// front bead, which is parallel to the bore in the GLB, less the 24.5
// arc-min droop OP 911 zeroes at 750 yd -- not modelled, spec §4), up = the
// cradle's +Y carried through the pose. No roll, ever (spec §2.6).
struct SightCamera {
    glm::dvec3 eye{0.0}, forward{0.0}, up{0.0};
};
inline SightCamera sight_camera(const MountFrame& f, const Stations& s,
                                const Pose& pose) {
    SightCamera c;
    c.eye = cradle_station_world(f, s, pose, s.eye);
    const glm::dvec3 front = cradle_station_world(f, s, pose, s.sight_front);
    const glm::dvec3 rear = cradle_station_world(f, s, pose, s.sight_rear);
    c.forward = glm::normalize(front - rear);
    const glm::dvec3 m =
        train_quat(pose.train_rad) * (cradle_quat(pose.elev_rad) * glm::dvec3(0, 1, 0));
    c.up = glm::normalize(model_dir_to_world(f, m));
    return c;
}

// --- F-POSE: THE SIGHT PICTURE IS CLEAN, THE PULLOUT SHOWS THE MAN ---
// Chad's 2026-08-31 ruling, after his fly ("freelook shows floating mits...
// floating hands look funny"): NO body is drawn down the sight -- the
// gunner's own view is gun, ring and pipper and nothing else. Holding
// free-look (SPACE) instead swings the camera OUT behind his shoulder,
// where the whole crouched Sudburian is drawn; releasing eases back onto
// the sight. That is also what spec §7.6 meant by "third person only": the
// man is authored to be SEEN, not to be worn. (The first-person arm cut
// this replaced could not be made to read: flat-tinted limbs seen from
// inside his own head are featureless blobs, and the hands sit ~22 deg
// below the sight axis, at or past the bottom of the frame -- measured,
// four pull-back distances, 2026-08-31.)
//
// How far the sight camera sits BEHIND st_eye along the sight axis. The
// TRUE eye is 0.2 m AHEAD of the grips (st_eye z=-0.604 vs st_grip
// z=-0.802), so this is a framing dial, not a near-plane dodge (that is
// solved per-view in draw.cpp): it keeps the ring and the whole sight
// assembly in frame while sitting close enough to be his head, which is
// what Chad asked for ("maybe cam should be closer in" -- it was 2.9 m).
// The eye moves ALONG the axis only, so the pipper stays parallax-true.
// FEEL DIAL.
inline constexpr double kSightEyeBackM = 0.85;

// The pullout: where the camera goes while free-look is held. Behind the
// gunner (he stands at the BREECH end, so the eye goes further back along
// -bore), lifted and swung off his shoulder so the man is not a silhouette
// filling the lens. All four are FEEL DIALS.
inline constexpr double kExtDistM = 4.5;        // eye distance from the mount
inline constexpr double kExtAzBiasRad = 0.44;   // ~25 deg off his shoulder
inline constexpr double kExtElBiasRad = 0.22;   // ~13 deg above the horizon
inline constexpr double kExtTargetUpM = 0.7;    // aim point above the pad
inline constexpr double kExtEaseHz = 6.0;       // ease in/out (the sled rate)
// The gunner fades IN across this band of the pullout blend, so he can
// never pop into existence inside the lens: at t below the start the
// camera is still effectively at his eye. Smoothstep = C1 at both knees.
inline constexpr double kExtFadeStart = 0.20;
inline constexpr double kExtFadeEnd = 0.55;
inline double gunner_fade(double t) {
    if (t <= kExtFadeStart) return 0.0;
    if (t >= kExtFadeEnd) return 1.0;
    const double u = (t - kExtFadeStart) / (kExtFadeEnd - kExtFadeStart);
    return u * u * (3.0 - 2.0 * u);
}

// The pullout camera. Built from the TRAINED frame so it follows the gun
// round the compass; az/el are the free-look head angles, which orbit it.
// Pure (no world axis assumed): every direction comes from the mount frame.
struct ExternalCamera {
    glm::dvec3 eye{0.0}, target{0.0}, up{0.0};
};
inline ExternalCamera external_camera(const MountFrame& f, const Pose& pose,
                                      double az, double el, double dist) {
    ExternalCamera c;
    c.up = f.up;
    c.target = f.pos + f.up * kExtTargetUpM;
    // Behind the breech = MINUS the trained forward, in the tangent plane.
    const glm::dvec3 fwd_tr = glm::normalize(model_dir_to_world(
        f, train_quat(pose.train_rad) * glm::dvec3(0, 0, 1)));
    const glm::dvec3 right_tr = glm::normalize(glm::cross(fwd_tr, f.up));
    const double a = az + kExtAzBiasRad;
    const double e = std::clamp(el + kExtElBiasRad, -0.3, 1.3);
    const glm::dvec3 back =
        (-fwd_tr) * (std::cos(e) * std::cos(a)) +
        right_tr * (std::cos(e) * std::sin(a)) + f.up * std::sin(e);
    c.eye = c.target + glm::normalize(back) * dist;
    return c;
}

// ============================================================================
// ★ STAGE B -- THE IMMERSION COSMETICS (muzzle flash, camera shake, snow
// blast). ALL THREE ARE COSMETIC: nothing here is read by the fire control,
// the aim, the pose, the pipper or any sim state. They are pure closed-form
// envelopes so the SHAPES are gateable headlessly -- the app owns only one
// accumulator each, and render/ owns the geometry.
//
// ★ THE FRAME-ORDER CONTRACT. All three accumulators are stepped from
// `FlakWorld::spawned_accum` WITHOUT draining it -- exactly like the recoil
// (app/main.cpp): step_frame() ADDS to spawned_accum, the flak input block
// reads it (recoil + these three), and the AUDIO block later in the same
// frame owns the zero. A fourth reader must sit in that same window.
//
// Each `*_step` is the same shape: exponential decay over dt, then a
// per-shot add, then a ceiling. Written once here rather than three times in
// main.cpp so the tests can walk them.
// ============================================================================

// Low-discrepancy scalars in [0,1) -- the combat/fx_curves.h idiom, re-derived
// here because render_core must not depend on combat/. j = a shot index.
inline double fk_frac(double x) { return x - std::floor(x); }
inline double fk_h1(double j) { return fk_frac(j * 0.61803398874989485); }
inline double fk_h2(double j) { return fk_frac(j * 0.75487766624669276); }
inline double fk_clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// One decay-then-add-then-ceiling step. shots <= 0 adds nothing.
inline double fk_env_step(double v, double dt, int shots, double tau_s,
                          double per_shot, double ceiling) {
    if (dt > 0.0 && tau_s > 0.0) v *= std::exp(-dt / tau_s);
    if (shots > 0) v += per_shot * static_cast<double>(shots);
    return v > ceiling ? ceiling : (v < 0.0 ? 0.0 : v);
}

// --- MUZZLE FLASH -----------------------------------------------------------
// ★ Why an envelope and not a one-frame pop: the game runs at ~285 fps and
// the gun at 7.5 Hz, so a single-frame flash is lit for 3.5 ms in every 133 --
// a strobe the eye reads as a flicker, not as a gun going off. ~40 ms is the
// photographic persistence of a real muzzle flash and covers ~11 frames.
// FEEL DIALS, every one.
inline constexpr double kFlashTauS = 0.040;      // decay time constant [s]
inline constexpr double kFlashPerShot = 1.0;     // add per spawned round
inline constexpr double kFlashCeil = 1.35;       // saturation at the cyclic
// ★ SIZED FROM THE SIGHT, NOT FROM OUTSIDE. The first build shipped 0.38 m
// and it looked right in an orbit shot -- but the sight camera sits ~4.6 m
// behind this ball ON THE BORE, so 0.38 m subtended ~9 deg and buried the
// aim point (and the pipper) under a white disc on every round. The gunner
// view is the one that has to survive; the external view is spectacle.
inline constexpr double kFlashStarRM = 0.22;     // white-hot ball radius [m]
inline constexpr double kFlashConeLM = 1.30;     // gas cone down the bore [m]
inline constexpr double kFlashConeRM = 0.20;     // cone base radius [m]

inline double flash_step(double inten, double dt, int shots) {
    return fk_env_step(inten, dt, shots, kFlashTauS, kFlashPerShot, kFlashCeil);
}

struct FlashState {
    double star_radius_m = 0.0;   // the ball at the bell
    double cone_len_m = 0.0;      // the gas cone along the bore
    double cone_radius_m = 0.0;   // at the bell; tapers to a point
    glm::dvec3 color{1.0};        // linear RGB, white-hot -> orange
    double brightness = 0.0;      // additive gain; EXACTLY 0 when dark
};

inline FlashState flash_state(double inten) {
    FlashState f;
    const double u = fk_clamp01(inten);
    if (!(u > 0.0)) return f;  // exact zero in => nothing drawn
    f.brightness = u;
    f.star_radius_m = kFlashStarRM * (0.35 + 0.65 * u);
    f.cone_len_m = kFlashConeLM * u;
    f.cone_radius_m = kFlashConeRM * (0.50 + 0.50 * u);
    // Fresh (u -> 1) is white-hot; the tail cools to orange. One lerp, so the
    // colour is monotone in u and a test can walk it.
    const glm::dvec3 W{1.00, 0.96, 0.85};
    const glm::dvec3 O{1.00, 0.45, 0.10};
    f.color = O + (W - O) * u;
    return f;
}

// --- CAMERA SHAKE (the manned sight view only) ------------------------------
// ★ Small ON PURPOSE. The Oerlikon hammers at 7.5 Hz into a gunner strapped
// into two shoulder pads -- the sight picture SHIVERS, it does not swim. The
// scale is sub-milliradian angular plus a few mm of eye travel; anything
// larger and the pipper (the thing he actually shoots with) becomes unusable,
// which would be this rung breaking the last one. FEEL DIALS.
inline constexpr double kShakeTauS = 0.080;       // decay [s]
inline constexpr double kShakePerShot = 1.0;
inline constexpr double kShakeCeil = 1.60;        // saturation at the cyclic
inline constexpr double kShakeYawRad = 0.00045;   // 0.45 mrad lateral
inline constexpr double kShakePitchRad = 0.00060; // 0.6 mrad, biased UP
inline constexpr double kShakeEyeM = 0.004;       // 4 mm of eye travel

inline double shake_step(double amp, double dt, int shots) {
    return fk_env_step(amp, dt, shots, kShakeTauS, kShakePerShot, kShakeCeil);
}

// A pure VIEW offset. Deterministic in the SHOT INDEX -- never a wall clock,
// never a frame counter: the same shot always jolts the same way, so a smoke
// run reproduces and a frame stutter cannot change the picture.
struct ShakeSample {
    double yaw_rad = 0.0;    // + = view swings right
    double pitch_rad = 0.0;  // + = view swings up (biased positive: it climbs)
    double right_m = 0.0, up_m = 0.0, back_m = 0.0;  // eye translation
};

inline ShakeSample shake_sample(int shot_index, double amp) {
    ShakeSample s;
    const double a = amp <= 0.0 ? 0.0 : (amp > kShakeCeil ? kShakeCeil : amp);
    if (!(a > 0.0)) return s;  // exact zero in => a bit-identical view
    const double j = static_cast<double>(shot_index);
    const double r1 = fk_h1(j) * 2.0 - 1.0;        // [-1,1)
    const double r2 = fk_h2(j) * 2.0 - 1.0;        // [-1,1)
    s.yaw_rad = kShakeYawRad * a * r1;
    // The gun CLIMBS under recoil, so the pitch jolt never goes fully
    // negative -- a symmetric jitter reads as noise, a biased one reads as a
    // weapon.
    s.pitch_rad = kShakePitchRad * a * (0.55 + 0.45 * r2);
    s.right_m = kShakeEyeM * a * r2;
    s.up_m = kShakeEyeM * a * 0.6 * r1;
    s.back_m = kShakeEyeM * a * (0.6 + 0.4 * fk_h1(j + 0.5));
    return s;
}

// --- SNOW BLAST -------------------------------------------------------------
// ★ The Oerlikon sits on a 10 ft packed-snow pad [OP 909 working circle] and
// the muzzle gases go somewhere. On sustained fire they lift a low, wide,
// ground-hugging billow off the pad a few metres ahead of the mount.
//
// ★ RULED HERE (and surfaced in the report): the cloud is ANCHORED AT THE
// PAD, in front of the mount, at ALL elevations -- it swings with TRAIN but
// never with elevation. The cue is the blast striking the snow the gun is
// standing on, not a plume chasing the bore into the sky. Above the 45 deg
// knee the blast genuinely does point skyward and less of it reaches the pad,
// so the intensity is scaled DOWN to kBlastHighElevGain at the +87 deg stop
// rather than switched off -- a hard cut-off would pop mid-burst.
inline constexpr double kBlastTauS = 0.90;        // decay [s]: ~2 s to gone
inline constexpr double kBlastPerShot = 0.20;     // ~1 s of fire to build
inline constexpr double kBlastCeil = 1.0;
inline constexpr double kBlastDistM = 4.5;        // ahead of the mount [3-6 m]
inline constexpr double kBlastSpreadM = 3.2;      // lateral half-width [m]
inline constexpr double kBlastHeightM = 1.35;     // it stays LOW [m]
inline constexpr double kBlastLobeRM = 1.25;      // one lobe's radius [m]
inline constexpr double kBlastPeakAlpha = 0.55;   // centre alpha at full
inline constexpr double kBlastElevKneeRad = 45.0 * kDeg;
inline constexpr double kBlastHighElevGain = 0.15;  // at the +87 deg stop

inline double blast_step(double inten, double dt, int shots) {
    return fk_env_step(inten, dt, shots, kBlastTauS, kBlastPerShot, kBlastCeil);
}

// 1.0 at or below the knee, falling linearly to kBlastHighElevGain at the
// elevation stop. Monotone non-increasing in elevation, and never zero.
inline double blast_elev_gain(double elev_rad) {
    if (elev_rad <= kBlastElevKneeRad) return 1.0;
    const double span = kElevMaxRad - kBlastElevKneeRad;
    const double t = span > 0.0
                         ? fk_clamp01((elev_rad - kBlastElevKneeRad) / span)
                         : 1.0;
    return 1.0 - (1.0 - kBlastHighElevGain) * t;
}

struct BlastState {
    double alpha = 0.0;      // centre alpha; EXACTLY 0 when nothing to draw
    double dist_m = 0.0;     // ahead of the mount, along the TRAINED forward
    double spread_m = 0.0;   // lateral half-width
    double height_m = 0.0;   // how far it lifts off the pad
    double lobe_r_m = 0.0;   // one lobe's radius
    // ★ NEAR-WHITE WITH A COOL CAST, not the grey a smoke cloud wants: this
    // billows over a SNOWFIELD, and the first build shipped (0.88,0.90,0.94)
    // -- a grey that measured as drawn (32 k px in the diagnostic pass) and
    // read as NOTHING against ground already brighter than it was. Lifted
    // above the snow so it reads as lit dust, and it earns its silhouette
    // against the treeline and the mount shadow.
    glm::dvec3 color{0.98, 0.99, 1.00};  // linear RGB: sunlit snow dust
};

inline BlastState blast_state(double inten, double elev_rad) {
    BlastState b;
    const double a = fk_clamp01(inten) * blast_elev_gain(elev_rad);
    if (!(a > 0.0)) return b;
    b.alpha = kBlastPeakAlpha * a;
    b.dist_m = kBlastDistM;
    b.spread_m = kBlastSpreadM * (0.55 + 0.45 * a);
    b.height_m = kBlastHeightM * (0.50 + 0.50 * a);
    b.lobe_r_m = kBlastLobeRM * (0.60 + 0.40 * a);
    return b;
}

// --- F-PLACE (Chad ruled 2026-08-27, spec §1): the gun sits on the THREAT
// FLANK of its surface pump -- 60-100 m toward the ENEMY surface pump, so a
// strafer running at the pump crosses the gun's front and the gunner is out
// of the beaten zone. kFlankOffsetM is the middle of the ruled band; it is a
// PLACEMENT DIAL for the fly, not a measured fact.
inline constexpr double kFlankOffsetM = 80.0;

// Walk kFlankOffsetM (arc length, radius R) from the pump's unit direction
// toward the enemy pump's, on the sphere. Outputs the gun site's unit
// direction and the threat-bearing forward hint (toward the enemy, tangent
// enough for make_mount_frame's re-projection). False if the two pumps are
// parallel or antipodal (no defined bearing) -- caller keeps no gun rather
// than an arbitrary one.
inline bool flak_site(const glm::dvec3& u_pump, const glm::dvec3& u_enemy,
                      double R_m, double offset_m, glm::dvec3& u_site,
                      glm::dvec3& fwd_hint) {
    if (!(R_m > 0.0)) return false;
    const glm::dvec3 up = glm::normalize(u_pump);
    const glm::dvec3 ue = glm::normalize(u_enemy);
    glm::dvec3 t = ue - up * glm::dot(ue, up);
    const double tn = glm::length(t);
    if (tn < 1e-9) return false;
    t /= tn;
    const double th = offset_m / R_m;
    u_site = glm::normalize(up * std::cos(th) + t * std::sin(th));
    fwd_hint = ue - u_site * glm::dot(ue, u_site);
    if (glm::length(fwd_hint) < 1e-12) return false;
    return true;
}

// ============================================================================
// ★ STAGE C -- THE RELOAD YOU CAN SEE AND THE BRASS THAT PILES UP (immersion
// ladder 3/4). BOTH ARE COSMETIC: nothing below is read by the fire control,
// the reload timer, the aim, the pipper or any sim state. app/flak_tick.h --
// the 4 s reload FACT, rounds_left, spawned_accum -- is UNTOUCHED by this
// section; it only READS reload_left_s and the spawn count.
//
// ★ THE FRAME-ORDER CONTRACT AGAIN. brass_spawn_burst() is a FOURTH reader of
// the undrained FlakWorld::spawned_accum and MUST sit in the same window as
// the recoil and the three Stage B envelopes (app/main.cpp's flak input
// block): step_frame() ADDS, that block READS WITHOUT DRAINING, the audio
// block far below OWNS THE ZERO. Put it after the drain and no case ever
// ejects. The reload CLANKS are NOT in that window -- they are edges on
// reload_left_s, not shot counts, and they live with the audio.
// ============================================================================

// --- 1. THE DRUM SWAP -------------------------------------------------------
// ★ WHICH BRACKET. The stage brief said "outboard LEFT"; the GLB says
// otherwise and the GLB wins: assets/flak/flak_src/flak_gun_geom.py places
// flak_drum "offset to the RIGHT (+X)" and test_flak_gun.cpp pins
// `st_drum.x > 0.05`. The part that IS outboard left is the SIGHT
// (`sf.x < -0.05`, the eye-view finding). So the drum lifts out along the
// bracket toward +X -- and nothing here hardcodes that sign:
// render/flak_model.cpp takes it from the loaded st_drum node, so a re-export
// that moves the drum moves the animation with it (the LAW, paid 7x: a
// constant that describes the shipped table stops describing it the moment
// the table moves).
//
// ★ CLOSED FORM IN reload_left_s. No new state, no timer, no history: the
// whole three-phase swap is a function of the fraction of the reload still
// left, exactly as the Stage B envelopes are functions of a shot count.
// Phase 1 (first kDrumOutFrac): the empty drum translates OUT along the
// bracket and DROPS (quadratic -- it is falling). Middle: ABSENT, the feed
// throat bare. Phase 3 (last kDrumInFrac): a fresh one comes back the same
// way, reversed. At reload_left_s == 0 the transform is EXACTLY identity.
inline constexpr double kDrumOutFrac = 0.25;  // phase 1 share of the reload
inline constexpr double kDrumInFrac = 0.25;   // phase 3 share
inline constexpr double kDrumOutM = 0.42;     // outboard travel [m]
inline constexpr double kDrumDropM = 0.55;    // how far it falls away [m]

struct DrumSwap {
    bool visible = true;  // false = mid-swap, no drum on the gun at all
    double out_m = 0.0;   // along the bracket axis, outboard (model +/-X)
    double down_m = 0.0;  // model -Y
};

// reload_frac = reload_left_s / reload_s, in [0, 1]. 1 = the reload just
// began, 0 = no reload running (the rest state). EXACTLY 0 in => the identity
// DrumSwap out, and render/flak_model.cpp then does not touch the node at all.
inline DrumSwap drum_swap(double reload_frac) {
    DrumSwap d;
    if (!(reload_frac > 0.0)) return d;  // bit-identical rest
    const double p = fk_clamp01(1.0 - reload_frac);  // progress, 0 -> 1
    double u;                                        // 0 = seated, 1 = clear
    if (p < kDrumOutFrac) {
        u = p / kDrumOutFrac;
    } else if (p > 1.0 - kDrumInFrac) {
        u = (1.0 - p) / kDrumInFrac;
    } else {
        d.visible = false;
        return d;
    }
    u = fk_clamp01(u);
    d.out_m = kDrumOutM * u;
    d.down_m = kDrumDropM * u * u;  // it FALLS: quadratic, not linear
    return d;
}

// --- 2. THE SPENT BRASS -----------------------------------------------------
// ★ 20 x 110 RB Oerlikon case: 110 mm long, 24.9 mm rim [NavWeaps]. Drawn as
// one tiny bright cylinder -- DrawBillboard renders NOTHING in this raylib
// (the standing render note), so a case is a DrawCylinderEx or it is nothing.
//
// ★ WHICH WAY. st_eject sits at model (+RECEIVER_W/2 + 0.02, bore height,
// +0.35 fwd) -- the case chute is on the gun's RIGHT, the same side as the
// drum. The real gun throws its cases down and forward off that chute, so the
// ejection direction is RIGHT + DOWN with a little FORWARD, in CRADLE space:
// it swings with the train AND tips with the elevation, because the chute is
// bolted to the gun.
//
// ★ DETERMINISM. Per-case jitter is the low-discrepancy pair off the SHOT
// INDEX (fk_h1/fk_h2), never a clock and never an RNG -- the same shot always
// throws the same case the same way, so a smoke run reproduces the pile.
inline constexpr int kBrassCap = 150;          // ring buffer size
inline constexpr double kBrassLenM = 0.110;    // case length [m]
inline constexpr double kBrassRadM = 0.0125;   // case radius [m] (24.9 rim)
inline constexpr double kBrassSpeedMinMps = 3.0;
inline constexpr double kBrassSpeedMaxMps = 5.0;
inline constexpr double kBrassLifeS = 52.0;    // inside the ruled 45-60 s band
inline constexpr double kBrassFadeS = 7.0;     // the last seconds fade out

struct BrassCase {
    glm::dvec3 pos{0.0};        // world
    glm::dvec3 vel{0.0};        // world [m/s]
    glm::dvec3 axis{0, 0, 1};   // world unit: the case's long axis
    double age_s = 0.0;
    bool active = false;
    bool at_rest = false;
};

struct BrassPool {
    std::vector<BrassCase> cases;  // sized to kBrassCap on first spawn
    int next = 0;                  // ring write cursor
    long long spawned = 0;         // total ever ejected (diagnostics/tests)
};

inline int brass_live(const BrassPool& bp) {
    int n = 0;
    for (const BrassCase& c : bp.cases)
        if (c.active) ++n;
    return n;
}

// One case, ejected from st_eject under the CURRENT pose. shot_index is the
// running shot counter -- the only source of the jitter.
inline void brass_spawn(BrassPool& bp, const MountFrame& f, const Stations& s,
                        const Pose& pose, long long shot_index) {
    if (bp.cases.size() != static_cast<std::size_t>(kBrassCap))
        bp.cases.assign(kBrassCap, BrassCase{});
    const double j = static_cast<double>(shot_index);
    const double a = fk_h1(j);
    const double b = fk_h2(j);
    const double c = fk_h1(j + 0.37);
    // Cradle-space throw: RIGHT, DOWN, a little FORWARD, with jitter.
    const glm::dvec3 d_model = glm::normalize(
        glm::dvec3(1.0, -0.55 + 0.30 * (b - 0.5), 0.10 + 0.30 * (c - 0.5)));
    const glm::dquat q = train_quat(pose.train_rad) * cradle_quat(pose.elev_rad);
    const glm::dvec3 dir = glm::normalize(model_dir_to_world(f, q * d_model));
    const double speed =
        kBrassSpeedMinMps + (kBrassSpeedMaxMps - kBrassSpeedMinMps) * a;
    BrassCase& k = bp.cases[static_cast<std::size_t>(bp.next)];
    bp.next = (bp.next + 1) % kBrassCap;
    k = BrassCase{};
    k.pos = cradle_station_world(f, s, pose, s.eject);
    k.vel = dir * speed;
    k.axis = dir;  // it leaves the chute end-on; brass_step lays it flat
    k.active = true;
    ++bp.spawned;
}

// Spawn `shots` cases in one frame (the per-frame read of spawned_accum).
// shot_seq_before is the shot counter BEFORE this frame's shots, so case i
// gets index shot_seq_before + i -- unique, monotone, reproducible.
inline void brass_spawn_burst(BrassPool& bp, const MountFrame& f,
                              const Stations& s, const Pose& pose, int shots,
                              long long shot_seq_before) {
    for (int i = 0; i < shots; ++i)
        brass_spawn(bp, f, s, pose, shot_seq_before + i);
}

// Ballistic step + landing on the pad. The pad is the flat plane `pad_h_m`
// above f.pos along f.up (OP 909's 10 ft working circle is packed flat -- ONE
// height, no per-case terrain query, by ruling). g > 0 [m/s^2].
//
// ★ WHY pad_h_m EXISTS, and it is not a nicety. A gun's `pos` is the BARE
// TERRAIN radius -- app/main.cpp F-PLACE grounds it there deliberately, "the
// pedestal's snow pad is 0.15 m deep, so base-at-surface buries the crib, top
// flush". The SNOW the player sees is up to 0.75 m ABOVE that. The first
// build rested the cases at f.pos and every one of them was correctly
// simulated, correctly drawn, and completely INVISIBLE -- buried under the
// snowpack. The pile is the whole point of this rung, so the rest plane is
// the DRAWN surface, and the caller (which owns the snowpack field) supplies
// its height. Defaulted to 0 so the pure tests keep their flat pad.
inline void brass_step(BrassPool& bp, const MountFrame& f, double g, double dt,
                       double pad_h_m = 0.0) {
    if (!(dt > 0.0)) return;
    const double rest_h = pad_h_m + kBrassRadM;
    for (BrassCase& k : bp.cases) {
        if (!k.active) continue;
        k.age_s += dt;
        if (k.age_s >= kBrassLifeS) {
            k = BrassCase{};
            continue;
        }
        if (k.at_rest) continue;
        k.vel -= f.up * (g * dt);
        k.pos += k.vel * dt;
        const double h = glm::dot(k.pos - f.pos, f.up);
        if (h <= rest_h) {
            // Lay it flat ON the pad: put it at exactly one radius above the
            // drawn surface (so the cylinder touches and never crosses) and
            // turn the long axis into the tangent plane -- a case at rest lies
            // on its side, it does not stand up in the snow.
            k.pos -= f.up * (h - rest_h);
            glm::dvec3 lay = k.axis - f.up * glm::dot(k.axis, f.up);
            if (glm::length(lay) < 1e-9) lay = f.fwd0;
            k.axis = glm::normalize(lay);
            k.vel = glm::dvec3(0.0);
            k.at_rest = true;
        } else if (glm::length(k.vel) > 1e-9) {
            k.axis = glm::normalize(k.vel);  // it tumbles nose-first in flight
        }
    }
}

// 1 until the fade window, then linearly to exactly 0 at kBrassLifeS.
inline double brass_alpha(double age_s) {
    if (age_s >= kBrassLifeS) return 0.0;
    const double t0 = kBrassLifeS - kBrassFadeS;
    if (age_s <= t0) return 1.0;
    return fk_clamp01((kBrassLifeS - age_s) / kBrassFadeS);
}

}  // namespace render::flak
