#pragma once
// ★★★ L3 — WHERE THE PLAYER IS BORN
// (docs/PLAN_20260901_game_loop_millwright.md §4.3).
//
// Chad's ask, verbatim: "knowing from flying it's almost always death spawning
// in where we do, so the ability to fix a pump should be allowed through
// respawn straight into the snowmachine from a set distance away from the
// pump."
//
// ★ ONE ROUTING FUNCTION, THREE CALL SITES. Before this file there were THREE
// unrelated births -- the initial spawn in `app/main.cpp`, the crash respawn
// and the component-death respawn in `app/instructor_tick.h` -- each a bare
// `spawn_state(...)` call, and the two in the tick were a literal copy-paste
// pair. Adding a second KIND of birth to three copies is how one policy becomes
// four. `player_spawn` is now the only place a player is placed, and
// `PlayerSpawnChoice::Aircraft` through it is BIT-IDENTICAL to the old call by
// construction (it IS the old call -- see the Aircraft branch, and the identity
// leg in test/unit/test_spawn_policy.cpp).
//
// ★★★ `spawn_state` ITSELF MOVED HERE, unchanged to the character, from
// `app/instructor_tick.h`. It is the aircraft half of the policy and it now
// lives beside the other half; `app/instructor_tick.h` includes this header, so
// every existing `app::spawn_state` caller and test is untouched.
//
// ⚠ AND THIS HEADER OWNS NO GROUND. Every position below is either a pure
// direction on the unit sphere or a world point the CALLER produced by sampling
// a surface it owns (`snow_field.drive_radius_at` for the machine,
// `env.ground->radius_at` for the aeroplane). That is not tidiness: a
// `sample_at` fired inside the sled tape's `sample_tap` window makes every tape
// of that drive unreplayable (the R4c finding, app/main.cpp's dismount block),
// so the ground query has to be somewhere the tap is provably not armed -- and
// a header that COULD sample would eventually be called from somewhere it is.
//
// ⚠ NO RAYLIB HERE. The menu that ASKS the question is `app/spawn_menu.h`; this
// file only knows the answer's consequences, which is what lets the whole
// policy be tested with no window (`seads_tests` bans raylib in its include
// closure -- tools/graph/layer_rules.toml).

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "combat/conquest.h"
#include "world/tunnel_geo.h"  // the two mouths: Errington (Valley) / Murray (Sudbury)
#include "sim/params.h"
#include "sim/sled.h"
#include "sim/state.h"

namespace app {

// ★ THE SURFACE PUMP'S MAST. The pump's LOGICAL point (combat::Pump::pos --
// what the raiders fly at, what the map marks) sits this far above the
// terrain under it (main.cpp's surf() lambda: radius_at + mast). Its FOOT is
// the terrain point; the man walks to the foot, the reach law is measured
// along the ground to the foot, and (Chad 2026-09-06: "the location is still
// too high above me, it needs to come down") the pump BODY is drawn standing
// on the foot, not floating at the logical point. ONE number, read by the
// placement, the draw fill and the probe rig -- never a second literal.
inline constexpr double kSurfacePumpMastM = 10.0;

// ---------------------------------------------------------------------------
// ★★★ THE SIDE'S HOME — WHERE A PLAYER OF THAT FACTION IS BORN.
// Chad flew the first cut of the Central City rung and reported: "I picked
// central city but my spawn was over the valley".
// ---------------------------------------------------------------------------
//
// THE DEFECT, NAMED. `app/main.cpp` set the spawn anchor ONCE, with no faction
// term anywhere in it: `loop.spawn_up = world::kTunnelMouthErrington`, nose
// projected toward `kTunnelMouthMurray`. One home for both sides -- correct
// while there was only ever one playable side, and a lie the moment there were
// two. Every birth reads that one spec: the initial spawn, the crash respawn
// and the component-death respawn all go through LoopState::spawn_up/fwd.
//
// ⚠ NO SITE IS INVENTED HERE. Both mouths are already baked, named and
// geolocated in world/tunnel_geo.h, and the sides are already named after
// them: Errington Mine #3 is "HOME, the Chelmsford coalition's dive-in" (the
// VALLEY), and combat/conquest.h's own faction banner calls faction 1
// "SUDBURY (the Murray side)". So the Valley's home is Errington and Central
// City's is the Murray pit, by the codebase's own naming, not by a choice made
// in this header.
//
// ⚠ VALLEY IS BIT-IDENTICAL. faction 0 returns exactly the two vectors
// main.cpp typed before -- same constant, same projection, same order of
// operations -- so the shipped spawn is unchanged to the last bit. Central
// City is the MIRROR: born over Murray, nose toward Errington, so each side
// spawns at home looking at the other side's home ("the raid is dead ahead",
// Chad's 2026-07-18 ruling, now true for both).
//
// Pure: unit directions only, no ground sampling (the header's own banner
// above -- a sample here would be unreplayable inside the sled tape's tap).
inline glm::dvec3 faction_home_dir(int faction) {
    return (faction == combat::CQ_SUDBURY) ? world::kTunnelMouthMurray
                                           : world::kTunnelMouthErrington;
}

// The nose: the OTHER side's home, projected tangent to the sphere at this
// side's home. Same expression main.cpp carried, with the two mouths supplied
// by the function above instead of typed in.
inline glm::dvec3 faction_home_fwd(int faction) {
    const glm::dvec3 up = faction_home_dir(faction);
    const glm::dvec3 tgt =
        faction_home_dir(faction == combat::CQ_SUDBURY ? combat::CQ_VALLEY
                                                       : combat::CQ_SUDBURY);
    return glm::normalize(tgt - glm::dot(tgt, up) * up);
}

// Spawn / crash-respawn state (SPEC §6.3: respawn airborne AT speed, born at
// cruise power — not a 0.5 s spool from idle). Shared by main.cpp and the
// tick's crash branch so the shipped respawn data IS what the test asserts
// against. Level at `alt_m` over `up_dir` (radius R + alt_m), 140 m/s along
// the nose, nose = the component of `fwd_dir` tangent to the sphere at the
// spawn point. The DEFAULT args reproduce the legacy +X-pole/−Z-nose spawn
// BIT-identically (every default input is exact and the orthogonalization is
// a no-op there — pinned in test_tunnel.cpp), so every existing caller/test
// is untouched. All inputs are plain parameters — the caller-side spawn spec
// (main.cpp / LoopState) feeds them, never an env read here, so the
// crash-branch respawn on the tested tick path stays deterministic/env-free.
inline sim::SimState spawn_state(const sim::AircraftParams& p,
                                 double alt_m = 2000.0,
                                 const glm::dvec3& up_dir = {1.0, 0.0, 0.0},
                                 const glm::dvec3& fwd_dir = {0.0, 0.0, -1.0}) {
    sim::SimState s;
    const glm::dvec3 up = glm::normalize(up_dir);
    const glm::dvec3 fwd = glm::normalize(fwd_dir - glm::dot(fwd_dir, up) * up);
    s.position = up * (p.R + alt_m);
    const glm::dmat3 m{glm::cross(fwd, up),  // body X (right)
                       up,                   // body Y (up) = local up
                       -fwd};                // body Z (nose = -Z -> fwd)
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = 140.0 * fwd;
    s.last_vhat = fwd;
    s.throttle = 1.0;
    return s;
}

// ---------------------------------------------------------------------------
// The choice itself.

enum class PlayerSpawnChoice : int {
    Aircraft = 0,  // everything the game did before this rung
    Snowmachine,   // seated on the machine, near the pump that wants a wrench
};

// The dials (config/game.toml [spawn]). app/ invents no distance of its own.
struct SpawnDials {
    // How far from the pump the machine is set down, along the SURFACE.
    double sled_dist_m = 300.0;
    // How far to the machine's RIGHT the aeroplane is parked. The same 5 m the
    // KEY_J mount seed leaves between the two bodies, for the same reason:
    // putting one at the other's CG puts it through the fuselage.
    double aircraft_beside_m = 5.0;
};

// ---------------------------------------------------------------------------
// Sphere geometry. Pure, and on the GREAT CIRCLE -- never a tangent-plane
// offset. Over 300 m on a 15 km ball chord-versus-arc is 0.05 m, which is
// nothing; 300 m of TANGENT leaves the sphere by 3.0 m, which is a machine
// hovering three metres above the snow. (The dismount block's own lesson --
// "a tangential step on a 15 km ball leaves the sphere" -- at 300x the step.)

// Unit tangent at `at_dir` pointing along the great circle toward `to_dir`.
// Zero when the two are the same point or antipodal: the bearing is genuinely
// undefined there, and inventing one here would be a second answer to a
// question the caller has to handle anyway.
inline glm::dvec3 tangent_toward(const glm::dvec3& at_dir,
                                 const glm::dvec3& to_dir) {
    const glm::dvec3 u = glm::normalize(at_dir);
    const glm::dvec3 t = to_dir - glm::dot(to_dir, u) * u;
    const double len = glm::length(t);
    return len > 1.0e-9 ? t / len : glm::dvec3{0.0};
}

// Walk `dist_m` from `from_dir` along the great circle whose initial heading is
// the unit `tangent`.
inline glm::dvec3 great_circle_advance(const glm::dvec3& from_dir,
                                       const glm::dvec3& tangent, double dist_m,
                                       double R) {
    const glm::dvec3 u = glm::normalize(from_dir);
    if (glm::length(tangent) < 1.0e-9 || R <= 0.0) return u;
    const double a = dist_m / R;
    return glm::normalize(u * std::cos(a) + tangent * std::sin(a));
}

// ---------------------------------------------------------------------------
// The snowmachine placement.

// The solved DIRECTIONS plus the world points the caller grounded them to.
// `valid == false` means "there was nothing to place it near", and the caller
// must fall back to the aeroplane rather than put a machine at the origin.
struct SledSpawnPlacement {
    bool valid = false;
    glm::dvec3 sled_dir{0.0, 1.0, 0.0};      // unit, where the machine sits
    glm::dvec3 sled_fwd{0.0, 0.0, -1.0};     // unit tangent, TOWARD the pump
    glm::dvec3 aircraft_dir{0.0, 1.0, 0.0};  // unit, where the aeroplane parks
    // Filled by the caller after it samples its own surfaces (see the banner).
    glm::dvec3 sled_pos{0.0};
    glm::dvec3 aircraft_pos{0.0};
};

// ★★★ THE BEARING IS FROM THE PUMP TOWARD YOUR OWN BUBBLE CENTRE -- you arrive
// on YOUR SIDE of it, with the contested vacuum gap behind the pump rather than
// behind you (plan §4.3; the pumps sit at the DISTAL end of each faction's
// major axis, world/faction_bubbles.h, so "toward the centre" is unambiguously
// "deeper into friendly territory").
//
// The heading is the RETURN bearing: he is set down FACING the pump he came to
// fix, so the objective is the first thing on the screen.
inline SledSpawnPlacement solve_sled_spawn(const glm::dvec3& pump_pos_w,
                                           const glm::dvec3& faction_centre_dir,
                                           double R, const SpawnDials& d) {
    SledSpawnPlacement out;
    const double pr = glm::length(pump_pos_w);
    if (pr < 1.0e-9 || R <= 0.0) return out;
    const glm::dvec3 pump_dir = pump_pos_w / pr;
    glm::dvec3 t = tangent_toward(pump_dir, faction_centre_dir);
    if (glm::length(t) < 1.0e-9) {
        // Pump AT the centre (or antipodal to it): the bearing is undefined, so
        // take ANY tangent rather than refuse to spawn him.
        t = tangent_toward(pump_dir, glm::dvec3{0.0, 0.0, 1.0});
        if (glm::length(t) < 1.0e-9)
            t = tangent_toward(pump_dir, glm::dvec3{1.0, 0.0, 0.0});
        if (glm::length(t) < 1.0e-9) return out;
    }
    out.sled_dir = great_circle_advance(pump_dir, t, d.sled_dist_m, R);
    out.sled_fwd = tangent_toward(out.sled_dir, pump_dir);
    if (glm::length(out.sled_fwd) < 1.0e-9) return out;
    // The aeroplane parks to the machine's RIGHT. sim/sled.h's body frame is
    // +X right / -Z forward, so right = cross(fwd, up) at the machine's own up
    // -- the same handedness `spawn_state` builds its basis with.
    const glm::dvec3 right =
        glm::normalize(glm::cross(out.sled_fwd, out.sled_dir));
    out.aircraft_dir =
        great_circle_advance(out.sled_dir, right, d.aircraft_beside_m, R);
    out.valid = true;
    return out;
}

// The world orientation of a body standing on `up_dir` facing `fwd_dir` -- the
// SAME basis `spawn_state` builds, factored out so the parked aeroplane and the
// seeded machine cannot disagree about which way is which.
inline glm::dquat surface_orientation(const glm::dvec3& up_dir,
                                      const glm::dvec3& fwd_dir) {
    const glm::dvec3 up = glm::normalize(up_dir);
    glm::dvec3 f = fwd_dir - glm::dot(fwd_dir, up) * up;
    if (glm::length(f) < 1.0e-9) {
        f = glm::cross(up, glm::dvec3{0.0, 0.0, 1.0});
        if (glm::length(f) < 1.0e-9)
            f = glm::cross(up, glm::dvec3{1.0, 0.0, 0.0});
    }
    f = glm::normalize(f);
    const glm::dmat3 m{glm::cross(f, up), up, -f};
    return glm::normalize(glm::quat_cast(m));
}

// ★★★ PUTTING A MACHINE IN THE WORLD, IN ONE PLACE. Hoisted out of the KEY_J
// mount seed (app/main.cpp) unchanged, because L3 adds a SECOND site that puts
// a machine down and the seed's four re-arm lines are not obvious enough to
// survive being written twice:
//
// ⚠ `sled = sim::SledState{}` FIRST, then the four `right_*` fields. R4a's
// note at the original site: "an external write to sled state becomes a tape O
// RECORD, and the positional pin cannot carry derived fields -- replay resets
// them to struct defaults, so the RECORD side must do the same or record and
// replay fork after an R press."
//
// ⚠ AND IT DOES NOT TOUCH THE GRIP OR THE MAN. Whole-struct assignment has
// already re-attached the grip by construction; the RIDER goes on through
// `app::player_mount_request` (app/player_mount.h), which is the one seam for
// that, and every caller of this function must call it.
inline void seed_sled_at(sim::SledState& sled, const glm::dvec3& pos_w,
                         const glm::dvec3& up_dir, const glm::dvec3& fwd_dir) {
    sled = sim::SledState{};
    sled.orientation = surface_orientation(up_dir, fwd_dir);
    sled.right_charge = 1.0;
    sled.right_gs_lp = 0.0;
    sled.right_assist_armed = true;
    sled.right_assist_nm_now = 0.0;
    sled.position = pos_w;
}

// ★★★ THE ROUTING FUNCTION. It returns the AIRCRAFT's state either way -- the
// aeroplane exists in both births, it is only FLYING in one of them.
//
//   Aircraft     -> `spawn_state(p, alt_m, up_dir, fwd_dir)`: the same call the
//                   crash branch has always made, unchanged.
//   Snowmachine  -> the aeroplane is PARKED beside the machine: grounded, zero
//                   speed, throttle shut, gear down, facing the way the machine
//                   faces. "The aircraft is parked beside it ... so 'walk to
//                   the plane and take off' remains possible" (plan §4.3) -- a
//                   snowmachine spawn must never be a spawn that lost the
//                   aeroplane.
//
// ⚠ A Snowmachine choice with no usable placement falls back to the Aircraft
// path. "Refuse to spawn" is not an answer a birth function is allowed to give.
inline sim::SimState player_spawn(
    PlayerSpawnChoice choice, const sim::AircraftParams& p,
    double alt_m = 2000.0, const glm::dvec3& up_dir = {1.0, 0.0, 0.0},
    const glm::dvec3& fwd_dir = {0.0, 0.0, -1.0},
    const SledSpawnPlacement* placement = nullptr) {
    if (choice == PlayerSpawnChoice::Aircraft || placement == nullptr ||
        !placement->valid)
        return spawn_state(p, alt_m, up_dir, fwd_dir);
    sim::SimState s;
    s.position = placement->aircraft_pos;
    s.orientation =
        surface_orientation(placement->aircraft_dir, placement->sled_fwd);
    s.velocity = glm::dvec3{0.0};
    s.throttle = 0.0;
    s.last_vhat = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
    // Parked means parked: the gear is DOWN (a belly in the snow is a wreck,
    // not a parking spot) and the kernel is told it is already in contact. Both
    // are re-derived by the ground tick from the next tick on -- this only
    // stops the first frame drawing a retracted-gear aeroplane inside the
    // ground.
    s.gear = 1.0;
    s.on_ground = true;
    return s;
}

// ---------------------------------------------------------------------------
// The menu gate.

// The own SURFACE pump that wants a wrench: dead, or merely damaged. -1 = none.
//
// ⚠ INDEX == FACTION FOR SURFACE PUMPS is the audit's fact (plan §2), and it is
// READ here rather than assumed -- the loop asks each pump whether it is a
// surface pump of the player's own faction, exactly as the L1 site table does.
inline int own_surface_pump_to_fix(const combat::ConquestState& cs) {
    for (int i = 0; i < combat::kNumPumps; ++i) {
        const combat::Pump& pm = cs.pumps[i];
        if (!pm.surface) continue;
        if (pm.faction != cs.player_faction) continue;
        if (pm.alive && pm.hp >= pm.max_hp) continue;
        return i;
    }
    return -1;
}

// The own SURFACE pump, damaged or not. The FIRST spawn always offers the menu
// (nothing is broken yet at tick zero) and a machine still has to be put
// somewhere, so this is what the Snowmachine choice aims at when nothing needs
// fixing. -1 = the player's faction has no surface pump at all.
inline int own_surface_pump(const combat::ConquestState& cs) {
    for (int i = 0; i < combat::kNumPumps; ++i) {
        const combat::Pump& pm = cs.pumps[i];
        if (pm.surface && pm.faction == cs.player_faction) return i;
    }
    return -1;
}

// Which pump a Snowmachine spawn is AIMED at: the one that needs fixing if
// there is one, else the own surface pump. -1 = there is nowhere to put him,
// and `solve_sled_spawn` is not called at all.
inline int spawn_target_pump(const combat::ConquestState& cs) {
    const int fix = own_surface_pump_to_fix(cs);
    return fix >= 0 ? fix : own_surface_pump(cs);
}

// ★★★ WHEN THE MENU APPEARS. "First spawn: menu Aircraft / Snowmachine.
// Respawn after death: menu offered ONLY while an own surface pump is dead or
// damaged (millwright canon); otherwise Aircraft, as today." (Plan §4.3.)
//
//   interactive  false under --smoke / probe / any headless run. A menu that
//                waits for a keypress in a headless smoke run is a HANG, and a
//                hang in CI reads as a timeout, not as a menu.
//   first_spawn  the session's very first birth.
//   conquest_on  no conquest => no pumps => nothing to drive to, so a respawn
//                is the aeroplane, exactly as it always was.
inline bool spawn_menu_should_show(bool interactive, bool first_spawn,
                                   bool conquest_on,
                                   const combat::ConquestState& cs) {
    if (!interactive) return false;
    if (first_spawn) return true;
    if (!conquest_on) return false;
    return own_surface_pump_to_fix(cs) >= 0;
}

}  // namespace app
