// THROWAWAY: is the tape v5 COLUMN SET sufficient to seed a mid-window replay
// faithfully? Flies a scripted BANKED TURN recording exactly the v5 fields,
// then re-seeds from those fields alone at a mid-window tick and re-flies the
// recorded mouse deltas. Any divergence is a MISSING COLUMN, by construction:
// the two runs share the plant, the controller and the input.
//
// v4 failed this on Chad's real dives (roll diverged ~100 deg in 6 s) because
// it recorded the aim's FORWARD but not the aim FRAME (apply_mouse rotates
// about the frame's own axes) and none of control::Internal's latches.
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

#include "app/feel_tape.h"
#include "app/feel_tape_columns.h"
#include "app/instructor_tick.h"
#include "test/harness/feel_tape.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/world.h"
#include "test/harness/instructor.h"

namespace {
const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);
constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

// EXACTLY the v5 tape columns that seed a replay.
struct Snap {
    glm::dvec3 pos, vel, vhat, angvel;
    glm::dquat q, aq;
    double roll_latch, elev_latch, aoa_filtered, held_bank, hand_rest;
    glm::dvec3 integ;
    int capture;
    bool ballistic, deadzoned, pursuit, righting;
    double rest_time, inv_rest;
    // the S-rimshot capture block + the rest of Internal: a HALF-seeded
    // capture event steers the pointing demand for a few hundred ms, which is
    // what the 1.6 deg roll residual was
    double cap_ux, cap_uy, cap_w_hold, cap_err0, cap_d_allow, cap_rim_t;
    int cap_stall_ticks;
    bool cap_crossed, cap_refractory, cap_inbound;
    glm::dvec3 ovr_ramp, aim_rate_filt;
    bool any_override, push_mode;
    // APP-side: the horizon-recovery machinery ROLLS THE AIM FRAME
    // (rest_horizon_tick), so without it the recorded deltas steer along a
    // different basis -- this, not control::Internal, was the residual.
    double aim_rest;
    bool recov_armed;
    glm::dvec3 prev_path;
    input::HorizonRecovery recov;
    double dx, dy;      // the recorded mouse
    double phi, theta, alt;  // the recorded truth
};

app::LoopState flying(const sim::SimState& s) {
    app::LoopState st;
    st.curr = s;
    st.prev = s;
    st.prev_up = sim::local_up(s.position);
    st.aim.reseed(s.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    return st;
}

void record(app::LoopState& st, const app::TickResult& tr, Snap& s) {
    s.pos = st.curr.position;
    s.vel = st.curr.velocity;
    s.vhat = st.curr.last_vhat;
    s.angvel = st.curr.angular_vel;
    s.q = st.curr.orientation;
    s.aq = st.aim.q;
    s.roll_latch = st.internal.roll_latch;
    s.elev_latch = st.internal.elev_latch;
    s.aoa_filtered = st.internal.aoa_filtered;
    s.held_bank = st.internal.held_bank;
    s.hand_rest = st.internal.hand_rest;
    s.integ = st.internal.integ;
    s.capture = int(st.internal.capture);
    s.ballistic = st.internal.ballistic;
    s.deadzoned = st.internal.deadzoned;
    s.pursuit = st.internal.pursuit;
    s.righting = st.internal.righting;
    s.rest_time = st.internal.rest_time;
    s.inv_rest = st.internal.inv_rest;
    s.cap_ux = st.internal.cap_ux;
    s.cap_uy = st.internal.cap_uy;
    s.cap_w_hold = st.internal.cap_w_hold;
    s.cap_err0 = st.internal.cap_err0;
    s.cap_d_allow = st.internal.cap_d_allow;
    s.cap_rim_t = st.internal.cap_rim_t;
    s.cap_stall_ticks = st.internal.cap_stall_ticks;
    s.cap_crossed = st.internal.cap_crossed;
    s.cap_refractory = st.internal.cap_refractory;
    s.cap_inbound = st.internal.cap_inbound;
    s.ovr_ramp = st.internal.ovr_ramp;
    s.aim_rate_filt = st.internal.aim_rate_filt;
    s.any_override = st.internal.any_override;
    s.push_mode = st.internal.push_mode;
    s.aim_rest = st.aim_rest;
    s.recov_armed = st.recov_armed;
    s.prev_path = st.prev_path;
    s.recov = st.recov;
    const glm::dvec3 up = sim::local_up(st.curr.position);
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0, 0, -1};
    s.phi = tr.telem.extracted.phi;
    s.theta = std::asin(std::clamp(glm::dot(nose, up), -1.0, 1.0));
    s.alt = sim::altitude(st.curr.position, kAp);
}

app::LoopState seed(const Snap& s) {
    app::LoopState st;
    st.curr.position = s.pos;
    st.curr.velocity = s.vel;
    st.curr.orientation = s.q;
    st.curr.last_vhat = s.vhat;
    // THE one that mattered: a tick's body rate carries into the
    // next tick's semi-implicit integration, so a zero angular_vel
    // at the seed gets pitch wrong on tick ONE (dphi was already
    // 0.000e+00 -- roll seeded fine -- while dtheta was 1.3e-02 to
    // 3.4e-01 deg before this line existed).
    st.curr.angular_vel = s.angvel;
    st.prev = st.curr;
    st.prev_up = sim::local_up(s.pos);
    st.aim.q = s.aq;                       // v5: the FRAME, not the forward
    st.internal = control::reset();
    st.internal.roll_latch = s.roll_latch;
    st.internal.elev_latch = s.elev_latch;
    st.internal.aoa_filtered = s.aoa_filtered;
    st.internal.held_bank = s.held_bank;
    st.internal.hand_rest = s.hand_rest;
    st.internal.integ = s.integ;
    st.internal.capture = control::CaptureState(s.capture);
    st.internal.ballistic = s.ballistic;
    st.internal.deadzoned = s.deadzoned;
    st.internal.pursuit = s.pursuit;
    st.internal.righting = s.righting;
    st.internal.rest_time = s.rest_time;
    st.internal.inv_rest = s.inv_rest;
    st.internal.last_vhat = s.vhat;
    st.internal.cap_ux = s.cap_ux;
    st.internal.cap_uy = s.cap_uy;
    st.internal.cap_w_hold = s.cap_w_hold;
    st.internal.cap_err0 = s.cap_err0;
    st.internal.cap_d_allow = s.cap_d_allow;
    st.internal.cap_rim_t = s.cap_rim_t;
    st.internal.cap_stall_ticks = s.cap_stall_ticks;
    st.internal.cap_crossed = s.cap_crossed;
    st.internal.cap_refractory = s.cap_refractory;
    st.internal.cap_inbound = s.cap_inbound;
    st.internal.ovr_ramp = s.ovr_ramp;
    st.internal.aim_rate_filt = s.aim_rate_filt;
    st.internal.any_override = s.any_override;
    st.internal.push_mode = s.push_mode;
    st.aim_rest = s.aim_rest;
    st.recov_armed = s.recov_armed;
    st.prev_path = s.prev_path;
    st.recov = s.recov;
    st.grounded = false;
    return st;
}
}  // namespace

TEST_CASE("TAPE v5: the column set seeds a mid-window replay faithfully") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.9;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 230.0, 3000.0, up, heading, &thr);
    // A scripted BANKED TURN: a sustained lateral sweep, the manoeuvre v4
    // could not seed.
    const int N = 1200;  // 10 s
    std::vector<Snap> tape(N);
    {
        app::LoopState st = flying(s0);
        harness::MiniCamera cam;
        cam.seed(st.curr);
        for (int i = 0; i < N; ++i) {
            app::TickInput in;
            in.raw_mode = false;
            in.throttle = 1.0;
            in.aim_dx = rad(70.0) * kAp.sim_dt / kCp.aim_sensitivity;
            in.aim_dy = (i > 400) ? -rad(15.0) * kAp.sim_dt / kCp.aim_sensitivity
                                  : 0.0;
            in.cam_fwd = cam.cam_fwd;
            in.cam_up = cam.cam_up;
            tape[i].dx = in.aim_dx;
            tape[i].dy = in.aim_dy;
            const app::TickResult tr = app::tick(st, in, kAp, kCp, nullptr);
            cam.advance(st.curr, st.aim.forward(), st.aim.up(), kCp,
                        kAp.sim_dt);
            record(st, tr, tape[i]);
        }
    }
    // Re-seed MID-WINDOW and re-fly the recorded deltas.
    for (int start : {300, 600, 900}) {
        app::LoopState st = seed(tape[start]);
        harness::MiniCamera cam;
        cam.seed(st.curr);
        double dphi = 0, dth = 0, dalt = 0;
        double dphi_first = 0, dth_first = 0, dalt_first = 0;
        bool first = true;
        for (int i = start + 1; i < N; ++i) {
            app::TickInput in;
            in.raw_mode = false;
            in.throttle = 1.0;
            in.aim_dx = tape[i].dx;
            in.aim_dy = tape[i].dy;
            in.cam_fwd = cam.cam_fwd;
            in.cam_up = cam.cam_up;
            const app::TickResult tr = app::tick(st, in, kAp, kCp, nullptr);
            cam.advance(st.curr, st.aim.forward(), st.aim.up(), kCp,
                        kAp.sim_dt);
            const glm::dvec3 u = sim::local_up(st.curr.position);
            const glm::dvec3 nz = st.curr.orientation * glm::dvec3{0, 0, -1};
            if (first) {
                const glm::dvec3 uu = sim::local_up(st.curr.position);
                const glm::dvec3 nn =
                    st.curr.orientation * glm::dvec3{0, 0, -1};
                const double th1 =
                    std::asin(std::clamp(glm::dot(nn, uu), -1.0, 1.0));
                std::printf(
                    "[v5]   first replayed tick: dphi %.3e dtheta %.3e dalt %.3e\n",
                    deg(std::abs(tr.telem.extracted.phi - tape[i].phi)),
                    deg(std::abs(th1 - tape[i].theta)),
                    std::abs(sim::altitude(st.curr.position, kAp) -
                             tape[i].alt));
                dphi_first = std::abs(tr.telem.extracted.phi - tape[i].phi);
                dth_first = std::abs(th1 - tape[i].theta);
                dalt_first =
                    std::abs(sim::altitude(st.curr.position, kAp) -
                             tape[i].alt);
                first = false;
            }
            dphi = std::max(dphi,
                            std::abs(tr.telem.extracted.phi - tape[i].phi));
            dth = std::max(
                dth, std::abs(std::asin(std::clamp(glm::dot(nz, u), -1.0, 1.0))
                              - tape[i].theta));
            dalt = std::max(
                dalt, std::abs(sim::altitude(st.curr.position, kAp)
                               - tape[i].alt));
        }
        std::printf(
            "[v5] seed at tick %4d (t=%.2f s, phi %6.1f) -> over the "
            "remaining %.2f s: max |dphi| %.6f deg  |dtheta| %.6f deg  "
            "|dalt| %.4f m\n",
            start, start * kAp.sim_dt, deg(tape[start].phi),
            (N - start) * kAp.sim_dt, deg(dphi), deg(dth), dalt);
        // WHAT IS PINNED: the SEED, on the first replayed tick. That is
        // what the column set controls, and it is now exact to ~1e-5 deg --
        // dphi is identically 0.
        CHECK(deg(dphi_first) < 1e-3);
        CHECK(deg(dth_first) < 1e-3);
        CHECK(dalt_first < 1e-2);
        // WHAT IS *NOT* PINNED HERE, and why -- CORRECTED 2026-09-13.
        // This case round-trips an in-memory Snap that carries only PART of
        // the replay state, so it drifts. The earlier comment blamed that
        // drift on Lyapunov growth ("~1e-6 deg grows to ~0.6 deg over
        // 300-900 ticks, NOT a missing column") and concluded a
        // counterfactual is only readable over a SHORT window. That was
        // WRONG, and it was load-bearing: it justified capping every
        // lateral counterfactual at 1 s.
        //
        // The e2e case below seeds the COMPLETE state (every member of
        // sim::SimState and control::Internal, generated from
        // SEADS_TAPE_STATE_FIELDS) and replays BIT-IDENTICALLY across the
        // whole window -- dalt and dV exactly 0. The drift was never chaos;
        // it was the missing columns. Long-window grading is legitimate.
        //
        // The bound below is therefore a property of THIS partial fixture,
        // not of the plant.
        CHECK(deg(dphi) < 2.0);
    }
}


// ===========================================================================
// END-TO-END: the SHIPPED writer -> a real CSV -> the THROWING reader -> a
// seeded replay. This is the proof; the in-memory Snap case above is now only
// a unit on the seeding logic.
//
// The in-memory case could not have caught what actually broke: the writer
// named 52 columns while emitting 74, so every v5 seed field was on disk but
// unnamed, and a name-indexed probe silently read 0.0 for angular_vel, the
// aim quaternion and all of control::Internal. Nothing here is allowed to
// default -- harness::FeelTape throws on a missing column.
// ===========================================================================
TEST_CASE("TAPE e2e: the shipped writer's own CSV re-seeds a replay") {
    // --- 1. FLY, recording through app::feel_tape_hook itself ---
    const std::string path =
        std::string(SEADS_TEST_TMP_DIR) + "/tape_e2e.csv";
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 1.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 190.0, 2500.0, up, heading, &thr);

    app::FeelTapeCtx tape_ctx;
    REQUIRE(app::feel_tape_open(tape_ctx, path.c_str(),
                                "e2e: scripted banked turn\n", kAp));

    app::LoopState st = flying(s0);
    // NON-DEFAULT plant device state. This is the regression for the second
    // instance of the missing-seed bug: throttle/flap/gear are slewed PLANT
    // state that no INPUT can restore in one tick, so if any of them is
    // absent from the tape the replay flies a different aeroplane and dV
    // diverges. Defaults (0/0/0) would let the omission pass unnoticed.
    st.curr.throttle = 0.62;
    st.curr.flap = 0.35;
    st.curr.gear = 0.20;
    st.prev = st.curr;

    harness::MiniCamera cam;
    cam.seed(st.curr);
    const int N = 360;  // 3 s
    for (int i = 0; i < N; ++i) {
        app::TickInput in;
        in.raw_mode = false;
        // a throttle the plant must SLEW toward, away from the seeded 0.62
        in.throttle = (i < 180) ? 0.40 : 0.95;
        in.flap_cmd = 0.35;
        in.gear_cmd = 0.20;
        in.aim_dx = rad(55.0) * kAp.sim_dt / kCp.aim_sensitivity;
        in.aim_dy = (i > 120) ? rad(18.0) * kAp.sim_dt / kCp.aim_sensitivity
                              : 0.0;
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult tr = app::tick(st, in, kAp, kCp, nullptr);
        app::feel_tape_hook(in, st, tr.telem, &tape_ctx);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), kCp, kAp.sim_dt);
    }
    std::fclose(tape_ctx.f);
    tape_ctx.f = nullptr;
    // the fixture must actually have moved the devices, or it proves nothing
    REQUIRE(st.curr.throttle != 0.62);
    REQUIRE(st.curr.flap > 0.0);

    // --- 2. READ IT BACK with the throwing reader ---
    harness::FeelTape tp;
    std::vector<std::string> need = harness::FeelTape::seed_columns();
    for (const std::string& c : harness::FeelTape::input_columns())
        need.push_back(c);
    for (const std::string& c : harness::FeelTape::truth_columns())
        need.push_back(c);
    REQUIRE_NOTHROW(tp.load(path, need));  // throws if ANY name is absent
    std::printf("[e2e] wrote + parsed %zu rows x %zu columns (emitter "
                "declares %d; %d of them are replay state)\n",
                tp.size(), tp.columns(), app::kFeelTapeColumnCount,
                app::kFeelTapeStateFieldCount);
    CHECK(int(tp.columns()) == app::kFeelTapeColumnCount);
    REQUIRE(tp.size() == size_t(N));

    // --- 3. SEED MID-TAPE from those columns ALONE and replay ---
    const size_t start = 200;
    app::LoopState r;
    tp.seed(start, r);   // the COMPLETE state, generated from the field list

    harness::MiniCamera rcam;
    rcam.seed(r.curr);
    double dphi_first = 0, dth_first = 0, dv_first = 0, dalt_first = 0;
    double dphi = 0, dth = 0, dv = 0;
    bool first = true;
    // s_* is the POST-tick state of row `start`, so the next input to apply
    // is row start+1's. (Seeding at `start` and replaying row `start` would
    // redo that tick -- an off-by-one that reads as a 0.1 deg seed error.)
    for (size_t i = start + 1; i < tp.size(); ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = (i < 180) ? 0.40 : 0.95;
        in.flap_cmd = 0.35;
        in.gear_cmd = 0.20;
        in.aim_dx = tp.at(i, "aim_dx");
        in.aim_dy = tp.at(i, "aim_dy");
        in.frame_ticks = int(tp.at(i, "frame_ticks"));
        in.cam_fwd = rcam.cam_fwd;
        in.cam_up = rcam.cam_up;
        const app::TickResult tr = app::tick(r, in, kAp, kCp, nullptr);
        rcam.advance(r.curr, r.aim.forward(), r.aim.up(), kCp, kAp.sim_dt);
        const glm::dvec3 u = sim::local_up(r.curr.position);
        const glm::dvec3 nz = r.curr.orientation * glm::dvec3{0, 0, -1};
        const double th = std::asin(std::clamp(glm::dot(nz, u), -1.0, 1.0));
        const double a = std::abs(tr.telem.extracted.phi - tp.at(i, "phi"));
        const double b = std::abs(th - tp.at(i, "theta"));
        const double c =
            std::abs(glm::length(r.curr.velocity) - tp.at(i, "speed"));
        if (first) {
            dphi_first = a;
            dth_first = b;
            dv_first = c;
            dalt_first = std::abs(sim::altitude(r.curr.position, kAp)
                                  - tp.at(i, "alt"));
            first = false;
        }
        dphi = std::max(dphi, a);
        dth = std::max(dth, b);
        dv = std::max(dv, c);
    }
    std::printf("[e2e] seed at tick %zu -> FIRST TICK |dphi| %.3e deg "
                "|dtheta| %.3e deg |dalt| %.3e m |dV| %.3e m/s\n"
                "[e2e]   over %.2f s: max |dphi| %.4f deg |dtheta| %.4f deg "
                "|dV| %.4f m/s\n",
                start, deg(dphi_first), deg(dth_first), dalt_first, dv_first,
                (tp.size() - start) * kAp.sim_dt, deg(dphi), deg(dth), dv);
    // The SEED is what the column set controls: exact on the first tick.
    CHECK(deg(dphi_first) < 1e-3);
    CHECK(deg(dth_first) < 1e-3);
    CHECK(dalt_first < 1e-6);
    // dV is the one that caught the throttle/flap/gear omission. It must be
    // machine-eps, not "small" -- a 4.7e-02 m/s offset read as small for a
    // whole night.
    CHECK(dv_first < 1e-9);
    // The complete seed replays EXACTLY: no drift to allow for.
    CHECK(deg(dphi) < 2.0);
}

// A tape that predates a required column must FAIL LOUDLY, never default.
// This is the regression for the silent 0.0 that voided the dive battery.
TEST_CASE("TAPE e2e: a stale tape throws instead of defaulting to zero") {
    const std::string path =
        std::string(SEADS_TEST_TMP_DIR) + "/tape_stale.csv";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    // a v4-era header: everything through push_mode2, nothing after
    std::fprintf(f, "# stale\n");
    for (int i = 0; i < 52; ++i)
        std::fprintf(f, "%s%s", app::kFeelTapeColumns[i], i == 51 ? "\n" : ",");
    for (int i = 0; i < 52; ++i) std::fprintf(f, "0%s", i == 51 ? "\n" : ",");
    std::fclose(f);

    harness::FeelTape tp;
    CHECK_THROWS_AS(tp.load(path, harness::FeelTape::seed_columns()),
                    std::runtime_error);
}

// ---------------------------------------------------------------------------
// ★★★ terrain-clip T2b (red-team P1-5) — THE CRASH SURFACE IS A TAPE FIELD.
//
// app/main.cpp appends "[config] ground: facet_contact %.2f (injected: %s)" to
// g_config_banner AFTER the ground params and the render-side facet injection
// both resolve, and feel_tape_open copies the whole banner into the tape as '#'
// comment lines. A tape flown against the drawn mesh facet replayed against the
// DEM field disagrees by up to 66.7 m of terrain, so the reader warns.
//
// Three legs: the field is READ off a written banner; a PRE-T2b tape without
// the line reads as 0/absent and does not throw (backwards-readable); and the
// mismatch warning fires on a disagreement and stays quiet on a match.
// ---------------------------------------------------------------------------
namespace {
void write_tape(const std::string& path, const char* banner) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    std::fprintf(f, "# SEADS feel tape\n");
    if (banner != nullptr) std::fprintf(f, "# %s\n", banner);
    for (int i = 0; i < app::kFeelTapeColumnCount; ++i)
        std::fprintf(f, "%s%s", app::kFeelTapeColumns[i],
                     i + 1 < app::kFeelTapeColumnCount ? "," : "\n");
    for (int i = 0; i < app::kFeelTapeColumnCount; ++i)
        std::fprintf(f, "0%s", i + 1 < app::kFeelTapeColumnCount ? "," : "\n");
    std::fclose(f);
}
}  // namespace

TEST_CASE("TAPE T2b: facet_contact is stamped, read back, and mismatch warns") {
    const std::string armed =
        std::string(SEADS_TEST_TMP_DIR) + "/tape_facet_armed.csv";
    const std::string old =
        std::string(SEADS_TEST_TMP_DIR) + "/tape_facet_pre_t2b.csv";

    // 1. THE ARMED TAPE — the exact line app/main.cpp writes.
    write_tape(armed, "[config] ground: facet_contact 1.00 (injected: yes)");
    harness::FeelTape a;
    a.load(armed, harness::FeelTape::seed_columns());
    REQUIRE(a.has_facet_contact());
    REQUIRE(a.facet_contact_or(-1.0) == 1.0);
    REQUIRE(a.facet_injected());
    // Agreement is silent and true; a disagreement is loud and false.
    CHECK(a.warn_if_facet_mismatch(1.0, true) == true);
    CHECK(a.warn_if_facet_mismatch(0.0, true) == false);   // the kill env arm
    CHECK(a.warn_if_facet_mismatch(1.0, false) == false);  // no render layer

    // 2. BACKWARDS-READABLE — a pre-T2b tape has no such line. It must load
    //    clean, report absent, and read as the field arm (0/absent), which is
    //    what it actually was.
    write_tape(old, "[config] auto_level: lean_lead 0.300");
    harness::FeelTape b;
    b.load(old, harness::FeelTape::seed_columns());
    REQUIRE(b.has_facet_contact() == false);
    REQUIRE(b.facet_contact_or(0.0) == 0.0);
    REQUIRE(b.facet_injected() == false);
    CHECK(b.warn_if_facet_mismatch(0.0, false) == true);
    CHECK(b.warn_if_facet_mismatch(1.0, true) == false);

    // 3. THE DISARMED-BUT-INJECTED TAPE: the honest OFF arm of the A/B.
    const std::string off =
        std::string(SEADS_TEST_TMP_DIR) + "/tape_facet_off.csv";
    write_tape(off, "[config] ground: facet_contact 0.00 (injected: yes)");
    harness::FeelTape c;
    c.load(off, harness::FeelTape::seed_columns());
    REQUIRE(c.has_facet_contact());
    REQUIRE(c.facet_contact_or(-1.0) == 0.0);
    REQUIRE(c.facet_injected());
    CHECK(c.warn_if_facet_mismatch(0.0, true) == true);
    CHECK(c.warn_if_facet_mismatch(1.0, true) == false);
}
