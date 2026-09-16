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
// real app::tick, deliver scripted mouse flicks at tick boundaries. The
// recorder is tapped at the same point step_frame would tap it — after each
// tick.
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
#include "test/harness/instructor.h"  // harness::level_trim_state
#include "test/harness/recorder.h"    // the proposal under test

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
struct Flick {
    long tick;
    double dx;
    double dy;
};
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
        // null env/worlds: firewalled; v2 taps the tick's telemetry.
        const app::TickResult res = app::tick(st, in, kAp, kCp, nullptr);
        if (rec) rec->on_tick(in, st, res.telem);  // READ-ONLY tap
    }
    return st;
}

// F9 RECORD (v5 kernel-v5-reconcile): the app::step_frame hook seam the
// felt-flight recorder actually taps through (main.cpp never calls
// Recorder::on_tick directly — it wires this hook). A free function, since
// app::TickHook is a raw function pointer + void* ctx (not std::function),
// matching seads_replay::Recorder::on_tick's signature.
void hook_thunk(const app::TickInput& in, const app::LoopState& st,
                const control::Telemetry& telem, void* ctx) {
    static_cast<seads_replay::Recorder*>(ctx)->on_tick(in, st, telem);
}

}  // namespace

TEST_CASE(
    "recorder is read-only: on vs off flies the bit-identical trajectory") {
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
    // ctest cwd = the build dir (the harness CSVs land there too).
    const std::string path = "felt_roundtrip.seadsrec";
    REQUIRE(rec.flush(path, "test@89447aba5"));

    std::vector<seads_replay::TickRecord> recs;
    std::string tag;
    bool sig_ok = false;
    REQUIRE(seads_replay::read_records(path, recs, &tag, &sig_ok));
    CHECK(sig_ok);  // signature verifies
    CHECK(tag == "test@89447aba5");
    REQUIRE(recs.size() == static_cast<size_t>(kTicks));

    // v2 telemetry columns round-trip: every read record carries has_telem
    // and the exact captured values; at least one tick must carry a NONZERO
    // blend (the 200-px flick drives err through the band) so the equality
    // is not a vacuous zeros-match.
    bool any_nonzero_blend = false;
    for (size_t i = 0; i < recs.size(); ++i) {
        REQUIRE(recs[i].has_telem == 1);
        CHECK(recs[i].telem_blend == rec.records()[i].telem_blend);
        CHECK(recs[i].telem_held_bank == rec.records()[i].telem_held_bank);
        if (recs[i].telem_blend > 0.0) any_nonzero_blend = true;
    }
    CHECK(any_nonzero_blend);

    // Re-simulate from s0 feeding the recorded TickInputs; each tick must match
    // the pin the recorder stored (determinism on the SAME build).
    app::LoopState st = flying(s0);
    for (const auto& r : recs) {
        const app::TickInput in = seads_replay::to_tick_input(r);
        app::tick(st, in, kAp, kCp, nullptr);  // null env/worlds: firewalled
        CHECK(st.curr.position.x == r.pin_position[0]);
        CHECK(st.curr.position.y == r.pin_position[1]);
        CHECK(st.curr.position.z == r.pin_position[2]);
        CHECK(st.curr.orientation.w == r.pin_orientation[0]);
        CHECK(st.curr.orientation.x == r.pin_orientation[1]);
        CHECK(st.curr.angular_vel.x == r.pin_angular_vel[0]);
        CHECK(st.curr.throttle == r.pin_throttle);
    }
}

// F9 RECORD: app::step_frame's new OPTIONAL tick_hook parameter is a new
// seam layer (the "moved-consumer trap," recurred 5+ times per lessons.md) —
// it needs its OWN leg, not just app::tick's. Two claims: (1) wiring the
// hook is a strict superset — the SAME frame schedule with the hook wired
// vs nullptr flies the BIT-IDENTICAL LoopState (the hook is a read-only tap,
// exactly like Recorder::on_tick itself); (2) the hook fires EXACTLY once
// per tick actually stepped (summed across frames, including 0-tick and
// multi-tick frames from an irregular frame_dt schedule), never more, never
// less.
TEST_CASE(
    "step_frame: the optional tick_hook fires once per tick and stays "
    "bit-identical") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    // An irregular frame_dt schedule (varies 0.5x..2.5x sim_dt) so some
    // frames step 0 ticks and some step multiple — exercising both edges of
    // the "once per tick actually stepped" claim.
    auto frame_dt_at = [&](int f) {
        const double mult = 0.5 + 2.0 * (0.5 + 0.5 * std::sin(f * 0.7));
        return mult * kAp.sim_dt;
    };
    constexpr int kFrames = 200;

    auto run_frames = [&](seads_replay::Recorder* rec, long* hook_calls) {
        app::LoopState st = flying(s0);
        app::Accumulator accum(kAp.sim_dt);
        double pending_dx = 0.0, pending_dy = 0.0;
        app::FrameInput fin;
        fin.throttle = thr;
        long total_ticks = 0;
        for (int f = 0; f < kFrames; ++f) {
            const app::FrameResult fr = app::step_frame(
                st, accum, frame_dt_at(f), fin, pending_dx, pending_dy, kAp,
                kCp, nullptr, nullptr, nullptr, nullptr, nullptr,
                rec ? hook_thunk : nullptr,
                rec ? static_cast<void*>(rec) : nullptr);
            total_ticks += fr.ticks;
        }
        if (hook_calls) *hook_calls = total_ticks;
        return st;
    };

    seads_replay::Recorder rec;
    long expected_ticks = 0;
    const app::LoopState with_hook = run_frames(&rec, &expected_ticks);
    const app::LoopState no_hook = run_frames(nullptr, nullptr);

    REQUIRE(expected_ticks > 0);  // premise: the schedule actually ticks
    CHECK(rec.records().size() == static_cast<size_t>(expected_ticks));
    CHECK(loopstate_eq(with_hook, no_hook));
}

// ---------------------------------------------------------------------------
// v1 BACKWARD-COMPAT (the sealed-tape condition): a v1-format body — old
// header tag, old 40-column data lines, NO trailing telemetry — must parse
// with the v2 reader, verify its fnv1a signature bit-identically (the body
// bytes are hashed verbatim and are untouched by the column addition), and
// report has_telem == 0 with the telemetry defaults intact. This is the
// executable form of the acceptance check run against the four canonical
// sealed goldens (D:\mandalark-kernel\goldens\golden_1..4). Mutation this
// kills (verified): a STRICT reader that requires the trailing extraction
// (s >> tb >> thb unconditional + stream-fail -> reject line) drops the
// record / fails sig bookkeeping here.
// ---------------------------------------------------------------------------
TEST_CASE("recorder v1 tape: parses, signature verifies, has_telem == 0") {
    // A hand-built v1 body: header exactly as the v1 writer emitted it, one
    // data line of the 40 v1 columns (tick..pin_throttle), values chosen
    // non-zero and exactly representable so the parse spot-checks are ==.
    std::string body;
    body += "# seads-felt-flight v1  tag=sealed@v10\n";
    body +=
        "# columns: tick raw_mode raw_pitch raw_yaw raw_roll raw_throttle "
        "raw_flap raw_gear throttle flap_cmd gear_cmd freelook orient om0 om1 "
        "om2 os0 os1 os2 aim_dx aim_dy aim_gain_scale ffx ffy ffz frame_ticks "
        "px py pz vx vy vz qw qx qy qz wx wy wz pin_throttle\n";
    body +=
        "7 0 0 0 0 0 0 0 0.75 0 0 0 0 0 0 0 0 0 0 12.5 -3.25 1 0 0 0 1 "
        "19000 0 0 0 0 -150 1 0 0 0 0 0 0 0.75\n";
    const std::string path = "felt_v1_compat.seadsrec";
    {
        std::ofstream out(path);
        REQUIRE(out.good());
        out << "# sig fnv1a=" << seads_replay::fnv1a(body) << '\n' << body;
    }

    std::vector<seads_replay::TickRecord> recs;
    std::string tag;
    bool sig_ok = false;
    REQUIRE(seads_replay::read_records(path, recs, &tag, &sig_ok));
    CHECK(sig_ok);  // the v1 signature verifies under the v2 reader
    CHECK(tag == "sealed@v10");
    REQUIRE(recs.size() == 1);
    CHECK(recs[0].tick == 7);
    CHECK(recs[0].throttle == 0.75);
    CHECK(recs[0].aim_dx == 12.5);
    CHECK(recs[0].aim_dy == -3.25);
    CHECK(recs[0].pin_position[0] == 19000.0);
    CHECK(recs[0].pin_throttle == 0.75);
    // The v1 line has no trailing telemetry: defaults intact, flag off.
    CHECK(recs[0].has_telem == 0);
    CHECK(recs[0].telem_blend == 0.0);
    CHECK(recs[0].telem_held_bank == 0.0);
    std::remove(path.c_str());
}
