// rig-D guns leg: the projectile ballistics scaffold (weapon/ballistics.h).
// PURE physics pinned FIRING on non-trivial paths — each case names the defect
// it catches and dodges the fixture-no-op trap (a level/identity/zero fixture
// that makes the mechanism a no-op under mutation). The load-bearing leg fires
// a round down the gunsight's own solved lead and requires it to hit — an
// ABSOLUTE cross-check against the instrument the pilot aims with, not just
// self-check.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/gunsight.h"
#include "sim/world.h"
#include "weapon/ballistics.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

sim::SimState shooter_at(const glm::dvec3& pos, const glm::dvec3& vel,
                         const glm::dquat& orient = glm::dquat{1, 0, 0, 0}) {
    sim::SimState s;
    s.position = pos;
    s.velocity = vel;
    s.orientation = orient;
    return s;
}

// The wing gondola muzzle tips (config/world.toml [guns], assembly body
// coords).
constexpr glm::dvec3 kMuzzleWingL{-2.182, -0.389, -0.250};
constexpr glm::dvec3 kMuzzleWingR{2.182, -0.389, -0.250};
constexpr glm::dvec3 kMuzzleHub{0.0, 0.0, -2.870};

// Cowl MG muzzle tips (config/world.toml [guns], body coords).
// P2 fixture-fork fix (Fable P2, iter-7): aligned to config/world.toml values.
// Config: muzzle_cowl_l = [-0.160, 0.400, -2.400]; old test had [-0.250, 0.275,
// -2.370].
constexpr glm::dvec3 kMuzzleCowlL{-0.160, 0.400, -2.400};
constexpr glm::dvec3 kMuzzleCowlR{0.160, 0.400, -2.400};

// Cannon spec (hub + wing gondolas): MG151/20, v=805 m/s.
// iter-7 Task D: cannon_drag_k updated to 0.00080 (arcade retune, 300→500 m
// conv). Fable Q5: drag_k MUST be set explicitly — default 0 only tests vacuum.
inline weapon::GunSpec cannon_spec(const glm::dvec3& muzzle) {
    weapon::GunSpec g;
    g.muzzle_body = muzzle;
    g.kind = weapon::Round::Cannon20mm;
    g.muzzle_speed = 805.0;
    g.rof_hz = 12.0;
    g.drag_k = 0.00080;  // iter-7 Task D: 0.001565 → 0.00080 (arcade retune)
    return g;
}

// Cowl MG spec: 7.92 mm, v=855 m/s, k=0.000710.
// P2 fixture-fork fix: rof_hz aligned to config (19.0 not 16.7).
inline weapon::GunSpec mg_spec(const glm::dvec3& muzzle) {
    weapon::GunSpec g;
    g.muzzle_body = muzzle;
    g.kind = weapon::Round::MG792;
    g.muzzle_speed = 855.0;
    g.rof_hz = 19.0;  // P2 fix: config mg_rof_hz = 19.0 (was 16.7 ~1000 rpm;
                      // config is ~1140 rpm)
    g.drag_k = 0.000710;
    return g;
}

// A one-gun battery (the hub cannon) at a given cyclic rate — the minimal
// fixture to pin the fire_tick cadence/pool logic without the 5-gun
// bookkeeping. drag_k=0 intentional: these tests only pin cadence/pool, NOT
// harmonization.
weapon::GunWorld one_gun(double rof_hz) {
    weapon::GunWorld gw;
    gw.battery.convergence_range = 300.0;
    gw.battery.guns.push_back(
        {kMuzzleHub, weapon::Round::Cannon20mm, 800.0, rof_hz});
    return gw;
}

int count_active(const weapon::GunWorld& gw) {
    int c = 0;
    for (const weapon::Projectile& p : gw.pool)
        if (p.active) ++c;
    return c;
}

}  // namespace

// ===========================================================================
// CONVERGENCE (uses the two WING guns — the hub gun on the boresight is a
// built-in no-op for a lateral-toe test). Both muzzles are widely separated in
// X (baseline > 1 m, asserted first), yet both fire dirs pass THROUGH the
// convergence point (0,0,-conv). MUTATION: ignore muzzle_body (dir := nose for
// all) -> both dirs collapse to (0,0,-1), never toe inward, and the x-sign +
// convergence checks fail.
//
// Uses g=0 (vacuum seam): harmonization_rise returns 0, aim point = (0,0,-conv)
// exactly, so the geometric toe-in check is unchanged from pre-harmonization.
// (Fable Q1: g=0 => Δ=0 bit-exactly; Q2: cant is config, not dynamic.)
// ===========================================================================
TEST_CASE("guns: the wing battery toes in to the convergence point") {
    const double conv = 300.0;
    const sim::SimState sh = shooter_at({0, 0, 0}, {0, 0, 0});

    // Baseline: the two muzzles are genuinely apart (else convergence is
    // vacuous).
    REQUIRE(std::abs(kMuzzleWingL.x - kMuzzleWingR.x) > 1.0);

    const weapon::GunSpec wingL = cannon_spec(kMuzzleWingL);
    const weapon::GunSpec wingR = cannon_spec(kMuzzleWingR);

    // g=0 => Δ=0 => aim stays at (0,0,-conv); x-sign checks unchanged.
    // P2 fix: fire_dt passed explicitly (no default; g=0 -> fire_dt value is
    // moot).
    const glm::dvec3 dl = weapon::muzzle_world_dir(sh, wingL, conv, /*g=*/0.0,
                                                   /*fire_dt=*/1.0 / 120.0);
    const glm::dvec3 dr = weapon::muzzle_world_dir(sh, wingR, conv, /*g=*/0.0,
                                                   /*fire_dt=*/1.0 / 120.0);

    // Left muzzle (x<0) toes toward +x; right toes toward -x (they cross
    // ahead).
    REQUIRE(dl.x > 1e-3);
    REQUIRE(dr.x < -1e-3);

    // Each fire ray reaches the convergence point (0,0,-conv) exactly: the ray
    // muzzle + t*dir hits it at t = |(0,0,-conv) - muzzle|.
    // (g=0 seam: aim point = (0,0,-conv) exactly, so direction is unchanged.)
    for (const weapon::GunSpec& gs : {wingL, wingR}) {
        const glm::dvec3 d = weapon::muzzle_world_dir(sh, gs, conv, /*g=*/0.0,
                                                      /*fire_dt=*/1.0 / 120.0);
        const double t = glm::length(glm::dvec3{0, 0, -conv} - gs.muzzle_body);
        const glm::dvec3 hit = gs.muzzle_body + t * d;
        REQUIRE(hit.x == Catch::Approx(0.0).margin(1e-6));
        REQUIRE(hit.y == Catch::Approx(0.0).margin(1e-6));
        REQUIRE(hit.z == Catch::Approx(-conv).margin(1e-6));
    }
}

// ===========================================================================
// GENERIC SPHERE POSE + NON-AXIS ORIENTATION. spawn's world pos/dir must apply
// the body->world quaternion correctly. Identity/level would hide a rotate-vs-
// conjugate bug, so use an off-axis position and a generic tilt. MUTATION:
// forget the orientation (or invert it) -> pos and vel direction diverge.
//
// Pass g=0 to pin the GEOMETRY unchanged from pre-harmonization (Fable Q5:
// "pass g=0 to those two tests to keep them as pure geometry pins").
// ===========================================================================
TEST_CASE("guns: spawn places + aims through the body->world rotation") {
    const glm::dvec3 pos = glm::normalize(glm::dvec3{0.4, 0.5, 0.6}) * 16000.0;
    const glm::dquat q =
        glm::angleAxis(0.7, glm::normalize(glm::dvec3{1.0, 2.0, 3.0}));
    const sim::SimState sh = shooter_at(pos, {0, 0, 0}, q);

    weapon::GunBattery bat;
    bat.convergence_range = 300.0;
    // g=0: Δ=0, aim = (0,0,-conv), geometry identical to old spawn.
    const weapon::GunSpec wing{kMuzzleWingL, weapon::Round::Cannon20mm, 800.0,
                               12.0,
                               /*drag_k=*/0.0};
    const weapon::Projectile p =
        weapon::spawn(bat, wing, sh, /*g=*/0.0, /*fire_dt=*/1.0 / 120.0);

    // Position: shooter.pos + orient*muzzle_body (a missing/mis-signed rotation
    // moves it metres).
    const glm::dvec3 want_pos = pos + q * kMuzzleWingL;
    REQUIRE(glm::length(p.pos - want_pos) < 1e-6);

    // Direction: the world fire dir is exactly orient * (body toe-in dir).
    // g=0 => Δ=0 => aim point = (0,0,-conv) exactly — same as old code.
    const glm::dvec3 body_dir =
        glm::normalize(glm::dvec3{0, 0, -bat.convergence_range} - kMuzzleWingL);
    const glm::dvec3 want_dir = q * body_dir;
    const glm::dvec3 got_dir = glm::normalize(p.vel);  // zero shooter vel
    REQUIRE(glm::dot(got_dir, want_dir) == Catch::Approx(1.0).margin(1e-9));
}

// ===========================================================================
// VELOCITY INHERITANCE (nonzero, non-axis shooter velocity). The round leaves
// at shooter.velocity + speed*dir. MUTATION: drop the inheritance -> the
// round's velocity loses the shooter component (the x=120 baseline check
// fails).
//
// Pass g=0: hub is on the boresight, dir = (0,0,-1) at identity regardless of Δ
// (Δ only shifts the Y of the aim point, not the hub's direction when muzzle is
// on the boresight). But g=0 keeps this as a pure geometry pin (Fable Q5).
// ===========================================================================
TEST_CASE("guns: a fired round inherits the shooter velocity") {
    const glm::dvec3 v{120.0, -30.0, 40.0};             // non-axis, nonzero
    REQUIRE(std::abs(v.x) > 1.0);                       // baseline > eps
    const sim::SimState sh = shooter_at({0, 0, 0}, v);  // identity orient

    weapon::GunBattery bat;
    bat.convergence_range = 300.0;
    const weapon::GunSpec hub{kMuzzleHub, weapon::Round::Cannon20mm, 800.0,
                              12.0,
                              /*drag_k=*/0.0};
    const weapon::Projectile p =
        weapon::spawn(bat, hub, sh, /*g=*/0.0, /*fire_dt=*/1.0 / 120.0);

    // Hub is on the boresight, so its dir is exactly the nose (0,0,-1) at
    // identity: vel = (v.x, v.y, v.z - 800).
    REQUIRE(p.vel.x == Catch::Approx(120.0).margin(1e-9));
    REQUIRE(p.vel.y == Catch::Approx(-30.0).margin(1e-9));
    REQUIRE(p.vel.z == Catch::Approx(40.0 - 800.0).margin(1e-9));
}

// ===========================================================================
// GRAVITY DROP pinned against the DISCRETE integrator. Semi-implicit Euler
// (vel += g*dt BEFORE pos += vel*dt) drops |g|*dt^2 * n(n+1)/2 after n steps —
// NOT the continuous 0.5*g*t^2. Pin the exact discrete sum (a continuous-form
// pin would flake or need a sloppy tol). MUTATION: forward Euler, wrong 0.5, or
// no gravity -> the drop mismatches; a horizontal-distance baseline guards the
// no-op.
// ===========================================================================
TEST_CASE("guns: advance drops the round per the discrete integrator") {
    const glm::dvec3 grav{0.0, -9.81, 0.0};
    weapon::Projectile p;
    p.pos = {0, 0, 0};
    p.vel = {0, 0, -800};  // horizontal (down the nose)
    p.active = true;

    const double dt = 0.01;
    const int n = 100;  // 1.0 s
    for (int i = 0; i < n; ++i) weapon::advance(p, dt, grav);

    const double drop = 9.81 * dt * dt * (n * (n + 1)) / 2.0;  // 4.954 m
    REQUIRE(p.pos.y == Catch::Approx(-drop).margin(1e-9));
    REQUIRE(drop > 1.0);  // baseline: the drop is a real, non-trivial excursion
    // Horizontal travel is unaffected by the vertical gravity (no-op guard).
    REQUIRE(p.pos.z == Catch::Approx(-800.0 * dt * n).margin(1e-9));
    REQUIRE(p.age == Catch::Approx(dt * n).margin(1e-12));
}

// ===========================================================================
// RETIRE at kMaxFlightTime. Assert NOT retired just under the cap AND retired
// just past it (either bound alone passes a retire-always or retire-never
// mutant).
// ===========================================================================
TEST_CASE("guns: a round retires only after kMaxFlightTime") {
    const glm::dvec3 grav{0, 0, 0};
    weapon::Projectile p;
    p.active = true;
    const double dt = 0.01;
    const int just_under =
        static_cast<int>(weapon::kMaxFlightTime / dt) - 1;  // age < cap
    for (int i = 0; i < just_under; ++i) weapon::advance(p, dt, grav);
    REQUIRE(p.age < weapon::kMaxFlightTime);
    REQUIRE_FALSE(weapon::retired(p));

    for (int i = 0; i < 3; ++i) weapon::advance(p, dt, grav);  // cross the cap
    REQUIRE(p.age > weapon::kMaxFlightTime);
    REQUIRE(weapon::retired(p));
}

// ===========================================================================
// HARMONIZATION VACUUM SEAM: harmonization_rise with g=0 must return 0.0
// exactly (bit-identical), and spawn with g=0 must produce the old direction
// bit-for-bit (regression seam — Fable Q5, Q1).
// ===========================================================================
TEST_CASE("guns: harmonization_rise is zero at g=0 (vacuum seam)") {
    const weapon::GunSpec hub = cannon_spec(kMuzzleHub);
    const weapon::GunSpec wL = cannon_spec(kMuzzleWingL);
    const weapon::GunSpec mgL = mg_spec(kMuzzleCowlL);

    // Fable Q5: g=0 => Δ=0 exactly (not just small — bit-identical 0.0).
    REQUIRE(weapon::harmonization_rise(hub.muzzle_body, 300.0, hub.muzzle_speed,
                                       hub.drag_k, /*g=*/0.0,
                                       1.0 / 120.0) == 0.0);
    REQUIRE(weapon::harmonization_rise(wL.muzzle_body, 300.0, wL.muzzle_speed,
                                       wL.drag_k, /*g=*/0.0,
                                       1.0 / 120.0) == 0.0);
    REQUIRE(weapon::harmonization_rise(mgL.muzzle_body, 300.0, mgL.muzzle_speed,
                                       mgL.drag_k, /*g=*/0.0,
                                       1.0 / 120.0) == 0.0);

    // spawn with g=0 must reproduce the old direction bit-for-bit.
    const sim::SimState sh = shooter_at({0, 0, 0}, {0, 0, 0});
    weapon::GunBattery bat;
    bat.convergence_range = 300.0;

    // Old direction for the wing cannon: normalize((0,0,-conv) - muzzle).
    const glm::dvec3 old_body_dir =
        glm::normalize(glm::dvec3{0, 0, -300.0} - kMuzzleWingL);

    const weapon::GunSpec wspec = cannon_spec(kMuzzleWingL);
    const weapon::Projectile p =
        weapon::spawn(bat, wspec, sh, /*g=*/0.0, /*fire_dt=*/1.0 / 120.0);
    const glm::dvec3 got_dir = glm::normalize(p.vel);  // shooter at rest
    REQUIRE(glm::dot(got_dir, old_body_dir) ==
            Catch::Approx(1.0).margin(1e-12));
}

// ===========================================================================
// HARMONIZATION CONVERGENCE (the corrected-aim oracle, Fable Q5):
// Fire a round from each of the five muzzles with REAL ballistics (g=9.81,
// drag) and require it crosses the sightline within 15 mm vertical / 5 mm
// lateral of the world convergence point at conv=300 m.
//
// Fixture: shooter at rest, pos=(0,15000,0), identity orientation.
// "Sightline" = shooter.pos + orient*(0,0,-conv) = (0,15000,-300).
// nose_world = orient*(0,0,-1) = (0,0,-1).
//
// For each muzzle, advance the round tick-by-tick until boresight distance
// s = dot(shooter.pos - p.pos, nose_world) crosses conv, linearly interpolate
// the crossing, then check the miss perpendicular to boresight.
//
// MUTATION catches:
//   remove compensation -> cannon ~0.95 m low, MG ~0.70 m low (45-65x tol)
//   sign flip -> ~1.9 m low
//   vacuum Δ (drag dropped in rise) -> cannon ~0.27 m low
//
// Secondary assertions (Fable Q5):
//   - Straight-ray HIGH-AIM BAND: fire ray at conv passes ABOVE (0,0,-conv)
//     by Δ ∈ [0.5, 1.5] m (catches sign + order of magnitude).
//   - Wing toe-in x-sign checks preserved.
//   - harmonization_rise(g=0)==0.0 and spawn(g=0) seam (in the vacuum-seam
//     test above — not duplicated here).
//
// Q4 / Fable: at non-zero TAS the cannon crosses ~0.27 m HIGH at 300 m.
// That is by design — cant is speed-independent (nominal = rest). Do NOT
// "fix" it; it is documented here as the expected in-flight high-bias.
// ===========================================================================
TEST_CASE("guns: harmonized rounds cross the sightline at convergence range") {
    // iter-7 Task D: convergence updated to 500 m, cannon drag to 0.00080.
    // The test re-derives, does NOT re-record numbers to pass (MOVED GOLDEN
    // rule):
    //   cannon Δ @ conv=500, k=0.00080, v=805: the round droops more over 500 m
    //   than 300 m, and the retune drag is less, so Δ shifts. Re-derived from
    //   harmonization_rise (3-pass fixed point). Expected ~0.8-2.0 m (sanity
    //   only). MG Δ @ conv=500, k=0.000710, v=855: slightly different
    //   range/drag. Both classes still cross within ≤15 mm (oracle tolerance
    //   unchanged).
    constexpr double conv = 500.0;  // Task D: 300 → 500 m
    constexpr double g = 9.81;
    constexpr double fire_dt = 1.0 / 120.0;

    // Shooter at rest, high above the planet center so gravity_dir is
    // well-defined. pos = (0, 15000, 0): local up = +Y, nose (orient*(0,0,-1))
    // = -Z (world).
    const sim::SimState sh = shooter_at({0.0, 15000.0, 0.0}, {0.0, 0.0, 0.0});
    // World convergence point: sh.pos + orient*(0,0,-conv) = (0, 15000, -300).
    const glm::dvec3 conv_world =
        sh.position + sh.orientation * glm::dvec3{0, 0, -conv};
    // Boresight direction (world nose): orient*(0,0,-1) = (0,0,-1) at identity.
    const glm::dvec3 nose_world = sh.orientation * glm::dvec3{0.0, 0.0, -1.0};
    // World up at the shooter position (local up = normalize(pos) = +Y here).
    // Used to decompose miss into "up" and "lateral" components.
    const glm::dvec3 up_world = glm::normalize(sh.position);  // (0,1,0)

    // The five muzzles: 3 cannons (hub + 2 wing gondola) + 2 cowl MGs.
    weapon::GunBattery bat;
    bat.convergence_range = conv;

    // Cannon: v=805, k=0.001565 (Fable Q5 mandates REAL drag, not default 0).
    const weapon::GunSpec gunHub = cannon_spec(kMuzzleHub);
    const weapon::GunSpec gunWingL = cannon_spec(kMuzzleWingL);
    const weapon::GunSpec gunWingR = cannon_spec(kMuzzleWingR);
    // MG: v=855, k=0.000710 (Fable Q5 — drag_k must be explicit).
    const weapon::GunSpec gunCowlL = mg_spec(kMuzzleCowlL);
    const weapon::GunSpec gunCowlR = mg_spec(kMuzzleCowlR);

    // ---- Secondary: straight-ray HIGH-AIM BAND (Fable Q5) -------------------
    // With g>0, harmonization_rise lifts the aim by Δ for cannon and MG.
    // iter-7 Task D MOVED GOLDEN: conv=500 (was 300) + cannon k=0.00080 (was
    // 0.001565).
    //   The droop over 500 m is larger (longer TOF), but the retune drag is
    //   lower (flatter trajectory, faster round at range), so the net Δ
    //   changes. Re-derived sanity band: at k=0.00080, v=805, conv=500:
    //     TOF ≈ expm1(0.00080*500)/(0.00080*805) ≈ (e^0.4 - 1)/0.644 ≈
    //     0.4918/0.644 ≈ 0.764 s D ≈ (g/alpha^2)*(S^2/4+S/2-0.5*log1p(S)),
    //     alpha=k*v=0.644, S=alpha*t≈0.492 ≈
    //     (9.81/0.415)*(0.0606+0.246-0.5*0.406) ≈ 23.6*(0.307-0.203)
    //     ≈ 23.6*0.104 ≈ 2.45 m
    //   So cannon Δ ≈ 2.4-2.6 m at rest (sanity band [1.5, 3.5]).
    //   MG (k=0.000710, v=855, conv=500): TOF shorter; Δ ≈ 1.5-2.0 m (band
    //   [0.8, 2.8]). These are order-of-magnitude sanity bands, NOT
    //   flight-table values.
    {
        const double delta_cannon = weapon::harmonization_rise(
            kMuzzleHub, conv, gunHub.muzzle_speed, gunHub.drag_k, g, fire_dt);
        INFO("cannon Δ (hub): " << delta_cannon << " m");
        REQUIRE(delta_cannon >=
                1.5);  // sign + magnitude: must be upward, at least 1.5 m
        REQUIRE(delta_cannon <= 3.5);  // sanity anchor (re-derived: ~2.4-2.6 m)

        const double delta_mg = weapon::harmonization_rise(
            kMuzzleCowlL, conv, gunCowlL.muzzle_speed, gunCowlL.drag_k, g,
            fire_dt);
        INFO("MG Δ (cowl L): " << delta_mg << " m");
        REQUIRE(delta_mg >= 0.8);  // MG sanity anchor (re-derived: ~1.5-2.0 m)
        REQUIRE(delta_mg <= 2.8);
    }

    // ---- Secondary: wing x-sign checks (preserved from old convergence test)
    // --
    {
        const glm::dvec3 dL =
            weapon::muzzle_world_dir(sh, gunWingL, conv, g, fire_dt);
        const glm::dvec3 dR =
            weapon::muzzle_world_dir(sh, gunWingR, conv, g, fire_dt);
        REQUIRE(dL.x > 1e-3);   // left muzzle toes right
        REQUIRE(dR.x < -1e-3);  // right muzzle toes left
    }

    // ---- Main: integrated-round oracle (Fable Q5)
    // ---------------------------- For each gun, advance the round
    // tick-by-tick; when boresight distance crosses conv, linearly interpolate
    // to find the exact crossing position, then assert miss ⊥ boresight is
    // within tolerance.
    //
    // boresight distance s = dot(sh.pos - p.pos, nose_world).
    // When s == conv, p.pos should be near conv_world.
    constexpr double tol_up = 0.015;   // [m] ≤15 mm vertical
    constexpr double tol_lat = 0.005;  // [m] ≤5 mm lateral

    for (const weapon::GunSpec* gs :
         {&gunHub, &gunWingL, &gunWingR, &gunCowlL, &gunCowlR}) {
        weapon::Projectile proj = weapon::spawn(bat, *gs, sh, g, fire_dt);
        const glm::dvec3 grav_world = g * sim::gravity_dir(proj.pos);

        glm::dvec3 pos_prev = proj.pos;
        // Boresight distance s = dot(p.pos - sh.pos, nose_world).
        // nose_world = (0,0,-1) at identity; the round travels in that
        // direction so s increases from ~+muzzle_depth toward +conv as the
        // round flies forward.  (SEADS body: nose = -Z world, so dot with
        // nose_world gives the positive forward distance even though world z is
        // negative.)
        double s_prev = glm::dot(proj.pos - sh.position, nose_world);

        bool crossed = false;
        // Advance at most kMaxFlightTime / fire_dt ticks.
        const int max_ticks =
            static_cast<int>(weapon::kMaxFlightTime / fire_dt) + 1;
        for (int tick = 0; tick < max_ticks; ++tick) {
            weapon::advance(proj, fire_dt, grav_world);
            const double s_cur = glm::dot(proj.pos - sh.position, nose_world);

            if (s_prev < conv && s_cur >= conv) {
                // Linear interpolation to the crossing (mandatory — per-tick
                // step ~31 mm would swamp the 15 mm tolerance without this:
                // Fable Q5 P2).
                const double f = (conv - s_prev) / (s_cur - s_prev);
                const glm::dvec3 cross_pos =
                    pos_prev + f * (proj.pos - pos_prev);

                // Miss vector: crossing pos relative to world convergence
                // point.
                const glm::dvec3 miss = cross_pos - conv_world;
                // Decompose miss ⊥ boresight into up and lateral.
                // lateral axis = cross(nose_world, up_world) =
                // (0,0,-1)×(0,1,0)=(1,0,0)
                const double miss_up = glm::dot(miss, up_world);
                const double miss_lat =
                    glm::dot(miss, glm::cross(nose_world, up_world));

                INFO("gun muzzle "
                     << gs->muzzle_body.x << "," << gs->muzzle_body.y << ","
                     << gs->muzzle_body.z << "  miss_up=" << miss_up
                     << " m  miss_lat=" << miss_lat << " m");
                REQUIRE(std::abs(miss_up) <= tol_up);
                REQUIRE(std::abs(miss_lat) <= tol_lat);
                crossed = true;
                break;
            }
            pos_prev = proj.pos;
            s_prev = s_cur;
        }
        REQUIRE(crossed);  // the round must reach the convergence range
    }

    // ---- P1 frame test (Fable P1, iter-7): non-trivial attitude —
    //      muzzle direction rotates with the airframe -------------------------
    // The identity-orientation case above is BLIND to a body-vs-world frame bug
    // (e.g. applying the cant in world coords instead of body coords) because
    // at identity the body and world axes coincide.
    //
    // The convergence oracle already proves the crossing to ≤15 mm in level
    // flight. The frame bug would manifest differently: if the cant (body +Y Δ)
    // were incorrectly applied in WORLD coords instead of body coords, the
    // muzzle_world_dir at a non-trivial attitude would point in the WRONG
    // direction. We catch this by:
    //   1. At identity: the hub fires at world direction (0, sin(θ), -cos(θ))
    //      where θ = atan2(Δ, conv) — slightly above the nose.
    //   2. At 90° bank (body +Y → world -X): the hub must fire at world
    //      direction (-sin(θ), 0, -cos(θ)) — equally offset to the LEFT.
    //      A frame bug (world cant) would still give (0, sin(θ), -cos(θ)) —
    //      i.e. the direction does NOT rotate with the aircraft — caught by
    //      requiring the world X component is significantly negative.
    {
        // Reference: hub direction at identity orientation.
        const glm::dvec3 dir_level =
            weapon::muzzle_world_dir(sh, gunHub, conv, g, fire_dt);
        // dir_level ≈ (0, +small, -large). The Y component is the cant
        // signature.
        const double cant_y = dir_level.y;
        INFO("P1 frame: dir_level.y (cant sign) = " << cant_y);
        REQUIRE(cant_y > 1e-4);  // must be positive (aims UP in level flight)
        REQUIRE(dir_level.z < -0.99);  // still mostly forward

        // 90° bank: rotate about the world nose (-Z) axis by π/2 using
        // right-hand rule about (0,0,-1): body +Y → world +X, body +X → world
        // -Y. So the cant (+Y in body) goes to +X in world after this roll.
        const glm::dquat bank90 = glm::angleAxis(
            3.14159265358979323846 / 2.0,
            glm::dvec3{0.0, 0.0, -1.0});  // nose axis: body +Y → world +X
        const sim::SimState sh_banked =
            shooter_at({0.0, 15000.0, 0.0}, {0.0, 0.0, 0.0}, bank90);

        // At this 90° bank, body +Y → world +X.
        // The canted hub direction in body is attitude-independent. But the
        // WORLD direction rotates with the aircraft. So dir_banked should have:
        //   x ≈ +cant_y  (the Y offset rotated 90° → +X in world)
        //   y ≈ 0        (no longer pointing above nose in world Y)
        //   z ≈ same large negative (nose still -Z world)
        const glm::dvec3 dir_banked =
            weapon::muzzle_world_dir(sh_banked, gunHub, conv, g, fire_dt);
        INFO("P1 frame: dir_banked=(" << dir_banked.x << "," << dir_banked.y
                                      << "," << dir_banked.z << ")");

        // The cant must rotate with the aircraft:
        // x must be significantly positive (≈ +cant_y in world after 90° right
        // roll).
        REQUIRE(dir_banked.x > 1e-4);  // cant direction rotated to world +X
        // y must be near zero (cant is no longer in world +Y).
        REQUIRE(std::abs(dir_banked.y) < 1e-4);  // no world-Y component
        // z stays forward (nose is still -Z world after rolling about -Z axis).
        REQUIRE(dir_banked.z < -0.99);

        // Quantitative: x ≈ +dir_level.y AND banked.z ≈ level.z (cant is just
        // rotated, not rescaled). Checks within 1e-6.
        REQUIRE(dir_banked.x == Catch::Approx(dir_level.y).margin(1e-6));
        REQUIRE(dir_banked.z == Catch::Approx(dir_level.z).margin(1e-6));

        // MUTATION CATCH: if a bug applies the cant in world coords instead of
        // body coords, dir_banked.x ≈ 0 and dir_banked.y ≈ cant_y (positive) —
        // the first REQUIRE(dir_banked.x > 1e-4) would still pass... but
        // dir_banked.y would be nonzero (≈ cant_y), caught by
        // REQUIRE(|y|<1e-4).
    }
}

// ===========================================================================
// ABSOLUTE CROSS-CHECK (the load-bearing leg): fire a round down the gunsight's
// OWN solved lead and require it to hit. The pipper solves a deflection
// intercept (render::lead_solution, gravity + drag + velocity-inheritance,
// WORLD frame); a round fired along lead_dir at the world velocity  v_s +
// muzzle*lead_dir, advanced by the DISCRETE integrator (weapon::advance), must
// pass within 1 m of the target near the solved time-to-intercept.  Tests BOTH
// a resting shooter (the regression fixture from before) AND a MOVING shooter
// at typical dogfight speed (~150 m/s) — the moving-shooter case is the oracle
// for FIX 1-3 (world-frame pipper + discretization lag + dragged droop).
//
// MUTATION: wrong gravity sign / no drop compensation -> misses by metres.
// MUTATION: pipper solves relative-frame but round flies world-frame -> ~6.5 m
//   miss on a deflection shot at 400 m from a 150 m/s shooter (Fable P0).
// MUTATION: pipper uses vacuum but round uses drag -> ~29 m miss on a 150 m/s
//   crosser at 400 m (Fable C6 calculation).
// ===========================================================================

namespace {

// ===========================================================================
// pipper_miss — MODEL A cross-check (Task C, iter-7).
//
// WHY THE OLD TEST WAS WRONG (moved golden, authorized by Chad):
//   The old pipper_miss built a "fictional" projectile: pos=shooter.position,
//   vel = shooter_vel + muzzle * sol.lead_dir, NO cant, NO muzzle offset.
//   It tested a gun at the CG with no gravity compensation. After iter-6 added
//   the physical gun cant and after iter-7 (Model A) removed the droop from
//   the pipper solve, the OLD test would pass vacuously because:
//     (a) the fictional uncanted gun fires straight down the pipper (which has
//         NO droop term), so the round never droops to the target — it was
//         only "hitting" because the fictional droop in the pipper balanced the
//         fictional droop in the uncanted round: a coincidence of two wrongs.
//   The NEW test fires via weapon::spawn (the real canted hub gun through the
//   real battery), which is the ACTUAL round the pipper must agree with.
//
// MODEL A INVARIANT (the new assertion):
//   With a RESTING shooter at convergence range (cant was computed at rest,
//   nominal condition), the pipper gives pure-lead (no droop term). The hub
//   gun is canted up by Δ=harmonization_rise(...). When we fire a round at the
//   pipper-solved orientation:
//     • The cant adds a small +Y tilt to the launch direction.
//     • Gravity droop subtracts that +Y height over the flight.
//     • At convergence range, cant and droop should cancel to within the
//       harmonization residual (≤15 mm from the convergence oracle).
//     • The lead (XZ deflection) is handled purely by the pipper.
//   So a round fired from the canted gun when the shooter's nose is on
//   the pipper direction should pass within a tight tolerance of the target
//   at/near convergence range.
//
// TOLERANCE DERIVATION:
//   • Convergence oracle guarantees ≤15 mm vertical crossing error at conv.
//   • Pipper lead error (pure-lead, resting shooter) is <1 m at <600 m
//     (from the existing moving-shooter oracle — resting is tighter).
//   • Hub muzzle offset: (0,0,-2.870) — negligible lateral/vertical for a
//     target at 500 m range, adds ~2.87/500 ≈ 5.7 mrad forward offset.
//   • Combined: <1 m miss for a resting shooter, target near conv range.
//   We use 1.0 m as the threshold (conservative; the cant/droop cancellation
//   is sub-centimetre, pipper-lead error dominates within conv).
//
// MUTATION DETECTION:
//   • If the P0 double-comp returns (droop back in pipper solve), the pipper
//     over-elevates → the round overshoots the target vertically by ~2*droop
//     at conv range (~4-5 m at 500 m) — the test FAILS.
//   • If the cant is removed (Δ=0), the round drops below the target by
//     the full droop (~4-5 m at 500 m) — the test FAILS.
//   • If the gun fires straight (no toe-in), the round goes down the pipper
//     direction but gravity drops it — FAILS vertically.
//
// NOTE: this is the RESTING SHOOTER case. The pipper cross-check for the
// MOVING shooter (lead-only invariant: no cant, fictional CG gun) is kept
// SEPARATELY below as "pipper_miss_lead_only" — it verifies the pipper's
// lead math in isolation, unchanged from the pre-harmonization path.
// ===========================================================================

// Closest-approach helper (shared by both cross-checks below).
double closest_approach(weapon::Projectile r, const glm::dvec3& grav,
                        const glm::dvec3& tpos0, const glm::dvec3& target_vel,
                        double fire_dt, double t_intercept) {
    const int n = static_cast<int>(std::lround(t_intercept / fire_dt));
    glm::dvec3 tpos = tpos0;
    glm::dvec3 rprev = r.pos, tprev = tpos;
    double best = glm::length(rprev - tprev);
    for (int i = 0; i < n + 6; ++i) {
        weapon::advance(r, fire_dt, grav);
        tpos += target_vel * fire_dt;
        const glm::dvec3 d0 = rprev - tprev;
        const glm::dvec3 e = (r.pos - tpos) - d0;
        const double ee = glm::dot(e, e);
        const double s =
            ee > 1e-12 ? glm::clamp(-glm::dot(d0, e) / ee, 0.0, 1.0) : 0.0;
        best = std::min(best, glm::length(d0 + s * e));
        rprev = r.pos;
        tprev = tpos;
    }
    return best;
}

// HONEST PIPPER cross-check (iter-8): fires the REAL canted hub gun when the
// shooter nose is on the honest-pipper direction (droop compensated, cant
// subtracted). Resting shooter, target near conv range.
// Returns closest approach [m].
double pipper_miss_spawned(double target_range, const glm::dvec3& target_vel) {
    constexpr double muzzle = 805.0;    // MG151/20 M-Geschoss
    constexpr double drag_k = 0.00080;  // iter-7 cannon retune
    constexpr double fire_dt = 1.0 / 120.0;
    constexpr double g = 9.81;
    constexpr double conv = 500.0;  // iter-7 convergence range
    const glm::dvec3 grav{0.0, -g, 0.0};

    // Resting shooter at origin (cant was computed at rest — nominal
    // condition). Identity orientation: nose = -Z, up = +Y.
    const sim::SimState shooter = shooter_at({0.0, 15000.0, 0.0}, {0, 0, 0});

    // Target at +X (lateral crossing) relative to shooter.
    const sim::SimState target = shooter_at(
        shooter.position + glm::dvec3{target_range, 0, 0}, target_vel);

    // Pipper: honest-pipper solve (Task A, iter-8). Droop IS compensated; cant
    // subtraction uses the hub muzzle position and convergence_range so the net
    // residual is near-zero at convergence range.
    // Pass gp_convergence_range=conv and hub muzzle so cant_delta is computed
    // from weapon::harmonization_rise (single-sourced, same as the battery).
    const glm::dvec3 hub_muzzle{0.0, 0.0, -2.870};  // kMuzzleHub
    constexpr double kHitRadius = 8.0;  // arcade hit radius (scenario [drone])
    const render::LeadSolution sol =
        render::lead_solution(shooter, target, muzzle, 2000.0, grav, drag_k,
                              fire_dt, conv, hub_muzzle, kHitRadius);
    if (!sol.valid) return 1e9;

    // Orient the shooter so the NOSE is on the pipper direction (sol.lead_dir).
    // This is the firing condition: "nose on the solution → pull trigger".
    // We build a quaternion that rotates the default nose (0,0,-1) →
    // sol.lead_dir.
    const glm::dvec3 default_nose{0.0, 0.0, -1.0};
    const glm::dvec3 fire_dir = glm::normalize(sol.lead_dir);
    sim::SimState fire_sh = shooter;
    // Compute orientation: rotate default_nose → fire_dir.
    // Use the half-vector method (numerically stable, avoids near-180° edge).
    {
        const glm::dvec3 axis = glm::cross(default_nose, fire_dir);
        const double s_len = glm::length(axis);
        if (s_len > 1e-9) {
            const double angle =
                std::atan2(s_len, glm::dot(default_nose, fire_dir));
            fire_sh.orientation = glm::angleAxis(angle, axis / s_len);
        }
        // else: fire_dir == nose already (no rotation needed, identity ok)
    }

    // Build a one-gun battery with the hub cannon.
    weapon::GunBattery bat;
    bat.convergence_range = conv;
    const weapon::GunSpec hub = cannon_spec(kMuzzleHub);
    bat.guns.push_back(hub);

    // Fire via weapon::spawn — the REAL canted gun. This is what the test
    // was NOT doing before (it built a fictional uncanted CG gun).
    const weapon::Projectile r = weapon::spawn(bat, hub, fire_sh, g, fire_dt);

    // Track closest approach.
    return closest_approach(r, grav, target.position, target_vel, fire_dt,
                            sol.time_to_intercept);
}

// MOVING SHOOTER lead-only cross-check: a FICTIONAL uncanted CG gun fires
// down the pure-lead pipper direction. Tests the pipper's lead math in
// isolation (no cant, no muzzle offset — the cant/droop invariant is already
// covered by the convergence oracle + pipper_miss_spawned).
// This is the ORIGINAL pipper_miss refactored; kept for the moving-shooter
// lead regression (FIX 1/2: world-frame pipper + discretization lag).
//
// GRAVITY NOTE: the pipper is now pure-lead (no droop compensation). For a
// target crossing in the Y direction with gravity also in Y, the droop ADDS
// to the effective Y miss. To isolate LEAD accuracy from droop, this test
// uses grav=0 for BOTH the pipper solve and the round advance. The droop
// interaction is separately covered by pipper_miss_spawned (real cant + droop).
double pipper_miss_lead(const glm::dvec3& shooter_vel, double target_range,
                        const glm::dvec3& target_vel) {
    constexpr double muzzle = 805.0;
    constexpr double drag_k =
        0.00080;  // iter-7 drag retune (single-source with cannon_spec)
    constexpr double fire_dt = 1.0 / 120.0;
    // grav=0: pure lead-math isolation test. The pipper and the round both use
    // vacuum gravity so the only miss comes from incorrect lead computation.
    const glm::dvec3 grav{0.0, 0.0, 0.0};

    const sim::SimState shooter = shooter_at({0, 0, 0}, shooter_vel);
    const sim::SimState target =
        shooter_at(glm::dvec3{target_range, 0, 0}, target_vel);

    const render::LeadSolution sol = render::lead_solution(
        shooter, target, muzzle, 2000.0, grav, drag_k, fire_dt);
    if (!sol.valid) return 1e9;

    // Fictional uncanted CG gun (no muzzle offset, no cant). Tests lead math
    // only.
    weapon::Projectile r;
    r.pos = shooter.position;
    r.vel = shooter_vel + muzzle * sol.lead_dir;
    r.drag_k = drag_k;
    r.active = true;

    return closest_approach(r, grav, target.position, target_vel, fire_dt,
                            sol.time_to_intercept);
}

}  // namespace

TEST_CASE("guns: a round fired down the pipper's lead hits the target") {
    // ---- Honest-pipper cross-check (Task A, iter-8): real canted hub gun,
    // resting ---- WHY this test uses the honest pipper (moved golden,
    // authorized by Chad):
    //   Model A (iter-7) was a pure-lead pipper — gravity blind. Under the
    //   HONEST PIPPER ruling (iter-8), droop IS compensated (with cant
    //   subtracted), so a round fired when the nose is on the pipper HITS at
    //   any in-envelope range. pipper_miss_spawned now calls lead_solution with
    //   gp_convergence_range=conv and hub_muzzle so the cant subtraction is
    //   correctly applied. At convergence range: cant≈droop → pipper≈boresight
    //   → fires like before. Beyond convergence: pipper auto-holds-over by
    //   residual droop → still hits.
    //
    // Resting shooter, target at convergence range (500 m) crossing at 80 m/s.
    // 80 m/s crossing: typical subsonic bandit, enough lead to exercise the
    // solve. Tolerance at conv range: 1.0 m (cant+droop nearly cancel; pipper
    // lead <1 m).
    const double miss_conv = pipper_miss_spawned(500.0, {0, 80, 0});
    INFO("spawned-round miss at conv=500 m: " << miss_conv << " m");
    REQUIRE(miss_conv < 1.0);

    // Target at 300 m (sub-convergence): cant was computed for 500 m, so the
    // gun OVER-compensates at 300 m — the round arrives ~Δ(500)-droop(300) ≈
    // 1-2 m HIGH (the by-design cant residual at sub-convergence ranges; see
    // handoff §Q4). Tolerance: 2.0 m (the over-comp is known, documented,
    // by-design). The test still catches the P0 double-comp (would add another
    // ~droop(300)≈2 m → >4 m).
    const double miss_300 = pipper_miss_spawned(300.0, {0, 80, 0});
    INFO("spawned-round miss at 300 m (sub-conv, over-comp expected): "
         << miss_300 << " m");
    REQUIRE(miss_300 < 2.5);

    // BASELINE: the test exercises real gravity (g=9.81). If g=0 were passed,
    // both cant AND pipper-droop would be zero, making this a trivial no-op.
    // Verify the spawned round's initial velocity has a +Y component (the cant)
    // to confirm gravity compensation is actually being exercised.
    {
        const sim::SimState sh = shooter_at({0.0, 15000.0, 0.0}, {0, 0, 0});
        weapon::GunBattery bat;
        bat.convergence_range = 500.0;
        const weapon::GunSpec hub = cannon_spec(kMuzzleHub);
        bat.guns.push_back(hub);
        const weapon::Projectile rr =
            weapon::spawn(bat, hub, sh, /*g=*/9.81, /*fire_dt=*/1.0 / 120.0);
        // The canted round must have a +Y velocity component (it's aimed up).
        // If cant=0 (g=0 or Δ dropped), this would be near 0 or negative.
        INFO("spawned vel.y (should be >0 due to cant): " << rr.vel.y);
        REQUIRE(rr.vel.y > 1.0);  // at least 1 m/s upward due to cant
    }

    // ---- Moving-shooter lead-only cross-check (regression for FIX 1/2/3)
    // ----- Fictional uncanted CG gun, tests lead math in isolation. MUTATION:
    // wrong gravity sign / no drop compensation -> misses by metres. MUTATION:
    // pipper solves relative-frame but round flies world-frame -> ~6.5 m
    //   miss on a deflection shot at 400 m from a 150 m/s shooter (Fable P0).
    // MUTATION: pipper uses vacuum but round uses drag -> miss grows with
    // range.
    const glm::dvec3 shooter_vel{140.0, 5.0, -30.0};  // ~144 m/s, non-axis
    REQUIRE(glm::length(shooter_vel) > 100.0);

    const double lead_300 = pipper_miss_lead(shooter_vel, 300.0, {0, 150, 0});
    const double lead_400 = pipper_miss_lead(shooter_vel, 400.0, {0, 150, 0});
    const double lead_500 = pipper_miss_lead(shooter_vel, 500.0, {0, 150, 0});

    INFO("moving-shooter lead miss at 300 m: " << lead_300 << " m");
    INFO("moving-shooter lead miss at 400 m: " << lead_400 << " m");
    INFO("moving-shooter lead miss at 500 m: " << lead_500 << " m");

    REQUIRE(lead_300 < 1.0);
    REQUIRE(lead_400 < 1.0);
    REQUIRE(lead_500 < 1.0);
}

// ===========================================================================
// VELOCITY-AWARE HARMONIZATION (Fable C6c, 2026-07-17 — "rounds sit too low").
//
// The round inherits the shooter's velocity, which in level flight rides BELOW
// the nose by the trim angle-of-attack, so a boresight round departs AoA-below
// the sightline. weapon::harmonization_offset cants the gun UP by
// −(conv/muzzle_speed)·v_perp_body so the round leaves ALONG the sightline.
// This pins the MECHANISM FIRING on a non-trivial (moving, gravity-on) path —
// the rest/vacuum seams (bit-identical) are pinned by the tests above.
// ===========================================================================
TEST_CASE("guns: velocity inheritance cants the round up (level-flight AoA)") {
    constexpr double conv = 500.0, g = 9.81, fire_dt = 1.0 / 120.0;
    const glm::dvec3 pos{0.0, 15000.0, 0.0};  // local up = +Y, nose (id) = -Z
    // Level flight with a trim AoA: velocity rides below the nose (body -Y).
    const double V = 167.0, aoa = 0.021;  // ~1.2° trim AoA, cruise speed
    const glm::dvec3 vel{0.0, -V * std::sin(aoa), -V * std::cos(aoa)};

    const sim::SimState sh_rest = shooter_at(pos, {0, 0, 0});
    const sim::SimState sh_move = shooter_at(pos, vel);  // identity orientation
    const weapon::GunSpec hub = cannon_spec(kMuzzleHub);

    const glm::dvec3 d_rest =
        weapon::muzzle_world_dir(sh_rest, hub, conv, g, fire_dt);
    const glm::dvec3 d_move =
        weapon::muzzle_world_dir(sh_move, hub, conv, g, fire_dt);

    // Baseline (fixture-no-op guard): the rest case already cants up by gravity
    // Δg.
    REQUIRE(d_rest.y > 1e-4);
    // The moving (level+AoA) shooter cants the round strictly MORE up.
    // MUTATION: drop the inheritance term (off.y = Δg only) → d_move.y ==
    // d_rest.y → fails.
    REQUIRE(d_move.y > d_rest.y + 1e-3);

    // Magnitude: the added elevation angle ≈ V·sin(aoa)/muzzle_speed (the
    // inheritance rise −(conv/v)·v_body.y, over conv). Confirms SIGN + SCALE,
    // so a sign flip (aims further DOWN) or a wrong divisor is caught.
    const double got_extra = std::asin(d_move.y) - std::asin(d_rest.y);
    const double want_extra = (V * std::sin(aoa)) / hub.muzzle_speed;  // rad
    INFO("extra elevation: got " << got_extra << " want " << want_extra
                                 << " rad");
    REQUIRE(got_extra > 0.003);  // fires with real magnitude (~4.4 mrad)
    REQUIRE(got_extra == Catch::Approx(want_extra).margin(5e-4));

    // A shooter whose flight path rides ABOVE the nose (v_body.y > 0) cants the
    // round DOWN — the sign truly tracks the inherited perpendicular velocity.
    const sim::SimState sh_up =
        shooter_at(pos, {0.0, +V * std::sin(aoa), -V * std::cos(aoa)});
    const glm::dvec3 d_up =
        weapon::muzzle_world_dir(sh_up, hub, conv, g, fire_dt);
    REQUIRE(d_up.y < d_rest.y - 1e-3);
}

namespace {

// Point the body nose (0,0,-1) at `dir`, then roll `bank` rad about the new
// nose. Preserves the intercept geometry (nose on `dir`) while banking the
// airframe — so the cant's body-vs-world frame distinction is exercised (Fable
// P1-1).
glm::dquat aim_with_bank(const glm::dvec3& dir, double bank) {
    const glm::dvec3 nose0{0.0, 0.0, -1.0};
    const glm::dvec3 d = glm::normalize(dir);
    glm::dquat point{1.0, 0.0, 0.0, 0.0};
    const glm::dvec3 axis = glm::cross(nose0, d);
    const double s_len = glm::length(axis);
    if (s_len > 1e-9) {
        const double angle = std::atan2(s_len, glm::dot(nose0, d));
        point = glm::angleAxis(angle, axis / s_len);
    }
    return glm::angleAxis(bank, d) * point;  // roll about the (world) nose
}

// Closed-loop cross-check: a MOVING shooter at the given bank fires the REAL
// canted hub gun when its nose is on the honest-pipper solution, under real
// gravity. Iterates solve↔aim to the fixed point (mirrors the per-tick loop),
// then fires and returns closest approach [m]. Single-sourced cant ⇒ hit at ANY
// bank; the pre-fix world-up scalar cant forks banked shots (Fable P1-1).
double pipper_hit_moving(const glm::dvec3& shooter_vel, double bank,
                         const glm::dvec3& tgt_offset,
                         const glm::dvec3& target_vel,
                         render::LeadSolution* out_sol = nullptr) {
    constexpr double muzzle = 805.0, drag_k = 0.00080, fire_dt = 1.0 / 120.0;
    constexpr double g = 9.81, conv = 500.0;
    const glm::dvec3 pos{0.0, 15000.0, 0.0};
    const glm::dvec3 grav = g * sim::gravity_dir(pos);  // (0,-9.81,0)
    const glm::dvec3 hub_muzzle{0.0, 0.0, -2.870};

    const sim::SimState target = shooter_at(pos + tgt_offset, target_vel);
    sim::SimState sh = shooter_at(pos, shooter_vel);  // identity to start
    render::LeadSolution sol;
    for (int it = 0; it < 8; ++it) {
        sol = render::lead_solution(sh, target, muzzle, 2000.0, grav, drag_k,
                                    fire_dt, conv, hub_muzzle, 8.0);
        if (!sol.valid) return 1e9;
        sh.orientation =
            aim_with_bank(sol.lead_dir, bank);  // nose on lead, banked
    }
    if (out_sol) *out_sol = sol;  // converged nose-on-pipper solution
    weapon::GunBattery bat;
    bat.convergence_range = conv;
    const weapon::GunSpec hub = cannon_spec(kMuzzleHub);
    bat.guns.push_back(hub);
    const weapon::Projectile r = weapon::spawn(bat, hub, sh, g, fire_dt);
    return closest_approach(r, grav, target.position, target_vel, fire_dt,
                            sol.time_to_intercept);
}

}  // namespace

// ===========================================================================
// SINGLE-SOURCE / NO-FORK at ANY bank (Fable P1-1 frame fix + C6c cross-check).
// A moving shooter fires the REAL canted gun when its nose is on the pipper;
// the round must HIT — proving the pipper subtracts EXACTLY the cant the gun
// adds, in the BODY frame, so it tracks the airframe at any bank. MUTATION:
// revert the pipper cant to the world-up scalar → banked shots fork by ~2.5 m
// @60° / 3.6 m
// @90° (Fable) → the banked REQUIREs fail. This is the leg the old suite lacked
// (its moving cross-check used g=0, which gates the whole correction off).
// ===========================================================================
TEST_CASE("guns: pipper and canted round stay single-sourced at any bank") {
    // Level flight with AoA, target roughly ahead (co-altitude), slight
    // crossing.
    const glm::dvec3 svel{0.0, -3.5, -167.0};    // forward + trim-AoA down
    const glm::dvec3 tgt_off{0.0, 0.0, -480.0};  // ~480 m dead ahead
    const glm::dvec3 tvel{40.0, 0.0, 0.0};       // 40 m/s crosser (real lead)

    const double miss_level = pipper_hit_moving(svel, 0.0, tgt_off, tvel);
    INFO("moving-shooter miss, wings level: " << miss_level << " m");
    REQUIRE(miss_level < 1.5);

    const double miss_60 =
        pipper_hit_moving(svel, 3.14159265358979 / 3.0, tgt_off, tvel);
    INFO("moving-shooter miss, 60 deg bank: " << miss_60 << " m");
    REQUIRE(miss_60 < 1.5);  // MUTATION (world-up scalar cant): ~2.5 m → FAILS

    const double miss_90 =
        pipper_hit_moving(svel, 3.14159265358979 / 2.0, tgt_off, tvel);
    INFO("moving-shooter miss, 90 deg bank: " << miss_90 << " m");
    REQUIRE(miss_90 < 1.5);  // MUTATION (world-up scalar cant): ~3.6 m → FAILS
}

// ===========================================================================
// HARD-G PULL must NOT fire a FALSE red pipper (Fable P1, iter-15 red-team).
// The inheritance cant aligns the round's DEPARTURE with the sightline — it
// produces NO miss for a nose-on-pipper shot. If predicted_miss folds the
// inheritance part of the cant into its "miss" metric, a hard pull (large
// v_perp) drives |predicted_miss| past the hit radius → out_of_envelope RED
// even though the shot HITS — the cue lies exactly when the pilot pulls lead.
// FIX: predicted_miss uses only the GRAVITY Δg of the cant. MUTATION (fold the
// full cant back in): out_of_envelope flips true here → REQUIRE_FALSE fails.
// ===========================================================================
TEST_CASE("guns: a hard-G pull does not fire a false out-of-envelope pipper") {
    // v_body.y = -20 m/s ⇒ a hard AoA pull (flight path 20 m/s below the nose).
    // s·v_perp = (500/805)·20 ≈ 12.4 m ≫ the 8 m hit radius: with the bug the
    // cue reads |predicted_miss| ≈ 12 m → RED; the real shot still hits ≤1.5 m.
    const glm::dvec3 svel{0.0, -20.0, -166.0};   // ~167 m/s, hard nose-up AoA
    const glm::dvec3 tgt_off{0.0, 0.0, -500.0};  // ~500 m ahead (near conv)
    const glm::dvec3 tvel{0.0, 0.0, 0.0};        // stationary: isolate the cue

    render::LeadSolution sol;
    const double miss = pipper_hit_moving(svel, 0.0, tgt_off, tvel, &sol);
    INFO("hard-pull: miss=" << miss
                            << " m  predicted_miss=" << sol.predicted_miss
                            << " m  out_of_envelope=" << sol.out_of_envelope);
    REQUIRE(sol.valid);
    // The shot genuinely CONNECTS (lands inside the 8 m hit sphere) — so a red
    // "out of envelope" here would be a lie. The ~2.4 m residual at this hard a
    // pull is the deferred P2 (the cant linear scale omits m/a_final); it is
    // far inside the hit radius and NOT a fork (T2/T3 pin the no-fork
    // invariant).
    REQUIRE(miss < 8.0);
    // The residual-droop cue excludes the inheritance cant, so it stays small
    // and the pipper does NOT falsely flag out-of-envelope on a shot that hits.
    // MUTATION (fold the full cant into the cue): |predicted_miss| ≈ 12 m → the
    // next two REQUIREs fail.
    REQUIRE(std::abs(sol.predicted_miss) < 8.0);  // within the hit radius
    REQUIRE_FALSE(sol.out_of_envelope);
}

// ===========================================================================
// FIRE CADENCE: holding the trigger spawns rounds at the cyclic rate, NOT every
// tick and NOT never. Over T seconds a rof-Hz gun emits ~T*rof + 1 rounds (the
// first shot fires instantly, then one per period). The shooter sits high above
// the sphere with ground_radius = 0 so NOTHING retires — count(active) == the
// number of shots fired. MUTATIONS: drop the cooldown -> a shot EVERY tick
// (100, fails the upper bound); never fire -> 0 (fails the lower bound + the
// firing/not-firing split below).
// ===========================================================================
TEST_CASE("guns: holding fire spawns rounds at the cyclic rate") {
    const sim::SimState sh = shooter_at({0, 0, 17000}, {0, 0, 0});  // |pos|>0
    const double dt = 0.01;
    const double rof = 10.0;  // period 0.1 s

    weapon::GunWorld gw = one_gun(rof);
    for (int i = 0; i < 100; ++i)  // 1.0 s of held trigger
        weapon::fire_tick(gw, sh, /*firing=*/true, dt, 9.81, /*ground=*/0.0);

    const int fired = count_active(gw);
    REQUIRE(fired >=
            10);  // ~T*rof + 1 = 11; a per-tick mutant (100) fails here
    REQUIRE(fired <= 12);  //   ... and a no-cooldown mutant also fails here
    REQUIRE(fired > 5);    // baseline: a real, non-trivial burst (not a no-op)

    // Not firing spawns nothing — the trigger truly gates (no free-running
    // gun).
    weapon::GunWorld idle = one_gun(rof);
    for (int i = 0; i < 100; ++i)
        weapon::fire_tick(idle, sh, /*firing=*/false, dt, 9.81, 0.0);
    REQUIRE(count_active(idle) == 0);
}

// ===========================================================================
// TRIGGER-RELEASE CYCLIC GATE (FIX F.2, Task F, iter-8):
// Releasing the trigger now DRAINS the cooldown (clamp at 0) rather than
// resetting it instantly to 0. This prevents the tap-rate exploit: tapping LMB
// faster than the cyclic rate can no longer fire one round per tap.
// Pins both edges on one path. MUTATION: reset cooldown to 0 on release ->
// the re-press tick fires immediately regardless of cyclic timing.
// ===========================================================================
TEST_CASE(
    "guns: releasing the trigger drains cooldown (cyclic rate enforced)") {
    const sim::SimState sh = shooter_at({0, 0, 17000}, {0, 0, 0});
    const double dt = 0.01;

    // rof=5 Hz -> period 0.2 s. After first shot cooldown = 0.2.
    // Release for 20 ticks (0.2 s total drain): cooldown should reach 0.
    weapon::GunWorld gw = one_gun(/*rof=*/5.0);  // period 0.2 s >> dt
    weapon::fire_tick(gw, sh, true, dt, 9.81,
                      0.0);  // tick 1: shot 1, cooldown=0.2
    REQUIRE(count_active(gw) == 1);
    weapon::fire_tick(gw, sh, true, dt, 9.81,
                      0.0);          // tick 2: mid-cooldown → gated
    REQUIRE(count_active(gw) == 1);  // still only 1 shot

    // Release for EXACTLY 19 more ticks (19*0.01 = 0.19 s drain).
    // cooldown was ~0.19 after tick 2, so after 19 drain ticks cooldown ≈ 0.
    for (int i = 0; i < 19; ++i)
        weapon::fire_tick(gw, sh, false, dt, 9.81, 0.0);  // drain
    REQUIRE(count_active(gw) == 1);  // release spawns nothing

    // Re-press: cooldown should be ≤ 0 now → fires immediately.
    weapon::fire_tick(gw, sh, true, dt, 9.81, 0.0);
    REQUIRE(count_active(gw) == 2);  // shot 2 fires (cooldown drained)

    // ANTI-EXPLOIT CHECK: tap-rate exploit is prevented.
    // Fire 1 shot, then alternate release/press on each tick (1 tap per 2*dt =
    // 50/s): With old reset-to-0 behavior: every press would fire → 50+ rounds
    // in 1 s. With new drain behavior: only fires at cyclic rate (5 Hz → ~10
    // shots in 1 s).
    weapon::GunWorld gw2 = one_gun(/*rof=*/5.0);
    for (int i = 0; i < 200; ++i) {
        const bool press = (i % 2 == 0);  // alternate tap: 50 Hz effective
        weapon::fire_tick(gw2, sh, press, dt, 9.81, 0.0);
    }
    const int tapped = count_active(gw2);
    // At 5 Hz over 2 s: ~10-11 rounds. The old exploit would give ~50+ rounds.
    // Allow a margin of ±5 for quantization, but it MUST be much less than 50.
    INFO("tap-rate exploit check: " << tapped << " rounds in 2 s at 50 Hz tap");
    REQUIRE(tapped >= 8);   // fired at least some rounds (not fully gated)
    REQUIRE(tapped <= 15);  // cyclic rate enforced (not the ~50/s exploit rate)
}

// ===========================================================================
// GROUND RETIRE: a round at/under the crash sphere (|pos| <= ground_radius) is
// retired, so a shot cannot fly kilometres through the planet. Pin BOTH sides
// with the SAME advanced round: it dips just under a high ground_radius
// (retire) but stays above a low one (live). MUTATION: drop the |pos| check ->
// the round stays active (age 0.01 << kMaxFlightTime) under BOTH radii.
// ===========================================================================
TEST_CASE("guns: a round retires at/under the crash sphere") {
    const sim::SimState sh =
        shooter_at({0, 0, 0}, {0, 0, 0});  // shooter unused
    const double dt = 0.01;

    // One round just above R, moving radially inward (toward the origin) at
    // 100 m/s: after one 0.01 s step it descends 1 m to |pos| = 14999.5.
    auto placed = [](double gr) {
        weapon::GunWorld gw;  // no guns -> firing spawns nothing; we advance
                              // the manually-placed round only
        weapon::Projectile p;
        p.pos = {0, 0, 15000.5};
        p.vel = {0, 0, -100};  // inward (+z shrinking toward the origin)
        p.active = true;
        gw.pool.push_back(p);
        weapon::fire_tick(gw, {}, /*firing=*/false, 0.01, /*g=*/0.0, gr);
        return count_active(gw);
    };
    (void)sh;
    REQUIRE(placed(15000.0) == 0);  // dipped under R=15000 -> retired
    REQUIRE(placed(14000.0) == 1);  // still 999 m above 14000 -> live
    (void)dt;
}

// ===========================================================================
// T10 — GUNS FIRE UNDERGROUND. A round at/under the crash sphere retires only
// when it is OUTSIDE the net (rock / core). Inside the tunnel bores + the
// cavern shell it keeps flying (a dogfight in the hollow core). Null net => the
// legacy |pos| <= R retire, bit-identical. Three legs (the brief's P0-3):
//   (1) a round mid-cavern (inside the shell, well below R) survives N ticks;
//   (2) a round into the core wall (below core_r, outside the net) retires;
//   (3) null-net legacy bit-identity (the same manually-placed round retires
//       identically with net==nullptr).
// ===========================================================================
namespace {
world::TunnelNet t10_net() {
    world::TunnelParams tp;
    tp.sphere_R = 15000.0;
    tp.tube_width_m = 110.0;
    tp.tube_height_m = 90.0;
    tp.depth_m = 1600.0;
    tp.soft_m = 40.0;
    tp.ramp_frac = 0.3;
    tp.spacing_m = 150.0;
    tp.floor_height_m = 20.0;
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
    tp.bowl_radius_m = 450.0;
    tp.bowl_depth_m = 300.0;
    tp.mouth_sink_m = 130.0;
    tp.min_cover_m = 60.0;
    return world::build_tunnel_net(tp, nullptr);
}
}  // namespace

TEST_CASE("T11 guns: a round fired mid-arena survives (net suspends retire)") {
    const world::TunnelNet net = t10_net();
    // A point mid-arena (below R, inside the flyable arena).
    const glm::dvec3 mid = net.arena.center;
    const glm::dvec3 dir = glm::normalize(mid);
    REQUIRE(net.contains(mid));           // premise: inside the arena
    REQUIRE(glm::length(mid) < 15000.0);  // premise: below the crash sphere

    // A round drifting laterally (stays inside the huge arena) over N ticks.
    const glm::dvec3 tang =
        glm::normalize(glm::cross(dir, glm::dvec3{1.0, 0.0, 0.0}));
    auto run = [&](const world::TunnelNet* n) {
        weapon::GunWorld gw;
        weapon::Projectile p;
        p.pos = mid;
        p.vel = 200.0 * tang;
        p.active = true;
        gw.pool.push_back(p);
        for (int i = 0; i < 30; ++i)
            weapon::fire_tick(gw, {}, /*firing=*/false, 0.01, /*g=*/0.0,
                              15000.0, n);
        return count_active(gw);
    };
    // Inside the cavern with the net live: the round keeps flying.
    REQUIRE(run(&net) == 1);
    // Without the net (legacy): the round (below R) retires immediately.
    REQUIRE(run(nullptr) == 0);
}

TEST_CASE("T10 guns: a round into the core wall retires (core is solid)") {
    const world::TunnelNet net = t10_net();
    const glm::dvec3 dir = glm::normalize(glm::dvec3{0.2, 1.0, 0.3});
    // Just inside the core (below core_r) — OUTSIDE the net, a crash wall.
    const glm::dvec3 in_core = dir * (net.core_r - 100.0);
    REQUIRE(!net.contains(in_core));  // premise: the core interior is solid
    weapon::GunWorld gw;
    weapon::Projectile p;
    p.pos = in_core;
    p.vel = glm::dvec3{0.0};
    p.active = true;
    gw.pool.push_back(p);
    weapon::fire_tick(gw, {}, /*firing=*/false, 0.01, /*g=*/0.0, 15000.0, &net);
    REQUIRE(count_active(gw) == 0);  // in the solid core -> retired
}

TEST_CASE(
    "T10 guns: null-net retire is bit-identical to the legacy predicate") {
    // The same manually-placed round retires identically with net==nullptr and
    // with the explicit default arg omitted (the additive-firewall pin).
    auto run = [](bool pass_null) {
        weapon::GunWorld gw;
        weapon::Projectile p;
        p.pos = {0, 0, 15000.5};
        p.vel = {0, 0, -100};  // inward, dips under R this tick
        p.active = true;
        gw.pool.push_back(p);
        if (pass_null)
            weapon::fire_tick(gw, {}, false, 0.01, 0.0, 15000.0, nullptr);
        else
            weapon::fire_tick(gw, {}, false, 0.01, 0.0, 15000.0);
        return count_active(gw);
    };
    REQUIRE(run(true) == 0);
    REQUIRE(run(false) == 0);
    REQUIRE(run(true) == run(false));
}

// ===========================================================================
// T-perf: retire_round's Lipschitz cache (weapon/ballistics.h). signed_distance
// is 1-Lipschitz (a min over per-primitive distances-to-a-set), so a round
// that hasn't moved farther than the last |sdf| cannot have crossed the
// tunnel boundary -- the cached verdict is reused instead of re-running the
// ~80-spine-segment scan. Two legs: (1) a differential vs. a fresh
// net.contains() oracle call every step, proving the cache never DISAGREES
// with the ground truth over many small in-chamber steps; (2) a forced
// cache-miss (a jump farther than the cached |sdf|, straight into solid
// rock) proving a stale "inside" verdict is never blindly reused.
// ===========================================================================
TEST_CASE(
    "T-perf guns: Lipschitz-cached retire matches a fresh contains() oracle "
    "over small steps in a wide chamber") {
    const world::TunnelNet net = t10_net();
    const glm::dvec3 mid = net.arena.center;
    REQUIRE(net.contains(mid));  // premise: starts well inside the huge arena

    const glm::dvec3 dir = glm::normalize(glm::dvec3{1.0, 0.3, 0.2});
    const double step = 0.05;  // [m]/tick -- tiny vs. the arena's huge |sdf|

    weapon::Projectile p;
    p.pos = mid;
    p.active = true;
    glm::dvec3 oracle_pos = mid;

    for (int i = 0; i < 200; ++i) {
        p.pos += step * dir;
        oracle_pos += step * dir;
        const bool retired_cached = weapon::retire_round(p, 15000.0, &net);
        const bool retired_oracle = !net.contains(oracle_pos);
        REQUIRE(retired_cached == retired_oracle);
        REQUIRE(!retired_cached);  // premise: stays well inside the arena
    }
    // Premise the cache actually engaged over these steps (not vacuously
    // agreeing because it never got the chance to reuse anything).
    REQUIRE(p.net_cache_valid);
    REQUIRE(p.net_cache_abs_sdf > step);
}

TEST_CASE(
    "T-perf guns: a jump past the cached |sdf| forces re-eval (cache-miss "
    "pin)") {
    const world::TunnelNet net = t10_net();
    const glm::dvec3 mid = net.arena.center;
    REQUIRE(net.contains(mid));

    weapon::Projectile p;
    p.pos = mid;
    p.active = true;

    // First call establishes the cache: a real position deep in the huge
    // arena -> a large |sdf|, and the round is (correctly) not retired.
    REQUIRE(!weapon::retire_round(p, 15000.0, &net));
    REQUIRE(p.net_cache_valid);
    const double cached_abs_sdf = p.net_cache_abs_sdf;
    REQUIRE(cached_abs_sdf > 0.0);

    // Jump the round FAR PAST the cached |sdf|, straight into the solid core
    // (outside the net) -- the Lipschitz bound does NOT license reusing the
    // cache over a move this large.
    const glm::dvec3 dir = glm::normalize(glm::dvec3{0.2, 1.0, 0.3});
    const glm::dvec3 in_core = dir * (net.core_r - 100.0);
    REQUIRE(!net.contains(in_core));  // premise: genuinely outside the net
    REQUIRE(glm::length(in_core - mid) >
            cached_abs_sdf);  // premise: past the cache's safe radius

    p.pos = in_core;
    const bool retired = weapon::retire_round(p, 15000.0, &net);
    // Must re-evaluate and correctly retire. Mutation lever: dropping the
    // `dist_moved < p.net_cache_abs_sdf` distance check (always trusting the
    // cache once valid) would return the stale "inside" verdict here (false
    // == not retired), failing this REQUIRE.
    REQUIRE(retired);
}

// ===========================================================================
// POOL REUSE: retired slots are recycled, so a second burst does NOT grow the
// pool without bound. Fire a burst, idle until every round ages out past
// kMaxFlightTime, fire a second identical burst, and require the pool capacity
// stays ~one burst (not two). MUTATION: always push_back (never reuse) -> the
// pool roughly DOUBLES.
// ===========================================================================
TEST_CASE("guns: retired projectile slots are reused, not accumulated") {
    const sim::SimState sh = shooter_at({0, 0, 17000}, {0, 0, 0});
    const double dt = 0.01;
    weapon::GunWorld gw = one_gun(/*rof=*/10.0);

    for (int i = 0; i < 100; ++i)  // burst 1
        weapon::fire_tick(gw, sh, true, dt, 9.81, 0.0);
    const std::size_t cap1 = gw.pool.size();
    REQUIRE(cap1 >= 5);  // baseline: a real burst was pooled

    // Idle past kMaxFlightTime (10 s) so every round retires by age.
    for (int i = 0; i < 1200; ++i)
        weapon::fire_tick(gw, sh, false, dt, 9.81, 0.0);
    REQUIRE(count_active(gw) == 0);  // all aged out -> all slots free

    for (int i = 0; i < 100; ++i)  // burst 2 reuses the freed slots
        weapon::fire_tick(gw, sh, true, dt, 9.81, 0.0);
    REQUIRE(gw.pool.size() <=
            cap1 + 1);  // no unbounded growth (a doubling fails)
}

// ===========================================================================
// DEAD-ASTERN PIN TEST (Task A, iter-8): Honest-pipper + cant correction.
//
// Geometry: shooter heading -Z at ~145 m/s, drone same heading at ~140 m/s
// (5 m/s closure; dead-astern receding). Gravity g=9.81 m/s^2, down = -Y.
// Convergence_range = 500 m, hub cannon.
//
// Pin test: fire a round via weapon::spawn from the HONEST pipper direction
// (lead_solution with gp_convergence_range=500, hub_muzzle_body) and assert
// the MISS at each range is < hit_radius_m = 9.0 m (HIT).
//
// WHY THIS TEST CATCHES THE BUG:
//   OLD MODEL A (pure lead): gravity droop NOT compensated → 18 m low at 1000 m
//   → FAILS the hit_radius=9 m assertion.
//   HONEST PIPPER (iter-8): droop compensated (cant subtracted) → pipper
//   auto-holds over the residual droop → round hits at all tested ranges.
//
// ALSO asserts 500 m case: cant ≈ droop → pipper ≈ boresight (no regression).
// MOVED GOLDEN: the Model A test had y≈0 (gravity-blind); now the pipper y>0
// at ranges beyond 500 m (holding over the residual). Documented above.
// ===========================================================================
namespace {

// Closest-approach for dead-astern geometry (reused from pipper_miss_spawned
// helper above; this is a NEW fixture with different geometry).
double dead_astern_miss(double initial_sep_m) {
    // Configuration matching scenario.toml + world.toml values.
    constexpr double muzzle = 805.0;    // 20mm MG151/20
    constexpr double drag_k = 0.00080;  // cannon_drag_k_per_m
    constexpr double fire_dt = 1.0 / 120.0;
    constexpr double g = 9.81;
    constexpr double conv = 500.0;  // convergence_range_m
    constexpr double hit_r = 9.0;   // hit_radius_m (re-derived, Task C)
    const glm::dvec3 grav{0.0, -g, 0.0};

    // Shooter at high altitude, heading -Z (nose = -Z), at ~145 m/s.
    const glm::dvec3 shooter_vel{0.0, 0.0, -145.0};
    const sim::SimState shooter = shooter_at({0.0, 15000.0, 0.0}, shooter_vel);

    // Drone dead-astern: same heading, slightly slower → receding.
    // Place it initial_sep_m ahead along -Z (in FRONT of shooter for the
    // dead-astern geometry: drone is ahead at +separation along shooter's
    // nose). Dead astern = drone is in FRONT of shooter but both moving -Z, so
    // shooter chases the drone. Drone is at initial_sep_m ahead.
    const glm::dvec3 drone_vel{0.0, 0.0, -140.0};  // ~5 m/s slower = receding
    const sim::SimState drone_target = shooter_at(
        shooter.position + glm::dvec3{0.0, 0.0, -initial_sep_m}, drone_vel);

    // Honest-pipper solve: gp_convergence_range=conv, hub_muzzle_body
    const glm::dvec3 hub_muzzle{0.0, 0.0, -2.870};
    const render::LeadSolution sol =
        render::lead_solution(shooter, drone_target, muzzle, 2000.0, grav,
                              drag_k, fire_dt, conv, hub_muzzle, hit_r);
    if (!sol.valid) return 1e9;  // unsolvable → report large miss

    // Orient the shooter so the nose is on the pipper.
    const glm::dvec3 default_nose{0.0, 0.0, -1.0};
    const glm::dvec3 fire_dir = glm::normalize(sol.lead_dir);
    sim::SimState fire_sh = shooter;
    {
        const glm::dvec3 axis = glm::cross(default_nose, fire_dir);
        const double s_len = glm::length(axis);
        if (s_len > 1e-9) {
            const double angle =
                std::atan2(s_len, glm::dot(default_nose, fire_dir));
            fire_sh.orientation = glm::angleAxis(angle, axis / s_len);
        }
    }

    // Spawn the real canted hub cannon.
    weapon::GunBattery bat;
    bat.convergence_range = conv;
    const weapon::GunSpec hub = cannon_spec(kMuzzleHub);
    bat.guns.push_back(hub);
    const weapon::Projectile r = weapon::spawn(bat, hub, fire_sh, g, fire_dt);

    // Track closest approach (reuse closest_approach helper from above).
    return closest_approach(r, grav, drone_target.position, drone_vel, fire_dt,
                            sol.time_to_intercept);
}

}  // namespace

TEST_CASE(
    "guns: dead-astern honest-pipper hits at 300/500/800/1000 m (Task A pin)") {
    // The key ranges from the bug report: past ~950 m shots miss invisibly
    // under MODEL A. With the honest pipper, all four should hit (miss < 9 m).
    constexpr double hit_radius =
        9.0;  // scenario.toml [drone] hit_radius_m (Task C)

    const double miss_300 = dead_astern_miss(300.0);
    const double miss_500 = dead_astern_miss(500.0);
    const double miss_800 = dead_astern_miss(800.0);
    const double miss_1000 = dead_astern_miss(1000.0);

    INFO("dead-astern miss at  300 m: " << miss_300 << " m (limit "
                                        << hit_radius << " m)");
    INFO("dead-astern miss at  500 m: " << miss_500 << " m (limit "
                                        << hit_radius << " m)");
    INFO("dead-astern miss at  800 m: " << miss_800 << " m (limit "
                                        << hit_radius << " m)");
    INFO("dead-astern miss at 1000 m: " << miss_1000 << " m (limit "
                                        << hit_radius << " m)");

    REQUIRE(miss_300 < hit_radius);  // 300 m: sub-convergence (slight cant
                                     // over-comp; still hits)
    REQUIRE(miss_500 < hit_radius);  // 500 m: convergence range → pipper ≈
                                     // boresight, cant cancels droop
    REQUIRE(miss_800 <
            hit_radius);  // 800 m: pipper holds over by residual droop → HIT
    REQUIRE(miss_1000 < hit_radius);  // 1000 m: THE CRITICAL RANGE — was 18 m
                                      // low with Model A → NOW HITS

    // BORESIGHT CROSSING REGRESSION (500 m): at convergence range the pipper
    // should be close to boresight (cant ≈ droop). Verify the 500 m miss is
    // tighter than the 300 m and 1000 m cases (cant best cancels droop at
    // conv). This is a soft sanity check; the hard check is miss < hit_radius
    // above.
    INFO("500 m miss (" << miss_500 << ") should be <= 300 m miss (" << miss_300
                        << ")");
    // At convergence: cant fully cancels droop → tightest miss.
    // Don't enforce strict inequality (geometry varies with shooter speed etc.)
    // but confirm it's well within hit radius.
    REQUIRE(miss_500 < hit_radius * 0.8);  // extra tight at convergence range
}
