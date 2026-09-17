// ===========================================================================
// S-yawbudget (2026-09-13, TARGET 2 -- the lateral nose-down).
//
// Chad: "when I make a large sideways deflection of my mouse and ask the
// plane to follow it, it noses down crashing me if I am near the deck", under
// his law "the plane should follow my mouse... an instructor would never
// crash me into the ground if I did not mount my mouse anywhere near there."
//
// WHY THESE LEGS ARE SCRIPTED, NOT TAPE-SEEDED: the attribution and the
// grading were done on Chad's OWN tape (feel_tape_lateral4.csv, seeded exact
// -- first-tick dV identically 0). That tape is 8 MB and an excerpt spanning
// one event is ~600 KB, well past what belongs in the repo as a fixture. So
// the legs below reproduce the ev6 SIGNATURE from a scripted entry instead:
//     cosPhiTheta 0.25-0.49 at onset, yaw*sin(phi) ~ -23 deg/s, the AoA
//     ceiling binding, the nose BELOW the aim.
// The tape numbers they stand in for are recorded in control/params.h.
// ===========================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "app/feel_tape_fields.h"
#include "app/instructor_tick.h"
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

struct Out {
    double dalt = 0, agl = 1e18, gap_end = 0, yaw_mean = 0, turn = 0;
    double n_pk = 0, v_end = 0, cpt_onset = 0, yv_mean = 0;
    int aoa_bound = 0, ticks = 0;
};

// A scripted lateral deflection from a banked entry -- the ev6 shape.
Out lateral_run(const control::ControllerParams& cp, double lat_deg_s,
                double V, double alt, int ticks) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 1.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, alt, up, heading, &thr);
    app::LoopState st;
    st.curr = s0;
    st.prev = s0;
    st.prev_up = sim::local_up(s0.position);
    st.aim.reseed(s0.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    harness::MiniCamera cam;
    cam.seed(st.curr);
    Out o;
    const double alt0 = sim::altitude(st.curr.position, kAp);
    glm::dvec3 h0{0.0};
    double ysum = 0, yvsum = 0;
    for (int i = 0; i < ticks; ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;
        in.aim_dx = rad(lat_deg_s) * kAp.sim_dt / cp.aim_sensitivity;
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult tr = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        const glm::dvec3 u = sim::local_up(st.curr.position);
        const glm::dvec3 nb = st.curr.orientation * glm::dvec3{0, 0, -1};
        if (i == 0)
            h0 = glm::normalize(st.curr.velocity
                                - u * glm::dot(st.curr.velocity, u));
        const double th = std::asin(std::clamp(glm::dot(nb, u), -1.0, 1.0));
        const double ae = std::asin(std::clamp(
            glm::dot(glm::normalize(st.aim.forward()), u), -1.0, 1.0));
        o.gap_end = th - ae;  // nose - aim; NEGATIVE = nose BELOW the aim
        const double a = sim::altitude(st.curr.position, kAp);
        o.agl = std::min(o.agl, a);
        o.dalt = a - alt0;
        o.n_pk = std::max(o.n_pk, tr.telem.load_factor);
        ysum += tr.telem.omega_des.y;
        yvsum += tr.telem.omega_des.y * std::sin(tr.telem.extracted.phi);
        if (o.cpt_onset == 0.0 && o.gap_end < rad(-5.0))
            o.cpt_onset = tr.telem.extracted.cos_phi_theta;
        o.v_end = glm::length(st.curr.velocity);
        const glm::dvec3 h = glm::normalize(
            st.curr.velocity - u * glm::dot(st.curr.velocity, u));
        o.turn = std::acos(std::clamp(glm::dot(h0, h), -1.0, 1.0))
                 / ((i + 1) * kAp.sim_dt);
        ++o.ticks;
    }
    o.yaw_mean = ysum / o.ticks;
    o.yv_mean = yvsum / o.ticks;
    return o;
}

control::ControllerParams off_arm() {
    control::ControllerParams cp = kCp;
    // Only THIS dial is zeroed here: these legs measure S-yawbudget against
    // the rest of the shipped tree (the S-leanlead red-team P1 lesson -- an
    // off arm that also walks back a DIFFERENT landed dial measures the
    // wrong counterfactual). The hash-pinned pre-change arm that must zero
    // EVERY lane dial lives in test_loop_rollover.cpp and uses
    // SEADS_FEEL_DIALS_OFF.
    cp.yaw_vert_budget = 0.0;
    return cp;
}
}  // namespace

TEST_CASE("S-yawbudget: the shipped dial is ON and its gate is armed") {
    CHECK(kCp.yaw_vert_budget == 1.0);
    // The edges, re-derived against ONSET fixtures (the 5..20 band was set
    // from mid-manoeuvre windows that could not see the roll-in).
    CHECK(kCp.yaw_vert_gap_lo == Catch::Approx(std::sin(rad(-5.0))));
    CHECK(kCp.yaw_vert_gap_hi == Catch::Approx(std::sin(rad(10.0))));
    CHECK(kCp.yaw_vert_gap_lo < kCp.yaw_vert_gap_hi);
    // THE WALL guards gap_HI: the band must not SATURATE, because a
    // saturated band IS the ungated form (V250 lat-90 turn 41.01 -> 31.19,
    // yaw -30.85 -> +4.16). Tracked hard turns measure nose-below-aim sag
    // p90 8.0 / p95 10.5 deg over Chad's tapes 4 and 5 (n=3534).
    CHECK(kCp.yaw_vert_gap_hi > std::sin(rad(8.0)));
    // NOT a wall on gap_lo: a NEGATIVE gap_lo (trim starting before the nose
    // reaches the aim) is the measured-better setting, not the dangerous one.
    CHECK(kCp.yaw_vert_gap_lo < 0.0);
    // (red-team P1-2) but the lower edge has its own wall: at -40 the V250
    // lat-90 yaw demand reads -0.52 (the plane stops following the mouse).
    CHECK(kCp.yaw_vert_gap_lo >= std::sin(rad(-20.0)));
}

TEST_CASE("S-yawbudget: budget 0 is bit-identical to the pre-dial tree") {
    // The knob-off arm. Every kernel dial must have a 0 that reproduces the
    // legacy expression tree exactly (the frozen-kernel law).
    control::ControllerParams a = off_arm();
    control::ControllerParams b = off_arm();
    b.yaw_vert_gap_lo = 0.3;  // gate values must not matter at budget 0
    b.yaw_vert_gap_hi = 0.9;
    const Out ra = lateral_run(a, 90.0, 250.0, 3000.0, 480);
    const Out rb = lateral_run(b, 90.0, 250.0, 3000.0, 480);
    CHECK(ra.dalt == rb.dalt);
    CHECK(ra.yaw_mean == rb.yaw_mean);
    CHECK(ra.turn == rb.turn);
    CHECK(ra.gap_end == rb.gap_end);
}

TEST_CASE("S-yawbudget: DIRECTION -- the trim reduces the digging yaw") {
    // HONEST SCOPE OF THIS LEG. A scripted entry from level trim CANNOT
    // reproduce Chad's dive: his events start already banked, mid-manoeuvre,
    // with the nose below the aim at cosPhiTheta 0.25-0.49 and the AoA
    // ceiling binding 14-87% of ticks. This fixture reaches cosPhiTheta
    // ~0.08 and loses ~4 m, not 124-490 m. Tuning it until it looked like
    // ev6 would be exactly the "green by tweaking constants" failure this
    // repo has paid for repeatedly -- so it is NOT tuned, and it pins only
    // what it can honestly see: the DIRECTION of the trim.
    //
    // The MAGNITUDES live in control/params.h, measured on Chad's tape 4
    // with exact seeds (first-tick dV identically 0):
    //     ev7 dAlt -490 -> -446   ev1 -286 -> -251   ev6 -187 -> -124
    // A compact tape fixture (one full-precision seed row + the recorded
    // mouse for the event, ~50 KB) is the honest way to pin those here; it
    // is not yet built.
    const Out off = lateral_run(off_arm(), 100.0, 205.0, 1700.0, 200);
    const Out on = lateral_run(kCp, 100.0, 205.0, 1700.0, 200);

    std::printf("[yawbudget dir] OFF gap %7.2f yv %7.2f cpt@onset %5.3f | "
                "ON gap %7.2f yv %7.2f\n",
                deg(off.gap_end), deg(off.yv_mean), off.cpt_onset,
                deg(on.gap_end), deg(on.yv_mean));

    // PREMISE -- the dial must actually be firing, or the checks are vacuous.
    REQUIRE(off.gap_end < rad(-5.0));   // the nose IS below the aim
    REQUIRE(deg(off.yv_mean) < -3.0);   // the rudder IS digging
    REQUIRE(on.yv_mean != off.yv_mean); // the block IS reached

    // THE DIRECTION. Mutation: delete the `yaw *= 1 - gate*(...)` line and
    // ON == OFF, so the REQUIRE above and both CHECKs below red.
    CHECK(on.yv_mean > off.yv_mean);   // less digging
    CHECK(on.gap_end > off.gap_end);   // nose closer to the aim
}

TEST_CASE("S-yawbudget: COST -- the V250 hard turn is not flattened") {
    // THE GATE'S REASON TO EXIST. Ungated, this dial collapses the lat-90
    // turn 41.01 -> 29.33 deg/s and FLIPS the yaw demand -30.85 -> +6.06:
    // the plane stops following the mouse, which inverts Chad's own law.
    // Mutation: delete the `gate` factor (or set gap_lo >= gap_hi so the
    // smoothstep saturates to 1) and the lat-90 leg below reds.
    const Out off40 = lateral_run(off_arm(), 40.0, 250.0, 3000.0, 480);
    const Out on40 = lateral_run(kCp, 40.0, 250.0, 3000.0, 480);
    const Out off90 = lateral_run(off_arm(), 90.0, 250.0, 3000.0, 480);
    const Out on90 = lateral_run(kCp, 90.0, 250.0, 3000.0, 480);

    std::printf("[yawbudget COST] lat40 turn %6.2f -> %6.2f  yaw %7.2f -> "
                "%7.2f\n[yawbudget COST] lat90 turn %6.2f -> %6.2f  yaw "
                "%7.2f -> %7.2f  G %5.2f -> %5.2f\n",
                deg(off40.turn), deg(on40.turn), deg(off40.yaw_mean),
                deg(on40.yaw_mean), deg(off90.turn), deg(on90.turn),
                deg(off90.yaw_mean), deg(on90.yaw_mean), off90.n_pk,
                on90.n_pk);

    // lat 40: BIT-IDENTICAL -- the gate never opens on a tracked turn.
    CHECK(on40.turn == off40.turn);
    CHECK(on40.yaw_mean == off40.yaw_mean);
    CHECK(on40.dalt == off40.dalt);

    // lat 90: the turn must NOT be flattened. Chad ruled OUT capping the
    // bank on 2026-07-06 ("flat turns were too low-G"), and under the
    // gun-director law a HIGHER turn rate toward a 90 deg/s aim is a
    // benefit, not a cost. The ungated form lands at 29.33 -- far below
    // this floor.
    CHECK(deg(on90.turn) > 0.98 * deg(off90.turn));
    // The G is not bled.
    CHECK(on90.n_pk > 0.98 * off90.n_pk);
    // The yaw demand keeps its SIGN (the ungated form flips it to +6.06).
    CHECK(on90.yaw_mean * off90.yaw_mean > 0.0);
}

// ===========================================================================
// THE MAGNITUDE LEGS -- Chad's OWN dives, from the compact fixtures.
//
// test/fixtures/lateral4_ev6.csv and _ev7.csv carry one full-precision SEED
// row plus the recorded mouse for the event (provenance in each file's
// header: flown 2026-09-13 12:51, exe 11:46, kernel v15). Both arms are
// REPLAYED from the same seed, so no recorded truth is needed to compare
// them -- which is what keeps the fixtures at 28 KB / 40 KB.
//
// These are the numbers the dial is FOR. The scripted leg above can only
// show direction; this shows the size.
// ===========================================================================
namespace {
struct Fix {
    std::vector<std::string> names;
    std::vector<std::vector<std::string>> rows;
    int idx(const std::string& n) const {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == n) return int(i);
        throw std::runtime_error("fixture: no column " + n);
    }
    double at(size_t r, const std::string& n) const {
        const std::string& v = rows[r][size_t(idx(n))];
        return v.empty() ? 0.0 : std::atof(v.c_str());
    }
    // ONLY for SEADS_TAPE_OPT_FIELDS (app/feel_tape_fields.h carries the rule
    // and the reason). These fixtures are Chad's OWN dives, recorded
    // 2026-09-13 under kernel v15 -- they cannot carry a column added after
    // the flight, and they can never be re-recorded. Every other column stays
    // a hard throw, which is the point of this reader.
    double at_or(size_t r, const std::string& n, double dflt) const {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == n) return at(r, n);
        return dflt;
    }
};

Fix load_fix(const char* path) {
    Fix f;
    std::FILE* fp = std::fopen(path, "rb");
    REQUIRE(fp != nullptr);
    std::vector<char> line(65536);
    bool hdr = false;
    while (std::fgets(line.data(), int(line.size()), fp)) {
        if (line[0] == '#') continue;
        std::string s(line.data());
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        std::vector<std::string> cell;
        size_t a = 0;
        for (size_t i = 0; i <= s.size(); ++i)
            if (i == s.size() || s[i] == ',') {
                cell.push_back(s.substr(a, i - a));
                a = i + 1;
            }
        if (!hdr) { f.names = cell; hdr = true; }
        else f.rows.push_back(cell);
    }
    std::fclose(fp);
    return f;
}

struct FixRes { double dalt = 0, agl = 1e18, gap_end = 0, turn = 0,
                n_pk = 0, gap_min = 1e18; int nt = 0; };

FixRes fly_fix(const Fix& f, const control::ControllerParams& cp) {
    app::LoopState st;
    // seed from row 0 -- the ONLY row whose state columns are valid
#define SEED_D(name, expr) (expr) = f.at(0, #name);
#define SEED_B(name, expr) (expr) = f.at(0, #name) > 0.5;
#define SEED_I(name, expr) (expr) = int(f.at(0, #name));
#define SEED_E(name, expr) \
    (expr) = control::CaptureState(int(f.at(0, #name)));
#define SEED_OPT_D(name, expr) (expr) = f.at_or(0, #name, 0.0);
#define SEED_OPT_B(name, expr) (expr) = f.at_or(0, #name, 0.0) > 0.5;
#define SEED_OPT_I(name, expr) (expr) = int(f.at_or(0, #name, 0.0));
#define SEED_OPT_E(name, expr) \
    (expr) = control::CaptureState(int(f.at_or(0, #name, 0.0)));
    {
        app::LoopState& S = st;
        SEADS_TAPE_SIM_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
        SEADS_TAPE_INT_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
        SEADS_TAPE_OPT_FIELDS(SEED_OPT_D, SEED_OPT_B, SEED_OPT_I, SEED_OPT_E)
        SEADS_TAPE_APP_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
    }
#undef SEED_D
#undef SEED_B
#undef SEED_I
#undef SEED_E
#undef SEED_OPT_D
#undef SEED_OPT_B
#undef SEED_OPT_I
#undef SEED_OPT_E
    st.curr.orientation = glm::normalize(st.curr.orientation);
    st.aim.q = glm::normalize(st.aim.q);
    st.prev = st.curr;
    harness::MiniCamera cam;
    cam.seed(st.curr);
    const double alt0 = sim::altitude(st.curr.position, kAp);
    FixRes r;
    glm::dvec3 h0{0.0};
    // s_* is the POST-tick state of row 0, so drive from row 1 onward.
    for (size_t i = 1; i < f.rows.size(); ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;  // s_throttle == 1.0 on every row of tape 4
        in.aim_dx = f.at(i, "aim_dx");
        in.aim_dy = f.at(i, "aim_dy");
        in.frame_ticks = int(f.at(i, "frame_ticks"));
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult tr = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        const glm::dvec3 u = sim::local_up(st.curr.position);
        const glm::dvec3 nb = st.curr.orientation * glm::dvec3{0, 0, -1};
        const double th = std::asin(std::clamp(glm::dot(nb, u), -1.0, 1.0));
        const double ae = std::asin(std::clamp(
            glm::dot(glm::normalize(st.aim.forward()), u), -1.0, 1.0));
        r.gap_end = th - ae;
        const double a = sim::altitude(st.curr.position, kAp);
        r.agl = std::min(r.agl, a);
        r.dalt = a - alt0;
        r.n_pk = std::max(r.n_pk, tr.telem.load_factor);
        r.gap_min = std::min(r.gap_min, r.gap_end);
        ++r.nt;
        const glm::dvec3 h = glm::normalize(
            st.curr.velocity - u * glm::dot(st.curr.velocity, u));
        if (i == 1) h0 = h;
        r.turn = std::acos(std::clamp(glm::dot(h0, h), -1.0, 1.0))
                 / (double(i) * kAp.sim_dt);
    }
    return r;
}
}  // namespace

TEST_CASE("S-yawbudget: MAGNITUDE on Chad's ev6 (the -138 m/s dive)") {
    const Fix f = load_fix(SEADS_FIXTURE_DIR "/lateral4_ev6.csv");
    const FixRes off = fly_fix(f, off_arm());
    const FixRes on = fly_fix(f, kCp);
    std::printf("[yawbudget ev6] OFF dAlt %7.1f AGLmin %6.0f gap %7.2f | "
                " ON dAlt %7.1f AGLmin %6.0f gap %7.2f\n",
                off.dalt, off.agl, deg(off.gap_end), on.dalt, on.agl,
                deg(on.gap_end));
    // Measured 2026-09-13: OFF -186.5 m / gap -66.2 deg,
    //                       ON -124.5 m / gap -18.2 deg.
    // Bounds are LOOSE around those, so a retune of an unrelated dial does
    // not false-fail; the SIGN and the SIZE are what is pinned.
    REQUIRE(off.dalt < -150.0);            // the fixture IS the dive
    CHECK(on.dalt > off.dalt + 40.0);      // >= 40 m recovered (measured 62)
    CHECK(on.agl > off.agl + 40.0);
    CHECK(deg(on.gap_end) > deg(off.gap_end) + 30.0);  // measured +48 deg
}

TEST_CASE("S-yawbudget: MAGNITUDE on Chad's ev7 (the -194 m/s dive)") {
    const Fix f = load_fix(SEADS_FIXTURE_DIR "/lateral4_ev7.csv");
    const FixRes off = fly_fix(f, off_arm());
    const FixRes on = fly_fix(f, kCp);
    std::printf("[yawbudget ev7] OFF dAlt %7.1f AGLmin %6.0f gap %7.2f | "
                " ON dAlt %7.1f AGLmin %6.0f gap %7.2f\n",
                off.dalt, off.agl, deg(off.gap_end), on.dalt, on.agl,
                deg(on.gap_end));
    // Measured: OFF -490.2 m / AGLmin 1201, ON -445.5 m / AGLmin 1246.
    // HONEST: 45 m of a 490 m descent. A MITIGATION, not a fix -- the
    // remaining lever is the BANK itself.
    REQUIRE(off.dalt < -400.0);
    CHECK(on.dalt > off.dalt + 25.0);
    CHECK(on.agl > off.agl + 25.0);
}

// ===========================================================================
// THE ONSET LEGS -- seeded ~2 s BEFORE the lateral input, so the ROLL-IN is
// inside the window. The mid-manoeuvre legs above start after the bank is
// established and are structurally blind to what the gate edges do at onset;
// these are what the -5..10 band was derived against.
// ===========================================================================
TEST_CASE("S-yawbudget: ONSET ev6 -- the trim arrests the dumping") {
    const Fix f = load_fix(SEADS_FIXTURE_DIR "/onset_t4_ev6.csv");
    const FixRes off = fly_fix(f, off_arm());
    const FixRes on = fly_fix(f, kCp);
    std::printf("[yawbudget ONSET ev6] OFF dAlt %7.1f AGL %6.0f gap %7.2f "
                "turn %6.2f |  ON dAlt %7.1f AGL %6.0f gap %7.2f turn %6.2f"
                "\n",
                off.dalt, off.agl, deg(off.gap_end), deg(off.turn),
                on.dalt, on.agl, deg(on.gap_end), deg(on.turn));
    // Measured: OFF -439.2 m / AGL 1389 / gap -62.8 / turn 23.67
    //            ON -102.1 m / AGL 1716 / gap +23.9 / turn 38.01
    // 77% of the descent removed AND the turn rate RISES.
    REQUIRE(off.dalt < -350.0);               // the fixture IS the dive
    CHECK(on.dalt > off.dalt + 250.0);        // measured +337 m
    CHECK(on.agl > off.agl + 250.0);
    CHECK(deg(on.gap_end) > 0.0);             // the nose ends ABOVE the aim
    CHECK(deg(on.turn) > deg(off.turn));      // and the turn is not flattened
}

TEST_CASE("S-yawbudget: ONSET ev7 and ev1 -- smaller but the same sign") {
    for (const char* nm : {"onset_t4_ev7.csv", "onset_t4_ev1.csv"}) {
        const Fix f = load_fix((std::string(SEADS_FIXTURE_DIR) + "/" + nm)
                                   .c_str());
        const FixRes off = fly_fix(f, off_arm());
        const FixRes on = fly_fix(f, kCp);
        std::printf("[yawbudget ONSET %s] OFF dAlt %7.1f AGL %6.0f |  ON "
                    "dAlt %7.1f AGL %6.0f\n",
                    nm, off.dalt, off.agl, on.dalt, on.agl);
        // ev7 -719.2 -> -606.3 (AGL 1021 -> 1134); ev1 -530.2 -> -428.9
        // (1567 -> 1669). HONEST: 16-19%, not ev6's 77%.
        REQUIRE(off.dalt < -450.0);
        CHECK(on.dalt > off.dalt + 80.0);
        CHECK(on.agl > off.agl + 80.0);
    }
}

// ===========================================================================

// ===========================================================================
// RED-TEAM P1-3 PINS (2026-09-13 red-team, folded 2026-09-15 for v16).
//
//   (1) the smoothstep band is a RAMP, not a step -- the gate takes
//       intermediate values on Chad's own onset, and collapsing the band to
//       a step (the in-test mutation) removes every intermediate tick;
//   (2) a DIRECT pure-pitch leg: a nose-down push (the split-S entry) and a
//       held pull (the loop) are BIT-IDENTICAL with the dial on and off and
//       the recorded scale is 1.0 on every tick. AT-15 was a weak witness
//       because it drives control::step open-loop on a fixed state and
//       spends most of its ticks in push_mode; this drives the real
//       app::tick closed-loop with the dial live.
// ===========================================================================
namespace {
struct ScaleStats {
    int armed = 0, partial = 0, ticks = 0, armed_lowphi = 0;
    double sc_min = 1.0, jump_max = 0.0, prev = 1.0;
};

// Same as fly_fix but reads telem.yaw_budget_scale per tick.
ScaleStats fix_scale(const Fix& f, const control::ControllerParams& cp) {
    app::LoopState st;
#define SEED_D(name, expr) (expr) = f.at(0, #name);
#define SEED_B(name, expr) (expr) = f.at(0, #name) > 0.5;
#define SEED_I(name, expr) (expr) = int(f.at(0, #name));
#define SEED_E(name, expr) \
    (expr) = control::CaptureState(int(f.at(0, #name)));
#define SEED_OPT_D(name, expr) (expr) = f.at_or(0, #name, 0.0);
#define SEED_OPT_B(name, expr) (expr) = f.at_or(0, #name, 0.0) > 0.5;
#define SEED_OPT_I(name, expr) (expr) = int(f.at_or(0, #name, 0.0));
#define SEED_OPT_E(name, expr) \
    (expr) = control::CaptureState(int(f.at_or(0, #name, 0.0)));
    {
        app::LoopState& S = st;
        SEADS_TAPE_SIM_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
        SEADS_TAPE_INT_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
        SEADS_TAPE_OPT_FIELDS(SEED_OPT_D, SEED_OPT_B, SEED_OPT_I, SEED_OPT_E)
        SEADS_TAPE_APP_FIELDS(SEED_D, SEED_B, SEED_I, SEED_E)
    }
#undef SEED_D
#undef SEED_B
#undef SEED_I
#undef SEED_E
#undef SEED_OPT_D
#undef SEED_OPT_B
#undef SEED_OPT_I
#undef SEED_OPT_E
    st.curr.orientation = glm::normalize(st.curr.orientation);
    st.aim.q = glm::normalize(st.aim.q);
    st.prev = st.curr;
    harness::MiniCamera cam;
    cam.seed(st.curr);
    ScaleStats s;
    for (size_t i = 1; i < f.rows.size(); ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;
        in.aim_dx = f.at(i, "aim_dx");
        in.aim_dy = f.at(i, "aim_dy");
        in.frame_ticks = int(f.at(i, "frame_ticks"));
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult tr = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        const double sc = tr.telem.yaw_budget_scale;
        ++s.ticks;
        s.sc_min = std::min(s.sc_min, sc);
        if (sc < 1.0 - 1e-12) ++s.armed;
        // "partial" = the GATE is between its edges. The scale itself is
        // 1 - gate*(1 - budget/dig); with the budget clipped to 0 (the
        // AoA ceiling binding, the common case on these dives) the scale
        // IS 1 - gate, so a scale strictly inside (0, 1) is a gate strictly
        // inside (0, 1). With budget > 0 the same reading is conservative.
        if (sc > 0.02 && sc < 0.98) ++s.partial;
        s.jump_max = std::max(s.jump_max, std::abs(sc - s.prev));
        s.prev = sc;
    }
    return s;
}

// A scripted PURE-PITCH input from level trim (aim_dx identically 0):
// dy > 0 pushes the aim DOWN (the split-S entry), dy < 0 pulls it UP (the
// loop). Returns the lateral_run Out plus the per-tick scale stats.
struct VOut { Out o; ScaleStats s; };
VOut vertical_run(const control::ControllerParams& cp, double dy_deg_s,
                  double V, double alt, int ticks, int ticks_drive) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 1.0;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, alt, up, heading, &thr);
    app::LoopState st;
    st.curr = s0;
    st.prev = s0;
    st.prev_up = sim::local_up(s0.position);
    st.aim.reseed(s0.orientation, st.prev_up);
    st.internal = control::reset();
    st.grounded = false;
    harness::MiniCamera cam;
    cam.seed(st.curr);
    VOut r;
    const double alt0 = sim::altitude(st.curr.position, kAp);
    double ysum = 0;
    for (int i = 0; i < ticks; ++i) {
        app::TickInput in;
        in.raw_mode = false;
        in.throttle = 1.0;
        in.aim_dx = 0.0;
        // drive for ticks_drive, then HOLD (dy = 0) so the nose catches
        // the aim -- a split-S is "push 180 deg, then fly it", not a
        // 240 deg push that laps the airframe.
        in.aim_dy = (i < ticks_drive)
                        ? rad(dy_deg_s) * kAp.sim_dt / cp.aim_sensitivity
                        : 0.0;
        in.cam_fwd = cam.cam_fwd;
        in.cam_up = cam.cam_up;
        const app::TickResult tr = app::tick(st, in, kAp, cp, nullptr);
        cam.advance(st.curr, st.aim.forward(), st.aim.up(), cp, kAp.sim_dt);
        const glm::dvec3 u = sim::local_up(st.curr.position);
        const glm::dvec3 nb = st.curr.orientation * glm::dvec3{0, 0, -1};
        const double th = std::asin(std::clamp(glm::dot(nb, u), -1.0, 1.0));
        const double ae = std::asin(std::clamp(
            glm::dot(glm::normalize(st.aim.forward()), u), -1.0, 1.0));
        r.o.gap_end = th - ae;
        const double a = sim::altitude(st.curr.position, kAp);
        r.o.agl = std::min(r.o.agl, a);
        r.o.dalt = a - alt0;
        r.o.n_pk = std::max(r.o.n_pk, tr.telem.load_factor);
        ysum += tr.telem.omega_des.y;
        r.o.v_end = glm::length(st.curr.velocity);
        ++r.o.ticks;
        const double sc = tr.telem.yaw_budget_scale;
        ++r.s.ticks;
        r.s.sc_min = std::min(r.s.sc_min, sc);
        if (sc < 1.0 - 1e-12) {
            ++r.s.armed;
            if (std::abs(std::sin(tr.telem.extracted.phi)) < 0.2)
                ++r.s.armed_lowphi;
        }
    }
    r.o.yaw_mean = ysum / r.o.ticks;
    return r;
}
}  // namespace

TEST_CASE("S-yawbudget P1-3: the gate is a RAMP, not a step") {
    const Fix f = load_fix(SEADS_FIXTURE_DIR "/onset_t4_ev6.csv");
    const ScaleStats on = fix_scale(f, kCp);
    // THE MUTATION, run live: collapse the band to a step one ulp wide. The
    // loader refuses gap_lo >= gap_hi, so this is the narrowest band a toml
    // could carry -- and it is a step in everything but name.
    control::ControllerParams step = kCp;
    step.yaw_vert_gap_lo = kCp.yaw_vert_gap_hi - 1e-9;
    const ScaleStats st = fix_scale(f, step);
    std::printf("[yawbudget P1-3 ramp] shipped band: armed %d/%d sc_min %.3f "
                "max tick-jump %.3f | step band: armed %d max tick-jump %.3f\n",
                on.armed, on.ticks, on.sc_min, on.jump_max, st.armed,
                st.jump_max);
    REQUIRE(on.armed > 0);                       // premise: the dial fires
    REQUIRE(st.armed > 0);                       // the step arms too
    // HONEST LIMIT (red-team P2-2): the GATE value is not observable from
    // outside the kernel -- the recorded scale also carries budget/dig,
    // which is itself binary where the elevator clips (measured max
    // tick-jump 0.90 on the shipped band vs 0.97 on the step), so neither
    // "interior ticks" nor per-tick continuity can witness the ramp. What
    // CAN be pinned: the band's edges are what the loader accepted
    // (-5..10 deg, the "shipped dial is ON" case), the loader refuses a
    // collapsed band (gap_lo >= gap_hi), and a step band flies a DIFFERENT
    // trajectory on Chad's onset (the shape matters, not just the edges).
    const FixRes a = fly_fix(f, kCp);
    const FixRes b = fly_fix(f, step);
    std::printf("[yawbudget P1-3 ramp] dAlt shipped %7.1f  step %7.1f\n",
                a.dalt, b.dalt);
    CHECK(a.dalt != b.dalt);
    CHECK(on.sc_min < 0.5);   // and the shipped dial DOES bite on this dive
}

TEST_CASE("S-yawbudget P1-3: pure PITCH is untouched -- push and pull "
          "bit-identical, scale 1.0 every tick") {
    // dy +60 deg/s for 4 s = the aim driven 240 deg DOWN through the
    // vertical (the split-S entry, push_mode then the roll-through);
    // dy -90 deg/s for 4 s = a full 360 deg pull (the loop).
    for (const double dy : {90.0, -90.0}) {
        // push: 90 deg/s for 2 s (180 deg, the split-S) then HOLD 2 s;
        // pull: 90 deg/s for 4 s (a full 360 deg loop under a held pull).
        const int drive = dy > 0.0 ? 240 : 480;
        const VOut off = vertical_run(off_arm(), dy, 250.0, 3000.0, 480, drive);
        const VOut on = vertical_run(kCp, dy, 250.0, 3000.0, 480, drive);
        std::printf("[yawbudget P1-3 pitch dy %+5.1f] OFF dAlt %8.1f gap "
                    "%7.2f G %5.2f | ON dAlt %8.1f  armed %d/%d sc_min %.3f\n",
                    dy, off.o.dalt, deg(off.o.gap_end), off.o.n_pk, on.o.dalt,
                    on.s.armed, on.s.ticks, on.s.sc_min);
        // premise: the manoeuvre happened (a loop or a split-S moves the
        // aeroplane hundreds of metres vertically at V250)
        REQUIRE(std::abs(off.o.dalt) > 100.0);
        if (dy < 0.0) {
            // THE LOOP: phi stays +/-0 and there is no rudder, so the dial
            // never has a dig to trim. BIT-IDENTICAL, scale 1.0 every tick.
            // (Before the roundoff floor in the kernel block this leg read
            // "armed 75/480" on yv ~ -1e-17 rad/s: a lying instrument, not
            // a lying aeroplane. The floor is what makes this pin honest.)
            CHECK(on.s.armed == 0);
            CHECK(on.s.sc_min == 1.0);
            CHECK(on.o.dalt == off.o.dalt);
            CHECK(on.o.gap_end == off.o.gap_end);
            CHECK(on.o.yaw_mean == off.o.yaw_mean);
            CHECK(on.o.n_pk == off.o.n_pk);
            CHECK(on.o.v_end == off.o.v_end);
        } else {
            // THE SPLIT-S is NOT a pure-pitch manoeuvre once the aim is
            // aft: the roll-through banks the airframe with the nose below
            // an aft-level aim and the rudder digging. Before the 1 deg/s
            // budget floor this leg armed on 61 ticks, 54 of them WINGS-
            // LEVEL (|phi| ~ 0.5 deg, dig 0.2 deg/s) with 27-30 deg/s of
            // rudder removed for nothing (red-team P1-1). With the floor:
            // 7 ticks, all banked, dAlt -579.6 -> -580.9 m. Pinned: no
            // armed tick at |sin phi| < 0.2, <= 5% of ticks, shape kept.
            REQUIRE(on.s.armed > 0);                 // it IS reached here
            CHECK(on.s.armed_lowphi == 0);
            CHECK(on.s.armed * 20 <= on.s.ticks);
            CHECK(std::abs(on.o.dalt - off.o.dalt) < 5.0);        // metres
            CHECK(std::abs(deg(on.o.gap_end) - deg(off.o.gap_end)) < 2.0);
            CHECK(std::abs(on.o.n_pk - off.o.n_pk) < 0.5);
            CHECK(std::abs(on.o.v_end - off.o.v_end) < 2.0);
        }
    }
}

// ===========================================================================
// THE RULING'S RECORD (red-team P2-1). These three fixtures were committed
// for the S-unload legs; S-unload was REMOVED at Chad's ruling ("no deck
// save unload keep the yaw budget"). They stay, pinned for what the budget
// ALONE does on them -- including the one it does NOT save.
// ===========================================================================
TEST_CASE("S-yawbudget: what the ruling gave up -- t5worst, t6deck, t6worst") {
    struct Row { const char* nm; double off_lt; double gain_lo, gain_hi; };
    // measured 2026-09-15 (budget floor in): t5worst -582.2 -> -527.1;
    // t6_worst -558.2 -> -554.2; t6_deck AGL 1 -> 0 on BOTH arms.
    for (const Row& r : {Row{"onset_t5_worst.csv", -500.0, 20.0, 120.0},
                         Row{"onset_t6_worst.csv", -500.0, -10.0, 40.0}}) {
        const Fix f = load_fix((std::string(SEADS_FIXTURE_DIR) + "/" + r.nm)
                                   .c_str());
        const FixRes off = fly_fix(f, off_arm());
        const FixRes on = fly_fix(f, kCp);
        std::printf("[yawbudget RULING %s] OFF dAlt %7.1f AGL %6.0f | ON dAlt "
                    "%7.1f AGL %6.0f\n", r.nm, off.dalt, off.agl, on.dalt,
                    on.agl);
        REQUIRE(off.dalt < r.off_lt);
        CHECK(on.dalt - off.dalt > r.gain_lo);
        CHECK(on.dalt - off.dalt < r.gain_hi);
    }
    {
        const Fix f = load_fix(SEADS_FIXTURE_DIR "/onset_t6_deck.csv");
        const FixRes off = fly_fix(f, off_arm());
        const FixRes on = fly_fix(f, kCp);
        std::printf("[yawbudget RULING t6_deck] OFF AGLmin %6.1f | ON AGLmin "
                    "%6.1f  (the budget does NOT save the deck)\n", off.agl,
                    on.agl);
        // The unload saved this event (AGL 0 -> 80). The budget does not.
        // This is the cost of the ruling, kept visible.
        CHECK(off.agl < 5.0);
        CHECK(on.agl < 5.0);
    }
}
