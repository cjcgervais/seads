// ★★★ STING RPAS — the P-key FPV interceptor (the sting lane, 2026-09-03;
// plan Game_loop_idea/PLAN_STING_RPAS.md).
//
// WHAT THESE LEGS EXIST TO CATCH. The mechanic's decisions live in
// app/sting.h as pure functions and in app/player_mode.h's table precisely so
// they can be executed here with no window: the deploy gate (upright,
// shouldered, stocked), the launch counter, the battery clock, the
// TWO-LARGE-TURNS law (a hysteretic event counter — the named wrong
// implementations are counting g-TICKS, and skipping the release hysteresis),
// the one-shot detonate command, and the ram sweep that reuses combat_tick
// verbatim with the drone AS the round.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/player_mode.h"
#include "app/sting.h"
#include "combat/kill.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "drone/drone.h"
#include "render/sting_audio.h"

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);
// The Sting's OWN derived airframe (fly-2) — the flight legs fly what ships.
const sim::AircraftParams kSap = app::sting_aircraft_params(kAp);

// A launch site out on the sphere at flight altitude, aim along a horizontal
// tangent — the same frame every drone test flies in.
const glm::dvec3 kSite = glm::dvec3(0.0, kAp.R + 2000.0, 0.0);
const glm::dvec3 kAim = glm::dvec3(0.0, 0.0, -1.0);  // horizontal at kSite

}  // namespace

// ===========================================================================
// The mode table: the drone key's gate and its two transitions.
// MUTATION: drop the man_upright or drone_ready clause -> the refusal legs go
// red; write the mode raw instead of through the table -> the J-while-flying
// leg goes red (J would mount from the pilot's chair of a flying drone).
// ===========================================================================
TEST_CASE("sting mode: deploy needs afoot + upright + ready, and ends home") {
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Afoot;
    app::ModeContext ctx;
    ctx.man_upright = true;
    ctx.drone_ready = true;

    // The full gate passes: Afoot -> Drone.
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::DeployDrone);
    REQUIRE(st.mode == app::PlayerMode::Drone);

    // The flight's end: Drone -> Afoot, same key event.
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::EndDrone);
    REQUIRE(st.mode == app::PlayerMode::Afoot);

    // A man face-down in the snow does not launch (the pump/gun law).
    ctx.man_upright = false;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::None);
    REQUIRE(st.mode == app::PlayerMode::Afoot);

    // Upright but not shouldered/stocked: no launch.
    ctx.man_upright = true;
    ctx.drone_ready = false;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::None);

    // From the cockpit, the key does nothing.
    st.mode = app::PlayerMode::Pilot;
    ctx.drone_ready = true;
    ctx.sled_upright = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::None);
    REQUIRE(st.mode == app::PlayerMode::Pilot);
}

// Chad 2026-09-04: "press P on the snowmachine ... deployment from the seat
// to the Sudburian's hands" -- no J first. MUTATION: drop the Sled leg ->
// the deploy goes red; forget drone_home -> the end drops him on foot
// beside a machine he never left.
TEST_CASE("sting mode: deploys from the seat and the end hands back the bars") {
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Sled;
    st.sled_seeded = true;
    app::ModeContext ctx;
    ctx.drone_ready = true;
    ctx.man_upright = false;  // seated: the walker is Riding, not Afoot
    // A rolled machine is no seat.
    ctx.sled_upright = false;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::None);
    REQUIRE(st.mode == app::PlayerMode::Sled);
    // On its skis: Sled -> Drone.
    ctx.sled_upright = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::DeployDrone);
    REQUIRE(st.mode == app::PlayerMode::Drone);
    REQUIRE(st.drone_home == app::PlayerMode::Sled);
    // The flight's end: back on the machine, not on foot.
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::EndDrone);
    REQUIRE(st.mode == app::PlayerMode::Sled);
    // And a launch from the feet still comes home to the feet.
    st.mode = app::PlayerMode::Afoot;
    ctx.man_upright = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::DeployDrone);
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::DroneKey, ctx) ==
            app::ModeAction::EndDrone);
    REQUIRE(st.mode == app::PlayerMode::Afoot);
}

TEST_CASE("sting mode: J does not yank a pilot off his flying drone") {
    app::PlayerModeState st;
    st.mode = app::PlayerMode::Drone;
    st.sled_seeded = true;
    app::ModeContext ctx;
    ctx.sled_seeded = true;
    ctx.sled_in_reach = true;  // the machine is right there --
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::MountKey, ctx) ==
            app::ModeAction::None);  // -- and J still refuses
    REQUIRE(st.mode == app::PlayerMode::Drone);
    // The pump and gun keys refuse too (a flying drone is a job in progress).
    ctx.pump_in_reach = true;
    ctx.man_upright = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::InteractKey,
                                        ctx) == app::ModeAction::None);
    ctx.gun_in_reach = true;
    REQUIRE(app::player_mode_transition(st, app::ModeEvent::GunKey, ctx) ==
            app::ModeAction::None);
    REQUIRE(st.mode == app::PlayerMode::Drone);
}

// ===========================================================================
// The launch: off the stock on the aim, the counter paid, the clock full.
// MUTATION: forget the counter re-seat in sting_launch_counted -> the left
// check fails (a fresh flight would refill the magazine to 3).
// ===========================================================================
TEST_CASE("sting launch: on the aim, counter paid, battery full") {
    app::StingParams sp;
    app::StingState st;
    st.left = sp.per_match;  // fly-4: the loadout is 5, one dial
    app::sting_launch_counted(st, kSite, kAim, sp);

    REQUIRE(st.active);
    REQUIRE(st.left == sp.per_match - 1);
    REQUIRE(sp.per_match == 5);         // Chad's fly-4 loadout ruling
    REQUIRE(sp.battery_s == 60.0);      // fly-5: "only a 1 min battery"
    REQUIRE(sp.turns_max == 0);         // the turn destruct is OFF
    REQUIRE(st.battery_s == Catch::Approx(sp.battery_s));
    REQUIRE(st.turns_used == 0);
    REQUIRE(st.end == app::StingEnd::None);
    // Velocity is launch_speed along the aim.
    REQUIRE(glm::length(st.curr.velocity) == Catch::Approx(sp.launch_speed));
    REQUIRE(glm::dot(glm::normalize(st.curr.velocity), kAim) ==
            Catch::Approx(1.0));
    // Nose (-Z body) on the aim.
    const glm::dvec3 nose =
        st.curr.orientation * glm::dvec3(0.0, 0.0, -1.0);
    REQUIRE(glm::dot(nose, kAim) == Catch::Approx(1.0).margin(1e-9));
    // Above the man, not inside him.
    REQUIRE(glm::length(st.curr.position - kSite) > 1.0);
}

// ===========================================================================
// The battery clock ends the flight — flown through the REAL plant + cascade.
// MUTATION: drop the battery decrement -> this leg spins forever red at the
// tick-budget assert; end reason mislabeled -> the enum check fails.
// ===========================================================================
TEST_CASE("sting flight: the battery clock ends the flight, and it flew") {
    app::StingParams sp;
    sp.battery_s = 2.0;  // a short flight is still a flight
    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);
    app::StingCmd cmd;
    cmd.speed_cmd = sp.speed_cruise;

    app::StingEnd end = app::StingEnd::None;
    int ticks = 0;
    const int budget = static_cast<int>(sp.battery_s / kAp.sim_dt) + 10;
    while (st.active && ticks < budget) {
        end = app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
        ++ticks;
    }
    REQUIRE(end == app::StingEnd::Battery);
    REQUIRE_FALSE(st.active);
    REQUIRE(st.end == app::StingEnd::Battery);
    // It genuinely flew: forward along the aim, state finite, throttle-driven
    // speed at or above the launch toss.
    REQUIRE(std::isfinite(st.curr.position.x));
    REQUIRE(glm::dot(st.curr.position - kSite, kAim) > 20.0);
    REQUIRE(glm::length(st.curr.velocity) > sp.launch_speed * 0.8);
}

// ===========================================================================
// The detonate command is one-shot and immediate.
// MUTATION: fly the tick before honoring the press -> the position-moved
// check fails; forget to consume -> the second flight would die on launch.
// ===========================================================================
TEST_CASE("sting flight: P detonates now, and the press is consumed") {
    app::StingParams sp;
    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);
    app::StingCmd cmd;
    cmd.detonate = true;

    const glm::dvec3 before = st.curr.position;
    const app::StingEnd end =
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(end == app::StingEnd::Manual);
    REQUIRE_FALSE(st.active);
    REQUIRE_FALSE(cmd.detonate);  // consumed
    REQUIRE(st.curr.position == before);  // it did not fly one more tick
}

// ===========================================================================
// The two-large-turns law, driven with a synthetic n sequence.
// MUTATION: count ticks above threshold instead of latched events -> the
// "one long turn is ONE turn" check fails; drop the release hysteresis ->
// the "re-load without unloading is still the same turn" check fails.
// ===========================================================================
TEST_CASE("sting turns: a large turn is one hysteretic event, two arm it") {
    app::StingParams sp;  // fly-2 band: turn_g 6.0, dwell 0.7, release 3.0
    // fly-4 RULED the destruct OFF by default (turns_max 0) — the latch
    // machinery lives on behind the dial; this leg turns the dial to
    // exercise it.
    sp.turns_max = 2;
    app::StingState st;
    st.active = true;
    const double dt = 1.0 / 120.0;

    // A hard 8 g reversal held 2 full seconds: exactly ONE turn. (Kept clear
    // of the exact 7.0 boundary: |n-1| == turn_g is a STRICT >, by design.)
    for (int i = 0; i < 240; ++i) app::sting_turn_tick(st, sp, 8.0, dt);
    REQUIRE(st.turns_used == 1);
    REQUIRE(st.warn_s < 0.0);  // one turn does not arm anything

    // Load eases to 4 g of turn (below turn_g, ABOVE release): same turn.
    for (int i = 0; i < 120; ++i) app::sting_turn_tick(st, sp, 5.0, dt);
    REQUIRE(st.turns_used == 1);

    // Unload to level, then a second hard reversal: the second event...
    for (int i = 0; i < 60; ++i) app::sting_turn_tick(st, sp, 1.0, dt);
    for (int i = 0; i < 120; ++i) app::sting_turn_tick(st, sp, 8.0, dt);
    REQUIRE(st.turns_used == 2);
    // ...arms the destruct warble.
    REQUIRE(st.warn_s == Catch::Approx(sp.warn_s));

    // A brief 0.3 s spike never reaches the dwell: no third turn.
    app::StingState st2;
    st2.active = true;
    for (int i = 0; i < 36; ++i) app::sting_turn_tick(st2, sp, 8.0, dt);
    for (int i = 0; i < 36; ++i) app::sting_turn_tick(st2, sp, 1.0, dt);
    REQUIRE(st2.turns_used == 0);
    // Negative g counts by magnitude: a -7 g bunt (|n-1| = 8) loads.
    for (int i = 0; i < 120; ++i) app::sting_turn_tick(st2, sp, -7.0, dt);
    REQUIRE(st2.turns_used == 1);

    // ★ fly-4 DEFAULT: the destruct is OFF. Shipped params, three latched
    // reversals — turns COUNT, nothing ever arms.
    app::StingParams sp_ship;  // turns_max 0
    app::StingState st3;
    st3.active = true;
    for (int r = 0; r < 3; ++r) {
        for (int i = 0; i < 240; ++i) app::sting_turn_tick(st3, sp_ship, 8.0, dt);
        for (int i = 0; i < 60; ++i) app::sting_turn_tick(st3, sp_ship, 1.0, dt);
    }
    REQUIRE(st3.turns_used == 3);
    REQUIRE(st3.warn_s < 0.0);
}

// ===========================================================================
// The ram: the drone's tick segment AS a one-round pool through the ordinary
// combat_tick — kill, credit, retire, all the existing law.
// MUTATION: skip sting_ram_round's damage/v_ref seeding -> the one-shot
// check fails (ke_damage of a 0-reference round is 0).
// ===========================================================================
TEST_CASE("sting ram: one pass one-shots a bandit through combat_tick") {
    app::StingParams sp;
    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);

    // A bandit parked dead ahead; the sting's tick segment passes through it.
    drone::DroneParams dp;
    std::vector<drone::DroneState> drones(1);
    drones[0].curr.position = st.curr.position + kAim * 50.0;
    drones[0].curr.velocity = glm::dvec3(0.0);
    drones[0].prev = drones[0].curr;
    drones[0].hp = dp.hp;
    drones[0].spawn_index = 0;

    // Hand-advance the segment through the target (the flight itself is the
    // battery leg's business; this leg is the sweep's).
    st.prev = st.curr;
    st.curr.position += kAim * 60.0;
    st.curr.velocity = kAim * 120.0;

    combat::CombatWorld cw;
    cw.params.hit_radius_m = dp.hit_radius_m;
    std::vector<weapon::Projectile> pool;
    app::sting_ram_round(st, sp, pool);
    REQUIRE(pool.size() == 1);
    combat::combat_tick(pool, drones, cw, kAp, dp, kAp.sim_dt, sp.prox_add_m,
                        /*burst_fx=*/true, /*kill_sink=*/nullptr,
                        /*clear_kills=*/false);

    REQUIRE_FALSE(pool[0].active);  // the detonation
    REQUIRE(cw.kills == 1);         // the player's kill, booked as his own
    REQUIRE(cw.killed_spawn_indices.size() == 1);
}

// ===========================================================================
// The cascade chases the carried aim (Chad's fly ruling: "mouse aim circle
// and it have a cascade"). Launch, sweep the mouse right once, fly on: the
// aim moves off the launch line and the NOSE converges onto the aim — the
// plane's own §9.2 contract, running on the drone.
// MUTATION: feed the cascade the raw launch aim instead of the carried
// frame's forward -> the convergence-onto-the-MOVED-aim check fails.
// ===========================================================================
TEST_CASE("sting cascade: the nose chases the mouse-carried aim circle") {
    app::StingParams sp;
    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);
    app::StingCmd cmd;
    cmd.speed_cmd = sp.speed_cruise;

    // One firm mouse sweep right on the first tick, then hands off.
    cmd.dx = 400.0;
    const int ticks = static_cast<int>(4.0 / kAp.sim_dt);
    for (int i = 0; i < ticks && st.active; ++i)
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);

    REQUIRE(st.active);  // 4 s is well inside every life clock
    const glm::dvec3 aim_now = st.aim.forward();
    // The aim genuinely moved off the launch line...
    REQUIRE(glm::dot(aim_now, kAim) < 0.999);
    // ...to the RIGHT (+x at this site: fwd -z, up +y — the frame's own
    // carried right, the plane's non-inverted convention)...
    REQUIRE(aim_now.x > 0.01);
    // ...and the cascade flew the nose onto it.
    const glm::dvec3 nose =
        st.curr.orientation * glm::dvec3(0.0, 0.0, -1.0);
    REQUIRE(glm::dot(nose, aim_now) > 0.95);
}

// ===========================================================================
// The fly-2 band: the derived airframe actually reaches it. The plane's own
// 18 kN tops out ~245 m/s — a Sting commanded to 450 must blow past that.
// MUTATION: drop the T_max scale in sting_aircraft_params -> the 300 m/s
// check fails (the unscaled airframe cannot).
// ===========================================================================
TEST_CASE("sting airframe: W-held flight blows past the plane's ceiling") {
    REQUIRE(kSap.T_max == Catch::Approx(kAp.T_max * 6.0));
    REQUIRE(kSap.v_redline > kAp.v_redline * 1.5);
    app::StingParams sp;
    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);
    app::StingCmd cmd;
    cmd.speed_cmd = sp.speed_max;  // W held: 450 commanded
    const int ticks = static_cast<int>(20.0 / kAp.sim_dt);
    for (int i = 0; i < ticks && st.active; ++i)
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(st.active);  // straight, level, well inside every life clock
    REQUIRE(glm::length(st.curr.velocity) > 300.0);

    // ★ fly-4 ("can the plane slow fairly fast?"): drop the command to the
    // line-up speed — the SIGNED throttle law cuts power and the flaps
    // deploy as speed brakes, so it sheds >100 m/s in 10 s.
    // MUTATION: restore the old max(0,·) unsigned error -> throttle idles at
    // 40% while "slowing" and this deceleration check fails.
    const double v_fast = glm::length(st.curr.velocity);
    cmd.speed_cmd = sp.speed_min;
    const int ticks_slow = static_cast<int>(10.0 / kAp.sim_dt);
    for (int i = 0; i < ticks_slow && st.active; ++i)
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(st.active);
    REQUIRE(glm::length(st.curr.velocity) < v_fast - 100.0);
}

// ===========================================================================
// ★★★ THE ST-5 BAND BOTTOM. Chad, verbatim: "I would like to have a lower
// bottom end, right now it only goes as slow as 250 m/s, but by pressing s, I
// should hear it go down to a lower frequency and volume and see the blades
// seemingly spinning slower and it shall go down to 150 m/s as a bottom end."
//
// Two things to pin, because the ruling has two halves. The FLOOR itself is
// the number he gave, and the setpoint clamp must honour it (a leftover 250
// in the clamp would silently hold the drone 100 m/s above what he asked
// for). And the drone must actually GET there on S — the fly-4 leg above only
// proves it sheds 100 m/s, which a 250 floor also satisfies.
// MUTATION: put speed_min back to 250 -> the floor check fails immediately
// and the settled speed lands ~100 m/s high.
// ===========================================================================
TEST_CASE("sting band: S held settles at the ruled 150 m/s bottom") {
    app::StingParams sp;
    REQUIRE(sp.speed_min == Catch::Approx(150.0));
    REQUIRE(sp.speed_max == Catch::Approx(450.0));

    app::StingState st;
    st.left = 1;
    app::sting_launch_counted(st, kSite, kAim, sp);
    app::StingCmd cmd;
    // Up to the chase speed first, then S held for long enough that the
    // autothrottle + speed-brake flaps have settled rather than merely
    // started falling.
    cmd.speed_cmd = sp.speed_max;
    for (int i = 0; i < static_cast<int>(15.0 / kAp.sim_dt) && st.active; ++i)
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(st.active);
    cmd.speed_cmd = sp.speed_min;
    for (int i = 0; i < static_cast<int>(25.0 / kAp.sim_dt) && st.active; ++i)
        app::sting_step(st, cmd, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(st.active);
    // The autothrottle is a proportional hold, so it settles NEAR the
    // setpoint, not on it; what matters for the ruling is that the drone is
    // now living down at the new floor and nowhere near the old one.
    const double v_slow = glm::length(st.curr.velocity);
    REQUIRE(v_slow < 200.0);   // well below the retired 250 floor
    REQUIRE(v_slow > 100.0);   // and it is flying, not falling out of the sky

    // The clamp must not let a command below the floor through either — the
    // band is the band in both directions.
    app::StingCmd under;
    under.speed_cmd = 40.0;
    for (int i = 0; i < static_cast<int>(10.0 / kAp.sim_dt) && st.active; ++i)
        app::sting_step(st, under, sp, kSap, kCp, nullptr, kAp.sim_dt);
    REQUIRE(st.active);
    REQUIRE(glm::length(st.curr.velocity) > 100.0);
}

// ===========================================================================
// ★★★ THE ST-5 PROP LADDER. Chad, verbatim: "I would like the blades to
// appear to move even faster at the top end and sound even faster (but not go
// faster)".
//
// "But not go faster" is the load-bearing clause: the TRUE spin law and the
// airframe are untouched, and only the DRAWN rate and the VOICE move. These
// legs pin exactly that split, plus the wagon-wheel margin the drawn cap
// exists to protect (a later "faster still" must fail here, not in his eye).
// MUTATION: put kStingSpinApparentMax back to 25 -> the top-end spread check
// fails (pinned draws no faster than 25) while idle still passes, which is
// precisely the complaint he filed.
// ===========================================================================
TEST_CASE("sting props: the drawn ladder widens without aliasing") {
    // The true law is untouched — 150 m/s of band bottom is 40 rad/s, pinned
    // is 150 rad/s, exactly as signed.
    REQUIRE(render::sting_spin_rate(0.0) == Catch::Approx(40.0));
    REQUIRE(render::sting_spin_rate(1.0) == Catch::Approx(150.0));

    // Idle stays 1:1 — "very nice with the animation" was about this half.
    REQUIRE(render::sting_apparent_spin_rate(10.0) == Catch::Approx(10.0));
    REQUIRE(render::sting_apparent_spin_rate(18.0) == Catch::Approx(18.0));

    const double lo = render::sting_apparent_spin_rate(
        render::sting_spin_rate(0.0));  // the loiter
    const double hi = render::sting_apparent_spin_rate(
        render::sting_spin_rate(1.0));  // pinned
    REQUIRE(hi > lo);
    // The complaint in one number: the pinned prop must draw at least half
    // again as fast as the loitering one. The old ladder managed 1.21x.
    REQUIRE(hi / lo > 1.5);
    // The compressed ramp lands ON the cap at the pinned rate — nothing at
    // the top of the throttle is thrown away.
    REQUIRE(hi == Catch::Approx(render::kStingSpinApparentMax).margin(0.05));

    // Monotone everywhere, and never past the cap however it is driven (the
    // smoke rig pins the true rate straight from an env var).
    double prev = -1.0;
    for (int i = 0; i <= 400; ++i) {
        const double r = render::sting_apparent_spin_rate(i * 2.0);
        REQUIRE(r >= prev);
        REQUIRE(r <= render::kStingSpinApparentMax + 1e-9);
        prev = r;
    }
    // The wagon wheel, stated: 45 deg of blade travel per frame at 60 fps is
    // the readable limit for a two-blade prop, and the cap keeps >25% under.
    REQUIRE(render::kStingSpinApparentMax <
            0.75 * render::kStingAliasWorkingRadS);
}

// ===========================================================================
// ★★★ THE ST-5 VOICE. Chad: "hear it go down to a lower frequency and volume"
// at the bottom, "sound even faster" at the top. Pitch AND level must both be
// monotone in the throttle and the two ends must be far enough apart that the
// S key is an event rather than a nuance.
// MUTATION: restore kStingIdleGain 0.34 -> the >2.5x level spread fails.
// ===========================================================================
TEST_CASE("sting voice: pitch and level span the widened band") {
    const double f_lo = render::sting_blade_hz(0.0);
    const double f_hi = render::sting_blade_hz(1.0);
    const double g_lo = render::sting_throttle_gain(0.0);
    const double g_hi = render::sting_throttle_gain(1.0);

    // The loiter sits LOW — under 100 Hz of blade pass, in the chest.
    REQUIRE(f_lo < 100.0);
    // ...and the pinned end higher than the round he complained about (300).
    REQUIRE(f_hi > 340.0);
    REQUIRE(g_hi / g_lo > 2.5);

    double pf = -1.0, pg = -1.0, pc = -1.0;
    for (int i = 0; i <= 100; ++i) {
        const double t = i * 0.01;
        const double f = render::sting_blade_hz(t);
        const double g = render::sting_throttle_gain(t);
        const double c = render::sting_cutoff(t);
        REQUIRE(f > pf);   // strictly rising pitch
        REQUIRE(g > pg);   // strictly rising level
        REQUIRE(c > pc);   // and the timbre opens with them
        pf = f; pg = g; pc = c;
    }
    // The brightness lowpass must sit ABOVE the whole harmonic stack at the
    // top, or it would be clipping the series off rather than shaping it.
    REQUIRE(render::sting_cutoff(1.0) >
            f_hi * static_cast<double>(render::kStingHarmonics));
    // Level discipline unchanged: the channel is still worth less on the bus
    // than the snowmachine you are sitting on, and pre-clip peak is linear.
    REQUIRE(render::kStingFlownLevel < 0.31);
    REQUIRE(g_hi * render::kStingMaster < 1.0);
}
