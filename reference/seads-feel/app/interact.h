#pragma once
// ★★★ L1 — THE INTERACT SITES (docs/PLAN_20260901_game_loop_millwright.md §3).
//
// Every key in the millwright loop is DIEGETIC: it is a key AT A POSITION, and
// the position is the gate. This file is the one place that turns "where is he
// standing" into "what can he do", so the reach law is written once and the
// three keys (J / F / O) cannot drift apart about what counts as close enough.
//
// ⚠ THE SITE TABLE IS REBUILT EVERY FRAME AND HOLDS NOTHING. A cached site is a
// cached position, and the sled moves; `world/snowpack.h`'s own rule about
// second lookups is the same rule one layer up.
//
// ★ `AircraftBoard` IS A DEVIATION FROM THE PLAN, NAMED RATHER THAN SLIPPED IN.
// §3 lists three kinds (SledMount / PumpRepair / FlakGun). With only those, the
// J key's cycle is Pilot -> Sled -> Afoot -> Sled -> ... and there is NO WAY
// BACK INTO THE AEROPLANE for the rest of the session: before L1, J on the sled
// returned you to the cockpit. Dropping that would take flying away from a
// flight simulator, so the aeroplane is registered as a fourth site and boarded
// by the SAME key at the SAME kind of reach gate. Reported as an open issue for
// the plan to ratify or replace with L3's spawn menu.

#include <glm/glm.hpp>

#include "app/player_mode.h"

namespace app {

enum class SiteKind : int {
    None = 0,
    SledMount,      // the snowmachine, J
    PumpRepair,     // an own surface pump that is dead or damaged, F
    FlakGun,        // the flak gun, O (no gun on this branch -- L5)
    AircraftBoard,  // the parked aeroplane, J (see the banner)
    // ★★★ L10 (Chad 2026-09-07: "fix the airplane engine when it says engine
    // out, the same way the sudburian can fix the pump"). The SAME aeroplane
    // as `AircraftBoard`, at the SAME position, under a DIFFERENT key and a
    // different reach -- because standing at the cowling with a wrench and
    // climbing into the cockpit are two verbs about one object, and a site
    // kind is a verb, not a thing.
    EngineRepair,   // the parked aeroplane's engine, U
};

struct InteractSite {
    SiteKind kind = SiteKind::None;
    glm::dvec3 pos_w{0.0};
    double reach_m = 0.0;
    // Whose it is (combat::CqFaction), -1 = nobody's. Only PumpRepair uses it
    // today; the gun will.
    int faction = -1;
    // Which one of its kind -- the pump index, the gun index. -1 = the only
    // one there is.
    int index = -1;
};

// The reach law, all of it, as dials (config/game.toml [interact] / [repair]).
struct InteractDials {
    double sled_reach_m = 2.5;
    double aircraft_reach_m = 3.0;
    double gun_reach_m = 3.0;
    // How far to the LEFT of the machine's centreline he steps off.
    double dismount_side_m = 0.9;
    // You do not step off a moving snowmachine.
    double dismount_stop_ms = 1.0;
    double repair_reach_m = 8.0;
    // ★ L10: the engine's own reach, NOT the boarding reach. Boarding is 3 m
    // because you climb in at the cockpit; the wrench works at the nose and
    // the man must be able to stand clear of a wing to do it. Its own dial for
    // the same reason the pump's is its own: config/game.toml [repair]
    // engine_reach_m, and it must equal combat::EngineRepairParams::reach_m --
    // the prompt and the wrench are one fact.
    double engine_reach_m = 5.0;
};

// ★ A SITE IS ONLY LIVE ON FOOT. Nothing is reachable from the cockpit or the
// saddle -- you get off first, which is the whole of Chad's ruling R5 ("drives
// up, gets off, then makes the fix"). Repairing keeps the pump site live so
// walking away from it can end the job.
//
// ⚠ "ON FOOT" HERE IS THE OUTER MODE, NOT THE MAN'S POSTURE. This predicate
// answers "are the keys his", and a man face-down in the snow still owns the
// keys. Whether he can WORK is `ModeContext::man_upright`, which the walker
// answers -- the two questions were one bit until a red team pressed F while
// Buried and fixed a pump lying down (2026-09-01).
inline bool site_legal_in_mode(SiteKind k, PlayerMode m) {
    if (k == SiteKind::None) return false;
    if (m == PlayerMode::Afoot) return true;
    // ★ L10: BOTH repairables stay live while he is working, so walking away
    // from the job can end it (the `player_mode_update` law) whichever job it
    // is. Which one he is on is `PlayerModeState::repair_target`, not this.
    if (m == PlayerMode::Repairing)
        return k == SiteKind::PumpRepair || k == SiteKind::EngineRepair;
    return false;
}

// The nearest site whose OWN reach the man is inside. Returns a `None` site
// when nothing is in range -- so a key press near nothing shows nothing, which
// is the prompt behaviour this rung ships for the gun.
//
// ⚠ NEAREST BY DISTANCE, AND EACH SITE CARRIES ITS OWN REACH. A single global
// radius would make a 6 m pump beat a 2.5 m machine you are standing on.
inline InteractSite nearest_site(const InteractSite* sites, int n,
                                 const glm::dvec3& pos_w, PlayerMode mode) {
    InteractSite best;
    double best_d2 = 0.0;
    for (int i = 0; i < n; ++i) {
        const InteractSite& s = sites[i];
        if (!site_legal_in_mode(s.kind, mode)) continue;
        const glm::dvec3 d = s.pos_w - pos_w;
        const double d2 = glm::dot(d, d);
        if (d2 > s.reach_m * s.reach_m) continue;
        if (best.kind == SiteKind::None || d2 < best_d2) {
            best = s;
            best_d2 = d2;
        }
    }
    return best;
}

// The HUD line. Static strings: the HUD holds the pointer for a frame and
// never owns it (render/draw.h's `sled_note` precedent).
inline const char* interact_prompt(const InteractSite& s) {
    switch (s.kind) {
        case SiteKind::SledMount:
            return "J  GET ON";
        case SiteKind::PumpRepair:
            return "U  FIX PUMP";
        case SiteKind::FlakGun:
            return "O  MAN THE GUN";
        case SiteKind::AircraftBoard:
            return "J  BOARD AIRCRAFT";
        case SiteKind::EngineRepair:
            return "U  FIX ENGINE";
        case SiteKind::None:
            break;
    }
    return nullptr;
}

// Fold the whole site table plus the two vehicle facts into the mode machine's
// context. ONE function, so `main()` cannot half-fill the struct.
//
// ⚠ PER KIND, NOT "THE NEAREST". Each key has its OWN site kind, so standing
// between the machine (2.5 m) and a pump (6 m) must arm BOTH J and F. Building
// the context off `nearest_site` alone would have let the nearer thing silently
// disarm the other key -- the prompt shows one line, but the keys are three.
inline ModeContext mode_context_from(const InteractSite* sites, int n,
                                     const glm::dvec3& pos_w, PlayerMode mode,
                                     bool aircraft_ready, bool sled_seeded,
                                     bool sled_stopped, bool man_upright,
                                     bool repair_complete) {
    ModeContext c;
    c.aircraft_ready = aircraft_ready;
    c.sled_seeded = sled_seeded;
    c.sled_stopped = sled_stopped;
    // ★ THE TWO NON-GEOMETRIC FACTS COME IN AS ARGUMENTS RATHER THAN BEING
    // WRITTEN ONTO THE RESULT BY THE CALLER. This function's whole reason to
    // exist is that `main()` cannot half-fill the struct; a field the caller is
    // trusted to remember to set afterwards is a half-filled struct with extra
    // steps. `man_upright` is the walker's answer (BY ENUMERATOR NAME);
    // `repair_complete` is the pump's own HP.
    c.man_upright = man_upright;
    c.repair_complete = repair_complete;
    double pump_d2 = 0.0;
    for (int i = 0; i < n; ++i) {
        const InteractSite& s = sites[i];
        if (!site_legal_in_mode(s.kind, mode)) continue;
        const glm::dvec3 d = s.pos_w - pos_w;
        const double d2 = glm::dot(d, d);
        if (d2 > s.reach_m * s.reach_m) continue;
        switch (s.kind) {
            case SiteKind::SledMount:
                c.sled_in_reach = true;
                break;
            case SiteKind::AircraftBoard:
                c.aircraft_in_reach = true;
                break;
            case SiteKind::FlakGun:
                c.gun_in_reach = true;
                break;
            case SiteKind::EngineRepair:
                c.engine_in_reach = true;
                break;
            case SiteKind::PumpRepair:
                if (!c.pump_in_reach || d2 < pump_d2) {
                    c.pump_in_reach = true;
                    c.pump_index = s.index;
                    pump_d2 = d2;
                }
                break;
            case SiteKind::None:
                break;
        }
    }
    return c;
}

}  // namespace app
