#pragma once

#include <string>

#include "sim/params.h"

// TOML -> GameParams (SCARCE SKIES, MASTER_PLAN §4 / the R4+ tuning contract):
// the single source for every GAME-MECHANIC felt/balance value. Lives in
// config/ (with the data), never in sim/ or app/ (no bare numbers there).
// Strict like load_aircraft / load_scenario: every key present, every value
// sane, or it throws. Angles convert from degrees (the config boundary) once,
// here.
//
// RAW values at the boundary (degrees / metres / fractions) — the app maps
// [ground] onto sim::GroundParams (deg -> cos there is main.cpp's one line;
// GroundParams wants the cos both checks compare against). [deck] is a Phase-2
// placeholder: loaded strictly now so the key exists for Chad, consumed when
// the AtmosphereField lands.

namespace cfg {

struct GameParams {
    struct {
        bool enabled = false;  // false = bare-sphere world (env.ground null)
        double slope_limit_rad = 0.0;   // landable terrain-slope / touchdown-
                                        // attitude limit vs the terrain normal
        double friction = 0.0;          // rolling decel, fraction of g
        double max_sink_ms = 0.0;       // gentle-contact sink ceiling [m/s]
        double normal_probe_m = 0.0;    // structural finite-diff step [m]
        double contact_height_m = 0.0;  // CG height above the wheels [m]
        // R4g (Chad's fly asks, 2026-07-15):
        double brake_friction = 0.0;       // held-brake EXTRA decel, fraction
                                           // of g (may exceed the accelerate
                                           // check — a brake-hold runup holds)
        double roll_align_rate_rad = 0.0;  // grounded roll-to-terrain-plane
                                           // rate [rad/s]; 0 = off (REVERTED)
        // R4-FLY-5 consequence dials (Chad: damage, not prevention):
        double ground_ang_damp = 0.0;         // tire rotational friction [1/s]
        double wing_halfspan_m = 0.0;         // wingtip strike geometry [m]
        double ground_loop_lat_g = 0.0;       // lateral-g tire limit (crash)
        double brake_pitch_rate_rad = 0.0;    // nose-over pitch rate [rad/s at
                                              // full brake decel fraction 1]
        double prop_strike_pitch_rad = 0.0;   // prop clearance angle [rad]
        double noseover_full_speed_ms = 0.0;  // nose-dip fades below this
                                              // ground speed [m/s]; 0 = the
                                              // unscaled dip (R4-FLY-6)
        // T3 (docs/tunnel_staging.md): deep-penetration wall-strike floor [m].
        // A state deeper than this below the contact surface can only be a
        // tunnel-wall graze into rock — a crash, not a teleport-landing. Must
        // exceed contact_height_m + a few metres (loader-enforced). 0 = off.
        double deep_penetration_m = 0.0;
    } ground;
    struct {
        // R4f building collision (Chad: "I go right through houses").
        bool collide = false;        // false = buildings are ghosts (pre-R4f)
        double inflate_m = 0.0;      // widen every footprint radius [m]
        double base_margin_m = 0.0;  // prism lower-band slack [m]
    } buildings;
    struct {
        // R4-FLY-6/7 touchdown FX (render-only; app spawns, draw shapes).
        double touchdown_ref_speed_ms = 0.0;  // full-burst ground speed [m/s]
        double touchdown_intensity = 0.0;     // overall burst scale (0 = off)
        double rolling_min_speed_ms = 0.0;    // re-spawn while rolling above
                                              // this [m/s]; 0 = touchdown-only
    } fx;
    struct {
        // R4-FLY-7 thumb-throttle button codes (raylib; -1 = unbound). The
        // caller builds input::ThumbBinds from these — input/ is config-free.
        int thumb_up_a = 4;
        int thumb_up_b = 5;
        int thumb_down_a = 3;
        int thumb_down_b = -1;
    } input;
    struct {
        // R6 AtmosphereField (MASTER_PLAN §3.C — air becomes SPATIAL).
        // ACTIVATION IS CHAD'S HALT: true makes air a bubble you can fly out
        // of (thin ρ outside => thrust/lift/authority all starve together);
        // false = env.atm null, the altitude-taper kernel bit-identically.
        // The single R6 TEST bubble is centered on the SPAWN point so Chad
        // starts inside and flies to the soft edge (the B key toggles it live).
        bool enabled = false;
        double deck_agl_m = 0.0;   // full air below this AGL EVERYWHERE (the
                                   // go-anywhere floor; land outside a bubble)
        double deck_soft_m = 0.0;  // deck fade width above [m]
        // RUNG E16: measure the deck from the TERRAIN, not from the sphere
        // (sim::AtmosphereField::deck_terrain_relative — the full rationale is
        // at that field). false = the R6 bare-sphere deck, bit-for-bit.
        bool deck_terrain_relative = false;
        double bubble_radius_m = 0.0;   // test-bubble surface (arc) radius [m]
        double bubble_ceiling_m = 0.0;  // dome height AMSL [m]
        double bubble_edge_soft_m = 0.0;  // horizontal edge fade [m] (km-scale
                                          // => a felt, not scripted, wall)
        double bubble_ceil_soft_m = 0.0;  // ceiling fade [m]
        // S-domeround (docs/airdome_round_spec.md): the superellipse
        // roundness dial (Chad's fly-dial) + the volume-preservation flag,
        // both read straight from TOML; bubble_dome_h_m is DERIVED by the
        // loader (never a TOML key) from these two + bubble_ceiling_m.
        double bubble_dome_exponent = 0.0;
        bool bubble_ceiling_volume_preserve = true;
        double bubble_dome_h_m = 0.0;  // derived: bubble_ceiling_m / I(n)
    } atmosphere;
    struct {
        // R5 GravityField (MASTER_PLAN §3.B — the Escape Ceiling).
        // ACTIVATION IS CHAD'S HALT: true deliberately changes flight above
        // h_g0_m (the felt ceiling moves by design); false = constant g,
        // env.grav null, the frozen kernel bit-identically.
        bool enabled = false;
        double h_g0_m = 0.0;     // full g at/below this altitude [m]
        double sigma_g_m = 0.0;  // Gaussian falloff width above [m]
        // (R5d's no_return_atm_frac REMOVED by Chad's fly-4 ruling: the
        // no-return is a GAME RULE at the E=0 crossing — app::tick's escape
        // claim — not a display threshold.)
    } gravity;
    struct {
        // T1 — the Errington tunnel net (MASTER_PLAN §2.5; world/tunnel_net.h).
        // The app builds a world::TunnelParams from these + the aircraft R and
        // maps enabled onto env.tunnels. false => env.tunnels null, the frozen
        // kernel bit-identically. All lengths in metres (FEEL dials — Chad's
        // stick). Defaults match world::TunnelParams' documented canon.
        bool enabled = false;
        // THE ELLIPTICAL BORE (T6b): width/height are SEMI-axes (bore = 2x).
        double tube_width_m = 110.0;
        double tube_height_m = 90.0;
        double depth_m = 2000.0;
        double soft_m = 40.0;
        double ramp_frac = 0.3;
        double spacing_m = 150.0;
        // THE FLOOR (T5a): a flat floor raised floor_height_m above the spine
        // tube's lowest point (chord-truncating the circular section). 0 = OFF
        // (bit-identical to no floor).
        double floor_height_m = 20.0;
        // T11/T12 — THE SEALED-CORE ARENA: one shallow ARENA ellipsoid, floor
        // sealed over a buried core (T12; the T11 core-window SHAFT is gone).
        double arena_a_m = 7350.0;      // arena horizontal semi-axis
        double arena_c_m = 2600.0;      // arena vertical semi-axis (oblate)
        double arena_depth_m = 1500.0;  // arena apex depth below mid terrain
        double cavern_core_m = 2500.0;  // buried |position| safety-floor radius
        double breach_margin_m =
            300.0;  // plateau below the arena apex (breach)
        double chamber_long_m = 200.0;
        double chamber_lat_m = 140.0;
        double chamber_vert_m = 120.0;
        double chamber_breach_offset_m = 800.0;  // chamber above its breach
        double connector_radius_m = 60.0;
        bool chambers_on = false;  // T13 (S1): pump pockets gated off until the
                                   // pumps land; true = T4-era side chambers
        // THE MURRAY BOWL (T4a): the open-pit raid mouth.
        double bowl_radius_m = 450.0;  // opening radius at the surface
        double bowl_depth_m = 300.0;   // floor depth below the Murray surface
        // THE ERRINGTON ENTRY PIT (T6c) + THE MONOTONE DESCENT (T6d).
        double mouth_sink_m = 130.0;  // bore-center sink below the home surface
        double min_cover_m = 60.0;    // min rock above the bore crown en route
        // T13 — THE ENTRY APPROACH TRENCH (S3): up-tangent open-cut ramp.
        double trench_len_m = 450.0;  // trench reach from the pit rim (0 = OFF)
        double trench_rim_m = 150.0;  // trench open-cut rim radius
        // T13 (B1) — THE SURFACE HEADFRAME + BULKHEAD COLLAR: a visual-only
        // steel pit-head structure over the Errington mouth (NO SDF; planes fly
        // through). headframe_on == false => structurally absent (bit-identical
        // mesh). headframe_h_m is the sheave head-house height above grade.
        double headframe_h_m = 80.0;  // head-house height above grade [m]
        bool headframe_on = true;     // false = piece absent
    } tunnel;
    struct {
        // [conquest] — the faction-bubble growth rung (world/faction_bubbles.h,
        // Scarce Skies MASTER_PLAN §4). ACTIVATION IS CHAD'S HALT: false ships
        // the mechanism a structural no-op (growth stays 1.0/1.0 forever, so
        // whatever consumes it is bit-identical to the R6 single-bubble path
        // with these two ovals substituted). player_faction picks which side
        // the pilot flies; the caller maps it onto world::Faction.
        bool enabled = false;
        double growth_radius_frac = 0.0;   // radius growth PER enemy pump
                                           // destroyed, fraction of baseline
        double growth_ceiling_frac = 0.0;  // ceiling growth PER enemy pump
                                           // destroyed, fraction of baseline
        std::string player_faction = "sudbury";  // "valley" | "sudbury"
        // 2026-07-25 fly-2 ruling C: destroying a pump SHRINKS the victim
        // faction's own dome by these fractions (floored at 0.4 in
        // combat::conquest_tick — never a vanishing bubble).
        double shrink_radius_frac = 0.0;
        double shrink_ceiling_frac = 0.0;
        // 2026-07-25 fly-2 ruling D: seconds of sustained all-guns-on-target
        // fire to kill a pump; the app derives pump_max_hp from this * the
        // live gun battery's DPS (combat::derive_pump_max_hp) — never a
        // welded HP number.
        double pump_kill_seconds = 15.0;
        // SUDDEN-DEATH MATCH CLOCK (Chad, 2026-07-26): seconds a faction has to
        // win outright after losing its LAST pump, or it loses on the buzzer.
        double match_countdown_s = 600.0;
        // ADEPT-AI + COMPETITIVE rung (2026-07-25 fly-2): fraction of the
        // PLAYER battery DPS a designated enemy raider deals to the
        // player-faction surface pump while on-station. A pump dies to an
        // uninterrupted raider in ~pump_kill_seconds / raid_dps_frac s.
        // Loader-checked to [0, 1].
        double raid_dps_frac = 0.15;
        // ALL-VS-PLAYER FURBALL (Chad's ask, 2026-07-26): true = the whole
        // non-inert fleet (either faction) hunts the player, overriding the
        // 5v5 team split, the difficulty max_engaged cap, and the
        // engage/disengage range gate; also suppresses the maverick tunnel-
        // run diversions (see main.cpp) so nobody peels off mid-chase. false
        // = the pre-furball 5v5 conquest mode exactly.
        bool all_vs_player = true;
        // FIX-F3 (docs/ai_phase2_fix_spec.md): the raid-pause player-range
        // latch's OWN dial pair -- split from the FOE-ASSIGNMENT
        // engage_range/disengage_range (scenario.toml [combat], 6/7 km,
        // sized for dogfight assignment) so a raid pauses only when the
        // player is close enough to actually SEE it. Loader-checked
        // disengage > engage > 0.
        double raid_pause_engage_m = 1500.0;
        double raid_pause_disengage_m = 2500.0;
        // RUNG E5 REINFORCEMENT WAVES (Chad, 2026-08-20): seconds between a
        // conquest pilot's death and his replacement launching from his own
        // faction's air, and whether that replacement re-enters the kill
        // roster. 0 = OFF = permanent death (the R-era ruling, consciously
        // superseded — see config/game.toml [conquest] and combat/reinforce.h).
        double reinforce_delay_s = 0.0;
        bool reinforce_restores_roster = true;
        // RUNG E6 — the relentless pass (see combat/conquest.h for the full
        // rationale on each; every default here IS the pre-E6 behaviour).
        int reinforce_pool_n = -1;
        double leash_min_radius_scale = 0.0;
        bool raid_no_pause = false;
        // RUNG E12.1 -- the raider backfill. false = the pre-E12 static
        // trait-table designation, bit-identical.
        bool raid_backfill = false;
    } conquest;
    struct {
        // ★ L1 THE MILLWRIGHT LOOP (docs/PLAN_20260901_game_loop_millwright.md
        // §3): the diegetic reach law, in metres. Every key in the loop is a
        // key AT A POSITION and these are the positions. Consumed as
        // app::InteractDials -- app/ builds no reach numbers of its own.
        double sled_reach_m = 2.5;
        double aircraft_reach_m = 3.0;
        double gun_reach_m = 3.0;
        double dismount_side_m = 0.9;
        double dismount_stop_ms = 1.0;
    } interact;
    struct {
        // L1 registered the pump sites and the prompt at this reach; L2 owns
        // the progress clock. Consumed as combat::RepairParams -- combat/ holds
        // no dial of its own, app/ builds no number of its own.
        double reach_m = 6.0;
        double full_s = 60.0;
        int repair_points = 0;
        // ★ L10: the engine's own pair, consumed as
        // combat::EngineRepairParams + app::InteractDials::engine_reach_m.
        double engine_reach_m = 5.0;
        double engine_full_s = 45.0;
    } repair;
    struct {
        // ★ L3 THE SPAWN POLICY (plan §4.3; app/spawn_policy.h). Chad's ruling
        // R4: "respawn straight into the snowmachine from a set distance away
        // from the pump." This is that distance, and the gap the aeroplane is
        // parked at beside it. Consumed as app::SpawnDials.
        double sled_dist_m = 300.0;
        double aircraft_beside_m = 5.0;
    } spawn;
};

// Takes the already-loaded AircraftParams (the load_scenario pattern): the
// ground dials are cross-checked against the airframe — rolling friction above
// T_max/(m*g) makes a full-throttle takeoff roll decelerate, which is a config
// bug, not a tune.
GameParams load_game_toml(const std::string& path,
                          const sim::AircraftParams& ap);

}  // namespace cfg
