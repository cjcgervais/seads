// ★★ SLED TAPE R1 acceptance (SLED_TAPE_CONSULT_REPLY §4 R1, consult F7).
// Four legs, mirroring test_recorder_firewall.cpp's shape:
//   1. on/off bit-identity — the firewall PROOF (tap armed + recording vs
//      bare run flies the bit-identical trajectory).
//   2. round-trip — record, serialize, re-parse, params reconstruct from the
//      tape, replay over taped key-checked ground, every pin exact-==, and
//      the signature verifies. Non-vacuous by construction (lean active,
//      an override mid-run, creep/sink nonzero on a sinkable surface).
//   3. ground key-check — a perturbed taped dir fires the mismatch at the
//      exact sample index (the Q1 first-divergence attribution).
//   4. the writer refuses non-finite values loudly and the tape stays
//      parseable (a tape that can contain a NaN is a lying instrument).
// Test names stay pure ASCII (gate.sh tripwire; the codepage trap).

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <iterator>
#include <glm/gtc/quaternion.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "sim/sled.h"
#include "test/harness/sled_tape.h"
#include "world/heightfield.h"
#include "world/snowpack.h"

namespace {

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

world::SnowpackField field_at_depth(const world::HeightField& hf, double d) {
    world::SnowpackField f;
    f.hf = &hf;
    f.p.base_m = d;
    f.p.curv_gain = 0.0;
    f.p.drain_gain = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.slope_shed = 0.0;
    f.p.depth_max_m = 5.0;
    return f;
}

sim::SledState spawn(const sim::SledParams& p, const world::SnowpackField& f,
                     double speed_ms) {
    sim::SledState s;
    const glm::dvec3 dir(0.0, 0.0, 1.0);
    s.position = dir * (f.drive_radius_at(dir) + p.cg_height_m + 0.05);
    const glm::dvec3 up = dir, fwd(1.0, 0.0, 0.0);
    glm::dmat3 m;
    m[0] = glm::cross(fwd, up);
    m[1] = up;
    m[2] = -fwd;
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = fwd * speed_ms;
    return s;
}

sim::SledState settle(const sim::SledParams& p, const world::SnowpackField& f,
                      double v0) {
    sim::SledState s = spawn(p, f, v0);
    const sim::SledInputs idle;
    for (int i = 0; i < 90; ++i) s = sim::step_sled(s, idle, p, f, 1.0 / 120.0);
    return s;
}

// The scripted drive: deterministic, lean ACTIVE (the non-vacuity rule from
// the recorder round-trip leg), throttle/steer varying, per-tick cold drift.
sim::SledInputs script(int i) {
    sim::SledInputs in;
    in.throttle = static_cast<float>((i % 7) / 7.0);
    in.brake = (i / 60) % 3 == 2 ? 0.6f : 0.0f;
    in.steer = ((i / 40) % 2 == 0) ? 0.8f : -0.8f;
    in.lean_lat = static_cast<float>((i % 11) / 11.0 - 0.5);
    in.lean_fwd = static_cast<float>((i % 5) / 5.0 - 0.4);
    in.stand = (i / 80) % 2 == 0 ? 0.0f : 1.0f;
    return in;
}
void cold_of_tick(sim::SledParams& p, int i) {
    p.air_temp_c = -15.0 + (i % 5) * 0.25;
    p.snow_hardness = 1.0 + (i % 3) * 0.05;
}

constexpr double kDt = 1.0 / 120.0;  // the DRIVE's dt, never the gate's 1/60
constexpr int kTicks = 300;
constexpr int kOverrideTick = 150;

// The autoright-shaped out-of-kernel mutation (app/main.cpp R key): upright
// on the surface at own position, motion killed.
void autoright(sim::SledState& s, const sim::SledParams& p,
               const world::SnowpackField& f) {
    const glm::dvec3 up = glm::normalize(s.position);
    glm::dvec3 fwd = s.orientation * glm::dvec3(0.0, 0.0, -1.0);
    fwd = fwd - glm::dot(fwd, up) * up;
    if (glm::length(fwd) < 1e-6)
        fwd = glm::normalize(glm::cross(up, glm::dvec3(0, 0, 1)));
    fwd = glm::normalize(fwd);
    glm::dmat3 basis;
    basis[0] = glm::cross(fwd, up);
    basis[1] = up;
    basis[2] = -fwd;
    s.orientation = glm::normalize(glm::quat_cast(basis));
    s.position = up * (f.drive_radius_at(up) + p.cg_height_m + 0.05);
    s.velocity = glm::dvec3{0.0};
    s.angular_vel = glm::dvec3{0.0};
    s.rolled = false;
    s.rolled_hold_s = 0.0;
    s.air_s = 0.0;
    // ★ K-WS1 / K2: the teleport clears the exchange-momentum history too --
    // same reason app/main.cpp's R key does, and this fixture is the shape of
    // that key. Without it the round-trip diverges at this tick with
    // k_air_shift > 0, because the replay rebuilds state from a pin that does
    // not carry this (non-roster) field.
    s.ws_exch_l = glm::dvec3{0.0};
}

// One recorded drive: returns the serialized tape text (with footer) and the
// final state, optionally leaving the tap dark (for the off-arm leg).
struct Drive {
    std::string text;
    sim::SledState final_state;
    int refusals = 0;
};
Drive drive(const world::SnowpackField& f_in, bool record) {
    // Own copy so the tap arm on one run cannot touch the other's field.
    world::SnowpackField f = f_in;
    sim::SledParams p;
    // A moved dial, so leg 2 PROVES params reconstruct from the tape rather
    // than from the build defaults (consult F4).
    p.comfort.lean_bite_gain = 0.1234567891234;
    seads_sledtape::Writer w;
    long tick = 0;
    sim::SledState s = settle(p, f, 6.0);
    if (record) {
        w.begin("test-tag", kDt, p, "test fixture", tick, s);
        f.sample_tap = [&](const glm::dvec3& d,
                           const world::SnowpackField::GroundSample& g) {
            w.on_ground(tick, d, g);
        };
    }
    for (int i = 0; i < kTicks; ++i) {
        if (i == kOverrideTick) {
            autoright(s, p, f);
            if (record) w.on_override(tick, s);
        }
        cold_of_tick(p, i);
        s = sim::step_sled(s, script(i), p, f, kDt);
        if (record) w.on_tick(tick, script(i), p, s);
        ++tick;
    }
    Drive d;
    if (record) {
        w.drain(d.text);
        d.text += w.footer();
        d.refusals = w.nonfinite_refusals();
    }
    d.final_state = s;
    return d;
}

bool states_equal(const sim::SledState& a, const sim::SledState& b) {
#define SLEDTAPE_C(f) \
    if (!(a.f == b.f)) return false;
    SLEDTAPE_PIN_D(SLEDTAPE_C)
#undef SLEDTAPE_C
    return a.surface == b.surface && a.rolled == b.rolled;
}

}  // namespace

TEST_CASE("sled_tape_off_arm_is_bit_identical", "[sled][tape]") {
    // The firewall PROOF: recording on vs off drives the bit-identical
    // trajectory. If any feedback path existed, this diverges.
    const world::HeightField hf = flat_field();
    const world::SnowpackField f = field_at_depth(hf, 0.30);
    const Drive on = drive(f, true);
    const Drive off = drive(f, false);
    REQUIRE(states_equal(on.final_state, off.final_state));
    REQUIRE(on.refusals == 0);
    REQUIRE_FALSE(on.text.empty());
}

TEST_CASE("sled_tape_round_trip_replays_bit_identical", "[sled][tape]") {
    const world::HeightField hf = flat_field();
    const world::SnowpackField f = field_at_depth(hf, 0.30);
    const Drive d = drive(f, true);

    std::istringstream in(d.text);
    seads_sledtape::Tape t;
    std::string err;
    REQUIRE(seads_sledtape::load(in, t, &err));
    INFO(err);
    REQUIRE(t.sig_present);
    REQUIRE(t.sig_ok);
    REQUIRE(t.recs.size() == kTicks);
    // Two overrides: the initial pin + the autoright.
    REQUIRE(t.overrides.size() == 2);
    REQUIRE(t.overrides[1].tick == kOverrideTick);
    REQUIRE(t.dt == kDt);
    // Params reconstruct FROM the tape (consult F4): the moved dial survives
    // the text round-trip bit-exactly, and it is NOT the build default.
    REQUIRE(t.params.comfort.lean_bite_gain == 0.1234567891234);
    REQUIRE(t.params.comfort.lean_bite_gain !=
            sim::SledParams{}.comfort.lean_bite_gain);
    // Non-vacuity: the scripted drive leans, brakes and stands.
    bool any_lean = false, any_ground = !t.ground.empty();
    for (const auto& r : t.recs)
        if (r.in.lean_lat != 0.0f) any_lean = true;
    REQUIRE(any_lean);
    REQUIRE(any_ground);

    const seads_sledtape::ReplayResult r =
        seads_sledtape::replay_taped_ground(t);
    INFO("first divergence tick " << r.first_div_tick << " field "
                                  << r.first_div_field
                                  << " ground mismatch at "
                                  << r.ground_mismatch_index);
    REQUIRE(r.ground_mismatch_index == -1);
    REQUIRE(r.first_div_tick == -1);
    REQUIRE(r.ticks_replayed == kTicks);
    // The replayed final pin IS the drive's final state.
    REQUIRE(states_equal(t.recs.back().pin, d.final_state));
}

// ★★★ THE DIAL GAP. Found 2026-08-26 while attributing the tape 32/33
// divergence: `load` starts from a default-constructed SledParams and
// overwrites only the dials the tape NAMES, so a dial the tape does not carry
// silently takes TODAY'S struct default rather than the value in force when the
// drive was recorded -- and the replay still reports "bit-exact", because the
// pins are re-derived under the same wrong assumption. The writer emits the
// whole SLEDTAPE_COMFORT_D list, so "absent" means "did not exist at record
// time" and the gap is exactly measurable.
//
// TWO ARMS, because one of them cannot fail on its own: a freshly written tape
// must report an EMPTY gap (or a stub returning {} would pass), and a tape with
// one dial removed must report EXACTLY that dial (or a stub returning every
// dial would pass). Neither arm alone pins the behaviour; together they do.
TEST_CASE("sled_tape_dial_gap_names_exactly_the_dials_the_tape_lacks",
          "[sled][tape]") {
    const world::HeightField hf = flat_field();
    const world::SnowpackField f = field_at_depth(hf, 0.30);
    const Drive d = drive(f, true);

    // ARM 1 -- a tape this build wrote knows every dial this build declares.
    {
        std::istringstream in(d.text);
        seads_sledtape::Tape t;
        std::string err;
        REQUIRE(seads_sledtape::load(in, t, &err));
        INFO("a freshly written tape must have NO gap");
        REQUIRE(t.dial_gap.empty());
    }

    // The roster must be non-trivial, or arm 2 proves nothing about a real
    // dial (a build with zero comfort dials would pass it vacuously).
    const std::vector<std::string> all = seads_sledtape::all_comfort_dials();
    REQUIRE(all.size() > 10);

    // ARM 2 -- delete ONE dial line and the gap must name it, and only it.
    // This is the shape of a tape recorded before that dial existed.
    const std::string victim = all.front();
    const std::string needle = "# cparam " + victim + " ";
    const std::size_t at = d.text.find(needle);
    REQUIRE(at != std::string::npos);
    const std::size_t eol = d.text.find('\n', at);
    REQUIRE(eol != std::string::npos);
    std::string cut = d.text;
    cut.erase(at, eol - at + 1);
    REQUIRE(cut.size() < d.text.size());

    std::istringstream in2(cut);
    seads_sledtape::Tape t2;
    std::string err2;
    REQUIRE(seads_sledtape::load(in2, t2, &err2));
    INFO("the removed dial was " << victim);
    REQUIRE(t2.dial_gap.size() == 1);
    REQUIRE(t2.dial_gap.front() == victim);
}

// ★★★ THE UNPINNED LATCH (red-team finding, 2026-09-01).
//
// The pin roster (SLEDTAPE_PIN_D) is 41 doubles plus `surface` and `rolled`.
// `sim::SledState::grip` is not in it, `read_pin` fills a DEFAULT-constructed
// SledState, and the replay applies an override by WHOLE-STRUCT assignment --
// so every override silently resets `grip` to its struct default, in which
// `attached` is TRUE. `sim/sled.cpp` reads that latch as `hands_on`, which
// gates `rider_frac` -> `cg_off` -> the patch geometry.
//
// The millwright rung made this reachable: `app/main.cpp` J dismount detaches
// the grip and it USED to record the result as a tape override. The record then
// ran a machine with nobody on it and the replay ran the same machine with a
// rider, from that tick to the end of the drive.
//
// THREE ARMS, and only all three pin the behaviour:
//   1. the defect is REAL -- an override carrying a detached grip forks the
//      replay (a stub replay that never diverges would pass arms 2 and 3);
//   2. the same drive with the grip LEFT ATTACHED at that tick replays
//      bit-identically, so arm 1 is attributable to the latch and to nothing
//      else about the fixture;
//   3. the shipped answer -- END THE EPISODE at the dismount and open a new
//      one at the mount -- replays bit-identically on both halves.
TEST_CASE("sled_tape_an_override_with_a_detached_grip_forks_the_replay",
          "[sled][tape]") {
    const world::HeightField hf = flat_field();
    const world::SnowpackField f0 = field_at_depth(hf, 0.30);

    // One drive, recorded, with a DISMOUNT-shaped out-of-kernel write at
    // kOverrideTick: the grip latch drops and nothing else changes. `detach`
    // selects the defect arm; the control arm runs the identical fixture with
    // the latch left alone.
    struct Cut {
        std::string text;
        sim::SledState at_cut;   // state the instant the cut was taken
        std::string tail;        // the SECOND episode, begun at the re-mount
    };
    const auto run = [&](bool detach, bool split) {
        world::SnowpackField f = f0;
        sim::SledParams p;
        p.comfort.lean_bite_gain = 0.1234567891234;
        seads_sledtape::Writer w;
        seads_sledtape::Writer w2;
        bool on2 = false;
        long tick = 0;
        sim::SledState s = settle(p, f, 6.0);
        w.begin("test-tag", kDt, p, "grip-latch fixture", tick, s);
        f.sample_tap = [&](const glm::dvec3& d,
                           const world::SnowpackField::GroundSample& g) {
            if (on2)
                w2.on_ground(tick, d, g);
            else
                w.on_ground(tick, d, g);
        };
        Cut out;
        for (int i = 0; i < kTicks; ++i) {
            if (i == kOverrideTick) {
                if (detach) s.grip.attached = false;
                out.at_cut = s;
                if (split) {
                    // THE SHIPPED ANSWER: the episode ends here (no override
                    // is recorded at all) and the next one begins with the
                    // rider back on -- exactly what a J dismount followed by a
                    // walk-back and a J mount now does.
                    s.grip = sim::GripState{};
                    on2 = true;
                    w2.begin("test-tag", kDt, p, "grip-latch fixture 2", tick,
                             s);
                } else {
                    w.on_override(tick, s);
                }
            }
            cold_of_tick(p, i);
            s = sim::step_sled(s, script(i), p, f, kDt);
            if (on2)
                w2.on_tick(tick, script(i), p, s);
            else
                w.on_tick(tick, script(i), p, s);
            ++tick;
        }
        w.drain(out.text);
        out.text += w.footer();
        if (split) {
            w2.drain(out.tail);
            out.tail += w2.footer();
        }
        return out;
    };

    const auto replay_of = [](const std::string& text) {
        std::istringstream in(text);
        seads_sledtape::Tape t;
        std::string err;
        REQUIRE(seads_sledtape::load(in, t, &err));
        INFO(err);
        return seads_sledtape::replay_taped_ground(t);
    };

    // ARM 1 -- the defect. The recorded override carries a DETACHED grip; the
    // replay rebuilds it ATTACHED and the two machines are not the same one.
    {
        const Cut c = run(/*detach=*/true, /*split=*/false);
        REQUIRE_FALSE(c.at_cut.grip.attached);
        const seads_sledtape::ReplayResult r = replay_of(c.text);
        INFO("first divergence tick " << r.first_div_tick << " field "
                                      << r.first_div_field);
        REQUIRE(r.first_div_tick >= kOverrideTick);
        REQUIRE_FALSE(r.ok());
    }

    // ARM 2 -- the control. Same fixture, same override tick, latch untouched:
    // bit-identical. So arm 1 is the latch and not the fixture.
    {
        const Cut c = run(/*detach=*/false, /*split=*/false);
        REQUIRE(c.at_cut.grip.attached);
        const seads_sledtape::ReplayResult r = replay_of(c.text);
        INFO("first divergence tick " << r.first_div_tick << " field "
                                      << r.first_div_field);
        REQUIRE(r.ground_mismatch_index == -1);
        REQUIRE(r.first_div_tick == -1);
    }

    // ARM 3 -- the shipped answer. The episode ENDS at the dismount and a new
    // one opens at the mount; both halves replay bit-identically, and together
    // they cover every tick of the drive.
    {
        const Cut c = run(/*detach=*/true, /*split=*/true);
        const seads_sledtape::ReplayResult a = replay_of(c.text);
        REQUIRE(a.ground_mismatch_index == -1);
        REQUIRE(a.first_div_tick == -1);
        REQUIRE(a.ticks_replayed == kOverrideTick);
        const seads_sledtape::ReplayResult b = replay_of(c.tail);
        REQUIRE(b.ground_mismatch_index == -1);
        REQUIRE(b.first_div_tick == -1);
        REQUIRE(a.ticks_replayed + b.ticks_replayed == kTicks);
    }
}

// ★★★ WHY `app/main.cpp` NOW HAS EXACTLY ONE PLACE THAT OPENS A TAPE.
// The mount-seed block carried the open block TWICE inside one pair of braces
// (an edit that lost a `}`; it is on origin/main, it predates this lane). A
// second `open()` on an already-open `ofstream` sets `failbit` -- and
// `is_open()` still answers true, which is what `sled_tape_on` was read off --
// so the file was created and every later write was silently discarded. This
// leg is the measurement, not the reasoning.
TEST_CASE("sled_tape_a_second_open_on_a_live_stream_eats_every_write",
          "[sled][tape]") {
    const char* path = "sled_tape_double_open_leg.tmp";
    std::remove(path);
    {
        std::ofstream f;
        f.open(path, std::ios::out | std::ios::binary);
        REQUIRE(f.is_open());
        f.open(path, std::ios::out | std::ios::binary);  // the duplicate
        // THE TRAP, both halves: the stream reports itself open...
        CHECK(f.is_open());
        // ... and is in fact dead.
        CHECK(f.fail());
        f << "SEADSTAPE1\n";
        f.flush();
        f.close();
    }
    std::ifstream in(path, std::ios::in | std::ios::binary);
    REQUIRE(in.good());
    const std::string got((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    in.close();
    std::remove(path);
    INFO("bytes written through a double-opened stream: " << got.size());
    CHECK(got.empty());
}

TEST_CASE("sled_tape_ground_key_check_fires_on_perturbed_ground",
          "[sled][tape]") {
    const world::HeightField hf = flat_field();
    const world::SnowpackField f = field_at_depth(hf, 0.30);
    const Drive d = drive(f, true);
    std::istringstream in(d.text);
    seads_sledtape::Tape t;
    std::string err;
    REQUIRE(seads_sledtape::load(in, t, &err));
    REQUIRE(t.ground.size() > 600);
    // One ulp-scale nudge on one taped query key: the replay must attribute
    // the mismatch to EXACTLY that sample, not silently serve wrong ground.
    const long idx = 500;
    t.ground[idx].dir.x += 1e-13;
    const seads_sledtape::ReplayResult r =
        seads_sledtape::replay_taped_ground(t);
    REQUIRE(r.ground_mismatch_index == idx);
    REQUIRE_FALSE(r.ok());
}

TEST_CASE("sled_tape_writer_refuses_non_finite", "[sled][tape]") {
    seads_sledtape::Writer w;
    const sim::SledParams p;
    sim::SledState good;
    good.position = glm::dvec3(0.0, 0.0, 6371000.0);
    w.begin("test-tag", kDt, p, "nonfinite-refusal leg", 0, good);
    sim::SledState bad = good;
    bad.velocity.x = std::nan("");
    w.on_tick(0, sim::SledInputs{}, p, bad);   // refused
    w.on_tick(1, sim::SledInputs{}, p, good);  // kept
    REQUIRE(w.nonfinite_refusals() == 1);
    std::string text;
    REQUIRE(w.drain(text));
    text += w.footer();
    REQUIRE(text.find("nan") == std::string::npos);
    // The tape with the refused record dropped still parses and verifies.
    std::istringstream in(text);
    seads_sledtape::Tape t;
    std::string err;
    REQUIRE(seads_sledtape::load(in, t, &err));
    REQUIRE(t.sig_ok);
    REQUIRE(t.recs.size() == 1);
    REQUIRE(t.recs[0].tick == 1);
}

namespace {

// Load a committed golden, sig-checked. Fails the test loudly on any problem.
seads_sledtape::Tape load_golden(const char* name) {
    const std::string path = std::string(SEADS_ASSET_DIR) +
                             "/../test/golden/sled/" + name;
    std::ifstream in(path, std::ios::in | std::ios::binary);
    REQUIRE(in.good());
    seads_sledtape::Tape t;
    std::string err;
    REQUIRE(seads_sledtape::load(in, t, &err));
    INFO(err);
    REQUIRE(t.sig_present);
    REQUIRE(t.sig_ok);
    return t;
}

// The GI3 provocation contract (SLED_TAPE_CONSULT_REPLY Q7 leg 2), one body
// for every provocation golden. TWO arms:
//  - OFF arm (the non-vacuity proof): the recorded inputs over live-ish
//    ground, GI3 dials OFF, must ROLL — the leg bites, the fixture is
//    faithful to the drive it was cut from.
//  - SHIPPED arm (the ratchet): the same inputs under the shipped kernel
//    defaults must NOT latch rolled, with sanity bounds (stays on the
//    surface, keeps a ground-speed band). The claim is EXACTLY "the recorded
//    provocation no longer rolls the machine" — never "his 360 works"; the
//    claim upgrade is always Chad's next drive.
void provocation_contract(const seads_sledtape::Tape& t) {
    sim::SledParams off = t.params;
    off.comfort.assist_hull_frac = 0.0;
    off.comfort.roll_stiff_vgain = 0.0;
    off.comfort.release_floor_frac = 0.0;
    const seads_sledtape::OpenLoopResult r_off =
        seads_sledtape::replay_open_loop_liveish(t, off);
    REQUIRE(r_off.rolled_tick >= 0);  // non-vacuous: it rolled him, it rolls

    sim::SledParams shipped = t.params;
    const sim::SledComfort d;  // the SHIPPED build defaults, tracked live
    shipped.comfort.assist_hull_frac = d.assist_hull_frac;
    shipped.comfort.roll_stiff_vgain = d.roll_stiff_vgain;
    shipped.comfort.release_floor_frac = d.release_floor_frac;
    shipped.comfort.release_floor_hi_rad = d.release_floor_hi_rad;
    shipped.comfort.assist_v_lo_ms = d.assist_v_lo_ms;
    shipped.comfort.assist_v_hi_ms = d.assist_v_hi_ms;
    const seads_sledtape::OpenLoopResult r =
        seads_sledtape::replay_open_loop_liveish(t, shipped);
    INFO("rolled_tick " << r.rolled_tick << " max_tilt " << r.max_tilt_deg
                        << " v [" << r.min_v_ms << ", " << r.max_v_ms
                        << "] h_err [" << r.min_h_err_m << ", "
                        << r.max_h_err_m << "]");
    REQUIRE(r.rolled_tick == -1);
    REQUIRE(r.ticks == static_cast<long>(t.recs.size()));
    // Sanity bounds: on the surface (cg rides ~0.5-0.9 m above drive_r; an
    // escaped machine reads metres), and the speed band stays physical.
    REQUIRE(r.max_h_err_m < 2.0);
    REQUIRE(r.min_h_err_m > 0.0);
    REQUIRE(r.max_v_ms < 45.0);
}

}  // namespace

TEST_CASE("tape_360_chad_provocation", "[sled][tape][chad]") {
    // ★★ THE FIX'S RATCHET (GI3, consult Q7 leg 2; provenance
    // tape_360_chad_VERDICT.md). Chad's attempt-2 inputs — the ones that
    // rolled him at tick 7188 — fed open-loop over live-ish ground must not
    // roll the shipped kernel. Lives forever: the exact input sequence that
    // once rolled the machine must never roll it again.
    provocation_contract(load_golden("tape_360_chad.sledtape"));
}

TEST_CASE("tape_360_chad_attempt1_provocation", "[sled][tape][chad]") {
    // ★ The second provocation (VERDICT: attempt 1, t 9.9-14.5 s of the
    // parent tape — full-lock LEFT + full brake at 31.4 m/s, 92 deg of yaw
    // then 3.8 rolls at tick 1368). Same contract, opposite steer sign and a
    // harder brake — a different path into the same measured mechanism.
    provocation_contract(load_golden("tape_360_chad_attempt1.sledtape"));
}

TEST_CASE("tape_chad_flip_fence", "[sled][tape][chad]") {
    // ★★ THE WHEELIE FENCE (finding-5 ruling: lean authority NEVER weakened,
    // the wheelie gets stronger or stays). Chad's landed flip (parent tape
    // t 34.4-37.1 s: 2.06 s of air, no latch) replayed open-loop over
    // live-ish ground on the SHIPPED kernel must still fly and land without
    // a rolled latch. The GI3 terms act only through ground contact
    // (w_contact reads patch + hull load, both identically 0 airborne), so
    // this leg going red would mean that contract broke.
    const seads_sledtape::Tape t = load_golden("tape_chad_flip.sledtape");
    sim::SledParams shipped = t.params;  // his params, the SHIPPED GI3 dials
    const sim::SledComfort d;
    shipped.comfort.assist_hull_frac = d.assist_hull_frac;
    shipped.comfort.roll_stiff_vgain = d.roll_stiff_vgain;
    shipped.comfort.release_floor_frac = d.release_floor_frac;
    shipped.comfort.release_floor_hi_rad = d.release_floor_hi_rad;
    shipped.comfort.assist_v_lo_ms = d.assist_v_lo_ms;
    shipped.comfort.assist_v_hi_ms = d.assist_v_hi_ms;
    const seads_sledtape::OpenLoopResult r =
        seads_sledtape::replay_open_loop_liveish(t, shipped);
    INFO("rolled_tick " << r.rolled_tick << " max_air_s " << r.max_air_s
                        << " max_tilt " << r.max_tilt_deg);
    REQUIRE(r.rolled_tick == -1);
    REQUIRE(r.ticks == static_cast<long>(t.recs.size()));
    // It still FLIES: a real airborne spell, inverted on the way around.
    REQUIRE(r.max_air_s > 1.5);
    REQUIRE(r.max_tilt_deg > 150.0);
}

TEST_CASE("tape_360_chad_repro", "[sled][tape][chad]") {
    // ★★ THE GATE CUT (SLED_TAPE_CONSULT_REPLY Q7 leg 1; provenance +
    // Chad's verbatim ruling in test/golden/sled/tape_360_chad_VERDICT.md).
    // His second real 360 attempt, 2026-08-13: full-lock right + brake at
    // 33.4 m/s on a real road, 253 deg of yaw, then ~3.6 barrel rolls. The
    // leg asserts the INSTRUMENT: current kernel + taped inputs + taped
    // key-checked ground reproduce his roll exact-==, latch tick and all —
    // the thing the synthetic 8-cell gate never proved. It is NOT a fix
    // judgment: a candidate kernel is judged by the SEPARATE provocation
    // leg (live ground, must-not-roll) and, above all, by his next drive.
    // KILLED BY: re-recording or re-tolerating this golden. A fix that
    // moves determinism-relevant behavior turns this leg red BY DESIGN —
    // read first_div_tick/field as the attribution, then re-cut from
    // Chad's next drive on the new kernel.
    const std::string path = std::string(SEADS_ASSET_DIR) +
                             "/../test/golden/sled/tape_360_chad.sledtape";
    std::ifstream in(path, std::ios::in | std::ios::binary);
    REQUIRE(in.good());
    seads_sledtape::Tape t;
    std::string err;
    REQUIRE(seads_sledtape::load(in, t, &err));
    INFO(err);
    REQUIRE(t.sig_present);
    REQUIRE(t.sig_ok);  // CRLF or bit-rot -> fail LOUD before any physics
    REQUIRE(t.recs.size() == 961);
    REQUIRE(t.dt == 1.0 / 120.0);  // the DRIVE's dt, from the tape header
    const seads_sledtape::ReplayResult r =
        seads_sledtape::replay_taped_ground(t);
    INFO("first divergence tick " << r.first_div_tick << " field "
                                  << r.first_div_field
                                  << " ground mismatch at "
                                  << r.ground_mismatch_index);
    REQUIRE(r.ground_mismatch_index == -1);
    REQUIRE(r.first_div_tick == -1);
    REQUIRE(r.ticks_replayed == 961);
    // The event layer, in Chad's own vocabulary: the roll happened, and it
    // happened at the same tick on tape and on replay.
    REQUIRE(r.rolled_tick_tape == 7188);
    REQUIRE(r.rolled_tick_replay == 7188);
}

TEST_CASE("sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default",
          "[sled][tape][v2]") {
    // ★★★ THE LEG test/harness/sled_tape.h:399 HAS BEEN PROMISING. It named
    // this test as "the leg that reds on the delete" and the leg did not
    // exist -- a comment that ADVERTISES a guard is worse than no comment,
    // because it invites the tidy-up it claims to catch. Landing red-team
    // P1-2, 2026-09-19, folded.
    //
    // WHAT IT BARS. `load()` starts from a default-constructed SledParams and
    // overwrites only what the tape NAMES, so a dial absent from an old header
    // silently takes TODAY'S struct default. SLED KERNEL v2 is the first rung
    // to ship NON-INERT struct defaults, so the OFF-by-absence preset block is
    // now the only thing keeping every pre-v2 tape replaying the kernel that
    // CUT it. This leg pins that block, dial by dial, after a real load().
    //
    // WHY PROSE WAS NOT ENOUGH -- MEASURED, one preset deleted at a time,
    // seads_tests rebuilt, `ctest -R "tape|golden|replay"` run:
    //   traction_mu deleted          -> tape_360_chad_repro RED, flip_fence RED
    //   rolled_throttle_frac deleted -> tape_360_chad_repro RED
    //   track_lat_slip_shed deleted  -> NOTHING REDS  (40/40 passed)
    // The shed dial is dark in all three goldens, so its identity line could
    // be deleted today for free and the corpus would still report "bit-exact"
    // -- the precise failure the harness paragraph predicts. It reds HERE.
    //
    // KILLED BY: deleting or moving any of the five identity lines in
    // test/harness/sled_tape.h's OFF-by-absence block.
    const seads_sledtape::Tape t = load_golden("tape_360_chad.sledtape");

    // NON-VACUITY, HALF ONE: these dials really are ABSENT from this tape --
    // otherwise the tape would be overwriting them and the presets would be
    // untested no matter what they said.
    auto absent = [&t](const char* dial) {
        return std::find(t.dial_gap.begin(), t.dial_gap.end(),
                         std::string(dial)) != t.dial_gap.end();
    };
    REQUIRE(absent("traction_mu"));
    REQUIRE(absent("track_lat_slip_shed"));
    REQUIRE(absent("rolled_throttle_frac"));
    REQUIRE(absent("right_assist_max_ms"));
    REQUIRE(absent("right_stand_shift_frac"));

    // NON-VACUITY, HALF TWO: today's struct default is DIFFERENT for the three
    // v2 dials that ship from sim/sled.h, so "reconstructed at the identity"
    // and "took the struct default" are distinguishable outcomes. Without this
    // the leg could pass on a kernel where the two agree -- which is exactly
    // the coincidence that hid the traction_mu preset's teeth until 2026-09-18.
    const sim::SledParams def;
    REQUIRE(def.traction_mu == 3.0);
    REQUIRE(def.track_lat_slip_shed == 1.4);
    REQUIRE(def.comfort.rolled_throttle_frac == 0.15);

    // THE CLAIM: an ABSENT dial reconstructs at the IDENTITY -- the value the
    // kernel that cut this tape had -- never at today's default.
    REQUIRE(t.params.traction_mu == 0.0);
    REQUIRE(t.params.track_lat_slip_shed == 0.0);
    REQUIRE(t.params.comfort.rolled_throttle_frac == 0.0);
    // These two ship from config/scenario.toml, so their struct defaults still
    // ARE the identity and the two assertions below cannot currently fail.
    // They are here so the law does not rest on that coincidence a second
    // time: the day anybody "repairs" the deliberate divergence stated in
    // sled_kernel_v2_defaults_are_the_driven_values clause (iii), this leg is
    // already standing over the corpus.
    REQUIRE(t.params.comfort.right_assist_max_ms == 5.0 / 3.6);
    REQUIRE(t.params.comfort.right_stand_shift_frac == 1.0);
}
