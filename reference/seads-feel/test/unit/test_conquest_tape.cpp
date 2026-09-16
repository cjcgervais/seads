// CONQUEST TAPE unit tests (docs/conquest_tape_spec.md §5). Pure, headless:
// no raylib, no clock, no rng. Each TEST_CASE name is pure ASCII (the 4x-
// recurred ctest trap). Every asserted line is located by its "t":"xx" tag +
// tick, never by absolute line index (the spec's own discipline).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

#include "app/conquest_tape.h"
#include "app/instructor_tick.h"
#include "app/loop.h"
#include "combat/conquest.h"
#include "combat/kill.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "drone/drone.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/world.h"
#include "world/heightfield.h"
#include "test/harness/instructor.h"
#include "weapon/ballistics.h"
#include "world/tunnel_net.h"

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

// Line lookup by tag + tick — never by absolute line index (spec §5 rule).
// Splits the drained body into lines and returns the FIRST line carrying
// both the "t":"tag" marker and the "k":N marker (N as an exact decimal —
// safe because every k in these tests is small and distinct enough not to
// prefix-collide within one call site's expectations).
std::vector<std::string> split_lines(const std::string& body) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= body.size()) {
        const std::size_t nl = body.find('\n', start);
        if (nl == std::string::npos) break;
        out.push_back(body.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

std::string find_line(const std::string& body, const std::string& tag,
                      long long k) {
    const std::string tag_marker = "\"t\":\"" + tag + "\"";
    const std::string k_marker = "\"k\":" + std::to_string(k) + ",";
    for (const std::string& line : split_lines(body)) {
        if (line.find(tag_marker) != std::string::npos &&
            line.find(k_marker) != std::string::npos)
            return line;
    }
    return {};
}

int count_lines_with_tag(const std::string& body, const std::string& tag) {
    const std::string tag_marker = "\"t\":\"" + tag + "\"";
    int n = 0;
    for (const std::string& line : split_lines(body))
        if (line.find(tag_marker) != std::string::npos) ++n;
    return n;
}

drone::DroneState make_drone(int spawn_index, const glm::dvec3& pos,
                             const glm::dvec3& vel = glm::dvec3{0.0}) {
    drone::DroneState d;
    d.spawn_index = spawn_index;
    d.curr.position = pos;
    d.curr.velocity = vel;
    d.curr.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    d.prev = d.curr;
    d.hp = 100.0;
    return d;
}

app::TickInput noop_in() { return app::TickInput{}; }

// The T1 canon dials (test_tunnel.cpp's own fixture, reused verbatim so this
// leg's net matches the tree's real tunnel shape). ground=nullptr is a legal
// build (test_tunnel.cpp line ~330's precedent) — the bare-sphere mouths.
world::TunnelParams test_tp() {
    world::TunnelParams tp;
    tp.sphere_R = kAp.R;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 0.0;
    tp.arena_a_m = 7350.0;
    tp.arena_c_m = 2600.0;
    tp.arena_depth_m = 1500.0;
    tp.cavern_core_m = 2500.0;
    tp.breach_margin_m = 300.0;
    tp.chamber_long_m = 200.0;
    tp.chamber_lat_m = 140.0;
    tp.chamber_vert_m = 120.0;
    tp.chamber_breach_offset_m = 800.0;
    tp.connector_radius_m = 60.0;
    tp.chambers_on = true;
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    tp.trench_len_m = 450.0;
    tp.trench_rim_m = 150.0;
    return tp;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. The hook fires once per tick with the frame's own pointers.
TEST_CASE("conquest tape hook fires once per tick with the frame's own pointers") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);
    app::LoopState st = flying(s0);
    app::Accumulator accum(kAp.sim_dt);
    app::DroneWorld dw;
    combat::CombatWorld cw;
    app::ConquestWorld cq;
    double pdx = 0.0, pdy = 0.0;
    app::FrameInput fin;
    fin.throttle = thr;

    struct Ctx {
        long calls = 0;
        const app::DroneWorld* seen_dw = nullptr;
        const combat::CombatWorld* seen_cw = nullptr;
        const app::ConquestWorld* seen_cq = nullptr;
        const sim::Environment* seen_env = nullptr;
        bool mismatch = false;
    } ctx;

    struct Hook {
        static void call(const app::TickInput&, const app::LoopState&,
                         const app::DroneWorld* dw, const combat::CombatWorld* cw,
                         const app::ConquestWorld* cq, const sim::Environment* env,
                         void* raw) {
            Ctx* c = static_cast<Ctx*>(raw);
            ++c->calls;
            if (c->calls == 1) {
                c->seen_dw = dw;
                c->seen_cw = cw;
                c->seen_cq = cq;
                c->seen_env = env;
            } else if (dw != c->seen_dw || cw != c->seen_cw || cq != c->seen_cq ||
                      env != c->seen_env) {
                c->mismatch = true;
            }
        }
    };

    const app::FrameResult fr = app::step_frame(
        st, accum, kAp.sim_dt * 5.4, fin, pdx, pdy, kAp, kCp, nullptr, &dw,
        nullptr, &cw, nullptr, nullptr, nullptr, &cq, Hook::call,
        static_cast<void*>(&ctx));

    REQUIRE(fr.ticks > 0);
    CHECK(ctx.calls == fr.ticks);
    CHECK_FALSE(ctx.mismatch);
    CHECK(ctx.seen_dw == &dw);
    CHECK(ctx.seen_cw == &cw);
    CHECK(ctx.seen_cq == &cq);
    CHECK(ctx.seen_env == nullptr);
}

// ---------------------------------------------------------------------------
// 2. The off arm (hook null) is bit-identical to the hook wired (the S8-drone
// differential firewall leg): the tape is a strict read-only tap.
namespace {
void tape_hook_thunk(const app::TickInput& in, const app::LoopState& st,
                     const app::DroneWorld* dw, const combat::CombatWorld* cw,
                     const app::ConquestWorld* cq, const sim::Environment* env,
                     void* ctx) {
    static_cast<seads_tape::ConquestTape*>(ctx)->on_tick(in, st, dw, cw, cq, env);
}
}  // namespace

TEST_CASE("conquest tape off arm is bit identical") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 150.0, 4000.0, up, heading, &thr);

    auto build_dw = [&]() {
        app::DroneWorld dw;
        dw.drones.push_back(
            make_drone(0, s0.position + glm::dvec3{500.0, 0.0, 0.0}));
        dw.drones.push_back(
            make_drone(5, s0.position + glm::dvec3{-800.0, 200.0, 100.0}));
        return dw;
    };

    auto run = [&](bool with_hook) {
        app::LoopState st = flying(s0);
        app::Accumulator accum(kAp.sim_dt);
        app::DroneWorld dw = build_dw();
        combat::CombatWorld cw;
        app::ConquestWorld cq;
        double pdx = 0.0, pdy = 0.0;
        app::FrameInput fin;
        fin.throttle = thr;
        seads_tape::ConquestTape tape("test");
        for (int f = 0; f < 200; ++f) {
            app::step_frame(st, accum, kAp.sim_dt, fin, pdx, pdy, kAp, kCp,
                            nullptr, &dw, nullptr, &cw, nullptr, nullptr,
                            nullptr, &cq,
                            with_hook ? tape_hook_thunk : nullptr,
                            with_hook ? static_cast<void*>(&tape) : nullptr);
        }
        return std::make_pair(st, dw);
    };

    const auto a = run(true);
    const auto b = run(false);

    CHECK(a.first.curr.position == b.first.curr.position);
    CHECK(a.first.curr.velocity == b.first.curr.velocity);
    CHECK(a.first.curr.orientation == b.first.curr.orientation);
    REQUIRE(a.second.drones.size() == b.second.drones.size());
    for (std::size_t i = 0; i < a.second.drones.size(); ++i) {
        CHECK(a.second.drones[i].curr.position == b.second.drones[i].curr.position);
        CHECK(a.second.drones[i].curr.velocity == b.second.drones[i].curr.velocity);
    }
}

// ---------------------------------------------------------------------------
// 3. Enemy round spawn edge attributes the shooter.
TEST_CASE("enemy round spawn edge attributes the shooter") {
    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(make_drone(3, glm::dvec3{1000.0, 0.0, 0.0}));
    dw.drones.push_back(make_drone(7, glm::dvec3{9000.0, 9000.0, 9000.0}));

    combat::CombatWorld cw;
    weapon::Projectile inactive{};
    inactive.active = false;
    cw.enemy_pool.push_back(inactive);

    app::LoopState st = flying(sim::SimState{});
    st.curr.position = glm::dvec3{1000.0, 0.0, 300.0};  // player, 300 m off
    st.tick_count = 0;

    // Tick 0: init snapshot only, no events.
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    // Tick 1: the pool slot rises active AT drone 3's exact position (the
    // spawn contract: prev==pos).
    cw.enemy_pool[0].active = true;
    cw.enemy_pool[0].pos = dw.drones[0].curr.position;
    cw.enemy_pool[0].prev_pos = dw.drones[0].curr.position;
    dw.drones[0].foe = drone::kFoePlayer;
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    std::string out;
    tape.drain(out);
    const std::string ef = find_line(out, "ef", 1);
    REQUIRE_FALSE(ef.empty());
    CHECK(ef.find("\"shooter\":3") != std::string::npos);
    CHECK(ef.find("\"tgt_foe\":-2") != std::string::npos);
    // rng_p == |shooter pos - player pos| == 300 m exactly.
    CHECK(ef.find("\"rng_p\":300.000") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 4. Drone kill vs crash discrimination (§9 P0-2: dc is now an age_ticks
// RESET, never an inert-rise guess — a terrain crash in conquest never rises
// inert, drone::tick calls respawn_in_place unconditionally).
TEST_CASE("drone kill vs crash discrimination") {
    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(make_drone(1, glm::dvec3{100.0, 200.0, 300.0}));
    dw.drones.push_back(make_drone(2, glm::dvec3{5000.0, 0.0, 0.0}));
    dw.drones[1].age_ticks = 500;  // well into a flight, so a reset is visible

    combat::CombatWorld cw;
    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;

    // Tick 0: init snapshot (neither drone inert/killed/reset).
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    // Tick 1: drone 1 is killed by fire (kill-list membership; inert rises).
    const glm::dvec3 death_pos = dw.drones[0].curr.position;
    dw.drones[0].inert = true;
    cw.killed_spawn_indices.push_back(1);
    dw.drones[1].age_ticks = 501;  // drone 2 keeps flying normally this tick
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);
    cw.killed_spawn_indices.clear();  // non-empty only on the kill tick

    // Tick 2: drone 2 crashes into terrain and respawns (age_ticks resets to
    // 0; never touches `inert`, and its hp is UNCHANGED — the deleted
    // hp_reset/jump arm must not be what's doing the detecting here).
    const double hp_before = dw.drones[1].hp;
    dw.drones[1].age_ticks = 0;
    st.tick_count = 2;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    std::string out;
    tape.drain(out);
    const std::string dk = find_line(out, "dk", 1);
    REQUIRE_FALSE(dk.empty());
    CHECK(dk.find("\"i\":1") != std::string::npos);
    char pos_buf[64];
    std::snprintf(pos_buf, sizeof pos_buf, "\"pos\":[%.3f,%.3f,%.3f]",
                 death_pos.x, death_pos.y, death_pos.z);
    CHECK(dk.find(pos_buf) != std::string::npos);
    CHECK(find_line(out, "dc", 1).empty());  // no crash credited on the kill tick
    CHECK(find_line(out, "da", 1).empty());  // and no AI-vs-AI credit either

    const std::string dc = find_line(out, "dc", 2);
    REQUIRE_FALSE(dc.empty());
    CHECK(dc.find("\"i\":2") != std::string::npos);
    CHECK(find_line(out, "dk", 2).empty());  // drone 2 was never killed by fire
    CHECK(find_line(out, "da", 2).empty());  // and never AI-vs-AI credited
    CHECK(dw.drones[1].hp == hp_before);  // premise: hp truly never moved
}

// ---------------------------------------------------------------------------
// NEW (§9 TESTS): da vs dk discrimination — an inert rise WITHOUT kill-list
// membership (instructor_tick.h's AI-vs-AI gunnery site) must be tagged "da",
// never "dc" (the pre-amendment behavior) and never "dk".
TEST_CASE("da vs dk discrimination") {
    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(make_drone(4, glm::dvec3{10.0, 20.0, 30.0}));
    dw.drones.push_back(make_drone(9, glm::dvec3{40.0, 50.0, 60.0}));

    combat::CombatWorld cw;
    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);  // init snapshot

    // Tick 1: drone 4 is killed by fire (kill-list) -- dk.
    dw.drones[0].inert = true;
    cw.killed_spawn_indices.push_back(4);
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);
    cw.killed_spawn_indices.clear();

    // Tick 2: drone 9 is shot down AI-vs-AI (inert rises, NOT in the kill
    // list -- the instructor_tick.h gunnery site sets inert directly).
    dw.drones[1].inert = true;
    st.tick_count = 2;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    std::string out;
    tape.drain(out);
    REQUIRE_FALSE(find_line(out, "dk", 1).empty());
    CHECK(find_line(out, "da", 1).empty());
    CHECK(find_line(out, "dc", 1).empty());

    const std::string da = find_line(out, "da", 2);
    REQUIRE_FALSE(da.empty());
    CHECK(da.find("\"i\":9") != std::string::npos);
    CHECK(find_line(out, "dk", 2).empty());
    CHECK(find_line(out, "dc", 2).empty());
}

// ---------------------------------------------------------------------------
// NEW (§9 P0-1 regression pin): ef fires on a SAME-TICK slot-reuse fixture —
// a slot that is retired-and-refilled inside one enemy_fire_tick call never
// shows a true->true edge, which is exactly what the old slot-index detector
// (prev_pool_active_) missed 90% of the time. The spawn contract needs no
// snapshot at all: even the round's FIRST-ever appearance (tick 0) fires.
TEST_CASE("enemy round spawn edge fires on a same-tick slot-reuse fixture") {
    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(make_drone(2, glm::dvec3{500.0, 0.0, 0.0}));

    combat::CombatWorld cw;
    weapon::Projectile old_round{};
    old_round.active = true;
    old_round.age = 3.5;  // well past its own spawn tick
    old_round.pos = glm::dvec3{9000.0, 9000.0, 9000.0};
    old_round.prev_pos = glm::dvec3{8990.0, 9000.0, 9000.0};  // in flight
    cw.enemy_pool.push_back(old_round);

    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;
    // Tick 0: the pool already holds an in-flight round; no spawn contract
    // is met yet, so no ef (proves the fixture isn't vacuously firing).
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    // Tick 1: the SAME slot is retired and refilled with a brand-new round —
    // active stays true both ticks (true->true, the old edge detector's
    // blind spot), but the spawn contract (age==0, prev_pos==pos) is met.
    cw.enemy_pool[0].age = 0.0;
    cw.enemy_pool[0].pos = dw.drones[0].curr.position;
    cw.enemy_pool[0].prev_pos = dw.drones[0].curr.position;
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, nullptr);

    std::string out;
    tape.drain(out);
    CHECK(find_line(out, "ef", 0).empty());  // premise: the in-flight round is silent
    const std::string ef = find_line(out, "ef", 1);
    REQUIRE_FALSE(ef.empty());  // the P0-1 regression pin: the reused slot IS caught
    CHECK(ef.find("\"shooter\":2") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 5. Tunnel transition edges both ways for player and drone.
TEST_CASE("tunnel transition edges both ways for player and drone") {
    const world::TunnelNet net = world::build_tunnel_net(test_tp(), nullptr);
    const glm::dvec3 inside = net.arena.center;
    REQUIRE(net.contains(inside));  // premise: genuinely inside
    const glm::dvec3 outside =
        glm::normalize(glm::dvec3{-1.0, 0.3, 0.1}) * (kAp.R + 5000.0);
    REQUIRE_FALSE(net.contains(outside));  // premise: genuinely outside

    sim::Environment env;
    env.tunnels = &net;

    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(make_drone(4, outside));

    app::LoopState st = flying(sim::SimState{});
    st.curr.position = outside;
    st.tick_count = 0;
    tape.on_tick(noop_in(), st, &dw, nullptr, nullptr, &env);  // init snapshot

    // Tick 1: both move inside.
    dw.drones[0].curr.position = inside;
    st.curr.position = inside;
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, &dw, nullptr, nullptr, &env);

    // Tick 2: both move back outside.
    dw.drones[0].curr.position = outside;
    st.curr.position = outside;
    st.tick_count = 2;
    tape.on_tick(noop_in(), st, &dw, nullptr, nullptr, &env);

    std::string out;
    tape.drain(out);

    bool found_drone_in = false, found_player_in = false;
    bool found_drone_out = false, found_player_out = false;
    for (const std::string& line : split_lines(out)) {
        if (line.find("\"t\":\"tn\"") == std::string::npos) continue;
        const bool tick1 = line.find("\"k\":1,") != std::string::npos;
        const bool tick2 = line.find("\"k\":2,") != std::string::npos;
        const bool who_drone = line.find("\"who\":4,") != std::string::npos;
        const bool who_player = line.find("\"who\":-2,") != std::string::npos;
        if (tick1 && who_drone && line.find("\"in\":1") != std::string::npos)
            found_drone_in = true;
        if (tick1 && who_player && line.find("\"in\":1") != std::string::npos)
            found_player_in = true;
        if (tick2 && who_drone && line.find("\"in\":0") != std::string::npos)
            found_drone_out = true;
        if (tick2 && who_player && line.find("\"in\":0") != std::string::npos)
            found_player_out = true;
    }
    CHECK(found_drone_in);
    CHECK(found_player_in);
    CHECK(found_drone_out);
    CHECK(found_player_out);
}

// ---------------------------------------------------------------------------
// 6. Sample cadence is every 24th tick and only that.
TEST_CASE("sample cadence is every 24th tick and only that") {
    seads_tape::ConquestTape tape("test");
    app::LoopState st = flying(sim::SimState{});
    for (long long k = 0; k <= 48; ++k) {
        st.tick_count = k;
        tape.on_tick(noop_in(), st, nullptr, nullptr, nullptr, nullptr);
    }
    std::string out;
    tape.drain(out);
    CHECK(count_lines_with_tag(out, "p") == 3);  // ticks 0, 24, 48
    CHECK_FALSE(find_line(out, "p", 0).empty());
    CHECK_FALSE(find_line(out, "p", 24).empty());
    CHECK_FALSE(find_line(out, "p", 48).empty());
    CHECK(find_line(out, "p", 12).empty());
    CHECK(find_line(out, "p", 23).empty());
}

// ---------------------------------------------------------------------------
// 7. Footer signature matches a recomputed fnv1a over the drained body.
TEST_CASE("footer signature matches a recomputed fnv1a over the drained body") {
    seads_tape::ConquestTape tape("test");
    app::LoopState st = flying(sim::SimState{});
    for (long long k = 0; k <= 30; ++k) {
        st.tick_count = k;
        tape.on_tick(noop_in(), st, nullptr, nullptr, nullptr, nullptr);
    }
    std::string body;
    tape.drain(body);
    // Strip the hdr line (the sig hashes body bytes AFTER hdr, spec §3.0).
    const std::size_t nl = body.find('\n');
    REQUIRE(nl != std::string::npos);
    const std::string hdr = body.substr(0, nl + 1);
    const std::string rest = body.substr(nl + 1);
    REQUIRE(hdr.find("\"t\":\"hdr\"") != std::string::npos);

    const std::uint64_t recomputed =
        seads_tape::fnv1a_update(seads_tape::kFnvOffsetBasis, rest);
    const std::string footer = tape.footer();
    const std::string want =
        "\"fnv1a\":\"" + std::to_string(recomputed) + "\"";
    CHECK(footer.find(want) != std::string::npos);

    // Corrupt one byte of the body (not the hdr) and confirm the recomputed
    // hash over the corrupted bytes no longer matches the footer.
    std::string corrupted = rest;
    REQUIRE_FALSE(corrupted.empty());
    corrupted[corrupted.size() / 2] =
        static_cast<char>(corrupted[corrupted.size() / 2] + 1);
    const std::uint64_t bad_hash =
        seads_tape::fnv1a_update(seads_tape::kFnvOffsetBasis, corrupted);
    REQUIRE(bad_hash != recomputed);
}

// ---------------------------------------------------------------------------
// 8. Pump death and bubble rows fire on the radius-change tick.
TEST_CASE("pump death and bubble rows fire on the radius change tick") {
    seads_tape::ConquestTape tape("test");
    app::ConquestWorld cq;
    cq.state.pumps[0].hp = 10.0;
    cq.state.pumps[0].alive = true;
    cq.state.pumps[0].faction = combat::CQ_VALLEY;

    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;
    // First on_tick: bub row unconditionally (spec §2.1), no pk yet.
    tape.on_tick(noop_in(), st, nullptr, nullptr, &cq, nullptr);

    // Tick 1: the pump dies AND the destroyer's dome grows (radius_scale
    // change), between ticks — hand-set, mirroring damage_pump's bookkeeping.
    cq.state.pumps[0].alive = false;
    cq.state.radius_scale[combat::CQ_SUDBURY] += 0.15;
    st.tick_count = 1;
    tape.on_tick(noop_in(), st, nullptr, nullptr, &cq, nullptr);

    std::string out;
    tape.drain(out);
    const std::string pk = find_line(out, "pk", 1);
    REQUIRE_FALSE(pk.empty());
    CHECK(pk.find("\"pump\":0") != std::string::npos);
    CHECK(pk.find("\"fac\":0") != std::string::npos);  // CQ_VALLEY == 0

    CHECK_FALSE(find_line(out, "bub", 0).empty());  // first-tick bub row
    CHECK_FALSE(find_line(out, "bub", 1).empty());  // radius-change bub row
}

// ---------------------------------------------------------------------------
// 9. RUNG B2 — THE FLOWN-COMMAND WITNESS ON THE DRONE ROW (ta/gc/bc/af).
//
// Not a presence check: the drone is flown for one real drone::tick under a
// height field that puts it 100 m AGL, well inside avoid_agl_enter_m, so the
// terrain-avoid latch owns the command on that very tick. The latch's own
// signature is target_gamma == dp.avoid_gamma exactly, so the row's "gc" must
// read back avoid_gamma. That pins the SEAM, not the format: if a later
// producer writes target_gamma after the mirror at drone.h's autopilot call,
// gc stops matching the flown command and this leg goes red.
TEST_CASE("conquest tape drone row carries the B2 command witness") {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = 4000.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    // A deliberately THIN deck (10 m full, gone by 30 m) so the drone's 100 m
    // AGL station reads air ~0, not the trivial 1.0 a bare sphere would give:
    // that makes the "af" leg below able to fail against a hardcoded 1.0, not
    // only against a hardcoded 0.5.
    sim::AtmosphereField atm;
    atm.deck_agl_m = 10.0;
    atm.deck_soft_m = 20.0;
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = sim::GroundParams{};
    env.atm = &atm;

    drone::DroneParams dp;
    dp.maverick.enabled = false;

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    drone::DroneState d =
        make_drone(3, up * (hf.radius_at(up) + 100.0), glm::dvec3{0.0});
    d.curr = drone::level_state_at(dp, d.curr.position, glm::dvec3{1.0, 0.0, 0.0});
    d.prev = d.curr;

    sim::SimState player = d.curr;
    player.position = d.curr.position + glm::dvec3{500.0, 0.0, 0.0};
    drone::tick(d, kAp, dp, &env, &player);
    REQUIRE(d.terrain_avoid_engaged);  // non-vacuous: the latch really ran

    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(d);
    combat::CombatWorld cw;
    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, &env);

    std::string out;
    tape.drain(out);
    const std::string row = find_line(out, "d", 0);
    REQUIRE_FALSE(row.empty());

    // The four keys exist on the row (the analyzer's contract).
    CHECK(row.find("\"ta\":") != std::string::npos);
    CHECK(row.find("\"gc\":") != std::string::npos);
    CHECK(row.find("\"bc\":") != std::string::npos);
    CHECK(row.find("\"af\":") != std::string::npos);

    // The latch is engaged, so ta is 1 and gc IS avoid_gamma, to the row's
    // own %.4f precision.
    CHECK(row.find("\"ta\":1") != std::string::npos);
    char gc_buf[64];
    std::snprintf(gc_buf, sizeof gc_buf, "\"gc\":%.4f", dp.avoid_gamma);
    CHECK(row.find(gc_buf) != std::string::npos);

    // And the mirror on the state agrees with the row, so a probe reading the
    // state and a tape reading the row can never fork.
    CHECK(d.cmd_gamma == Catch::Approx(dp.avoid_gamma));
    const double air_here = sim::atm_frac_at(d.curr.position, &env, kAp);
    REQUIRE(air_here < 0.5);  // non-vacuous: this station is genuinely thin
    CHECK(d.air_frac == Catch::Approx(air_here));
    char af_buf[64];
    std::snprintf(af_buf, sizeof af_buf, "\"af\":%.3f", air_here);
    CHECK(row.find(af_buf) != std::string::npos);

    // ★ RUNG S1-DECK: the row carries the DECK-SCOPE bit too. It is a pure
    // OBSERVATION of the world (is there air at the 400 m release altitude
    // over my ground point?), so it reads 1 at this dead-air station even
    // with the deck band itself at its struct-default OFF — and the latch that
    // actually fired is still the SHIPPED 250 m one (proved by the gc check
    // above, which read back avoid_gamma at 100 m AGL).
    CHECK(row.find("\"ds\":") != std::string::npos);
    CHECK(row.find("\"ds\":1") != std::string::npos);
    CHECK(d.deck_scope);
}

// ★★★ RUNG S1-DECK — THE DECK-SCOPE LATCH, ON THE ROW AND IN THE STATE.
// The SAME station as the B2 case above (100 m AGL, a 10/30 m deck, so the
// air at the shipped 400 m release altitude is zero) — the only thing that
// changes is that the deck band is armed. It must flip `ds` to 1 AND swap the
// latch band: at 100 m AGL the drone is ABOVE deck_avoid_agl_enter_m (60) and
// BELOW avoid_agl_enter_m (250), so the shipped band latches and the deck band
// does not. That makes the two bands distinguishable on one tick.
TEST_CASE("conquest tape drone row carries the S1-DECK scope bit") {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = 0.0;  // flat: nothing ahead for the forward eyes
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 0);
    sim::AtmosphereField atm;
    atm.deck_agl_m = 10.0;
    atm.deck_soft_m = 20.0;
    sim::Environment env;
    env.ground = &hf;
    env.ground_params = sim::GroundParams{};
    env.atm = &atm;

    drone::DroneParams dp;
    dp.maverick.enabled = false;
    dp.deck_avoid_agl_enter_m = 60.0;
    dp.deck_avoid_agl_release_m = 110.0;
    dp.deck_track_agl_m = 100.0;

    const glm::dvec3 up = glm::normalize(glm::dvec3{0.0, 0.0, 1.0});
    drone::DroneState d =
        make_drone(3, up * (hf.radius_at(up) + 100.0), glm::dvec3{0.0});
    d.curr = drone::level_state_at(dp, d.curr.position, glm::dvec3{1.0, 0.0, 0.0});
    d.prev = d.curr;
    sim::SimState player = d.curr;
    player.position = d.curr.position + glm::dvec3{500.0, 0.0, 0.0};
    drone::tick(d, kAp, dp, &env, &player);

    CHECK(d.deck_scope);  // no air at 400 m AGL here => the deck band is live
    // AND the band actually swapped: 100 m AGL is inside the shipped 250 m
    // enter but OUTSIDE the deck 60 m enter, so the pull-up is NOT engaged.
    CHECK_FALSE(d.terrain_avoid_engaged);

    seads_tape::ConquestTape tape("test");
    app::DroneWorld dw;
    dw.drones.push_back(d);
    combat::CombatWorld cw;
    app::LoopState st = flying(sim::SimState{});
    st.tick_count = 0;
    tape.on_tick(noop_in(), st, &dw, &cw, nullptr, &env);
    std::string out;
    tape.drain(out);
    const std::string row = find_line(out, "d", 0);
    REQUIRE_FALSE(row.empty());
    CHECK(row.find("\"ds\":1") != std::string::npos);
    CHECK(row.find("\"ta\":0") != std::string::npos);
}

// ---------------------------------------------------------------------------
// RUNG S3-GUNS: the player row carries the COSMETIC round count.
//
// This is the "one number" the fly report quotes for "are there enemy rounds in
// the air?". It is a strict OBSERVATION: cosmetic rounds carry damage 0.0 and
// are handed to no damage sweep (combat/kill.h), so `cr` can never explain a
// point of lost HP — it explains the LOOK of one. Two clauses: the field is
// emitted with the live value, and it is emitted with 0 when there is no
// CombatWorld (the tests/harness path, which must stay well-formed).
// MUTATION (run, went RED): emit `alive` in cr's place -> the tick-24 line reads
//   "cr":1 and the CHECK fails.
TEST_CASE("conquest tape: the player row carries the cosmetic round count") {
    seads_tape::ConquestTape tape("test");
    app::LoopState st = flying(sim::SimState{});
    st.curr.position = glm::dvec3{1000.0, 0.0, 300.0};

    combat::CombatWorld cw;
    cw.cosmetic_rounds = 0;
    st.tick_count = 0;
    tape.on_tick(noop_in(), st, nullptr, &cw, nullptr, nullptr);
    cw.cosmetic_rounds = 4173;  // a value nothing else on this row can be
    st.tick_count = 24;  // sample rows land on multiples of kSampleEvery
    tape.on_tick(noop_in(), st, nullptr, &cw, nullptr, nullptr);
    // No CombatWorld at all: the field must still be there, at 0.
    st.tick_count = 48;
    tape.on_tick(noop_in(), st, nullptr, nullptr, nullptr, nullptr);

    std::string out;
    tape.drain(out);
    const std::string p0 = find_line(out, "p", 0);
    const std::string p7 = find_line(out, "p", 24);
    const std::string p9 = find_line(out, "p", 48);
    REQUIRE_FALSE(p0.empty());
    REQUIRE_FALSE(p7.empty());
    REQUIRE_FALSE(p9.empty());
    CHECK(p0.find("\"cr\":0") != std::string::npos);
    CHECK(p7.find("\"cr\":4173") != std::string::npos);
    CHECK(p9.find("\"cr\":0") != std::string::npos);
}
