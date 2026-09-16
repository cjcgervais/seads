// ★★★ L1 — THE MILLWRIGHT LOOP'S OUTER MODE MACHINE
// (docs/PLAN_20260901_game_loop_millwright.md §4.1).
//
// WHAT THESE LEGS EXIST TO CATCH.
//
// The rung's whole risk is a state machine written INSIDE a seven-thousand-line
// `main()`, where the only way to exercise a transition is to open a window and
// press a key. That is the shape this ladder has lost rungs to before (the gait
// law that lived in a TU no test could compile, render/body_drive.h's banner).
// So the decision lives in `app/player_mode.h` as a pure table, the reach law
// lives in `app/interact.h` as pure geometry, and the two seams that write a
// rider onto and off a machine live in `app/player_mount.h` -- and every one of
// them is executed here with no raylib, no window, and no world.
//
// ★★★ AND THE ONE THAT IS A MUTATION LEG BY CONSTRUCTION: the CO-ATTACH.
// `app/main.cpp`'s KEY_R autoright re-attached `sled.grip` and, for a while,
// left the man where he had fallen. The consequence was not a visible bug but a
// SILENT one -- `sim::walker_throw` returns early on a man who is already off,
// so every later fall did nothing, forever. The named wrong implementation is
// "grip re-attached without the man"; `mount seam re-attaches the grip AND the
// man` below is red under exactly that and green under nothing else.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

#include "app/interact.h"
#include "app/player_mode.h"
#include "app/player_mount.h"
#include "app/walker_place.h"
#include "sim/sled.h"
#include "sim/walker.h"

namespace {

// A machine sitting on the drive surface of the 15 km ball, nose along +Z's
// tangent. Doubles, and world -- the sphere invariants are not negotiable in a
// test either.
constexpr double kR = 15000.0;

sim::SledState parked_sled() {
    sim::SledState s;
    const glm::dvec3 up{0.0, 1.0, 0.0};
    const glm::dvec3 fwd{0.0, 0.0, -1.0};
    glm::dmat3 basis;
    basis[0] = glm::cross(fwd, up);  // +X = RIGHT
    basis[1] = up;
    basis[2] = -fwd;
    s.orientation = glm::normalize(glm::quat_cast(basis));
    s.position = up * (kR + 0.564);
    s.velocity = glm::dvec3{0.0};
    return s;
}

// The site table `app/main.cpp` builds every frame, in its smallest honest
// form: a machine and a damaged own pump.
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

// ★ `man_upright` IS REQUIRED, NEVER DEFAULTED. It is the bit a red team
// found F admitting a repair without (2026-09-01); a helper that filled it in
// for a forgetful caller would be the same hole one layer up. `repair_complete`
// defaults because it is only read while Repairing and false is the state every
// leg but the finish leg is in.
app::ModeContext ctx_of(const Table& t, const glm::dvec3& man,
                        app::PlayerMode mode, bool aircraft_ready,
                        bool sled_seeded, bool sled_stopped, bool man_upright,
                        bool repair_complete = false) {
    return app::mode_context_from(t.sites, t.n, man, mode, aircraft_ready,
                                  sled_seeded, sled_stopped, man_upright,
                                  repair_complete);
}

}  // namespace

TEST_CASE("player mode: the aircraft only gives up the pilot when parked") {
    app::PlayerModeState st;
    app::ModeContext c;
    c.aircraft_ready = false;
    // The pre-L1 law, verbatim: you cannot leave a flying aeroplane.
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::RefuseMountMoving);
    CHECK(st.mode == app::PlayerMode::Pilot);
    CHECK(!st.sled_seeded);

    c.aircraft_ready = true;
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::SeedAndMount);
    CHECK(st.mode == app::PlayerMode::Sled);
    // ★ THE MACHINE NOW EXISTS IN THE WORLD, and this bit is no longer the same
    // bit as "the player is riding it". That separation is the rung.
    CHECK(st.sled_seeded);
}

TEST_CASE("player mode: you do not step off a moving snowmachine") {
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Sled;
    st.sled_seeded = true;
    app::ModeContext c;
    c.sled_seeded = true;
    c.sled_stopped = false;
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::RefuseDismountMoving);
    CHECK(st.mode == app::PlayerMode::Sled);

    c.sled_stopped = true;
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::Dismount);
    CHECK(st.mode == app::PlayerMode::Afoot);
    // ★ AND THE MACHINE IS STILL THERE. A dismount does not un-seed it.
    CHECK(st.sled_seeded);
}

TEST_CASE("player mode: the mount is refused beyond reach and taken at it") {
    Table t;
    const sim::SledState sled = parked_sled();
    t.add(app::SiteKind::SledMount, sled.position, 2.5);
    const glm::dvec3 up = glm::normalize(sled.position);
    const glm::dvec3 east =
        glm::normalize(glm::cross(up, glm::dvec3{0.0, 0.0, 1.0}));

    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    st.sled_seeded = true;

    // Three metres away: nothing in reach, so J does nothing at all -- not a
    // refusal message, NOTHING. A key near nothing shows nothing.
    const glm::dvec3 far_pos = sled.position + east * 3.0;
    app::ModeContext c =
        ctx_of(t, far_pos, app::PlayerMode::Afoot, false, true, true, true);
    CHECK(!c.sled_in_reach);
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Afoot);

    // Two metres away: inside the 2.5 m reach.
    const glm::dvec3 near_pos = sled.position + east * 2.0;
    c = ctx_of(t, near_pos, app::PlayerMode::Afoot, false, true, true, true);
    CHECK(c.sled_in_reach);
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::Mount);
    CHECK(st.mode == app::PlayerMode::Sled);
}

TEST_CASE("player mode: one J press produces exactly one mount") {
    // The J key is a CONTEXT key: the same press mounts, dismounts, or does
    // nothing depending on where the player is standing. The failure this leg
    // pins is a press that produces the mount action twice (or that mounts and
    // then immediately dismounts because the table fell through).
    Table t;
    const sim::SledState sled = parked_sled();
    t.add(app::SiteKind::SledMount, sled.position, 2.5);
    const glm::dvec3 up = glm::normalize(sled.position);
    const glm::dvec3 east =
        glm::normalize(glm::cross(up, glm::dvec3{0.0, 0.0, 1.0}));
    const glm::dvec3 man = sled.position + east * 1.0;

    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    st.sled_seeded = true;

    int mounts = 0, dismounts = 0;
    for (int press = 0; press < 4; ++press) {
        const app::ModeContext c =
            ctx_of(t, man, st.mode, false, true, /*sled_stopped=*/true,
                   /*man_upright=*/true);
        const app::ModeAction a =
            app::player_mode_transition(st, app::ModeEvent::MountKey, c);
        if (a == app::ModeAction::Mount) ++mounts;
        if (a == app::ModeAction::Dismount) ++dismounts;
    }
    // Press 1 mounts, press 2 dismounts, press 3 mounts, press 4 dismounts:
    // the machine is never seeded twice and the man is never mounted twice in
    // a row.
    CHECK(mounts == 2);
    CHECK(dismounts == 2);
    CHECK(st.mode == app::PlayerMode::Afoot);
}

TEST_CASE(
    "interact: a site is only reachable on foot, and only by its own "
    "reach") {
    Table t;
    const sim::SledState sled = parked_sled();
    t.add(app::SiteKind::SledMount, sled.position, 2.5);
    const glm::dvec3 up = glm::normalize(sled.position);
    const glm::dvec3 east =
        glm::normalize(glm::cross(up, glm::dvec3{0.0, 0.0, 1.0}));
    // A pump 5 m away with a 6 m reach: FURTHER than the machine and still in
    // range, which is the case a single global radius would get wrong.
    t.add(app::SiteKind::PumpRepair, sled.position + east * 5.0, 6.0, 0);

    const glm::dvec3 man = sled.position + east * 1.0;
    // From the saddle, nothing is offered -- you get off first (Chad's R5).
    CHECK(app::nearest_site(t.sites, t.n, man, app::PlayerMode::Sled).kind ==
          app::SiteKind::None);
    CHECK(app::nearest_site(t.sites, t.n, man, app::PlayerMode::Pilot).kind ==
          app::SiteKind::None);

    // On foot the NEAREST wins the prompt line...
    const app::InteractSite near_site =
        app::nearest_site(t.sites, t.n, man, app::PlayerMode::Afoot);
    CHECK(near_site.kind == app::SiteKind::SledMount);
    CHECK(std::string(app::interact_prompt(near_site)) == "J  GET ON");

    // ... but BOTH keys are armed, because each key has its own site kind and
    // the nearer thing must not silently disarm the other one.
    const app::ModeContext c =
        ctx_of(t, man, app::PlayerMode::Afoot, false, true, true, true);
    CHECK(c.sled_in_reach);
    CHECK(c.pump_in_reach);
    CHECK(c.pump_index == 0);

    // While Repairing only the pump stays live: walking to the machine must
    // not offer a mount out of a job in progress.
    CHECK(
        app::nearest_site(t.sites, t.n, man, app::PlayerMode::Repairing).kind ==
        app::SiteKind::PumpRepair);
}

TEST_CASE(
    "player mode: F starts the fix, F again and walking away both end "
    "it") {
    Table t;
    const glm::dvec3 pump{0.0, kR, 0.0};
    t.add(app::SiteKind::PumpRepair, pump, 6.0, 1);
    const glm::dvec3 east{1.0, 0.0, 0.0};

    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;

    // Out of reach: F does nothing. A key near nothing shows nothing.
    app::ModeContext c =
        ctx_of(t, pump + east * 9.0, st.mode, false, false, false, true);
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, c) ==
          app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);

    // In reach: the job starts, and the L2 hook carries WHICH pump.
    c = ctx_of(t, pump + east * 4.0, st.mode, false, false, false, true);
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, c) ==
          app::ModeAction::BeginRepair);
    CHECK(st.mode == app::PlayerMode::Repairing);
    CHECK(st.repairing);
    CHECK(st.repair_pump == 1);

    // F again pauses it, and clears BOTH halves of the hook -- a mode that
    // said Afoot while `repairing` stayed true would hand L2 a clock with no
    // man at the pump.
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, c) ==
          app::ModeAction::EndRepair);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);

    // And so does walking away, through the same two writes.
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, c) ==
          app::ModeAction::BeginRepair);
    const app::ModeContext gone =
        ctx_of(t, pump + east * 9.0, st.mode, false, false, false, true);
    CHECK(app::player_mode_update(st, gone) == app::ModeAction::EndRepair);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);
    // ... and staying put does NOT end it (an update that fired every tick
    // would make the job unstartable).
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, c) ==
          app::ModeAction::BeginRepair);
    CHECK(app::player_mode_update(st, c) == app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Repairing);
}

TEST_CASE("player mode: the gun site exists and O is dead without one") {
    // No gun is registered on this branch (L5 is blocked on the flak lane), so
    // the key must be inert -- not a message, not a mode change.
    Table t;
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    const glm::dvec3 man{0.0, kR, 0.0};
    app::ModeContext c = ctx_of(t, man, st.mode, false, false, false, true);
    CHECK(!c.gun_in_reach);
    CHECK(app::player_mode_transition(st, app::ModeEvent::GunKey, c) ==
          app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Afoot);

    // But the KIND is live: the day the flak lane registers a site, one block
    // in main.cpp is the whole of L5.
    t.add(app::SiteKind::FlakGun, man + glm::dvec3{2.0, 0.0, 0.0}, 3.0, 0);
    c = ctx_of(t, man, st.mode, false, false, false, true);
    CHECK(c.gun_in_reach);
    CHECK(app::player_mode_transition(st, app::ModeEvent::GunKey, c) ==
          app::ModeAction::ManGun);
    CHECK(st.mode == app::PlayerMode::OnGun);
    CHECK(app::player_mode_transition(st, app::ModeEvent::GunKey, c) ==
          app::ModeAction::LeaveGun);
    CHECK(st.mode == app::PlayerMode::Afoot);
}

TEST_CASE(
    "player mode: the aeroplane is boardable on foot so flying survives "
    "the loop") {
    // ★ THE DEVIATION, PINNED. The plan lists three site kinds and none of them
    // gets the pilot back into the cockpit; with only those, one J press on a
    // stopped machine ends flying for the session. The fourth kind is the fix
    // and this leg is what stops it being quietly deleted as "not in the plan".
    Table t;
    const glm::dvec3 plane{0.0, kR, 0.0};
    t.add(app::SiteKind::AircraftBoard, plane, 3.0);
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    st.sled_seeded = true;
    const app::ModeContext c =
        ctx_of(t, plane + glm::dvec3{2.0, 0.0, 0.0}, st.mode, true, true, true,
               true);
    CHECK(c.aircraft_in_reach);
    CHECK(app::player_mode_transition(st, app::ModeEvent::MountKey, c) ==
          app::ModeAction::BoardAircraft);
    CHECK(st.mode == app::PlayerMode::Pilot);
    // The machine is still out there. It is not un-seeded by flying away.
    CHECK(st.sled_seeded);
}

TEST_CASE("mount seam re-attaches the grip AND the man") {
    // ★★★ THE MUTATION LEG. The named wrong implementation is the
    // pre-2026-09-01 KEY_R autoright: `sled.grip = sim::GripState{}` and
    // NOTHING ELSE. Under it the first CHECK passes and the second fails --
    // which is exactly the shape of the original defect, where the machine
    // looked fixed and every later fall was silently dead because
    // `walker_throw` returns early on a man who is already off.
    sim::SledState sled = parked_sled();
    sim::WalkerState walker;
    // He has fallen: the latch is broken and he is a free body in the snow.
    sled.grip.attached = false;
    sled.grip.load_lp = 900.0;
    walker.mode = sim::WalkerMode::Down;
    walker.pos = sled.position + glm::dvec3{40.0, 0.0, 0.0};
    walker.vel = glm::dvec3{7.0, 0.0, 0.0};

    app::player_mount_request(sled, walker);

    CHECK(sled.grip.attached);
    // ★ THE HALF THE MUTATION DROPS.
    CHECK(walker.mode == sim::WalkerMode::Riding);
    // And the whole grip struct is reset, not just the latch: `grip` is not in
    // the sled tape's pin roster, so a replay rebuilds it from struct defaults
    // and a carried `load_lp` would fork record from replay on the next
    // substep.
    CHECK(sled.grip.load_lp == 0.0);
    // ★ AND THE MACHINE DOES NOT MOVE. Mounting from the snow is walking back
    // to your machine, never being handed a new one at a new place -- that is
    // the whole of "the sled persists in the world".
    const sim::SledState ref = parked_sled();
    CHECK(glm::length(sled.position - ref.position) == 0.0);
}

TEST_CASE("dismount seam detaches the grip AND places the man") {
    // The mirror image, and it fails the same way in the other direction: a
    // dismount that placed the man but left the latch attached would leave the
    // machine drivable from fifty metres away, and one that detached the latch
    // while the man's mode was still Riding would be read by the tick loop as a
    // CRASH ("grip broke this tick") and dent the helmet on a deliberate step
    // off the running board.
    sim::SledState sled = parked_sled();
    sim::WalkerState walker;  // Riding, on the machine
    REQUIRE(walker.mode == sim::WalkerMode::Riding);
    REQUIRE(sled.grip.attached);

    const glm::dmat3 sR = glm::mat3_cast(sled.orientation);
    const glm::dvec3 dir = app::dismount_dir(sled.position, sR[0], 0.9);
    const glm::dvec3 pos = app::dismount_pos(dir, kR, 0.564);
    app::player_dismount_request(sled, walker, pos,
                                 sR * glm::dvec3{0.0, 0.0, -1.0});

    CHECK(!sled.grip.attached);
    CHECK(walker.mode == sim::WalkerMode::Afoot);
}

TEST_CASE("dismount geometry: 0.9 m off the LEFT board, on the drive surface") {
    const sim::SledState sled = parked_sled();
    const glm::dmat3 sR = glm::mat3_cast(sled.orientation);
    const glm::dvec3 right = sR[0];
    const glm::dvec3 dir = app::dismount_dir(sled.position, right, 0.9);
    const glm::dvec3 pos = app::dismount_pos(dir, kR, 0.564);

    // LEFT, not right: the offset projects NEGATIVE on the machine's own +X.
    const glm::dvec3 off = pos - sled.position;
    CHECK(glm::dot(off, right) < 0.0);
    // ... by very nearly the dial, the residue being the re-grounding (a
    // tangential step on a 15 km ball leaves the sphere; the placement is
    // re-projected rather than merely offset).
    CHECK(std::abs(glm::dot(off, right) + 0.9) < 1.0e-3);
    // ON the surface: his own origin a stand-height above the drive radius,
    // which is the invariant `sim/walker.cpp` re-projects him onto every step.
    CHECK(std::abs(glm::length(pos) - (kR + 0.564)) < 1.0e-9);
}

TEST_CASE(
    "walker placement asks for Afoot by name and gives him a tangent "
    "heading") {
    sim::WalkerState w;
    w.mode = sim::WalkerMode::Buried;
    w.vel = glm::dvec3{12.0, 3.0, 0.0};
    w.gait_phase = 0.77;
    w.submerge = 0.9;

    const glm::dvec3 pos{0.0, kR + 0.564, 0.0};
    // A heading with a large RADIAL component, as a body-frame forward handed
    // in off a machine on a ball always has.
    const glm::dvec3 raw{0.0, 0.4, -1.0};
    app::walker_place_afoot(w, pos, raw);

    CHECK(w.mode == sim::WalkerMode::Afoot);
    CHECK(glm::length(w.pos - pos) == 0.0);
    CHECK(glm::length(w.vel) == 0.0);
    // Tangent, and unit: a heading carried off a body frame is not tangent at
    // his feet, and `step_walker` re-orthogonalises but does not rescue a
    // radial one.
    CHECK(std::abs(glm::length(w.heading) - 1.0) < 1.0e-12);
    CHECK(std::abs(glm::dot(w.heading, glm::normalize(pos))) < 1.0e-12);
    // The fall's leftovers are gone -- he is standing, not mid-crawl.
    CHECK(w.gait_phase == 0.0);
    CHECK(w.submerge == 0.0);
    CHECK(w.t_mode_s == 0.0);
}

TEST_CASE("player mode reads: off_aircraft is the old drive_mode exactly") {
    // The refactor's own tripwire. `drive_mode` meant "the aeroplane has a dead
    // stick", which is true in EVERY state but Pilot -- including on foot,
    // where nobody is in the cockpit at all. A read that answered "riding the
    // machine" instead would hand the parked aircraft its controls back the
    // moment the player stepped off into the snow.
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Pilot;
    CHECK(!st.off_aircraft());
    CHECK(!st.driving());
    CHECK(!st.on_foot());
    st.mode = app::PlayerMode::Sled;
    CHECK(st.off_aircraft());
    CHECK(st.driving());
    CHECK(!st.on_foot());
    st.mode = app::PlayerMode::Afoot;
    CHECK(st.off_aircraft());
    CHECK(!st.driving());
    CHECK(st.on_foot());
    st.mode = app::PlayerMode::Repairing;
    CHECK(st.off_aircraft());
    CHECK(!st.driving());
    CHECK(st.on_foot());
    st.mode = app::PlayerMode::OnGun;
    CHECK(st.off_aircraft());
    CHECK(!st.driving());
}

// ★★★ THE FOUR RED-TEAM LEGS (2026-09-01). Each names the wrong
// implementation it is red under, because a leg that cannot name one is a leg
// that cannot fail.

// WRONG IMPLEMENTATION: `if (st.mode == Afoot && ctx.pump_in_reach)` -- the
// condition as shipped before the red team, with no question put to the walker.
// The repro was a real one: drop the machine inside `[repair] reach_m = 6.0` of
// your own damaged surface pump, press F while `sim::WalkerMode` is Buried, and
// the pump went from nothing to whole in sixty seconds with the man never once
// upright. `app::PlayerMode::Afoot` is set by the grip-break branch the instant
// he comes off; it means the keys are his, not that he is standing.
TEST_CASE("player mode: the fix is refused while the man is not on his feet") {
    Table t;
    const glm::dvec3 pump{0.0, kR, 0.0};
    t.add(app::SiteKind::PumpRepair, pump, 6.0, 1);
    const glm::dvec3 east{1.0, 0.0, 0.0};
    const glm::dvec3 at_pump = pump + east * 4.0;

    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;

    // FACE DOWN IN THE SNOW, standing right on top of the pump. The site is in
    // reach -- the geometry is not what refuses him.
    app::ModeContext down =
        ctx_of(t, at_pump, st.mode, false, false, false, /*man_upright=*/false);
    CHECK(down.pump_in_reach);
    CHECK(!down.man_upright);
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, down) ==
          app::ModeAction::None);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);

    // He gets up. Same position, same pump, same key.
    const app::ModeContext up =
        ctx_of(t, at_pump, st.mode, false, false, false, /*man_upright=*/true);
    CHECK(app::player_mode_transition(st, app::ModeEvent::InteractKey, up) ==
          app::ModeAction::BeginRepair);
    CHECK(st.mode == app::PlayerMode::Repairing);
    CHECK(st.repair_pump == 1);
}

// WRONG IMPLEMENTATION: `player_mode_update` returning `EndRepair` for both
// causes -- which is what it did, so a completed fix printed "FIX ABANDONED"
// over "PUMP BACK ON LINE" for two seconds. `combat::repair_pump_tick` sets
// `hp = max_hp` exactly and `alive = true`, and the site table drops a pump on
// `alive && hp >= max_hp`, so the very next frame has nothing in reach: the
// success and the failure are the SAME transition and only the pump own HP
// tells them apart.
TEST_CASE("player mode: a finished fix reports finished, not abandoned") {
    Table t;
    const glm::dvec3 pump{0.0, kR, 0.0};
    t.add(app::SiteKind::PumpRepair, pump, 6.0, 1);
    const glm::dvec3 east{1.0, 0.0, 0.0};

    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    const app::ModeContext at =
        ctx_of(t, pump + east * 4.0, st.mode, false, false, false, true);
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, at) ==
            app::ModeAction::BeginRepair);

    // THE PUMP IS WHOLE. The site table has dropped it, so nothing is in reach
    // -- the man has not moved a millimetre. `Table` with no sites is exactly
    // the frame after a revive.
    Table empty;
    const app::ModeContext done =
        ctx_of(empty, pump + east * 4.0, st.mode, false, false, false, true,
               /*repair_complete=*/true);
    CHECK(!done.pump_in_reach);
    CHECK(app::player_mode_update(st, done) == app::ModeAction::FinishRepair);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);

    // ... and the same shape with the pump still broken is still an abandon.
    // Both arms are needed: a stub returning FinishRepair always would pass the
    // first one alone, and the old EndRepair-always is the second one alone.
    st.mode = app::PlayerMode::Afoot;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey, at) ==
            app::ModeAction::BeginRepair);
    const app::ModeContext walked =
        ctx_of(t, pump + east * 9.0, st.mode, false, false, false, true,
               /*repair_complete=*/false);
    CHECK(app::player_mode_update(st, walked) == app::ModeAction::EndRepair);
}

// WRONG IMPLEMENTATION: `return st.off_aircraft();` -- i.e. R legal in every
// state but Pilot, which is the guard the block actually sat behind after L1
// widened `drive_mode` to `off_aircraft()`. R teleports the man onto the
// machine at ANY distance, so that guard let a player step off deliberately,
// walk the `[spawn] sled_dist_m = 300` to the pump, press R, and be back on the
// bars -- `[interact] sled_reach_m = 2.5` and the whole diegetic mount made
// decorative, and it fired from Repairing and OnGun too.
TEST_CASE(
    "player mode: the autoright rights a machine, it does not fetch one") {
    app::PlayerModeState st;
    st.sled_seeded = true;

    // ON the machine: always legal, rolled or not -- you are righting it under
    // yourself, which is what Chad asked the key for.
    st.mode = app::PlayerMode::Sled;
    CHECK(app::autoright_legal(st, /*sled_rolled=*/false));
    CHECK(app::autoright_legal(st, /*sled_rolled=*/true));

    // THROWN OFF, machine down: still legal at any distance. This is the crash
    // case the scaffolding exists for and it must not narrow.
    st.mode = app::PlayerMode::Afoot;
    CHECK(app::autoright_legal(st, /*sled_rolled=*/true));

    // STEPPED OFF, machine upright and parked where you left it: NOT legal.
    // Walk back and press J -- that is the walk-back Chad ruled in.
    CHECK(!app::autoright_legal(st, /*sled_rolled=*/false));

    // Mid-job and on the gun: R does not yank a man out of either, even with a
    // machine lying on its side somewhere.
    st.mode = app::PlayerMode::Repairing;
    CHECK(!app::autoright_legal(st, true));
    st.mode = app::PlayerMode::OnGun;
    CHECK(!app::autoright_legal(st, true));

    // And no machine means no key at all.
    st.mode = app::PlayerMode::Sled;
    st.sled_seeded = false;
    CHECK(!app::autoright_legal(st, true));
}

// WRONG IMPLEMENTATION: `return true;` -- the law before it was written down,
// under which `app/main.cpp` recorded the J dismount as a tape override.
// `grip` is not in the tape pin roster (test/harness/sled_tape.h,
// SLEDTAPE_PIN_D), replay applies an override by whole-struct assignment from
// a DEFAULT-constructed SledState, and `GripState::attached` defaults to TRUE
// and is a one-way latch -- so the record ran the machine without a rider and
// the replay ran it with one. test_sled_tape.cpp measures the fork; this leg
// pins the predicate main.cpp gates on.
TEST_CASE("mount seam: only a default grip may be pinned into a tape") {
    sim::SledState sled = parked_sled();
    sim::WalkerState walker;

    // A mounted machine is exactly the tape default and may be pinned.
    app::player_mount_request(sled, walker);
    CHECK(sled.grip.attached);
    CHECK(app::sled_override_replayable(sled));

    // A dismounted one may NOT: the latch is the one bit that structurally
    // cannot re-converge from the taped inputs.
    const glm::dvec3 up = glm::normalize(sled.position);
    const glm::dvec3 east =
        glm::normalize(glm::cross(up, glm::dvec3{0.0, 0.0, 1.0}));
    app::player_dismount_request(sled, walker, sled.position + east * 0.9,
                                 glm::dvec3{0.0, 0.0, -1.0});
    CHECK(!sled.grip.attached);
    CHECK(!app::sled_override_replayable(sled));

    // Getting back on makes it pinnable again -- which is why a walk-back
    // after a FALL stays inside one tape episode.
    app::player_mount_request(sled, walker);
    CHECK(app::sled_override_replayable(sled));
}

// WRONG IMPLEMENTATION: `inline void player_mode_force(PlayerModeState& st,
// PlayerMode m) { st.mode = m; }` -- the raw assignment `app/main.cpp` made at
// the snowmachine spawn and at the KEY_R autoright before this rung. Every
// path OUT of `Repairing` in the table above settles the L2 hook, so the hook
// was safe as long as the mode only ever moved through the table; the two
// direct writes are the two places it does not. Die mid-fix, respawn on a
// machine 300 m away, and `repairing` stayed true with `repair_pump` naming a
// pump nobody is standing at -- stranded silently, because the repair clock is
// gated on `mode == Repairing` and so never complains. (Red-team finding,
// 2026-09-01.)
TEST_CASE("player mode: a forced mode never strands the repair hook") {
    app::PlayerModeState st;

    // The stranding shape, verbatim: mid-repair, then the world moves him.
    st.mode = app::PlayerMode::Repairing;
    st.repairing = true;
    st.repair_pump = 1;
    app::player_mode_force(st, app::PlayerMode::Sled);  // the spawn / KEY_R
    CHECK(st.mode == app::PlayerMode::Sled);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);

    // The same for the fall -- Sled -> Afoot is a dismount nobody asked for,
    // and it must land on the same invariant.
    st.mode = app::PlayerMode::Repairing;
    st.repairing = true;
    st.repair_pump = 0;
    app::player_mode_force(st, app::PlayerMode::Afoot);
    CHECK(st.mode == app::PlayerMode::Afoot);
    CHECK(!st.repairing);
    CHECK(st.repair_pump == -1);

    // ★ AND IT IS AN INVARIANT, NOT A SPECIAL CASE. `repairing` is the mirror
    // of `mode == Repairing`; no forced mode may leave the two disagreeing,
    // including from a clean state where there was nothing to clear.
    for (const app::PlayerMode m :
         {app::PlayerMode::Pilot, app::PlayerMode::Sled,
          app::PlayerMode::Afoot, app::PlayerMode::Repairing,
          app::PlayerMode::OnGun}) {
        app::PlayerModeState f;
        app::player_mode_force(f, m);
        CHECK(f.mode == m);
        CHECK(f.repairing == (m == app::PlayerMode::Repairing));
        if (!f.repairing) CHECK(f.repair_pump == -1);
    }

    // The force does not invent a machine: `sled_seeded` is the world's fact,
    // not the mode's, and the spawn sets it separately.
    app::PlayerModeState g;
    app::player_mode_force(g, app::PlayerMode::Sled);
    CHECK(!g.sled_seeded);
}
