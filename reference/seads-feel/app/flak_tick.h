#pragma once
// FLAK F-FIRE -- the manned Oerlikon's per-tick step (docs/FLAK_GUN_SPEC.md
// §7.4).
//
// PURE (glm + std + the pure weapon/render-core headers; no raylib, no clock,
// no getenv) so every leg of the firing loop -- the slew, the drum, the
// reload, the self-destruct, the shooter frame -- is pinned HEADLESSLY in
// test/unit/test_flak_gun.cpp without the app fixture. app::tick calls
// flak_tick() behind a defaulted-nullptr FlakWorld* (absent => not one line
// runs => bit-identical by construction; every write in here lands on
// fk-owned state only).
//
// THE FIREWALL, restated: nothing here reads or writes sim/ or control/
// state -- the shooter is a SYNTHETIC sim::SimState built from the mount
// frame (weapon::fire_tick is READ-ONLY on its shooter, so a fake state is
// safe; the codebase-survey consult's recipe). The flak pool is NEVER passed
// to combat::conquest_tick -- the gun must not shoot its own pump.
//
// THE INPUT SHAPE (the aim discipline, transposed): the DEMAND pose is set
// per-frame from raw mouse deltas (main.cpp), unsmoothed -- the §9.1 rule.
// The GUN carries the only lag: slew_toward's rate cap, which is the gun's
// MASS, not a filter on the hand. Rates are feel dials (flak_gun.h).

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/flak_gun.h"
#include "sim/state.h"
#include "weapon/ballistics.h"

namespace app {

// One round that DIED VISIBLY inside flak_tick, recorded for the frame side
// to turn into a combat::Fx. This header must not include combat/kill.h and
// must not touch an FxPool -- it is the PURE, headless-tested half, and an FX
// pool is app/render state. So the tick BOOKS the event (the spawned_accum
// precedent, one level up: an int there, a position here) and the caller
// drains it; every write still lands on fk-owned state.
//
// Only the SELF-DESTRUCT retire is booked here. The proximity/direct-hit
// retire happens inside combat::combat_tick, which already owns the FxPool
// and already spawns FX at exactly that point -- routing it through here too
// would be a second seam for one event, and two seams for one event is how a
// puff goes missing.
enum class FlakBurstCause {
    SelfDestruct,  // age >= selfdestruct_s: the curtain / ranging cue
};

struct FlakBurst {
    glm::dvec3 pos{0.0};  // world position of the round on its LAST live tick
    FlakBurstCause cause = FlakBurstCause::SelfDestruct;
};

struct FlakWorld {
    bool manned = false;
    int gun = -1;  // index into the app's placed guns (-1 = none)
    render::flak::MountFrame mount{};   // of the manned gun
    // ARRIVES, NEVER TYPED: the station table read off the shipped gun GLB
    // (main.cpp sets it when the model is available). Left zero -- the
    // headless fixtures that type a muzzle_body directly -- the shooter
    // rebuild leaves the caller's offset alone. See flak_rebuild_shooter.
    render::flak::Stations stations{};
    render::flak::Pose pose{};          // the GUN (rate-capped)
    render::flak::Pose demand{};        // the HAND (raw, per-frame)
    render::flak::SlewRates rates{};
    weapon::GunWorld gw{};              // its OWN pool + cooldown
    // Drum + reload (config [guns] flak_*; OP 911's rhythm: one 60-round
    // drum ~ one strafing pass, the reload is the breath between passes).
    int drum_rounds = 60;
    int rounds_left = 60;
    double reload_s = 4.0;
    double reload_left_s = 0.0;
    double selfdestruct_s = 1.6;        // [s] the flak-curtain read (spec §2.2)
    // [m] proximity-fuze radius ADD, handed to combat::combat_tick for this
    // pool only (spec §2.2 item 5 / red-team P0-1: a ring sight cannot solve
    // a crossing lead to the +-2.5% a direct hit needs, so the shell bursts
    // NEAR the aeroplane). A FEEL DIAL -- [guns] flak_prox_radius_m. 0 = the
    // bare 20 mm, direct hits only.
    double prox_radius_m = 4.0;
    bool fire_held = false;             // main mirrors the LMB while manned
    int spawned_accum = 0;              // spawns since main last drained it
                                        //   (audio triggers, one per shot)
    // ★ MONOTONE TOTAL, NEVER DRAINED (Stage C). spawned_accum is only
    // meaningful to a reader that a DRAINER keeps honest, and the only
    // drainer is main.cpp's audio block -- which is skipped entirely when
    // audio fails to open (every --smoke run). An ENVELOPE reader survives
    // that (a stuck count just saturates an exponential), but a DISCRETE
    // reader does not: the spent-brass spawner re-spawned the whole running
    // total every frame and booked 3.1 MILLION cases in a 12 k-frame run.
    // So discrete cosmetic readers difference THIS instead, and cannot be
    // broken by whether anybody else drains anything. Bookkeeping only --
    // no timing, no ballistics, no reload behaviour reads it.
    long long spawned_total = 0;
    // ROUND 5 (the burst boom): monotone count of self-destruct bursts, the
    // spawned_total pattern restated -- main.cpp's audio block DIFFERENCES
    // this to fire one distant crump per curtain pop, and a discrete reader
    // of a drained vector (bursts, below) would be broken by whoever drains
    // it first. Never drained, bookkeeping only.
    long long burst_total = 0;
    std::vector<FlakBurst> bursts;      // deaths since the caller last drained
                                        //   (puff FX, one per burst)
    sim::SimState shooter{};            // rebuilt every tick from mount+pose
};

// The synthetic shooter: position at the MOUNT BASE, orientation such that
// BODY -Z (the nose, SPEC §7) is the bore and BODY +Y is the pose-carried up,
// velocity ZERO (a gun does not inherit the planet's spin -- rounds fly in
// the world frame like everything else). The muzzle offset is carried in the
// battery's muzzle_body (BODY coords), and it is REBUILT HERE every tick off
// the station table through the cradle kinematics, so fire_tick's
// position + orientation * muzzle_body lands exactly on the drawn muzzle at
// EVERY elevation -- see the banner on that block below.
inline void flak_rebuild_shooter(FlakWorld& fk) {
    using namespace render::flak;
    const glm::dvec3 bore = bore_dir_world(fk.mount, fk.pose);
    const glm::dvec3 up_m = glm::normalize(model_dir_to_world(
        fk.mount, train_quat(fk.pose.train_rad) *
                      (cradle_quat(fk.pose.elev_rad) * glm::dvec3(0, 1, 0))));
    glm::dmat3 B;
    B[2] = -bore;                       // body +Z = aft
    B[1] = up_m;                        // body +Y = up
    B[0] = glm::cross(B[1], B[2]);      // body +X = right (RH)
    fk.shooter.position = fk.mount.pos;
    fk.shooter.orientation = glm::quat_cast(B);
    fk.shooter.velocity = glm::dvec3(0.0);
    // ★★★ THE MUZZLE IS A CRADLE STATION, NOT A RIGID OFFSET FROM THE
    // BASE. Elevation rotates the gun about the TRUNNION (1.543 m up the
    // pedestal), so a muzzle_body measured once at zero elevation and then
    // carried by the shooter's orientation rotates st_muzzle about the mount
    // ORIGIN instead -- which drags the spawn point AFT and DOWN as the gun
    // rises (at +45 deg: 1.09 m behind the bell and 0.45 m under it, i.e.
    // out of the breech, a metre from the gunner's own eye). That is exactly
    // what Chad reported on 2026-08-31: "the tracer appears to come out of
    // the sudburians view, rather than the end of the barrel". So the offset
    // is rebuilt EVERY TICK from the same cradle_station_world() the muzzle
    // FLASH already draws through (render/draw.cpp) -- ONE kinematic source
    // for the metal, the flash and the round, so they can never drift apart.
    // B is orthonormal (up_m and bore are the cradle's +Y and +Z), so the
    // transpose is the inverse: world delta -> BODY coords, which is the
    // frame muzzle_body is declared in.
    if (fk.stations.muzzle != glm::dvec3(0.0) && !fk.gw.battery.guns.empty()) {
        const glm::dvec3 mw = cradle_station_world(fk.mount, fk.stations,
                                                   fk.pose, fk.stations.muzzle);
        fk.gw.battery.guns[0].muzzle_body =
            glm::transpose(B) * (mw - fk.shooter.position);
    }
}

// One fixed tick. Returns the number of rounds spawned THIS tick (also
// accumulated on fk.spawned_accum for the frame-side audio drain).
inline int flak_tick(FlakWorld& fk, bool grounded_tick, double dt, double g,
                     double ground_radius,
                     const world::TunnelNet* net = nullptr) {
    using namespace render::flak;
    // The gun slews toward the hand even while the pool is empty -- walking
    // away mid-slew must not freeze the barrel mid-air next mount.
    if (fk.manned) fk.pose = slew_toward(fk.pose, fk.demand, fk.rates, dt);
    // Reload timer runs whether or not he is on the gun.
    if (fk.reload_left_s > 0.0) {
        fk.reload_left_s -= dt;
        if (fk.reload_left_s <= 0.0) {
            fk.reload_left_s = 0.0;
            fk.rounds_left = fk.drum_rounds;
        }
    }
    flak_rebuild_shooter(fk);
    const bool firing = fk.manned && fk.fire_held && !grounded_tick &&
                        fk.rounds_left > 0 && fk.reload_left_s <= 0.0;
    weapon::fire_tick(fk.gw, fk.shooter, firing, dt, g, ground_radius, net);
    // Fresh spawns are the age == 0.0 actives: fire_tick advances the live
    // pool FIRST (each advance adds dt to age), THEN spawns -- so exactly
    // this tick's rounds sit at 0.0 (ballistics.h's documented order; the
    // test pins it so a reorder goes red here, not silently).
    int spawned = 0;
    for (const weapon::Projectile& p : fk.gw.pool)
        if (p.active && p.age == 0.0) ++spawned;
    if (spawned > 0) {
        fk.rounds_left -= spawned;
        if (fk.rounds_left <= 0) {
            fk.rounds_left = 0;
            fk.reload_left_s = fk.reload_s;  // the drum swap begins
        }
        fk.spawned_accum += spawned;
        fk.spawned_total += spawned;  // Stage C: monotone, never drained
    }
    // Self-destruct: the 20 mm reads as FLAK because the tracer dies in a
    // flash at ~selfdestruct_s (~870 m) -- a curtain that is also a ranging
    // cue (spec §2.2 item 5). The retire is the mechanism; the BURST is the
    // read, booked here at p.pos -- the position the round reached on this,
    // its last live tick, which is where the eye was already tracking the
    // tracer. (NOT prev_pos: that is a whole tick -- ~7 m at 835 m/s --
    // behind the streak the player is watching.)
    if (fk.selfdestruct_s > 0.0)
        for (weapon::Projectile& p : fk.gw.pool)
            if (p.active && p.age >= fk.selfdestruct_s) {
                p.active = false;
                fk.bursts.push_back({p.pos, FlakBurstCause::SelfDestruct});
                ++fk.burst_total;  // ROUND 5: monotone, never drained
            }
    return spawned;
}

}  // namespace app
