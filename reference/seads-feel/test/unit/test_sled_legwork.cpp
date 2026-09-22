// SLED KERNEL N1 -- THE LEG WORK (Chad, drive runs 5 and 7, 2026-09-18).
//
//   run 5: "designate ctrl when not rolled on side, but stuck in bank upside
//           down nose down vertical or nose up on track, rider attached can use
//           legs to roll it over backward and on its side where it can then be
//           weight shift mounted."
//   run 7: "that crouch mechanic built where sudburian can pull it over and on
//           its side and then use shift to re right it for when vertically
//           static in snow, also when fully upside down to extend legs with
//           shift would put the sled up first then falling over on its side is
//           the stage that another press of the shift can right you. TO make
//           the r autoright key fully redundant."
//
// The ladder, every rung press-gated and one-way (sim/sled.h leg_work_nm):
//   PITCHED  (nose-down / nose-up, not on a side) + CTRL  -> falls onto a side
//   INVERTED (on its back)                        + SHIFT -> up on its end,
//                                                            then onto a side
//   ON_SIDE                                       + SHIFT -> the v2 pendulum,
//                                                            byte-untouched
// R stays bound (app/main.cpp KEY_R, untouched: the tripwire in the landing
// doc); the ladder is what makes it unnecessary.
//
// ★ SAME LAW AS test_sled_selfright.cpp: every OUTCOME leg reads the SHIPPED
// config/scenario.toml through `shipped()`, never a test-local number, so a
// leg cannot certify a torque nobody ships. The IDENTITY legs set the dial to
// 0.0 on a copy of that same table.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config/load_aircraft.h"
#include "config/load_scenario.h"
#include "sim/sled.h"
#include "test/harness/sled_tape.h"  // SLEDTAPE_PIN_D: the SledState roster
#include "world/snowpack.h"

namespace {

const glm::dvec3 kDir = glm::normalize(glm::dvec3(1.0, 1.0, 1.0));
const double kDeg = 3.14159265358979323846 / 180.0;
const double kDt = 1.0 / 120.0;

world::HeightField flat_field() {
    world::HeightField hf;
    hf.w = 64;
    hf.h = 32;
    hf.R = 6'371'000.0;
    hf.relief_scale = 400.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    return hf;
}

const world::HeightField& shared_hf() {
    static const world::HeightField hf = flat_field();
    return hf;
}

world::SnowpackField field(double base_m = 0.5) {
    world::SnowpackField f;
    f.hf = &shared_hf();
    f.p.base_m = base_m;
    f.p.curv_gain = 0.0;
    f.p.drain_gain = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.slope_shed = 0.0;
    f.p.depth_max_m = 5.0;
    return f;
}

// The shipped table, read from the SAME toml the game reads.
const sim::SledParams& shipped() {
    static const sim::SledParams p = [] {
        sim::SledParams q;
        const sim::AircraftParams ap =
            cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
        q.comfort =
            cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", ap)
                .sled_comfort;
        return q;
    }();
    return p;
}

// The body frame at a generic point: fwd tangent, up radial, right = fwd x up.
struct Frame {
    glm::dvec3 up, fwd, right;
};

Frame frame_at(const glm::dvec3& up_in) {
    Frame fr;
    fr.up = glm::normalize(up_in);
    glm::dvec3 fwd(0.0, 0.0, 1.0);
    fr.fwd = glm::normalize(fwd - glm::dot(fwd, fr.up) * fr.up);
    fr.right = glm::cross(fr.fwd, fr.up);
    return fr;
}

// Body axes for a machine PITCHED by pitch_rad (+ = nose DOWN) and then ROLLED
// by roll_rad about its own forward axis. pitch 0 / roll 0 = upright.
glm::dmat3 posed_axes(double pitch_rad, double roll_rad) {
    const Frame fr = frame_at(kDir);
    // pitch about the RIGHT axis: nose down tips fwd toward -up, up toward fwd
    const glm::dvec3 f1 =
        std::cos(pitch_rad) * fr.fwd - std::sin(pitch_rad) * fr.up;
    const glm::dvec3 u1 =
        std::cos(pitch_rad) * fr.up + std::sin(pitch_rad) * fr.fwd;
    const glm::dvec3 r1 = glm::cross(f1, u1);
    // roll about the (pitched) forward axis
    const glm::dvec3 u2 = std::cos(roll_rad) * u1 + std::sin(roll_rad) * r1;
    const glm::dvec3 r2 = glm::cross(f1, u2);
    glm::dmat3 m;
    m[0] = r2;
    m[1] = u2;
    m[2] = -f1;
    return m;
}

// The lowest body point of the coarse hull the kernel rests a downed machine
// on (sim/sled.cpp C1 pts[10]) plus the three patch mounts, so a posed
// machine is spawned CLEAR of the floor at every attitude instead of buried
// 0.86 m deep nose-first.
double lowest_offset_m(const sim::SledParams& p, const glm::dmat3& R,
                       const glm::dvec3& up) {
    const double half = 0.5 * p.stance_m;
    const double my = -(p.cg_height_m - p.susp_rest_m);
    const glm::dvec3 pts[13] = {
        {0.32, 0.45, p.track_aft_m},         {-0.32, 0.45, p.track_aft_m},
        {0.38, 0.55, -0.30},                 {-0.38, 0.55, -0.30},
        {half + 0.12, my, -p.ski_fwd_m},     {-(half + 0.12), my, -p.ski_fwd_m},
        {0.58, my + 0.12, -0.9},             {-0.58, my + 0.12, -0.9},
        {0.58, my + 0.12, 0.7},              {-0.58, my + 0.12, 0.7},
        {half, -p.cg_height_m, -p.ski_fwd_m}, {-half, -p.cg_height_m, -p.ski_fwd_m},
        {0.0, -p.cg_height_m, p.track_aft_m},
    };
    double lo = 0.0;
    for (const glm::dvec3& pt : pts) lo = std::min(lo, glm::dot(R * pt, up));
    return lo;
}

sim::SledState posed_raw(const sim::SledParams& p,
                         const world::SnowpackField& f, double pitch_rad,
                         double roll_rad) {
    sim::SledState s;
    const glm::dmat3 m = posed_axes(pitch_rad, roll_rad);
    s.orientation = glm::normalize(glm::quat_cast(m));
    const double lo = lowest_offset_m(p, m, kDir);
    s.position = kDir * (f.drive_radius_at(kDir) - lo + 0.05);
    return s;
}

glm::dvec3 up_body_of(const sim::SledState& s) {
    const glm::dmat3 R = glm::mat3_cast(s.orientation);
    return glm::transpose(R) * glm::normalize(s.position);
}

double tilt_of(const sim::SledState& s) {
    return std::acos(std::clamp(up_body_of(s).y, -1.0, 1.0));
}

double ground_speed_of(const sim::SledState& s) {
    const glm::dvec3 up = glm::normalize(s.position);
    return glm::length(s.velocity - glm::dot(s.velocity, up) * up);
}

// Idle until the pose has landed: the same 3 s test_sled_selfright.cpp uses.
sim::SledState settled_pose(const sim::SledParams& p,
                            const world::SnowpackField& f, double pitch_rad,
                            double roll_rad, int ticks = 360) {
    sim::SledState s = posed_raw(p, f, pitch_rad, roll_rad);
    const sim::SledInputs idle;
    for (int i = 0; i < ticks; ++i) s = sim::step_sled(s, idle, p, f, kDt);
    return s;
}

// The attitude bands, in the kernel's own terms (|up_body.z| pitched,
// |up_body.x| on a side, up_body.y inverted). Test-local readouts.
bool on_end(const sim::SledState& s) {
    return std::abs(up_body_of(s).z) >= std::sin(50.0 * kDeg);
}
bool on_side(const sim::SledState& s) {
    return std::abs(up_body_of(s).x) >= std::sin(50.0 * kDeg);
}
bool inverted(const sim::SledState& s) {
    return up_body_of(s).y <= std::cos(150.0 * kDeg);
}
bool upright(const sim::SledState& s, const sim::SledParams& p) {
    return tilt_of(s) < p.comfort.right_tilt_lo_rad;
}

// A scripted press cadence on one key (`key` = -1 CTRL, +1 SHIFT), on_s held /
// off_s released, until `done(s)` or total_s. Reports what happened.
struct StageResult {
    double t_done = -1.0;   // seconds until done(); -1 never
    int presses = 0;        // presses BEGUN before done()
    bool grip_held = true;  // s.grip.attached never went false
    bool rolled_seen = false;
    double air_peak_s = 0.0;
    double omega_peak = 0.0;  // rad/s, any axis
    double gs_peak = 0.0;
    double tilt_final = 0.0;
    sim::SledState s;
};

template <class Done>
StageResult press_until(const sim::SledParams& p, const world::SnowpackField& f,
                        sim::SledState s, int key, double on_s, double off_s,
                        double total_s, Done done) {
    StageResult r;
    sim::SledInputs in;
    const int n = static_cast<int>(total_s / kDt);
    const double period = on_s + off_s;
    bool was_on = false;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * kDt;
        const bool on = std::fmod(t, period) < on_s;
        if (on && !was_on) ++r.presses;
        was_on = on;
        in.stand = on ? static_cast<float>(key) : 0.0f;
        s = sim::step_sled(s, in, p, f, kDt);
        if (!s.grip.attached) r.grip_held = false;
        if (s.rolled) r.rolled_seen = true;
        r.air_peak_s = std::max(r.air_peak_s, s.air_s);
        r.omega_peak = std::max(r.omega_peak, glm::length(s.angular_vel));
        r.gs_peak = std::max(r.gs_peak, ground_speed_of(s));
        if (r.t_done < 0.0 && done(s)) {
            r.t_done = t + kDt;
            break;
        }
    }
    r.tilt_final = tilt_of(s);
    r.s = s;
    return r;
}

#define SEADS_N1_CMP(f) \
    if (!(a.f == b.f)) return false;
bool same_pin(const sim::SledState& a, const sim::SledState& b) {
    SLEDTAPE_PIN_D(SEADS_N1_CMP)
    if (a.surface != b.surface) return false;
    if (a.rolled != b.rolled) return false;
    return true;
}
#undef SEADS_N1_CMP

void print_state(const char* tag, const sim::SledState& s) {
    std::printf("[N1 %s] pos %.17g %.17g %.17g  vel %.17g %.17g %.17g  "
                "q %.17g %.17g %.17g %.17g  w %.17g %.17g %.17g  tilt %.2f\n",
                tag, s.position.x, s.position.y, s.position.z, s.velocity.x,
                s.velocity.y, s.velocity.z, s.orientation.w, s.orientation.x,
                s.orientation.y, s.orientation.z, s.angular_vel.x,
                s.angular_vel.y, s.angular_vel.z, tilt_of(s) / kDeg);
}

}  // namespace


namespace {

// Identity arm: the shipped table with ONLY this dial at its identity.
sim::SledParams identity() {
    sim::SledParams p = shipped();
    p.comfort.leg_work_nm = 0.0;
    return p;
}

// A flat, upright, moving machine on the same field (for the "never while
// riding" legs). fwd is the frame's tangent at kDir.
sim::SledState moving_raw(const sim::SledParams& p, const world::SnowpackField& f,
                          double speed_ms, double up_ms = 0.0) {
    sim::SledState s = posed_raw(p, f, 0.0, 0.0);
    const Frame fr = frame_at(kDir);
    s.velocity = fr.fwd * speed_ms + fr.up * up_ms;
    return s;
}

// Step two arms (dial A vs dial B) in lockstep on the SAME input script and
// return the first tick at which the pinned state differs (-1 = never).
template <class Script>
int first_divergence(const sim::SledParams& pa, const sim::SledParams& pb,
                     const world::SnowpackField& f, sim::SledState a,
                     sim::SledState b, int ticks, Script script,
                     sim::SledState* out_a = nullptr) {
    for (int i = 0; i < ticks; ++i) {
        const sim::SledInputs in = script(i);
        a = sim::step_sled(a, in, pa, f, kDt);
        b = sim::step_sled(b, in, pb, f, kDt);
        if (!same_pin(a, b)) return i;
    }
    if (out_a) *out_a = a;
    return -1;
}

}  // namespace

// ---------------------------------------------------------------------------
// PRECONDITION (the recon's §0.4 risk): is a nose-down / nose-up vertical
// machine a REST STATE of this kernel at all? The hull (sim/sled.cpp C1) only
// engages on surface-relative ROLL > 25 deg, so a pitched-vertical machine
// stands on patch springs and the penetration rail alone. Measured, not
// assumed: idle 3 s, print the attitude every 0.5 s, then pin the band each
// pose ENDS in. MEASURED on the pre-edit build (flat Bush 0.5 / 1.0 / 2.0 m,
// the hidden "legwork rest sweep" probe): nose-UP 70..90 deg RESTS on its
// tail at tilt 105.5 deg, |up_body.z| 0.96 -- his "nose up on track", the
// PITCHED fixture; INVERTED rests at 173.5 deg; nose-DOWN 70..90 deg is NOT
// a rest pose on flat snow -- it falls onto its back in 0.7-1.5 s and IS the
// INVERTED case (a nose held by a BANK is his real nose-down case and has no
// flat fixture; the PITCHED band covers it with the same expression). Pinned
// as measured, not faked.
TEST_CASE("legwork_rest_pose_holds_before_any_press", "[sled][n1][legwork]") {
    const sim::SledParams& p = shipped();
    const world::SnowpackField f = field(0.5);
    struct Pose {
        const char* name;
        double pitch_deg, roll_deg;
        int ends_in;  // sim::LegStage band the pose rests in after 3 s
    };
    const Pose poses[3] = {{"nose-down 90", 90.0, 0.0, sim::kLegInverted},
                           {"nose-up 90", -90.0, 0.0, sim::kLegPitched},
                           {"inverted 180", 0.0, 180.0, sim::kLegInverted}};
    for (const Pose& po : poses) {
        sim::SledState s = posed_raw(p, f, po.pitch_deg * kDeg, po.roll_deg * kDeg);
        const sim::SledInputs idle;
        std::printf("[N1 rest %s]", po.name);
        for (int i = 0; i < 360; ++i) {
            s = sim::step_sled(s, idle, p, f, kDt);
            if ((i + 1) % 60 == 0) {
                const glm::dvec3 u = up_body_of(s);
                std::printf("  t%.1f tilt %.1f ub(%.2f,%.2f,%.2f) air %.2f%s",
                            (i + 1) * kDt, tilt_of(s) / kDeg, u.x, u.y, u.z,
                            s.air_s, s.rolled ? " R" : "");
            }
        }
        std::printf("\n");
        const glm::dvec3 u = up_body_of(s);
        CHECK(s.grip.attached);
        CHECK(s.air_s <= p.comfort.rolled_grace_s);  // it is on the ground
        CHECK(std::abs(u.x) < std::sin(35.0 * kDeg));  // never on a side
        if (po.ends_in == sim::kLegInverted) {
            CHECK(u.y <= std::cos(150.0 * kDeg));  // INVERTED band
        } else {
            CHECK(std::abs(u.z) >= std::sin(50.0 * kDeg));  // PITCHED band
            CHECK(u.z < 0.0);  // nose UP is -up_body.z (measured below)
        }
        // And with the dial SHIPPED the stage is ARMED and nothing moved: the
        // same idle 3 s leaves it in that band with the stage readout set.
        CHECK(s.leg_stage == po.ends_in);
        CHECK(s.leg_nm_now == 0.0);
    }
}

// The sweep behind the precondition: over snow depth x pitch, how long does a
// pitched-vertical machine hold its band before gravity takes it? Hidden
// probe; the numbers it printed are in docs/SLED_KERNEL_N1_LEGWORK.md.
TEST_CASE("legwork rest sweep", "[.probe]") {
    const sim::SledParams& p = shipped();
    std::printf("\n depth pitch | t_leave_band  tilt@3s  ub.z@3s  ub.y@3s  "
                "ub.x@3s\n");
    for (double depth : {0.5, 1.0, 2.0}) {
        const world::SnowpackField f = field(depth);
        for (double pitch : {90.0, 80.0, 70.0, 60.0, -60.0, -70.0, -80.0, -90.0}) {
            sim::SledState s = posed_raw(p, f, pitch * kDeg, 0.0);
            const sim::SledInputs idle;
            double t_leave = -1.0;
            for (int i = 0; i < 360; ++i) {
                s = sim::step_sled(s, idle, p, f, kDt);
                if (t_leave < 0.0 && !on_end(s)) t_leave = (i + 1) * kDt;
            }
            const glm::dvec3 u = up_body_of(s);
            std::printf(" %4.1f %6.1f | %12.2f %8.1f %8.2f %8.2f %8.2f\n", depth,
                        pitch, t_leave, tilt_of(s) / kDeg, u.z, u.y, u.x);
        }
    }
}

// The sign law (sim/sled.h: a sign is MEASURED by test, never typed). Body X
// is the pitch axis; which way does +angular_vel.x take the nose? Set the
// rate on a settled upright machine and read the nose's radial component one
// step later. Everything in the leg block that steers a pitch is written
// against THIS measurement.
TEST_CASE("legwork_pitch_sign_is_measured", "[sled][n1][legwork]") {
    const sim::SledParams& p = shipped();
    const world::SnowpackField f = field(0.5);
    sim::SledState s = settled_pose(p, f, 0.0, 0.0);
    const glm::dvec3 up = glm::normalize(s.position);
    auto nose_rise = [&](const sim::SledState& q) {
        const glm::dmat3 R = glm::mat3_cast(q.orientation);
        return glm::dot(R * glm::dvec3(0.0, 0.0, -1.0), up);
    };
    const double before = nose_rise(s);
    s.angular_vel = glm::dvec3(0.5, 0.0, 0.0);
    const sim::SledInputs idle;
    s = sim::step_sled(s, idle, p, f, kDt);
    const double after = nose_rise(s);
    std::printf("[N1 pitch sign] nose radial %.6f -> %.6f under +0.5 rad/s "
                "about body X\n", before, after);
    REQUIRE(after > before);  // +angular_vel.x = nose UP
    // and the readout the stages key on: nose DOWN is +up_body.z
    const sim::SledState nd = posed_raw(p, f, 60.0 * kDeg, 0.0);
    REQUIRE(up_body_of(nd).z > 0.5);
    const sim::SledState nu = posed_raw(p, f, -60.0 * kDeg, 0.0);
    REQUIRE(up_body_of(nu).z < -0.5);
}

// The golden printer for the identity leg: the three stage fixtures under the
// scripted cadences, final state at 17 digits. Run on the PRE-EDIT build to
// record; the identity leg below pins what it printed.
TEST_CASE("legwork golden printer", "[.probe]") {
    const sim::SledParams p = identity();
    const world::SnowpackField f = field(0.5);
    auto never = [](const sim::SledState&) { return false; };
    {
        const sim::SledState s0 = settled_pose(p, f, 90.0 * kDeg, 0.0);
        const StageResult r = press_until(p, f, s0, -1, 1.0, 0.7, 6.0, never);
        print_state("golden nose-down CTRL", r.s);
    }
    {
        const sim::SledState s0 = settled_pose(p, f, -90.0 * kDeg, 0.0);
        const StageResult r = press_until(p, f, s0, -1, 1.0, 0.7, 6.0, never);
        print_state("golden nose-up CTRL", r.s);
    }
    {
        const sim::SledState s0 = settled_pose(p, f, 0.0, 180.0 * kDeg);
        const StageResult r = press_until(p, f, s0, +1, 1.0, 0.7, 6.0, never);
        print_state("golden inverted SHIFT", r.s);
    }
}

// ---------------------------------------------------------------------------
// THE IDENTITY. leg_work_nm = 0.0 on the shipped table is the PRE-EDIT kernel
// bit for bit on every stage fixture under the scripted presses: the goldens
// below were printed by "legwork golden printer" on the build BEFORE the leg
// block existed (lane tip 301a48c37 + the N1 constants only, 2026-09-22) and
// are compared with == at 17 digits. KILLED BY: any write outside the
// `leg_work_nm > 0.0` branch, or any read of the new state on the torque path.
TEST_CASE("legwork_identity_zero_is_bit_exact_on_every_stage_fixture",
          "[sled][n1][legwork]") {
    const sim::SledParams p = identity();
    const world::SnowpackField f = field(0.5);
    auto never = [](const sim::SledState&) { return false; };
    struct Golden {
        const char* name;
        double pitch_deg, roll_deg;
        int key;
        double pos[3], vel[3], q[4], w[3];
    };
    const Golden g[3] = {
        {"nose-down CTRL", 90.0, 0.0, -1,
         {3678298.7389791072, 3678298.7390284152, 3678299.9631969314},
         {-0.00031952825859103695, 0.0023226865530599225, 0.00077159886624527415},
         {0.35755074859336594, -0.13640261650655305, -0.32930008283267914,
          0.86319942298224506},
         {0.00034705549124358648, 1.4954514794094471e-05, -0.0067634524821750825}},
        {"nose-up CTRL", -90.0, 0.0, -1,
         {3678299.2687780871, 3678299.2687759358, 3678299.0572449588},
         {0.0020256724948841433, 0.00099345017223695527, -0.0025981536967300957},
         {-0.21902991953339945, 0.31380139820869429, 0.75758959892364475,
          -0.52878405463305345},
         {0.005388764910527896, -0.00021073336404295697, 0.0089170975095718436}},
        {"inverted SHIFT", 0.0, 180.0, +1,
         {3678297.9555506543, 3678300.2336918307, 3678298.7518427712},
         {0.00080220946837912738, 0.001154827622902457, 0.0030503807003511379},
         {0.21107239480337825, 0.39918562653008094, 0.86506235562306888,
          0.21855525756114966},
         {-0.0040565397985987831, 6.5646009687280485e-06, -0.0010691155793042027}},
    };
    for (const Golden& gg : g) {
        INFO(gg.name);
        const sim::SledState s0 =
            settled_pose(p, f, gg.pitch_deg * kDeg, gg.roll_deg * kDeg);
        const StageResult r =
            press_until(p, f, s0, gg.key, 1.0, 0.7, 6.0, never);
        const sim::SledState& s = r.s;
        CHECK(s.position.x == gg.pos[0]);
        CHECK(s.position.y == gg.pos[1]);
        CHECK(s.position.z == gg.pos[2]);
        CHECK(s.velocity.x == gg.vel[0]);
        CHECK(s.velocity.y == gg.vel[1]);
        CHECK(s.velocity.z == gg.vel[2]);
        CHECK(s.orientation.w == gg.q[0]);
        CHECK(s.orientation.x == gg.q[1]);
        CHECK(s.orientation.y == gg.q[2]);
        CHECK(s.orientation.z == gg.q[3]);
        CHECK(s.angular_vel.x == gg.w[0]);
        CHECK(s.angular_vel.y == gg.w[1]);
        CHECK(s.angular_vel.z == gg.w[2]);
        // and at the identity no leg state is ever written
        CHECK(s.leg_stage == 0);
        CHECK(s.leg_charge == 1.0);
        CHECK(s.leg_prev_stand == 0.0f);
        CHECK(s.leg_nm_now == 0.0);
    }
}

// ---------------------------------------------------------------------------
// STAGE 1 -- PITCHED + CTRL. From the nose-up-on-the-tail rest pose (tilt
// 105.5 deg, his "nose up on track"), the CTRL cadence (1.0 s held / 0.7 s
// off, the same rhythm the pendulum legs use) at the SHIPPED budget rolls the
// machine over BACKWARD off its end within the bound -- onto a side (then
// SHIFT is the pendulum's), onto its back (then SHIFT is stage 2), or, for a
// nose-down machine, back onto its track. MEASURED (the hidden ladder probe):
// the rest is a stable two-contact pose; 1500 never leaves it, 1800 leaves in
// 1.22 s, the shipped 2400 in 0.73 s on ONE press. The rider stays welded. At the identity the same cadence
// never leaves the PITCHED band (the positive control: today CTRL is only a
// tuck, pinned by "selfright: seated and tucked do nothing").
TEST_CASE("legwork_ctrl_from_nose_up_kicks_it_off_its_end_within_6_s",
          "[sled][n1][legwork]") {
    const world::SnowpackField f = field(0.5);
    const sim::SledParams& p = shipped();
    REQUIRE(p.comfort.leg_work_nm > 0.0);  // the shipped table ships it
    auto off_end = [&](const sim::SledState& s) {
        return on_side(s) || inverted(s) || upright(s, p);
    };
    for (double roll0 : {0.0, 10.0}) {
        INFO("roll seed " << roll0 << " deg");
        const sim::SledState s0 = settled_pose(p, f, -90.0 * kDeg, roll0 * kDeg);
        REQUIRE(on_end(s0));
        REQUIRE(s0.leg_stage == sim::kLegPitched);  // armed, silently
        const StageResult r = press_until(p, f, s0, -1, 1.0, 0.7, 6.0, off_end);
        const glm::dvec3 u = up_body_of(r.s);
        std::printf("[N1 stage1 CTRL nose-up roll0 %.0f] shipped %.0f N m: off its "
                    "end at %.2f s after %d press(es); final tilt %.1f ub(%.2f,"
                    "%.2f,%.2f) side=%d upright=%d; grip %d rolled_seen %d "
                    "air_peak %.2f omega_peak %.2f gs_peak %.2f\n",
                    roll0, p.comfort.leg_work_nm, r.t_done, r.presses,
                    r.tilt_final / kDeg, u.x, u.y, u.z, on_side(r.s) ? 1 : 0,
                    upright(r.s, p) ? 1 : 0, r.grip_held ? 1 : 0,
                    r.rolled_seen ? 1 : 0, r.air_peak_s, r.omega_peak, r.gs_peak);
        REQUIRE(r.t_done > 0.0);
        REQUIRE(r.t_done <= 6.0);
        REQUIRE(r.presses <= 3);
        REQUIRE(r.grip_held);
        REQUIRE(r.air_peak_s <= p.comfort.rolled_grace_s);  // never a send
        REQUIRE(r.presses == 1);  // one press, the measured shipped behaviour
        // RED-TEAM FOLD (P1-2, 2026-09-19): THE LANDING BAND IS PINNED.
        // `off_end` accepts a side, its back or upright, so the SIGN of the
        // kick was unpinned: the toward-level mutant (-1) passed this leg
        // landing UPRIGHT at tilt 13.1. The shipped +1 ("backward", his
        // word) lands it on its BACK at tilt 150.4 -- stage 2's fixture --
        // and that is what ships until his seat rules otherwise (sled.h).
        REQUIRE(inverted(r.s));
        REQUIRE(!upright(r.s, p));
        // POSITIVE CONTROL: the identity never leaves the band.
        const sim::SledParams p0 = identity();
        const sim::SledState z0 = settled_pose(p0, f, -90.0 * kDeg, roll0 * kDeg);
        const StageResult r0 = press_until(p0, f, z0, -1, 1.0, 0.7, 6.0, off_end);
        std::printf("[N1 stage1 CTRL nose-up roll0 %.0f] identity: t_done %.2f "
                    "final tilt %.1f\n", roll0, r0.t_done, r0.tilt_final / kDeg);
        REQUIRE(r0.t_done < 0.0);
        REQUIRE(on_end(r0.s));
    }
}

// ---------------------------------------------------------------------------
// STAGE 2 -- INVERTED + SHIFT. From the on-its-back rest pose (173.5 deg) one
// SHIFT cadence at the shipped budget LIFTS the machine onto its end (the
// PITCHED band, |up_body.z| >= sin 50 -- the leg stage's own signature) and
// the pendulum on the same press drops it off that end; the cadence then
// rights it, without R. MEASURED HONESTLY: on this flat fixture the v2
// pendulum ALONE also rights the machine from 173.5 deg (its latched brace
// seeds `dir` at the dead point), so the identity arm is NOT "never rights";
// the discriminator is the LIFT: at the identity |up_body.z| never reaches
// the PITCHED band. Both times are printed and reported.
TEST_CASE("legwork_shift_from_inverted_goes_on_end_then_rights",
          "[sled][n1][legwork]") {
    const world::SnowpackField f = field(0.5);
    const sim::SledParams& p = shipped();
    REQUIRE(p.comfort.leg_work_nm > 0.0);
    struct Ladder {
        double t_on_end = -1.0, t_off_end = -1.0, t_right = -1.0;
        bool grip = true;
        double air_peak = 0.0, omega_peak = 0.0, z_peak = 0.0;
        int presses = 0;
        sim::SledState s;
    };
    auto run = [&](const sim::SledParams& pp) {
        Ladder L;
        sim::SledState s = settled_pose(pp, f, 0.0, 180.0 * kDeg);
        REQUIRE(inverted(s));
        sim::SledInputs in;
        bool was_on = false;
        for (int i = 0; i < 12 * 120; ++i) {
            const double t = i * kDt;
            const bool on = std::fmod(t, 1.7) < 1.0;
            if (on && !was_on) ++L.presses;
            was_on = on;
            in.stand = on ? 1.0f : 0.0f;
            s = sim::step_sled(s, in, pp, f, kDt);
            if (!s.grip.attached) L.grip = false;
            L.air_peak = std::max(L.air_peak, s.air_s);
            L.omega_peak = std::max(L.omega_peak, glm::length(s.angular_vel));
            L.z_peak = std::max(L.z_peak, std::abs(up_body_of(s).z));
            if (L.t_on_end < 0.0 && on_end(s)) L.t_on_end = t + kDt;
            if (L.t_on_end >= 0.0 && L.t_off_end < 0.0 && !on_end(s))
                L.t_off_end = t + kDt;
            if (L.t_right < 0.0 && upright(s, pp)) {
                L.t_right = t + kDt;
                break;
            }
        }
        L.s = s;
        return L;
    };
    const Ladder a = run(p);
    const Ladder b = run(identity());
    std::printf("[N1 stage2 SHIFT inverted] shipped %.0f N m: on end %.2f s, off "
                "end %.2f s, upright %.2f s, presses %d, |z| peak %.2f, grip %d, "
                "air_peak %.2f, omega_peak %.2f, final tilt %.1f\n",
                p.comfort.leg_work_nm, a.t_on_end, a.t_off_end, a.t_right,
                a.presses, a.z_peak, a.grip ? 1 : 0, a.air_peak, a.omega_peak,
                tilt_of(a.s) / kDeg);
    std::printf("[N1 stage2 SHIFT inverted] identity: on end %.2f s, upright "
                "%.2f s, |z| peak %.2f, final tilt %.1f (the v2 pendulum alone)\n",
                b.t_on_end, b.t_right, b.z_peak, tilt_of(b.s) / kDeg);
    REQUIRE(a.z_peak > b.z_peak);  // the lift: the end rises above the identity's
    REQUIRE(a.t_right > 0.0);      // and righted, R never called
    REQUIRE(a.t_right <= 12.0);
    REQUIRE(a.grip);
    REQUIRE(a.air_peak <= p.comfort.rolled_grace_s);
    REQUIRE(b.t_on_end < 0.0);     // the identity never lifts onto its end
    // RED-TEAM FOLD (P1-1, 2026-09-19): the v2 SIGNED pendulum-alone
    // righting from full inversion is pinned HERE, on the identity arm
    // (test_sled_selfright.cpp's inversion legs now read shipped() = 2400
    // and measure pendulum + legs). A future edit that breaks the pendulum
    // alone while the legs still right the machine reds this line.
    REQUIRE(b.t_right > 0.0);
    REQUIRE(b.t_right <= 12.0);
    // Reported, not required: whether the shipped budget reaches the PITCHED
    // band (|z| >= sin 50) on this flat fixture -- see the doc's ladder.
    std::printf("[N1 stage2 SHIFT inverted] shipped reaches the end band: %s\n",
                a.t_on_end > 0.0 ? "yes" : "no");
}

// ---------------------------------------------------------------------------
// NEVER A SEND, NEVER A MOVING MACHINE (the tumble ruling). A 20 m/s hop
// (6 m/s of radial launch, ~1.2 s in the air, then the landing) with CTRL
// held through the flight and SHIFT through the landing, and a 5 m/s level
// run under the same keys: the leg budget at the TOP of its band (4000)
// against the identity is whole-state bit-identical on every tick.
TEST_CASE("legwork_never_touches_a_send_or_a_moving_machine",
          "[sled][n1][legwork]") {
    const world::SnowpackField f = field(0.5);
    sim::SledParams hi = shipped();
    hi.comfort.leg_work_nm = 4000.0;
    const sim::SledParams lo = identity();
    auto keys = [](int i) {
        sim::SledInputs in;
        in.stand = (i < 180) ? -1.0f : 1.0f;  // CTRL 1.5 s, then SHIFT
        return in;
    };
    {
        const sim::SledState s0 = moving_raw(hi, f, 20.0, 6.0);
        sim::SledState end;
        const int div = first_divergence(hi, lo, f, s0, s0, 360, keys, &end);
        std::printf("[N1 send] 20 m/s hop, 4000 vs 0: first divergence tick %d; "
                    "end air_s %.2f tilt %.1f stage %d\n", div, end.air_s,
                    tilt_of(end) / kDeg, end.leg_stage);
        REQUIRE(div == -1);
    }
    {
        const sim::SledState s0 = moving_raw(hi, f, 5.0, 0.0);
        auto drive = [&](int i) {
            sim::SledInputs in = keys(i);
            in.throttle = 0.5f;
            return in;
        };
        sim::SledState end;
        const int div = first_divergence(hi, lo, f, s0, s0, 360, drive, &end);
        std::printf("[N1 moving] 5 m/s level run, 4000 vs 0: first divergence "
                    "tick %d; end gs %.2f stage %d\n", div, ground_speed_of(end),
                    end.leg_stage);
        REQUIRE(div == -1);
    }
}

// ---------------------------------------------------------------------------
// PRESS-GATED, NOT AUTOMATIC. (a) A stuck machine with no key for 10 s, at
// 4000 vs 0: bit-identical -- an armed stage moves nothing. (b) THE HELD KEY:
// CTRL / SHIFT already down when the machine gets stuck (held from the raw
// pose, through the fall, for 10 s) never fires -- a rising edge AFTER
// arming is the only trigger. Both poses.
TEST_CASE("legwork_is_press_gated_not_automatic", "[sled][n1][legwork]") {
    const world::SnowpackField f = field(0.5);
    sim::SledParams hi = shipped();
    hi.comfort.leg_work_nm = 4000.0;
    const sim::SledParams lo = identity();
    struct Pose {
        const char* name;
        double pitch_deg, roll_deg;
        float held;  // the key that WOULD fire this stage
    };
    const Pose poses[2] = {{"nose-up", -90.0, 0.0, -1.0f},
                           {"inverted", 0.0, 180.0, 1.0f}};
    for (const Pose& po : poses) {
        INFO(po.name);
        const sim::SledState raw = posed_raw(hi, f, po.pitch_deg * kDeg,
                                             po.roll_deg * kDeg);
        {
            auto none = [](int) { return sim::SledInputs{}; };
            sim::SledState end;
            const int div = first_divergence(hi, lo, f, raw, raw, 1200, none, &end);
            std::printf("[N1 no-key %s] 10 s idle, 4000 vs 0: first divergence "
                        "tick %d; stage %d\n", po.name, div, end.leg_stage);
            CHECK(div == -1);
            CHECK(end.leg_stage != sim::kLegNone);  // armed, and moved nothing
        }
        {
            auto held = [&](int) {
                sim::SledInputs in;
                in.stand = po.held;
                return in;
            };
            sim::SledState end;
            const int div = first_divergence(hi, lo, f, raw, raw, 1200, held, &end);
            std::printf("[N1 held-key %s] key held from tick 0 for 10 s, 4000 vs "
                        "0: first divergence tick %d; stage %d\n", po.name, div,
                        end.leg_stage);
            CHECK(div == -1);
        }
    }
}

// ---------------------------------------------------------------------------
// CTRL WHILE RIDING IS STILL THE TUCK. A level drive with CTRL held: the rider
// drops to -tuck_drop_m as before, and 4000 vs 0 is whole-state identical --
// the attitude gate lives in the kernel stage, not in the key binding.
TEST_CASE("legwork_ctrl_is_still_a_tuck_when_riding", "[sled][n1][legwork]") {
    const world::SnowpackField f = field(0.5);
    sim::SledParams hi = shipped();
    hi.comfort.leg_work_nm = 4000.0;
    const sim::SledParams lo = identity();
    const sim::SledState s0 = settled_pose(hi, f, 0.0, 0.0);
    auto tuck = [](int) {
        sim::SledInputs in;
        in.throttle = 0.4f;
        in.stand = -1.0f;
        return in;
    };
    sim::SledState end;
    const int div = first_divergence(hi, lo, f, s0, s0, 480, tuck, &end);
    std::printf("[N1 tuck] 4 s level drive under CTRL, 4000 vs 0: first "
                "divergence tick %d; rider_up_m %.4f gs %.2f stage %d\n", div,
                end.rider_up_m, ground_speed_of(end), end.leg_stage);
    REQUIRE(div == -1);
    REQUIRE(end.rider_up_m <= -0.9 * hi.tuck_drop_m);
    REQUIRE(end.leg_stage == sim::kLegNone);
}

// ---------------------------------------------------------------------------
// RED-TEAM FOLD (P1-3, 2026-09-19): NEVER ON AN UPRIGHT MACHINE, ON ANY
// BANK. The PITCHED band reads |up_body.z| against RADIAL up, so before the
// fold a machine parked UPRIGHT on its track on a >= 50 deg flank (tilt 51.8
// to the radial, rolled 0) was "on its end" to the stage: it armed, the HUD
// invited the press, and one CTRL edge backflipped it down the hill (2049
// N m, omega 11.0 rad/s, 1.23 s airborne, onto its back) -- a tumble the
// identity kernel never produces. The fold gates every stage on the kernel's
// own rolled latch. This leg is the one that would have caught it: an
// upright machine coasting to rest below the speed gate on flat snow and on
// a 46 / 52 deg SF1 ridge flank (the rt_probe fixture: gaussian ridge h 20.5
// / 25.5, sx 12, sy 40, facing uphill), a CTRL RISING EDGE and then a SHIFT
// rising edge after it has stopped: 4000 vs 0 whole-state bit-identical on
// every tick, stage NONE through the CTRL press. The held-key rule cannot
// mask this (the edges come 2.0 s in) and the speed gate cannot either (it
// is below 3.2 m/s when the edge arrives -- printed and required).
// MEASURED, STATED: on the 52 deg flank the SHIFT edge fires the v2
// PENDULUM (its w_tilt reads the 51.8 deg of PITCH as tilt: red-team P2-4,
// carried, v2 bytes) and the machine goes over -- rolled 55 ticks, omega
// 4.96 rad/s, 0.40 s airborne -- IDENTICALLY in both arms (first divergence
// -1, leg_nm 0). That is the pendulum's owner's; this leg only requires that
// the LEGS never arm on an upright machine and never move a byte here, so
// the stage REQUIRE is read through the CTRL press (before the SHIFT edge)
// and the bit-identity over the whole run.
namespace {
const glm::dvec3 kZUp(0.0, 0.0, 1.0);
const glm::dvec3 kZEast(1.0, 0.0, 0.0);
const glm::dvec3 kZNorth(0.0, 1.0, 0.0);
// Surface normal of the DRIVEN ground (drive_radius_at) by finite differences.
glm::dvec3 ground_normal(const world::SnowpackField& f, const glm::dvec3& dir0) {
    auto P = [&](glm::dvec3 d) {
        d = glm::normalize(d);
        return d * f.drive_radius_at(d);
    };
    const double h = 0.05 / 6'371'000.0;
    const glm::dvec3 dPe = P(dir0 + kZEast * h) - P(dir0 - kZEast * h);
    const glm::dvec3 dPn = P(dir0 + kZNorth * h) - P(dir0 - kZNorth * h);
    glm::dvec3 nn = glm::normalize(glm::cross(dPe, dPn));
    if (glm::dot(nn, dir0) < 0.0) nn = -nn;
    return nn;
}
// A ridge: gaussian across EAST (sigma sx), long along NORTH (sy), height h,
// on its own flat planet (the hill frame is data, as in test_snowhill.cpp).
struct Ridge {
    world::HeightField hf;
    world::SnowpackField f;
    Ridge(double h, double sx, double sy, double depth) : hf(flat_field()) {
        f = field(depth);
        f.hf = &hf;
        f.hill.enabled = h > 0.0;
        f.hill.up = kZUp;
        f.hill.east = kZEast;
        f.hill.north = kZNorth;
        f.hill.peaks[0] = {0.0, 0.0, h, sx, sy, 0.0};
    }
    glm::dvec3 dir_at(double x_east_m) const {
        return glm::normalize(kZUp + kZEast * (x_east_m / 6'371'000.0));
    }
    double slope_deg_at(double x) const {
        const glm::dvec3 d = dir_at(x);
        const glm::dvec3 nn = ground_normal(f, d);
        return std::acos(std::clamp(glm::dot(nn, d), -1.0, 1.0)) / kDeg;
    }
    // Spawn a machine ON the slope at x, body up = surface normal, nose
    // uphill (+east), lifted 5 cm above the surface along the normal.
    sim::SledState spawn(const sim::SledParams& p, double x, double speed) const {
        const glm::dvec3 d = dir_at(x);
        const glm::dvec3 nn = ground_normal(f, d);
        glm::dvec3 fwd = kZEast;
        fwd = glm::normalize(fwd - glm::dot(fwd, nn) * nn);
        glm::dmat3 m;
        m[0] = glm::cross(fwd, nn);
        m[1] = nn;
        m[2] = -fwd;
        sim::SledState s;
        s.orientation = glm::normalize(glm::quat_cast(m));
        s.position = d * f.drive_radius_at(d) + nn * (p.cg_height_m + 0.05);
        s.velocity = fwd * speed;
        return s;
    }
};
}  // namespace

TEST_CASE("legwork_never_arms_on_an_upright_machine_on_a_bank",
          "[sled][n1][legwork]") {
    sim::SledParams hi = shipped();
    hi.comfort.leg_work_nm = 4000.0;
    const sim::SledParams lo = identity();
    struct Bank {
        const char* name;
        double h;  // ridge height [m]; 0 = flat
    };
    const Bank banks[3] = {{"flat", 0.0},
                           {"46 deg flank", 20.5},
                           {"52 deg flank", 25.5}};
    for (const Bank& bk : banks) {
        INFO(bk.name);
        const Ridge rg(bk.h, 12.0, 40.0, 0.5);
        const double x0 = bk.h > 0.0 ? -12.0 : 0.0;
        const double slope = rg.slope_deg_at(x0);
        // Roll in at 1.5 m/s, throttle off: it coasts to rest on the flank.
        const sim::SledState s0 = rg.spawn(hi, x0, 1.5);
        // CTRL rising edge at 2.0 s (held 1.0 s), SHIFT rising edge at 4.0 s
        // (held 1.0 s), 6 s total.
        auto keys = [](int i) {
            sim::SledInputs in;
            const double t = i * kDt;
            in.stand = (t >= 2.0 && t < 3.0)   ? -1.0f
                       : (t >= 4.0 && t < 5.0) ? 1.0f
                                               : 0.0f;
            return in;
        };
        // The dial arm alone first, for the readouts at the edges.
        sim::SledState a = s0;
        double gs_at_ctrl = -1.0, tilt_at_ctrl = -1.0;
        int stage_max = 0, stage_max_ctrl = 0, rolled_ticks = 0;
        double leg_nm_peak = 0.0, omega_peak = 0.0, air_peak = 0.0;
        for (int i = 0; i < 720; ++i) {
            a = sim::step_sled(a, keys(i), hi, rg.f, kDt);
            if (i == 240) {
                gs_at_ctrl = ground_speed_of(a);
                tilt_at_ctrl = tilt_of(a) / kDeg;
            }
            stage_max = std::max(stage_max, a.leg_stage);
            if (i < 480) stage_max_ctrl = std::max(stage_max_ctrl, a.leg_stage);
            if (a.rolled) ++rolled_ticks;
            leg_nm_peak = std::max(leg_nm_peak, a.leg_nm_now);
            omega_peak = std::max(omega_peak, glm::length(a.angular_vel));
            air_peak = std::max(air_peak, a.air_s);
        }
        sim::SledState end;
        const int div =
            first_divergence(hi, lo, rg.f, s0, s0, 720, keys, &end);
        std::printf("[N1 bank %s] slope %.1f deg: at the CTRL edge gs %.2f "
                    "tilt %.1f; stage max through CTRL %d (whole run %d) "
                    "rolled ticks %d leg_nm peak %.0f omega peak %.2f air peak "
                    "%.2f; 4000 vs 0 first divergence tick %d; end tilt %.1f "
                    "gs %.2f\n",
                    bk.name, slope, gs_at_ctrl, tilt_at_ctrl, stage_max_ctrl,
                    stage_max, rolled_ticks, leg_nm_peak, omega_peak, air_peak,
                    div, tilt_of(end) / kDeg, ground_speed_of(end));
        REQUIRE(gs_at_ctrl < hi.comfort.right_assist_max_ms);  // gate open
        REQUIRE(tilt_at_ctrl < 75.0);  // upright to the kernel: not rolled
        if (bk.h > 0.0) REQUIRE(tilt_at_ctrl >= 45.0);  // and really banked
        REQUIRE(stage_max_ctrl == sim::kLegNone);
        REQUIRE(leg_nm_peak == 0.0);
        REQUIRE(div == -1);
    }
}

// ---------------------------------------------------------------------------
// THE LADDER, MEASURED (hidden probe): time off the end / to upright per
// budget on both fixtures, for the doc's table and the toml value.
TEST_CASE("legwork ladder probe", "[.probe]") {
    std::printf("\n depth budget | nose-up CTRL: t_off_end presses side back "
                "upright grip | inverted SHIFT: t_on_end z_peak t_right "
                "presses grip\n");
    for (double depth : {0.5, 1.0, 2.0}) {
        const world::SnowpackField f = field(depth);
        for (double nm : {0.0, 1500.0, 1800.0, 2400.0, 3200.0, 4000.0}) {
            sim::SledParams p = shipped();
            p.comfort.leg_work_nm = nm;
            auto off_end = [&](const sim::SledState& s) {
                return on_side(s) || inverted(s) || upright(s, p);
            };
            const sim::SledState a0 = settled_pose(p, f, -90.0 * kDeg, 0.0);
            const StageResult a =
                press_until(p, f, a0, -1, 1.0, 0.7, 12.0, off_end);
            sim::SledState s = settled_pose(p, f, 0.0, 180.0 * kDeg);
            sim::SledInputs in;
            double t_on = -1.0, t_r = -1.0, z_peak = 0.0;
            int presses = 0;
            bool was_on = false, grip = true;
            for (int i = 0; i < 12 * 120; ++i) {
                const double t = i * kDt;
                const bool on = std::fmod(t, 1.7) < 1.0;
                if (on && !was_on) ++presses;
                was_on = on;
                in.stand = on ? 1.0f : 0.0f;
                s = sim::step_sled(s, in, p, f, kDt);
                if (!s.grip.attached) grip = false;
                z_peak = std::max(z_peak, std::abs(up_body_of(s).z));
                if (t_on < 0.0 && on_end(s)) t_on = t + kDt;
                if (upright(s, p)) {
                    t_r = t + kDt;
                    break;
                }
            }
            std::printf(" %4.1f %6.0f | %8.2f %d %d %d %d %d | %8.2f %5.2f %8.2f "
                        "%d %d\n",
                        depth, nm, a.t_done, a.presses, on_side(a.s) ? 1 : 0,
                        inverted(a.s) ? 1 : 0, upright(a.s, p) ? 1 : 0,
                        a.grip_held ? 1 : 0, t_on, z_peak, t_r, presses,
                        grip ? 1 : 0);
        }
    }
}
