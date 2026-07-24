#include "config/load_aircraft.h"

#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace cfg {

namespace {

// Every key required, every value a number — a missing coefficient must
// fail the load, never default.
double require(const toml::table& root, const char* section, const char* key) {
    const toml::node* node =
        root.at_path(std::string(section) + "." + key).node();
    if (node == nullptr || !node->is_number()) {
        throw std::runtime_error(std::string("aircraft.toml: missing or "
                                             "non-numeric key [") +
                                 section + "] " + key);
    }
    return node->value_or(0.0);
}

void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(std::string("aircraft.toml: invalid value: ") +
                                 what);
    }
}

}  // namespace

sim::AircraftParams load_aircraft_toml(const std::string& path) {
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("aircraft.toml parse error: ") +
                                 e.what());
    }

    sim::AircraftParams p;
    p.R = require(root, "world", "R");
    p.g = require(root, "world", "g");
    p.rho = require(root, "world", "rho");
    p.atm_taper_alt = require(root, "atmosphere", "taper_alt");
    p.atm_taper_sigma = require(root, "atmosphere", "taper_sigma");
    p.sim_dt = require(root, "world", "sim_dt");

    p.mass = require(root, "airframe", "mass");
    p.S = require(root, "airframe", "S");
    p.Cl_max = require(root, "airframe", "Cl_max");
    p.Cl_alpha = require(root, "airframe", "Cl_alpha");
    p.Cd0 = require(root, "airframe", "Cd0");
    p.k_induced = require(root, "airframe", "k_induced");
    p.T_max = require(root, "airframe", "T_max");
    p.throttle_slew_rate = require(root, "airframe", "throttle_slew_rate");

    p.I_pitch = require(root, "inertia", "I_pitch");
    p.I_yaw = require(root, "inertia", "I_yaw");
    p.I_roll = require(root, "inertia", "I_roll");

    p.c_pitch = require(root, "authority", "c_pitch");
    p.c_yaw = require(root, "authority", "c_yaw");
    p.c_roll = require(root, "authority", "c_roll");
    p.q_att_floor = require(root, "authority", "q_att_floor");
    p.damp_pitch = require(root, "authority", "damp_pitch");
    p.damp_yaw = require(root, "authority", "damp_yaw");
    p.damp_roll = require(root, "authority", "damp_roll");

    p.v_full = require(root, "compression", "v_full");
    p.v_redline = require(root, "compression", "v_redline");
    p.min_frac = require(root, "compression", "min_frac");

    p.dCl_flap = require(root, "flaps", "dCl_flap");
    p.dCd0_flap = require(root, "flaps", "dCd0_flap");
    p.flap_slew = require(root, "flaps", "slew");
    p.flap_combat = require(root, "flaps", "combat_frac");
    p.flap_wash_lo = require(root, "flaps", "wash_lo");
    p.flap_wash_hi = require(root, "flaps", "wash_hi");
    p.dCd0_gear = require(root, "gear", "dCd0_gear");
    p.gear_slew = require(root, "gear", "slew");

    p.Cy_beta = require(root, "airframe", "Cy_beta");
    p.Cd_beta = require(root, "airframe", "Cd_beta");

    p.v_dir_eps = require(root, "limits", "v_dir_eps");

    check(p.R > 0.0, "R > 0");
    check(p.g > 0.0, "g > 0");
    check(p.rho > 0.0, "rho > 0");
    check(p.sim_dt > 0.0, "sim_dt > 0");
    check(p.mass > 0.0, "mass > 0");
    check(p.S >= 0.0, "S >= 0");
    check(p.Cl_max > 0.0, "Cl_max > 0");
    check(p.Cl_alpha > 0.0, "Cl_alpha > 0");
    check(p.Cd0 >= 0.0, "Cd0 >= 0");
    check(p.k_induced >= 0.0, "k_induced >= 0");
    check(p.T_max >= 0.0, "T_max >= 0");
    // MB-atm: taper_alt >= 0; sigma >= 0 (0 = the taper structurally OFF —
    // atm_frac == 1 everywhere, the knob-off strict-superset arm).
    check(p.atm_taper_alt >= 0.0, "atmosphere taper_alt >= 0");
    // 0 = off; else >= 100 m — a meters-wide Gaussian is structurally the
    // hard wall the soft-ceiling design forbids (red-team P3-2).
    check(p.atm_taper_sigma == 0.0 || p.atm_taper_sigma >= 100.0,
          "atmosphere taper_sigma == 0 (off) or >= 100 m (soft, not a wall)");
    check(p.throttle_slew_rate > 0.0, "throttle_slew_rate > 0");
    check(p.I_pitch > 0.0 && p.I_yaw > 0.0 && p.I_roll > 0.0, "inertias > 0");
    // Strictly positive, not just non-negative: the controller inverts the
    // plant torque (control::plant_invert) with denom = c * q_eff * delta_max,
    // and q_eff = max(q, q_att_floor). A zero c_axis or a zero q_att_floor
    // makes that denominator 0 at (respectively any speed / v -> 0), so the
    // clamp(tau/0) emits NaN into the plant — controller.h promises "denom > 0
    // always", and THIS is where that promise is kept. (An airframe with no
    // authority on an axis is unflyable regardless.)
    check(p.c_pitch > 0.0 && p.c_yaw > 0.0 && p.c_roll > 0.0, "c_axis > 0");
    check(p.q_att_floor > 0.0, "q_att_floor > 0 (the §9.6 authority floor)");
    check(p.damp_pitch >= 0.0 && p.damp_yaw >= 0.0 && p.damp_roll >= 0.0,
          "damp_axis >= 0");
    check(p.v_full > 0.0 && p.v_redline > p.v_full, "0 < v_full < v_redline");
    check(p.min_frac > 0.0 && p.min_frac <= 1.0, "min_frac in (0, 1]");
    check(p.v_dir_eps > 0.0, "v_dir_eps > 0");
    // MB-flaps: coefficients non-negative; slews strictly positive (a 0 slew
    // freezes the device mid-travel forever — the throttle_slew precedent);
    // the combat detent strictly inside (0,1) so the three positions are
    // distinct; the lift-washout band ordered (lo < hi keeps the smoothstep
    // denominator positive — lo == hi would be the bare-step gate the
    // continuity rule forbids).
    check(p.dCl_flap >= 0.0 && p.dCd0_flap >= 0.0 && p.dCd0_gear >= 0.0,
          "flap/gear coefficients >= 0");
    check(p.flap_slew > 0.0 && p.gear_slew > 0.0, "flap/gear slew > 0");
    check(p.flap_combat > 0.0 && p.flap_combat < 1.0, "combat_frac in (0,1)");
    check(p.flap_wash_lo > 0.0 && p.flap_wash_hi > p.flap_wash_lo,
          "0 < flaps wash_lo < wash_hi");
    // S-wvane: negative Cy_beta is PRO-crab positive feedback (the side force
    // would grow the sideslip it exists to bleed — the S8 wrong-sign class);
    // negative Cd_beta is a sideslip thrust. 0 = structurally off, both.
    check(p.Cy_beta >= 0.0, "Cy_beta >= 0 (negative = pro-crab feedback)");
    check(p.Cd_beta >= 0.0, "Cd_beta >= 0");
    // Per-tick stability ceiling (diff red-team P1-1 — the ZOH discipline
    // applied to the plant's lateral bleed): the discrete decay
    // beta' = beta*(1 - dt/tau) diverges past dt/tau = 1; wall at the 0.5
    // margin, evaluated at redline where tau is smallest. ~92 at the shipped
    // table — far above any felt value, but a "snappier!" crank now fails
    // the LOAD instead of ringing at V_redline (the S8 tripwire precedent).
    check(p.Cy_beta <= p.mass / (p.rho * p.v_redline * p.S * p.sim_dt),
          "Cy_beta under the per-tick lateral-bleed stability ceiling");

    return p;
}

}  // namespace cfg
