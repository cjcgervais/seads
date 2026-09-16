#pragma once

#include <cstddef>
#include <vector>

#include "combat/conquest.h"
#include "combat/reinforce.h"
#include "sim/fields.h"
#include "world/faction_bubbles.h"

// CONQUEST integration glue (app layer, PURE: glm + combat/ + world/ + sim/,
// no raylib, no clock). Bundles the pure conquest game-state + tuning + the
// live-rebuild handle the app::tick loop needs so the whole mechanism threads
// through step_frame as ONE nullable pointer (the DroneWorld/GunWorld pattern).
//
// STRICT SUPERSET: nothing here runs unless the app builds a ConquestWorld and
// passes it (only when [conquest] enabled); a null cq leaves every tick path
// bit-identical (the firewall the goldens + AT-9 mirror prove).
//
// The faction map is 1:1 by construction (combat::CQ_VALLEY == world::VALLEY ==
// 0, combat::CQ_SUDBURY == world::SUDBURY == 1) — asserted in the test.

namespace app {

// The live faction-bubble domes are rebuilt IN THE SIM-TICK PATH (never on a
// frame/wall clock — AT-9) whenever a pump death changes a growth scale, so the
// plant reads the new air on the very next tick regardless of frame rate. The
// app owns the AtmosphereField storage (env.atm points at it); ConquestWorld
// carries a mutable handle to it plus the config-owned dome look values
// (single- sourced from config/game.toml [atmosphere] at startup) so the
// rebuild needs no config access inside the tick.
struct ConquestWorld {
    combat::ConquestState state;
    combat::ConquestParams params;
    // Pump-death FX queue (appended by conquest_tick each tick; drained by the
    // app after the frame to spawn Explosion FX — cosmetic, so frame-drained).
    std::vector<combat::PumpDeathEvent> events;
    // Per-hit FX queue (fly-3: sparks on every registered pump hit so a 15 s
    // HP budget reads as damage, not as an indestructible light). Cosmetic;
    // spawned into the CombatWorld FX pool in the cq tick block.
    std::vector<combat::PumpHitEvent> hit_events;

    // Live bubble rebuild handle (the field env.atm points at) + the dome look
    // values from config/game.toml [atmosphere]. atm_field null => rebuild is a
    // no-op (e.g. a headless test that only exercises the state).
    sim::AtmosphereField* atm_field = nullptr;
    double bubble_ceiling_m = 4000.0;
    double bubble_edge_soft_m = 1200.0;
    double bubble_ceil_soft_m = 600.0;
    // S-domeround (docs/airdome_round_spec.md §1.3): the loader-derived
    // volume-preserving dome centre height H (config bubble_ceiling_m /
    // I(bubble_dome_exponent), or bubble_ceiling_m literally if
    // bubble_ceiling_volume_preserve=false) -- growth scales this exactly
    // like bubble_ceiling_m (see build_faction_bubbles).
    double bubble_dome_h_m = 4959.7;

    // COMPETITIVE rung: raid HUD state, recomputed each tick (no counter
    // plumbing — the render blink uses its own clock). pump_under_attack true
    // while >= 1 enemy raider is on-station over the player-faction surface
    // pump this tick; raided_pump is that pump's index (-1 = none). The app
    // copies both into FrameInfo for the flashing "PUMP UNDER ATTACK" warning +
    // the blinking map marker.
    bool pump_under_attack = false;
    int raided_pump = -1;

    // ★★★ D2 — "MY OWN PUMP IS GETTING ATTACKED", PER FACTION.
    // Chad's stage-0 trigger needs this signal and IT DID NOT EXIST:
    // `pump_under_attack` above is PLAYER-FACTION ONLY (instructor_tick.h's
    // `tgt.faction == pf` guard) and it is an ON-STATION ENVELOPE, not damage
    // — it never sees the player's own rounds. combat/raid.h's `threatened`
    // is a 2500 m proximity predicate that is computed and discarded.
    // So this is an HP-DELTA LATCH at ONE seam: any tick a pump's hp falls,
    // its owner's window is refreshed. That catches player rounds, AI
    // abstracted DPS and anything future, and it names WHICH pump.
    // prev_pump_hp < 0 = the first tick (copy only, never a false trigger).
    // Nothing serialises this struct (the S4 note below), so no file format
    // moves. Dead while dparams.regroup_frac_arm <= 0 (nobody reads it).
    double prev_pump_hp[4] = {-1.0, -1.0, -1.0, -1.0};
    double pump_attacked_s[2] = {0.0, 0.0};  // [s] remaining in the window
    int attacked_pump[2] = {-1, -1};         // which pump last took the hit

    // ★★★ S4 (Chad's ruling, 2026-08-26) — `bool scrambled[2]` USED TO LIVE
    // HERE. It was the one-shot latch for the SCRAMBLE ON COLLAPSE hook, which
    // teleported a dead-dome faction's whole surviving wing into the other
    // faction's bubble. The hook is deleted ("this games ai must not teleport
    // but become skilled at deck flying"), so the latch has nothing to latch.
    // Nothing serialises this struct (the tape writes named conquest fields,
    // never a ConquestWorld blob), so removing the member changes no file
    // format. Do not re-add a collapse latch here: there is no collapse-tick
    // event any more, by ruling.

    // RUNG E5 REINFORCEMENT WAVES. The per-slot wave clock (pure tick
    // counters; combat/reinforce.h). Dead while params.reinforce_delay_s <= 0.
    combat::ReinforceState reinforce;
    // ★ THE MILLWRIGHT SEAM. null = the shipped airborne wave policy
    // (app::AirborneWavePolicy, constructed per tick in instructor_tick.h from
    // the live domes). The coming Millwright tier substitutes a
    // land-at-a-marker / class-choice policy by setting this ONE pointer from
    // main.cpp — the wave clock, the roster bookkeeping and the off-arm above
    // need no re-plumbing. Non-owning; the pointee must outlive the tick loop.
    combat::ReinforcePolicy* reinforce_policy = nullptr;
};

// Map the conquest per-faction growth scales onto world::FactionGrowth[2]. The
// CqFaction 0/1 index IS the world::Faction index (1:1), so the array copies
// straight across — the ONE place the mapping is written, pinned by the test.
inline void faction_growth_from_state(const combat::ConquestState& cs,
                                      world::FactionGrowth grow[2]) {
    for (int f = 0; f < 2; ++f) {
        grow[f].radius_scale = cs.radius_scale[f];
        grow[f].ceiling_scale = cs.ceiling_scale[f];
    }
}

// Rebuild the two faction ovals (2 ellipse bubbles) from the current growth
// into the live AtmosphereField (CLEARING the old bubbles first —
// build_faction_bubbles appends). deck_agl_m/deck_soft_m on the field are
// untouched (set once at startup). Call ONCE at startup and again on every pump
// death (tick-driven).
inline void rebuild_conquest_bubbles(ConquestWorld& cq) {
    if (cq.atm_field == nullptr) return;
    world::FactionGrowth grow[2];
    faction_growth_from_state(cq.state, grow);
    // The dome look adapter build_faction_bubbles reads
    // (config-single-sourced).
    struct DomeCfg {
        double bubble_ceiling_m;
        double bubble_edge_soft_m;
        double bubble_ceil_soft_m;
        double bubble_dome_h_m;
    } cfg{cq.bubble_ceiling_m, cq.bubble_edge_soft_m, cq.bubble_ceil_soft_m,
          cq.bubble_dome_h_m};
    cq.atm_field->bubbles.clear();
    world::build_faction_bubbles(cfg, grow, cq.atm_field->bubbles);
}

}  // namespace app
