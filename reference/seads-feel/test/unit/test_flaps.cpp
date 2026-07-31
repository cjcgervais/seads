// MB-flaps (SPEC §0): combat + landing flaps and landing gear — FORCE-ONLY
// plant devices (lift-curve shift + parasitic drag taxes; the torque model,
// controller inversion, and AT-18 never see them). The legs follow the
// MB-atm blindness discipline: for every term the mechanism scales, name the
// instrument structurally blind to it and give that term its own non-vacuous
// leg — the flap LIFT is invisible to the energy gate (lift.v == 0 by
// construction) so it gets the alpha = 0 isolation leg (where the BASE Cl is
// zero and the flap shift is the ONLY lift — the exact fixture that made the
// MB-atm lift leg vacuous makes THIS one surgical, because the flap shift is
// alpha-independent); the flap/gear DRAG is invisible to a clean-airframe
// AT-12 (terms identically 0) so it gets its own DEPLOYED energy
// fork-detector with the composition DUPLICATED from step.cpp (share only
// the aero.h primitives — factoring it would zero the residual under every
// mutation, the AT-12 lesson).

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/aero.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_flaps_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

// A level state at a GENERIC point (no world axis aligned with local_up —
// the S8-drone "fixed-axis hardcode coincides with +Y" fixture lesson).
sim::SimState level_at(double V, double alt) {
    const glm::dvec3 up = glm::normalize(glm::dvec3{1.0, 1.0, 1.0});
    const glm::dvec3 heading =
        glm::normalize(glm::cross(up, glm::dvec3{0.0, 0.0, 1.0}));
    return harness::level_state(kAp, V, alt, up, heading);
}

}  // namespace

TEST_CASE("flaps: the plant slews toward the command (deploy is physics)") {
    sim::SimState s = level_at(120.0, 2000.0);
    REQUIRE(s.flap == 0.0);
    REQUIRE(s.gear == 0.0);
    sim::Inputs in;
    in.throttle = 0.5f;
    in.flap_cmd = 1.0f;
    in.gear_cmd = 1.0f;
    // One tick moves exactly slew*dt (boundary-exact, the S4a discipline).
    const sim::SimState s1 = sim::step(s, in, kAp, kAp.sim_dt);
    CHECK(s1.flap == kAp.flap_slew * kAp.sim_dt);
    CHECK(s1.gear == kAp.gear_slew * kAp.sim_dt);
    // Reaches the target and HOLDS (no overshoot past the clamp).
    sim::SimState cur = s;
    const int deploy_ticks =
        static_cast<int>(std::ceil(1.0 / (kAp.flap_slew * kAp.sim_dt))) + 2;
    for (int i = 0; i < deploy_ticks; ++i)
        cur = sim::step(cur, in, kAp, kAp.sim_dt);
    CHECK(cur.flap == 1.0);
    // Retract: command 0 walks it back down.
    in.flap_cmd = 0.0f;
    cur = sim::step(cur, in, kAp, kAp.sim_dt);
    CHECK(cur.flap == 1.0 - kAp.flap_slew * kAp.sim_dt);
    // A clean airframe at 0 command stays EXACTLY 0 (the bit-identity arm's
    // slew leg: clamp(0-0) == 0).
    const sim::SimState c1 = sim::step(s, sim::Inputs{}, kAp, kAp.sim_dt);
    CHECK(c1.flap == 0.0);
    CHECK(c1.gear == 0.0);
}

TEST_CASE(
    "flaps: lift shift isolated at alpha = 0 (the term AT-12 cannot "
    "see)") {
    // Velocity exactly along the nose => base Cl = 0; thrust off; the ONLY
    // non-gravity force along body-up is the flap's dCl shift. Below the
    // washout band, so the shift is full.
    sim::SimState s = level_at(100.0, 2000.0);
    s.throttle = 0.0;
    REQUIRE(100.0 < kAp.flap_wash_lo);
    const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};

    sim::Inputs in;  // throttle 0
    // DIFFERENTIAL from the identical start: flapped minus clean. Gravity,
    // base lift (0 at alpha = 0), and drag (along -vhat, perpendicular to
    // body_up here) all cancel — the residual body-up acceleration IS the
    // flap's dCl shift, exactly.
    const sim::SimState c1 = sim::step(s, in, kAp, kAp.sim_dt);
    // Deployed (state.flap set directly — the slewed position, not the cmd;
    // hold it with a matching command so the slew doesn't walk it).
    sim::SimState sf = s;
    sf.flap = 1.0;
    in.flap_cmd = 1.0f;
    const sim::SimState f1 = sim::step(sf, in, kAp, kAp.sim_dt);
    const double dv_up_delta =
        glm::dot(f1.velocity - c1.velocity, body_up) / kAp.sim_dt;
    // Expected: q*S*dCl_flap/m along body-up (lift axis == body_up here:
    // zero sideslip, alpha = 0).
    const double q = sim::q_dyn(sim::rho_at(2000.0, kAp), 100.0);
    const double expected = q * kAp.S * kAp.dCl_flap / kAp.mass;
    REQUIRE(expected > 1.0);  // non-vacuous: a real, felt force (> 0.1 g)
    CHECK(std::abs(dv_up_delta - expected) < 1e-6 * expected);

    // The true-n instrument reads the SAME composition the plant flies (diff
    // red-team P1-2: a bare-lift_coeff load_factor under-read the felt G by
    // ~2 g with landing flaps — the H1 fork, now single-sourced through
    // total_lift_coeff). At alpha = 0 the flap IS the whole n.
    const double n_flapped = sim::load_factor(0.0, 100.0, 2000.0, 1.0, kAp);
    CHECK(n_flapped == q * kAp.S * kAp.dCl_flap / (kAp.mass * kAp.g));
    CHECK(sim::load_factor(0.0, 100.0, 2000.0, 0.0, kAp) == 0.0);
}

TEST_CASE(
    "flaps: over-speed washout kills the LIFT shift, keeps the DRAG "
    "(the speed-brake default, pinned as a choice)") {
    // Above wash_hi: the lift shift is EXACTLY 0...
    const double Vhi = kAp.flap_wash_hi + 20.0;
    CHECK(sim::flap_lift_frac(Vhi, kAp) == 0.0);
    CHECK(sim::flap_dCl(1.0, Vhi, kAp) == 0.0);
    // ...and full at/below wash_lo (boundary-exact at wash_lo)...
    CHECK(sim::flap_lift_frac(kAp.flap_wash_lo, kAp) == 1.0);
    // ...with the config-recomputed smoothstep at an ASYMMETRIC in-band
    // point (diff red-team P1-1: the band midpoint t = 0.5 is the FIXED
    // POINT of the flip mutant `smoothstep <-> 1-smoothstep` — a probe there
    // passed the flipped curve bit-exactly, which also manufactures a step
    // at wash_lo, the bare-step gate the loader ordering exists to forbid).
    // t = 0.25: honest 0.84375 vs flipped 0.15625 — the mutant dies here.
    const double Vq =
        kAp.flap_wash_lo + 0.25 * (kAp.flap_wash_hi - kAp.flap_wash_lo);
    const double t =
        (Vq - kAp.flap_wash_lo) / (kAp.flap_wash_hi - kAp.flap_wash_lo);
    CHECK(sim::flap_lift_frac(Vq, kAp) == 1.0 - t * t * (3.0 - 2.0 * t));
    // Continuity at the lo edge: just inside the band the washout is still
    // ~1 (a flipped/stepped shape lands near 0 here).
    CHECK(sim::flap_lift_frac(kAp.flap_wash_lo + 1e-6, kAp) > 0.999);

    // Behavioral: at Vhi the deployed airframe bleeds speed strictly faster
    // (the drag tax survives the washout) while gaining no body-up kick.
    sim::SimState s = level_at(Vhi, 2000.0);
    s.throttle = 0.0;
    sim::Inputs in;
    sim::SimState sf = s;
    sf.flap = 1.0;
    in.flap_cmd = 1.0f;
    const sim::SimState clean = sim::step(s, sim::Inputs{}, kAp, kAp.sim_dt);
    const sim::SimState flapped = sim::step(sf, in, kAp, kAp.sim_dt);
    CHECK(glm::length(flapped.velocity) < glm::length(clean.velocity));
    const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const double dv_up = glm::dot(flapped.velocity - clean.velocity, body_up);
    CHECK(std::abs(dv_up) < 1e-9);  // no lift delta above the washout
}

TEST_CASE(
    "flaps: drag tax is quadratic in flap, linear in gear (combat "
    "detent cheap)") {
    CHECK(sim::flap_dCd0(0.5, kAp) == 0.25 * kAp.dCd0_flap);
    CHECK(sim::flap_dCd0(1.0, kAp) == kAp.dCd0_flap);
    CHECK(sim::gear_dCd0(0.5, kAp) == 0.5 * kAp.dCd0_gear);
    // Gear alone: pure drag, zero lift delta at alpha = 0.
    sim::SimState s = level_at(100.0, 2000.0);
    s.throttle = 0.0;
    sim::SimState sg = s;
    sg.gear = 1.0;
    sim::Inputs in;
    in.gear_cmd = 1.0f;
    const sim::SimState clean = sim::step(s, sim::Inputs{}, kAp, kAp.sim_dt);
    const sim::SimState geared = sim::step(sg, in, kAp, kAp.sim_dt);
    CHECK(glm::length(geared.velocity) < glm::length(clean.velocity));
    const glm::dvec3 body_up = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
    CHECK(std::abs(glm::dot(geared.velocity - clean.velocity, body_up)) < 1e-9);
}

// ===========================================================================
// The DEPLOYED energy fork-detector (the AT-12 shape, flaps/gear edition).
// The clean-airframe AT-12 is structurally blind to the flap/gear drag terms
// (identically 0 on its flight), so a step.cpp fork in flap_dCd0/gear_dCd0 —
// or in the flapped-Cl induced-drag coupling — would survive the whole gate.
// This leg flies an OPEN-LOOP deploying arc (throttle + a held pitch input,
// flap and gear commanded down mid-run so the SLEW path is exercised) and
// reconciles the plant's per-tick linear work m.v.(v'-v) against the config
// force model, with the composition DUPLICATED from step.cpp (aero.h
// primitives shared, assembly not — the fork-detector duplication, AT-12
// lesson). Mutation that must fail: flap_dCd0 -> 0 in step.cpp's composition
// (rel residual jumps from ~1e-13 to the tax's share of throughput).
// ===========================================================================
namespace {
struct DeployedPower {
    double net = 0.0;      // (grav+thrust+drag).v [W]
    double comp = 0.0;     // component throughput [W]
    double devices = 0.0;  // the flap+gear parasitic share of drag power [W]
};

DeployedPower deployed_power_config(const sim::SimState& s,
                                    const sim::Inputs& in,
                                    const sim::AircraftParams& p) {
    // Mirror step.cpp's seams exactly: float casts, clamps, slews (the
    // throttle float-seam lesson applies to flap/gear identically).
    const double thr_cmd =
        std::clamp(static_cast<double>(in.throttle), 0.0, 1.0);
    const double thr_cap = p.throttle_slew_rate * p.sim_dt;
    const double next_thr =
        s.throttle + std::clamp(thr_cmd - s.throttle, -thr_cap, thr_cap);
    const double flap_cmd =
        std::clamp(static_cast<double>(in.flap_cmd), 0.0, 1.0);
    const double flap_cap = p.flap_slew * p.sim_dt;
    const double next_flap =
        s.flap + std::clamp(flap_cmd - s.flap, -flap_cap, flap_cap);
    const double gear_cmd =
        std::clamp(static_cast<double>(in.gear_cmd), 0.0, 1.0);
    const double gear_cap = p.gear_slew * p.sim_dt;
    const double next_gear =
        s.gear + std::clamp(gear_cmd - s.gear, -gear_cap, gear_cap);

    const double speed = glm::length(s.velocity);
    const double alt = sim::altitude(s.position, p);
    const double q = sim::q_dyn(sim::rho_at(alt, p), speed);
    const glm::dvec3 vhat = sim::current_vhat(s, p);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double alpha = sim::alpha_of(sim::body_dir_of(s.orientation, vhat));
    const double Cl = sim::lift_coeff(alpha, p);
    double cl_total = Cl;
    double cd0_total = p.Cd0;
    double dev_cd = 0.0;
    if (next_flap > 0.0) {
        cl_total += sim::flap_dCl(next_flap, speed, p);
        cd0_total += sim::flap_dCd0(next_flap, p);
        dev_cd += sim::flap_dCd0(next_flap, p);
    }
    if (next_gear > 0.0) {
        cd0_total += sim::gear_dCd0(next_gear, p);
        dev_cd += sim::gear_dCd0(next_gear, p);
    }
    const glm::dvec3 gravity = (p.mass * p.g) * sim::gravity_dir(s.position);
    const glm::dvec3 thrust =
        (p.T_max * sim::atm_frac(alt, p) * next_thr) * nose;
    const glm::dvec3 drag =
        -(q * p.S * (cd0_total + p.k_induced * cl_total * cl_total)) * vhat;
    const double gp = glm::dot(gravity, s.velocity);
    const double tp = glm::dot(thrust, s.velocity);
    const double dp = glm::dot(drag, s.velocity);
    DeployedPower out;
    out.net = gp + tp + dp;
    out.comp = std::abs(gp) + std::abs(tp) + std::abs(dp);
    out.devices = q * p.S * dev_cd * glm::dot(vhat, s.velocity);
    return out;
}
}  // namespace

TEST_CASE(
    "flaps: deployed-arc energy reconciles with the config force model "
    "(the fork detector AT-12 is blind to)") {
    sim::SimState s = level_at(150.0, 2500.0);
    sim::Inputs in;
    in.throttle = 0.7f;
    in.pitch = 0.1f;  // a gentle held pull: alpha develops, Cl coupling live
    double residual = 0.0, throughput = 0.0, devices_work = 0.0;
    for (int i = 0; i < 720; ++i) {  // 6 s; devices deploy at 1 s
        if (i == 120) {
            in.flap_cmd = 1.0f;
            in.gear_cmd = 1.0f;
        }
        const glm::dvec3 v = s.velocity;
        const DeployedPower P = deployed_power_config(s, in, kAp);
        const sim::SimState next = sim::step(s, in, kAp, kAp.sim_dt);
        const double plant_work = kAp.mass * glm::dot(v, next.velocity - v);
        residual += std::abs(plant_work - P.net * kAp.sim_dt);
        throughput += P.comp * kAp.sim_dt;
        devices_work += P.devices * kAp.sim_dt;
        s = next;
    }
    const double rel = residual / throughput;
    std::printf(
        "[flaps energy] rel=%.2e devices=%.3f MJ throughput=%.3f MJ "
        "flap=%.2f gear=%.2f V=%.1f\n",
        rel, devices_work * 1e-6, throughput * 1e-6, s.flap, s.gear,
        glm::length(s.velocity));
    REQUIRE(s.flap == 1.0);         // the deploy actually happened
    REQUIRE(devices_work > 1.0e5);  // non-vacuous: the tax did real work
    CHECK(rel < 1e-9);              // plant == config (machine-eps class)
}

TEST_CASE("flaps: instructor passthrough (throttle pattern), incl. GROUNDED") {
    const sim::SimState s = level_at(140.0, 2000.0);
    control::Input in;
    in.target_dir_world = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    in.throttle = 0.6;
    in.flap_cmd = 0.7;
    in.gear_cmd = 0.3;
    control::Internal internal = control::reset();
    const control::Output o =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    CHECK(o.inputs.flap_cmd == static_cast<float>(0.7));
    CHECK(o.inputs.gear_cmd == static_cast<float>(0.3));
    // GROUNDED passes the device commands through like throttle (no one-tick
    // retract twitch under a held setting on a spawn/mode-toggle tick).
    in.grounded = true;
    const control::Output g =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    CHECK(g.inputs.flap_cmd == static_cast<float>(0.7));
    CHECK(g.inputs.gear_cmd == static_cast<float>(0.3));
    // Out-of-range commands clamp (caller robustness, the throttle clamp).
    in.grounded = false;
    in.flap_cmd = 1.7;
    in.gear_cmd = -0.4;
    const control::Output c =
        control::step(s, in, internal, kAp, kCp, kAp.sim_dt);
    CHECK(c.inputs.flap_cmd == 1.0f);
    CHECK(c.inputs.gear_cmd == 0.0f);
}

TEST_CASE("flaps: loader rejects a degenerate washout band") {
    const std::string base = slurp(SEADS_CONFIG_DIR "/aircraft.toml");
    REQUIRE(base.find("wash_lo     = 130.0") != std::string::npos);
    // lo >= hi: the smoothstep denominator dies (and lo == hi would be the
    // bare-step gate the continuity rule forbids).
    const std::string bad =
        replace_all(base, "wash_lo     = 130.0", "wash_lo     = 250.0");
    CHECK_THROWS(cfg::load_aircraft_toml(write_temp(bad, "wash_band")));
    // Zero slew freezes a mid-travel device forever.
    const std::string bad2 =
        replace_all(base, "slew        = 0.7", "slew        = 0.0");
    CHECK_THROWS(cfg::load_aircraft_toml(write_temp(bad2, "zero_slew")));
}
