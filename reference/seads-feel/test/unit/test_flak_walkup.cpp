// ★★★ L5 — THE FLAK WALK-UP
// (docs/PLAN_20260901_game_loop_millwright.md §4.5, docs/FLAK_GUN_SPEC.md §7).
//
// WHAT THIS RUNG REPLACED, AND WHY THE REPLACEMENT NEEDS LEGS.
//
// Before it, O manned the gun from a STOPPED SLED OR A LANDED AEROPLANE WITHIN
// 30 METRES. That rule carried its own label in `app/main.cpp` -- "SCAFFOLDING
// of the KEY_J class: the walk-up is R5+, no on-foot state exists yet" -- and
// the on-foot state now exists, so the rule is deleted rather than kept as a
// fallback. Deleting a rule is exactly the change a gate cannot see: nothing
// goes red when a permissive path survives. So the legs below are written as
// the three things the OLD rule allowed and the new one must not:
//
//   * manning it from the saddle at all (`from_sled`),
//   * manning it from twenty metres away (`30 m`),
//   * manning it from a body that is not on its feet (never gated at all).
//
// ★★★ THE NAMED WRONG IMPLEMENTATION: re-admitting the old rule as an OR --
// `Sled && dist < 30` alongside the new gate. "the saddle is not a firing
// step" and "the gun is not manned from across the pad" are red under it and
// nothing else here is.
//
// The geometry half (`app/flak_walkup.h`) exists as its own header for the
// same reason: `main()` needs it in two places that must agree exactly -- the
// site the key is gated on, and the mark the man is put down on when he lets
// go -- and neither could be executed from a test while it lived inside a
// seven-thousand-line frame block.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <glm/glm.hpp>

#include "app/flak_walkup.h"
#include "app/interact.h"
#include "app/player_mode.h"
#include "app/walker_place.h"
#include "render/flak_gun.h"
#include "sim/walker.h"

using Catch::Approx;

namespace {

constexpr double kR = 15000.0;
// The man's own standing height above the drive surface (app/main.cpp sets
// `walker_params.lie_clearance_m` from the machine's cg height).
constexpr double kStandH = 0.564;

// A gun on the ball at a GENERIC point: no world axis is up, none is forward,
// so a leg cannot pass by accidentally agreeing with an axis.
render::flak::MountFrame gun_frame() {
    const glm::dvec3 up = glm::normalize(glm::dvec3(0.4, 1.0, 0.25));
    return render::flak::make_mount_frame(up * kR, up,
                                          glm::dvec3(0.3, -0.2, 0.9));
}

// The shipped station table's shape, not its numbers: `st_approach` is a TRAIN
// station at y == 0 sitting BEHIND the shoulder pads (test_flak_gun.cpp pins
// both facts off the GLB). -Z is behind in the model frame the loader uses.
render::flak::Stations stations() {
    render::flak::Stations s;
    s.approach = glm::dvec3(0.0, 0.0, -1.9);
    s.trunnion_h = 1.2;
    return s;
}

// The site table `app/main.cpp` builds every frame, in the form this rung
// cares about: one own flak gun at its approach mark.
struct Table {
    app::InteractSite sites[4];
    int n = 0;
    void add(app::SiteKind k, const glm::dvec3& p, double reach, int idx = -1) {
        app::InteractSite& s = sites[n++];
        s.kind = k;
        s.pos_w = p;
        s.reach_m = reach;
        s.index = idx;
    }
};

app::ModeContext ctx_of(const Table& t, const glm::dvec3& man,
                        app::PlayerMode mode, bool man_upright) {
    return app::mode_context_from(t.sites, t.n, man, mode,
                                  /*aircraft_ready=*/false,
                                  /*sled_seeded=*/true,
                                  /*sled_stopped=*/true, man_upright,
                                  /*repair_complete=*/false);
}

// Where `app/main.cpp` puts the site: the mark, re-grounded.
glm::dvec3 mark(const render::flak::MountFrame& mf,
                const render::flak::Stations& st, double train_rad) {
    return app::flak_stand_pos(app::flak_approach_world(mf, st, train_rad), kR,
                               kStandH);
}

}  // namespace

TEST_CASE("flak walk-up: the approach mark rides the gun's own train") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();

    // At zero train the mark is BEHIND the muzzle: its offset from the
    // pedestal has a negative component along the mount's forward.
    const glm::dvec3 a0 = app::flak_approach_world(mf, st, 0.0);
    CHECK(glm::dot(a0 - mf.pos, mf.fwd0) < -1.0);
    // ...and it is on the pedestal's own plane (st_approach.y == 0), not up on
    // the cradle: no component along the mount's up.
    CHECK(glm::dot(a0 - mf.pos, mf.up) == Approx(0.0).margin(1e-9));

    // ★ TRAIN THE GUN HALF A TURN AND THE MARK COMES WITH IT. A mark read at
    // zero train would sit in FRONT of a gun trained 180 degrees round -- the
    // player would have to stand in the muzzle blast to press O.
    const glm::dvec3 a180 = app::flak_approach_world(mf, st, render::flak::kPi);
    CHECK(glm::dot(a180 - mf.pos, mf.fwd0) > 1.0);
    // Same standoff, opposite side.
    CHECK(glm::length(a180 - mf.pos) ==
          Approx(glm::length(a0 - mf.pos)).margin(1e-9));
    CHECK(glm::length(a180 - a0) > 3.0);

    // A quarter turn puts it beside, not fore or aft -- so the mark tracks the
    // train continuously and not just at the two ends.
    const glm::dvec3 a90 =
        app::flak_approach_world(mf, st, render::flak::kPi / 2.0);
    CHECK(glm::dot(a90 - mf.pos, mf.fwd0) == Approx(0.0).margin(1e-9));
    CHECK(std::fabs(glm::dot(a90 - mf.pos, mf.right0)) > 1.0);
}

TEST_CASE("flak walk-up: the mark is re-grounded to where the man stands") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();
    const glm::dvec3 a = app::flak_approach_world(mf, st, 0.4);

    // ★ THE STATION'S OWN RADIUS IS NOT THE MAN'S. The station sits on the
    // terrain plane under the pedestal and the walker lives a lie-clearance
    // above the DRIVE (snow) surface; standing him on the raw station would
    // spend better than a metre of a three-metre reach on a vertical offset --
    // a reach gate that fails while you are standing on the mark.
    const glm::dvec3 stand = app::flak_stand_pos(a, kR, kStandH);
    CHECK(glm::length(stand) == Approx(kR + kStandH).margin(1e-9));
    // ...and it is the SAME DIRECTION: the re-grounding moves him vertically
    // only, so the mark he walks to is the mark he stands on.
    CHECK(glm::dot(glm::normalize(stand), glm::normalize(a)) ==
          Approx(1.0).margin(1e-12));

    // Facing AWAY from the gun once he lets go.
    const glm::dvec3 face = app::flak_face_away(stand, mf.pos);
    CHECK(glm::dot(face, stand - mf.pos) > 0.0);
    sim::WalkerState w;
    app::walker_place_afoot(w, stand, face);
    CHECK(w.mode == sim::WalkerMode::Afoot);  // BY NAME
    CHECK(glm::length(w.pos - stand) == Approx(0.0).margin(1e-12));
    // The heading is a TANGENT at his feet (walker_place_afoot's job) and it
    // still points away from the pedestal.
    CHECK(glm::length(w.heading) == Approx(1.0).margin(1e-9));
    CHECK(glm::dot(w.heading, glm::normalize(w.pos)) ==
          Approx(0.0).margin(1e-12));
    CHECK(glm::dot(w.heading, stand - mf.pos) > 0.0);
}

TEST_CASE("flak walk-up: the gun is not manned from across the pad") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();
    const glm::dvec3 m = mark(mf, st, 0.0);
    const double reach = 3.0;  // [interact] gun_reach_m

    Table t;
    t.add(app::SiteKind::FlakGun, m, reach, 0);

    // Twenty metres away -- INSIDE the deleted 30 m rule, outside the reach.
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::normalize(m), glm::dvec3(0, 1, 0.3)));
    app::PlayerModeState st_m;
    st_m.mode = app::PlayerMode::Afoot;
    const app::ModeContext far =
        ctx_of(t, m + east * 20.0, st_m.mode, /*man_upright=*/true);
    CHECK(!far.gun_in_reach);
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, far) ==
          app::ModeAction::None);
    CHECK(st_m.mode == app::PlayerMode::Afoot);

    // Just outside the reach: still nothing. The gate is the reach, not a
    // rounded-off idea of "near".
    const app::ModeContext edge =
        ctx_of(t, m + east * (reach + 0.05), st_m.mode, true);
    CHECK(!edge.gun_in_reach);
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, edge) ==
          app::ModeAction::None);

    // ...and inside it, he gets on.
    const app::ModeContext at =
        ctx_of(t, m + east * (reach - 0.05), st_m.mode, true);
    CHECK(at.gun_in_reach);
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, at) ==
          app::ModeAction::ManGun);
    CHECK(st_m.mode == app::PlayerMode::OnGun);

    // O again lets go, and it does NOT need reach to do it -- a man who could
    // not leave a gun he had somehow drifted off the mark of would be stuck on
    // it forever.
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, far) ==
          app::ModeAction::LeaveGun);
    CHECK(st_m.mode == app::PlayerMode::Afoot);
}

TEST_CASE("flak walk-up: the saddle is not a firing step") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();
    const glm::dvec3 m = mark(mf, st, 0.0);
    Table t;
    t.add(app::SiteKind::FlakGun, m, 3.0, 0);

    // ★ RIGHT ON THE MARK, ON THE MACHINE. The old rule manned the gun from a
    // stopped sled; Chad's R5 is "Sudburian walks up to the flak gun and
    // engages", so you get off first -- and the site table says so twice over
    // (nothing is legal in mode Sled, so `gun_in_reach` is false as well).
    app::PlayerModeState sled;
    sled.mode = app::PlayerMode::Sled;
    sled.sled_seeded = true;
    const app::ModeContext c = ctx_of(t, m, sled.mode, /*man_upright=*/true);
    CHECK(!c.gun_in_reach);
    CHECK(app::player_mode_transition(sled, app::ModeEvent::GunKey, c) ==
          app::ModeAction::None);
    CHECK(sled.mode == app::PlayerMode::Sled);

    // Nor from the cockpit -- the other half of the deleted rule.
    app::PlayerModeState pilot;
    const app::ModeContext cp = ctx_of(t, m, pilot.mode, true);
    CHECK(!cp.gun_in_reach);
    CHECK(app::player_mode_transition(pilot, app::ModeEvent::GunKey, cp) ==
          app::ModeAction::None);
    CHECK(pilot.mode == app::PlayerMode::Pilot);
}

TEST_CASE("flak walk-up: a man face-down in the snow does not man the gun") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();
    const glm::dvec3 m = mark(mf, st, 0.0);
    Table t;
    t.add(app::SiteKind::FlakGun, m, 3.0, 0);

    // `PlayerMode::Afoot` only means the keys are the man's. Fall off the
    // machine onto the gun pad and `sim::WalkerMode` is Falling / Buried /
    // Down / CrawlProne while the outer mode is already Afoot -- so without
    // the upright bit, O mounted the gun from a body in the snow. The walker
    // is the sole authority for the posture and it is asked BY ENUMERATOR NAME
    // in `app/main.cpp`; here it arrives as the bit that answer becomes.
    app::PlayerModeState st_m;
    st_m.mode = app::PlayerMode::Afoot;
    const app::ModeContext down =
        ctx_of(t, m, st_m.mode, /*man_upright=*/false);
    CHECK(down.gun_in_reach);  // he IS in reach -- posture is the refusal
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, down) ==
          app::ModeAction::None);
    CHECK(st_m.mode == app::PlayerMode::Afoot);

    // He gets up, and the same press takes.
    const app::ModeContext up = ctx_of(t, m, st_m.mode, true);
    CHECK(app::player_mode_transition(st_m, app::ModeEvent::GunKey, up) ==
          app::ModeAction::ManGun);
    CHECK(st_m.mode == app::PlayerMode::OnGun);
}

TEST_CASE("flak walk-up: he steps off onto the mark, not where he got on") {
    const render::flak::MountFrame mf = gun_frame();
    const render::flak::Stations st = stations();

    // He walked up from one side, got on, and trained the gun 100 degrees
    // round while manning it. Stepping off must put him on the mark WHERE IT
    // IS NOW -- the platform he is standing on turned with the gun -- and not
    // back at the pre-mount spot, which is the whole difference between
    // getting off a machine and being teleported.
    const glm::dvec3 pre = mark(mf, st, 0.0);
    const double trained = 100.0 * render::flak::kDeg;
    const glm::dvec3 now = mark(mf, st, trained);
    CHECK(glm::length(now - pre) > 2.0);

    sim::WalkerState w;
    app::walker_place_afoot(w, now, app::flak_face_away(now, mf.pos));
    CHECK(glm::length(w.pos - now) == Approx(0.0).margin(1e-12));
    CHECK(glm::length(w.pos - pre) > 2.0);
    CHECK(w.mode == sim::WalkerMode::Afoot);
    // And he is standing on the drive surface, not floating at the station's
    // own radius.
    CHECK(glm::length(w.pos) == Approx(kR + kStandH).margin(1e-9));
}
