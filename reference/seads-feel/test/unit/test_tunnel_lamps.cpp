// T5c — DESTRUCTIBLE GASLAMPS (docs/tunnel_staging.md rung T5; render/
// tunnel_lamp_hits.h). The pure swept-segment-vs-lamp hit pass: a fired round
// whose [prev_pos, pos] path passes within kLampHitRadius of a lamp kills it
// (dark spot) and is consumed. PURE (glm + std). Every leg REQUIRE/CHECK-based.
//
// Discipline (CLAUDE.md ## Learned): the segment (not endpoint-only) tunneling
// case is pinned explicitly; radius + segment mutations verified; the round
// retires on a hit; dead lamps are excluded from the rebuilt placement list.
//
// Also: P1-2 differential firewall leg (red-team fold) — step_frame N ticks
// with lw populated vs lw=nullptr, no firing, yields BIT-IDENTICAL sim state.
// Catches a future refactor that reads lamp state into the tick path.

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <glm/glm.hpp>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "render/tunnel_lamp_hits.h"
#include "weapon/ballistics.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kApL =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCpL =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kApL);

render::TunnelLampWorld make_world(const std::vector<glm::dvec3>& positions,
                                   float intensity = render::kTubeLampIntensity) {
    std::vector<render::TunnelLamp> lamps;
    for (const glm::dvec3& p : positions)
        lamps.push_back(render::TunnelLamp{p, intensity});
    render::TunnelLampWorld w;
    w.init(std::move(lamps));
    return w;
}

// One active round with a swept segment prev->pos.
weapon::Projectile round_seg(const glm::dvec3& prev, const glm::dvec3& pos) {
    weapon::Projectile p;
    p.prev_pos = prev;
    p.pos = pos;
    p.vel = pos - prev;
    p.active = true;
    return p;
}

}  // namespace

TEST_CASE("T5c lamp hit: a direct hit kills exactly one lamp + retires round") {
    // Two lamps 100 m apart; a round passing THROUGH the first's position kills
    // exactly it (the nearer, lower-index) and the round is consumed.
    const glm::dvec3 a{0.0, 0.0, 0.0};
    const glm::dvec3 b{100.0, 0.0, 0.0};
    render::TunnelLampWorld w = make_world({a, b});

    std::vector<weapon::Projectile> pool;
    // A segment straight through lamp a (from -10 to +10 in x, at y=z=0).
    pool.push_back(
        round_seg(glm::dvec3{-10.0, 0.0, 0.0}, glm::dvec3{10.0, 0.0, 0.0}));

    const int killed =
        render::lamp_hits(pool, w, render::kLampHitRadius);
    REQUIRE(killed == 1);
    REQUIRE(!w.alive[0]);  // lamp a died
    REQUIRE(w.alive[1]);   // lamp b survived
    REQUIRE(w.dirty);      // renderer rebuild pending
    REQUIRE(!pool[0].active);  // the round is consumed
}

TEST_CASE("T5c lamp hit: a near-miss just past the radius kills nothing") {
    // A round whose closest approach is kLampHitRadius + eps misses. Mutation
    // (radius doubled) would turn this into a hit — pinned by the alive check.
    const glm::dvec3 lamp{0.0, 0.0, 0.0};
    render::TunnelLampWorld w = make_world({lamp});

    const double miss = render::kLampHitRadius + 0.5;  // just past the radius
    std::vector<weapon::Projectile> pool;
    // A segment parallel to x at y = miss: closest approach == miss.
    pool.push_back(
        round_seg(glm::dvec3{-10.0, miss, 0.0}, glm::dvec3{10.0, miss, 0.0}));

    const int killed = render::lamp_hits(pool, w, render::kLampHitRadius);
    REQUIRE(killed == 0);
    REQUIRE(w.alive[0]);
    REQUIRE(!w.dirty);
    REQUIRE(pool[0].active);  // the round lives on (no hit)

    // Premise arm: JUST INSIDE the radius the SAME geometry IS a hit (so the
    // near-miss is a real boundary, not vacuously outside every plausible test).
    render::TunnelLampWorld w2 = make_world({lamp});
    const double hit = render::kLampHitRadius - 0.5;
    std::vector<weapon::Projectile> pool2;
    pool2.push_back(
        round_seg(glm::dvec3{-10.0, hit, 0.0}, glm::dvec3{10.0, hit, 0.0}));
    REQUIRE(render::lamp_hits(pool2, w2, render::kLampHitRadius) == 1);
}

TEST_CASE("T5c lamp hit: the TUNNELING case (segment through, endpoints out)") {
    // A fast round whose prev_pos AND pos both sit OUTSIDE the lamp sphere but
    // whose path passes THROUGH it between ticks. An endpoint-only test misses
    // it; the swept-segment test catches it.
    const glm::dvec3 lamp{0.0, 0.0, 0.0};
    render::TunnelLampWorld w = make_world({lamp});

    // Endpoints 100 m out on either side (way past kLampHitRadius ~ 8 m), path
    // straight through the center.
    const glm::dvec3 prev{-100.0, 0.0, 0.0};
    const glm::dvec3 pos{100.0, 0.0, 0.0};
    // Premise: BOTH endpoints are outside the hit sphere (the tunneling setup).
    REQUIRE(glm::length(prev - lamp) > render::kLampHitRadius);
    REQUIRE(glm::length(pos - lamp) > render::kLampHitRadius);

    std::vector<weapon::Projectile> pool{round_seg(prev, pos)};
    const int killed = render::lamp_hits(pool, w, render::kLampHitRadius);
    REQUIRE(killed == 1);   // the swept segment caught it
    REQUIRE(!w.alive[0]);
    REQUIRE(!pool[0].active);
}

TEST_CASE("T5c lamp hit: dead lamps are excluded from the rebuilt placement") {
    // After a kill, alive_positions() (what the renderer rebuilds from) omits
    // the dead lamp; the survivors remain.
    render::TunnelLampWorld w = make_world(
        {glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{200.0, 0.0, 0.0},
         glm::dvec3{400.0, 0.0, 0.0}});

    std::vector<weapon::Projectile> pool{round_seg(
        glm::dvec3{-5.0, 0.0, 0.0}, glm::dvec3{5.0, 0.0, 0.0})};  // hits lamp 0
    REQUIRE(render::lamp_hits(pool, w, render::kLampHitRadius) == 1);
    REQUIRE(!w.alive[0]);

    // The dim tier (all three are kTubeLampIntensity < the bright cut) rebuilds
    // from the two survivors only.
    const std::vector<glm::dvec3> dim =
        w.alive_positions(2.0f, /*bright=*/false);
    REQUIRE(dim.size() == 2);
    for (const glm::dvec3& p : dim) REQUIRE(p.x != 0.0);  // lamp 0 excluded
    // The bright tier is empty (no lamp >= the cut).
    REQUIRE(w.alive_positions(2.0f, /*bright=*/true).empty());
}

TEST_CASE("T5c lamp hit: one round kills at most one lamp (consumed on first)") {
    // Two lamps very close (< 2*radius) so a single segment could reach both;
    // the round kills only the FIRST (lower index) and retires.
    render::TunnelLampWorld w =
        make_world({glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{5.0, 0.0, 0.0}});
    std::vector<weapon::Projectile> pool{
        round_seg(glm::dvec3{-10.0, 0.0, 0.0}, glm::dvec3{10.0, 0.0, 0.0})};
    REQUIRE(render::lamp_hits(pool, w, render::kLampHitRadius) == 1);
    REQUIRE(!w.alive[0]);  // first killed
    REQUIRE(w.alive[1]);   // second survives (round already consumed)
}

TEST_CASE("T5c lamp hit: an inactive round is ignored") {
    render::TunnelLampWorld w = make_world({glm::dvec3{0.0, 0.0, 0.0}});
    std::vector<weapon::Projectile> pool{round_seg(
        glm::dvec3{-5.0, 0.0, 0.0}, glm::dvec3{5.0, 0.0, 0.0})};
    pool[0].active = false;  // already retired
    REQUIRE(render::lamp_hits(pool, w, render::kLampHitRadius) == 0);
    REQUIRE(w.alive[0]);  // untouched
}

// ===========================================================================
// T-perf BROAD-PHASE (render/tunnel_lamp_hits.h TunnelLampWorld::rebuild_cloud
// + lamp_hits): a bounding sphere over the alive-lamp cloud, inflated by the
// hit radius, gates the per-lamp inner loop so a round nowhere near the tube's
// lamp string skips it entirely. Behavior must be EXACTLY unchanged for any
// round that could actually hit a lamp.
// ===========================================================================
TEST_CASE("T-perf lamp hit: broad-phase skips a round far from the lamp cloud") {
    const glm::dvec3 a{0.0, 0.0, 0.0};
    const glm::dvec3 b{1000.0, 0.0, 0.0};
    render::TunnelLampWorld w = make_world({a, b});

    // Cloud center sits at (500,0,0), cloud radius ~= 500 + kLampHitRadius.
    // This segment is thousands of metres away on every axis -- a clean miss
    // of the cloud sphere, well before any per-lamp test would matter.
    std::vector<weapon::Projectile> pool;
    pool.push_back(
        round_seg(glm::dvec3{5000.0, 0.0, 0.0}, glm::dvec3{5020.0, 0.0, 0.0}));

    const int killed = render::lamp_hits(pool, w, render::kLampHitRadius);
    REQUIRE(killed == 0);
    REQUIRE(w.alive[0]);
    REQUIRE(w.alive[1]);
    REQUIRE(!w.dirty);
    REQUIRE(pool[0].active);
}

TEST_CASE(
    "T-perf lamp hit: a round grazing the cloud sphere's margin still "
    "registers the real hit") {
    // Lamp A at the origin, lamp B far away at (1000,0,0): the cloud center
    // sits at (500,0,0) and the BARE lamp-extent radius (no hit-radius
    // margin) is exactly 500 m. A round that hits lamp A from the far side
    // (away from the cloud center, along -X) sits at distance
    // ~(500 + kLampHitRadius - 0.5) from the cloud center: inside the
    // correctly-inflated (500 + kLampHitRadius) sphere by only 0.5 m, but
    // OUTSIDE a bare 500 m one. Mutation lever: dropping the `+margin` term
    // in TunnelLampWorld::rebuild_cloud makes this test fail (the
    // broad-phase would incorrectly reject a genuine hit near the cloud's
    // far edge).
    const glm::dvec3 a{0.0, 0.0, 0.0};
    const glm::dvec3 b{1000.0, 0.0, 0.0};
    render::TunnelLampWorld w = make_world({a, b});

    const double r = render::kLampHitRadius;
    const glm::dvec3 pt{-(r - 0.5), 0.0, 0.0};  // r-0.5 m from A -> a real hit
    std::vector<weapon::Projectile> pool{round_seg(pt, pt)};

    const int killed = render::lamp_hits(pool, w, r);
    REQUIRE(killed == 1);
    REQUIRE(!w.alive[0]);
    REQUIRE(w.alive[1]);
    REQUIRE(w.dirty);
    REQUIRE(!pool[0].active);
}

TEST_CASE("T-perf lamp hit: zero alive lamps skips the whole pass") {
    render::TunnelLampWorld w = make_world({glm::dvec3{0.0, 0.0, 0.0}});
    std::vector<weapon::Projectile> pool{
        round_seg(glm::dvec3{-5.0, 0.0, 0.0}, glm::dvec3{5.0, 0.0, 0.0})};
    // Kill the only lamp first (direct hit), then fire the same shot again --
    // the second pass has zero alive lamps and must be a clean no-op.
    REQUIRE(render::lamp_hits(pool, w, render::kLampHitRadius) == 1);
    REQUIRE(!w.alive[0]);
    w.dirty = false;  // simulate the renderer having consumed the rebuild
    pool[0].active = true;
    pool[0].prev_pos = glm::dvec3{-5.0, 0.0, 0.0};
    pool[0].pos = glm::dvec3{5.0, 0.0, 0.0};
    REQUIRE(render::lamp_hits(pool, w, render::kLampHitRadius) == 0);
    REQUIRE(!w.dirty);
    REQUIRE(pool[0].active);
}

// ===========================================================================
// T-perf CLUSTERED BROAD-PHASE: the single giant cloud sphere over the WHOLE
// Errington->Murray lamp string is replaced with small per-index-range
// clusters, so the early-out fires in the near field too (the pilot's
// firing-near-the-tunnel stutter). Hit SEMANTICS are exactly preserved.
// ===========================================================================

// Two far-apart lamp groups on the x-axis (all at y=z=0). Group 1 spans
// x=0..624 (indices 0..39, 16 m spacing); group 2 spans x=5000..5624 (indices
// 40..79). A brute-force all-lamps oracle: the lowest-index lamp whose sphere
// the segment [a,b] pierces (-1 if none).
namespace {
std::vector<glm::dvec3> two_group_positions() {
    std::vector<glm::dvec3> pos;
    for (int i = 0; i < 40; ++i) pos.push_back({16.0 * i, 0.0, 0.0});
    for (int i = 0; i < 40; ++i) pos.push_back({5000.0 + 16.0 * i, 0.0, 0.0});
    return pos;
}
int brute_lowest_hit(const render::TunnelLampWorld& w, const glm::dvec3& a,
                     const glm::dvec3& b, double r) {
    for (std::size_t i = 0; i < w.lamps.size(); ++i)
        if (w.alive[i] && render::seg_sphere_hit(a, b, w.lamps[i].pos, r))
            return static_cast<int>(i);
    return -1;
}
}  // namespace

TEST_CASE("T-perf lamp hit: grazing only the far cluster kills the oracle lamp") {
    render::TunnelLampWorld w = make_world(two_group_positions());
    const double r = render::kLampHitRadius;

    // A round straight through lamp 50 (x = 5000 + 10*16 = 5160), far from
    // every group-1 lamp (>4000 m away in x).
    const glm::dvec3 prev{5160.0, -10.0, 0.0};
    const glm::dvec3 pos{5160.0, 10.0, 0.0};
    const int oracle = brute_lowest_hit(w, prev, pos, r);
    REQUIRE(oracle == 50);  // fixture premise

    std::vector<weapon::Projectile> pool{round_seg(prev, pos)};
    REQUIRE(render::lamp_hits(pool, w, r) == 1);
    // The clustered pass kills EXACTLY the lamp the brute oracle names.
    REQUIRE(!w.alive[static_cast<std::size_t>(oracle)]);
    for (std::size_t i = 0; i < w.alive.size(); ++i)
        if (static_cast<int>(i) != oracle) REQUIRE(w.alive[i]);
    REQUIRE(!pool[0].active);
    REQUIRE(w.clusters.size() > 1);  // it really did cluster
}

TEST_CASE("T-perf lamp hit: a round BETWEEN clusters (inside the old giant "
          "sphere) still returns 0 hits") {
    render::TunnelLampWorld w = make_world(two_group_positions());
    const double r = render::kLampHitRadius;

    // The midpoint between the groups. The OLD single-cloud sphere had center
    // ~x=2812 and radius ~2812+r, so this point sat comfortably INSIDE it (the
    // old early-out would have paid the full 80-lamp inner loop). It is >2000 m
    // from the nearest lamp, so it misses every small cluster sphere.
    const glm::dvec3 mid{2812.0, 0.0, 0.0};
    std::vector<weapon::Projectile> pool{
        round_seg(mid, mid + glm::dvec3{20.0, 0.0, 0.0})};

    REQUIRE(render::lamp_hits(pool, w, r) == 0);
    for (bool a : w.alive) REQUIRE(a);
    REQUIRE(!w.dirty);
    REQUIRE(pool[0].active);
    // Structural perf pin: a mutant that collapses clustering back to ONE giant
    // sphere would still cover this segment and pay the full inner loop -- the
    // >1 cluster count makes that regression visible.
    REQUIRE(w.clusters.size() > 1);
    // Premise: brute force also finds no hit (this really is a clean miss,
    // just one the old single sphere failed to early-out).
    REQUIRE(brute_lowest_hit(w, pool[0].prev_pos, pool[0].pos, r) == -1);
}

TEST_CASE("T-perf lamp hit: lowest-index wins ACROSS clusters (iteration order)") {
    render::TunnelLampWorld w = make_world(two_group_positions());
    const double r = render::kLampHitRadius;

    // A long segment along the x-axis passing through a hittable lamp in BOTH
    // groups (lamp 0 at x=0 and lamp 40 at x=5000). The lower GLOBAL index must
    // win -- pins that clusters are scanned in index order (a proximity- or
    // reverse-ordered mutant would kill a group-2 lamp instead).
    const glm::dvec3 prev{-10.0, 0.0, 0.0};
    const glm::dvec3 pos{5200.0, 0.0, 0.0};
    REQUIRE(render::seg_sphere_hit(prev, pos, w.lamps[0].pos, r));   // group 1
    REQUIRE(render::seg_sphere_hit(prev, pos, w.lamps[40].pos, r));  // group 2

    w.rebuild_cloud(r);
    REQUIRE(w.clusters.size() > 1);  // the two hittable lamps live in different
                                     // clusters (cross-cluster, not intra)
    REQUIRE(brute_lowest_hit(w, prev, pos, r) == 0);  // oracle agrees

    std::vector<weapon::Projectile> pool{round_seg(prev, pos)};
    REQUIRE(render::lamp_hits(pool, w, r) == 1);
    REQUIRE(!w.alive[0]);   // lowest global index killed
    REQUIRE(w.alive[40]);   // the far-cluster lamp survives (round consumed)
    REQUIRE(!pool[0].active);
}

// ===========================================================================
// P1-2 DIFFERENTIAL FIREWALL — step_frame with lw populated vs lw=nullptr,
// NO firing, yields BIT-IDENTICAL player LoopState (curr + prev).
//
// The lamp-world tick path (render::lamp_hits) is FIREWALLED: nothing it does
// feeds control/sim state. This leg catches a future refactor that violates
// that contract by reading lamp state into the tick path. It mirrors the
// test_combat.cpp "cw=nullptr is bit-identical" shape (the proven pattern for
// render-world firewall legs in this codebase).
//
// Position lamps far from the spawn flight path (lamps sit on the -Y world
// axis; spawn state is at +X pole flying -Z) so none fall within
// kLampHitRadius of any projectile trajectory even if rounds were fired.
// fire_held=false ensures the GunWorld pool stays empty every tick, so
// lamp_hits is a no-op in BOTH arms — the alive[] invariant leg verifies this.
// A future refactor that e.g. reads lw->alive[0] into a tick branch would
// diverge the two LoopStates even with fire_held=false.
//
// Mutation-thought-check: a refactor inserting `if (lw && lw->lamps.size())
// st.curr.throttle = 0;` would cause curr.throttle to differ between arms,
// failing the memcmp — the leg catches it.
// ===========================================================================
TEST_CASE("T5c lw=nullptr is bit-identical to lw-populated, no firing (firewall)") {
    app::LoopState st_with{}, st_without{};
    const sim::SimState spawn = app::spawn_state(kApL);
    st_with.curr = st_without.curr = spawn;
    st_with.prev = st_without.prev = spawn;
    st_with.prev_up = st_without.prev_up = sim::local_up(spawn.position);
    st_with.aim.reseed(spawn.orientation, st_with.prev_up);
    st_without.aim.reseed(spawn.orientation, st_without.prev_up);
    st_with.grounded = st_without.grounded = true;

    // A GunWorld with one gun but fire_held=false — the pool stays empty every
    // tick, so lamp_hits is called with an empty pool (no-op in both arms).
    weapon::GunWorld gw_with;
    weapon::GunSpec gs;
    gs.muzzle_body = {0.0, 0.0, -2.87};
    gs.kind = weapon::Round::Cannon20mm;
    gs.muzzle_speed = 805.0;
    gs.rof_hz = 12.0;
    gs.drag_k = 0.0008;
    gs.damage = 30.0;
    gw_with.battery.guns.push_back(gs);
    gw_with.battery.convergence_range = 500.0;

    weapon::GunWorld gw_without;
    gw_without.battery.guns.push_back(gs);
    gw_without.battery.convergence_range = 500.0;

    // Three lamps on the -Y world axis, far from the +X-pole spawn trajectory.
    // They must never fall within kLampHitRadius of any round even if fired
    // (>1000 m separation from the spawn point).
    render::TunnelLampWorld lw = make_world({
        glm::dvec3{0.0, -14000.0, 0.0},
        glm::dvec3{0.0, -14100.0, 0.0},
        glm::dvec3{0.0, -14200.0, 0.0},
    });
    // Premise: all lamps start alive.
    REQUIRE(lw.alive.size() == 3);
    for (bool a : lw.alive) REQUIRE(a);
    REQUIRE(!lw.dirty);

    app::Accumulator accum_with(kApL.sim_dt, 0.25);
    app::Accumulator accum_without(kApL.sim_dt, 0.25);
    double pdx_w = 0.0, pdy_w = 0.0, pdx_n = 0.0, pdy_n = 0.0;

    // 100 ticks, hands-off (no firing, no override).
    for (int i = 0; i < 100; ++i) {
        app::FrameInput fin;
        fin.fire_held = false;
        fin.throttle = 1.0;
        app::step_frame(st_with, accum_with, kApL.sim_dt, fin, pdx_w, pdy_w,
                        kApL, kCpL, nullptr, nullptr, &gw_with, nullptr, &lw);
        app::step_frame(st_without, accum_without, kApL.sim_dt, fin, pdx_n,
                        pdy_n, kApL, kCpL, nullptr, nullptr, &gw_without,
                        nullptr, nullptr);
    }

    // Player state must be bit-identical — lw is a render-world firewall.
    CHECK(std::memcmp(&st_with.curr, &st_without.curr, sizeof(sim::SimState)) ==
          0);
    CHECK(std::memcmp(&st_with.prev, &st_without.prev, sizeof(sim::SimState)) ==
          0);

    // The lamp world is untouched: no firing => no hits => all alive, not dirty.
    for (bool a : lw.alive) CHECK(a);
    CHECK(!lw.dirty);
}
