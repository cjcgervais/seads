#include "sim/sled.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

// THE SLED KERNEL (WINTER_LAW §3). See sim/sled.h for the seal statement.
//
// Structure of one substep, and the order is the model:
//   1. per patch: sample THE ground once, resolve suspension travel (real
//      state), get the normal load
//   2. per patch: advance SINKAGE as a rate process -- dwell buries, travel
//      sheds (§3.5a)
//   3. per patch: planing lift, displacement drag, lateral bite
//   4. track: shear-limited thrust out of slip and the ONE roost product
//   5. sum forces and torques AT THE PATCH POSITIONS, integrate the rigid body
//
// Nothing in here tests "is it planing", "is it stuck", or "did it roll" in
// order to decide what to do next. Those words name regions of a continuous
// output, which is the whole of §3's ruling.

namespace sim {
namespace {

constexpr double kEps = 1e-9;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

struct PatchGeom {
    glm::dvec3 mount{0.0};  // body-frame suspension attachment
    double len = 0.0;
    double width = 0.0;
    // Half the patch's BEARING width: nonzero means the normal reaction is
    // applied at TWO points this far outboard, load split by their own
    // penetration -- the patch's roll restoring moment, which a single
    // centreline application throws away (PACKET_B §15, §8 P0-5).
    double rail_half = 0.0;
    // Half the patch's BEARING length: nonzero means the normal reaction is
    // ALSO split fore/aft this far along body +Z, load split by those two
    // points' own penetration -- the patch's PITCH restoring moment, which a
    // single mid-patch application throws away exactly as the centreline
    // application threw the roll one away (GI S2b; PACKET_B §15 rotated 90
    // deg).
    double pitch_half = 0.0;
    bool steered = false;  // skis follow the handlebar; the track does not
    bool is_track = false;
};

// ★ THE RIDER SUBTRACTS FROM EVERY MOUNT (§9d, §8 P2). `cg_off` is the
// composite-CG offset in body frame -- moving the rider's mass moves where
// the CG effectively sits, which (held the OTHER way, mount-relative-to-CG)
// is exactly a mount offset in the opposite direction. Called PER SUBSTEP:
// the rider's displacement is real state that advances every substep, so the
// mount geometry must track it every substep too, not once per step.
void patch_geometry(const SledParams& p, const glm::dvec3& cg_off,
                    PatchGeom g[kPatches]) {
    const double half = 0.5 * p.stance_m;
    // ★ THE MOUNTS SIT BELOW THE CG, and that offset is the whole of the
    // weight-transfer and rollover story. Placed so that at rest hang
    // (== susp_rest_m) the running surface is exactly cg_height_m below the CG.
    // With the mounts at y = 0 -- which is how this was first written -- every
    // patch force passes through the CG, cg_height_m does nothing, throttle
    // cannot unload the skis and the machine cannot tip. §2.4c.1's ruling is
    // this one line of geometry.
    const double my = -(p.cg_height_m - p.susp_rest_m);
    g[static_cast<int>(Patch::SkiLeft)] = {
        glm::dvec3(-half, my, -p.ski_fwd_m) - cg_off, p.ski_len_m,
        p.ski_width_m, 0.0, 0.0, true, false};
    g[static_cast<int>(Patch::SkiRight)] = {
        glm::dvec3(half, my, -p.ski_fwd_m) - cg_off, p.ski_len_m,
        p.ski_width_m, 0.0, 0.0, true, false};
    // ★ THE SPLITS ARE THE TRACK'S ALONE. The skis are two patches already
    // separated across the machine's width, and their own bearing length is
    // not what the launch wheelie stands on -- GI S2b corrects the ONE patch
    // that carries the whole machine once the nose is up.
    g[static_cast<int>(Patch::Track)] = {
        glm::dvec3(0.0, my, p.track_aft_m) - cg_off, p.track_len_m,
        p.track_width_m, std::max(p.track_rail_half_m, 0.0),
        std::max(p.track_pitch_half_m, 0.0), false, true};
}

// First-order slew toward `target`, rate-capped -- "a body cannot teleport"
// (§9d.7): a bare rate limiter alone has a corner at the target; a bare
// exponential alone has no bound on how fast the middle can move. Both
// together read `lean_tau_s` (95 % in ~3 tau) AND `lean_rate_ms` (never
// exceeded), which is what leg 7 (sled_lean_slew_is_bounded) pins.
double slew_toward(double current, double target, double tau_s,
                   double rate_cap, double h) {
    double step = tau_s > kEps ? (target - current) * (1.0 - std::exp(-h / tau_s))
                               : (target - current);
    step = std::clamp(step, -rate_cap * h, rate_cap * h);
    return current + step;
}

// Bekker's equilibrium sinkage for a bearing pressure. p = (k_c/b + k_phi) z^n
// inverted; `b` is the patch's SHORT dimension, which is what makes a narrow
// ski cut deeper than a wide track under the same load -- the reason a sled has
// a track at all.
//
// ★ AND THE PACK SOFTENS WITH DEPTH -- a named approximation, on the record.
// Bekker's constants describe one snow, and our field gives DEPTH, not density.
// Using them unscaled makes a 1.75 m drainage bottom bear exactly like a 0.77 m
// bush floor, which contradicts §2.2a's ruling that the p99 drainage lines are
// "genuinely hard ground, at the deep end of what will plane". Depth is a good
// proxy for consolidation IN THIS WORLD and not by coincidence: §2.2's own
// function derives depth from curvature and aspect, so the deep places ARE the
// hollows and lee slopes that collect loose drift, and the thin places ARE the
// wind-scoured crests. Stiffness is therefore scaled off the SIGNED median
// depth -- at 0.77 m the shipped Bekker constants apply exactly, and the
// reference is deliberately the number Chad signed rather than a free dial.
double pack_modulus(const SledParams& p, double b, double depth_m) {
    const double k = p.bekker_kc / std::max(b, kEps) + p.bekker_kphi;
    // ★ SC1 COLD IS POWER (WINTER_LAW §3.7, SC1_COLD_SPEC.md §3): the ONE live
    // cold chain. `snow_hardness` (world::hardness, written by the app once
    // per tick) scales the GLOBAL Bekker stiffness -- cold sinters the pack
    // stiffer, sinkage drops (z ~ modulus^(-1/n)), plow drag and track burial
    // fall with it, and top speed rises. Applied here because pack_modulus is
    // already surface-gated by its ONLY caller (reached under d.sinkable,
    // sled.cpp step_sled) -- no second gate, no SurfaceDials field added.
    return p.snow_hardness * k *
          std::pow(p.pack_ref_depth_m / std::max(depth_m, 0.02), p.pack_soften);
}


// ★ N2 LAKE-ICE BITE (sim/sled.h SledComfort::ice_bite_mu): the patch speed
// [m/s] at which the added low-speed ski bite has faded to EXACTLY zero. A
// CONSTANT, not a second dial (one dial per rung): the clutch engages at 3.25
// m/s, and every turn on his tape 90 sits in the 13-27 m/s band, which this
// leaves bit-untouched.
constexpr double kIceBiteVrefMs = 8.0;

// ★ N1 THE LEG WORK (SledComfort::leg_work_nm) -- the stage bands, constants
// not dials (one dial per rung). Enter / exit pairs are the hysteresis; the
// dwell reuses rolled_persist_s and the direction cone reuses right_dir_eps.
//   PITCHED : |up_body.z| >= sin 50 enter, < sin 40 exit, AND |up_body.x| <
//             sin 35 (a machine on its side is the pendulum's, not this)
//   INVERTED: up_body.y <= cos 150 enter, > cos 140 exit
//   ON_SIDE : |up_body.x| >= sin 50 (readout only; no new torque)
// Measured against the rest poses in test_sled_legwork.cpp: nose-up rests
// at |z| 0.96, inverted at y -0.99 -- both well inside their enter bands.
constexpr double kLegPitchEnter = 0.76604444311897801;   // sin 50 deg
constexpr double kLegPitchExit = 0.64278760968653925;    // sin 40 deg
constexpr double kLegSideBand = 0.57357643635104605;     // sin 35 deg
constexpr double kLegInvEnter = -0.86602540378443860;    // cos 150 deg
constexpr double kLegInvExit = -0.76604444311897801;     // cos 140 deg
constexpr double kLegRollShare = 0.5;                    // pitch : roll = 1 : 0.5

}  // namespace

SledParams::SledParams() {
    auto set = [&](world::Surface s, SurfaceDials d) {
        dials[static_cast<int>(s)] = d;
    };
    // ONE table, indexed by world::Surface (§2.3). mu_lat is where almost all
    // the feel lives (§3.2), and ice takes your steering away BY DATA rather
    // than by a special case.
    // ★ S3 (PACKET_B §15): sourced to the real packed-snow lateral mu band
    // (0.40-0.50 g) plus a trail feel margin -- the old values (0.80/1.05/
    // 1.02) sat above the machine's own tip threshold and made rollover, not
    // slide, the failure mode on flat ground.
    // ★★ §8 P1-7 APPLIED (Phase K, measured via `seads_sled_probe mu_check`):
    // TrailMain's peak a_lat measured 0.995x a_tip (0.717g vs 0.720g,
    // roll-decoupled, full-lock sweep at v0 8-22 m/s) -- INSIDE the 10 %
    // band, so the conditional fires: 0.75 -> 0.70.
    // ★ pack_k_scale (last column): the groomer's sinter (OPEN-VP2 CLOSED,
    // 2026-08-11). Bush is 1.0 EXACTLY -- natural pack bit-identical by x1.0
    // multiply. Trail values calibrated by measurement (`seads_sled_probe
    // topspeed`, k-sweep 1/7/12/100 -> 23.8/35.4/37.7/45.2 m/s): 12.0 puts
    // TrailMain at 37.7 m/s = 136 km/h -- a real 650's groomed-trail pace --
    // and carries the machine at centimetre sinkage, which is what a groomed
    // corridor IS. The asymptote (k=100, 45.2 m/s) is the power/track-speed
    // ceiling, so the dial saturates physically rather than running away.
    // Tributary is narrower and less groomed: ~2/3 the sinter.
    // ★ GI S1 (P1-1, "the mixed-length aggregate trap"): every row below is
    // now written with ALL EIGHT fields explicit, including the trailing
    // `pack_k_scale`/`mu_brake` this build adds -- the four non-sinkable rows
    // used to rely on `pack_k_scale`'s 1.0 default member initializer via a
    // short 6-field brace, which is exactly the trap this comment is pinning
    // (a silently-defaulted trailing field is indistinguishable, at the call
    // site, from a deliberately-typed one; adding mu_brake as ANOTHER
    // trailing field would have repeated the mistake one column over). None
    // of the four's pack_k_scale values change behaviour (pack_modulus is
    // only ever reached under `d.sinkable`, sled.cpp:118-121) -- this is
    // purely closing the trap, not a tuning change.
    //                        mu_kin mu_lat  c[Pa]  phi   rho_eff sinkable k     mu_brake
    set(world::Surface::Bush,
        {0.045, 0.55, 1200.0, 22.0, 260.0, true, 1.0, 0.66});
    set(world::Surface::TrailMain,
        {0.032, 0.70, 9000.0, 28.0, 380.0, true, 12.0, 0.72});
    set(world::Surface::TrailTributary,
        {0.034, 0.72, 8600.0, 28.0, 380.0, true, 8.0, 0.70});
    // Plowed and BARE (§2.4c): carbide runners on pavement. High drag, no
    // float, and the sparks S5 adds are gated on exactly this class.
    // ★ GI S2.2/S2.4 (ground_interaction_spec.md): mu_kin 0.140 -> 0.22
    // (honest carbide-on-pavement drag, measured against the S1 brake
    // ordering leg -- see docs/gi_measurements.md). mu_lat 0.55 -> 0.60:
    // swept {0.55, 0.60, 0.65, 0.70} against the S2.7 360 leg (full table in
    // docs/gi_measurements.md and at that leg) -- ★ STOP CONDITION: v0=20
    // m/s rolls at EVERY candidate in the ruled band, so no value clears
    // the leg fully; 0.60 is the measured BEST-OF-BAND (fails only v0=20,
    // where every other candidate also fails v0=16), shipped with v0=20 left
    // as a reported, non-gating finding rather than a silently-forced pass.
    // ★ GI S1: mu_brake GUESS-retuned against
    // sled_brake_ordering_road_beats_snow_beats_ice on the post-S2b kernel --
    // see docs/gi_measurements.md §Phase S1 for the swept ladder and the
    // shipped per-surface decel table.
    set(world::Surface::Road,
        {0.220, 0.60, 25000.0, 30.0, 0.0, false, 1.0, 0.95});
    set(world::Surface::LakeIce,
        {0.020, 0.22, 20000.0, 12.0, 0.0, false, 1.0, 0.24});
    // ★ GI S2.4: mu_kin 0.200 -> 0.28 (added to the S2.5 empirical fence
    // sweep, P2-8).
    set(world::Surface::RockOutcrop,
        {0.280, 0.60, 30000.0, 32.0, 0.0, false, 1.0, 0.70});
    set(world::Surface::MineWorks,
        {0.130, 0.58, 25000.0, 30.0, 0.0, false, 1.0, 0.68});
}

namespace {
// *** SK-1a: linear blend of the CONTINUOUS dials. `sinkable` is a bool and
// cannot be lerped -- it takes the DOMINANT class, so it flips at mix 0.5
// where every continuous term is already half-way, instead of at the class
// boundary where they all stepped together.
SurfaceDials blend_dials(const SurfaceDials& a, const SurfaceDials& b,
                         double t) {
    const double u = 1.0 - t;
    SurfaceDials r = a;
    r.mu_kin = u * a.mu_kin + t * b.mu_kin;
    r.mu_lat = u * a.mu_lat + t * b.mu_lat;
    r.c_snow_pa = u * a.c_snow_pa + t * b.c_snow_pa;
    r.phi_deg = u * a.phi_deg + t * b.phi_deg;
    r.rho_eff = u * a.rho_eff + t * b.rho_eff;
    r.pack_k_scale = u * a.pack_k_scale + t * b.pack_k_scale;
    r.mu_brake = u * a.mu_brake + t * b.mu_brake;
    r.sinkable = (t < 0.5) ? a.sinkable : b.sinkable;
    return r;
}
}  // namespace

SledState step_sled(const SledState& state, const SledInputs& in,
                    const SledParams& p, const world::SnowpackField& snow,
                    double dt, SledDebugSink* dbg) {
    SledState s = state;
    const int n = std::max(1, p.substeps);
    const double h = dt / static_cast<double>(n);

    const glm::dvec3 I = p.inertia;
    const double mass = std::max(p.mass_kg, kEps);

    // Accumulators reported once at the end -- the LAST substep's values, so
    // the dash reads the state the caller is about to render.
    double rep_plane_num = 0.0, rep_plane_den = 1.0;
    double rep_slip = 0.0, rep_flux = 0.0, rep_thrust = 0.0;
    double rep_depth = 0.0;
    double rep_belt = 0.0, rep_rpm = 1700.0;
    double rep_rotor_L = 0.0;  // G1: signed lateral rotor momentum
    world::Surface rep_surf = world::Surface::Bush;

    for (int step_i = 0; step_i < n; ++step_i) {
        const glm::dmat3 R = glm::mat3_cast(s.orientation);
        const glm::dvec3 body_up = R * glm::dvec3(0.0, 1.0, 0.0);
        const glm::dvec3 body_fwd = R * glm::dvec3(0.0, 0.0, -1.0);
        const glm::dvec3 up_cg = glm::normalize(s.position);
        const glm::dvec3 world_omega = R * s.angular_vel;

        // ★ A ROLLOVER ENDS THE RUN (§2.4c.1, and no fake dismount before S8).
        // `rolled` is a READOUT of the attitude the dynamics produced -- there
        // is no if(angle > X) anywhere in the force path above it, and the
        // machine is not righted, teleported, or respawned here.
        // ★ RC item D (§0b): the readout is GATED + PERSISTENT. The old
        // instantaneous form latched MID-AIR on legitimate sends (a backflip
        // at 165 deg read as a rollover) and cut throttle at the exact moment
        // a rider would blip it for attitude. Now the >75 deg condition must
        // HOLD for rolled_persist_s while in ground contact within
        // rolled_grace_s (air_s is advanced after the contact loop below).
        // Fixing this is MORE honest, not less — the old readout was a lying
        // instrument by the kernel's own P1-10 standard.
        const bool attitude_now = glm::dot(body_up, up_cg) < std::cos(1.31);
        if (attitude_now && s.air_s <= p.comfort.rolled_grace_s)
            s.rolled_hold_s = std::min(s.rolled_hold_s + h,
                                       p.comfort.rolled_persist_s + 0.5);
        else
            s.rolled_hold_s = std::max(0.0, s.rolled_hold_s - 2.0 * h);
        // persist = 0.0 makes this exactly the old instantaneous readout.
        s.rolled = attitude_now &&
                   s.rolled_hold_s >= p.comfort.rolled_persist_s;
        // ★★★ R4a §7.4 STAGE 2 -- NO HANDS, NO CONTROLS. ONE FACT, NOT THREE
        // RULES. Chad's causal chain for the release is a chain and not a list:
        //
        //   "hands release -> throttle springs to idle (the thumb lever is
        //    spring-loaded and there is no thumb on it any more) -> engine
        //    drops to idle rpm -> the clutch falls below engagement -> no drive
        //    at the track -> the track's own drag decelerates it -> it coasts,
        //    slowing but free, and comes to rest -- idling, and does not creep."
        //
        // ★ EVERYTHING AFTER THE FIRST ARROW ALREADY EXISTS AND IS UNTOUCHED.
        // The clutch engagement below is `clamp((throttle - 0.02) / 0.08)`, so
        // a throttle of 0 disengages it, the belt commands nothing, and the
        // track's own drag is the only longitudinal term left. NO CREEP is not
        // a rule imposed here -- it is what an unpowered track does, and the
        // kernel already said so. This block writes the FIRST arrow only.
        //
        // ★ AND THE FORM IS THE SHIPPED ONE, NOT A NEW ONE: `s.rolled ? 0.0`
        // has cut the throttle on this exact line for rungs. `hands_on` joins
        // it as a second reason for the same zero.
        //
        // ⚠ THE BRAKE AND THE BARS GO WITH IT, AND THAT IS THE SAME FACT, NOT
        // AN EXTRA ONE: the brake is a HAND LEVER and the steering is a HANDLE
        // BAR. A man who has let go of the bars is not squeezing the lever and
        // is not turning them. The steer COMMAND goes to centre and
        // `steer_actual` slews there at the shipped rate -- the bars fall back,
        // they do not snap. §7.4 names only the throttle because the throttle
        // is the one with a chain hanging off it; it does not license a
        // riderless machine that still steers.
        const bool hands_on = s.grip.attached;
        // ★★ B1 SPLIT (ladder_v2 §4.1, Chad 2026-09-18: "if I key press
        // throttle should ramp up"). ONE ternary became two statements, and
        // the two halves are DIFFERENT FACTS:
        //   `!hands_on` -> an unconditional zero. A man off the bars has no
        //     thumb on the lever (R4a §7.4). Not this dial's business, and
        //     `sled_rolled_throttle_never_reaches_a_handless_rider` keeps the
        //     two from being collapsed back into one.
        //   `s.rolled` -> scaled by `comfort.rolled_throttle_frac`, which SHIPS
        //     AT 0.0 and is therefore the shipped kernel exactly: `0.0 * x ==
        //     0.0` for every finite clamp01'd x, the same IEEE zero the literal
        //     produced.
        // ⚠ THE ORDER MATTERS AND IT IS THE SIGN TRAP: it is `frac * thr_in`,
        // never `(1 - frac) * thr_in`. The complement form delivers FULL
        // throttle at the identity value; `sled_rolled_throttle_frac_zero_is_
        // the_shipped_zero` exists to red exactly that slip.
        //
        // ★★ FOLDED RED-TEAM P1-4 (law+feel, 2026-09-18): THE PRODUCT IS
        // CLAMPED. `thr_in` is clamp01'd; the PRODUCT was not, and the env
        // route this build drives on has no range check the TOML route has.
        // `SEADS_SLED_ROLLED_THROTTLE=1.5` put 1.5 into `v_cmd`, `engine_rpm`
        // (11150), the moment arm and the HUD -- outside the kernel's
        // documented [0,1] domain for `throttle`, reachable only while rolled.
        // `clamp01` here is the IDENTITY FUNCTION for every frac <= 1.0 with
        // thr_in in [0,1], and at frac == 0.0 `clamp01(0.0)` is the same +0.0
        // the literal produced -- so the identity and the tape corpus are
        // untouched and only the out-of-band case changes.
        const double thr_in = hands_on ? clamp01(in.throttle) : 0.0;
        const double throttle =
            s.rolled ? clamp01(p.comfort.rolled_throttle_frac * thr_in)
                     : thr_in;
        const double brake = hands_on ? clamp01(in.brake) : 0.0;

        // --- steer slew (§3.5, feel not the rollover fix, PACKET_B §15.4) ---
        // `steer_actual` is what the rig reads AND what `delta` is computed
        // from below -- never the raw command.
        {
            const double steer_target =
                hands_on ? std::clamp(static_cast<double>(in.steer), -1.0, 1.0)
                         : 0.0;
            s.steer_actual = slew_toward(s.steer_actual, steer_target, 0.0,
                                         p.steer_rate_per_s, h);
        }
        const double delta = s.steer_actual * p.steer_max_rad;

        // --- the rider (§9d, §8 P2): real state, slewed PER SUBSTEP ---------
        // The reach box opens with how far the rider has ACTUALLY risen, read
        // BEFORE this substep's slew -- "a held key is a held muscle, with no
        // clamp, no threshold and no state machine."
        const double rise_frac_prev =
            clamp01(s.rider_up_m / std::max(p.stand_rise_m, kEps));
        const double lat_reach_m =
            p.lean_lat_seated_m +
            (p.lean_lat_stand_m - p.lean_lat_seated_m) * rise_frac_prev;
        // ★ AND THE WEIGHT-SHIFT GOES THE SAME WAY, FOR THE SAME REASON. The
        // lean axes are a MAN MOVING HIS BODY ON THE MACHINE (§9d); once he is
        // not on it there is no body to move, so the commands read zero and the
        // stored displacement SLEWS home at the shipped `lean_tau_s` -- the
        // ghost of his last pose relaxing, not a snap. The slew is deliberate:
        // a discontinuous jump to centre is exactly the defect-8 pop §7.8 is
        // about, and `cg_off` below is zeroed by the mass fraction anyway, so
        // nothing the machine feels depends on how fast this decays.
        const double lean_lat_cmd =
            hands_on ? std::clamp(static_cast<double>(in.lean_lat), -1.0, 1.0)
                     : 0.0;
        const double lean_fwd_cmd =
            hands_on ? std::clamp(static_cast<double>(in.lean_fwd), -1.0, 1.0)
                     : 0.0;
        const double stand_cmd =
            hands_on ? std::clamp(static_cast<double>(in.stand), -1.0, 1.0)
                     : 0.0;
        const double target_lat_m = lean_lat_cmd * lat_reach_m;
        // Fore/aft clamp is ASYMMETRIC on one signed value (§8 P2): aft is
        // bounded by arm reach, not seat length, so it is a different number
        // than forward.
        // ★★★ K-WS1 / K1. THE AFT CEILING IS STAND-DEPENDENT NOW, AND IT IS
        // THE ANIMATION'S OWN MEASURED ROW (sim/sled.h kAftCeilC1).
        //
        // The ladder carries 0.2786 m of rider CG aft when he is seated (the
        // kneel does it) and only 0.2021 m when he is standing, and the row
        // between is CONCAVE -- so the ceiling is read piecewise-linearly out
        // of the five measured slices, never lerped end-to-end.
        //
        // rise_frac_prev is deliberately the SAME number the lateral reach box
        // above reads: one rise per substep, taken before this substep's slew,
        // "a held key is a held muscle". It is also identically the animation's
        // `s` (both are clamp01(rider_up_m / stand_rise_m)), single-sourced --
        // so the kernel's demand and the body's delivery are indexed by the
        // same coordinate and cannot drift.
        //
        // ★ 0-OFF IS STRUCTURAL: at aft_ceiling_curve == 0 the branch is not
        // taken and `aft_max_eff` is the flat `lean_aft_max_m`, byte for byte
        // the pre-rung kernel.
        //
        // ★ K-A5(a), STATED: while the rider is standing UP with aft already
        // held, this ceiling FALLS, and the clamp below drags rider_fwd_m
        // forward with it. That is not a glitch -- a man standing up cannot
        // hold the seated kneel's reach -- and in flight that dragged motion
        // is a real rider velocity that FEEDS the K2 exchange term below. It
        // is physics, it is tested
        // (sled_the_stand_ceiling_drags_the_aft_lean_and_K2_feels_it), and it
        // is the one place K1 and K2 are coupled.
        double aft_max_eff = p.lean_aft_max_m;
        if (p.aft_ceiling_curve > 0.0) {
            const double t = rise_frac_prev * (kAftCeilSlices - 1);
            int i = static_cast<int>(t);
            if (i > kAftCeilSlices - 2) i = kAftCeilSlices - 2;
            const double fr = t - static_cast<double>(i);
            const double row =
                kAftCeilC1[i] + fr * (kAftCeilC1[i + 1] - kAftCeilC1[i]);
            aft_max_eff =
                p.lean_aft_max_m +
                p.aft_ceiling_curve * (row - p.lean_aft_max_m);
        }
        const double target_fwd_m = lean_fwd_cmd >= 0.0
                                        ? lean_fwd_cmd * p.lean_fwd_max_m
                                        : lean_fwd_cmd * aft_max_eff;
        // `stand > 0` rises toward stand_rise_m; `stand < 0` tucks toward
        // tuck_drop_m -- two different rates on one signed axis (§8 P2).
        const double target_up_m =
            stand_cmd > 0.0 ? stand_cmd * p.stand_rise_m
                           : stand_cmd * p.tuck_drop_m;
        // ★ `lean_return_tau_s` is a structural OFF-arm (§9d.3): side-hilling
        // must be HOLDABLE, so there is no spring pulling the rider back to
        // centre anywhere below -- the slew targets are the commanded
        // position, full stop. The field exists for Chad to A/B, unconsumed
        // at 0.0.
        // ★ K-WS1 / K2: the rider's offset BEFORE this substep's slew. The
        // exchange term needs his velocity relative to the chassis, and a
        // state delta over the substep is the only deterministic, clock-free,
        // tape-safe way to have it.
        const glm::dvec3 rider_r_prev(-s.rider_lat_m, s.rider_up_m,
                                      -s.rider_fwd_m);
        // ★ THE SELF-RIGHT'S OWN BRACE, added to whatever he is commanding.
        // "STANDING AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED": standing to
        // heave the machine over puts him on the uphill board. right_shift_cmd
        // is 0 unless the self-right is active, so this term is inert in all
        // ordinary riding and on every existing tape.
        const double lat_target_sr =
            std::clamp(target_lat_m + s.right_shift_cmd *
                                          p.comfort.right_stand_shift_frac *
                                          p.lean_lat_stand_m,
                       -p.lean_lat_stand_m, p.lean_lat_stand_m);
        s.rider_lat_m = slew_toward(s.rider_lat_m, lat_target_sr, p.lean_tau_s,
                                    p.lean_rate_ms, h);
        s.rider_fwd_m = std::clamp(
            slew_toward(s.rider_fwd_m, target_fwd_m, p.lean_tau_s,
                       p.lean_rate_ms, h),
            -aft_max_eff, p.lean_fwd_max_m);
        s.rider_up_m = slew_toward(s.rider_up_m, target_up_m, p.lean_tau_s,
                                   p.lean_rate_ms, h);
        // ★★★ K-WS1 / K2. THE EXCHANGE MOMENTUM, THIS SUBSTEP.
        // r_rel is the rider's position RELATIVE TO THE MACHINE: his measured
        // seated offset (sim/sled.h kRiderSeatOff*) plus his displacement. It
        // is NOT the displacement alone -- a pure aft throw would then have r
        // parallel to v and the whole term would vanish (see sled.h). It is
        // also NOT cg_off, which is the displacement scaled by the rider's
        // mass fraction and would under-scale a quadratic quantity by 0.070x.
        // The value is stored on EVERY substep; it is only SPENT below, while
        // airborne, at k_air_shift > 0.
        const glm::dvec3 rider_r_now(-s.rider_lat_m, s.rider_up_m,
                                     -s.rider_fwd_m);
        const glm::dvec3 rider_v_rel = (rider_r_now - rider_r_prev) / h;
        const glm::dvec3 rider_r_rel =
            rider_r_now +
            glm::dvec3(0.0, kRiderSeatOffY_m, kRiderSeatOffZ_m);
        // ★★★ R4a §7.7 STAGE 2 -- THE 87.5 kg LEAVES. This is the whole of what
        // §7.7 says the release does to the machine, and it is deliberately not
        // more than that: "when he releases, `rider_mass_kg` (87.5) leaves the
        // machine: `cg_off` and `patch_geometry` both change, and a sled
        // tumbling riderless is physically a different machine."
        //
        // ONE fraction, read by BOTH consumers (this exchange term and `cg_off`
        // below), because they are two spendings of the same fact and a second
        // copy is how they drift apart.
        //
        // ⚠⚠ AND THE HONEST GAP, NAMED RATHER THAN DISCOVERED LATER: `mass`
        // ITSELF IS STILL 331 kg. The composite total and the inertia tensor
        // (`p.inertia`, measured AT 331) do not change when he goes, so the
        // riderless machine still has a rider's worth of weight and swing in
        // its gravity, drag and rotation. §7.7 names `cg_off` and
        // `patch_geometry` and names nothing else, and dropping 87.5 kg out of
        // `mass` without re-deriving the inertia it was measured with is
        // exactly the trap `sim/sled.h` warns about in its own words ("a mass
        // move that leaves inertia behind is the silent version of the §1
        // trap"). So the spec is built as written and the gap is written down.
        // It is a REAL gap: a riderless Indy weighs 243.5 kg and would coast
        // differently. R4b, with the inertia re-derived, is where it belongs.
        //
        // ⚠ READ THROUGH `hands_on`, THE SAME LOCAL THE CONTROLS ABOVE READ,
        // and not a second `s.grip.attached` here. They are the same value
        // today -- `grip_step` runs at the END of this substep -- and that is
        // precisely why a second read is a trap: the day anything moves the
        // grip step earlier, one half of the machine would let go a substep
        // before the other and nothing would say so.
        const double rider_frac =
            hands_on ? (p.rider_mass_kg / std::max(mass, kEps)) : 0.0;
        // The exchange is a man throwing his mass about in flight. There is no
        // man. (Inert either way at the shipped k_air_shift 0 -- but a term
        // that is only correct because its dial is off is not correct.)
        const double mu_exch =
            hands_on ? p.rider_mass_kg * (mass - p.rider_mass_kg) /
                           std::max(mass, kEps)
                     : 0.0;
        const glm::dvec3 exch_l_now =
            mu_exch * glm::cross(rider_r_rel, rider_v_rel);
        const double rise_frac_now =
            clamp01(s.rider_up_m / std::max(p.stand_rise_m, kEps));
        const double cda = p.air_cda + p.stand_cda_add_m2 * rise_frac_now;

        // ★ THE MECHANIC (§9d, §8 item 1): composite-CG offset in body frame.
        // +lean_lat command = LEFT = -X; +lean_fwd = forward = -Z. Gravity and
        // air drag stay at the origin (s.position) -- ONLY the patch mounts
        // move, which is what makes "lean forward to bite" and "lean left to
        // free the outer ski" emergent from the existing per-patch load path
        // rather than a new steering term.
        // ★ INERTIA IS HELD CONSTANT under lean -- a named approximation, and
        // the honest number is measured, not guessed: up to 16 % error on
        // ROLL at full standing lean (§8 P1-6; an earlier claim of 1.4 % was
        // wrong and is withdrawn).
        //
        // ★★★ K-WS1 / K-A1 -- THE ANTI-CHEAT LEG, RE-STATED. It used to read
        // "AIRBORNE ANGULAR dynamics stay bit-exact under lean regardless",
        // and that sentence is now only HALF true, deliberately.
        //
        // What it was defending was real and still stands: NO TORQUE TERM
        // ANYWHERE READS cg_off. The 16 % constant-inertia error enters only
        // through the GROUND contact geometry above, and a cheat that made
        // the machine rotate in the air by moving a contact-patch mount
        // remains impossible.
        //
        // What changed is that airborne rotation under lean is no longer
        // ZERO, because zero was never the honest answer -- a rider who
        // throws 87.5 kg around in flight really does move the chassis, by
        // CONSERVATION OF ANGULAR MOMENTUM, and the kernel now says so
        // (`k_air_shift`, at the integrator below). That term does not touch
        // cg_off, does not touch a patch, and is a momentum exchange whose
        // deltas telescope to zero -- it cannot manufacture a sustained rate.
        // And it is a DIAL: at k_air_shift = 0, which is the shipped default,
        // airborne angular dynamics are bit-exact under lean exactly as
        // before. The gate is now the PAIR
        // sled_airborne_lean_is_inert_at_k_zero /
        // sled_airborne_lean_is_RESPONSIVE_at_k_nonzero.
        //
        // Linear drag under stand is NOT exact (cda changes with rise_frac),
        // and that is correct and stated, not a bug.
        // ★ THE COMPOSITE CG, AND IT IS COMPOSITE OF ONE ONCE HE IS GONE:
        // `rider_frac` is 0 while detached, so the mounts sit at the machine's
        // own CG and his last lean stops steering a sled he is no longer on.
        // ⚠ AND IT IS A STEP, NOT A RAMP. An earlier version of this comment
        // said "continuous by construction ... nothing jumps at the instant the
        // latch flips", reasoning from the displacement still slewing home --
        // but it is the FACTOR that jumps, not the displacement: `rider_frac`
        // goes from 0.264 to 0 in one substep, and the release fires precisely
        // when he is most extended, so `cg_off.x` can move ~0.09 m and take
        // every contact mount with it. Red-team, 2026-09-01.
        //
        // It is left as a step deliberately -- he IS gone, in one substep, and
        // ramping the mass of a man who has left would be a fiction. Whether it
        // is FELT is unmeasured and belongs in Chad's drive, not in a comment.
        const glm::dvec3 cg_off =
            rider_frac *
            glm::dvec3(-s.rider_lat_m, s.rider_up_m, -s.rider_fwd_m);
        PatchGeom geom[kPatches];
        patch_geometry(p, cg_off, geom);

        glm::dvec3 force{0.0};
        glm::dvec3 torque_body{0.0};
        double lift_sum = 0.0;
        double sink_area = 0.0, sink_area_den = 0.0, trench_rho = 0.0;
        // RC: per-substep contact bookkeeping for the roll assist (A) and the
        // readout gate (D) — the patch ground points fit the contact plane,
        // the summed normal load is the assist's contact gate.
        glm::dvec3 gpt[kPatches];
        double normal_sum = 0.0;
        // ★ K-WS1 / K-A3: is ANY running surface or hull point penetrating
        // this substep? The airborne exchange term's ONLY gate.
        bool ground_contact = false;
        // GI3/R3: per-term capture locals for the debug sink — written by the
        // C2/C3/assist blocks below, read ONLY by the sink fill at the end of
        // the substep (the kernel itself never branches on them).
        double dbg_tq_c2 = 0.0, dbg_tq_c3 = 0.0;
        double dbg_release = 0.0, dbg_w_contact = 0.0, dbg_phi = 0.0;
        int dbg_surf[kPatches] = {0, 0, 0};
        // ★ GI4/§8.4 roll-axis attribution (sled.h RollTerm). `dbg_term` names
        // the block add_at is currently inside; the buckets are filled ONLY
        // when a sink is attached, so the shipped path adds one predictable
        // branch and nothing else. Nothing below reads these back.
        // G1 added kRollGyro; zero-init so the roster width lives in ONE
        // place (the enum) and a future term cannot desync a literal here.
        double dbg_roll[kRollTermCount] = {};
        double dbg_c3_roll = 0.0;
        double dbg_plate_share[kPatches] = {0.0, 0.0, 0.0};
        double dbg_plate_n[kPatches] = {0.0, 0.0, 0.0};
        double dbg_lift_n[kPatches] = {0.0, 0.0, 0.0};
        int dbg_term = kRollNormal;
        // GI S2.6: per-patch normal load, held alongside gpt[] for the
        // load-weighted plane fit below (n_surf).
        double patchN[kPatches] = {0.0, 0.0, 0.0};
        // ★ GI4: the planing lateral plate is DEFERRED out of the patch loop.
        // Its load normalisation is against the MEAN of the STEERED patches,
        // which is not known until both skis have been solved, so the loop
        // banks each patch's plate (magnitude, direction, application point)
        // and a second pass applies them once the mean exists. Stateless:
        // these die with the substep.
        double platN[kPatches] = {0.0, 0.0, 0.0};
        glm::dvec3 platF[kPatches];
        glm::dvec3 platR[kPatches];
        bool platOn[kPatches] = {false, false, false};
        bool platSteer[kPatches] = {false, false, false};
        // RC B1: the slewed rider state as a signed fraction of the reach box
        // — the ONLY rider quantity the comfort terms read (never written).
        const double lean_frac =
            std::clamp(s.rider_lat_m / std::max(lat_reach_m, kEps), -1.0, 1.0);
        // ★ The turn sign is MACHINE-LEVEL (yaw rate about the local up),
        // not per-patch: the track's bite can momentarily oppose the skis'
        // (a kicked-out rear), and a per-patch sign let a wrong-way lean
        // touch the trajectory through that patch — breaking the "exactly
        // nothing" asymmetry contract (sled_wrong_way_lean_is_never_a_
        // penalty measured the leak at the 4th decimal). Ramped over
        // 0.02..0.07 rad/s so straight running with a held lean stays
        // continuous. +yaw = nose LEFT = lean_frac +1 (LEFT) is "into".
        double align_m = 0.0;
        if (p.comfort.lean_bite_gain > 0.0) {
            const double wy = glm::dot(R * s.angular_vel, up_cg);
            const double mag =
                std::clamp((std::abs(wy) - 0.02) / 0.05, 0.0, 1.0);
            align_m = clamp01(lean_frac * (wy >= 0.0 ? 1.0 : -1.0)) * mag *
                      mag * (3.0 - 2.0 * mag);
        }

        auto add_at = [&](const glm::dvec3& f_world,
                          const glm::dvec3& r_body) {
            force += f_world;
            // Torque in BODY axes: this is where every §2.4c.1 behaviour comes
            // from. A bank under one ski and not the other IS the roll moment;
            // thrust at the track and not the CG IS the weight transfer.
            const glm::dvec3 tq =
                glm::cross(r_body, glm::transpose(R) * f_world);
            torque_body += tq;
            // ★ GI4/§8.4: the SAME z-component, bucketed by block. Additive
            // readout only -- torque_body above is untouched by this.
            if (dbg) dbg_roll[dbg_term] += tq.z;
        };

        for (int i = 0; i < kPatches; ++i) {
            const PatchGeom& g = geom[i];
            const glm::dvec3 r_world = R * g.mount;
            const glm::dvec3 mount_w = s.position + r_world;
            const glm::dvec3 up_i = glm::normalize(mount_w);

            // ★ THE ONE GROUND QUERY (INV-1). Per patch, at the patch's OWN
            // sub-point -- which is the entire content of the per-ski ruling.
            const world::SnowpackField::GroundSample gs = snow.sample_at(up_i);
            // *** SK-1a (Chad 2026-08-24): BLEND THE PATCH DIALS ACROSS A
            // CLASS TRANSITION. classify() is a STEP and the dials behind it
            // are not continuous with the C1 bank_profile that drives it, so
            // one ski crossing a corridor edge stepped rho_eff 0 -> 260 (the
            // planing lateral plate switching on under ONE ski, a pure
            // yaw+roll couple with no counterpart) plus sinkable false ->
            // true and c_snow_pa /21, in a single tick. Measured on
            // `seads_sled_probe bankgraze`: a 25 cm lateral move took the
            // machine from yaw 0.0 deg / roll 2.4 deg to 1.34 ROTATIONS at
            // 24 m/s, straight, with NO steer input.
            // `surf_mix == 0` (the shipped default and what every taped
            // ground sample reconstructs) takes the ORIGINAL reference and is
            // bit-identical -- a branch, never a 0-weight lerp.
            SurfaceDials d_blend;
            const SurfaceDials& d =
                gs.surf_mix > 0.0
                    ? (d_blend = blend_dials(
                           p.dials[static_cast<int>(gs.surf)],
                           p.dials[static_cast<int>(gs.surf_b)], gs.surf_mix))
                    : p.dials[static_cast<int>(gs.surf)];
            gpt[i] = up_i * gs.drive_r;  // RC: contact-plane fit point
            dbg_surf[i] = static_cast<int>(gs.surf);  // GI3/R3 sink capture
            if (i == static_cast<int>(Patch::Track)) {
                rep_depth = gs.depth_m;
                rep_surf = gs.surf;
            }

            const double area = std::max(g.len * g.width, kEps);

            // --- suspension: geometric travel, real state -------------------
            // The patch rides `sink` BELOW the snow surface; the chassis hangs
            // above it along the body axis, so an off-angle bank compresses the
            // bank-side ski first without anything computing "tilt".
            const double axis_dot = std::max(glm::dot(body_up, up_i), 0.25);
            const glm::dvec3 v_patch =
                s.velocity + glm::cross(world_omega, r_world);
            const double xdot = -glm::dot(v_patch, up_i) / axis_dot;

            // ★★ SUSPENSION AND SNOW ARE IN SERIES, AND THAT STRUCTURE IS LOAD-
            // BEARING. The first version made them two INDEPENDENT springs --
            // the suspension produced the force, and sinkage relaxed toward a
            // Bekker equilibrium computed FROM that force. That is a positive
            // feedback loop (more load -> more sinkage -> lower support -> more
            // compression -> more load) with a measured loop gain above 1, and
            // the machine porpoised endlessly at a metre of amplitude. The
            // trace found it; the terminal speeds looked merely "noisy".
            //
            // The honest structure is one deflection `d` shared between the
            // spring and the pack: the pack pushes back with its own Bekker
            // reaction A*K*z^n, the suspension carries the rest, and the two
            // forces must agree. Two Newton steps from the previous penetration
            // are enough, and the result is unconditionally stable because the
            // snow's reaction now RISES with penetration instead of being a
            // free offset.
            const double surface_r = gs.drive_r - s.creep_m[i];
            const double hang = (glm::length(mount_w) - surface_r) / axis_dot;
            const double d_total = p.susp_rest_m - hang;

            double x = 0.0, z = 0.0, normal = 0.0;
            if (d_total > 0.0) {
                const double area0 = std::max(g.len * g.width, kEps);
                const double b0 = std::min(g.len, g.width);
                // ★ d.pack_k_scale: the groomer's compaction, per surface
                // class (OPEN-VP2) -- Bush is 1.0 so natural pack is
                // bit-identical; multiply-at-use, same seam as snow_hardness.
                const double K = d.sinkable
                                     ? d.pack_k_scale *
                                           pack_modulus(p, b0, gs.depth_m)
                                     : 0.0;
                // Penetration available before the pack bottoms out on the
                // terrain beneath it -- ★ the clamp IS the bottom of the
                // snowpack, not a safety rail.
                const double z_cap =
                    std::max(0.0, gs.depth_m - s.creep_m[i]);
                z = std::clamp(s.sink_m[i] - s.creep_m[i], 0.0,
                               std::min(d_total, z_cap));
                if (K > 0.0) {
                    for (int it = 0; it < 2; ++it) {
                        const double fz = area0 * K * std::pow(z, p.bekker_n);
                        const double fx = p.susp_k * (d_total - z);
                        const double dfz =
                            area0 * K * p.bekker_n *
                            std::pow(std::max(z, 1e-6), p.bekker_n - 1.0);
                        z = std::clamp(z - (fz - fx) / (dfz + p.susp_k), 0.0,
                                       std::min(d_total, z_cap));
                    }
                }
                x = std::clamp(d_total - z, 0.0, p.susp_travel_m * 2.0);
                normal = p.susp_k * x + p.susp_c * xdot;
                if (x > p.susp_travel_m)  // bump stop
                    normal += p.susp_k * p.susp_stop_k * (x - p.susp_travel_m);
                normal = std::max(normal, 0.0);
            }
            s.susp_x[i] = x;
            s.susp_v[i] = xdot;
            normal_sum += normal;  // RC: the assist's contact gate (A/D)
            patchN[i] = normal;    // GI S2.6: the plane fit's load weight

            // --- sinkage: THE RATE PROCESS (§3.5a) --------------------------
            const double v_tan_vec_len =
                glm::length(v_patch - glm::dot(v_patch, up_i) * up_i);
            // ★ TWO TIMESCALES, and getting this shape right is the whole rung.
            // A single relaxation toward one equilibrium was tried first and is
            // WRONG: it made the machine's sinkage at speed a few millimetres
            // at every depth, so 0.77 m and 1.75 m drove IDENTICALLY -- the
            // depth field, the signed input of this whole rung, had no effect
            // on the ride at all. Snow compaction under a moving load is fast
            // and plastic; what takes TIME is the creep on top of it.
            //   z_inst  -- the Bekker bearing depth, reached essentially at
            //              once, so it is felt at any speed
            //   creep   -- the dwell excess, growing with time under load and
            //              shed by travelling onto uncompacted snow
            // §3.5a's "dwell buries you, throttle saves you" is the SECOND term
            // only, which is exactly right: standing still keeps sinking; a
            // moving machine sits at its instantaneous bearing depth.
            if (d.sinkable) {
                // The creep target is read off the INSTANTANEOUS penetration
                // the series solve just produced. It enters the next substep as
                // a lowering of the bearing SURFACE, which is negative feedback
                // (more creep -> less deflection -> less load -> less creep) --
                // the opposite of the loop the first version had.
                const double de =
                    (p.creep_frac * z - s.creep_m[i]) /
                        std::max(p.tau_sink_s, kEps) -
                    s.creep_m[i] * v_tan_vec_len /
                        std::max(p.refresh_len_m, kEps);
                s.creep_m[i] = std::clamp(s.creep_m[i] + de * h, 0.0,
                                          gs.depth_m);
                s.sink_m[i] = std::min(z + s.creep_m[i], gs.depth_m);
            } else {
                s.creep_m[i] = 0.0;
                s.sink_m[i] = 0.0;
            }

            if (normal <= 0.0 && x <= 0.0) continue;  // patch in the air
            // ★★★ K-WS1 / K-A3. GROUNDED IS DEFINED BY CONTACT, NOT BY LOAD,
            // and this line is the whole definition -- it is the kernel's OWN
            // "patch in the air" test, one line above, read the other way
            // round, so the two can never disagree.
            //
            // WHY NOT LOAD. The plan wanted air_frac = 1 - load/(m g); it is
            // REFUTED by measurement. Planing unloads the patches while the
            // machine is unambiguously on the ground: 62 N of 4106 at the
            // -28 deg roll attractor, ~913 N of 4106 at WOT in a carve. A
            // load-based air_frac would read 0.78-0.99 THERE and spend
            // airborne authority all over a carve and all over the attractor
            // -- exactly the two places this kernel is most fragile. Contact
            // is binary and it is honest, and it makes "grounded is
            // bit-identical with K2 on" a fact instead of a promise
            // (sled_air_shift_is_inert_while_any_patch_touches).
            ground_contact = true;

            // --- planing lift (§3.1) ----------------------------------------
            // Flat-plate lift at the patch's OWN attack angle: the built-in
            // rake plus the machine's pitch against its local surface, so a
            // nose-up attitude planes and a nose-down one digs. Emergent, not
            // a mode.
            const double pitch_sin = std::clamp(glm::dot(body_fwd, up_i),
                                                -1.0, 1.0);
            const double alpha = std::clamp(
                (g.is_track ? 0.45 * p.ski_rake_rad : p.ski_rake_rad) +
                    std::asin(pitch_sin),
                -p.alpha_max_rad, p.alpha_max_rad);
            // ★ AND THE LIFT IS SCALED BY THE PATCH'S DRAFT -- how deep it is
            // riding in the pack. This is what makes planing a stable TRIM
            // rather than a launch: sinking raises lift, lift raises the
            // machine, riding higher lowers the draft and the lift with it, and
            // the machine settles skimming. A ski sitting ON TOP of the snow
            // has nothing to deflect and makes no planing lift, which is also
            // the honest reason "still plowing some" is structural (§2.2a): the
            // draft that makes the lift is the same draft that makes the plow
            // drag, so one can never go to zero while the other is holding the
            // machine up.
            const double draft =
                clamp01(s.sink_m[i] / std::max(p.plane_draft_m, kEps));
            const double lift = 0.5 * d.rho_eff * v_tan_vec_len *
                                v_tan_vec_len * area * std::sin(alpha) *
                                p.plane_gain * draft;
            dbg_lift_n[i] = d.rho_eff > 0.0 ? lift : 0.0;  // GI4/§8.4
            dbg_term = kRollLift;
            if (d.rho_eff > 0.0) {
                // ★★ GI4 §9.7 ITEM 1: THE LIFT'S RAIL/PITCH SPLIT
                // (plane_lift_split_frac, sled.h — the full why lives there).
                // Same separable product form as the normal reaction below:
                // lateral shares (lfr, lfl) summing to 1 and longitudinal
                // shares (lfa, lff) summing to 1, so the four quarter lifts
                // are lift * f_lat * f_lon and the TOTAL lift is conserved
                // exactly — ride height, planing and sinkage are untouched,
                // only the moment the same force makes.
                if (p.plane_lift_split_frac > 0.0 &&
                    (g.rail_half > kEps || g.pitch_half > kEps)) {
                    const glm::dvec3 right_lb(1.0, 0.0, 0.0);
                    const glm::dvec3 aft_lb(0.0, 0.0, 1.0);
                    const double inv_draft =
                        1.0 / std::max(p.plane_draft_m, kEps);
                    double lfr = 0.5, lfl = 0.5, lfa = 0.5, lff = 0.5;
                    if (g.rail_half > kEps) {
                        const double dx = -glm::dot(R * right_lb, up_i) *
                                          g.rail_half / axis_dot;
                        // The DRAFT each rail is riding at, unilaterally
                        // clamped: a rail lifted clear of the pack makes no
                        // lift, and all of it goes to the other rail.
                        const double dr =
                            clamp01((s.sink_m[i] + dx) * inv_draft);
                        const double dl =
                            clamp01((s.sink_m[i] - dx) * inv_draft);
                        if (dr + dl > kEps) lfr = dr / (dr + dl);
                        lfr = 0.5 + p.plane_lift_split_frac * (lfr - 0.5);
                        lfl = 1.0 - lfr;
                    }
                    if (g.pitch_half > kEps) {
                        const double dz = -glm::dot(R * aft_lb, up_i) *
                                          g.pitch_half / axis_dot;
                        const double da =
                            clamp01((s.sink_m[i] + dz) * inv_draft);
                        const double df =
                            clamp01((s.sink_m[i] - dz) * inv_draft);
                        if (da + df > kEps) lfa = da / (da + df);
                        lfa = 0.5 + p.plane_lift_split_frac * (lfa - 0.5);
                        lff = 1.0 - lfa;
                    }
                    const glm::dvec3 rgt_l = right_lb * g.rail_half;
                    const glm::dvec3 aft_l = aft_lb * g.pitch_half;
                    add_at(lift * lfr * lfa * up_i,
                           g.mount + rgt_l + aft_l);
                    add_at(lift * lfr * lff * up_i,
                           g.mount + rgt_l - aft_l);
                    add_at(lift * lfl * lfa * up_i,
                           g.mount - rgt_l + aft_l);
                    add_at(lift * lfl * lff * up_i,
                           g.mount - rgt_l - aft_l);
                } else {
                    add_at(lift * up_i, g.mount);
                }
                lift_sum += lift;
                // ★ THE TRENCH THE MACHINE MUST CLIMB OUT OF -- and this is
                // what makes deeper snow raise the planing speed (§3.1's
                // Froude-like threshold). Computed over the machine's PLAN
                // AREA, not the running surfaces: the first version charged
                // only the patch areas, which moved the threshold by ~5 % and
                // left the measured planing speed in 1.14 m LOWER than in
                // 0.77 m -- the opposite of the law, hidden under the noise of
                // everything else that moves with depth. The snow being lifted
                // is the snow over the whole footprint sitting in the hole.
                sink_area += s.sink_m[i] * area;
                sink_area_den += area;
                trench_rho = std::max(trench_rho, d.rho_eff);
            }

            // --- normal reaction, at the patch (or TWO rails, §8 P0-5) ------
            // *** ON A PATCH WITH REAL WIDTH, AT BOTH RAILS. Pure GEOMETRY --
            // no torque term, no roll assist, no angle tested. The split is
            // by the rails' OWN penetration, from the SAME `hang` geometry
            // that already makes the bank-side ski compress first (INV-1:
            // still one Bekker solve per patch, this is a linearisation off
            // it). `max(.,0)` is unilateral contact (a rail cannot PULL) and
            // the renormalization keeps the TOTAL exactly what the series
            // solve produced, so ride height, planing and sinkage are
            // untouched. When one rail lifts, all of it goes to the other, at
            // its offset -- that is the whole rollover story (PACKET_B §15).
            // ★ `susp_k`-based split errs toward MORE stability (comment, not
            // a rewrite): a real machine's roll arm also moves as one rail
            // compresses more than the other, and this split does not model
            // that -- CARRIED OPEN per the `track_rail_half_m` field comment.
            // ★★ GI S2b: THE SAME SPLIT ALONG THE PATCH'S LENGTH. Identical
            // mechanism, rotated 90 deg -- and it had to be, because the
            // defect was identical: all of a 1.14 m patch's normal reaction
            // applied at one mid-patch point carries NO pitch-restoring
            // moment, so a full-throttle launch that unloads the skis has
            // nothing left to bring the nose back down (MEASURED runaway to
            // 89.9 deg, docs/gi_measurements.md).
            //
            // THE ALGEBRA (why the composition is a product of fractions).
            // The rail split's own mechanism, read out of the code above: the
            // shares are NOT fixed at N/2 -- `dx` is the extra hang of the
            // outboard point off the SAME linearised `hang` geometry
            // (`d_total` at the offset point is `d_total + dx`), so the share
            // is `susp_k * dx` of differential load, unilaterally clamped and
            // renormalised so the TOTAL stays exactly what the series solve
            // produced. Under roll that load shift IS the restoring moment;
            // an unconditional N/2 at +-rail would contribute exactly zero
            // (the two offset torques cancel). Pitch is the same statement in
            // z: `dz` is the extra hang of the AFT point, so nose-up puts
            // more load aft and the couple pushes the nose back down.
            //
            // Composing the two is a QUARTERING: with lateral fractions
            // (fr, fl), fr + fl == 1, and longitudinal fractions (fa, ff),
            // fa + ff == 1, the four quarter loads are N * f_lat * f_lon at
            // (+-rail, +-pitch). Their lateral marginals are (fr, fl) * N and
            // their longitudinal marginals (fa, ff) * N exactly, so the roll
            // couple is the rail split's and the pitch couple is the pitch
            // split's, unchanged -- the product form is the unique separable
            // composition with that property (it is the independent-marginals
            // outer product). Bit-identity is guaranteed STRUCTURALLY rather
            // than by arithmetic luck: pitch_half == 0 takes the untouched
            // rail branch below, rail_half == 0 takes the two-point z branch.
            const glm::dvec3 right_b(1.0, 0.0, 0.0);
            const glm::dvec3 aft_b(0.0, 0.0, 1.0);  // +Z is AFT (SPEC §7)
            dbg_term = kRollNormal;  // GI4/§8.4
            if (g.pitch_half > kEps) {
                const double dz =
                    -glm::dot(R * aft_b, up_i) * g.pitch_half / axis_dot;
                double na = std::max(0.5 * normal + p.susp_k * dz, 0.0);
                double nf = std::max(0.5 * normal - p.susp_k * dz, 0.0);
                double fa = 0.5, ff = 0.5;
                if (na + nf > kEps) {
                    fa = na / (na + nf);
                    ff = nf / (na + nf);
                }
                const glm::dvec3 aft_off = aft_b * g.pitch_half;
                if (g.rail_half > kEps) {
                    const double dx =
                        -glm::dot(R * right_b, up_i) * g.rail_half / axis_dot;
                    double nr = std::max(0.5 * normal + p.susp_k * dx, 0.0);
                    double nl = std::max(0.5 * normal - p.susp_k * dx, 0.0);
                    double fr = 0.5, fl = 0.5;
                    if (nr + nl > kEps) {
                        fr = nr / (nr + nl);
                        fl = nl / (nr + nl);
                    }
                    const glm::dvec3 rgt_off = right_b * g.rail_half;
                    add_at(normal * fr * fa * up_i,
                           g.mount + rgt_off + aft_off);
                    add_at(normal * fr * ff * up_i,
                           g.mount + rgt_off - aft_off);
                    add_at(normal * fl * fa * up_i,
                           g.mount - rgt_off + aft_off);
                    add_at(normal * fl * ff * up_i,
                           g.mount - rgt_off - aft_off);
                } else {
                    add_at(normal * fa * up_i, g.mount + aft_off);
                    add_at(normal * ff * up_i, g.mount - aft_off);
                }
            } else if (g.rail_half > kEps) {
                const glm::dvec3 right_w = R * right_b;
                const double dx =
                    -glm::dot(right_w, up_i) * g.rail_half / axis_dot;
                double nr = std::max(0.5 * normal + p.susp_k * dx, 0.0);
                double nl = std::max(0.5 * normal - p.susp_k * dx, 0.0);
                const double tot = nr + nl;
                if (tot > kEps) {
                    const double kk = normal / tot;
                    nr *= kk;
                    nl *= kk;
                }
                // ★ GI S2.1: normal-stays. Vertical force at the mount vs the
                // contact differs by a roll torque ~= L*N*sin(roll) -- ~219
                // N*m at 20 deg roll, 23% of restoring, erring STABLE;
                // deliberately kept at the mount, same class as the
                // CARRIED-OPEN note above.
                add_at(nr * up_i, g.mount + right_b * g.rail_half);
                add_at(nl * up_i, g.mount - right_b * g.rail_half);
            } else {
                // ★ GI S2.1: normal-stays (NAMED APPROXIMATION). Vertical
                // force at the mount vs the contact differs by a roll torque
                // ~= L*N*sin(roll) -- ~219 N*m at 20 deg roll, 23% of
                // restoring, erring STABLE; deliberately kept at the mount,
                // same class as the CARRIED-OPEN note above.
                add_at(normal * up_i, g.mount);
            }

            // ★ GI S2.1 (P1-1): THE HONEST TANGENTIAL ARM. Every tangential
            // force below (friction, plow drag, lateral bite, track thrust,
            // both brake terms -- tangential-moves) is applied at
            // `tan_mount`, which blends from the suspension mount toward the
            // patch's true contact point by `bite_at_contact_frac`. The
            // max(0, ...) clamp is deliberate: past the bump stop
            // (x > susp_travel_m) the raw term goes negative, which would
            // raise the point ABOVE the mount mid-impact (P2-3) -- clamped to
            // 0 instead, i.e. never past the mount. At frac == 0.0 this is
            // g.mount exactly, bit-identical to the pre-GI kernel.
            const glm::dvec3 contact_off(
                0.0,
                -std::max(0.0, p.susp_rest_m - x + s.sink_m[i]) *
                    p.bite_at_contact_frac,
                0.0);
            const glm::dvec3 tan_mount = g.mount + contact_off;

            // --- tangential: friction, plow, bite ---------------------------
            // The patch's own surface frame. Degenerate only if the machine is
            // pointing straight up or down, where a contact patch has no
            // meaningful forward -- skip the tangential terms rather than
            // normalizing a zero vector.
            const glm::dvec3 fwd_raw =
                body_fwd - glm::dot(body_fwd, up_i) * up_i;
            if (glm::length(fwd_raw) < 1e-6) continue;
            const glm::dvec3 fwd_t = glm::normalize(fwd_raw);
            // +X right, -Z forward, +Y up (SPEC §7): right == fwd x up.
            const glm::dvec3 right_t = glm::normalize(glm::cross(fwd_t, up_i));
            const double v_fwd = glm::dot(v_patch, fwd_t);
            const double v_lat = glm::dot(v_patch, right_t);

            // Sliding friction, opposing motion.
            // ★ SC1 WARM DRAG (WINTER_LAW §3.7, spec §3): ALL of warm's
            // performance loss lives here, on sinkable rows only -- never on
            // hardness (spec §0: warm-softening the pack reopens the §2.2a
            // burial-saturation failure). Multiply-at-use, no new state.
            const double mu_kin_eff =
                d.sinkable
                    ? d.mu_kin *
                          (1.0 + p.warm_drag_gain *
                                     std::max(0.0, p.air_temp_c - p.cold_t_ref_c) /
                                     10.0)
                    : d.mu_kin;
            if (v_tan_vec_len > kEps) {
                const glm::dvec3 vhat =
                    (v_patch - glm::dot(v_patch, up_i) * up_i) / v_tan_vec_len;
                dbg_term = kRollFric;  // GI4/§8.4
                add_at(-mu_kin_eff * normal * vhat, tan_mount);  // GI S2.1: tangential-moves
            }

            // ★ DISPLACEMENT DRAG -- "still plowing some" (§2.2a). The
            // submerged frontal area shrinks as lift unloads the patch, but it
            // is NEVER zero while there is snow: there is no speed at which
            // this term switches off, and a binary planing flag is exactly the
            // thing this equation refuses to be.
            const double immersion = std::min(s.sink_m[i], gs.depth_m);
            if (immersion > 0.0 && v_fwd > 0.0) {
                const double a_sub = g.width * immersion;
                const double plow = 0.5 * d.rho_eff * v_fwd * v_fwd * a_sub *
                                    p.plow_cd;
                dbg_term = kRollPlow;  // GI4/§8.4
                add_at(-plow * fwd_t, tan_mount);  // GI S2.1: tangential-moves
            }

            // ★★ B3 THE HOIST (ladder_v2 §4.3, the sled first build). The six
            // lines that build the TRACK's longitudinal slip used to live 58
            // lines below, inside `if (g.is_track)`. They are here now, above
            // the lateral bite, because `track_lat_slip_shed` charges the
            // track's SIDEWAYS grip for the slip it is already spending
            // forwards -- "throttle should also be able to swing my tail around
            // on account of the roost" (Chad, 2026-09-18) -- and the bite is
            // computed first.
            //
            // IT IS A PURE MOVE, and that is a claim with a shape: the six lines
            // have no side effects (no `add_at`, no `s.` write, no `dbg_term`),
            // and every input they read is FIXED for this patch between the two
            // sites -- `p.*` (const params), `throttle` (the substep's, :293),
            // `v_fwd` (:1006, this patch's, computed above) and `kEps`. Nothing
            // between :1042 and the old site writes any of them. So the values
            // are the same values, in the same order, and the identity is
            // arithmetic rather than tolerance.
            //
            // ★★ FOLDED RED-TEAM P1-3 (mechanism, 2026-09-18): THE ROOST IS
            // HOISTED WITH THE SLIP, and the shed reads IT, not the bare slip.
            // His sentence is "swing my tail around ON ACCOUNT OF THE ROOST".
            // The kernel already owns the roost as ONE number -- `flux =
            // |trk_slip| * avail` -- and `avail` is identically 0 on every
            // NON-SINKABLE row (Road, LakeIce, RockOutcrop, MineWorks; pinned
            // by sled_road_sinkage_is_exactly_zero). The first build read
            // `|trk_slip|` alone, which is surface-BLIND: it stripped the
            // track's lateral grip at full strength on a plowed road, where by
            // construction there is no roost to swing anything with -- "just
            // going down the road", his words, and the surface his loudest OLD
            // complaint lives on. `bury`/`loose`/`avail` read only `s.sink_m[i]`
            // (written at :761-764, far above this force loop), `gs.depth_m`,
            // `d.sinkable` and const params, so hoisting them is the same PURE
            // move the slip was -- and `flux` becomes the ONE NUMBER's THIRD
            // consumer, which is what that comment is for, never a parallel
            // constant.
            double clutch_blend = 0.0, drive = 0.0, v_track = 0.0,
                   trk_slip = 0.0, trk_flux = 0.0;
            if (g.is_track) {
                // ★ CVT SMOOTHSTEP (§8 P1-2). A hard step at 480 Hz limit-
                // cycles on downhills; blend over clutch_engage_ms +/- 0.25
                // instead. 0 well below engage speed, 1 above -- and
                // `engine_rpm` is mapped off the SAME blend, never a second
                // one.
                const double engage_lo = p.clutch_engage_ms - 0.25;
                const double engage_hi = p.clutch_engage_ms + 0.25;
                const double engage_t = std::clamp(
                    (v_fwd - engage_lo) / std::max(engage_hi - engage_lo, kEps),
                    0.0, 1.0);
                clutch_blend = engage_t * engage_t * (3.0 - 2.0 * engage_t);

                // ★ THE BELT IS BACK-DRIVEN WHEN THE THUMB IS OFF (Phase V
                // P1-A, measured: without this the closed throttle commanded
                // v_track = 0, slip = -1, and the track developed FULL REVERSE
                // SHEAR -- a locked track, 1.29 g of face-brake the moment the
                // player let off at speed. A CVT-engaged two-stroke does the
                // opposite: the ground turns the track at ground speed, slip
                // ~ 0, and the only drivetrain drag is the engine-brake term
                // below. `drive` is smooth in THROTTLE (P1-B) so the 420 N
                // term cannot step across the spring-return thumb's 0.05 line.
                const double drive_t =
                    std::clamp((throttle - 0.02) / 0.08, 0.0, 1.0);
                drive = drive_t * drive_t * (3.0 - 2.0 * drive_t);
                const double v_cmd = throttle * p.track_speed_max_ms;
                const double v_back = clutch_blend * std::max(v_fwd, 0.0);
                v_track = drive * v_cmd + (1.0 - drive) * v_back;
                trk_slip = std::clamp(
                    (v_track - v_fwd) / std::max(v_track, 1.0), -1.0, 1.0);
                // A buried tunnel has no free cleat to throw snow with. This
                // one fraction is simultaneously why the roost dies and why the
                // escape fails -- §3.5a's terminal failure as arithmetic.
                const double bury =
                    clamp01(s.sink_m[i] / std::max(p.track_clearance_m, kEps));
                const double loose = d.sinkable ? gs.depth_m : 0.0;
                const double avail =
                    clamp01(loose / std::max(p.roost_ref_depth_m, kEps)) *
                    (1.0 - bury);
                // ★★ THE ONE NUMBER, NOW THREE CONSUMERS (§3.5a): S5's roost,
                // the escape thrust below, and B3's shed above.
                trk_flux = std::abs(trk_slip) * avail;
            }

            // ★ LATERAL BITE UNDER WEIGHT TRANSFER (§3.2). The force reads
            // THIS patch's normal load, so throttle (which unloads the skis via
            // the moment arm above) genuinely costs you steering. Nothing named
            // "weight transfer" exists in the code.
            // ★★ RED-TEAM P0-1 (2026-08-11): this was `- delta`, which makes
            // steer=+1 (documented LEFT, SledInputs::steer) yaw the nose
            // RIGHT -- every prior rollover/sweep leg was sign-blind because
            // each only ever checked magnitude (rolled/not-rolled), never
            // direction. `+ delta` is the fix, pinned by
            // sled_steer_plus_one_yaws_nose_left (measured to FAIL at
            // `- delta`, PASS at `+ delta`).
            const double slip_ang =
                std::atan2(v_lat, std::max(std::abs(v_fwd), 0.5)) +
                (g.steered ? delta : 0.0);
            double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;
            // ★★ N2 -- LAKE-ICE LOW-SPEED SKI BITE (sim/sled.h ice_bite_mu;
            // Chad, run 7, 2026-09-18: "the ski runners are not digging in to
            // the ice ... less grip at lower speeds than I would like").
            // ADDITIVE ski mu, LakeIce only (SK-1a blend-weighted from the
            // patch's OWN ground sample, so a shoreline stays continuous),
            // fading from full at rest to EXACTLY +0.0 by kIceBiteVrefMs on
            // THIS patch's tangential speed -- above that the sum is `mu_l +
            // 0.0`, bit-identical, which is how the high-speed limit he
            // called real is kept: by arithmetic, not a clamp.
            //
            // ★ `g.steered`, NEVER `!g.is_track`: his word is "runners" and the
            // track's lateral hold (track_lat_mu 0.70, surface-blind) is the
            // OTHER side of the ratio this dial is moving -- more track bite
            // would cancel the gain. KILLING MUTATION (measured, docs/
            // SLED_KERNEL_N2_ICEBITE.md): `!g.steered` here moves the 3 m/s
            // row's yaw the OTHER way and the leg
            // `sled_ice_bite_raises_low_speed_yaw` reds.
            //
            // A BRANCH, so at the identity 0.0 this term does not exist and
            // `mu_l` is the shipped expression byte for byte (pinned 17 digits
            // by `sled_ice_bite_zero_is_the_identity`).
            if (p.comfort.ice_bite_mu > 0.0 && g.steered) {
                const double w_ice =
                    (gs.surf == world::Surface::LakeIce ? 1.0 - gs.surf_mix
                                                        : 0.0) +
                    (gs.surf_mix > 0.0 && gs.surf_b == world::Surface::LakeIce
                         ? gs.surf_mix
                         : 0.0);
                if (w_ice > 0.0) {
                    const double u = clamp01(v_tan_vec_len / kIceBiteVrefMs);
                    // 1 at rest, exactly 0 at u == 1: 1 - 1*(3 - 2) == 0.0.
                    const double w_lo = 1.0 - u * u * (3.0 - 2.0 * u);
                    mu_l += p.comfort.ice_bite_mu * w_ice * w_lo;
                }
            }
            // ★★ B3 THE SHED (ladder_v2 §4.3). "throttle should also be able
            // to swing my tail around on account of the roost, esp with weight
            // shifting of the sudburian" (Chad, 2026-09-18). The track spends
            // its friction budget FORWARDS as `trk_slip`; a friction ellipse
            // says what it spends forwards it does not have sideways. This
            // kernel had no such law -- `mu_brake` caps longitudinal braking
            // and never charges lateral -- so the tail could not be swung with
            // the thumb.
            //
            // ★ `align_m`, NEVER the raw signed lean. align_m is already
            // clamped >= 0 (the house rule: a WRONG-WAY LEAN IS NEVER A
            // PENALTY, pinned by sled_wrong_way_lean_is_never_a_penalty at the
            // 4th decimal) and already requires a real yaw rate. The `1.0 +`
            // is what makes the shed real in a straight line too -- which is
            // honest, and is the part that can make a bank strike worse. The
            // lean-GATED fallback, if his drive says the banks got worse, is
            // `align_m` alone in place of `(1.0 + align_m)`.
            //
            // ★★ FOLDED RED-TEAM P1-3 (mechanism): `trk_flux`, NOT
            // `|trk_slip|`. `flux = |trk_slip| * avail` is the kernel's one
            // roost number and `avail` is identically 0 on the non-sinkable
            // rows, so the shed is now DARK on a plowed road and on lake ice --
            // where his loudest OLD complaint lives and where there is no roost
            // to swing anything with. His sentence names the roost; the dial
            // now reads it.
            //
            // ★★ FOLDED RED-TEAM P1-2 (mechanism): THE CLOSED-THUMB DECOUPLE,
            // the SAME factor the thrust uses at the track block below (`T *=
            // drive + (1 - drive) * clutch_blend`). MEASURED: with the thumb
            // SHUT, below clutch_engage_ms - 0.25 = 3.0 m/s, `v_track` is 0 and
            // `trk_slip` is exactly -1 -- the LARGEST value this dial can ever
            // see, reached at ZERO throttle. Without this factor the shed fired
            // at full strength on a coasting machine: closing the throttle
            // would NOT hook the tail back up below ~3.3 m/s, which is the
            // opposite of the sentence the dial is built from, and it is
            // exactly the regime the bank strikes live in. The shed now exists
            // only where the drivetrain is connected.
            //
            // ★ `g.is_track`: lean buys ski plate, never track plate.
            // ⚠ AND THE HONEST NOTE (FOLDED RED-TEAM P1-3 law+feel / P2-1
            // mechanism): the guard is REDUNDANT BY SCOPE today and a leg
            // cannot red on deleting it -- `trk_slip` and `trk_flux` are
            // per-patch locals initialised 0.0 and written only inside
            // `if (g.is_track)` above, so on a ski patch the factor is
            // `max(0, 1 - shed*0*...) == 1.0` and `mu_l *= 1.0` is
            // bit-identical. MEASURED by the red-team: delete `g.is_track &&`,
            // rebuild, the leg still passes. KEEP THE GUARD ANYWAY -- it is the
            // only protection the ski plate has the day someone hoists those
            // locals to substep scope as an "optimisation" -- but do not claim
            // a leg watches it.
            //
            // A BRANCH, so at the shipped 0.0 this term does not exist and
            // `mu_l` below is the shipped expression byte for byte.
            if (g.is_track && p.track_lat_slip_shed > 0.0)
                mu_l *= std::max(
                    0.0, 1.0 - p.track_lat_slip_shed * trk_flux *
                                   (drive + (1.0 - drive) * clutch_blend) *
                                   (1.0 + align_m));
            double bite = -normal * mu_l *
                          std::tanh(slip_ang /
                                    std::max(p.slip_ref_rad, kEps));
            // ★ RC item B1 (§0b): leaning INTO the turn multiplies the
            // effective bite — the reward channel. `align_m` (computed once
            // per substep, machine-level) is the SIGN-MATCHED lean fraction
            // clamped at 0, so the wrong-way lean gives exactly 1.0 on EVERY
            // patch (never a penalty: "bad driving allowed, good riding
            // keeps the benefit" as arithmetic) and lean 0 leaves the signed
            // tables bit-identical.
            bite *= 1.0 + p.comfort.lean_bite_gain * align_m;
            dbg_term = kRollBite;  // GI4/§8.4
            add_at(bite * right_t, tan_mount);  // GI S2.1: tangential-moves

            // ★ GI4 THE PLANING LATERAL FORCE (sled.h plane_lat_gain). The
            // lift above is what carries the machine once it is planing, and
            // it unloads `normal` -- and therefore the whole bite term -- to
            // 4.5 % of the machine's weight on the STEERED patches at WOT.
            // This is the lift equation's lateral twin: the same flat plate,
            // the same draft scaling, the same rho_eff gate (identically 0
            // where there is no snow to deflect), and the same mount, so its
            // yaw moment is geometry. It OPPOSES the slip, so it is a
            // restoring force, never a propulsive one. Deliberately NOT
            // tanh-saturated the way the mu bite is: a plate's deflection
            // grows with its yaw right up to stall, and alpha_max_rad is
            // already the stall clamp used by the lift.
            if (p.plane_lat_gain > 0.0 && d.rho_eff > 0.0) {
                const double draft_l =
                    clamp01(s.sink_m[i] / std::max(p.plane_draft_m, kEps));
                const double slip_l =
                    std::clamp(slip_ang, -p.alpha_max_rad, p.alpha_max_rad);
                double lat = 0.5 * d.rho_eff * v_tan_vec_len * v_tan_vec_len *
                             area * std::sin(slip_l) * p.plane_lat_gain *
                             draft_l;
                // ★ THE LEAN REWARD, AIMED. B1's `lean_bite_gain` multiplies
                // the mu bite on EVERY patch -- including the track, whose
                // lateral bite is precisely what resists yaw. Measured:
                // raising it 0.18 -> 0.45 made the lean-in carve WORSE (30 m
                // -> 295-1037 m of radius on Bush), because the track gains
                // more grip than the skis do (it carries 80 % of the load).
                // So the reward this term takes is STEERED-PATCH ONLY: lean
                // buys ski plate, never track plate, which is the direction
                // that actually tightens an arc. Wrong-way lean gives
                // exactly 1.0 (align_m >= 0), the house rule.
                if (g.steered)
                    lat *= 1.0 + p.plane_lat_lean_gain * align_m;
                // BANKED, not applied: the load normalisation below needs the
                // mean over the steered patches, which does not exist until
                // this loop has finished.
                platN[i] = lat;
                platF[i] = -right_t;
                platR[i] = tan_mount;
                platOn[i] = true;
                platSteer[i] = g.steered;
            }

            // --- track thrust (§3.2, §3.5a) ---------------------------------
            if (g.is_track) {
                // ★★ THE SLIP IS BUILT ABOVE NOW, NOT HERE (B3 THE HOIST,
                // ladder_v2 §4.3). `clutch_blend`, `drive`, `v_track` and
                // `trk_slip` are patch-scope locals, assigned in the hoisted
                // block just above the lateral bite, because the bite runs
                // FIRST and `track_lat_slip_shed` needs the slip there.
                //
                // ⚠ THIS BLOCK READS THEM. IT MUST NEVER RECOMPUTE THEM. Two
                // copies of the slip formula is how the CVT and the rpm readout
                // drift apart -- the comment at the engine_rpm map below says so
                // in its own words. The tripwire that catches a second copy is
                // in ladder_v2 §4.3: grep for the slip's DIVISOR form (the
                // subtraction together with its std::max divisor) and require
                // exactly ONE hit in this file. ⚠ DO NOT WRITE THAT FORM AGAIN,
                // COMMENTS INCLUDED -- a comment quoting it verbatim reds the
                // tripwire, which is how this comment came to be worded around
                // it. (The looser form, the subtraction alone, is 2 on the
                // shipped tree: `roost_thrust`'s `v_rel` below is the second,
                // and a lane obeying THAT form either believes a correct hoist
                // failed or deletes the roost -- the very term his "swing my
                // tail around on account of the roost" names.)
                // ★★ `bury`, `avail` AND `flux` ARE BUILT ABOVE NOW TOO
                // (FOLDED RED-TEAM P1-3, mechanism) -- the shed at the lateral
                // bite needs the ROOST, not the bare slip, and the bite runs
                // first. ⚠ THIS BLOCK READS THEM. IT MUST NEVER RECOMPUTE THEM,
                // for the same reason the slip must not be recomputed: two
                // copies is how the roost bar and the escape thrust drift
                // apart.
                const double flux = trk_flux;

                // Mohr-Coulomb shear on the running surface: what the pack can
                // carry. On hard pack cohesion is high and this is the whole
                // story; in powder it is small and the escape lives in the
                // momentum term below.
                // ★ SC1 (spec §3): c_snow_pa * h on sinkable rows only -- the
                // BOGGED-regime term (inert at top speed, capped elsewhere;
                // spec §0). Road/LakeIce/RockOutcrop/MineWorks (sinkable ==
                // false) read c_snow_pa unscaled, so the structural leg (§5.5)
                // sees them untouched even at an out-of-band test hardness.
                const double sigma = normal / area;
                const double c_eff =
                    d.c_snow_pa * (d.sinkable ? p.snow_hardness : 1.0);
                const double tau_max =
                    c_eff + sigma * std::tan(glm::radians(d.phi_deg));
                const double shear =
                    area * tau_max *
                    (1.0 - std::exp(-std::abs(trk_slip) * g.len /
                                    std::max(p.shear_K_m, kEps)));
                // ★ THE ESCAPE, AND WHY FULL THROTTLE IS THE ANSWER: slip
                // develops thrust BY THROWING MASS BACKWARDS, and that ejected
                // mass IS the roost bar. Momentum flux, read off `flux` -- the
                // same product the roost is drawn from, never a parallel
                // constant.
                const double v_rel = std::abs(v_track - v_fwd);
                const double roost_thrust =
                    p.roost_gain * flux * d.rho_eff * area * v_rel * v_rel;
                double T = (shear + roost_thrust) * (trk_slip >= 0.0 ? 1.0 : -1.0);
                // ★ CLOSED-THROTTLE DECOUPLE (§8: "full decouple below
                // clutch_engage_ms -- free-coast on mu alone"). Below the
                // engage speed the engine is unclutched from the track, so
                // the drivetrain shear/roost contribution fades with the same
                // blend that phases in the engine brake below -- what is left
                // to slow the coast is the generic sliding-friction term
                // (already applied above, unconditionally, to every patch
                // including this one), which is exactly "mu alone."
                T *= drive + (1.0 - drive) * clutch_blend;
                // Engine power is a CEILING, not the limit (§3.2) -- and the
                // drawbar cap is what keeps a contact spike from being spent as
                // thrust.
                // ★ SC1 BREATHING (WINTER_LAW §3.7, spec §3): rho ~ 1/T_K, a
                // few percent from -5 to -30 C. T_ref_K DERIVED from
                // cold_t_ref_c (never a literal), on the record even though
                // it is measurably inert below 32.7 m/s (engine_power_w/v
                // never undercuts max_thrust_n below that speed) -- the
                // smallness IS the record of why the hardness term carries
                // the mechanic, per WINTER_LAW.
                const double T_ref_K = p.cold_t_ref_c + 273.15;
                const double T_K = std::max(p.air_temp_c + 273.15, 1.0);
                const double breathing = T_ref_K / T_K;
                const double cap =
                    std::min(p.max_thrust_n,
                             (p.engine_power_w * breathing) /
                                 std::max(std::abs(v_fwd), 2.0));
                T = std::clamp(T, -cap, cap);
                // ★★ GI4 §9.7 ITEM 2: THE CONTACT CEILING (see traction_mu).
                // Both caps above are ENGINE ceilings; this is the only one the
                // SNOW gets a say in. `shear`'s cohesion term (area*c_eff) and
                // `roost_thrust` are both load-INDEPENDENT, so without this a
                // track carrying 55 N still pulls at max_thrust_n -- the term
                // that feeds the speed->lift->contact-down->speed runaway.
                // A BRANCH, so <= 0 is bit-identical to the pre-item-2 kernel.
                if (p.traction_mu > 0.0) {
                    const double tcap = p.traction_mu * normal;
                    T = std::clamp(T, -tcap, tcap);
                }
                dbg_term = kRollDrive;  // GI4/§8.4
                add_at(T * fwd_t, tan_mount);  // GI S2.1: tangential-moves

                // ★ ENGINE BRAKE (§8 P1-1, extended GI S1/P0-4a): gated on
                // brake < 0.05 OR the engine_brake_stacks dial. The original
                // reasoning (the 0.32-0.42 g measured coast band already
                // includes engine braking, so stacking it under the player's
                // OWN brake pedal would double-count the same deceleration)
                // was measured against the OLD flat brake_force_n alone; S1's
                // mu_brake budget now caps the player brake per-surface
                // independently of engine_brake_n, so the two terms are no
                // longer drawing on the same accounted band by construction.
                // engine_brake_stacks = false reproduces the byte-exact old
                // gate.
                if (brake < 0.05 || p.engine_brake_stacks) {
                    add_at(-p.engine_brake_n * clutch_blend * (1.0 - drive) *
                               fwd_t,
                           tan_mount);  // GI S2.1: tangential-moves
                }
                // ★ GI S1 (P1-10): mu_brake is the TOTAL Coulomb brake budget
                // for this surface, charged against whatever mu_kin_eff (the
                // UNCONDITIONAL sliding-friction term above, already dragging
                // the track) is already spending -- never stacked on top of
                // it. brake_force_n is a ceiling above that budget, not the
                // sole limiter (see SurfaceDials::mu_brake, sled.h). The OFF
                // pair (mu_brake 999 + brake_force_n 1201) reproduces
                // min(1201, huge) = flat 1201 exactly, the pre-S1 kernel.
                if (brake > 0.0 && v_fwd > kEps)
                    add_at(-brake *
                               std::min(p.brake_force_n,
                                        std::max(0.0, (d.mu_brake - mu_kin_eff) *
                                                          normal)) *
                               fwd_t,
                           tan_mount);  // GI S2.1: tangential-moves

                rep_slip = trk_slip;
                rep_flux = flux;
                rep_thrust = T;
            }
        }

        // ★★ GI4 THE PLANING LATERAL PLATE, SECOND PASS -- the MEAN
        // normalisation (plane_lat_load_frac).
        //
        // THE MEASURED HOLE this closes: `draft` is
        // clamp01(sink / plane_draft_m) with plane_draft_m = 0.075 m, so in
        // 0.30 m of Bush BOTH skis saturate it at 1.0 and the UNLOADED inside
        // ski made exactly as much side force as the loaded outside one. The
        // mu bite never had that problem -- it is proportional to the patch's
        // own normal load, so weight transfer quiets the inside ski for free.
        // The plate's version of that asymmetry was missing, and the result
        // was a pure roll couple with no lateral-acceleration benefit: the
        // fence's tip onset fell 1.035 g -> 0.316 g for only 0.10 of gain, a
        // 3.3x collapse bought by a small force (test_sled.cpp
        // gi4_tip_threshold_geometry_trade).
        //
        // WHY THE MEAN AND NOT THE WEIGHT. Normalising against the machine's
        // static weight was tried and MEASURED: it takes the Bush steady
        // radius from 33 m to 198 m, because the ABSOLUTE load is what
        // collapsed (the steered patches hold 186 N of 4106 N at WOT), so any
        // term scaled by it scales to nothing. That removes the FORCE when
        // the intent was to remove only the COUPLE. Normalising each steered
        // patch against the MEAN of the steered patches preserves the total
        // plate exactly -- the shares average to 1 by construction -- while
        // restoring the inside/outside asymmetry. The couple goes, the arc
        // stays.
        //
        // Non-steered patches (the track) are never normalised: there is no
        // pair to take a mean over and the track's plate is not the couple
        // under investigation. plane_lat_load_frac = 0.0 reproduces the
        // pre-fix saturated behaviour exactly.
        {
            // ★ THE DENOMINATOR IS GEOMETRIC, NOT CONTACT-COUNTED. Averaging
            // over the skis currently IN CONTACT was tried and MEASURED to
            // bifurcate: past ~16 m/s the machine rolls far enough to lift
            // the inside ski, that ski drops out of the average, and the
            // TOTAL plate halves (one share of 1 instead of two averaging to
            // 1) -- less turn, so more speed, so more roll, so less turn.
            // Bush went 30.6 m at v0 12 to 259 m at 16 and no turn at all at
            // 20, leaned over at -28 deg and running away at 32 m/s.
            // Dividing by the number of STEERED PATCHES THE MACHINE HAS
            // (a lifted ski contributes patchN = 0 to the sum but still
            // counts in the denominator) keeps the pair's shares summing to
            // 2 through the lift, so the total plate is conserved exactly
            // where it used to collapse.
            double steer_sum = 0.0;
            int steer_n = 0;
            for (int i = 0; i < kPatches; ++i)
                if (platOn[i] && platSteer[i]) {
                    steer_sum += patchN[i];
                    ++steer_n;
                }
            const double steer_mean =
                steer_n > 0 ? steer_sum / steer_n : 0.0;
            for (int i = 0; i < kPatches; ++i) {
                if (!platOn[i]) continue;
                double lat = platN[i];
                if (p.plane_lat_load_frac > 0.0 && platSteer[i] &&
                    steer_mean > kEps) {
                    // Clamped so a fully unloaded ski goes quiet but a fully
                    // loaded one cannot run away: the pair's shares still
                    // average to 1 whenever both are inside the clamp.
                    const double share =
                        std::clamp(patchN[i] / steer_mean, 0.0, 2.0);
                    lat *= (1.0 - p.plane_lat_load_frac) +
                           p.plane_lat_load_frac * share;
                }
                dbg_plate_share[i] = platN[i] > kEps ? lat / platN[i] : 0.0;
                dbg_plate_n[i] = lat;
                dbg_term = kRollPlate;  // GI4/§8.4
                add_at(lat * platF[i], platR[i]);
            }
        }

        // ★ belt_speed_ms / engine_rpm (§8 P1-10 + Phase V P2-A). Computed
        // from the DRIVETRAIN state at body level, never from track-patch
        // contact -- the old in-branch assignment sat behind the airborne
        // `continue`, so a jump at full throttle reported belt 0 and the
        // engine at idle: exactly the lying instrument P1-10 forbids. Same
        // clutch/drive blends as the thrust path, on the body's forward speed.
        {
            const double v_bf = glm::dot(s.velocity, body_fwd);
            const double e_t = std::clamp(
                (v_bf - (p.clutch_engage_ms - 0.25)) / 0.5, 0.0, 1.0);
            const double cb = e_t * e_t * (3.0 - 2.0 * e_t);
            const double d_t =
                std::clamp((throttle - 0.02) / 0.08, 0.0, 1.0);
            const double dv = d_t * d_t * (3.0 - 2.0 * d_t);
            const double v_cmd_b = throttle * p.track_speed_max_ms;
            // driving: the belt runs at least at command; coasting: the
            // ground drags it at ground speed whether the clutch is in or out
            rep_belt = std::max(dv * v_cmd_b, std::max(v_bf, 0.0));
            const double conn = dv + (1.0 - dv) * cb;
            const double rpm_frac =
                clamp01(std::max(v_cmd_b * dv,
                                 conn * rep_belt) /
                        std::max(p.track_speed_max_ms, kEps));
            rep_rpm = 1700.0 + rpm_frac * (8000.0 - 1700.0);
        }

        // RC: the CONTACT PLANE, fit once through the three patch ground
        // points (already sampled this substep — no new ground query, INV-1
        // intact). Falls back to the radial up when degenerate. `phi_surf`
        // is the FULL-RANGE signed body roll about the forward axis relative
        // to that plane: the hull/righting terms key off THIS — a machine
        // conformal to a slope is upright for hull purposes, and a nose-down
        // landing is PITCH, not roll, so it arms nothing (red-team P1-1/2:
        // the earlier gravity-tilt gate armed rigid hull contacts during
        // ordinary slope riding and misread pitched crashes as rollovers).
        glm::dvec3 n_surf = up_cg;
        if (p.plane_fit_load_weight <= 0.0) {
            // ★ GI S2.6 OFF (0.0): the original unweighted fit, untouched --
            // a SEPARATE branch, not a weight-1 special case of the weighted
            // path below, so this stays byte-exact for
            // sled_gi_off_is_bit_identical_to_rc1.
            const glm::dvec3 cxp =
                glm::cross(gpt[1] - gpt[0], gpt[2] - gpt[0]);
            if (glm::length(cxp) > kEps) {
                n_surf = glm::normalize(cxp);
                if (glm::dot(n_surf, up_cg) < 0.0) n_surf = -n_surf;
            }
        } else {
            // ★ GI S2.6 (plane_fit_load_weight, P0-5 restored). NOTE: three
            // points are ALWAYS exactly coplanar, so weighting the edge
            // terms of a 3-point Newell sum cannot change the fitted
            // DIRECTION (every edge cross-product from one triangle is
            // parallel to the same normal; a positive reweighting only
            // rescales a sum of parallel vectors) -- measured, not assumed:
            // an earlier version of this branch did exactly that and
            // produced a bit-identical n_surf to the unweighted branch on
            // every fixture tried. The mechanism that actually matters is
            // RELIABILITY, not a per-edge weight: when the three patch loads
            // are uneven (one patch barely loaded, e.g. a ski held off the
            // ground beside a bank face while its ground SAMPLE point is
            // still read from the bank's terrain), the raw 3-point fit is
            // untrustworthy, so blend it toward up_cg (the fallback below's
            // own target) in proportion to how uneven the loads are.
            // reliability = 1 when the three loads are equal (trust the raw
            // fit fully), -> 0 as the least-loaded patch's share of the mean
            // load vanishes (trust nothing, fall back to radial up).
            const double Nsum = patchN[0] + patchN[1] + patchN[2];
            double reliability = 0.0;
            if (Nsum > kEps) {
                const double Nmin =
                    std::min({patchN[0], patchN[1], patchN[2]});
                reliability = clamp01(Nmin / (Nsum / 3.0));
            }
            const double blend =
                (1.0 - p.plane_fit_load_weight) * 1.0 +
                p.plane_fit_load_weight * reliability;
            const glm::dvec3 cxp =
                glm::cross(gpt[1] - gpt[0], gpt[2] - gpt[0]);
            glm::dvec3 n_raw = up_cg;
            if (glm::length(cxp) > kEps) {
                n_raw = glm::normalize(cxp);
                if (glm::dot(n_raw, up_cg) < 0.0) n_raw = -n_raw;
            }
            n_surf = glm::normalize(
                glm::mix(up_cg, n_raw, std::clamp(blend, 0.0, 1.0)));
        }
        const double phi_surf =
            std::atan2(glm::dot(R * glm::dvec3(1, 0, 0), n_surf),
                       glm::dot(body_up, n_surf));

        // --- RC item C1: the on-side contact story (§0b, RC-4) -------------
        // Coarse hull points — tunnel/seat edge, bar ends, ski outers — as
        // unilateral spring-dampers against THE one drive surface (INV-1:
        // same snow.sample_at as the patches). A machine on its side finally
        // has somewhere to stand, so slope + momentum decide whether it
        // slides back over its ski line — the DISTRIBUTION Chad ruled, from
        // geometry, not RNG. Engagement blends in across [25, 35] deg of
        // SURFACE-RELATIVE roll: upright (or conformal to any slope, or
        // pitched nose-down into a landing) the hull is inert and hard
        // landings stay on the measured patch path.
        double side_normal_sum = 0.0;
        if (p.comfort.side_k > 0.0 &&
            std::abs(phi_surf) > glm::radians(25.0)) {
            const double ts = std::clamp(
                (std::abs(phi_surf) - glm::radians(25.0)) / glm::radians(10.0),
                0.0, 1.0);
            const double w_side = ts * ts * (3.0 - 2.0 * ts);
            const double half = 0.5 * p.stance_m;
            const double my = -(p.cg_height_m - p.susp_rest_m);
            // The coarse convex hull a downed machine rests on (~65-110 deg)
            // — the arrest is what makes a slide-back-over possible at all.
            // Offsets are taken from the same composite CG the patches use
            // (- cg_off): the rider's lean moves the hull's moment arms too
            // (red-team P2-8).
            // ★ GI S3.1 (P1-8/P1-9): the first 6 points are the RC1 literals,
            // BYTE-UNCHANGED and in the same order (structural bit-identity
            // -- side_hull_points = 6 in gi_off() takes exactly the RC1 hull
            // as a sub-loop of this same array, not a re-derivation). The 4
            // new points fill the LENGTHWISE gap the original 6 left: a
            // machine on its side had contact at the seat edge, the bar end
            // and the ski outer, with nothing between the bar (z=-0.30) and
            // the seat edge (z=track_aft_m) or beyond either -- a downed
            // machine's real running-board/tunnel edge spans nose to tail.
            // NUMERIC (P1-8): |x| = 0.58 (within the ski-outer's own 0.5835
            // so the new points actually load at 90 deg alongside it, not
            // float clear of the pack); y = 0.45 (between the ski
            // outer's own y=my and the seat-edge/bar-end row at y=0.45-0.55
            // -- MEASURED against the S3.1 requirement leg,
            // sled_hull_grows_to_ten_points_and_loads_at_90: +0.20 left only
            // 3 of 5 same-side points loaded at a settled 90 deg rest,
            // +0.12 clears the >=4 bar); z spans nose (-0.9, past
            // ski_fwd_m=0.86) to rear (+0.7, past track_aft_m=0.52).
            const glm::dvec3 pts[10] = {
                {0.32, 0.45, p.track_aft_m},   // tunnel/seat edge R
                {-0.32, 0.45, p.track_aft_m},  // tunnel/seat edge L
                {0.38, 0.55, -0.30},           // bar end R
                {-0.38, 0.55, -0.30},          // bar end L
                {half + 0.12, my, -p.ski_fwd_m},     // ski outer R
                {-(half + 0.12), my, -p.ski_fwd_m},  // ski outer L
                {0.58, my + 0.12, -0.9},   // hull running-board, nose R
                {-0.58, my + 0.12, -0.9},  // hull running-board, nose L
                {0.58, my + 0.12, 0.7},    // hull running-board, rear R
                {-0.58, my + 0.12, 0.7},   // hull running-board, rear L
            };
            const int n_hull =
                std::clamp(p.comfort.side_hull_points, 0, 10);
            dbg_term = kRollHull;  // GI4/§8.4
            for (int hi = 0; hi < n_hull; ++hi) {
                const glm::dvec3& pt = pts[hi];
                const glm::dvec3 ptc = pt - cg_off;
                const glm::dvec3 r_w = R * ptc;
                const glm::dvec3 w = s.position + r_w;
                const glm::dvec3 up_p = glm::normalize(w);
                // ★ GI S3.3 (P1-16): `.surf` captured here -- the hull loop
                // was previously not in scope of the SurfaceDials row at
                // all, so the cohesion plough below (a sinkable-only term,
                // same gate class as the patches' own c_snow_pa use) has
                // something to read.
                const world::SnowpackField::GroundSample hgs =
                    snow.sample_at(up_p);
                const double pen = hgs.drive_r - glm::length(w);
                if (pen <= 0.0) continue;
                // ★ K-WS1 / K-A3: a machine lying on its running-board IS in
                // ground contact -- the -28 deg attractor carries 4 kN here
                // with 62 N on the patches, and a term that called that
                // "airborne" would be wrong exactly where GI4 hurts.
                ground_contact = true;
                const glm::dvec3 v_pt =
                    s.velocity + glm::cross(world_omega, r_w);
                const double vn = glm::dot(v_pt, up_p);
                const double nrm =
                    std::max(p.comfort.side_k * pen - p.comfort.side_c * vn,
                             0.0) *
                    w_side;
                if (nrm <= 0.0) continue;
                side_normal_sum += nrm;
                add_at(nrm * up_p, ptc);
                const glm::dvec3 vt = v_pt - vn * up_p;
                const double lt = glm::length(vt);
                if (lt > kEps) {
                    const glm::dvec3 that = vt / lt;
                    add_at(-p.comfort.side_mu * nrm * that, ptc);
                    // ★ GI S3.3: the cohesion plough, sinkable rows only --
                    // 0.0 = OFF by measured finding (see SledComfort::
                    // hull_shear_width_m's header comment); MEASURED inert
                    // at the shipped value, computed here so it is a real
                    // code path a future retune can turn on rather than a
                    // decoration.
                    if (p.comfort.hull_shear_width_m > 0.0) {
                        const SurfaceDials& hd =
                            p.dials[static_cast<int>(hgs.surf)];
                        if (hd.sinkable) {
                            const double shear = hd.c_snow_pa * pen *
                                                 p.comfort.hull_shear_width_m;
                            add_at(-shear * that, ptc);
                        }
                    }
                }
            }
            // ★ RC item C2: righting bias — the one overtly arcade term, and
            // it may only AMPLIFY side-slide momentum that already exists.
            // Three honesty gates (red-team P1-3/P1-4): the speed it reads
            // is the LATERAL slide only (forward speed buys nothing — the
            // dial's name is not a lie), the enable ramps with the hull
            // LOAD (no step torque at first graze), and the roll window
            // [50, 140] deg of |phi_surf| hands the machine to the assist
            // band instead of holding odd equilibria. Zero below vmin: a
            // stationary machine NEVER self-rights (R is the backstop).
            if (p.comfort.side_right_gain_nm > 0.0 &&
                side_normal_sum > kEps) {
                const glm::dvec3 fwd_h =
                    body_fwd - glm::dot(body_fwd, up_cg) * up_cg;
                double v_side = 0.0;
                if (glm::length(fwd_h) > 1e-6) {
                    const glm::dvec3 lat_dir = glm::normalize(
                        glm::cross(up_cg, glm::normalize(fwd_h)));
                    v_side = std::abs(glm::dot(s.velocity, lat_dir));
                }
                const double wv = clamp01(
                    (v_side - p.comfort.side_right_vmin_ms) /
                    std::max(p.comfort.side_right_vref_ms -
                                 p.comfort.side_right_vmin_ms,
                             kEps));
                // Saturates at 2% of weight (~65 N): recovery is an IMPULSE
                // harvested during the bouncing side grazes — a slow ramp
                // throttled exactly those and the machine stalled at 72 deg
                // on the ski rail (measured); 65 N keeps the enable
                // continuous (red-team P1-4) without starving the impulse.
                const double w_load = clamp01(
                    side_normal_sum / (0.02 * mass * 9.80665));
                const double aphi = std::abs(phi_surf);
                // Lower window edge 40 deg (was 50): the trace showed a
                // DEAD ZONE 50-70 deg where the machine stalls mid-recovery
                // — C2 fading out while the A assist is still patch-load-
                // gated to ~0 (patches unloaded on the side) — so the bias
                // must keep pushing through the ski-line pivot before the
                // handoff.
                const double t_lo = std::clamp(
                    (aphi - glm::radians(40.0)) / glm::radians(10.0), 0.0,
                    1.0);
                const double t_hi = std::clamp(
                    (glm::radians(140.0) - aphi) / glm::radians(15.0), 0.0,
                    1.0);
                const double wt = t_lo * t_lo * (3.0 - 2.0 * t_lo) * t_hi *
                                  t_hi * (3.0 - 2.0 * t_hi);
                const double sgn_c2 = phi_surf >= 0.0 ? 1.0 : -1.0;
                // ★ GI S3.5 (P1-7): the wref gate -- MEASURED against the
                // L4 probe sweep to run the OPPOSITE way from the naive
                // "more spin, more push" guess. Recovery from a real trip
                // carries real roll-rate (|w_roll| ~ 5 rad/s, well above
                // every swept wref) at the exact moment the bias is needed,
                // so a gate that GROWS with |w| throttles the assist to
                // nothing right when it matters (measured: every gain
                // candidate failed the L4 leg under that reading). The gate
                // that actually reproduces "solve gain+wref together" is
                // the INVERSE: a machine already carrying more roll-rate
                // than wref does not need MORE push (it already has the
                // momentum C2 exists to harvest -- adding torque on top
                // risks ringing past the barrier, the same class of concern
                // sled_righting_bias_cannot_outwork_the_barrier gates), so
                // the bias fades OUT above wref and is strongest near zero
                // roll-rate, where the hull's own bouncing impulses (this
                // dial's own header comment: "recovery is an IMPULSE
                // harvested during the bouncing side grazes") most need the
                // extra nudge. 1e9 (gi_off()) makes this identically 1.0 --
                // inert.
                const double wo = clamp01(
                    1.0 - std::abs(s.angular_vel.z) /
                              std::max(p.comfort.side_right_wref_rads, kEps));
                dbg_tq_c2 = -sgn_c2 * p.comfort.side_right_gain_nm * wv * wt *
                            w_side * w_load * wo;
                torque_body.z += dbg_tq_c2;
            }
        }

        // --- RC item C3 (S3.4): yaw-arrest torque ---------------------------
        // ★ GI S3.4 (P1-6). The hull points above supply only TRANSLATIONAL
        // friction (side_mu, against the sliding velocity at each point) --
        // nothing damps a downed machine's SPIN about the world-vertical
        // axis, so a trip that leaves it turning kept turning forever.
        // hull_engage_lp is real state, LOW-PASSED (0.1 s) so a single
        // graze cannot slam a step torque; it tracks the same normalized
        // hull-load fraction the C2 bias itself gates on (w_load's own
        // target), updated every substep regardless of side_yaw_mu so the
        // filter stays warmed up whenever the hull is active at all.
        {
            const double engage_target =
                side_normal_sum > kEps
                    ? clamp01(side_normal_sum / (0.02 * mass * 9.80665))
                    : 0.0;
            s.hull_engage_lp = slew_toward(s.hull_engage_lp, engage_target,
                                           0.1, 1e9, h);
        }
        if (p.comfort.side_yaw_mu > 0.0 && s.hull_engage_lp > kEps) {
            const double w_vert = glm::dot(world_omega, up_cg);
            if (std::abs(w_vert) > 1e-4) {  // eps-guarded sign
                // I_eff projected LIVE: up_cg^T . R . I . R^T . up_cg, never
                // I.y (measured ~158.7 == I.x at 90 deg roll -- the roll
                // axis rotates the world-vertical direction into the body's
                // PITCH axis, not the yaw one).
                const glm::dvec3 up_body = glm::transpose(R) * up_cg;
                const double I_eff = glm::dot(up_body, I * up_body);
                // The ceiling: the torque that would EXACTLY zero the
                // vertical spin this SUBSTEP (h = dt/substeps, never dt --
                // the 12x matters, sled.cpp's own substep count). side_yaw_mu
                // is the FRACTION of that ceiling actually spent, so the
                // clamp below is a defensive guard against a future
                // side_yaw_mu > 1, not the thing doing the work day to day.
                const double ceiling = I_eff * std::abs(w_vert) / h;
                double T = -std::copysign(1.0, w_vert) * p.comfort.side_yaw_mu *
                           ceiling * s.hull_engage_lp;
                T = std::clamp(T, -ceiling, ceiling);
                dbg_tq_c3 = T;  // GI3/R3 sink capture (about world up)
                // Body-frame transform: the torque acts about WORLD up, so
                // it has to be rotated into body axes like every other
                // torque this kernel accumulates.
                const glm::dvec3 t_c3 = glm::transpose(R) * (T * up_cg);
                dbg_c3_roll = t_c3.z;  // GI4/§8.4: the yaw term's roll spill
                torque_body += t_c3;
            }
        }

        // --- R4a SEATED SELF-RIGHT v2 (Chad, 2026-08-26) ------------------
        // He stayed ON through the rollover; STAND rights the machine, and it
        // takes A FEW SUSTAINED PUSHES from a full inversion. The whole block
        // is inside `right_assist_nm > 0.0`, which is 0.0 by default and 0.0
        // for every existing tape (tape-absent preset) -- so the corpus is
        // bit-identical, integrators included, unless someone turns it on.
        //
        // The full derivation of why v1 failed, and why this is a ROCKING
        // model, is on the params in sim/sled.h. In short: the rock lives in
        // the rigid body's own state (orientation + angular_vel, damped by the
        // hull), nothing counts pushes, and the only new memory is a pusher who
        // TIRES -- which is what makes "a few sustained pushes" emerge instead
        // of being scripted.
        // ★ R4a STAGE 2: AND HE HAS TO BE ON IT TO HEAVE IT. This assist IS
        // the rider's own body weight on the uphill board -- its comment above
        // says "he stayed ON through the rollover" in as many words -- so a
        // machine he has been thrown off cannot be righted by it. Without this
        // the STAND key would right a riderless sled from across the field.
        // (Both gates are off by default today, so this changes nothing until
        // both are on; it is here so the two can never be on at once and lie.)
        if (p.comfort.right_assist_nm > 0.0 && hands_on) {
            const glm::dvec3 up_body = glm::transpose(R) * up_cg;
            const double tilt = std::acos(std::clamp(up_body.y, -1.0, 1.0));
            // Tilt RAMP, not a hard edge.
            const double t_lo = p.comfort.right_tilt_lo_rad;
            const double t_hi = p.comfort.right_tilt_hi_rad;
            double w_tilt = (t_hi > t_lo) ? (tilt - t_lo) / (t_hi - t_lo) : 1.0;
            w_tilt = std::clamp(w_tilt, 0.0, 1.0);
            w_tilt = w_tilt * w_tilt * (3.0 - 2.0 * w_tilt);  // smoothstep
            // Hysteretic speed gate on the LOW-PASSED speed (see the param).
            const glm::dvec3 v_h_r =
                s.velocity - glm::dot(s.velocity, up_cg) * up_cg;
            const double gs_r = glm::length(v_h_r);
            const double lp_tau = std::max(1e-6, p.comfort.right_speed_lp_s);
            s.right_gs_lp += (gs_r - s.right_gs_lp) * (h / lp_tau);
            const double gate_hi = p.comfort.right_assist_max_ms;
            const double gate_lo = gate_hi * p.comfort.right_assist_rearm_frac;
            if (s.right_assist_armed && s.right_gs_lp > gate_hi)
                s.right_assist_armed = false;
            else if (!s.right_assist_armed && s.right_gs_lp < gate_lo)
                s.right_assist_armed = true;
            // PROGRESSIVE: scales with how hard he is standing. in.stand is
            // [-1 tuck .. 0 seated .. +1 standing]; only the standing half
            // rights the machine, so a tuck does nothing.
            const double push = clamp01(static_cast<double>(in.stand));
            const double p_eff =
                (s.right_assist_armed ? 1.0 : 0.0) * push * w_tilt;
            // THE PUSHER TIRES. Contracting first-order ODE either way, so it
            // is unconditionally stable at this substep and bounded to [0,1].
            // A sustained hold drives charge -> 0: an infinite press injects
            // FINITE energy and then nothing, which is both his "a second
            // sustained press might help it further" and the anti-blow-up
            // bound. Releasing refills it for the next swing.
            const double tau_push = std::max(1e-6, p.comfort.right_charge_push_s);
            const double tau_rest = std::max(1e-6, p.comfort.right_charge_rest_s);
            s.right_charge +=
                h * ((1.0 - p_eff) * (1.0 - s.right_charge) / tau_rest -
                     p_eff * s.right_charge / tau_push);
            s.right_charge = std::clamp(s.right_charge, 0.0, 1.0);
            if (p_eff > 0.0) {
                // ★ NOTHING HERE DECIDES TO FAIL. The assist is a torque, the
                // hull contact carrying m*g about the pivot is another, and the
                // dampers eat energy every swing. When the per-cycle losses
                // match the per-press work the rock plateaus below the crest
                // and it does not come up. "IT CAN FAIL TO RIGHT GIVEN THE
                // SITUATION" is EMERGENT -- deep snow, a slope, or leaning the
                // wrong way -- never a scripted refusal. That is what makes R
                // the honest escape hatch he ruled it into.
                //
                // Direction, saturated: full authority except within ~10 deg of
                // exact inversion / exact upright, and the seed makes it agree
                // with the way his LEAN is already tipping the machine.
                // ★★ THE LEAN IS NOT PART OF THIS. Chad, 2026-08-26: "I dont
                // understand why you are conflating the lean mechanism with the
                // righting after the rollover, im not going to ask a player to
                // lean with the mouse when inverted... leaning is for the
                // flying off the handlebar direction."
                //
                // He is right, and it was my error twice over: a `right_seed_frac`
                // term read his LEAN INPUT into the righting direction, so which
                // way the machine came up depended on a control that belongs to
                // a different system entirely. Deleted. The side comes from the
                // MACHINE'S OWN ATTITUDE and nothing else -- which is the only
                // honest source, since the machine is what is lying over.
                //
                // (The automatic brace below is NOT leaning: the player commands
                // nothing, it is the rider's body going where a man's body goes
                // when he stands up to shove -- his own ruling, "standing
                // automatically helps push you over righted".)
                // The commit reinforces itself: once he has thrown his weight
                // to a side, that side is where the shove goes. This reads the
                // LATCHED BRACE -- the mechanic's own committed side -- and
                // NEVER `rider_lat_m`, which carries his lean input too. That
                // distinction is the whole of his correction: the machine's
                // attitude and the shove's own commitment decide the righting;
                // his lean decides which way he flies off, and the two must not
                // touch.
                const double braced =
                    (s.right_shift_cmd > 0.0) ? 1.0
                                              : (s.right_shift_cmd < 0.0 ? -1.0
                                                                         : 0.0);
                const double e =
                    -up_body.x - p.comfort.right_seed_frac * braced;
                const double eps = std::max(1e-6, p.comfort.right_dir_eps);
                const double dir = std::tanh(e / eps);
                // ★ PUSH WITH THE MOTION. `align` is the roll rate projected
                // onto the direction the push wants to turn the machine: > 0
                // means the rock is already going that way and the press does
                // POSITIVE work (energy in), < 0 means it would fight the
                // return and take energy back out. At rest it is 0 and the
                // weight is 0.5, so the first press can still start the rock.
                // Without this the torque nets ~zero work per cycle and no
                // number of presses beats one -- measured, see sim/sled.h.
                const double om_eps =
                    std::max(1e-6, p.comfort.right_pump_omega_eps);
                const double align = s.angular_vel.z * dir / om_eps;
                const double w_pump = 0.5 * (1.0 + std::tanh(align));
                s.right_assist_nm_now = p.comfort.right_assist_nm * p_eff *
                                        s.right_charge * dir * w_pump;
                // Brace toward the side the righting is pushing. rider_lat_m is
                // + LEFT and the torque that rights a machine leaning that way
                // is negative, so the brace is OPPOSITE the torque's sign.
                //
                // ★ THE SIDE IS LATCHED FOR THE WHOLE PRESS. Tracking `dir`
                // instantaneously makes the brace FLIP as the machine rocks
                // through, so his weight fights the swing it is supposed to be
                // driving -- measured as a stall at ~96 deg. A man commits to a
                // side and shoves. The latch clears on release (the else
                // branches below), so every fresh press picks again.
                const double side = (s.right_shift_cmd != 0.0)
                                        ? (s.right_shift_cmd > 0.0 ? 1.0 : -1.0)
                                        : (dir < 0.0 ? 1.0 : -1.0);
                s.right_shift_cmd = side * p_eff * s.right_charge;
                torque_body.z += s.right_assist_nm_now;
            } else {
                s.right_assist_nm_now = 0.0;
                s.right_shift_cmd = 0.0;
            }
        } else {
            s.right_assist_nm_now = 0.0;
            s.right_shift_cmd = 0.0;
        }

        // --- N1 THE LEG WORK (Chad 2026-09-18, runs 5 and 7) ---------------
        // The two stuck attitudes the pendulum above cannot reach: on its END
        // (PITCHED) and on its BACK (INVERTED). Everything is on sim/sled.h
        // SledComfort::leg_work_nm; here is the mechanism, in order:
        //   1. gates: hands on, ground contact (the rolled latch's own
        //      air_s <= rolled_grace_s), the pendulum's hysteretic
        //      low-passed speed gate (right_assist_armed, advanced above),
        //      AND the rolled latch itself (s.rolled: tilt > 75 deg held
        //      rolled_persist_s in contact -- the red-team fold below).
        //   2. band: which attitude this substep is in, read against the
        //      ARMED stage's EXIT threshold and everything else's ENTER
        //      threshold (the hysteresis).
        //   3. dwell: a band must hold rolled_persist_s before it ARMS; the
        //      substep the attitude leaves an armed band (or a gate drops)
        //      the stage DISARMS and a fresh dwell is owed -- one-way.
        //   4. press: a RISING EDGE of the stage's own key AFTER it armed
        //      (CTRL for PITCHED, SHIFT for INVERTED); a key already held when
        //      the machine got stuck never fires. Release ends the press.
        //   5. torque: budget x charge x pump weight on the driven axis;
        //      PITCHED also spends kLegRollShare of the budget about Z
        //      toward the side the machine already leans, so it lands on a
        //      side (exact symmetry lands it on the track -- accepted).
        // The sign law: +angular_vel.x is nose UP and nose-down is
        // +up_body.z (MEASURED, test legwork_pitch_sign_is_measured). PITCHED
        // pushes +X, nose UP, both ends ("backward", see below); INVERTED
        // pushes tanh(up_body.z / eps) about X, which GROWS |z| (lifts the
        // end that is already higher).
        // ★ 0.0 == today's kernel bit for bit: nothing inside is reached, and
        // no state is written (leg_prev_stand included).
        if (p.comfort.leg_work_nm > 0.0) {
            const glm::dvec3 up_body = glm::transpose(R) * up_cg;
            const double ax = std::abs(up_body.x);
            const double az = std::abs(up_body.z);
            const bool contact = s.air_s <= p.comfort.rolled_grace_s;
            // ★ RED-TEAM FOLD (P1, 2026-09-19): AND THE KERNEL'S OWN ROLLED
            // LATCH. Without it the PITCHED band (|up_body.z| >= sin 50)
            // read against RADIAL up armed on a machine parked UPRIGHT on a
            // >= 50 deg bank -- measured: 52.2 deg ridge flank, tilt 51.8,
            // rolled 0, stage ARMED, one CTRL edge -> 2049 N m, omega 11.0
            // rad/s, 1.23 s airborne, over onto its back down the hill: the
            // tumble the identity kernel never produces. `s.rolled` is the
            // readout above (tilt > 75 deg HELD rolled_persist_s in ground
            // contact), the same fact that makes R legal (app/player_mode.h
            // autoright_legal) -- the legs are legal exactly where R is.
            // Both rest fixtures sit at 105.5 / 173.5 deg with it latched,
            // so nothing this block was measured on moves. Arithmetic: rolled
            // (|y| <= cos 75) AND not on a side (|x| < sin 35) already puts
            // |z| > 0.777 > sin 50, so the PITCHED enter threshold is implied
            // here; it stays as the band's stated shape and its exit (sin 40)
            // is the hysteresis that still matters.
            const bool gated =
                hands_on && contact && s.right_assist_armed && s.rolled;
            int band = kLegNone;
            if (up_body.y <=
                (s.leg_stage == kLegInverted ? kLegInvExit : kLegInvEnter))
                band = kLegInverted;
            else if (az >= (s.leg_stage == kLegPitched ? kLegPitchExit
                                                       : kLegPitchEnter) &&
                     ax < kLegSideBand)
                band = kLegPitched;
            else if (ax >= kLegPitchEnter)
                band = kLegOnSide;
            if (!gated) {
                s.leg_stage = kLegNone;
                s.leg_press = false;
                s.leg_cand = kLegNone;
                s.leg_dwell_s = 0.0;
            } else if (band == s.leg_stage) {
                s.leg_cand = kLegNone;
                s.leg_dwell_s = 0.0;
            } else {
                if (s.leg_stage != kLegNone) {  // left the armed band: one-way
                    s.leg_stage = kLegNone;
                    s.leg_press = false;
                }
                if (band != s.leg_cand) {
                    s.leg_cand = band;
                    s.leg_dwell_s = 0.0;
                }
                s.leg_dwell_s += h;
                if (band != kLegNone &&
                    s.leg_dwell_s >= p.comfort.rolled_persist_s) {
                    s.leg_stage = band;  // ARMED, silently
                    s.leg_press = false;
                    s.leg_cand = kLegNone;
                    s.leg_dwell_s = 0.0;
                }
            }
            // The press: a rising edge of the stage's own key, after arming.
            auto key_of = [](float v) {
                return v > 0.5f ? +1 : (v < -0.5f ? -1 : 0);
            };
            const int key = key_of(in.stand);
            const int prev = key_of(s.leg_prev_stand);
            s.leg_prev_stand = in.stand;
            const int want = (s.leg_stage == kLegPitched)    ? -1
                             : (s.leg_stage == kLegInverted) ? +1
                                                             : 0;
            if (want != 0 && key == want && prev != want) s.leg_press = true;
            if (want == 0 || key != want) s.leg_press = false;
            const double p_leg = s.leg_press ? 1.0 : 0.0;
            // The legs tire exactly as the pusher does (the pendulum's two
            // taus): a hold spends finite energy, a release refills it.
            const double tau_push =
                std::max(1e-6, p.comfort.right_charge_push_s);
            const double tau_rest =
                std::max(1e-6, p.comfort.right_charge_rest_s);
            s.leg_charge +=
                h * ((1.0 - p_leg) * (1.0 - s.leg_charge) / tau_rest -
                     p_leg * s.leg_charge / tau_push);
            s.leg_charge = std::clamp(s.leg_charge, 0.0, 1.0);
            if (p_leg > 0.0) {
                const double eps = std::max(1e-6, p.comfort.right_dir_eps);
                const double om_eps =
                    std::max(1e-6, p.comfort.right_pump_omega_eps);
                // PITCHED: "roll it over BACKWARD" (his word) = nose UP, +X
                // by the measured law, for BOTH ends -- a nose-down machine
                // comes back onto its track, a nose-up one goes over onto
                // its back or side (the roll share below). MEASURED: the
                // nose-up rest is a STABLE two-contact pose at 105.5 deg
                // (tail + rear hull), so "toward level" would fight ~900 N m
                // of gravity through top dead centre and 1500 never leaves
                // it (test legwork ladder probe); backward goes WITH it.
                // INVERTED: lift the end that is already higher,
                // tanh(up_body.z / eps) -- in that band the sign that grows
                // |z| is the one that raises the higher end.
                const double dir_x = (s.leg_stage == kLegPitched)
                                         ? 1.0
                                         : std::tanh(up_body.z / eps);
                const double w_pump_x =
                    0.5 * (1.0 + std::tanh(s.angular_vel.x * dir_x / om_eps));
                const double t_x = p.comfort.leg_work_nm * p_leg *
                                   s.leg_charge * dir_x * w_pump_x;
                torque_body.x += t_x;
                double t_z = 0.0;
                if (s.leg_stage == kLegPitched) {
                    // The pendulum's own side rule (its latched brace is 0
                    // under CTRL, so this is the machine's own lean).
                    const double braced =
                        (s.right_shift_cmd > 0.0)
                            ? 1.0
                            : (s.right_shift_cmd < 0.0 ? -1.0 : 0.0);
                    const double dir_z = std::tanh(
                        (-up_body.x - p.comfort.right_seed_frac * braced) /
                        eps);
                    const double w_pump_z =
                        0.5 *
                        (1.0 + std::tanh(s.angular_vel.z * dir_z / om_eps));
                    t_z = kLegRollShare * p.comfort.leg_work_nm * p_leg *
                          s.leg_charge * dir_z * w_pump_z;
                    torque_body.z += t_z;
                }
                s.leg_nm_now = std::abs(t_x) + std::abs(t_z);
            } else {
                s.leg_nm_now = 0.0;
            }
        }

        // --- RC item A (+B2): contact-gated saturating roll stiffness ------
        // The §0b core fix — "the sway bar that doesn't exist". phi is the
        // signed body roll against a BLEND of the radial up and the contact
        // plane (roll_ref_blend; at the shipped 0.5 a settled 20-deg
        // side-hill reads phi ~ 10 deg, i.e. a deliberate, permanent uphill
        // bias on cross-slopes — the measured dial choice in sled.h, stated
        // here too so this comment is not a lying instrument). The release
        // smoothstep lets go across [release_lo, release_hi] — applied to
        // the DAMPING term as well (red-team P2-7), so past the band the
        // whole assist lets go and abuse still rolls (RC-1); past 90 deg
        // atan2 keeps growing and a downed machine gets NO push. w_contact
        // is the summed PATCH load: identically 0 airborne, so backflips and
        // the airborne-lean anti-cheat leg stay bit-exact. B2: the rider's
        // commanded lean shifts the release band outward — a committed rider
        // carries more roll angle before the assist lets go (lean =
        // enhancer, formalized). The torque is REPORTED in assist_nm, never
        // branched on.
        s.assist_nm = 0.0;
        if (p.comfort.roll_stiff_nm > 0.0 || p.comfort.roll_damp_nms > 0.0) {
            const glm::dvec3 n_ref = glm::normalize(
                up_cg + (n_surf - up_cg) * p.comfort.roll_ref_blend);
            const double phi =
                std::atan2(glm::dot(R * glm::dvec3(1, 0, 0), n_ref),
                           glm::dot(body_up, n_ref));
            const double ext =
                p.comfort.lean_sat_gain_rad *
                clamp01(lean_frac * (phi >= 0.0 ? 1.0 : -1.0));
            const double a = std::max(0.0, std::abs(phi) - ext);
            const double lo = p.comfort.roll_release_lo_rad;
            const double hi =
                std::max(p.comfort.roll_release_hi_rad, lo + 1e-3);
            const double tr = std::clamp((a - lo) / (hi - lo), 0.0, 1.0);
            const double release = 1.0 - tr * tr * (3.0 - 2.0 * tr);
            // ★ GI3 F-A: the contact gate reads the HULL's load too — the
            // sink trace of Chad's roll (tapedbg 58.5-60.2 s) measured the
            // patch-only gate zeroing the assist at 32-43 deg while the
            // machine stood on its running-board at ~4 kN of side_normal_sum
            // with the release band still at 0.96. A machine on its hull is
            // in ground contact; airborne stays exactly 0 (both sums are).
            // assist_hull_frac = 0.0 is the pre-GI3 gate bit-exactly.
            const double w_contact = clamp01(
                (normal_sum + p.comfort.assist_hull_frac * side_normal_sum) /
                (0.5 * mass * 9.80665));
            // ★ GI3 F-B/F-C: authority-at-speed and the release floor, on
            // one shared smoothstep speed gate. Both 0.0-OFF: the branch is
            // skipped and stiff/release are the pre-GI3 values untouched.
            double stiff = p.comfort.roll_stiff_nm;
            double release_eff = release;
            if (p.comfort.roll_stiff_vgain > 0.0 ||
                p.comfort.release_floor_frac > 0.0) {
                const glm::dvec3 vh =
                    s.velocity - glm::dot(s.velocity, up_cg) * up_cg;
                const double tv = clamp01(
                    (glm::length(vh) - p.comfort.assist_v_lo_ms) /
                    std::max(p.comfort.assist_v_hi_ms -
                                 p.comfort.assist_v_lo_ms,
                             kEps));
                const double gv = tv * tv * (3.0 - 2.0 * tv);
                // F-B: the ceiling grows with the speed whose lateral demand
                // it must answer — tanh keeps the saturating SHAPE, only the
                // scale moves. Never below the parked value.
                stiff = p.comfort.roll_stiff_nm *
                        (1.0 + p.comfort.roll_stiff_vgain * gv);
                // F-C: the floor a live carve keeps. Its own outer fade
                // (release_floor_hi_rad) reaches 0 before 90 deg of roll, so
                // a downed machine still gets NO push — the release band's
                // abuse-still-rolls contract (RC-1) holds at every speed.
                const double fh =
                    std::max(p.comfort.release_floor_hi_rad, lo + 1e-3);
                const double tf = std::clamp((a - lo) / (fh - lo), 0.0, 1.0);
                const double fade = 1.0 - tf * tf * (3.0 - 2.0 * tf);
                release_eff = std::max(
                    release, p.comfort.release_floor_frac * gv * fade);
            }
            const double tq =
                (-stiff *
                     std::tanh(phi / std::max(p.comfort.roll_ref_rad, kEps)) -
                 p.comfort.roll_damp_nms * s.angular_vel.z) *
                release_eff * w_contact;
            torque_body.z += tq;
            s.assist_nm = tq;
            dbg_release = release_eff;  // GI3/R3 sink captures
            dbg_w_contact = w_contact;
            dbg_phi = phi;
        }

        // ★ GI3/R3: the sink fill — ONE write site, after every torque term
        // has accumulated, before integration. Everything below is a copy of
        // locals already computed; nothing here feeds back (the bit-identity
        // leg sled_debug_sink_is_write_only is the proof, not this comment).
        if (dbg) {
            SledDebugSubstep rec;
            for (int i = 0; i < kPatches; ++i) {
                rec.patch_n[i] = patchN[i];
                rec.patch_surf[i] = dbg_surf[i];
            }
            rec.normal_sum = normal_sum;
            rec.side_normal_sum = side_normal_sum;
            rec.lean_frac = lean_frac;
            rec.align_m = align_m;
            rec.phi = dbg_phi;
            rec.phi_surf = phi_surf;
            rec.release = dbg_release;
            rec.w_contact = dbg_w_contact;
            rec.tq_assist = s.assist_nm;
            rec.tq_c2_right = dbg_tq_c2;
            rec.tq_c3_yaw = dbg_tq_c3;
            const glm::dvec3 v_h =
                s.velocity - glm::dot(s.velocity, up_cg) * up_cg;
            rec.v_ground = glm::length(v_h);
            // ★ GI4/§8.4 roll-axis attribution + the plate's own shares.
            for (int t = 0; t < kRollTermCount; ++t) rec.roll_tq[t] = dbg_roll[t];
            rec.tq_c3_roll = dbg_c3_roll;
            for (int i = 0; i < kPatches; ++i) {
                rec.plate_share[i] = dbg_plate_share[i];
                rec.plate_n[i] = dbg_plate_n[i];
                rec.lift_n[i] = dbg_lift_n[i];
                rec.sink_m[i] = s.sink_m[i];
            }
            rec.omega_body[0] = s.angular_vel.x;
            rec.omega_body[1] = s.angular_vel.y;
            rec.omega_body[2] = s.angular_vel.z;
            dbg->substeps.push_back(rec);
        }

        // Air drag and gravity act at the CG -- the rider's cg_off never
        // moves these two, only the patch mounts (§8: "airborne claim is
        // ANGULAR only, stand changes cda so linear drag differs, and that is
        // correct and stated").
        const double v_len = glm::length(s.velocity);
        if (v_len > kEps) force += -0.5 * p.air_rho * cda * v_len * s.velocity;
        force += -9.80665 * mass * up_cg;

        const double mean_sink =
            sink_area_den > kEps ? sink_area / sink_area_den : 0.0;
        rep_plane_num = lift_sum;
        rep_plane_den = mass * 9.80665 +
                        trench_rho * 9.80665 * p.plan_area_m2 * mean_sink;

        // --- integrate the rigid body ---------------------------------------
        s.velocity += (force / mass) * h;
        s.position += s.velocity * h;

        // ★ G1 ROTOR GYROSCOPICS. L_r rides INSIDE the existing cross product
        // because that is what the body+rotor Euler equation says:
        //     I*w_dot + w x (I*w + L_r) = tau
        // A separate torque term would fork the gyroscopic expression in two,
        // and a rotor whose momentum PERSISTS must sit inside the product or a
        // steady yaw produces no roll at all.
        //
        // rep_belt, NOT s.belt_speed_ms: the state field is written once after
        // this loop, so reading it here would be a whole outer step stale.
        //
        // Direction: forward travel drives the top of the sprocket forward
        // (-Z), which in this right-handed frame (+X right, +Y up, +Z aft)
        // puts the rotor momentum along -X. DERIVED, then PINNED BY TEST --
        // sled.h rules that a sign is measured by test, never typed.
        const double w_rotor =
            p.drive_radius_m > 0.0 ? rep_belt / p.drive_radius_m : 0.0;
        // ★ THE PHYSICAL MOMENTUM, UNDIALLED. The rotor carries this whether
        // or not either effect is armed, so it is computed once, reported as
        // truth, and each dial scales only ITS OWN term. Folding k_gyro in
        // here would make the reaction depend on the precession dial -- one
        // knob judging two effects, which breaks the one-dial-at-a-time rule
        // and silently zeroed G2 on its first run.
        const double L_raw = -p.rotor_inertia_track_kgm2 * w_rotor;
        rep_rotor_L = L_raw;
        const glm::dvec3 L_r(p.k_gyro * L_raw, 0.0, 0.0);
        // k_gyro 0 => L_r is exactly the zero vector => Iw is bit-identical to
        // the pre-G1 expression and no golden can move.
        const glm::dvec3 Iw = I * s.angular_vel + L_r;
        const glm::dvec3 alpha_body =
            (torque_body - glm::cross(s.angular_vel, Iw)) / I;
        // ★ MERGE 2026-08-29: BOTH SIDES KEPT, and that IS the resolution.
        // R4a's sink write and the G1/G2 gyro block landed at the SAME insertion
        // point -- after alpha_body, before the rate integrates -- but they share
        // no state. R4a writes rec.a_body/rec.alpha_body into the WRITE-ONLY sink
        // and reads force/mass/R/alpha_body; the gyro block writes
        // dbg_roll[kRollGyro], s.angular_vel.x and s.gyro_prev_L. Neither reads
        // what the other writes, so there is no ordering dependency and no side
        // to pick. Both mechanisms are Chad-ruled; they are additive.
        // ★ R4a PHASE 0: the sink's SECOND write site. Two copies of locals
        // that already exist -- `force` (final: gravity and drag are in) over
        // `mass`, rotated into body axes, and the `alpha_body` one line above
        // -- appended to the record this substep already pushed. Nothing here
        // feeds back; it is the same write-only sink the R3 block above uses
        // and sled_debug_sink_is_write_only is the proof.
        const glm::dvec3 a_body_now = glm::transpose(R) * (force / mass);
        if (dbg && !dbg->substeps.empty()) {
            SledDebugSubstep& rec = dbg->substeps.back();
            const glm::dvec3& a_b = a_body_now;
            rec.a_body[0] = a_b.x;
            rec.a_body[1] = a_b.y;
            rec.a_body[2] = a_b.z;
            rec.alpha_body[0] = alpha_body.x;
            rec.alpha_body[1] = alpha_body.y;
            rec.alpha_body[2] = alpha_body.z;
        }

        // ★★★ R4a §7.7 STAGE 1 -- THE GRIP, STEPPED WHERE THE FIELD IS KNOWN.
        //
        // |g_eff| is the field the machine is actually pressing on him with:
        // TRUE gravity in body axes MINUS the body acceleration, which already
        // carries gravity and drag (`force` is final here). So in free fall it
        // is ZERO -- a weightless rider -- and at a landing it spikes. (Zero to
        // rounding: the two gravities are now the SAME constant, so they
        // cancel exactly rather than to 3 mm/s^2.) It is
        // the same construction `tools/sled_probe.cpp superman` measures the
        // whole calibration through, computed here from the kernel's own
        // locals rather than reconstructed from a tape.
        //
        // ⚠⚠ THIS BLOCK WAS WRITE-ONLY AT STAGE 1 AND IS NOT ANY MORE. Stage 2
        // wired the latch in: `s.grip.attached` is read at the top of this
        // substep as `hands_on` and spent on the throttle, the brake, the bars,
        // the whole weight-shift, `rider_frac`, `mu_exch` and the self-right.
        // The paragraph that used to stand here still claimed write-only and
        // still named `sled_the_grip_law_moves_no_golden`, a test that no
        // longer exists -- on the one paragraph a reader would use to decide
        // whether the corpus is safe. Red-team, 2026-09-01.
        //
        // ★ THE CORPUS IS STILL SAFE, AND FOR A DIFFERENT AND BETTER REASON:
        // every tape predates the grip dials entering `SLEDTAPE_PARAMS_D`, so
        // the loader forces `unseat_gain = 0`, the extension is then identically
        // zero and no capacity can be exceeded. That is what
        // `sled_the_tape_preset_still_moves_no_golden` pins, and Chad's three
        // goldens replay bit-exactly.
        {
            // ★ THE SAME GRAVITY THE FORCE ABOVE USED, and typed once. An
            // earlier draft wrote -9.81 here while `force` had been built with
            // -9.80665 twenty lines up (`force += -9.80665 * mass * up_cg`), so
            // the two did not cancel and this block's own comment -- "in free
            // fall it is ZERO, a weightless rider" -- was false by 0.00335
            // m/s^2. Harmless against a g0 of 9.81, and still a comment that
            // had stopped describing its code, which is this repo's most
            // expensive recurring defect. Referenced, never re-typed.
            const glm::dvec3 g_body_now =
                glm::transpose(R) * (up_cg * -9.80665);
            GripStep gi;
            gi.g_eff_mag = glm::length(g_body_now - a_body_now);
            gi.dt_s = h;
            grip_step(s.grip, p.grip, p.grip_buck, gi);
            // Into the write-only sink at FULL substep resolution, so a
            // capacity is never again read off a tick-level subsample.
            if (dbg && !dbg->substeps.empty()) {
                SledDebugSubstep& rec = dbg->substeps.back();
                rec.grip_load = s.grip.load;
                rec.grip_load_lp = s.grip.load_lp;
                rec.grip_extension_m = s.grip.extension_m;
            }
        }

        // ★ The roll-axis attribution must SEE this term or the instrument
        // becomes the fork: precession roll is applied outside torque_body and
        // would otherwise be invisible to the only tool that explains where a
        // roll came from.
        dbg_roll[kRollGyro] = -glm::cross(s.angular_vel, L_r).z;
        // ★ G2 THE REACTION WHEEL. d(L_r) about the lateral axis is a real
        // momentum the chassis must pay for: spin the track up and the machine
        // pitches. Applied to the RATE as a telescoping delta, exactly like the
        // rider exchange below, so a spin-up/spin-down cycle returns every
        // borrowed rad/s and only an ATTITUDE change survives.
        //
        // NOTE the honest gap: this kernel's belt is KINEMATIC, so it spins up
        // for free. The reaction charges the chassis for momentum whose energy
        // was never charged to the engine. Acceptable at this fidelity, and
        // recorded so nobody discovers it as a surprise.
        if (p.k_gyro_react > 0.0) {
            const double dL = L_raw - s.gyro_prev_L;
            s.angular_vel.x -= (p.k_gyro_react * dL) / I.x;
        }
        // Written on EVERY substep, armed or not, so arming the dial mid-run
        // cannot dump an accumulated delta in one tick.
        s.gyro_prev_L = L_raw;
        s.angular_vel += alpha_body * h;

        // ★★★ K-WS1 / K2 -- THE AIRBORNE MOMENTUM EXCHANGE, SPENT HERE.
        //
        // I*omega + L_exch is conserved about the system CG while no running
        // surface and no hull point is touching, so the chassis takes exactly
        // the NEGATIVE of the momentum the rider's body just spent:
        //     d(omega) = -k * I^-1 * (L_exch_now - L_exch_prev).
        // It is applied straight to the rate, not through the torque sum,
        // because it IS a momentum transfer and not a force -- routing it
        // through `torque_body` would hand it to the gyroscopic term as
        // though it were an external moment.
        //
        // I is the machine+rider inertia held constant under lean, the SAME
        // named ~16 % approximation this file already carries at the cg_off
        // block; inertia work is not this rung.
        //
        // Note there is no `else` and no decay: the delta TELESCOPES, so once
        // the rider stops moving L_exch is zero again and every borrowed
        // rad/s has been handed back. The net effect of a full throw is an
        // ATTITUDE change, never a rate.
        if (p.k_air_shift > 0.0 && !ground_contact) {
            const glm::dvec3 dl = exch_l_now - s.ws_exch_l;
            s.angular_vel -= (p.k_air_shift * dl) / I;
        }
        // Written on EVERY substep, in contact or not -- that is what makes
        // the first airborne delta zero instead of a takeoff pop.
        s.ws_exch_l = exch_l_now;
        const glm::dquat wq(0.0, s.angular_vel.x, s.angular_vel.y,
                            s.angular_vel.z);
        s.orientation = glm::normalize(s.orientation +
                                       0.5 * (s.orientation * wq) * h);

        // LAST-RESORT PENETRATION RAIL. The suspension is what holds the
        // machine up; this only catches a CG that has ended a substep BELOW the
        // snow surface entirely, which a stiff enough impact can do at any
        // finite substep count. It is asserted NEVER to fire in the goldens --
        // if it starts doing the work, the substep count is wrong, and the
        // measurement (not this clamp) is the fix.
        const glm::dvec3 up_after = glm::normalize(s.position);
        const double floor_r = snow.sample_at(up_after).drive_r;
        const double r_after = glm::length(s.position);
        if (r_after < floor_r) {
            s.position = up_after * floor_r;
            const double vr = glm::dot(s.velocity, up_after);
            if (vr < 0.0) s.velocity -= vr * up_after;
        }

        // RC item D bookkeeping: time since the machine last stood on (or
        // lay against) the ground — the rolled readout's air gate. Load on
        // any patch or hull point counts; so does CG PROXIMITY off the rail
        // sample above (no new query), because a machine lying on its side
        // with the hull dialed OFF carries no contact load at all and would
        // otherwise never latch ROLLED (red-team P1-5). 1.2x cg_height is
        // comfortably above any downed attitude and comfortably below the
        // shallowest send.
        if (normal_sum + side_normal_sum > 1.0 ||
            (r_after - floor_r) < 1.2 * p.cg_height_m)
            s.air_s = 0.0;
        else
            s.air_s += h;
    }

    s.plane_frac = rep_plane_num / std::max(rep_plane_den, kEps);
    s.track_slip = rep_slip;
    s.roost_flux = rep_flux;
    s.thrust_n = rep_thrust;
    s.depth_under_m = rep_depth;
    s.surface = rep_surf;
    s.belt_speed_ms = rep_belt;
    s.rotor_momentum_kgm2s = rep_rotor_L;
    s.engine_rpm = rep_rpm;
    const glm::dvec3 up_f = glm::normalize(s.position);
    s.ground_speed_ms =
        glm::length(s.velocity - glm::dot(s.velocity, up_f) * up_f);
    return s;
}

}  // namespace sim
