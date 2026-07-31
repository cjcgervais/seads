#include "config/load_controller.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace cfg {

namespace {

constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }

double require(const toml::table& root, const char* section, const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_number()) {
        throw std::runtime_error(std::string("controller.toml: missing or "
                                             "non-numeric key [") +
                                 section + "] " + key);
    }
    return node->value_or(0.0);
}

// Optional boolean key: absent => fallback (the S-relorient differential-
// firewall shape — an untouched toml stays bit-identical legacy). Present but
// non-boolean is still a hard error: a typo must fail loud, never default.
bool optional_bool(const toml::table& root, const char* section,
                   const char* key, bool fallback) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr) return fallback;
    if (!node->is_boolean()) {
        throw std::runtime_error(std::string("controller.toml: non-boolean "
                                             "key [") +
                                 section + "] " + key);
    }
    return node->value_or(fallback);
}

// Optional numeric key: absent => fallback (the same structural-off-switch
// shape as optional_bool). Present but non-numeric is still a hard error.
double optional_double(const toml::table& root, const char* section,
                       const char* key, double fallback) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr) return fallback;
    if (!node->is_number()) {
        throw std::runtime_error(std::string("controller.toml: non-numeric "
                                             "key [") +
                                 section + "] " + key);
    }
    return node->value_or(fallback);
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(
            std::string("controller.toml: invalid value: ") + what);
    }
}

}  // namespace

control::ControllerParams load_controller_toml(const std::string& path,
                                               const sim::AircraftParams& ap) {
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("controller.toml parse error: ") +
                                 e.what());
    }

    control::ControllerParams c;

    c.K_theta = require(root, "outer", "K_theta");
    c.K_phi = require(root, "outer", "K_phi");
    c.k_b = require(root, "outer", "k_b");

    c.yaw_max = rad(require(root, "rate_clamps", "yaw_max"));
    c.p_max = rad(require(root, "rate_clamps", "p_max"));
    c.omega_bal = rad(require(root, "rate_clamps", "omega_bal"));

    c.K_w_pitch = require(root, "inner", "K_w_pitch");
    c.K_w_yaw = require(root, "inner", "K_w_yaw");
    c.K_w_roll = require(root, "inner", "K_w_roll");
    c.K_wi_pitch = require(root, "inner", "K_wi_pitch");
    c.K_wi_yaw = require(root, "inner", "K_wi_yaw");
    c.K_wi_roll = require(root, "inner", "K_wi_roll");
    c.integ_cap = require(root, "inner", "integ_cap");
    c.damp_ff_pitch = require(root, "inner", "damp_ff_pitch");
    c.damp_ff_yaw = require(root, "inner", "damp_ff_yaw");
    c.damp_ff_roll = require(root, "inner", "damp_ff_roll");

    c.n_max = require(root, "g_limits", "n_max");
    c.n_min = require(root, "g_limits", "n_min");

    c.aoa_max = rad(require(root, "aoa", "aoa_max"));
    c.aoa_max_neg = rad(require(root, "aoa", "aoa_max_neg"));
    c.K_aoa = require(root, "aoa", "K_aoa");
    c.aoa_filter_tau = require(root, "aoa", "filter_tau");

    c.deadzone_lo = rad(require(root, "deadzone", "lo"));
    c.deadzone_hi = rad(require(root, "deadzone", "hi"));
    c.deadzone_rest_dwell = require(root, "deadzone", "rest_dwell");  // [s]

    c.coord_center_frac = require(root, "coordination", "center_frac");
    c.coord_center_band = rad(require(root, "coordination", "center_band"));

    c.pursuit_step = rad(require(root, "pursuit", "step"));  // deg/s -> rad/s
    c.pursuit_expo = require(root, "pursuit", "expo");       // 1/rad

    // S-aimff: gain is DIMENSIONLESS (rad/s of demand per rad/s of aim rate
    // — never rad() it); tau is seconds.
    c.aim_ff_gain = require(root, "aim_ff", "gain");
    c.aim_ff_tau = require(root, "aim_ff", "tau");

    // S-rimshot v2: carry, rim_frac, engage_frac, handback_frac, break_frac
    // are DIMENSIONLESS fractions (never rad() them); circle_deg is an
    // angle, return_w and w_eps rates.
    c.capture_carry = require(root, "capture", "carry");
    c.capture_rim_frac = require(root, "capture", "rim_frac");
    c.capture_circle = rad(require(root, "capture", "circle_deg"));
    c.capture_return_w = rad(require(root, "capture", "return_w"));
    c.capture_engage_frac = require(root, "capture", "engage_frac");
    c.capture_handback_frac = require(root, "capture", "handback_frac");
    c.capture_break_frac = require(root, "capture", "break_frac");
    c.capture_w_eps = rad(require(root, "capture", "w_eps"));
    // S-rimshot v3: glance_frac is a DIMENSIONLESS fraction of the circle.
    c.capture_glance_frac = require(root, "capture", "glance_frac");
    // Kernel v5 rung A (S-truedepth): depth_frac is a DIMENSIONLESS
    // multiplier on the momentum-earned glance (never rad() it).
    c.capture_depth_frac = require(root, "capture", "depth_frac");
    // Rung A2: dimensionless curve exponent (never rad() it).
    c.capture_depth_pow = require(root, "capture", "depth_pow");

    c.auto_level_rate = require(root, "auto_level", "rate");
    c.inverted_delay = require(root, "auto_level", "inverted_delay");
    c.inverted_rate = rad(require(root, "auto_level", "inverted_rate"));
    // MB-lean: lean_gain is DIMENSIONLESS (deg bank per deg lateral error ==
    // rad/rad) — never rad() it; lean_max is an angle.
    c.lean_gain = require(root, "auto_level", "lean_gain");
    c.lean_max = rad(require(root, "auto_level", "lean_max"));

    c.blend_lo = rad(require(root, "regime", "blend_lo"));
    c.blend_hi = rad(require(root, "regime", "blend_hi"));
    c.bank_align_power = require(root, "regime", "bank_align_power");
    c.wings_level_band =
        require(root, "regime", "wings_level_band");  // cos units
    // Kernel v5 rung C2b (S-holdline sag servo): pull_floor is a
    // DIMENSIONLESS fraction of the sag-servo demand, never rad().
    c.pull_floor = require(root, "regime", "pull_floor");
    // Blend-band roll target continuity: dimensionless mix weight, never
    // rad()'d.
    c.roll_target_mix = require(root, "regime", "roll_target_mix");

    c.push_gate_bank = rad(require(root, "push_gate", "bank_hi"));
    c.push_gate_bank_lo = rad(require(root, "push_gate", "bank_lo"));
    const double down_enter_deg = require(root, "push_gate", "down_enter");
    const double down_exit_deg = require(root, "push_gate", "down_exit");
    c.push_down_z_enter = -std::cos(rad(down_enter_deg));
    c.push_down_z_exit = -std::cos(rad(down_exit_deg));
    // World-horizon gate (§7 Item 2): degrees BELOW the horizon (positive =
    // below). aim world-elev = dot(aim, local_up) = -sin(deg-below). enter
    // deeper below than exit -> enter dot < exit dot (hysteresis around the
    // horizon).
    const double horizon_enter_deg =
        require(root, "push_gate", "horizon_enter");
    const double horizon_exit_deg = require(root, "push_gate", "horizon_exit");
    c.push_horizon_enter = -std::sin(rad(horizon_enter_deg));
    c.push_horizon_exit = -std::sin(rad(horizon_exit_deg));
    // Sideways cone (§7 Item 2 refinement): degrees the aim is OUT of the
    // vertical plane -> |sin| threshold. enter (narrower) < exit (wider).
    const double side_enter_deg = require(root, "push_gate", "side_cone_enter");
    const double side_exit_deg = require(root, "push_gate", "side_cone_exit");
    c.push_side_enter = std::sin(rad(side_enter_deg));
    c.push_side_exit = std::sin(rad(side_exit_deg));
    // Rung F "THE SACRED MIDDLE": a second, narrower in-plane cone gating an
    // OR-arm on the horizon leg (any depth below horizon, tightly in-plane).
    // Same |sin| convention as the side cone above.
    const double side_pure_enter_deg =
        require(root, "push_gate", "side_pure_enter");
    const double side_pure_exit_deg =
        require(root, "push_gate", "side_pure_exit");
    c.push_side_pure_enter = std::sin(rad(side_pure_enter_deg));
    c.push_side_pure_exit = std::sin(rad(side_pure_exit_deg));

    c.roll_latch_on = rad(require(root, "latches", "roll_on"));
    c.roll_latch_off = rad(require(root, "latches", "roll_off"));
    c.astern_on = rad(require(root, "latches", "astern_on"));
    c.astern_off = rad(require(root, "latches", "astern_off"));

    c.v_min = require(root, "floors", "v_min");
    c.v_ballistic = require(root, "floors", "v_ballistic");
    c.v_ballistic_exit = require(root, "floors", "v_ballistic_exit");

    c.K_coord = require(root, "coordination", "K_coord");
    c.yaw_scale = require(root, "coordination", "yaw_scale");
    c.yaw_align_band = require(root, "coordination", "yaw_align_band");
    c.yaw_min_frac = require(root, "coordination", "yaw_min_frac");

    c.ovr_ramp_time = require(root, "override", "ramp_time");

    c.freelook_easeback_time = require(root, "freelook", "easeback_time");
    c.orient_double_tap_s = require(root, "freelook", "orient_double_tap_s");
    // S-relorient: optional-with-default-false — absent key = legacy
    // bit-identical (the structural off-switch pattern; fixtures untouched).
    c.freelook_release_orient =
        optional_bool(root, "freelook", "release_orient", false);
    // S-relorient ADDENDUM (D9 retirement): same optional-with-default-false
    // pattern — absent key = the sealed-v6 kernel exactly.
    c.freelook_release_orient_with_keys =
        optional_bool(root, "freelook", "release_orient_with_keys", false);
    // S-globelook (v4 rung 3): globe-inertia dials — tau stays seconds, the
    // cap crosses the deg->rad boundary here (stored rad/s, the convention).
    c.freelook_inertia_tau = require(root, "freelook", "inertia_tau");
    c.freelook_inertia_cap = rad(require(root, "freelook", "inertia_cap"));
    // dwell stays SECONDS like tau (no rad conversion — it gates coast entry).
    c.freelook_inertia_dwell = require(root, "freelook", "inertia_dwell");

    c.horizon_recovery_rate = rad(require(root, "horizon_recovery", "rate"));
    c.horizon_recovery_settle = require(root, "horizon_recovery", "settle");

    c.aim_sensitivity = rad(require(root, "ui", "aim_sensitivity"));
    // MB-aim curve knobs: rates stay in device px/s (no rad conversion — they
    // key the GAIN, not an angle).
    c.aim_curve_knee = require(root, "ui", "aim_curve_knee");
    c.aim_curve_rate_hi = require(root, "ui", "aim_curve_rate_hi");
    c.aim_curve_gain_max = require(root, "ui", "aim_curve_gain_max");
    c.aim_curve_expo = require(root, "ui", "aim_curve_expo");
    c.aim_curve_quant_px = require(root, "ui", "aim_curve_quant_px");
    c.freelook_orbit_sensitivity =
        rad(require(root, "ui", "freelook_orbit_sensitivity"));
    c.freelook_orbit_yaw_max =
        rad(require(root, "ui", "freelook_orbit_yaw_max"));
    c.freelook_orbit_pitch_max =
        rad(require(root, "ui", "freelook_orbit_pitch_max"));

    c.cam_lead = require(root, "camera", "lead");
    c.cam_lag_base = require(root, "camera", "lag_base");
    c.cam_lag_gain = require(root, "camera", "lag_gain");
    // (S-keyprec / v8: [camera] key_anchor_rate is GONE. It was optional, so a
    // stale key left in a pilot's TOML is simply ignored, not an error.)

    // S-cues (comfort program): peripheral orientation cues, pure HUD, DEFAULT
    // OFF (alpha 0 skips the draw — strict superset). Read from [comfort].
    c.cue_horizon_alpha = require(root, "comfort", "cue_horizon_alpha");
    c.cue_horizon_gap_frac = require(root, "comfort", "cue_horizon_gap_frac");
    c.cue_bank_arc_alpha = require(root, "comfort", "cue_bank_arc_alpha");
    // S-carets (REC-2): screen-edge threat indicators, pure HUD, DEFAULT OFF.
    c.cue_caret_alpha = require(root, "comfort", "cue_caret_alpha");
    // REC-6: stylized cockpit frame (the steady-state rest frame), pure HUD.
    c.cue_cockpit_alpha = require(root, "comfort", "cue_cockpit_alpha");

    // ---- Internal sanity ------------------------------------------------
    check(c.K_theta > 0.0 && c.K_phi > 0.0, "K_theta, K_phi > 0");
    check(c.k_b > 0.0 && c.k_b <= 1.0, "k_b in (0, 1]");
    check(c.yaw_max > 0.0 && c.p_max > 0.0 && c.omega_bal > 0.0,
          "rate clamps > 0");
    check(c.K_w_pitch > 0.0 && c.K_w_yaw > 0.0 && c.K_w_roll > 0.0, "K_w > 0");
    check(c.K_wi_pitch >= 0.0 && c.K_wi_yaw >= 0.0 && c.K_wi_roll >= 0.0,
          "K_wi >= 0");
    check(c.integ_cap > 0.0, "integ_cap > 0");
    // S-dampff: the damping feedforward fraction lives in [0, 1]. 0 = the
    // term is structurally OFF (bit-identical legacy); 1 = full inversion of
    // the plant's damping torque at the demand. ABOVE 1 the wall is
    // load-bearing for a subtle reason: the rate-feedback (eo) coefficient is
    // damp_ff-INDEPENDENT (tau = (K_w + damp*Q)*eo + (damp_ff-1)*damp*Q*w_des
    // + K_wi*integ), so >1 is NOT "negative damping" — it makes the w_des DC
    // gain exceed 1, and the integrator winds NEGATIVE to cancel the excess:
    // the mirrored image of the carry-past defect the mechanism removes.
    check(c.damp_ff_pitch >= 0.0 && c.damp_ff_pitch <= 1.0 &&
              c.damp_ff_yaw >= 0.0 && c.damp_ff_yaw <= 1.0 &&
              c.damp_ff_roll >= 0.0 && c.damp_ff_roll <= 1.0,
          "damp_ff_* in [0, 1] (0 = off, 1 = full inversion)");
    // n_min strictly below -1: w_min_pitch = (n_min - cosPhiTheta)*g/V is the
    // negative-G pitch budget AND the denominator of the push-vs-roll ratio
    // (controller.cpp). cosPhiTheta ranges [-1, 1], so only n_min < -1
    // guarantees w_min_pitch < 0 at EVERY attitude (incl. fully inverted,
    // cosPhiTheta = -1). At n_min in (-1, 0] a reachable bank drives the
    // budget through zero: the ratio divides by zero, and past the crossing
    // the "floor" flips sign and forces a pull while inverted.
    check(c.n_max > 1.0 && c.n_min < -1.0, "n_max > 1 and n_min < -1");
    check(c.aoa_max > 0.0 && c.aoa_max_neg > 0.0, "aoa limits > 0");
    check(c.K_aoa > 0.0 && c.aoa_filter_tau > 0.0, "K_aoa, filter_tau > 0");
    check(c.deadzone_hi > c.deadzone_lo && c.deadzone_lo > 0.0,
          "0 < deadzone_lo < deadzone_hi (hysteresis)");
    // Aim-motion gate: 0 = legacy off; negative is nonsense. Past ~2 s the
    // deadzone would effectively never latch in flight (any hand tremor
    // resets the window) and "Inputs at rest = trim" silently disarms.
    // BELOW 0.1 s the gate breaks by FRAME RATE (red-team P2-1): the fixed-dt
    // accumulator manufactures (N-1) at-rest ticks per frame, so the deadzone
    // relatches mid-drag once (ticks_per_frame - 1)*sim_dt >= rest_dwell —
    // 0.05 s puts the break at ~20 fps, loaded-laptop territory. The stairs
    // would return with the gate "on".
    check(c.deadzone_rest_dwell == 0.0 ||
              (c.deadzone_rest_dwell >= 0.1 && c.deadzone_rest_dwell <= 2.0),
          "deadzone rest_dwell = 0 (legacy) or in [0.1, 2] s (fps floor)");
    // S-yaw-magnet: frac in [0,1] (1 = off; negative would REVERSE the
    // coordination near center — the sign-flip class). When armed the band
    // must be strictly positive (smoothstep's divisor) and must sit WELL
    // inside blend_lo: past the FINE region the relief would leak into
    // bank-to-turn territory where AT-16's skid wall lives.
    check(c.coord_center_frac >= 0.0 && c.coord_center_frac <= 1.0,
          "0 <= coordination center_frac <= 1 (1 = relief off)");
    check(
        c.coord_center_frac == 1.0 || (c.coord_center_band > 0.0 &&
                                       c.coord_center_band <= 0.5 * c.blend_lo),
        "armed coordination relief needs 0 < center_band <= blend_lo/2");
    check(c.pursuit_step >= 0.0 && c.pursuit_expo >= 0.0,
          "pursuit_step, pursuit_expo >= 0 (0 = plain sqrt_law)");
    // S-aimff: gain in [0, 2]. 0 = structurally OFF; negative would ANTI-lead
    // the moving aim (the sign-flip class); the 2 wall is the research bound
    // (PN framing N <= 2 — a lead demand past twice the aim's own rate is no
    // longer proportional navigation, it is an overshoot generator). tau >= 0
    // (0 = unfiltered pass-through of the already-smeared rate).
    check(c.aim_ff_gain >= 0.0 && c.aim_ff_gain <= 2.0,
          "aim_ff gain in [0, 2] (0 = off)");
    check(c.aim_ff_tau >= 0.0, "aim_ff tau >= 0 (0 = pass-through)");
    // S-rimshot: carry in [0, 1.5]. 0 = structurally OFF (the machine never
    // arms); negative would REVERSE the carry (sign-flip class); past ~1.5 the
    // held demand exceeds the rate the nose actually brought in — an overshoot
    // generator, not a carry (the aim_ff N <= 2 reasoning, tighter because
    // carry acts at the capture point itself).
    check(c.capture_carry >= 0.0 && c.capture_carry <= 1.5,
          "capture carry in [0, 1.5] (0 = off)");
    // When live (carry > 0), every other dial must be live: rim in (0, 1]
    // ("nearly touches" — past 1 the rim detect sits outside the circle it
    // is scaled to); circle/return_w > 0; engage_frac in (0, 1] (1 = the
    // rung-2 onset "pointed < w_rel"; past 1 the trigger fires while the law
    // still OUT-demands the rate — pre-knee, not an arrival; at/below the
    // closed-loop decay-ratio floor ~0.86 pitch / 0.69 yaw it is stillborn —
    // the ratio ASYMPTOTES there); handback_frac > 1 (at/below 1 the
    // hand-back fires on the engage tick itself — a stillborn event);
    // break_frac > 1 (at/below 1 the yank exit kills the event at its own
    // honest apex — the exact class it replaced); w_eps > 0 (a 0 floor
    // noise-engages on the coordination demand-vs-achieved residual
    // ~0.27 deg/s and crawls the nose for seconds).
    check(c.capture_carry == 0.0 ||
              (c.capture_rim_frac > 0.0 && c.capture_rim_frac <= 1.0 &&
               c.capture_circle > 0.0 && c.capture_return_w > 0.0 &&
               c.capture_engage_frac > 0.0 && c.capture_engage_frac <= 1.0 &&
               c.capture_handback_frac > 1.0 &&
               c.capture_handback_frac <= 2.0 && c.capture_break_frac > 1.0 &&
               c.capture_break_frac <= 3.0 && c.capture_w_eps > 0.0 &&
               c.capture_glance_frac > 0.0 && c.capture_glance_frac <= 1.0),
          "live capture needs rim_frac in (0,1], circle_deg > 0, "
          "return_w > 0, engage_frac in (0,1], handback_frac in (1,2], "
          "break_frac in (1,3], w_eps > 0, glance_frac in (0,1]");
    check(c.capture_carry == 0.0 ||
              c.capture_rim_frac * c.capture_circle > c.deadzone_hi,
          "capture rim (rim_frac*circle_deg) must sit outside the deadzone "
          "re-arm circle (hi) or the event is invisible");
    // Kernel v5 rung A (S-truedepth): depth_frac has no range wall (<=0 is
    // the v4 fixed-rim OFF arm, >1 a deeper-per-momentum glance, still
    // wall-capped downstream) — only finiteness is asserted.
    check(std::isfinite(c.capture_depth_pow) && c.capture_depth_pow > 0.0,
          "capture depth_pow must be finite and > 0 (1.0 = the rung-A "
          "knob-off arm; pow <= 0 would weld every sub-wall event to the "
          "full wall through s^0 = 1 � the exact symptom the dial exists "
          "to remove)");
    check(std::isfinite(c.capture_depth_frac),
          "capture depth_frac must be finite");
    // (No break_frac*rim_frac product tripwire: the re-flick exits do not
    // set the refractory latch at all — there is no stranded-latch dead
    // window for a product bound to guard. The latch belongs solely to the
    // completion exits, whose clear scale is the stored exit err.)
    check(c.auto_level_rate >= 0.0, "auto_level_rate >= 0 (0 disables)");
    // MB-right: delay must span at least one tick (a 0 delay would fire the
    // righting the instant the mouse rests inverted — the S7-loop-invert
    // "stays inverted" window would silently vanish); rate >= 0, 0 = off.
    check(c.inverted_delay >= ap.sim_dt, "inverted_delay >= sim_dt");
    check(c.inverted_rate >= 0.0, "inverted_rate >= 0 (0 disables)");
    // MB dial tripwire (the "retune silently disarms" class): the righting
    // roll must stay the SLOW arm of the roll authority — past p_max the
    // clamp goes inert and "a lazy/committed roll" silently becomes the
    // p_max slam the mechanism exists to avoid. 180 <= 260 today.
    check(c.inverted_rate <= c.p_max,
          "inverted_rate <= p_max (the righting roll stays the slow arm)");
    // MB-lean bounds: gain >= 0 (0 = decay to level, the knob-off arm);
    // the cap stays a SHALLOW bank — 45 deg is already the bank-to-turn's
    // territory, past it the lean stops being "fine tracking".
    check(c.lean_gain >= 0.0, "lean_gain >= 0 (0 = decay to level)");
    check(c.lean_max >= 0.0 && c.lean_max <= rad(45.0),
          "lean_max in [0, 45] deg (a lean is a SHALLOW bank)");
    // Stability tripwire (MB-lean diff red-team P2-1, the "retune silently
    // disarms" class): the lean loop's crossover ~ lean_gain*g/V must stay
    // under the auto_level decay pole at the SLOWEST cascade speed (the
    // ballistic exit floor), or a lean_gain retune erodes the phase margin
    // where V is low with nothing tripping loud. Shipped: 8*9.81/40 = 1.96
    // < 3.0; trips at lean_gain ~12 (the plan red-team's band-edge estimate
    // was ~11.5).
    check(c.lean_gain * ap.g / c.v_ballistic_exit <= c.auto_level_rate,
          "lean_gain*g/v_ballistic_exit <= auto_level_rate (lean loop "
          "crossover must stay under the decay pole at low V)");
    check(c.blend_hi > c.blend_lo && c.blend_lo > 0.0,
          "0 < blend_lo < blend_hi (hysteresis)");
    check(c.bank_align_power >= 1.0, "bank_align_power >= 1");
    check(c.roll_target_mix >= 0.0 && c.roll_target_mix <= 1.0,
          "0 <= roll_target_mix <= 1 (0 = legacy two-target roll blend)");
    // Kernel v5 rung C2b: pull_floor in [0,1] (0 disarms the sag servo
    // entirely; above 1 would let the servo demand more than the full
    // K_theta line-hold, above the spec).
    check(c.pull_floor >= 0.0 && c.pull_floor <= 1.0,
          "0 <= pull_floor <= 1 (0 disarms the S-holdline sag servo)");
    check(c.wings_level_band > 0.0 && c.wings_level_band < 1.0,
          "0 < wings_level_band < 1 (fade band around the 90 deg knife-edge)");
    check(c.push_gate_bank > c.push_gate_bank_lo && c.push_gate_bank_lo > 0.0,
          "push bank hysteresis");
    // Angle range guards the z-monotonicity the hysteresis relies on (z =
    // -cos(angle) is monotone only on [0,180]); down_exit > down_enter gives
    // the hysteresis band (S7-push).
    check(down_enter_deg > 0.0 && down_enter_deg < down_exit_deg &&
              down_exit_deg < 180.0,
          "0 < down_enter < down_exit < 180 (push geometry hysteresis)");
    // enter deeper below the horizon than exit: enter dot < exit dot (§7 Item
    // 2)
    check(c.push_horizon_enter < c.push_horizon_exit,
          "horizon_enter deeper-below than horizon_exit (push horizon "
          "hysteresis)");
    check(c.push_side_enter < c.push_side_exit && c.push_side_enter > 0.0,
          "0 < side_cone_enter < side_cone_exit (push sideways hysteresis)");
    check(c.push_side_pure_enter < c.push_side_pure_exit &&
              c.push_side_pure_enter > 0.0,
          "0 < side_pure_enter < side_pure_exit (sacred-middle hysteresis)");
    check(c.push_side_pure_enter < c.push_side_enter,
          "side_pure_enter < side_cone_enter (the sacred middle is a "
          "narrower subset of the wider side cone)");
    check(c.roll_latch_on > c.roll_latch_off && c.roll_latch_off > 0.0,
          "roll latch hysteresis");
    check(c.astern_on > c.astern_off && c.astern_off > 0.0,
          "astern latch hysteresis");
    check(c.v_min > 0.0 && c.v_ballistic > 0.0 &&
              c.v_ballistic_exit > c.v_ballistic,
          "0 < v_ballistic < v_ballistic_exit; v_min > 0");
    check(c.K_coord >= 0.0 && c.yaw_scale >= 0.0, "K_coord, yaw_scale >= 0");
    check(c.yaw_min_frac >= 0.0 && c.yaw_min_frac <= 1.0,
          "yaw_min_frac in [0, 1]");
    // Strictly positive: smoothstep(-band, 0, cos) needs lo < hi, and band = 0
    // would reintroduce a bare knife-edge toggle (every gate hysteretic /
    // continuous — CLAUDE.md). Capped at 1: past band = 1 the gate FLOOR is
    // unreachable (cos >= -1 > -band, smoothstep never reaches 0) and the
    // split-S corkscrew guard silently softens — the AT-15 "retune silently
    // disarms the guard" class; the toml comment invites widening, so the
    // tripwire is load-time (diff red-team P3-1).
    check(c.yaw_align_band > 0.0 && c.yaw_align_band <= 1.0,
          "yaw_align_band in (0, 1]");
    // The engage ramp must span at least one tick, or "ramped" is a hard step
    // (dt/ramp_time >= 1 reaches full on the first held tick — the twitch the
    // ramp exists to remove, SPEC §9.5).
    check(c.ovr_ramp_time >= ap.sim_dt, "ovr_ramp_time >= sim_dt (SPEC §9.5)");
    // Ease-back must be a real suspension window (> 0 — CQ2 mandates it) and no
    // longer than 300 ms (CQ2's "<=300 ms"): past that the held aim would feel
    // frozen long after the pilot came back to the stick.
    check(c.freelook_easeback_time > 0.0 && c.freelook_easeback_time <= 0.300,
          "0 < freelook_easeback_time <= 0.30 (SPEC §16 CQ2)");
    // Orient double-tap window: 0 disables the ORIENT verb structurally; when
    // enabled it must be a real, human-tappable window (a negative would be a
    // typo, an absurdly large one would fire on any two spaced taps).
    check(c.orient_double_tap_s >= 0.0 && c.orient_double_tap_s <= 1.0,
          "0 <= orient_double_tap_s <= 1.0 (0 disables S-orient)");
    // S-globelook: tau = 0 disables structurally; when the mechanism is on the
    // cap must be a real ceiling (> 0), or the seeded coast velocity clamps to
    // zero and the "on" dial silently disarms (the AT-15 class).
    check(c.freelook_inertia_tau >= 0.0,
          "freelook inertia_tau >= 0 (0 disables S-globelook)");
    check(c.freelook_inertia_tau == 0.0 || c.freelook_inertia_cap > 0.0,
          "freelook inertia_cap > 0 when inertia_tau > 0");
    // dwell >= 0: 0 is the legal coast-immediately arm (it re-opens the
    // staircase amplification — documented in the table); negative is a typo.
    check(c.freelook_inertia_dwell >= 0.0,
          "freelook inertia_dwell >= 0 (0 = coast-immediately)");
    // Horizon recovery: rate = 0 disables; when enabled the settle rate must be
    // a real terminal ease (> 0) or the D3 profile divides its shape away.
    check(c.horizon_recovery_rate >= 0.0,
          "horizon_recovery rate >= 0 (0 disables)");
    check(c.horizon_recovery_rate == 0.0 || c.horizon_recovery_settle > 0.0,
          "horizon_recovery settle > 0 when rate > 0");
    check(c.aim_sensitivity > 0.0 && c.freelook_orbit_sensitivity > 0.0,
          "mouse sensitivities > 0");
    // MB-aim curve: knee < rate_hi keeps the ramp's denominator positive;
    // gain_max >= 1 (exactly 1 = the structural OFF arm); expo > 0 (a 0 expo
    // would step the gain AT the knee — the curve must be continuous).
    check(c.aim_curve_knee >= 0.0 && c.aim_curve_rate_hi > c.aim_curve_knee,
          "aim curve: 0 <= knee < rate_hi");
    check(c.aim_curve_gain_max >= 1.0, "aim_curve_gain_max >= 1 (1 = off)");
    check(c.aim_curve_expo > 0.0, "aim_curve_expo > 0");
    // quant_px is a LEGALITY guard, not a feel knob (diff red-team P2): it is
    // the ONLY protection keeping fps-scaled 1-3 px integer-delta rate noise
    // from accelerating inside the precision zone (the knee does not cover
    // it). With the curve ON it must be a real guard; 0 is legal only with
    // the curve OFF — the AT-15 "retune silently disarms the rail" class.
    check(c.aim_curve_gain_max <= 1.0 || c.aim_curve_quant_px > 0.0,
          "aim_curve_quant_px > 0 while the curve is on (fps-noise guard)");
    // Freelook orbit range: yaw full circle to the front (<= pi), pitch stops
    // at the straight-up/down pole (<= pi/2). The +1e-9 dodges a 1-ulp false
    // trip from rad(180)/rad(90) not landing bit-exact on kPi / kPi*0.5.
    // S-freelook360: yaw_max 0 = the UNLIMITED orbit (wraps each frame, no
    // swing stop — Chad 2026-07-17); > 0 = the legacy clamped swing.
    check(c.freelook_orbit_yaw_max >= 0.0 &&
              c.freelook_orbit_yaw_max <= kPi + 1e-9 &&
              c.freelook_orbit_pitch_max > 0.0 &&
              c.freelook_orbit_pitch_max <= 0.5 * kPi + 1e-9,
          "freelook orbit max: yaw in [0, pi] (0 = unlimited), pitch in "
          "(0, pi/2]");
    check(c.cam_lead >= 0.0 && c.cam_lead <= 1.0, "cam lead in [0, 1]");
    check(c.cam_lag_base > 0.0 && c.cam_lag_gain >= 0.0,
          "cam_lag_base > 0, cam_lag_gain >= 0");
    // S-cues: alphas in [0,1] (0 = OFF, the structural default); gap frac in
    // [0,1) (a full-screen gap would erase the cue).
    check(c.cue_horizon_alpha >= 0.0 && c.cue_horizon_alpha <= 1.0,
          "cue_horizon_alpha in [0, 1]");
    check(c.cue_bank_arc_alpha >= 0.0 && c.cue_bank_arc_alpha <= 1.0,
          "cue_bank_arc_alpha in [0, 1]");
    check(c.cue_horizon_gap_frac >= 0.0 && c.cue_horizon_gap_frac < 1.0,
          "cue_horizon_gap_frac in [0, 1)");
    check(c.cue_caret_alpha >= 0.0 && c.cue_caret_alpha <= 1.0,
          "cue_caret_alpha in [0, 1]");
    check(c.cue_cockpit_alpha >= 0.0 && c.cue_cockpit_alpha <= 1.0,
          "cue_cockpit_alpha in [0, 1]");

    // ---- Cross-checks against the airframe (the load-bearing ones) ------
    // AoA protection must guard the SIM's stall (SPEC §9.3b): the plant caps
    // Cl at Cl_max = Cl_alpha * alpha_stall, so the stall AoA is Cl_max /
    // Cl_alpha. A limiter set beyond it protects nothing.
    const double stall_alpha = ap.Cl_max / ap.Cl_alpha;
    check(c.aoa_max <= stall_alpha,
          "aoa_max <= plant stall alpha (Cl_max / Cl_alpha)");
    check(c.aoa_max_neg <= stall_alpha, "aoa_max_neg <= plant stall alpha");

    // Critical-damping rule and the discrete ZOH ceiling (SPEC §9.4). Both
    // are load-bearing: violate the first and the linear response overshoots;
    // violate the second and rate feedback under zero-order hold destabilizes
    // (raising K_w past it CAUSES the oscillation it normally cures).
    const struct {
        const char* name;
        double K_w, K_theta, I;
    } axes[] = {
        {"pitch", c.K_w_pitch, c.K_theta, ap.I_pitch},
        {"yaw", c.K_w_yaw, c.K_theta, ap.I_yaw},
        {"roll", c.K_w_roll, c.K_phi, ap.I_roll},
    };
    for (const auto& a : axes) {
        check(a.K_w >= 4.0 * a.I * a.K_theta,
              "K_w >= 4*I*K_theta (critical-damping rule, SPEC §9.4)");
        check(a.K_w * ap.sim_dt / a.I <= 0.5,
              "K_w*sim_dt/I <= 0.5 (ZOH ceiling, SPEC §9.4)");
    }

    return c;
}

}  // namespace cfg
