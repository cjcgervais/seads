#pragma once
// ===========================================================================
// THE FEEL TAPE WRITER -- the hook that records Chad's real hand, lifted out
// of main.cpp so the ROUND-TRIP TEST can drive THE SHIPPED WRITER.
//
// It used to live in main.cpp, which is not linked into the tests. The
// round-trip test therefore round-tripped an in-memory struct that it also
// owned, and "proved by a round-trip test" proved only that the test agreed
// with itself. The emitter meanwhile named 52 columns while writing 74, and
// nothing caught it. A test that cannot reach the real writer cannot defend
// the format; so the real writer now lives where the test can reach it.
// ===========================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/feel_tape_columns.h"
#include "app/feel_tape_fields.h"
#include "app/instructor_tick.h"
#include "sim/world.h"
#include "test/harness/recorder.h"

namespace app {

struct FeelTapeCtx {
    std::FILE* f = nullptr;
    seads_replay::Recorder* rec = nullptr;  // chained (F9), may be null
    long tick = 0;
    double sim_dt = 1.0 / 120.0;
    sim::AircraftParams ap_R{};  // for sim::altitude
};

inline void feel_tape_hook(const app::TickInput& in, const app::LoopState& st,
                    const control::Telemetry& telem, void* ctx) {
    FeelTapeCtx* c = static_cast<FeelTapeCtx*>(ctx);
    if (c->f != nullptr && c->tick == 0) {
        // THE INITIAL STATE -- what makes the tape REPLAYABLE. v1 had none,
        // so the replay started from a synthetic trim and could not reproduce
        // the recorded trace at all. st.prev is the PRE-tick state, i.e. the
        // true t=0. max_digits10 (%.17g): at |p| = R a float ulp is one tick
        // of gravity (the S1 lesson), so the seed must be exact or the replay
        // diverges for reasons that have nothing to do with the kernel.
        std::fprintf(c->f,
                     "# seed_pos %.17g %.17g %.17g\n"
                     "# seed_vel %.17g %.17g %.17g\n"
                     "# seed_quat %.17g %.17g %.17g %.17g\n"
                     "# seed_vhat %.17g %.17g %.17g\n",
                     st.prev.position.x, st.prev.position.y,
                     st.prev.position.z, st.prev.velocity.x,
                     st.prev.velocity.y, st.prev.velocity.z,
                     st.prev.orientation.w, st.prev.orientation.x,
                     st.prev.orientation.y, st.prev.orientation.z,
                     st.prev.last_vhat.x, st.prev.last_vhat.y,
                     st.prev.last_vhat.z);
    }
    // Chain the felt-flight recorder so turning the tape on never silently
    // disables F9 (the two share step_frame's single hook slot).
    if (c->rec != nullptr) c->rec->on_tick(in, st, telem);
    if (c->f == nullptr) return;
    const glm::dvec3 aim = st.aim.forward();
    const glm::dvec3 tb = sim::body_dir_of(st.curr.orientation, aim);
    const glm::dvec3 up = sim::local_up(st.curr.position);
    const glm::dvec3 nose = st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const double theta = std::asin(std::clamp(glm::dot(nose, up), -1.0, 1.0));
    // TARGET 2 (the lateral nose-down): the aim's WORLD elevation and the
    // nose's, and the GAP between them. Chad's law -- "the plane should follow
    // my mouse... an instructor would never crash me into the ground if I did
    // not mount my mouse anywhere near there" -- is a statement about THIS
    // number. tb.y alone cannot express it: that is the aim above the WING
    // line, which is bank-contaminated (the MB-lean P1-1 class).
    const double aim_elev =
        std::asin(std::clamp(glm::dot(aim, up), -1.0, 1.0));
    const double elev_gap = theta - aim_elev;
    // The pitch channel's two ceilings, recomputed here exactly as
    // control::step does (an INSTRUMENT mirror -- change both together).
    const double v_c = std::max(telem.extracted.speed, 10.0);
    const double w_max_p =
        (32.0 - telem.extracted.cos_phi_theta) * 9.81 / v_c;
    const double w_min_p =
        (-9.0 - telem.extracted.cos_phi_theta) * 9.81 / v_c;
    const double aoa_ceil = std::clamp(
        10.0 * (0.34906585 - telem.aoa_filtered), w_min_p, w_max_p);
    std::fprintf(
        c->f,
        "%ld,%.9g,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
        "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.17g,%.17g,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
        "%.9g,%.9g,%.9g,"
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,%d,"
        "%.17g,%.17g,%.17g,"
        "%.17g,%.17g,%.17g,%.17g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%d,%d,%d,%.9g,%.9g,%d,%.9g,%.9g,%.9g",
        c->tick, c->tick * c->sim_dt, in.frame_ticks, in.aim_dx, in.aim_dy,
        in.aim_rate_ff.x, in.aim_rate_ff.y, in.aim_rate_ff.z, aim.x, aim.y,
        aim.z, tb.x, tb.y, tb.z, telem.e, telem.blend, telem.extracted.phi,
        theta, telem.extracted.cos_phi_theta, telem.omega_des.x,
        telem.omega_des.y, telem.omega_des.z, telem.held_bank,
        // v2: the state the replay grades against, and the roll channel
        // limb by limb so an attribution is read, never inferred.
        sim::altitude(st.curr.position, c->ap_R), glm::length(st.curr.velocity),
        int(telem.righting), int(telem.push_mode), int(telem.deadzoned),
        st.internal.roll_latch, st.internal.hand_rest, telem.hand_gate,
        telem.roll_hold, telem.roll_maneuver, telem.roll_right,
        aim_elev, theta, elev_gap,
        // v4: the FULL per-tick state. v2's single t=0 seed made only the
        // opening window replayable -- a mid-tape event (every one of the 61
        // lateral events) could not be seeded at all, so the grading probe
        // had to RECONSTRUCT entry conditions and reproduced 1 of 3. With
        // this, any tick is a seed. max_digits10: at |p| = R a float ulp is
        // one tick of gravity (S1).
        st.prev.position.x, st.prev.position.y, st.prev.position.z,
        st.prev.velocity.x, st.prev.velocity.y, st.prev.velocity.z,
        st.prev.orientation.w, st.prev.orientation.x, st.prev.orientation.y,
        st.prev.orientation.z, st.prev.last_vhat.x, st.prev.last_vhat.y,
        st.prev.last_vhat.z, telem.load_factor, int(telem.push_mode),
        st.prev.angular_vel.x, st.prev.angular_vel.y,
        st.prev.angular_vel.z,
        // v5: the AIM FRAME's full quaternion and control::Internal's
        // latches. v4 recorded the aim's FORWARD only -- but apply_mouse
        // rotates about the frame's OWN up/right, so a mid-tape seed steered
        // the recorded deltas along a different path and the replay's roll
        // diverged ~100 deg in 6 s. The controller's latches matter for the
        // same reason: roll_latch is a SIGN latch, so starting it at 0
        // mid-manoeuvre picks the other way round.
        st.aim.q.w, st.aim.q.x, st.aim.q.y, st.aim.q.z,
        st.internal.roll_latch, st.internal.elev_latch,
        st.internal.integ.x, st.internal.integ.y, st.internal.integ.z,
        st.internal.aoa_filtered, int(st.internal.capture),
        int(st.internal.ballistic), int(st.internal.deadzoned),
        int(st.internal.pursuit), st.internal.rest_time,
        st.internal.inv_rest, int(st.internal.righting),
        // the pitch clamps, so a tape answers "was the channel bounded?"
        // by reading (the AoA pushback is what binds -- measured 112-333 of
        // 720 ticks on his lateral dives, against 0-3 for the G ceiling)
        aoa_ceil, w_max_p, telem.yaw_budget_scale);
    // THE FULL REPLAY STATE -- one "%.17g," per field of
    // SEADS_TAPE_STATE_FIELDS, in the same order kFeelTapeColumns names them.
    // max_digits10: at |p| = R a float ulp is one tick of gravity (S1).
    {
        const app::LoopState& S = st;
#define SEADS_TAPE_WRITE_D(name, expr) \
    std::fprintf(c->f, ",%.17g", double(expr));
#define SEADS_TAPE_WRITE_B(name, expr) \
    std::fprintf(c->f, ",%d", int(expr) ? 1 : 0);
#define SEADS_TAPE_WRITE_I(name, expr) \
    std::fprintf(c->f, ",%d", int(expr));
#define SEADS_TAPE_WRITE_E(name, expr) \
    std::fprintf(c->f, ",%d", int(expr));
        SEADS_TAPE_STATE_FIELDS(SEADS_TAPE_WRITE_D, SEADS_TAPE_WRITE_B,
                                SEADS_TAPE_WRITE_I, SEADS_TAPE_WRITE_E)
#undef SEADS_TAPE_WRITE_D
#undef SEADS_TAPE_WRITE_B
#undef SEADS_TAPE_WRITE_I
#undef SEADS_TAPE_WRITE_E
        std::fprintf(c->f, "\n");
    }
    ++c->tick;
}

// Opens the tape at `path` and writes the "# ..." banner plus the column
// header. The header is PRINTED FROM kFeelTapeColumns -- never spelled by
// hand, which is how it came to disagree with the row writer.
inline bool feel_tape_open(FeelTapeCtx& t, const char* path,
                           const std::string& banner,
                           const sim::AircraftParams& params) {
    t.f = std::fopen(path, "wb");
    if (t.f == nullptr) return false;
    t.tick = 0;
    t.sim_dt = params.sim_dt;
    t.ap_R = params;
    std::fprintf(t.f, "# SEADS feel tape\n");
    for (const char* q = banner.c_str(); *q;) {
        const char* nl = std::strchr(q, '\n');
        std::fprintf(t.f, "# %.*s\n", int(nl ? nl - q : std::strlen(q)), q);
        if (!nl) break;
        q = nl + 1;
    }
    for (int i = 0; i < kFeelTapeColumnCount; ++i)
        std::fprintf(t.f, "%s%s", kFeelTapeColumns[i],
                     i + 1 < kFeelTapeColumnCount ? "," : "\n");
    return true;
}

}  // namespace app
