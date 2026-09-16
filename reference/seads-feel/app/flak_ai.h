#pragma once
// FLAK STAGE D -- THE AI GUNNER (docs/FLAK_GUN_SPEC.md §7, immersion ladder
// 4/4). When the player is not standing behind an Oerlikon, somebody else is:
// this is that man. He acquires an OPPOSING-faction raider, slews with the
// SAME render::flak::SlewRates the player's hands get, leads with the SAME
// render::lead_solution the sight pipper draws, fires 8-15 round bursts, runs
// his drum dry and swaps it -- so from the air, and from the far side of the
// valley, the world is DEFENDED: distant flashes, tracer streams, a flak
// curtain, and a report that arrives quiet because it is two kilometres away.
//
// ★★★ THE PLAYER-ISOLATION PROOF IS BY CONSTRUCTION, NOT BY REVIEW.
// flak_ai_tick's signature admits NO player state: no sim::SimState of the
// aeroplane, no CombatWorld, no ConquestState, no app::FlightState, no
// camera. The ONLY things it can see are the gun's own FlakWorld, the gun's
// faction int, and a CONST view of the drone fleet. There is therefore no
// expression inside this file that can name the player, and no future edit
// can quietly add one without changing the signature at every call site.
// (E-ladder canon: the AI has never fired a shot at Chad. Not this rung's
// decision to change.)
//
// PURE: glm + std + the pure headers (render/flak_gun.h, render/gunsight.h,
// drone/drone.h, combat/conquest.h for maverick_faction, app/flak_tick.h for
// the FlakWorld). No raylib, no clock, no getenv, no RNG -- the wander and
// the burst rhythm are CLOSED-FORM in (per-gun seed, an accumulated phase,
// a shot count), the house FX discipline, so a headless test sees exactly
// what the game sees.
//
// THE DOUBLE-DRIVE RULE (constraint 3): this controller never decides whether
// it is allowed to run. The CALLER sets FlakAiGun::fk.manned = false for the
// gun the player is standing at, and flak_tick's own gates (`if (fk.manned)`
// on the slew, `fk.manned && fk.fire_held` on the fire) make the AI gun inert
// that frame. flak_ai_tick returns immediately on an unmanned world WITHOUT
// touching demand or fire_held, so the release re-arms cleanly from wherever
// the barrel actually is (no snap -- the same "walking away mid-slew must not
// freeze the barrel" rule flak_tick already keeps).

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "app/flak_tick.h"
#include "combat/conquest.h"  // maverick_faction -- the VALLEY/SUDBURY split
#include "drone/drone.h"
#include "render/flak_gun.h"
#include "render/gunsight.h"

namespace app {

// v0 code constants, the combat::RaidParams precedent ("promote to config if
// this stops being a placeholder"). Every one is a FEEL DIAL Chad judges from
// the air; none is a measured fact (the measured facts -- muzzle speed, ROF,
// drum -- are read off the gun's own battery, single-sourced from
// render/flak_gun.h).
// ★ RULED OFF (Chad 2026-08-30: "Take off the automatic ai operation of the
// gun against enemies. It should be for player use only."). Gates the fk_ai
// pointer at the step_frame call in app/main.cpp: the gunners are still
// CONSTRUCTED (the SEADS_FLAK_AI_TARGET smoke rig reads their mount frames)
// but never tick, never fire, never sweep. The ruling is attached to the
// Stage-D design that prompted it -- flip this if he re-rules.
inline constexpr bool kFlakAiOperate = false;

struct FlakAiParams {
    // Acquire inside this slant range OF THE GUN, hold to hold_frac x it --
    // the sight's own sticky-target numbers (app/main.cpp's flak pipper),
    // shortened from 1800 m because a man is deciding, not a HUD: the round
    // is dead at ~870 m (self-destruct) and doctrine opened at ~1100 m.
    double engage_m = 1500.0;
    double hold_frac = 1.15;  // anti-chatter: drop the target only past this
    // The burst rhythm. Deterministic per gun (see FlakAiState::seed).
    int burst_min_rounds = 8;
    int burst_max_rounds = 15;
    double pause_min_s = 0.5;
    double pause_max_s = 1.0;
    // He does not shoot while the barrel is still swinging onto the solution:
    // the bore must be inside this cone of the demanded aim.
    double fire_cone_cos = 0.9962;  // cos(5 deg)
    // The aim wander -- he is a man on two spade grips, not an autocannon
    // director. Closed-form: two incommensurate sines per axis. 6 mrad is
    // ~9 m at 1.5 km and ~2.4 m at 400 m, so the 4 m proximity fuze turns it
    // into a near-miss curtain at range and hits up close. NOT dispersion
    // (the rounds already scatter by nothing); this is the AIM.
    double wander_rad = 0.006;
    double wander_hz_a = 0.31;
    double wander_hz_b = 0.53;
};

// Per-gun controller state. Bookkeeping only -- no ballistics, no timing that
// flak_tick owns.
struct FlakAiState {
    int target = -1;            // index into the drone fleet, or -1
    unsigned seed = 0u;         // per-gun; the ONLY thing that makes two guns
                                //   differ (set it from the gun index)
    double phase_s = 0.0;       // wander clock (accumulated dt, never a clock)
    double pause_left_s = 0.0;  // >0 = between bursts
    int burst_left = 0;         // rounds still owed to the current burst
    long long shots_seen = 0;   // last FlakWorld::spawned_total (the MONOTONE
                                //   total -- the documented discrete-reader
                                //   contract; spawned_accum belongs to the
                                //   frame-side audio drain and is not ours)
    unsigned rng = 0u;          // advanced ONLY at burst boundaries
};

// A gun the AI mans: its own world, its own controller, and which drawn gun
// it is. One of these per unmanned Oerlikon; main owns the vector.
struct FlakAiGun {
    FlakWorld fk{};
    FlakAiState ai{};
    int faction = 0;  // combat::CqFaction of the pump this gun defends
    int gun = -1;     // index into the app's render::FlakDraw list
    // COSMETIC, app-stepped (main.cpp), read-only in render/: this gun's own
    // muzzle-flash and snow-blast envelopes and its FX phase, so a gun firing
    // two kilometres away FLASHES. Nothing in the tick reads them.
    double flash = 0.0;
    double blast = 0.0;
    int shot_seq = 0;
};

// A 32-bit LCG stepped by hand -- deterministic, seedable, and NOT std::rand
// (the house rule: no hidden global state in a tick). Returns [0,1).
inline double flak_ai_next01(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<double>((s >> 8) & 0xffffffu) / 16777216.0;
}

// ★ TARGET SELECTION. The signature is the proof: `drones` is the ONLY world
// this can see, and it is const. Returns a fleet index or -1.
//
// FACTION (constraint 2): a drone's side is combat::maverick_faction of its
// spawn_index -- the same function combat/raid.h uses to decide who raids
// whom, and it returns CqFaction values (0 = VALLEY, 1 = SUDBURY) which is
// exactly what render::FlakDraw::faction carries. So the gun on the Valley
// pump shoots faction-1 aircraft and vice versa, and the two can never drift
// apart because there is one owner of "which side is this".
//
// A drone marked `inert` is a frozen wreck (conquest no-respawn) -- never a
// target. The sticky `keep` index is honoured while it is still valid and
// still inside hold_frac x engage: the anti-chatter latch the gunsight
// selector already paid for.
// ★ L12: `player_faction` is what turns a spawn_index into a faction LABEL
// (combat/conquest.h roster_sizes) -- without it a Central City player's gun
// would shoot his own wing. Defaulted to the shipped Valley roster.
inline int flak_ai_select_target(
    const std::vector<drone::DroneState>& drones, const glm::dvec3& gun_pos,
    int gun_faction, int keep, const FlakAiParams& pp,
    int player_faction = combat::CQ_VALLEY) {
    const double eng2 = pp.engage_m * pp.engage_m;
    const double hold2 =
        (pp.hold_frac * pp.engage_m) * (pp.hold_frac * pp.engage_m);
    const auto valid = [&](int i) -> bool {
        if (i < 0 || i >= static_cast<int>(drones.size())) return false;
        const drone::DroneState& d = drones[i];
        if (d.inert) return false;
        return combat::maverick_faction(d.spawn_index, player_faction) !=
               gun_faction;
    };
    const auto d2 = [&](int i) -> double {
        const glm::dvec3 dd = drones[i].curr.position - gun_pos;
        return glm::dot(dd, dd);
    };
    if (valid(keep) && d2(keep) <= hold2) return keep;
    int best = -1;
    double bd2 = eng2;
    for (int i = 0; i < static_cast<int>(drones.size()); ++i) {
        if (!valid(i)) continue;
        const double q = d2(i);
        if (q < bd2) {
            bd2 = q;
            best = i;
        }
    }
    return best;
}

// ★ THE AI GUNNER'S TICK. Writes fk.demand and fk.fire_held; nothing else.
// (flak_tick, called right after by the SAME caller, owns the slew, the
// battery, the drum, the reload and the self-destruct -- the AI gun's firing
// loop is bit-for-bit the loop the player gets, which is the whole point.)
//
// The muzzle speed and the drag constant come off the gun's OWN battery, so
// the lead the AI shoots is solved from the same numbers the round flies
// with; a retune of one cannot leave the other behind.
//
// NOTE THE ABSENT PARAMETER. There is no player here. There is no way to
// write one in without changing this line.
inline void flak_ai_tick(FlakAiState& ai, FlakWorld& fk, int gun_faction,
                         const std::vector<drone::DroneState>& drones,
                         double dt, double g,
                         const FlakAiParams& pp = FlakAiParams{},
                         int player_faction = combat::CQ_VALLEY) {
    // The player has this gun (or it is otherwise off): leave demand and
    // fire_held exactly as they are. Constraint 3, enforced at the top.
    if (!fk.manned) {
        ai.target = -1;
        return;
    }
    ai.phase_s += dt;

    // -- acquire / hold -----------------------------------------------------
    ai.target =
        flak_ai_select_target(drones, fk.mount.pos, gun_faction, ai.target,
                              pp, player_faction);
    if (ai.target < 0) {
        fk.fire_held = false;
        return;  // demand held where it is: the barrel rests, it does not snap
    }
    const sim::SimState& tgt = drones[ai.target].curr;

    // -- lead ---------------------------------------------------------------
    // The SAME solver the sight pipper uses, from the gun's own synthetic
    // shooter (rebuilt by flak_tick last tick; zero velocity, local gravity).
    const double muzzle = fk.gw.battery.guns.empty()
                              ? render::flak::kMuzzleSpeedMps
                              : fk.gw.battery.guns[0].muzzle_speed;
    const double drag_k =
        fk.gw.battery.guns.empty() ? 0.0 : fk.gw.battery.guns[0].drag_k;
    const render::LeadSolution sol =
        render::lead_solution(fk.shooter, tgt, muzzle, pp.engage_m,
                              -g * fk.mount.up, drag_k, dt);
    glm::dvec3 aim = tgt.position - fk.mount.pos;  // fallback: pure pursuit
    if (sol.valid) aim = sol.lead_dir;

    // -- the wander ---------------------------------------------------------
    // Closed-form in (seed, phase) -- no RNG in the aim, so a headless step
    // reproduces the game exactly. Applied in POSE space (train / elevation),
    // which is the space his shoulders actually move in.
    render::flak::Pose want{};
    if (!render::flak::aim_to_pose(fk.mount, aim, want)) {
        fk.fire_held = false;
        return;
    }
    const double ph = static_cast<double>(ai.seed % 977u) * 0.0643;
    const double w2pi = 6.283185307179586;
    want.train_rad = render::flak::wrap_pi(
        want.train_rad +
        pp.wander_rad * std::sin(w2pi * pp.wander_hz_a * ai.phase_s + ph));
    want.elev_rad = render::flak::clamp_elev(
        want.elev_rad +
        pp.wander_rad *
            std::sin(w2pi * pp.wander_hz_b * ai.phase_s + ph * 1.7));
    fk.demand = want;

    // -- the burst rhythm ---------------------------------------------------
    // Rounds actually fired are read off the MONOTONE spawned_total (never
    // drained, so this reader cannot be broken by whether the audio block
    // ran -- the Stage C brass-storm lesson, restated). A burst is spent in
    // ROUNDS, not seconds: a drum swap mid-burst therefore pauses it and it
    // resumes on the fresh drum, which is what a man does.
    const long long fired = fk.spawned_total - ai.shots_seen;
    ai.shots_seen = fk.spawned_total;
    if (fired > 0 && ai.burst_left > 0)
        ai.burst_left -= static_cast<int>(fired);
    if (ai.burst_left <= 0 && ai.pause_left_s <= 0.0 && fired > 0) {
        // burst just ended -> draw this gun's next pause
        ai.burst_left = 0;
        ai.pause_left_s =
            pp.pause_min_s +
            flak_ai_next01(ai.rng) * (pp.pause_max_s - pp.pause_min_s);
    }
    if (ai.pause_left_s > 0.0) {
        ai.pause_left_s -= dt;
        fk.fire_held = false;
        return;
    }
    if (ai.burst_left <= 0) {
        const int span = pp.burst_max_rounds - pp.burst_min_rounds;
        ai.burst_left =
            pp.burst_min_rounds +
            static_cast<int>(flak_ai_next01(ai.rng) *
                             static_cast<double>(span + 1));
    }

    // -- the trigger --------------------------------------------------------
    // Only once the barrel is ON the solution: a rate-capped gun swinging
    // 90 deg/s onto a crossing target would otherwise hose the whole sky, and
    // the tracer stream is the READ the player gets from two kilometres up.
    const glm::dvec3 bore = render::flak::bore_dir_world(fk.mount, fk.pose);
    const double an = glm::length(aim);
    const bool on_solution =
        an > 0.0 && glm::dot(bore, aim / an) >= pp.fire_cone_cos;
    fk.fire_held = on_solution && (!sol.valid || sol.in_range);
}

}  // namespace app
