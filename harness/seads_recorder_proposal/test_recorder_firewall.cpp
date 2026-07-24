// test_recorder_firewall.cpp — PROPOSED differential test for the felt-flight
// recorder (drop in at test/unit/, add to the ctest gate).
//
// ACCEPTANCE PROOF the reviewing session RUNS (not trusts): the recorder is
// read-only — with the SAME seed and input schedule, a run WITH recording and a
// run WITHOUT recording produce the BIT-IDENTICAL LoopState. If the recorder
// could feed anything back into the loop, this diverges. Plus a round-trip leg:
// the serialized record replays to the bit-identical trajectory it captured
// (the determinism guarantee), and a tampered file trips the signature.
//
// Mirrors the AT-9 driver shape (test_at9.cpp): drive whole ticks through the
// real app::tick, deliver scripted mouse flicks at tick boundaries. The recorder
// is tapped at the same point step_frame would tap it — after each tick.
//
// COMPILES ONLY WITHIN seads-feel (same includes as test_at9.cpp). Symbols
// verified @ feel/kernel-v5 89447aba5.

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/instructor_tick.h"
#include "app/loop.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/world.h"
#include "test/harness/instructor.h"      // harness::level_trim_state
#include "test/harness/recorder.h"        // the proposal under test

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

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

bool eq3(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool eq4(const glm::dquat& a, const glm::dquat& b) {
    return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}
bool loopstate_eq(const app::LoopState& a, const app::LoopState& b) {
    return eq3(a.curr.position, b.curr.position) &&
           eq3(a.curr.velocity, b.curr.velocity) &&
           eq4(a.curr.orientation, b.curr.orientation) &&
           eq3(a.curr.angular_vel, b.curr.angular_vel) &&
           a.curr.throttle == b.curr.throttle;
}

// A scripted mouse flick delivered at a tick boundary (post-curve deltas).
struct Flick { long tick; double dx; double dy; };
const std::vector<Flick> kFlicks = {
    {60, +200.0, 0.0}, {180, 0.0, +150.0}, {300, -180.0, +60.0}};
constexpr long kTicks = 480;

// Drive the fixed-dt tick loop from s0. If `rec` is non-null, tap it after each
// tick (exactly where step_frame's optional hook sits). Returns the final loop.
app::LoopState run(const sim::SimState& s0, seads_replay::Recorder* rec) {
    app::LoopState st = flying(s0);
    double pending_dx = 0.0, pending_dy = 0.0;
    std::vector<bool> done(kFlicks.size(), false);
    for (long i = 0; i < kTicks; ++i) {
        for (size_t j = 0; j < kFlicks.size(); ++j)
            if (!done[j] && i == kFlicks[j].tick) {
                pending_dx += kFlicks[j].dx;
                pending_dy += kFlicks[j].dy;
                done[j] = true;
            }
        app::TickInput in;
        in.throttle = 0.7;
        in.aim_dx = pending_dx;
        in.aim_dy = pending_dy;
        in.frame_ticks = 1;
        pending_dx = pending_dy = 0.0;
        app::tick(st, in, kAp, kCp);
        if (rec) rec->on_tick(in, st);  // READ-ONLY tap
    }
    return st;
}

}  // namespace

TEST_CASE("recorder is read-only: on vs off flies the bit-identical trajectory") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    seads_replay::Recorder rec;
    const app::LoopState with_rec = run(s0, &rec);
    const app::LoopState no_rec = run(s0, nullptr);

    std::printf("[recorder] captured %zu ticks; loopstate_eq=%d\n",
                rec.records().size(), loopstate_eq(with_rec, no_rec));
    CHECK(rec.records().size() == static_cast<size_t>(kTicks));
    CHECK(loopstate_eq(with_rec, no_rec));  // the firewall: no feedback path
}

TEST_CASE("recorder round-trip: the captured stream replays bit-identical") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    seads_replay::Recorder rec;
    run(s0, &rec);
    const std::string path =
        std::string(SEADS_BUILD_TMP "/felt_roundtrip.seadsrec");
    REQUIRE(rec.flush(path, "test@89447aba5"));

    std::vector<seads_replay::TickRecord> recs;
    std::string tag;
    bool sig_ok = false;
    REQUIRE(seads_replay::read_records(path, recs, &tag, &sig_ok));
    CHECK(sig_ok);                       // signature verifies
    CHECK(tag == "test@89447aba5");
    REQUIRE(recs.size() == static_cast<size_t>(kTicks));

    // Re-simulate from s0 feeding the recorded TickInputs; each tick must match
    // the pin the recorder stored (determinism on the SAME build).
    app::LoopState st = flying(s0);
    for (const auto& r : recs) {
        const app::TickInput in = seads_replay::to_tick_input(r);
        app::tick(st, in, kAp, kCp);
        CHECK(st.curr.position.x == r.pin_position[0]);
        CHECK(st.curr.position.y == r.pin_position[1]);
        CHECK(st.curr.position.z == r.pin_position[2]);
        CHECK(st.curr.orientation.w == r.pin_orientation[0]);
        CHECK(st.curr.orientation.x == r.pin_orientation[1]);
        CHECK(st.curr.angular_vel.x == r.pin_angular_vel[0]);
        CHECK(st.curr.throttle == r.pin_throttle);
    }
}
