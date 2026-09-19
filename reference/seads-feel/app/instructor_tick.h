#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/conquest_world.h"  // CONQUEST: faction bubbles + pumps + lives
#include "app/loop.h"
#include "app/rest_horizon.h"  // v13 rest-edge horizon law (shared w/ the Sting)
#include "app/spawn_policy.h"  // L3: spawn_state + the ONE player_spawn
#include "app/sting.h"  // STING RPAS: the P-key FPV interceptor (sting lane)
#include "combat/kill.h"
#include "app/flak_ai.h"    // FLAK STAGE D: the AI gunner
#include "app/flak_tick.h"  // FLAK F-FIRE: the manned ground gun
#include "combat/raid.h"  // CONQUEST: enemy pump raids (is_raider/on_station)
#include "control/controller.h"
#include "drone/drone.h"
#include "input/aim_frame.h"
#include "input/aim_state.h"
#include "render/camera.h"
#include "render/gunsight.h"
#include "render/tunnel_lamp_hits.h"  // T5c destructible gaslamp hit pass
#include "sim/fields.h"
#include "sim/ground.h"  // T2b: sim::air_ground_radius -- THE one surface
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"
#include "weapon/ballistics.h"
#include "world/tunnel_net.h"  // T1 — env->tunnels->contains (crash yield)

// The app's fixed-dt per-tick core, extracted so the SHIPPED tick IS the tested
// tick (CLAUDE.md: "route the live path THROUGH the tested function"; the P1b
// audit finding, pre_spec_audits/fable5_section6_fixreport.md). Before this the
// crash predicate (altitude <= 0 -> respawn), the GROUNDED pairing, and the
// frame-carried camera call site executed under NO test — a golden pinned only
// the code that recorded it, everywhere except the app loop.
//
// The tick is MODE-COMMON, not "the instructor branch": transport, the GROUNDED
// pairing, and the crash-reset fire in RAW mode too (they sit outside
// main.cpp's `if (raw_mode)`). Extracting them once, driven by a `raw_mode`
// flag, is what keeps the crash predicate from forking into an untested
// raw-side copy (the "grep every branch that does the gated thing" /
// "two-copies seam" lessons).
//
// PURE / raylib-free (it may include control/, input/, render/ but never a
// device or a clock): the caller (app/main.cpp) owns the accumulator, device
// polling, the per-FRAME mouse accrual + freelook-orbit camera, and focus-loss;
// this owns the per-TICK plant/instructor advance. The harness ClosedLoop::tick
// (test/harness/instructor.h) is the OTHER caller of the same shared pieces
// (extract/transport/Freelook/control::step); a mirror-equivalence test pins
// the two against drift (test_instructor_tick.cpp).

namespace app {

// ALL-VS-PLAYER FURBALL (Chad's ask, 2026-07-26): the engage/disengage range
// gate bypass. Not a feel dial (there is no "beatable" reading of a bandit
// that never notices the player exists) — a structural sentinel so a
// furball's assign_engagements() call never patrols a qualifying candidate
// away for being merely far off or high up; the fleet's OWN speed/turn/stall
// limits (and the terrain/thin-air guards in drone::tick) are what keep the
// fight fair, not a notice-range fiction.
constexpr double kFurballRangeM = 1.0e9;
// game-AI-R4 pump defense: an enemy aircraft inside this radius of an alive
// surface pump scrambles this many same-faction defenders to it.
constexpr double kDefendThreatRadiusM = 2500.0;
constexpr int kDefendersPerFaction = 2;
// game-AI-R4 AI-vs-AI gunnery: while a drone has gun solution on a DRONE foe
// (wants_fire), it deals the difficulty table's expected DPS scaled by this —
// abstracted like the pump raids: only the player is swept against rounds.
// ★ RUNG S3-GUNS — THE COMMENT THAT USED TO STAND HERE WAS FALSE. It claimed
//   "real tracers fly for the look". They did not: the AI-vs-AI window and the
//   raid credit both used the SNAPSHOT band, not the player-facing wants_fire
//   discipline that enemy_fire_tick spawns off, so `wants_fire` was 0 of 70,220
//   drone samples across tapes 10 and 11 and this DPS came out of nowhere on
//   screen. It is TRUE NOW, and by construction: both seams call
//   combat::cosmetic_fire (combat/kill.h) at the difficulty table's own
//   bandit_rof_hz, into a pool no damage sweep is ever handed.
constexpr double kAiVsAiDpsFrac = 0.8;  // my pacing call (not a Chad ruling):
                                        // 0.5 measured one victim to 11 hp
                                        // and zero deaths in ten minutes —
                                        // AI fights should visibly resolve.

// T9a — inside the tunnel net? The SINGLE-SOURCE predicate shared by BOTH the
// crash-yield (tick() below: a null-ground world flying through the tunnel
// volume must not respawn) and the CAVECAM camera flag (main.cpp: skip the
// surface clamp so the eye stays on the plane inside the cavern). One helper so
// the two consumers can never fork. tunnels null / env null => false (the
// byte-identical altitude<=0 rule / the surface-flight clamp).
inline bool inside_tunnel(const sim::Environment* env, const glm::dvec3& pos) {
    return env != nullptr && env->tunnels != nullptr &&
           env->tunnels->contains(pos);
}

// RUNG E6.1 — BOOK ONE PLAYER DEATH, WITH ITS CAUSE.
//
// Called from BOTH app-side death-reset seams (the terrain-crash branch and the
// component-death branch) with the state as it stood BEFORE the reset. This is
// the whole of the E6.1 mechanism: a pure observation write into
// combat::CombatWorld's E6.1 ledger, which nothing but the tape ever reads.
//
// WHY IT EXISTS: `CombatWorld::deaths` was only ever incremented by the
// component branch, so a terrain crash left no record at all — measured on
// Chad's tape 2 as "died twice, zero 'pd' events". Both branches book here now.
//
// THE CAUSE COMES FROM STATE THE APP ALREADY HOLDS at the reset — the branch
// itself (crash vs component) plus the `last_damage_src` latch the two damage
// sites already write. NOTHING is plumbed through sim/ or control/ for this.
//
// ON "o2": the air fraction at the death point is RECORDED (death_air_frac)
// rather than turned into a cause, because this build has NO hypoxia/O2 damage
// mechanic at all — no code path anywhere reduces a component for lack of air.
// A vacuum death can therefore only ever present as a crash (mush -> terrain)
// and the recorded air fraction is what proves it. Inventing an "o2" cause
// would be a guess; the field is the honest instrument.
inline void book_player_death(combat::CombatWorld& cw,
                              combat::CombatWorld::DeathCause cause,
                              const sim::SimState& at,
                              const sim::AircraftParams& ap,
                              const sim::Environment* env) {
    ++cw.death_events;
    cw.death_cause = cause;
    cw.death_pos = at.position;
    cw.death_air_frac = sim::atm_frac_at(at.position, env, ap);
    cw.death_in_net = inside_tunnel(env, at.position);
    cw.death_damage = cw.damage;  // the component picture BEFORE reset_damage
    cw.last_damage_src = 0;       // a fresh life inherits no attribution
}

// The component-death branch's cause, derived from the damage-source latch.
inline combat::CombatWorld::DeathCause component_death_cause(
    const combat::CombatWorld& cw) {
    switch (cw.last_damage_src) {
        case 1: return combat::CombatWorld::DeathCause::kGun;
        case 2: return combat::CombatWorld::DeathCause::kGround;
        default: return combat::CombatWorld::DeathCause::kComponent;
    }
}

// S-aimff (v4 rung 1): the axis*angle rotation vector of a unit quaternion
// [rad, same frame as the quaternion]. Shortest arc (w >= 0 branch), total —
// an identity/near-identity delta returns exactly 0 (no normalize(0) NaN).
// Used to turn the apply_mouse quaternion delta into the aim's angular
// velocity for the aim-rate feedforward. Red-team P2-1: the shortest-arc
// fold means a per-frame rotation > pi (~1287 px/frame at 0.14 deg/px)
// reads as its < pi complement — ONE frame of anti-lead on an absurd
// flick, bounded by the ceilings/clamps downstream — accepted.
inline glm::dvec3 quat_rotation_vec(const glm::dquat& dq) {
    const glm::dquat q = dq.w < 0.0 ? -dq : dq;  // shortest arc
    const glm::dvec3 v{q.x, q.y, q.z};
    const double s = glm::length(v);
    if (s < 1e-12) return glm::dvec3{0.0};
    return (2.0 * std::atan2(s, q.w) / s) * v;
}

// The ORIENT verb's snap TARGET — the NOSE (v9 S-nosesnap, ruling ledger
// docs/DECISIONS.md @ mandalark-kernel b4c0751; supersedes the S7-nest D8
// guarded velocity). The S7-nest rationale — "the chase camera's rest is
// behind velocity, one composed settle" — died when the camera's rest target
// became the AIM ([camera] lead = 1.0), and Chad's ruling is direct: freelook
// welds aim := nose (the §5b nest, every freelook tick, keys or not), so the
// release snap is a NO-OP on the aim by construction — entry did the welding,
// release only re-establishes mouse authority. The old velocity target was
// term A of the v9 defect: a spurious ~AoA-sized aim/camera jump (measured
// 18.6 deg at aoa_max) at the exact instant the mouse takes over. The old
// ballistic/tail-slide guard is SUBSUMED, not violated — the nose was its
// fallback and the nose is now primary at every speed (dropping the now-unread
// cp parameter is a recorded future candidate, docs/DECISIONS.md, per the
// guard's strike — no simplification rides along with v9).
//
// Hoisted 2026-07-28 (S-relorient ADDENDUM) from the two verbatim copies at the
// release site and the double-tap site. Both orient triggers still call THIS —
// the "one verb, two triggers" redundancy Chad asked for is a property of the
// code, not of two paragraphs of comment that could drift apart again.
inline glm::dvec3 orient_snap_dir(const sim::SimState& s,
                                  const control::ControllerParams&) {
    return s.orientation * glm::dvec3{0.0, 0.0, -1.0};
}

// ★★★ L3 -- `spawn_state` MOVED to `app/spawn_policy.h`, unchanged to the
// character, and this header includes it -- so `app::spawn_state` still
// resolves for every caller and every test that ever called it. It went there
// because the two respawn sites below now route through `app::player_spawn`,
// the ONE function that places a player (plan §4.3): the aircraft half of that
// policy could not stay in the file that CALLS the policy.

// The per-tick mutable set the accumulator loop carries across ticks (main.cpp
// held these as loose locals). main.cpp legitimately writes `grounded` (the F1
// mode toggle) and `aim`/`fl` (focus loss) from OUTSIDE a tick — this is loop
// state, not a sealed invariant.
struct LoopState {
    sim::SimState prev{};
    sim::SimState curr{};
    // The spawn spec — where crash/death respawns are born (tunnel program,
    // Chad 2026-07-18: "spawn me over errington mine"). Defaults reproduce
    // the legacy +X-pole spawn BIT-identically (every test constructs
    // LoopState{}); main.cpp points it at the home base over Errington when
    // the tunnel world is enabled. Plain data — the tick's crash branch
    // stays deterministic/env-free.
    double spawn_alt = 2000.0;
    glm::dvec3 spawn_up{1.0, 0.0, 0.0};
    glm::dvec3 spawn_fwd{0.0, 0.0, -1.0};
    input::AimFrame aim{};
    input::Freelook fl{};
    input::HorizonRecovery recov{};  // S7-hrz open-loop roll latch (v13: the
                                     // REST-EDGE recovery's live latch — the
                                     // struct is no longer inert)
    // v13 rest-edge camera horizon recovery (pilot ruling 2026-08-06): the
    // hand-at-rest dwell accumulated while the mouse is still, and the
    // once-per-rest-edge capture latch. Both are caller-side gauge state — the
    // instructor cannot see either (the recovery only rolls the aim frame's UP
    // about its own forward). Cleared by any aim motion, by freelook, by
    // GROUNDED, and by focus loss.
    double aim_rest = 0.0;     // [s] mouse-still dwell, capped at the dial
    bool recov_armed = false;  // the debt for THIS rest edge was captured
    // v13d straightness gate state: last tick's velocity direction (zero =
    // invalid/reset — first valid tick measures no rate and cannot arm).
    // Flight-path state only; the camera law's legal read.
    glm::dvec3 prev_path{0.0, 0.0, 0.0};
    control::Internal internal{};
    glm::dvec3 prev_up{0.0, 0.0, 1.0};
    bool grounded = true;  // the first tick is a spawn tick (SPEC §9.5)
    // Monotone tick counter (little_planet_plan.md Stage 1, P0-1): the source
    // of TICK-DERIVED celestial time t_cel = tick_count * sim_dt (never
    // wall-clock — that would drift at 30 vs 240 fps, the S7-mouselevel/AT-9
    // class). Bumped once at the top of app::tick so BOTH callers (main.cpp and
    // the harness mirror) advance it in lockstep; MONOTONE across crash-reset /
    // F1 / raw mode — celestial time survives the plane's death, so the crash
    // branch below does NOT reset it. Frame-rate independent (per-tick, not
    // per-frame): AT-9 pins 30 fps and 240 fps to the same count.
    long long tick_count = 0;
    // R5e ESCAPE CLAIM (Chad's fly-4 ruling: "I want no return to be at
    // 4000m at 100m/s"): latched the first LIVE tick specific energy crosses
    // 0 with the GravityField active — the sky has the plane; the plant
    // flies NEUTRAL inputs (dead stick, throttle-down) until crash-respawn.
    // Physics alone cannot make the crossing absolute (air at the knee can
    // always bleed the surplus — fly-3: "I dove down and recovered"), so
    // the GAME enforces it here, at the same post-cascade seam the damage
    // model owns. Field null => never set (the frozen path is structural).
    bool escape_claimed = false;
};

// Per-tick inputs the caller resolves for THIS tick (rebuilt every tick from
// the frame-scope device sample, so a respawn mid-frame cannot leak the
// pre-crash stick/freelook into the fresh airframe). `aim_dx/aim_dy` are the
// per-FRAME mouse delta the caller OFFERS on the one consuming tick (0
// otherwise); the tick applies it to the aim iff the CQ2 gate allows
// (mouse_aim_live && !grounded), matching main.cpp — so the offer is harmless
// when freelook routes the same delta to the orbit (Freelook::step returns
// mouse_aim_live=false while held). Aim sensitivity is read from `cp` (single
// source), never carried here.
struct TickInput {
    bool raw_mode = false;
    sim::Inputs raw_in{};  // used iff raw_mode
    double throttle = 0.0;
    // MB-flaps: device passthrough alongside throttle (instructor mode; raw
    // mode carries them inside raw_in). Defaulted 0 = clean airframe, so
    // every pre-flap caller (goldens/mirror/tests) is bit-identical.
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    // R4g wheel brakes: passthrough alongside throttle (raw mode carries it
    // inside raw_in). Defaulted 0 = brake-free, every pre-brake caller
    // bit-identical.
    double wheel_brake = 0.0;
    bool freelook_held = false;
    // LMB fire (rig-D guns). Defaulted false => the harness/goldens/mirror
    // never set it, so every pre-guns caller is bit-identical (a strict
    // superset); consumed only when a GunWorld is threaded (below).
    bool fire_held = false;
    // S-orient (docs/comfort program Q3): the caller sets this TRUE on the tick
    // its input::OrientTap detector fired (a freelook double-tap within the
    // window). Defaulted false => bit-identical (the mirror-equivalence /
    // firewall path never sets it), a strict superset. The tick composes the
    // ORIENT verb: aim := NOSE (v9 S-nosesnap, b4c0751 — was guarded
    // velocity) and a reported camera-forward cut (res.orient_fired) the
    // caller hard-seats; the instant horizon righting rides the release edge.
    bool orient_cmd = false;
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    double aim_dx =
        0.0;  // per-frame mouse delta, offered on the consuming tick
    double aim_dy = 0.0;
    // RMB-zoom mouse-gain scale (2026-07-07): multiplies aim_sensitivity for
    // THIS tick's apply_mouse so a zoomed (magnified) view aims proportionally
    // slower — precision on the target. Defaulted 1.0 => bit-identical to the
    // pre-zoom tick (the harness/goldens/mirror never set it), a strict
    // superset. The caller sets it BINARY (zoom held ? kZoom/kChase : 1.0), a
    // pure function of the button state — NOT the eased fov — so it stays
    // frame-rate independent (an fov-eased gain would be the frame-quantized
    // signal driving the control-relevant aim that diverged S7-mouselevel /
    // AT-9). A scalar magnitude on the raw delta, not a basis smoothing (RA9).
    double aim_gain_scale = 1.0;
    // S-aimff (v4 rung 1): the frame's mouse-induced aim angular velocity
    // [rad/s, world], FORWARDED by step_frame to ticks 2..N of a multi-tick
    // frame (the smear/ZOH — mouse deltas arrive per FRAME and land on the
    // consuming tick; a naive rotation/sim_dt there is a frame-rate-dependent
    // spike train, the AT-9 / S7-mouselevel ban). On the CONSUMING tick
    // app::tick IGNORES this field and computes the rate itself from the
    // rotation apply_mouse actually applied, spread over frame_ticks:
    // rate = rotation/(frame_ticks*sim_dt), so the FF demand INTEGRAL equals
    // K_ff*(frame rotation) at any frame rate. Defaults (0, 1) => every
    // direct caller is per-tick exact and bit-identical while the mouse is
    // still.
    glm::dvec3 aim_rate_ff{0.0};
    int frame_ticks = 1;  // N of the frame this tick belongs to (step_frame)
    // Vestigial (S7-raw removed the F1 screen-relative mouse): the mouse now
    // rotates the RAW carried aim frame (§9.1), so app::tick no longer reads
    // cam_fwd/cam_up. Retained only so the caller's FrameInput->TickInput
    // forwarding stays uniform; the camera-render path uses main.cpp's OWN
    // cam_fwd/cam_up, not these. Safe to remove with the 5-arg apply_mouse.
    glm::dvec3 cam_fwd{0.0, 0.0, 0.0};
    glm::dvec3 cam_up{0.0, 0.0, 0.0};
};

struct TickResult {
    control::Telemetry telem{};  // instructor mode only (default in raw)
    sim::Inputs inputs{};        // the emitted Inputs (mirror of last_inputs)
    bool respawned = false;      // the crash predicate fired THIS tick
    // ★★★ L6 — THE LAST LIFE (Chad's ruling R9, 2026-09-03: "once the match
    // clock is initiated once, even if paused, then there are no new respawns
    // from either side"). TRUE on the tick a player death was REFUSED a
    // respawn: no aeroplane was placed, `respawned` stays FALSE (nothing was
    // reborn, so no life is spent and no spawn menu is offered), and the match
    // has just been resolved. Report-only, for the HUD note.
    bool respawn_refused = false;
    // R4-FLY-6 touchdown report (Chad: "something to indicate touchdown"):
    // TRUE on the airborne -> GROUNDED capture edge, with the CG position +
    // |velocity| at that tick (≈ ground speed; the ≤ max_sink radial part
    // is <1% at landing speeds). A frame runs several ticks — the caller
    // spawns the render burst at the reported point, not frame-end state.
    // Pure report fields: read state, never write it.
    bool touchdown = false;
    glm::dvec3 touchdown_pos{0.0};
    double touchdown_speed = 0.0;
    // R5e: the sky holds the plane this tick (E_spec crossed 0 with the
    // GravityField live — LoopState.escape_claimed mirrored out for the
    // HUD's NO RETURN plate). Report field: read, never write.
    bool escape_claimed = false;
    // S-orient (docs/comfort program Q3): the ORIENT verb fired THIS tick — the
    // CALLER re-seats its lagged cam_fwd := st.aim.forward() instantly (a hard
    // angular cut; position/FOV keep their smoothing — research REC-1). Wired
    // in app/main.cpp AND harness::MiniCamera so the instrument measures the
    // law the app ships. Default false => no cut (bit-identical firewall path).
    bool orient_fired = false;
    // S-aimff: the aim rate control::step SAW this tick (== the value fed to
    // control::Input::aim_rate_world). The consuming tick reports its
    // computed rate here so step_frame can forward THE SAME value into ticks
    // 2..N of the frame (the smear); also the moved-consumer test's probe.
    glm::dvec3 aim_rate_ff{0.0};
};

// The optional per-tick drone bundle threaded through app::tick / step_frame by
// pointer, DEFAULTED to nullptr (SPEC §0 S8-drone). nullptr => the drone path
// is skipped and the tick is BIT-IDENTICAL to the pre-drone tick — the
// firewall that keeps the player's flight-model / controller golden unmoved,
// pinned by the existing mirror-equivalence test (test_instructor_tick.cpp,
// which calls tick/step_frame with no dw). The drone advances once per PLAYER
// tick (frame-rate independent, in lockstep — the single-accumulator interleave
// that keeps the future TOT meter frame-rate independent, avoiding the
// two-copies-seam trap).
struct DroneWorld {
    std::vector<drone::DroneState> drones{};
    drone::DroneParams dparams{};
    render::GunsightParams gparams{};
    render::OnTargetMeter
        meter{};  // the player's lead/time-on-target instrument
};

// The engaged-target selection (extracted so the HYSTERESIS is pinnable
// open-loop — the S6 "drive the latch on an oscillating fixed state"
// discipline; Fable P1-2). Returns the fleet slot of the engaged bandit, or -1.
// A bandit qualifies if within the forward cone AND track_range; the
// currently-latched bandit `prev` qualifies in a WIDER cone (+0.08 cos)
// + 1.1*range and KEEPS engagement unless a challenger is materially closer (<
// 0.85*its range) — so the pipper does not strobe between crisscrossing
// bandits. PURE.
inline int select_engaged_target(int prev,
                                 const std::vector<drone::DroneState>& drones,
                                 const glm::dvec3& shooter_pos,
                                 const glm::dvec3& nose,
                                 const render::GunsightParams& gp) {
    int best = -1;
    double best_r = 1e300;
    bool prev_ok = false;
    double prev_r = 1e300;
    for (std::size_t i = 0; i < drones.size(); ++i) {
        if (drones[i].inert) continue;  // a conquest wreck is not a target
        const glm::dvec3 to = drones[i].curr.position - shooter_pos;
        const double r = glm::length(to);
        if (r < 1e-6) continue;
        const bool latched = static_cast<int>(i) == prev;
        const double cone =
            latched ? gp.track_cone_cos - 0.08 : gp.track_cone_cos;
        const double range_lim =
            latched ? gp.track_range * 1.1 : gp.track_range;
        if (glm::dot(to / r, nose) <= cone || r > range_lim) continue;
        if (latched) {
            prev_ok = true;
            prev_r = r;
        }
        if (r < best_r) {
            best_r = r;
            best = static_cast<int>(i);
        }
    }
    if (prev_ok && (best < 0 || best_r >= 0.85 * prev_r)) return prev;
    return best;
}

// One fixed-dt tick (SPEC §9.5 sequence), both modes. Mutates `st`. Mirrors
// harness::ClosedLoop::tick tick-for-tick (transport -> GROUNDED pairing ->
// ★★★ S4 — THE TELEPORT IS DELETED (Chad's ruling, 2026-08-26).
// -----------------------------
//
// VERBATIM: "this games ai must not teleport but become skilled at deck
// flying. That is the answer" / "the dome being gone means they have to ride
// the deck they should have to respawn, but if they crash due to the moment of
// air loss and their context of orientation and speed, well then they respawn
// from their zone at the deck".
//
// WHAT USED TO BE HERE. A "SCRAMBLE ON COLLAPSE" hook (2026-07-26) that, the
// moment a faction's dome reached 0, REWROTE d.curr for every living plane of
// that faction into the SURVIVING faction's dome. Measured on tape 12: all 7
// enemies jumped 13,163-32,513 m inside ONE 0.2 s sample gap, every one landing
// at 2000 m inside the player's bubble. Same event on tape 10. There was a
// second, quieter copy of the same rule in the crash-respawn fallback below
// ("own dome gone -> the surviving faction's dome"). BOTH ARE GONE.
//
// THE THREE RULES THAT REPLACE IT:
//  1. NO RESCUE AT THE COLLAPSE TICK. Whoever is airborne when the dome dies
//     keeps exactly the air, orientation and speed they had. If that kills
//     them, it kills them. Chad has accepted this explicitly. There is no
//     grace period, no fade, no one-time nudge -- and no hook here at all.
//  2. RESPAWN IN THEIR OWN ZONE, AT THE DECK. A crash respawn for a pilot
//     whose own dome is gone is planted over his OWN faction's home ground at
//     deck_track_agl_m AGL -- inside the 60/110 m deck band, under the 120 m
//     full-air deck lid, which is exactly the altitude the S1-DECK track law
//     holds. Never in the survivor's dome.
//  3. THEN HE RIDES THE DECK like any other aeroplane: no special-casing of a
//     collapsed faction's behaviour anywhere downstream.
//
// WHY THIS IS BUILDABLE NOW AND WAS NOT IN JULY. The scramble's own rationale
// (docs/ai_vacuum_strand.md, FIX-F2) was that a plane placed in deleted air has
// no lift and no thrust with which to execute any ORDER, so only a state move
// could rescue it. That is still true of a placement at 2000 m in vacuum. It is
// NOT true of a placement at 100 m AGL: the global terrain-hugging deck holds
// full air below 120 m AGL everywhere on the planet, dome or no dome, and
// S1-DECK (deck band 60/110, forward terrain arming, deck_track) is the law
// that lets an aeroplane stay in it. The respawn lands IN breathable air, so
// there is nothing to rescue it from.
// ⚠ AND IT RESTS ON ONE SHIPPED WORD: `deck_terrain_relative = true`
// (config/game.toml:172). With it FALSE the deck is measured from the SPHERE,
// which on the real DEM is UNDERGROUND over 69% of the surface — a 100 m-AGL
// respawn would then land in vacuum over most of the planet. MEASURED, not
// argued: E12's tape-7 arm pins that dial to 0, and on that arm alone the S4
// respawn moved enemy crashes 20 -> 36 in 22.4 min (0.89 -> 1.61/min) while
// the SHIPPED arm did not move at all (10, 0.45/min, both sides of the
// change). If that word is ever walked back, this placement must be walked
// back with it.
// THE SHARED PLACEMENT LAW (FIX-F2, docs/ai_phase2_fix_spec.md): the exact
// per-drone bearing math, factored out into ONE routine so the crash-respawn
// (the vacuum respawn-pen fix, below), the E5 reinforcement wave and the S4
// deck respawn can never drift apart into three placement laws. Plants `d` at
// the golden-angle bearing for `slot`, 0.6 of the semi-minor axis from the
// dome centre (inside the ellipse in every bearing), nose pointed at the dome
// centre, at the fleet's own cruise speed (level_state_at's dp.speed reseed).
// Deterministic, no rng/clock: `slot` is the drone's spawn_index, exactly like
// spawn_state's Fibonacci scatter.
//
// Returns false and leaves `d` untouched if `faction` has no air right now:
// either the ellipse itself is dead (a<=0 or b<=0 after growth/shrink), OR
// (red-team P1-2, docs/ai_phase2_fix_spec.md) the dome's own CEILING cannot
// hold a placement — `world::faction_ellipse`/`radius_scale` alone say
// nothing about height, so a dome that kept its footprint but shrank its
// ceiling (radius_scale and ceiling_scale are INDEPENDENT fields; e.g.
// ceiling_scale=0.25 -> ceiling_m = 4000*0.25 = 1000 m, reachable under the
// shipped [conquest] growth/shrink fracs) was placing every relocated drone
// at the OLD fixed 2000 m AGL regardless -- probe-measured atm_frac 0.000 at
// that exact point: the vacuum respawn-pen FIX-F2 exists to end, reproduced
// deterministically. `ceiling_m`/`ceil_soft_m` are the LIVE, growth-scaled
// dome ceiling values (callers pass `grow[f].ceiling_scale *
// cq.bubble_ceiling_m` and `cq.bubble_ceil_soft_m` -- both already on
// ConquestWorld, so no new config surface). Below 500 m there is no sane
// placement altitude left (the caller decides the fallback: since S4 that is
// the OWN-ZONE DECK respawn, never another faction's dome) -- above it, the
// placement altitude is clamped INSIDE the dome's own ceiling with a
// ceil_soft_m + 200 m margin (never above it, atm_falloff's fade band sits
// ABOVE ceiling_m, so alt <= ceiling_m is unconditionally full air): a
// full-height dome (ceiling_m >= 2000+ceil_soft_m+200) clamps to the OLD
// 2000 m exactly (bit-identical to pre-P1-2 behavior); a shrunken dome
// places just under its own soft band instead of blind at 2000.
// ---------------------------------------------------------------------------
// ★★★ S2-TUNNEL — THE ROUTE LEGALITY MEASUREMENT (Chad's ruling 2026-08-26:
// "no more crossing over the thin air zone ... I would prefer actually if the
// ai were more conservative than say a player might be and that they would
// choose to cross via the tunnel").
//
// `uncovered_arc_m` returns how many metres of the great-circle track from
// `from` to `to` lie OUTSIDE BOTH faction domes — i.e. how much of the crossing
// is the thin air that kills. Pure, deterministic, rng-free.
//
// ★ IT READS world::ellipse_r_eff, the SAME air edge the containment leash and
// the plant read (drone/drone.h apply_bubble_leash). Never a copy of the
// geometry: a route legality test that forked the ellipse would go quietly
// wrong the first time a dome grew or shrank — the law this ladder has paid
// for eight times.
//
// ⚠ IT DELIBERATELY IGNORES THE DECK. The global terrain-hugging deck holds air
// everywhere, so a sampler that asked "is there air here" at deck altitude
// would answer YES along every route and the router would never fire. The
// question this asks is the one Chad asked: does the CRUISE track cross the
// thin-air zone. Deck survival is S1-DECK's job and is what keeps the
// designated deck runners alive.
inline double uncovered_arc_m(const glm::dvec3& from, const glm::dvec3& to,
                              const world::FactionGrowth grow[2], double R,
                              int samples = 64) {
    const double lf = glm::length(from), lt = glm::length(to);
    if (!(lf > 1e-6 && lt > 1e-6) || samples < 2) return 0.0;
    const glm::dvec3 a_dir = from / lf, b_dir = to / lt;
    const double cosang = std::clamp(glm::dot(a_dir, b_dir), -1.0, 1.0);
    const double ang = std::acos(cosang);
    const double total_arc = R * ang;
    if (total_arc <= 1.0) return 0.0;

    // The two live ellipses, taken ONCE (a per-sample re-derivation would be
    // the same numbers at 64x the cost).
    glm::dvec3 cdir[2]{}, maj[2]{};
    double ea[2]{0.0, 0.0}, eb[2]{0.0, 0.0};
    for (int f = 0; f < 2; ++f)
        world::faction_ellipse(f, grow, cdir[f], maj[f], ea[f], eb[f]);

    // Slerp the direction; a degenerate (antipodal / coincident) pair falls
    // back to the endpoints, which is the conservative answer.
    const double sn = std::sin(ang);
    int outside = 0;
    for (int i = 0; i < samples; ++i) {
        const double t = (static_cast<double>(i) + 0.5) /
                         static_cast<double>(samples);
        glm::dvec3 d;
        if (sn > 1e-9) {
            d = (std::sin((1.0 - t) * ang) * a_dir + std::sin(t * ang) * b_dir) /
                sn;
        } else {
            d = a_dir;
        }
        const double dl = glm::length(d);
        if (dl < 1e-9) continue;
        d /= dl;
        bool covered = false;
        for (int f = 0; f < 2 && !covered; ++f) {
            if (!(ea[f] > 0.0 && eb[f] > 0.0)) continue;  // a dead dome covers
                                                          // nothing
            const double reff =
                world::ellipse_r_eff(cdir[f], maj[f], ea[f], eb[f], d);
            if (reff <= 1e-6) continue;
            const double arc =
                R * std::acos(std::clamp(glm::dot(d, glm::normalize(cdir[f])),
                                         -1.0, 1.0));
            if (arc <= reff) covered = true;
        }
        if (!covered) ++outside;
    }
    return total_arc * (static_cast<double>(outside) /
                        static_cast<double>(samples));
}

// S2-TUNNEL — this pilot's rank inside his own faction's raider order.
// ★ ONE RANKING AUTHORITY: this is combat::faction_raider's own `out_rank`
// loop, exposed rather than re-implemented, so "who is a raider" and "who is a
// designated DECK runner" can never disagree. Deterministic, live-masked,
// rng-free; tie-break is LOWER index, as everywhere else in the tree.
// ★ L12: no player_faction term -- the SET of same-wing pilots is invariant
// to which side the player picked (combat/conquest.h roster_sizes only swaps
// the LABELS), so this ranking is unchanged by the menu.
inline int raider_rank(int spawn_index, const bool* live) {
    const int idx = ((spawn_index % combat::kNumMavericks) +
                     combat::kNumMavericks) %
                    combat::kNumMavericks;
    const int own = combat::maverick_faction(idx);
    const double my_agg = maverick::traits_for(idx).aggression;
    int out_rank = 0;
    for (int i = 0; i < combat::kNumMavericks; ++i) {
        if (i == idx || combat::maverick_faction(i) != own) continue;
        if (live != nullptr && !live[i]) continue;
        const double a = maverick::traits_for(i).aggression;
        if (a > my_agg || (a == my_agg && i < idx)) ++out_rank;
    }
    return out_rank;
}

// THE BEARING HALF, shared by all three callers. Plants `d` at the golden-angle
// bearing for `slot`, `arc_m` of ground track from the ellipse centre `cdir`,
// nose pointed back at the centre. `ground` decides what `alt_m` is measured
// FROM: null => the render shell (ap.R + alt_m, the dome placement, which has
// always been a shell altitude); non-null => the terrain under the chosen
// bearing (radius_at + alt_m, i.e. a true AGL — the S4 deck respawn). ONE
// bearing law, two altitude references, no third copy.
inline void plant_at_slot(drone::DroneState& d, const glm::dvec3& cdir,
                          const glm::dvec3& maj, double arc_m, int slot,
                          const sim::AircraftParams& ap, const DroneWorld& dw,
                          double alt_m, const sim::Environment* env) {
    // Tangent basis at the dome's centre (major axis defensively
    // re-orthogonalized, same discipline as the atm_frac_at sampler).
    const glm::dvec3 c = glm::normalize(cdir);
    glm::dvec3 e1 = maj - c * glm::dot(maj, c);
    if (glm::length(e1) < 1e-9) {  // degenerate axis: any tangent will do
        const glm::dvec3 probe = std::abs(c.x) < 0.9
                                     ? glm::dvec3{1.0, 0.0, 0.0}
                                     : glm::dvec3{0.0, 1.0, 0.0};
        e1 = probe - c * glm::dot(probe, c);
    }
    e1 = glm::normalize(e1);
    const glm::dvec3 e2 = glm::normalize(glm::cross(c, e1));

    const double golden = 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
    const double ang = arc_m / ap.R;  // arc length -> central angle
    const double th = golden * static_cast<double>(slot);
    const glm::dvec3 t = e1 * std::cos(th) + e2 * std::sin(th);
    const glm::dvec3 pdir = c * std::cos(ang) + t * std::sin(ang);
    // ★ T2b (red-team P1-1): AGL is measured off sim::air_ground_radius, the
    // kernel's own contact surface, so a deck respawn cannot plant an aeroplane
    // INSIDE a hill the eye is shown (the deck AGL is ~90 m and the facet gap
    // reaches 66.7 m at Onaping). env/ground absent => shell altitude at ap.R,
    // unchanged from the pre-T2b body, bit for bit.
    const double r_base = (env != nullptr && env->ground != nullptr)
                              ? sim::air_ground_radius(*env, pdir)
                              : ap.R;
    const glm::dvec3 pos = pdir * (r_base + alt_m);
    // Great-circle tangent at pos pointing back at the dome centre.
    glm::dvec3 fwd = c - pdir * glm::dot(c, pdir);
    fwd = glm::length(fwd) > 1e-9 ? glm::normalize(fwd) : t;
    d.curr = drone::level_state_at(dw.dparams, pos, fwd);
    d.prev = d.curr;          // no interpolation streak across the jump
    d.leash_engaged = false;  // the containment latch re-arms clean
}

inline bool place_in_faction_air(drone::DroneState& d, int faction,
                                 const world::FactionGrowth grow[2],
                                 const sim::AircraftParams& ap,
                                 const DroneWorld& dw, int slot,
                                 double ceiling_m, double ceil_soft_m) {
    glm::dvec3 cdir{0.0, 1.0, 0.0};
    glm::dvec3 maj{0.0};
    double a = 0.0;
    double b = 0.0;
    world::faction_ellipse(faction, grow, cdir, maj, a, b);
    if (!(a > 0.0 && b > 0.0)) return false;  // no air in that dome
    if (ceiling_m < 500.0) return false;      // dome too flat to breathe in

    // Placement altitude: inside the dome's own ceiling with margin, never
    // above dw.dparams.spawn_alt's old fixed 2000 (the P1-2 fix).
    const double alt_m =
        std::clamp(ceiling_m - ceil_soft_m - 200.0, 300.0, 2000.0);
    // 0.6 * b: inside the ellipse in EVERY bearing. Shell altitude (ground
    // null) — unchanged from the pre-S4 body, bit for bit.
    plant_at_slot(d, cdir, maj, 0.6 * b, slot, ap, dw, alt_m, nullptr);
    return true;
}

// ★★★ S4 — THE OWN-ZONE DECK RESPAWN (Chad's ruling 2026-08-26: "they respawn
// from their zone at the deck").
//
// A pilot whose own dome is GONE has no faction air to be planted in, so
// place_in_faction_air refuses by construction (a == b == 0 at radius_scale 0).
// This is what he gets instead: his OWN faction's HOME GROUND, at deck
// altitude, in the breathable deck that covers the whole planet.
//
// THE ZONE is the faction's BAKED BASELINE ellipse (world/faction_bubbles.h's
// kValley*/kSudbury* constants, read through world::faction_ellipse with an
// unscaled FactionGrowth) — the territory, which is a fixed piece of geography
// and does not die with the dome. Never the live scaled ellipse: at
// radius_scale 0 that is a point, and at 0.5 it would crowd the whole wing onto
// half the ground for no reason.
//
// THE ALTITUDE is dp.deck_track_agl_m — the SAME AGL the S1-DECK track law
// holds (drone/drone.h:989), loader-pinned into (deck_avoid_agl_enter_m,
// deck_avoid_agl_release_m] = (60, 110] at config/load_scenario.cpp:1290-1294,
// and under the 120 m deck_agl_m full-air lid (config/game.toml). Not a new
// dial and not a guess: it is the altitude the deck law flies TO, so a plane
// planted there is already trimmed on the band it will be held on.
//
// Refuses (returns false, leaving `d` on the stock respawn) only when there is
// no terrain to measure AGL from — a null env/ground, i.e. a fixture with no
// world. With a live heightfield it always succeeds: the deck has no dome war
// to lose.
inline bool place_on_faction_deck(drone::DroneState& d, int faction,
                                  const sim::AircraftParams& ap,
                                  const DroneWorld& dw,
                                  const sim::Environment* env, int slot) {
    if (env == nullptr || env->ground == nullptr) return false;
    const double agl_m = dw.dparams.deck_track_agl_m;
    if (!(agl_m > 0.0)) return false;
    const world::FactionGrowth base[2]{};  // 1.0/1.0 = the BAKED zone
    glm::dvec3 cdir{0.0, 1.0, 0.0};
    glm::dvec3 maj{0.0};
    double a = 0.0;
    double b = 0.0;
    world::faction_ellipse(faction, base, cdir, maj, a, b);
    if (!(a > 0.0 && b > 0.0)) return false;
    plant_at_slot(d, cdir, maj, 0.6 * b, slot, ap, dw, agl_m, env);
    return true;
}

// RUNG E5 — THE SHIPPED AIRBORNE WAVE POLICY (docs/ENEMY_AI_E1_E2_SPEC.md).
// A reinforced pilot launches AIRBORNE from its OWN faction's home air, reusing
// the SHARED PLACEMENT LAW above (place_in_faction_air / level_state_at) — the
// same golden-angle slot, the same ceiling-aware altitude clamp, the same
// cruise-speed reseed the crash-respawn relocation and the scramble use. One
// placement law, three callers, no drift.
//
// REFUSES (returns false, holding the wreck where it fell) when the pilot's own
// faction has NO BREATHABLE AIR LEFT — a dome shrunk to nothing by pump deaths.
// That refusal is deliberate and load-bearing for "the war stays winnable":
// losing your last pump ends your reinforcements, so a crushed faction cannot
// refill the sky forever while the sudden-death clock runs on it. Note there is
// NO fallback into the surviving faction's dome here (unlike the FIX-F2 crash
// relocation, where the alternative is a drone auguring in forever): a wave has
// the option of simply not coming.
//
// ★ SUBSTITUTABLE — see combat/reinforce.h's millwright seam and
// ConquestWorld::reinforce_policy.
struct AirborneWavePolicy : combat::ReinforcePolicy {
    ConquestWorld* cq = nullptr;
    DroneWorld* dw = nullptr;
    const sim::AircraftParams* ap = nullptr;

    bool deploy(drone::DroneState& d, int spawn_index) override {
        if (cq == nullptr || dw == nullptr || ap == nullptr) return false;
        world::FactionGrowth grow[2];
        app::faction_growth_from_state(cq->state, grow);
        const int own = combat::maverick_faction(spawn_index,
                                                 cq->state.player_faction);
        return place_in_faction_air(
            d, own, grow, *ap, *dw, spawn_index,
            grow[own].ceiling_scale * cq->bubble_ceiling_m,
            cq->bubble_ceil_soft_m);
    }
};

// RUNG E6.6 — the wave params, built in ONE place so the tick's
// reinforcements_live stamp and the wave machine itself can never disagree
// about the pool.
inline combat::ReinforceParams wave_params(const ConquestWorld& cq,
                                           double hp_full) {
    combat::ReinforceParams rfp;
    rfp.delay_s = cq.params.reinforce_delay_s;
    rfp.restores_roster = cq.params.reinforce_restores_roster;
    rfp.pool_n = cq.params.reinforce_pool_n;
    rfp.hp_full = hp_full;
    return rfp;
}

// RUNG E6.6 — CAN THE ENEMY WING STILL BE REFILLED?
//
// This is exactly what ConquestState::reinforcements_live has always meant
// operationally: the veto on victory-by-wipe. Rung E5 approximated it with
// "waves are switched on at all", which was correct while the pool was
// infinite. With a finite pool the honest test is whether the faction whose
// wipe would END the match — the player's enemy, the only side
// conquest_eval_outcome's wipe route inspects — has any revives left. When its
// pool is spent the veto lifts and the wipe route re-arms EXACTLY as written
// (nothing in conquest.h changes). pool_n < 0 reproduces the E5 stamp.
// Red-team P2-2, CONFIRMED ON TAPE 3 (Chad killed the whole wing after
// their dome was crushed at 9:37 with one wave still pooled -- every deploy
// refused forever while this veto stayed true, and the wipe VICTORY never
// latched): "can still be refilled" must mean CAN ACTUALLY DEPLOY. This
// mirrors place_in_faction_air's own refusal set (degenerate ellipse / dome
// too flat to breathe in) so the veto and the wave machine cannot disagree.
inline bool faction_air_breathable(const ConquestWorld& cq, int faction) {
    world::FactionGrowth grow[2];
    app::faction_growth_from_state(cq.state, grow);
    glm::dvec3 cdir{0.0, 1.0, 0.0};
    glm::dvec3 maj{0.0};
    double a = 0.0;
    double b = 0.0;
    world::faction_ellipse(faction, grow, cdir, maj, a, b);
    if (!(a > 0.0 && b > 0.0)) return false;
    return grow[faction].ceiling_scale * cq.bubble_ceiling_m >= 500.0;
}

inline bool enemy_waves_live(const ConquestWorld& cq) {
    const combat::ReinforceParams rfp = wave_params(cq, 0.0);
    const int ef = 1 - cq.state.player_faction;
    // ★★★ L6: faction_can_reinforce now answers the RESPAWN LOCK first (Chad's
    // R9), so a locked match stamps reinforcements_live FALSE and the
    // victory-by-wipe route re-arms EXACTLY as written. Without that the veto
    // would still read "breathable dome + pooled revives" after an R8 repair
    // and hold a won match open on waves that can never come — the tape-3
    // defect this whole predicate exists to end, one rung later.
    return combat::faction_can_reinforce(cq.reinforce, rfp, cq.state, ef) &&
           faction_air_breathable(cq, ef);
}

// [raw: sim::step] | [instructor: freelook -> mouse-aim -> control::step ->
// sim::step] -> crash predicate -> respawn), the SAME shared pieces, so the two
// callers can't drift (pinned by the mirror-equivalence test).
// `env` is the nullable world (R4, the R3-review P1): NO default argument, so
// the compiler enumerates every call site — a wrapper that silently kept
// nullptr once `ground` went live would fork the crash surface (the player
// crashes on terrain, drones/harness fly through). null => bit-identical v3.
inline TickResult tick(LoopState& st, const TickInput& in,
                       const sim::AircraftParams& ap,
                       const control::ControllerParams& cp,
                       const sim::Environment* env, DroneWorld* dw = nullptr,
                       weapon::GunWorld* gw = nullptr,
                       combat::CombatWorld* cw = nullptr,
                       render::TunnelLampWorld* lw = nullptr,
                       ConquestWorld* cq = nullptr,
                       FlakWorld* fk = nullptr,
                       std::vector<FlakAiGun>* fk_ai = nullptr,
                       StingWorld* sw = nullptr) {
    TickResult res;

    // Celestial time source (Stage 1, P0-1): one bump per tick, at the top so
    // it counts EVERY tick in both modes and both callers, and is never reset
    // by the crash branch below (monotone across death — the sky keeps
    // wheeling).
    ++st.tick_count;

    st.prev = st.curr;
    const glm::dvec3 up = sim::local_up(st.curr.position);
    st.aim.transport(st.prev_up, up);  // every tick, every mode (SPEC §9.1)
    st.prev_up = up;
    // The mouse aim is PURE RAW (§9.1, S7-raw): transport + apply_mouse on the
    // frame's OWN carried up/right, nothing camera-referenced, nothing eased.
    // (An S7-mouselevel "gentle roll-to-local_up" was tried to fix the mouse
    // feeling inverted after a half-loop, but ANY easing of the mouse basis
    // curls an active sweep and is a §9.1-banned smoothing on the mouse->aim
    // path — Chad's standing rule "no auto-leveling, chase the raw aim".
    // REMOVED. The post-maneuver "up-is-down" WAS the accepted trade of the
    // raw frame; S7-hrz below now rights it — but only as an OPEN-LOOP
    // fixed-angle roll on a freelook release (a player action, never
    // autonomous), which is why it is not the S7-mouselevel dead-end:
    // docs/horizon_recovery_plan.md.)

    // Capture the spawn/reset status BEFORE the crash branch below clears
    // st.grounded (line ~348): the guns block at the tail gates on this so a
    // held trigger fires NO round on the GROUNDED reset tick either (the crash
    // tick is caught by st.grounded going true; the FOLLOWING reset tick has
    // st.grounded already cleared by the time the guns run — Fable C8). Keeps
    // the "spawn/reset ticks are inert" discipline exact across both ticks.
    const bool grounded_tick = st.grounded;

    // GROUNDED pairing (SPEC §9.5): the in-core reset paired with the
    // caller-side aim/freelook reset, exactly as ClosedLoop does.
    if (st.grounded) {
        st.fl.reset();
        st.recov.reset();        // a respawn never inherits a dead life's roll
        st.aim_rest = 0.0;       // nor its hand-at-rest dwell (v13)
        st.recov_armed = false;  // nor its rest-edge capture latch
        st.prev_path = glm::dvec3{0.0};  // nor its path-rate memory (v13d)
        st.aim.reseed(st.curr.orientation, up);
        st.internal = control::reset();
        st.escape_claimed = false;  // R5e: a fresh life is unclaimed
    }

    // Component damage (docs/damage_model_plan.md, Fable-vetted): a PURE
    // transform on the frozen kernel's params (+ a roll bias on the commanded
    // Inputs) — the plane flies WORSE because it is handed different numbers,
    // no kernel edit. The SAME damaged AircraftParams feeds control::step AND
    // sim::step (Fable P0-3: a mismatch is a gain amplifier); the controller
    // inverts the true degraded plant (H1 intact) while pilot damage separately
    // degrades the cascade gains. Gated on cw && !damage_zero ⇒ zero damage is
    // the BIT-IDENTICAL frozen path (goldens + firewall memcmp unmoved). The
    // roll bias is load-proportional (Fable P0-2) and applied only on LIVE
    // ticks (P0-4: never on a grounded/respawn tick).
    const sim::AircraftParams* ap_use = &ap;
    const control::ControllerParams* cp_use = &cp;
    sim::AircraftParams ap_dmg;
    control::ControllerParams cp_dmg;
    double dmg_roll_bias = 0.0;
    if (cw != nullptr && !combat::damage_zero(cw->damage)) {
        ap_dmg =
            combat::apply_damage_aircraft(ap, cw->damage, cw->damage_params);
        cp_dmg =
            combat::apply_damage_controller(cp, cw->damage, cw->damage_params);
        ap_use = &ap_dmg;
        cp_use = &cp_dmg;
        if (!st.grounded) {
            // Load factor n under the DAMAGED params, for the untrimmable roll
            // bias.
            const glm::dvec3 vb = sim::body_dir_of(
                st.curr.orientation, sim::current_vhat(st.curr, ap_dmg));
            // R6: SPATIAL n (position+env) — the damaged-plane roll bias
            // scales with the TRUE lift the plant flies here (null env => the
            // identical altitude n, bit-for-bit).
            const double n = sim::load_factor(
                sim::alpha_of(vb), glm::length(st.curr.velocity),
                st.curr.position, env, st.curr.flap, ap_dmg);
            dmg_roll_bias = combat::roll_bias(cw->damage, cw->damage_params, n);
        }
    }
    // Add the asymmetric-wing roll bias to the FINAL commanded Inputs
    // (post-cascade, pre-plant) and clamp — the plant flies the biased stick,
    // the controller fights it through the same axis (Fable Q1). STRUCTURALLY
    // gated on a nonzero bias (not an `x + 0.0f` FP identity — that flips -0.0f
    // -> +0.0f and would break the zero-damage bit-identity the whole firewall
    // rests on; Fable impl-review P0-A). dmg_roll_bias is nonzero only when cw
    // && !damage_zero && !grounded, so this is a true structural no-op on every
    // frozen-path tick.
    const auto with_bias = [&](sim::Inputs cmd) {
        if (dmg_roll_bias != 0.0)
            cmd.roll = std::clamp(cmd.roll + static_cast<float>(dmg_roll_bias),
                                  -1.0f, 1.0f);
        return cmd;
    };

    // R5e ESCAPE CLAIM: with the field LIVE, the first live tick whose entry
    // state carries E_spec > 0 latches the claim; the branches below then
    // hand the plant NEUTRAL Inputs{} — no elevator to dive with, throttle
    // slewing down — so "NO RETURN" is enforced, not predicted. Cleared on
    // the respawn tick (above) and whenever the field is off (the T debug
    // rescue frees the plane along with restoring gravity). env/grav null =>
    // this whole block is a structural no-op: the frozen path cannot claim.
    if (env != nullptr && env->grav != nullptr) {
        if (!st.grounded &&
            sim::specific_energy(glm::length(st.curr.velocity),
                                 sim::altitude(st.curr.position, *ap_use), env,
                                 *ap_use) > 0.0) {
            st.escape_claimed = true;
        }
    } else {
        st.escape_claimed = false;
    }
    res.escape_claimed = st.escape_claimed;

    if (in.raw_mode) {
        const sim::Inputs cmd =
            st.escape_claimed ? sim::Inputs{} : with_bias(in.raw_in);
        res.inputs =
            cmd;  // post-bias: the stick the plant actually flew (P1-3)
        st.curr = sim::step(st.curr, cmd, *ap_use, env, ap.sim_dt);
    } else {
        const bool any_ovr =
            in.override_mask[0] || in.override_mask[1] || in.override_mask[2];
        const input::Freelook::Step fs = st.fl.step(
            in.freelook_held, any_ovr, ap.sim_dt, cp.freelook_easeback_time);
        // §5b aim NESTING — the WELD (v9 S-nosesnap, ruling ledger
        // docs/DECISIONS.md @ b4c0751; supersedes S7-nest D7/D8's keys-held
        // scope and retires the MB-lean-era no-keys "freelook carves a held
        // lateral aim" side scope):
        //  - While freelook is held — KEYS OR NOT — the aim RIDES THE NOSE
        //    per tick ("the nose aim becomes welded to the nose
        //    directionality", Chad 2026-07-29). Entry does the welding: the
        //    pre-freelook mouse command stops driving the plane at the
        //    spacebar press, the nose holds its heading, and qweasd is the
        //    only control while looking. The mouse never feeds the aim during
        //    freelook, so nothing here touches the raw mouse->aim path.
        //  - The RELEASE snap therefore lands on the NOSE (orient_snap_dir
        //    above) and is a NO-OP on the aim by construction — it re-aligns
        //    to the current-tick nose (sub-degree: one tick of rotation since
        //    the last welded tick) and re-establishes mouse authority. The
        //    old guarded-velocity target (S7-nest D8) was term A of the v9
        //    defect: an ~AoA-sized aim/camera jump at the mouse handover.
        //  - With release_orient ON (S-relorient, Chad 2026-07-28), EVERY
        //    airborne release drives the snap and reports the orient camera
        //    cut — the
        //    flown double-tap verb, automatic. It fires HERE (not in the
        //    S-orient block below) so the S7-hrz capture on fs.released
        //    measures the up-debt about the POST-snap forward on the SAME
        //    tick (plan-audit P1: the later slot captures the stale pre-snap
        //    axis and retires the wrong debt). The !grounded term is
        //    REDUNDANT DEFENSE, not load-bearing (diff red-team P2-1): the
        //    GROUNDED pairing's fl.reset() above already eats the release
        //    edge on every grounded tick, so fs.released && grounded is
        //    unreachable — the term is mutation-unkillable by construction.
        //    An override still held at release kept legacy exactly (the D9
        //    precedent — "the pilot is actively maneuvering") until the
        //    S-relorient ADDENDUM (Chad 2026-07-28, flying the sealed v6:
        //    "anytime my finger isn't pressing freelook, I am in chase camera
        //    directly behind and using mouse aim — even if still turning and
        //    pressing hard keys for control surfaces"). That exception is now
        //    RETIRED under cp.freelook_release_orient_with_keys, here and at
        //    the two sibling D9 sites (the S7-hrz capture below, the double-tap
        //    below that) — one knob so the three can never diverge again.
        //    WHY it had to go: all three ride the ONE-TICK fs.released edge, so
        //    a key held through that tick spent the edge permanently —
        //    freelook_prev is already false, and releasing the keys LATER
        //    produces no new edge. The documented consolation ("the next clean
        //    release orients") was only true if the pilot pressed AND released
        //    Space again. Worse, it was a SPLIT, not a clean no-op: rule 3
        //    still fired (override_used latched during the hold), so the aim
        //    snapped to the guarded velocity while orient_fired was withheld —
        //    the reticle moved, cam_fwd stayed on ease_chase_forward (which in
        //    a sustained turn never converges), and the up-debt never retired.
        //    Walk-back scope (v9 red-team P2-1): release_orient_with_keys =
        //    false disables the WITH-KEYS release snap ONLY — the v9 weld,
        //    nose target, and instant righting are NOT behind this knob, so
        //    flipping it alone yields an unflown hybrid. The true walk-back
        //    is the v8 seal tag flight-kernel-v8-2026-07-29 (ae7ae8f23).
        const bool ovr_ok = !any_ovr || cp.freelook_release_orient_with_keys;
        const bool release_orient =
            cp.freelook_release_orient && fs.released && !st.grounded && ovr_ok;
        if (in.freelook_held) {  // the WELD: every freelook tick (b4c0751)
            st.aim.snap_forward_to_nose(st.curr.orientation);
        } else if (fs.snap_to_nose || release_orient) {
            st.aim.snap_forward_to_dir(orient_snap_dir(st.curr, cp),
                                       st.curr.orientation);
            if (release_orient) res.orient_fired = true;
        }
        // Consume the offered mouse delta into the aim iff MOUSE mode is live
        // and not on a spawn/reset tick (else drop it — never queue it into a
        // smoothed basis, RA9). transport already carried the aim this tick.
        // S-aimff (v4 rung 1): bracket apply_mouse to read back the rotation
        // it ACTUALLY applied (transport ran above, so the delta excludes it
        // by construction) — the aim's own mouse-induced angular velocity,
        // smeared over the frame's N ticks (rotation/(frame_ticks*sim_dt)):
        // the FF demand integral is K_ff*(frame rotation) at any frame rate.
        // A non-consuming tick (zero offered delta) uses the value step_frame
        // forwarded; freelook (mouse_aim_live false) and grounded ticks
        // report ZERO — the mouse fed the orbit or nothing, never the aim.
        glm::dvec3 aim_rate{0.0};
        if (fs.mouse_aim_live && !st.grounded) {
            aim_rate = in.aim_rate_ff;  // ticks 2..N: the forwarded smear
            const glm::dquat q_pre = st.aim.q;
            st.aim.apply_mouse(in.aim_dx, in.aim_dy,
                               cp.aim_sensitivity * in.aim_gain_scale);
            if (in.aim_dx != 0.0 || in.aim_dy != 0.0) {
                const int n = in.frame_ticks > 1 ? in.frame_ticks : 1;
                aim_rate = quat_rotation_vec(st.aim.q * glm::inverse(q_pre)) /
                           (n * ap.sim_dt);
            }
        }
        // Horizon righting at the release — INSTANT (v9 S-nosesnap, ruling
        // ledger docs/DECISIONS.md @ b4c0751; supersedes S7-hrz's 150 deg/s
        // open-loop D3 roll AT THIS, ITS ONLY, CONSUMER — and, for the
        // freelook-release case specifically, the 2026-07-07 "eased, not a
        // snap" ruling). Chad, verbatim: "Snap to view upon release of
        // freelook, no eased anything as I need to immediately view the back
        // of my plane, the aim, the nose, everything — making it lag there is
        // going to disorient." So on the release edge the ENTIRE
        // up-misalignment is retired as ONE gauge roll about the aim's own
        // forward, in the same tick as the forward snap — measured AFTER the
        // rule-3 snap above (the load-bearing v6 ordering: the angle is about
        // the released forward). Still a gauge move, invisible to
        // control::step; still §9.1-legal (a discrete player-commanded event,
        // not a continuous easing). Term B of the v9 defect was this roll's
        // ~0.3 s of rolled-world at release (measured 44.9 deg at the fire
        // tick). The with-keys clause keeps the knob-off legacy arm exactly
        // (release_orient_with_keys=false = sealed-v6 D9: keys-held release
        // rights nothing). kFinishEps keeps the aligned release a structural
        // no-op (bit-identical, no roll at all — the old capture()'s deadband).
        // rate = 0 still skips ALL of this structurally (the knob-off
        // strict-superset proof); with the profile retired, rate>0 is now a
        // pure enable and st.recov is permanently inert (its reset()s remain,
        // harmless; removing the struct is a recorded future candidate,
        // docs/DECISIONS.md).
        if (cp.horizon_recovery_rate > 0.0 && !st.grounded && fs.released &&
            !(any_ovr && !cp.freelook_release_orient_with_keys)) {
            const double mis = st.aim.up_misalignment(up);
            if (std::abs(mis) > input::HorizonRecovery::kFinishEps)
                st.aim.roll_about_forward(mis);
        }
        // S-orient (docs/comfort program Q3): the ORIENT verb — a DISCRETE,
        // player-commanded composed event that puts the pilot "back together"
        // behind the flight path with the horizon righted. Fires ONLY in
        // instructor mode, airborne, on the tick the caller's OrientTap
        // detected a freelook double-tap. The verb does TWO things here:
        //   1. aim := NOSE (orient_snap_dir — v9 S-nosesnap, b4c0751; was the
        //      S7-nest guarded velocity): a discrete player-commanded snap,
        //      the sanctioned shape (rules 2/3), and under the §5b weld a
        //      near-no-op — the aim already rides the nose in freelook.
        //   2. res.orient_fired => the caller hard-cuts cam_fwd :=
        //      aim.forward() (the camera cut behind the aircraft).
        //
        // The up-debt RETIREMENT is NOT done here (P1 red-team fix): on
        // every reachable input the orient fires on the SECOND-tap PRESS tick,
        // where freelook_held is TRUE. The debt is retired by the second tap's
        // RELEASE edge through the righting block above (fs.released -> the
        // v9 INSTANT whole-debt roll), which fires on the release AFTER this
        // press. So the orient verb = the aim snap + the camera cut; the
        // upright lands via the release edge the double-tap already produces.
        // In the D9 overlap (an override still held at that release) with
        // release_orient_with_keys=false, the righting is skipped that once —
        // the legacy knob-off arm, existing semantics.
        // Was SUPPRESSED while any override key is held (the D9 precedent).
        // The S-relorient ADDENDUM retires that here too, on the SAME knob as
        // the release site above — Chad's ask is that the double-tap be TRULY
        // redundant ("I want to keep the double tap but I want it truly
        // redundant"), and a backup verb suppressed by exactly the condition
        // that breaks the primary one is no backup at all. It is also the only
        // verb available WITHOUT leaving freelook. Note the §5b nested aim :=
        // nose fires on freelook-held ticks and this snap lands after it in the
        // same tick, so the velocity snap wins the tick it is commanded on.
        // orient_double_tap_s = 0 means the caller's OrientTap never fires, so
        // orient_cmd stays false — the structural off-switch.
        if (in.orient_cmd && !st.grounded && ovr_ok) {
            st.aim.snap_forward_to_dir(orient_snap_dir(st.curr, cp),
                                       st.curr.orientation);
            res.orient_fired = true;
        }
        // ===================================================================
        // v13 REST-EDGE CAMERA HORIZON RECOVERY (pilot ruling 2026-08-06:
        // "after a maneuver ending inverted — split-S, Immelmann — the CAMERA
        // also rights itself automatically at rest: horizon level, planet
        // below, without a freelook release").
        //
        // The SAME gauge move as the release roll above (a roll of the ONE
        // aim/camera quaternion about its OWN forward: the aim direction never
        // moves, control::step cannot observe it, mouse-up stays screen-up),
        // on a SECOND trigger — the aim coming to REST.
        //
        // Why this is NOT the banned S7-mouselevel per-tick horizon lock: the
        // target is measured ONCE, at the rest EDGE (input::HorizonRecovery's
        // capture/step machinery — the sanctioned open-loop shape, plan §10
        // F1), the debt is FINITE and retires to exactly 0, and nothing
        // recomputes local_up into the roll while it runs. A per-tick
        // recompute would close the banned control-driving-quaternion loop
        // through the mouse-driven forward and flip across the zenith.
        //
        // What it reads: the MOUSE (aim motion) and the flight path only —
        // never a key state, never the body attitude. The rest signal is the
        // same `aim_moved` bit the cascade's deadzone rest_dwell gate uses
        // (ONE seam), so freelook and grounded ticks structurally cannot count
        // as motion and cannot arm a recovery either.
        //
        // Cancels: freelook entry, GROUNDED, focus loss. An aim-moved tick
        // resets the DWELL (and disarms a finished roll) but a roll already in
        // flight COMPLETES — one smooth motion, see the aim_moved branch. The
        // freelook-release instant roll above is untouched — it simply leaves
        // ~0 debt for this block to find.
        const bool aim_moved = fs.mouse_aim_live && !st.grounded &&
                               (in.aim_dx != 0.0 || in.aim_dy != 0.0);
        // (The law itself now lives in app/rest_horizon.h so the STING's
        // carried aim frame can run the SAME code on the same dials — a pure
        // MOVE of the block that stood here; the aeroplane path is
        // bit-identical, only the state is passed by reference now.)
        rest_horizon_tick(st.aim, st.aim_rest, st.recov_armed, st.recov,
                          st.prev_path,
                          cp.horizon_recovery_rate > 0.0 && !st.grounded &&
                              !in.freelook_held,
                          aim_moved, st.curr.velocity, up, cp, ap.sim_dt);
        // RAW mouse basis (§9.1, S7-raw 2026-07-06 — REVERSES the F1 screen-
        // relative coupling). apply_mouse rotates the aim about the aim frame's
        // OWN carried up/right (transport + apply_mouse) — nothing camera-
        // referenced. The F1 experiment oriented the mouse delta by the eased/
        // lagged RENDER camera basis to keep mouse-up == screen-up; but that is
        // a SMOOTHED basis feeding mouse->aim (the RA9 ban), and near the
        // zenith the camera's screen-right rotates/degenerates under the input,
        // so a SLOW vertical mouse STALLED at ~vertical and could not loop over
        // (the mouse stopped being raw). Chad's ruling: make it RAW — the
        // carried §9.1 frame references nothing external, so it never poles and
        // a steady vertical mouse loops over the top continuously. TRADE-OFF (a
        // SEPARATE dial): the carried frame's UP drifts off local_up after a
        // big maneuver (holonomy + the maneuver's own rotation), so mouse
        // left/right can feel rotated — a gentle roll-to-local_up correction
        // (roll-only about forward, never poles) addresses THAT without
        // re-coupling the mouse to the camera. in.cam_fwd/cam_up are no longer
        // read here (retained on TickInput for the camera-render path the
        // caller drives; vestigial to the mouse). With zero mouse apply_mouse
        // is a strict no-op, so RA9 holds and mirror-equivalence stays
        // bit-identical. Keyboard override does NOT touch the aim (SPEC §9.5,
        // S7-ovr2): the MOUSE owns the reticle/camera, the keyboard is a pure
        // supplementary in-envelope nudge (the in-core override drives the
        // airframe rate, pursuit suspended). Earlier "aim rides the nose"
        // collapsed the aim onto the nose on the keypress, which swung the
        // (aim-leaning) camera back behind the plane — the exact "keypress
        // snaps the view" the pilot hit. On release, pursuit resumes toward the
        // held mouse aim (the reticle never moved), so the instructor simply
        // flies back to where you were already pointing.

        control::Input ci;
        ci.target_dir_world = st.aim.forward();
        ci.throttle = in.throttle;
        ci.flap_cmd = in.flap_cmd;  // MB-flaps passthrough (throttle pattern)
        ci.gear_cmd = in.gear_cmd;
        ci.wheel_brake = in.wheel_brake;  // R4g passthrough (same pattern)
        ci.grounded = st.grounded;
        // Aim-motion gate (SPEC §9.3 as amended): TRUE iff apply_mouse above
        // actually rotated the aim this tick — the same CQ2 gate, so freelook
        // (mouse -> camera) and grounded ticks can never count as motion.
        ci.aim_moved = aim_moved;  // ONE seam (the v13 rest gate reads it too)
        // S-aimff: the smeared aim rate (0 unless the mouse actually fed the
        // aim this frame — the same CQ2 gate as aim_moved, one seam).
        ci.aim_rate_world = aim_rate;
        res.aim_rate_ff = aim_rate;
        for (int i = 0; i < 3; ++i) {
            ci.override_mask[i] = in.override_mask[i];
            ci.override_sign[i] = in.override_sign[i];
        }
        // Damaged params to BOTH the cascade and the plant (Fable P0-3, same
        // object). At zero damage *ap_use==ap, *cp_use==cp, with_bias is a
        // no-op ⇒ the golden/mirror path is bit-identical.
        const control::Output o = control::step(
            st.curr, ci, st.internal, *ap_use, *cp_use, env, ap.sim_dt);
        st.internal = o.internal;
        res.telem = o.telem;
        // R5e: a claimed plane flies neutral surfaces regardless of the
        // cascade's output (the cascade still steps — its internal state
        // marches irrelevantly; the SKY owns the airframe now).
        const sim::Inputs cmd =
            st.escape_claimed ? sim::Inputs{} : with_bias(o.inputs);
        res.inputs =
            cmd;  // post-bias: the surfaces the plant actually flew (P1-3)
        st.curr = sim::step(st.curr, cmd, *ap_use, env, ap.sim_dt);
    }

    st.grounded = false;

    // R4-FLY-6 TOUCHDOWN report: the airborne -> GROUNDED capture edge
    // (st.prev is this tick's entry state, snapshotted above). Unconditional
    // report — no cw gate needed, it writes no state.
    if (!st.prev.on_ground && st.curr.on_ground) {
        res.touchdown = true;
        res.touchdown_pos = st.curr.position;
        res.touchdown_speed = glm::length(st.curr.velocity);
    }

    // R4-FLY-5 GROUND CONSEQUENCES -> the COMPONENT DAMAGE MODEL (Chad's
    // ruling: wings bank freely and DAMAGE on strike; hard braking breaks the
    // prop). The kernel only DETECTS (transient SimState events); here they
    // feed the SAME DamageState ballistic hits feed — one damage surface,
    // one transform, one kill rule. Gated on cw (no CombatWorld — harness,
    // goldens, tests — ignores the events: bit-identical).
    if (cw != nullptr) {
        if (st.curr.wing_strike != 0) {
            // Scrape chip rate scales with ground-speed energy: (v/ref)^2
            // per second of contact — a fast strike shreds the wing in a
            // fraction of a second, a slow tip-scrape chews it gradually.
            const double sp = glm::length(st.curr.velocity);
            const double ref =
                std::max(cw->damage_params.ground_wing_ref_ms, 1.0);
            const double chip = cw->damage_params.ground_wing_rate *
                                (sp / ref) * (sp / ref) * ap.sim_dt;
            double& wing = st.curr.wing_strike > 0 ? cw->damage.wing_right
                                                   : cw->damage.wing_left;
            wing = std::max(0.0, wing - chip);
            cw->player_hp = combat::summary_hp(cw->damage);
            cw->last_damage_src = 2;  // E6.1: a terrain scrape hurt him
        }
        if (st.curr.prop_strike && cw->damage.engine > 0.0) {
            cw->damage.engine = 0.0;  // "your propeller breaks" — dead stick
            cw->player_hp = combat::summary_hp(cw->damage);
            cw->last_damage_src = 2;  // E6.1: a terrain scrape hurt him
        }
    }

    // Crash -> reset (SPEC §6.3): respawn airborne; the NEXT tick is GROUNDED,
    // which pairs control::reset() with aim := nose + fl.reset() (AT-13 clean
    // rebirth). prev := curr so render::interpolate never streaks the dead
    // life's position to the spawn across one frame; prev_up reseeded so the
    // next transport can't hit transport_rotation's antiparallel assert.
    //
    // R4: with a live ground field the KERNEL owns the contact verdict
    // (sim/ground.h — radius_at >= R subsumes the bare-sphere rule, and a
    // sea-level lake TOUCHDOWN must read as a landing, never as altitude<=0
    // death). env==null keeps the v3 altitude rule bit-identically.
    const bool ground_live = env != nullptr && env->ground != nullptr;
    // T1 — the bare-sphere branch's crash predicate must ALSO yield inside the
    // net (the "every branch that does the gated thing" house lesson): a
    // null-ground world flying through the tunnel volume below R must not
    // respawn. tunnels null => the byte-identical altitude<=0 rule. T9a routes
    // this through the shared inside_tunnel() (bit-identical) so the CAVECAM
    // camera flag and the crash yield use ONE predicate.
    const bool in_tunnel = inside_tunnel(env, st.curr.position);
    if (ground_live
            ? st.curr.crashed
            : (sim::altitude(st.curr.position, ap) <= 0.0 && !in_tunnel)) {
        // ★★★ L6 — THE RESPAWN LOCK (Chad's ruling R9, 2026-09-03): "once the
        // match clock is initiated once, even if paused, then there are no new
        // respawns from either side." This life was the last one.
        //
        // ★ HOW THE MATCH RESOLVES, AND WHY THIS WAY. There is exactly one
        // rule in this codebase for "the player has no aeroplane left" -- it is
        // `planes_left <= 0` in conquest_eval_outcome, and it already carries
        // the deathmatch points tie-break Chad ruled on 2026-08-30. A second
        // terminal path invented here would be a second answer to a question
        // that is already answered, and a worse one, because the tie-break is
        // the half he actually flew. So the lock SPENDS THE POOL:
        // planes_left = 0, and the existing evaluator decides, tie-break and
        // all.
        //
        // ⚠ AND IT RUNS ONCE. The crash predicate keeps firing every tick on a
        // body that is never re-placed, so the work is gated on the outcome
        // still being PLAYING; after the latch this branch is a pure no-op and
        // the wreck stays where it fell.
        //
        // ⚠ `res.respawned` STAYS FALSE. That flag means an aeroplane was BORN:
        // it spends a life (the conquest block (1) below) and it opens the
        // spawn menu (app/main.cpp). Neither may happen here.
        const bool respawn_locked_out =
            cq != nullptr && combat::respawns_locked(cq->state);
        if (respawn_locked_out) {
            if (cq->state.outcome == combat::Outcome::PLAYING) {
                if (cw != nullptr)
                    book_player_death(*cw,
                                      combat::CombatWorld::DeathCause::kCrash,
                                      st.curr, ap, env);
                cq->state.planes_left = 0;
                combat::conquest_eval_outcome(cq->state);
                res.respawn_refused = true;
            }
        } else {
            // RUNG E6.1: book the death + its cause BEFORE the reset
            // overwrites the position/damage this record is made of.
            // Recorder-only (see book_player_death) — no trajectory reads it.
            if (cw != nullptr)
                book_player_death(*cw, combat::CombatWorld::DeathCause::kCrash,
                                  st.curr, ap, env);
            st.curr = player_spawn(PlayerSpawnChoice::Aircraft, ap,
                                   st.spawn_alt, st.spawn_up, st.spawn_fwd);
            st.prev = st.curr;
            st.prev_up = sim::local_up(st.curr.position);
            // Reseed the aim frame HERE, not only on the next GROUNDED tick
            // (P3c, pre_spec_audits/fable5_section6_fixreport.md): if the crash
            // lands on the frame's LAST tick, the render frame between this
            // return and the next tick would draw the fresh spawn position
            // through the DEAD life's aim frame (an astern camera pop for one
            // frame). Idempotent with the GROUNDED pairing's reseed above --
            // same (orientation, local_up) seed, since the next tick's
            // transport(prev_up==up) is identity -- so it changes nothing on
            // the loop-correctness path, only kills the stale render. Uses the
            // same reseed the GROUNDED pairing uses (aim := nose, up :=
            // local_up), never the carried up of the dead life.
            st.aim.reseed(st.curr.orientation, st.prev_up);
            st.grounded = true;
            res.respawned = true;
            // A crash is a fresh airframe: clear component damage AND mirror
            // the death-respawn housekeeping so the two paths can't drift
            // (Fable P0-4 / P1-3) — clear in-flight enemy rounds + arm the
            // respawn invuln (anti spawn-camp), same as the component-death
            // reset below.
            if (cw != nullptr) {
                combat::reset_damage(cw->damage);
                // derived (= full)
                cw->player_hp = combat::summary_hp(cw->damage);
                cw->player_invuln_ticks = static_cast<int>(
                    cw->setup.respawn_invuln_time / ap.sim_dt + 0.5);
                for (weapon::Projectile& p : cw->enemy_pool) p.active = false;
            }
        }
    }

    // The target-drone fleet advances once per player tick (SPEC §0 S8-drone),
    // AFTER the player's crash/respawn so it is independent of the player's
    // life (a player crash does not touch the bandits). Additive + nullptr-
    // gated => the no-drone path above is bit-identical (the firewall).
    if (dw != nullptr) {
        // Bandit combat AI (docs/bandit_combat_plan.md): the fleet HUNTS the
        // player only when a CombatWorld is present (combat is on) — else it
        // patrols bit-identically (the firewall; pinned by the no-cw tests).
        // assign_engagements flags the nearest max_engaged bandits as
        // ATTACKERS, PER-TICK on the POST-STEP player (P1-5, frame-rate
        // independent); the player is read-only (const&). drone::tick then
        // PURSUES the engaged ones.
        // ALL-VS-PLAYER FURBALL (Chad's ask, 2026-07-26): "MAKE THE AI CHASE
        // AND KILL ME, ALL OF THEM VS ME." cq->params.all_vs_player (default
        // true, config/game.toml [conquest]) is the ONE switch; both branches
        // below key off it, and cq == nullptr (no conquest) is always the old
        // path — bit-identical to pre-furball for every non-conquest combat
        // session, by construction.
        const bool furball =
            cw != nullptr && cq != nullptr && cq->params.all_vs_player;
        const sim::SimState* player = nullptr;
        if (cw != nullptr) {
            if (furball) {
                // Whole-fleet furball: every non-inert drone qualifies,
                // regardless of range (kFurballRangeM) — a copy of dparams
                // with the range gate blown open, so drone::assign_engagements
                // stays the ONE pure selection function (no forked selection
                // logic). max_engaged = the fleet size so nobody is bumped to
                // patrol for lack of a slot.
                drone::DroneParams furball_dp = dw->dparams;
                furball_dp.engage_range = kFurballRangeM;
                furball_dp.disengage_range = kFurballRangeM;
                drone::assign_engagements(dw->drones, st.curr,
                                          static_cast<int>(dw->drones.size()),
                                          furball_dp);
            } else if (cq != nullptr) {
                // game-AI-R4 THE AIR WAR (Chad's fly rulings 2026-08-05):
                // real 5v5 teams. Every drone picks a FOE — the nearest
                // max_engaged enemies-of-player take the PLAYER (hysteretic
                // slots), everyone else fights the nearest OPPOSING drone
                // (sticky). Friendlies fight WITH the player, never against
                // him; pump defense scrambles below via assign_defense.
                const int max_engaged =
                    combat::difficulty_params(cw->setup.difficulty).max_engaged;
                combat::assign_foes(dw->drones, st.curr,
                                    cq->state.player_faction, max_engaged,
                                    dw->dparams);
                // ENV-4: hand the defence the HP-delta latch D2 already
                // maintains. `attacked_pump[f] == f` is the SURFACE pump of
                // faction f (assign_defense's own indexing: pumps[0]/pumps[1]
                // are the surface pair, 2/3 the deep pair), so a deep-pump hit
                // does NOT arm a surface defence — Chad ruled deep pumps
                // undefended this rung.
                // ⚠ ONE-TICK LAG, and it is deliberate: the latch is refreshed
                // further down this same function, so this reads last tick's
                // value. The latch's own comment already rules that irrelevant
                // against a regroup_attack_window_s = 10 s window, and moving
                // the latch above the orders would reorder a signed block for
                // no measurable gain.
                const bool surface_hurt[2] = {
                    cq->pump_attacked_s[0] > 0.0 && cq->attacked_pump[0] == 0,
                    cq->pump_attacked_s[1] > 0.0 && cq->attacked_pump[1] == 1};
                combat::assign_defense(dw->drones, cq->state.pumps, st.curr,
                                       cq->state.player_faction,
                                       kDefendThreatRadiusM,
                                       kDefendersPerFaction, surface_hurt);
            } else {
                const int max_engaged =
                    combat::difficulty_params(cw->setup.difficulty).max_engaged;
                drone::assign_engagements(dw->drones, st.curr, max_engaged,
                                          dw->dparams);
            }
            player = &st.curr;
            // CONQUEST furball path only (the all_vs_player toggle): a killed
            // WRECK never engages; the friendly-faction exclusion is handled
            // structurally by assign_foes on the non-furball path.
            if (cq != nullptr && furball) {
                for (drone::DroneState& d : dw->drones) {
                    if (d.inert) d.engaged = false;
                    d.friendly_side = false;  // furball: everyone hostile
                }
            }
        }
        // CONQUEST air-orders (ADEPT-AI + COMPETITIVE rung, 2026-07-25 fly-2:
        // "AI should be adept and seek to stay in the bubble" + "a little
        // competitive"). Set each drone's bubble-LEASH (its own faction
        // ellipse, LIVE growth-scaled) and RAID directive BEFORE it ticks. cq
        // null => no orders => bit-identical (leash/raid default OFF). The
        // tunnel-run exemption is enforced INSIDE drone::tick (it owns
        // d.mav.mode).
        if (cq != nullptr) {
            world::FactionGrowth grow[2];
            app::faction_growth_from_state(cq->state, grow);
            // ---- ★★★ D2 — THE "MY PUMP IS UNDER ATTACK" LATCH -------------
            // An HP-DELTA latch, per faction, decayed each tick. See
            // ConquestWorld::pump_attacked_s for why the two existing signals
            // (cq->pump_under_attack, raid.h's `threatened`) could not serve.
            // Runs HERE, where the orders are armed; the one-tick lag against
            // the damage block later in this function is irrelevant against a
            // regroup_attack_window_s = 10 s window.
            for (int pi = 0; pi < combat::kNumPumps; ++pi) {
                const combat::Pump& p = cq->state.pumps[pi];
                const int pf_own = (p.faction == combat::CQ_VALLEY) ? 0 : 1;
                if (cq->prev_pump_hp[pi] >= 0.0 &&
                    p.hp < cq->prev_pump_hp[pi] - 1e-9) {
                    cq->pump_attacked_s[pf_own] =
                        dw->dparams.regroup_attack_window_s;
                    cq->attacked_pump[pf_own] = pi;
                }
                cq->prev_pump_hp[pi] = p.hp;
            }
            for (int f = 0; f < 2; ++f) {
                cq->pump_attacked_s[f] =
                    std::max(0.0, cq->pump_attacked_s[f] - ap.sim_dt);
                if (cq->pump_attacked_s[f] <= 0.0) cq->attacked_pump[f] = -1;
            }
            // ★★★ RUNG E12.1 — THE LIVE-ROSTER MASK for the raider
            // designation. A PRE-LOOP SNAPSHOT, exactly like the striker cap
            // below and for the same reason: raid duty must not depend on
            // fleet iteration order. combat::raid_backfill false => `mask`
            // stays null => combat::faction_raider takes its original,
            // trait-table-only path, bit-identically.
            bool raider_live[combat::kNumMavericks] = {};
            const bool* raider_mask = nullptr;
            if (cq->params.raid_backfill) {
                for (const drone::DroneState& d : dw->drones) {
                    const int si = ((d.spawn_index % combat::kNumMavericks) +
                                    combat::kNumMavericks) %
                                   combat::kNumMavericks;
                    if (!d.inert) raider_live[si] = true;
                }
                raider_mask = raider_live;
            }
            // RUNG E3.2 — THE CONCURRENT-STRIKER CAP. Counted here, BEFORE the
            // arming loop, over the fleet's mav state as it stood at the END of
            // the previous tick (drone::tick runs in the later loop below), so
            // the count is a PRE-LOOP SNAPSHOT and the outcome cannot depend on
            // fleet iteration order — the same determinism discipline the foe
            // snapshot uses. This is the one fact the per-drone, pure maverick
            // brain structurally cannot know, so the app supplies it as an
            // order field (the RaidOrder precedent) and drone/ stays fleet-
            // blind.
            //
            // Only COLLAPSE-TRIGGERED runs are counted and capped (mav
            // .run_on_order): a pilot flying his own trait-period run keeps the
            // old flavour untouched. Measured defect (conquest_tape_1): all
            // three non-raid enemy strikers collapsed at match start and spent
            // 53-80% of their lives in transit, so nobody was left on patrol —
            // where RUNG E1 is what makes them dangerous.
            //
            // NO STARVATION HAZARD: the pipeline's only unbounded mode is RUN
            // (every other one has a timeout), so a striker whose s_est stalls
            // off-spine could in principle hold a slot for a long time. That
            // pauses ORDERED launches only — the held pilots' trait countdowns
            // keep running underneath (maverick.h's two-countdown shape), so
            // the tunnel pipeline can never be shut off by one stuck pilot.
            // MIRROR PIN (red-team P2-4): test_stope_probe.cpp's
            // count_on_order_runs/arm_strike hand-mirror this block -- a
            // change here must be mirrored there or the probes silently
            // measure a different scheduler than the app runs.
            int on_order_runs[2] = {0, 0};
            if (dw->dparams.strike_concurrent_max > 0) {
                for (const drone::DroneState& d : dw->drones) {
                    if (d.inert) continue;
                    if (d.mav.run_on_order &&
                        d.mav.mode != maverick::MaverickState::Mode::PATROL)
                        ++on_order_runs[combat::maverick_faction(
                            d.spawn_index, cq->state.player_faction)];
                }
            }
            for (drone::DroneState& d : dw->drones) {
                if (d.inert) continue;
                // BANDITS ARE CONFINED TO THE AIR (Chad's ruling, 2026-07-26:
                // "Dont allow the bandits outside of the bubble zone. Confine
                // their programming to bubble.").
                //
                // The leash is now ALWAYS ON -- no furball exemption, no
                // engaged exemption. Earlier today both exemptions existed
                // because the leash was welded to the drone's OWN faction
                // ellipse, so containment fought a pursuit that had to leave
                // the dome to reach the player, and bandits parked on their own
                // bubble edge. This changes WHICH ellipse confines them instead
                // of switching the confinement off:
                //
                //   own dome alive  -> leashed to their own dome (as before)
                //   own dome GONE   -> leashed to the OTHER faction's dome
                //
                // That second line is the whole fix. A faction that has lost
                // both pumps has no air of its own, and the only breathable sky
                // left on the planet is the enemy's -- which is exactly where
                // the player is. So confinement now DRIVES the invasion instead
                // of preventing it: leash and pursuit point the same way, no
                // standoff is possible, and the endgame happens inside the one
                // remaining dome. It also structurally prevents the vacuum
                // strand (docs/ai_vacuum_strand.md): a bandit is never asked to
                // hold station where it cannot fly.
                //
                // Both domes gone => a stays 0 => leash off by its own contract
                // (BubbleLeash: "enabled=false (a<=0/b<=0) => no leash").
                // NOTE for docs/strike_mission_plan.md rung 1: drone::tick
                // already exempts the tunnel dispositions from the leash
                // (in_tunnel_mode), so the Murray/Errington sortie is
                // unaffected by this and still needs no leash special-casing.
                // ---- D2: MY OWN dome's facts, hoisted out of the leash block
                // below because the leash is SWAPPED to the enemy ellipse when
                // my own dome dies. "Home" must never be read off d.leash.
                bool own_home_alive = false;
                glm::dvec3 own_cdir{0.0, 1.0, 0.0};
                glm::dvec3 own_maj{1.0, 0.0, 0.0};
                double own_a = 0.0;
                double own_b = 0.0;
                drone::BubbleLeash lz;
                {
                    const int own = combat::maverick_faction(
                        d.spawn_index, cq->state.player_faction);
                    glm::dvec3 cdir{0.0, 1.0, 0.0};
                    glm::dvec3 maj{0.0};
                    double a = 0.0;
                    double b = 0.0;
                    world::faction_ellipse(own, grow, cdir, maj, a, b);
                    // RUNG E6.4: "extinct" now includes CRUSHED. A dome
                    // floored at kFactionScaleFloor is still a live ellipse,
                    // so the leash pinned a whole wing inside a bubble that
                    // barely exists — measured on tape 2 as revived enemies
                    // sitting at home while their radius_scale read 0.0. Below
                    // the floor the pilot is leashed to the surviving faction's
                    // air, exactly as the extinct branch already does.
                    // leash_min_radius_scale 0 => strictly-less-than is never
                    // true => bit-identical.
                    const bool own_crushed =
                        cq->state.radius_scale[own] <
                        cq->params.leash_min_radius_scale;
                    // D2: capture MY OWN ellipse BEFORE the crush swap below.
                    own_cdir = cdir;
                    own_maj = maj;
                    own_a = a;
                    own_b = b;
                    own_home_alive = !own_crushed && a > 0.0 && b > 0.0;
                    if (own_crushed || !(a > 0.0 && b > 0.0))
                        world::faction_ellipse(1 - own, grow, cdir, maj, a, b);
                    if (a > 0.0 && b > 0.0) {
                        lz.enabled = true;
                        lz.center_dir = cdir;
                        lz.major_axis = maj;
                        lz.a_m = a;
                        lz.b_m = b;
                    }
                }
                d.leash = lz;
                // ---- ENV-1: stamp BOTH dome ellipses, each tagged live ---
                // ⚠ This is deliberately NOT read off `lz`: the block above
                // swaps the leash to the ENEMY ellipse when the own dome dies,
                // so the leash is the one thing on this drone that cannot be
                // trusted to describe a named faction's air.
                // ⚠ The live/dead test is evaluated BEFORE anything derived —
                // a dead dome must never reach a divide or a normalize().
                {
                    const int own = combat::maverick_faction(
                        d.spawn_index, cq->state.player_faction);
                    world::FactionGrowth unit[2];  // radius_scale = 1 => BASE
                    drone::AirDomes ad;
                    for (int slot = 0; slot < 2; ++slot) {
                        const int f = (slot == 0) ? own : (1 - own);
                        drone::DomeEllipse& e = ad.dome[slot];
                        e.radius_scale = cq->state.radius_scale[f];
                        // A dome at/below the floor makes no air (E6.4's own
                        // rule, and combat::kFactionScaleFloor is 0.0).
                        if (!(e.radius_scale >= 1e-9)) continue;
                        world::faction_ellipse(f, grow, e.center_dir,
                                               e.major_axis, e.a_m, e.b_m);
                        glm::dvec3 bdir{0.0, 1.0, 0.0};
                        glm::dvec3 bmaj{1.0, 0.0, 0.0};
                        world::faction_ellipse(f, unit, bdir, bmaj, e.base_a_m,
                                               e.base_b_m);
                        e.live = e.a_m > 0.0 && e.b_m > 0.0;
                    }
                    d.air_domes = ad;
                }
                // game-AI-R3 (Chad's ruling 2026-08-05): BOTH factions play
                // the loop. Surface raid: each faction's designated raiders
                // (top-2 aggression, combat::faction_raider) fly at the
                // OPPOSING faction's surface pump. The old `!d.engaged` gate
                // is GONE — in the all_vs_player furball every drone is
                // engaged every tick, which silently disabled raids
                // entirely; the raid now pauses only when the PLAYER is
                // actually close (defense works by showing up), so far-off
                // pilots press the objective while near ones dogfight.
                const int own = combat::maverick_faction(
                    d.spawn_index, cq->state.player_faction);
                const int ef = 1 - own;  // this drone's enemy faction
                drone::RaidOrder ro;
                const combat::Pump& sp = cq->state.pumps[ef];
                // HYSTERETIC player-range gate (the house rule; red-team P2 —
                // a bare engage_range threshold flipped raid <-> fight at
                // tick rate when the player dwelt at the boundary): raid duty
                // needs the player beyond DISENGAGE range to arm, and pauses
                // only once the player closes INSIDE engage range.
                //
                // FIX-F3 (docs/ai_phase2_fix_spec.md): this reads the
                // conquest-owned raid_pause_engage_m/raid_pause_disengage_m
                // (cq->params, [conquest]), NOT dw->dparams.engage_range/
                // disengage_range (the FOE-ASSIGNMENT ranges, sized for
                // dogfight assignment at 6/7 km). The tape showed a raid
                // pausing at 6 km -- unseeable, so the player never saw the
                // AI "try". The pause exists so the player defends by
                // SHOWING UP: 1.5 km is inside visual range of the pump
                // fight.
                const double player_range =
                    glm::length(st.curr.position - d.curr.position);
                d.raid_range_ok =
                    d.raid_range_ok
                        ? (player_range > cq->params.raid_pause_engage_m)
                        : (player_range > cq->params.raid_pause_disengage_m);
                // RUNG E6.5 — RELENTLESS RAIDS. Chad's ruling supersedes the
                // Phase-2 raid pause: a raider under player threat does NOT
                // abandon the pump. The latch above still RUNS (its hysteresis
                // state stays coherent for his walk-back) but stops being able
                // to suspend the order. false => bit-identical.
                if (cq->params.raid_no_pause) d.raid_range_ok = true;
                // game-AI-R4: the player-proximity pause exists so the
                // player can DEFEND HIS OWN pump by showing up — it applies
                // only to ENEMY raiders. A friendly raider attacking the
                // ENEMY pump keeps pressing with the player alongside.
                if (combat::faction_raider(d.spawn_index, raider_mask,
                                           cq->state.player_faction) &&
                    sp.alive &&
                    (d.friendly_side || d.raid_range_ok)) {
                    ro.active = true;
                    ro.target_pos = sp.pos;
                    ro.pump_idx = ef;
                    // ---- D2: the two app-known facts stamped on every order.
                    // The rank was already computed inside the router below;
                    // hoisted here (raider_rank is pure) so the DECK-RUNNER
                    // fact exists even with the router off. deck_run selects
                    // who does the stage-1 RETURN — the one designated direct
                    // crosser; everyone else is in the bore.
                    const int raid_rank =
                        raider_rank(d.spawn_index, raider_mask);
                    const bool deck_slot =
                        raid_rank < dw->dparams.raid_deck_run_slots;
                    ro.deck_run = deck_slot;
                    ro.home_alive = own_home_alive;
                    // ---- ★★★ S2-TUNNEL — THE ROUTER --------------------
                    // Chad, 2026-08-26: "if they are leaving for a raid on a
                    // pump, the surface one, they should actually use the
                    // tunnel, that is what it is there for."
                    //
                    // THREE facts decide it, all measured HERE, none welded:
                    //  1. how much THIN AIR the direct great circle from THIS
                    //     DRONE'S OWN POSITION to the pump crosses;
                    //  2. how much thin air the TUNNEL route crosses
                    //     (drone -> own mouth, then far mouth -> pump; the
                    //     bore itself holds full air unconditionally and the
                    //     crash predicate is suspended inside the net, so it
                    //     contributes ZERO);
                    //  3. this pilot's rank in his own faction's raider order.
                    //
                    // ★ MID-MATCH FALLBACK, and it is why this is a
                    // comparison and not "tunnel always": when a faction loses
                    // a pump its dome halves, and the tunnel route grows its
                    // OWN thin legs (measured on tape 12 at k=55853, after the
                    // first pump death, BOTH mouths sat outside their domes).
                    // Tunnel-always would then march raiders into worse
                    // vacuum than the direct crossing. FLY THE LESSER.
                    //
                    // ★ RE-EVALUATED EVERY TICK, from the drone's own
                    // position. At the far mouth the remaining leg is fully
                    // in-dome, so this flips FALSE, the run-scheduler freeze
                    // returns, and the raid branch owns the approach and the
                    // attack. See RaidOrder::via_tunnel for why a sticky latch
                    // would abort the raid instead.
                    if (dw->dparams.raid_route_via_tunnel) {
                        const double R = ap.R;
                        const double direct_gap = uncovered_arc_m(
                            d.curr.position, sp.pos, grow, R);
                        // The designated DECK RUNNERS keep the direct route
                        // ("a few designated runs crossing the deck to make a
                        // run on the pump is okay"). Rank 0 is the faction's
                        // most aggressive live raider.
                        const world::TunnelNet* net =
                            env != nullptr ? env->tunnels : nullptr;
                        // ★ D2: a drone MID-REPOSITION is not stamped for the
                        // bore. `raid_now` therefore stays false, and per
                        // maverick.h:998-1000 the raid countdown FREEZES (it
                        // is not reset and no one-shot is burned) — the bore
                        // launch fires on the tick the regroup releases. No
                        // sticky latch is added here: the regroup latch is the
                        // only memory, and both its edges are hysteretic.
                        // ⚠ COST, stated: this delays a tunnel-routed raider's
                        // bore entry by the episode duration. Measured in the
                        // report.
                        // ★ D3: nor is a drone MID-BALLISTIC-RUN. VERIFIED
                        // WHY IT IS NEEDED: deck_slot is raider_rank <
                        // raid_deck_run_slots, and rank CHURNS when a raider
                        // dies (raid_backfill ships true) — so a backfill
                        // could flip deck_slot false mid-dive, stamp
                        // via_tunnel, and the phase reset below would zero a
                        // COMMITTED dive AND launch a bore TRANSIT out of a
                        // 245 m/s descent. Freeze-not-reset semantics are the
                        // D2 treatment verbatim: raid_now stays false, the
                        // raid countdown FREEZES, and the bore launch fires
                        // the tick the phase clears. Bounded by construction
                        // (the climb timeout + arrival), so this is seconds
                        // to tens of seconds, never a sticky latch.
                        const bool bal_committed =
                            d.ballistic == drone::BallisticPhase::CLIMB ||
                            d.ballistic == drone::BallisticPhase::DIVE ||
                            d.ballistic == drone::BallisticPhase::RUN;
                        if (!deck_slot && !d.regroup.active &&
                            !bal_committed &&
                            direct_gap > dw->dparams.raid_route_gap_max_m &&
                            net != nullptr && !net->spine.empty()) {
                            // OWN MOUTH: the identical measurement the strike
                            // site does below — whichever spine end is nearer
                            // this pilot's OWN deep pump is his own mouth.
                            const glm::dvec3& own_deep =
                                cq->state.pumps[2 + own].pos;
                            const glm::dvec3 front = net->spine.front().pos;
                            const glm::dvec3 back = net->spine.back().pos;
                            const bool front_is_own =
                                glm::length(front - own_deep) <=
                                glm::length(back - own_deep);
                            const glm::dvec3 mine =
                                front_is_own ? front : back;
                            const glm::dvec3 far = front_is_own ? back : front;
                            const double tunnel_gap =
                                uncovered_arc_m(d.curr.position, mine, grow,
                                                R) +
                                uncovered_arc_m(far, sp.pos, grow, R);
                            if (tunnel_gap < direct_gap) {
                                ro.via_tunnel = true;
                                ro.entry_dir = front_is_own ? +1 : -1;
                            }
                        }
                    }
                }
                d.raid = ro;
                // ---- ★★★ D2 — STAGE 0 (THE REPOSITION) + STAGE 1 (THE
                // RETURN). Chad, 2026-08-27, verbatim: "if they are near the
                // outer one third near the boundary and their own pump is
                // getting attacked maybe they go down and move closer to the
                // inside of their bubble, then climb gaining speed for a deck
                // run" and "they should go back to whatever bubble if they
                // have one left, climb to a decent altitude and then parabolic
                // dive".
                //
                // ★ A DOME-LESS FACTION IS NEVER ORDERED HOME. own_home_alive
                // is false for it, so `eligible` is false, no order is ever
                // armed, and the S4 deck respawn + the S1-DECK direct route
                // stand exactly as they are. His words: "IF THEY HAVE ONE
                // LEFT".
                // ★ AN ENGAGED TICK IS NEVER DRAGGED HOME. He ruled pursuit
                // unlimited, "chase you anywhere". `!d.engaged` is structural
                // here AND in the drone branch — both places, on purpose.
                {
                    const drone::DroneParams& rdp = dw->dparams;
                    // The one-shot phase resets whenever the run it belongs to
                    // is gone: no raid order at all, or the router put this
                    // raid in the bore (a tunnel raid has no deck run to set
                    // up). Respawn resets it too (drone.h respawn_in_place).
                    if (!ro.active || ro.via_tunnel) {
                        d.ballistic = drone::BallisticPhase::NONE;
                        d.ballistic_s = 0.0;  // D3: and the climb clock
                    }
                    const double own_frac =
                        own_home_alive
                            ? drone::ellipse_frac(d.curr.position, own_cdir,
                                                  own_maj, own_a, own_b, ap.R)
                            : 0.0;
                    // ★ regroup_frac_arm > 0 IS AN EXPLICIT DEAD SWITCH, the
                    // S1-DECK deck_avoid_agl_enter_m shape — NOT an interval
                    // that happens to be empty. "(0, lip]" would arm the
                    // trigger EVERYWHERE, i.e. the exact opposite of the
                    // walk-back the key advertises.
                    const bool eligible =
                        rdp.regroup_frac_arm > 0.0 && own_home_alive &&
                        !d.engaged && !d.defend.active &&
                        !maverick::is_tunnel_mode(d.mav.mode);
                    bool want = false;
                    int reason = d.regroup.reason;
                    if (eligible && d.regroup.active) {
                        // HOLD. Hysteretic on frac (arm 0.667 / release 0.45 —
                        // both coordinates, no tick-rate flapping), plus ONE
                        // timeout clocked FROM EPISODE START. It is deliberately
                        // NOT clocked off the attack window: a window-zero
                        // timeout would fire ~5 s into every episode on the
                        // dwell-alone arm, and 5 s at the 123 m/s errand speed
                        // is ~615 m against the ~3 km an episode needs — the
                        // fallback arm could never complete a single one.
                        d.regroup_s += ap.sim_dt;
                        want = own_frac >= rdp.regroup_frac_release &&
                               d.regroup_s < rdp.regroup_max_s;
                    } else if (eligible) {
                        const int own_f = combat::maverick_faction(
                            d.spawn_index, cq->state.player_faction);
                        const bool attacked =
                            !rdp.regroup_require_pump_attack ||
                            cq->pump_attacked_s[own_f] > 0.0;
                        if (attacked && own_frac > rdp.regroup_frac_arm &&
                            own_frac <= rdp.regroup_frac_lip) {
                            want = true;  // STAGE 0 — his literal trigger
                            reason = 1;
                        } else if (rdp.regroup_return_deck_run &&
                                   // ★ D3: the RETURN is gated on the master
                                   // dead switch too. Without this,
                                   // raid_ballistic_climb_agl_m = 0 would
                                   // still fly the homecoming — and a stage-1
                                   // return ALONE is a DIFFERENT machine
                                   // (measured -6.4% pressure), so the OFF
                                   // arm would not be the pre-D3 machine and
                                   // the walk-back would be a lie.
                                   rdp.raid_ballistic_climb_agl_m > 0.0 &&
                                   ro.active &&
                                   ro.deck_run && ro.home_alive &&
                                   d.ballistic ==
                                       drone::BallisticPhase::NONE &&
                                   own_frac > 1.0 &&
                                   own_frac <= rdp.regroup_frac_lip) {
                            // STAGE 1 — the designated deck runner, just
                            // outside its own live dome, goes home first.
                            // ⚠ THE LIP BOUNDS THE COST: past 1.35 r_eff the
                            // runner is committed and the S1-DECK direct route
                            // stands. Without it, a runner already halfway
                            // across would be turned around from anywhere.
                            want = true;
                            reason = 2;
                        }
                        if (want) d.regroup_s = 0.0;
                    }
                    if (want) {
                        glm::dvec3 pdir{0.0};
                        want = drone::regroup_dir(d.curr.position, own_cdir,
                                                  own_maj, own_a, own_b, ap.R,
                                                  rdp.regroup_pull_frac, pdir);
                        if (want) {
                            // T2b: the regroup aim point's AGL is measured off
                            // the kernel's contact surface, never the raw DEM
                            // field -- an order that plants a target inside a
                            // drawn hillside is an order to auger in.
                            const double gr =
                                (env != nullptr && env->ground != nullptr)
                                    ? sim::air_ground_radius(*env, pdir)
                                    : ap.R;
                            d.regroup.active = true;
                            d.regroup.reason = reason;
                            d.regroup.target_pos =
                                pdir * (gr + rdp.regroup_agl_m);
                        }
                        // want false here = degenerate geometry: NO aim point
                        // is formed, so NO order is armed. Never a normalize()
                        // of a zero vector into the plant (the guard-every-
                        // normalize law).
                    }
                    if (!want) {
                        // A RETURN is ONE-SHOT per raid-order lifetime,
                        // completed or timed out: the phase advances on the
                        // RELEASE edge, so the runner can never be turned
                        // around twice. RUNG D3 gave the CLIMB a body.
                        if (d.regroup.active && d.regroup.reason == 2) {
                            d.ballistic = drone::BallisticPhase::CLIMB;
                            d.ballistic_s = 0.0;
                        }
                        d.regroup = drone::RegroupOrder{};
                        d.regroup_s = 0.0;
                    }

                    // ---- ★★★ RUNG D3 — THE BALLISTIC DECK RUN (stages
                    // 2-5). Chad: "climb to a decent altitude and then
                    // parabolic dive to the deck to cross the no air zone to
                    // the enemy bubble to attack."
                    //
                    // THE APP OWNS THE TRANSITIONS (it alone holds conquest,
                    // the live ellipses and the ground); drone/ owns only the
                    // steering each phase asks for. Monotone: every edge here
                    // ADVANCES, and the ONLY way back to NONE is the reset
                    // above (order dropped / routed to the bore / respawn).
                    //
                    // ★ INVALIDATION IS ALREADY COMPLETE AT HEAD: pump death,
                    // raider-mask churn and a router re-route all clear
                    // ro.active or stamp via_tunnel, and that reset zeroes the
                    // phase; respawn resets it in drone.h. A run that loses
                    // its order mid-air ends SAFELY by existing construction
                    // (air-seek + deck band + leash).
                    if (rdp.raid_ballistic_climb_agl_m > 0.0) {
                        const double agl =
                            (env != nullptr && env->ground != nullptr)
                                ? glm::length(d.curr.position) -
                                      sim::air_ground_radius(
                                          *env,
                                          glm::normalize(d.curr.position))
                                : 0.0;
                        // The ballistic sequence never runs while a higher
                        // order owns the aeroplane. ENGAGED is excluded for
                        // ARMING only — Chad ruled pursuit unlimited, so a
                        // runner jumped mid-climb yields the steering to the
                        // fight (the raid branch already does), keeps its
                        // phase, and resumes on disengage.
                        const bool bal_free =
                            !d.defend.active && !d.regroup.active &&
                            !maverick::is_tunnel_mode(d.mav.mode);
                        if (d.ballistic == drone::BallisticPhase::NONE &&
                            bal_free && !d.engaged && ro.active &&
                            ro.deck_run && !ro.via_tunnel && ro.home_alive &&
                            own_home_alive && own_frac <= 1.0) {
                            // ARM THE CLIMB. A runner already INSIDE its own
                            // dome skips the RETURN — "go back to whatever
                            // bubble IF THEY HAVE ONE"; it already has one and
                            // is in it. (Outside it, frac > 1.0, the stage-1
                            // RETURN above arms instead and hands off here on
                            // its release edge. The two are disjoint by the
                            // frac test, so a runner can never do both.)
                            d.ballistic = drone::BallisticPhase::CLIMB;
                            d.ballistic_s = 0.0;
                        } else if (d.ballistic ==
                                   drone::BallisticPhase::CLIMB) {
                            d.ballistic_s += ap.sim_dt;
                            // ★★★ THE COMMIT. Height AND heading, or the
                            // timeout. The alignment is HORIZONTAL (see
                            // drone::horizontal_align — a 3D dot against a
                            // horizontal bearing cannot exceed cos(26 deg) in
                            // a 26 deg climb, so a 3D gate could never pass
                            // and every dive would commit by timeout from
                            // ~3.5 km: the E16 higher-is-worse trap).
                            // ★ THE TIMEOUT ADVANCES, IT NEVER ABORTS: a
                            // shadowed or slow climb dives from whatever
                            // altitude it reached. E15's "range limit wearing
                            // a timeout's clothes" is the shape this refuses.
                            const double align = drone::horizontal_align(
                                d.curr.position, d.curr.velocity,
                                ro.target_pos);
                            if ((agl >= rdp.raid_ballistic_climb_agl_m &&
                                 align >= rdp.raid_ballistic_align_min) ||
                                d.ballistic_s >
                                    rdp.raid_ballistic_climb_max_s) {
                                d.ballistic = drone::BallisticPhase::DIVE;
                            }
                        } else if (d.ballistic ==
                                   drone::BallisticPhase::DIVE) {
                            // ARRIVED AT THE DECK. ⚠ THE THRESHOLD MUST SIT
                            // ABOVE WHERE THE TERRAIN LATCH ARMS (eff_agl =
                            // agl - avoid_lookahead_s * sink < 60, i.e. agl
                            // ~287 m at 245 m/s and -18 deg) or the handoff
                            // happens INSIDE a latched 26 deg panic climb with
                            // the guns muted. Handing over at 400 gives the
                            // elevation law the last 300 m.
                            //
                            // ★★★ AND IT IS ALTITUDE ALONE — NO DECK-SCOPE
                            // CONJUNCT — BECAUSE THE FIRST BUILD HAD ONE AND
                            // THE PROBE CAUGHT IT. Chad's dive STARTS INSIDE
                            // HIS OWN DOME, where deck_scope is false by
                            // construction (the air at 400 m is full). With
                            // `d.deck_scope &&` in this clause the phase could
                            // not advance until the runner had flown all the
                            // way out of its own bubble, so it held the
                            // committed -18 deg — with the track law excluded
                            // — straight down into its own back yard and
                            // porpoised off the in-dome terrain latch for a
                            // MEASURED 161 s (9,690 DIVE ticks, gun-mute 22.3%
                            // against a 2.2% fleet baseline). The dive ends
                            // when it reaches the deck. That is all it ever
                            // meant.
                            if (agl < rdp.raid_ballistic_run_agl_m) {
                                d.ballistic = drone::BallisticPhase::RUN;
                            }
                        } else if (d.ballistic == drone::BallisticPhase::RUN) {
                            // "CROSS ... TO THE ENEMY BUBBLE TO ATTACK." The
                            // sprint ends when he ARRIVES — inside the enemy
                            // dome — and it is graded on the SAME shared
                            // drone::ellipse_frac the leash and the D2 trigger
                            // fly, about the OTHER faction's ellipse. No new
                            // range dial, and no forked geometry.
                            // ⚠ NOT `!d.deck_scope`, which was the first
                            // build: that reads "I am in ANY dome", and the
                            // runner starts in its own, so the sprint ended on
                            // the tick it began.
                            // ⚠ THE ARRIVAL-SPEED TRADE IS ROUTED TO CHAD, NOT
                            // PICKED HERE: the airframe barely bleeds (the
                            // level-flight e-fold is 2m/(rho*S*Cd0) = 15 km,
                            // so 245 -> 123 takes ~10.3 km of dome interior).
                            // The measured arrival speed is in the report.
                            glm::dvec3 ec{0.0, 1.0, 0.0};
                            glm::dvec3 em{1.0, 0.0, 0.0};
                            double ea = 0.0;
                            double eb = 0.0;
                            world::faction_ellipse(
                                1 - combat::maverick_faction(
                                        d.spawn_index,
                                        cq->state.player_faction),
                                grow, ec, em, ea, eb);
                            const bool arrived =
                                ea > 0.0 && eb > 0.0 &&
                                drone::ellipse_frac(d.curr.position, ec, em,
                                                    ea, eb, ap.R) <= 1.0;
                            // ★★★ ...OR AT THE BLEED RANGE, WHICHEVER COMES
                            // FIRST, AND THE RANGE IS DERIVED FROM THE
                            // AIRFRAME rather than dialled. A raider that
                            // sprints all the way to the pump arrives with a
                            // turn radius it cannot spend: the C1 defect at
                            // 2x. Coasting from v_sprint to the errand speed
                            // on parasitic drag alone covers
                            //     mass / (0.5 * rho * S * Cd0) * ln(v1 / v0)
                            // (integrate m dv/dx * v = -0.5 rho S Cd0 v^2),
                            // which on the shipped airframe is 10.3 km for
                            // 245 -> 123. Ending the sprint there means the
                            // bleed happens across the enemy dome's air and
                            // he arrives ON-STATION CAPABLE, which is what
                            // "cross at maximum speed ... to attack" needs
                            // both halves of. Induced drag only shortens it,
                            // so this is the conservative bound.
                            // ⚠ MEASURED, NOT ASSUMED: without it the runner
                            // sprinted 162 s at 238 m/s into the enemy dome
                            // and enemy pressure fell 0.0392 -> 0.0371.
                            double bleed_m = 0.0;
                            const double denom =
                                0.5 * ap.rho * ap.S * ap.Cd0;
                            if (denom > 1e-9 && rdp.raid_speed_target > 0.0 &&
                                rdp.raid_ballistic_dive_speed >
                                    rdp.raid_speed_target) {
                                bleed_m = ap.mass / denom *
                                          std::log(
                                              rdp.raid_ballistic_dive_speed /
                                              rdp.raid_speed_target);
                            }
                            const double pump_rng = glm::length(
                                ro.target_pos - d.curr.position);
                            if (arrived ||
                                (bleed_m > 0.0 && pump_rng < bleed_m)) {
                                d.ballistic = drone::BallisticPhase::SPENT;
                            }
                        }
                    }
                }
                // Deep-pump strike (the black stope): armed for EVERY pilot
                // while the enemy DEEP pump lives — the divert itself only
                // fires from a committed RUN passing the stope
                // (drone::tick's strike block), so this is a standing order,
                // not a beeline. make_pumps ordering: [2] Valley deep,
                // [3] Sudbury deep.
                // RUNG E2.2: the divert envelope comes from config now (the
                // [combat] strike_* keys) instead of the welded StrikeOrder
                // struct defaults no config could reach.
                drone::StrikeOrder so;
                // RUNG E3.2: the slot verdict for THIS pilot's faction. Held
                // (not discarded) inside the maverick machine, so the order's
                // one-shot collapse is never burned while waiting.
                so.order_hold =
                    dw->dparams.strike_concurrent_max > 0 &&
                    on_order_runs[own] >= dw->dparams.strike_concurrent_max;
                so.engage_m = dw->dparams.strike_engage_m;
                so.k_az = dw->dparams.strike_k_az;
                so.k_el = dw->dparams.strike_k_el;
                so.bank_cap = dw->dparams.strike_bank_cap;
                so.gamma_cap = dw->dparams.strike_gamma_cap;
                so.bail_s = dw->dparams.strike_bail_s;
                so.station_range_m = dw->dparams.strike_station_range_m;
                so.station_cos = dw->dparams.strike_station_cos;
                const combat::Pump& dpump = cq->state.pumps[2 + ef];
                if (dpump.alive) {
                    so.active = true;
                    so.target_pos = dpump.pos;
                    so.pump_idx = 2 + ef;
                    // RUNG E2.1 ENTRY DIRECTION: run in from the pilot's OWN
                    // faction mouth. MEASURED off the live net and the live
                    // pump table, never a welded faction->mouth map: each
                    // faction's deep pump is placed kDeepPumpFrac of the
                    // arena's horizontal semi-axis toward that faction's own
                    // mouth (combat::place_deep_pump), so whichever spine end
                    // is nearer THIS pilot's own deep pump is his mouth.
                    // run_dir +1 = Errington(front) -> Murray(back). The
                    // TARGET pump then sits on the FAR side of the arena,
                    // which is the descent the strike_gamma_cap is sized for
                    // — a near-mouth entry roughly doubles it.
                    const world::TunnelNet* net =
                        env != nullptr ? env->tunnels : nullptr;
                    if (net != nullptr && net->arena_on &&
                        !net->spine.empty()) {
                        const glm::dvec3& own_deep =
                            cq->state.pumps[2 + own].pos;
                        const double d_front =
                            glm::length(net->spine.front().pos - own_deep);
                        const double d_back =
                            glm::length(net->spine.back().pos - own_deep);
                        so.entry_dir = d_front <= d_back ? +1 : -1;
                    }
                }
                d.strike = so;
            }
        }
        // game-AI-R4: each drone flies against ITS FOE (the `player`
        // argument of drone::tick has always meant "the target aircraft" —
        // pursue/BFM/lead all read a SimState). Drone foes read a PRE-LOOP
        // snapshot so the outcome cannot depend on fleet iteration order
        // (deterministic, frame-rate independent). No conquest (foe never
        // set) => everyone still fights the player, bit-identical.
        std::vector<sim::SimState> foe_snap;
        foe_snap.reserve(dw->drones.size());
        for (const drone::DroneState& d : dw->drones)
            foe_snap.push_back(d.curr);
        for (drone::DroneState& d : dw->drones) {
            if (d.inert) continue;  // a conquest wreck is frozen (no fly/tick)
            const sim::SimState* target = player;
            if (d.foe >= 0 && d.foe < static_cast<int>(foe_snap.size()))
                target = &foe_snap[d.foe];
            else if (d.foe == drone::kFoeNone && cq != nullptr && !furball)
                target = nullptr;  // teams: nobody to fight = no pursuit
            const drone::DroneTickResult tr =
                drone::tick(d, ap, dw->dparams, env, target);
            // FIX-F2 (docs/ai_phase2_fix_spec.md): the vacuum respawn-pen.
            // drone::tick's crash branch respawns via the stock +X scatter
            // (drone::spawn_state), which for the conquest fleet often lands
            // 1-5 km OUTSIDE the Sudbury bubble edge in thin air -- a crashed
            // drone mushes, crashes again, forever (95 dc events/14 min on
            // the tape). RELOCATE a crash respawn into breathable air on the
            // SAME tick. spawn_index is the deterministic golden-angle slot (no
            // rng/clock in the tick path). Null cq => this whole block is
            // skipped => bit-identical (the firewall).
            //
            // ★★★ S4 — THE SECOND TELEPORT PATH, DELETED. The fallback used to
            // read "own dome gone -> the SURVIVING faction's dome (the scramble
            // rule)". That was the quiet copy of the collapse scramble: it
            // teleported a dead-dome faction's pilots into the enemy's bubble
            // one at a time as they died, so deleting only the visible hook
            // would have left the defect running. Chad 2026-08-26: "they
            // respawn from their zone at the deck". The ladder is now
            //   own dome alive  -> own dome, unchanged;
            //   own dome gone   -> OWN ZONE, AT THE DECK (place_on_faction_deck
            //                      — his own home ground, deck_track_agl_m AGL,
            //                      in the global deck air that no dome war can
            //                      delete);
            //   no ground at all-> the stock respawn (a fixture with no world;
            //                      there is no AGL to place against).
            // No branch anywhere in this ladder can now put a pilot in the
            // other faction's air.
            if (tr.respawned && cq != nullptr) {
                const int own = combat::maverick_faction(
                    d.spawn_index, cq->state.player_faction);
                // ★★★ L6 — THE RESPAWN LOCK, THE AI HALF (Chad's ruling R9,
                // 2026-09-03: "no new respawns from EITHER SIDE"). The lock is
                // match-wide, so this is the same one gate for both factions
                // and there is no per-team asymmetry to get wrong.
                //
                // drone::tick has ALREADY re-placed him (its own crash branch
                // calls respawn_in_place before returning `respawned`), so the
                // refusal here is to make him a WRECK rather than to relocate
                // him: `inert` is the frozen-wreck observable the whole app
                // already honours (no fly, no fire, no hits, no draw), and
                // `on_ai_kill` books the roster death with NO score to
                // anybody -- the AI-vs-AI bookkeeping, which is exactly what a
                // terrain crash with no respawn is. mav_alive going false is
                // what the VICTORY-by-elimination condition reads, which is
                // how Chad's "win or lose is determined by elimination"
                // actually resolves.
                //
                // ⚠ KNOWN AND ACCEPTED: the wreck freezes at drone::tick's
                // scatter slot, not at the point of impact, because the reset
                // happened one call ago. It is a frozen, undrawn wreck either
                // way; recovering the impact point would mean snapshotting
                // every drone's state every tick to serve a case that ends the
                // drone's participation in the match.
                if (combat::respawns_locked(cq->state)) {
                    d.inert = true;
                    combat::on_ai_kill(cq->state, d.spawn_index);
                    continue;
                }
                world::FactionGrowth grow[2];
                app::faction_growth_from_state(cq->state, grow);
                // P1-2: ceiling_m is THIS faction's own live, growth-scaled
                // ceiling (the two factions' ceiling_scale are independent
                // fields).
                if (!place_in_faction_air(
                        d, own, grow, ap, *dw, d.spawn_index,
                        grow[own].ceiling_scale * cq->bubble_ceiling_m,
                        cq->bubble_ceil_soft_m))
                    place_on_faction_deck(d, own, ap, *dw, env, d.spawn_index);
            }
        }
        // Advance the lead/time-on-target instrument against the ENGAGED bandit
        // (nearest ahead of the player nose AND within track_range), ONCE per
        // sim tick so the count is frame-rate independent (AT-9). Read-only —
        // only READS st.curr (const) and writes dw->meter, never the player
        // (the firewall). Skipped in RAW mode (the HUD hides the gunsight
        // there, so counting invisibly would pollute the session %).
        if (!in.raw_mode) {
            const glm::dvec3 pnose =
                st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const int eng =
                select_engaged_target(dw->meter.engaged_index, dw->drones,
                                      st.curr.position, pnose, dw->gparams);
            dw->meter.engaged_index = eng;
            const sim::SimState* engaged =
                eng >= 0 ? &dw->drones[eng].curr : nullptr;
            const glm::dvec3 grav =
                ap.g * sim::gravity_dir(st.curr.position);  // world down accel
            render::meter_tick(dw->meter, st.curr, pnose, engaged, dw->gparams,
                               grav);
        }
    }

    // Gun battery (rig-D firing pass): fire + advance the projectile pool ONCE
    // per player tick, in lockstep with the drone/meter advance so the cadence
    // is frame-rate independent (AT-9). Additive + nullptr-gated => the no-gun
    // path is BIT-IDENTICAL to the pre-guns tick (the firewall the mirror-
    // equivalence test pins). READ-ONLY on the player (st.curr const in);
    // nothing here feeds control/sim (the world-thread's one hard rule, §5/§6).
    // Firing is gated to instructor mode + a live airframe (no shot on a
    // spawn/reset tick, matching the GROUNDED zero-Inputs discipline); rounds
    // already in flight keep advancing regardless, so a raw-mode toggle or a
    // respawn never freezes the tracers. gravity_dir/altitude use ap.R, the
    // single-source crash sphere.
    if (gw != nullptr) {
        const bool firing =
            in.fire_held && !in.raw_mode && !st.grounded &&
            !grounded_tick;  // inert on BOTH crash + reset ticks
        // T10: thread the net so rounds fired underground keep flying inside
        // the tunnel/cavern (only rock/core retires them). Null net (no tunnel)
        // => the legacy at/under-R retire.
        const world::TunnelNet* net = env != nullptr ? env->tunnels : nullptr;
        weapon::fire_tick(*gw, st.curr, firing, ap.sim_dt, ap.g, ap.R, net);
    }
    // FLAK (docs/FLAK_GUN_SPEC.md §7.4): the manned ground gun's own
    // battery, advanced on the SAME fixed tick. Absent (nullptr) => not one
    // line runs and every write below lands on fk-owned state -- the
    // defaulted-off arm is bit-identical by construction.
    if (fk != nullptr) {
        const world::TunnelNet* fnet = env != nullptr ? env->tunnels : nullptr;
        flak_tick(*fk, grounded_tick, ap.sim_dt, ap.g, ap.R, fnet);
        // THE BURST SEAM. app/flak_tick.h is the PURE half: it books each
        // self-destruct death as a position (it cannot include combat/, and
        // the headless gate must not need an FxPool). Here -- the one place
        // fk and cw are both in hand -- each booked death becomes a FlakPuff.
        //
        // Drained on the SAME FIXED TICK it was booked, and drained
        // UNCONDITIONALLY (the clear sits outside the cw guard): at 7.5 Hz a
        // 120 Hz tick can retire several rounds inside one rendered frame, so
        // a per-frame drain that kept only the newest would eat the curtain;
        // and a session with a flak world but no CombatWorld must not grow
        // this vector without bound for the whole flight.
        for (const FlakBurst& b : fk->bursts)
            if (cw != nullptr)
                combat::fx_spawn(cw->fx, b.pos, glm::dvec3{0.0},
                                 combat::FxKind::FlakPuff);
        fk->bursts.clear();
    }
    // ── FLAK STAGE D: THE AI GUNNERS (app/flak_ai.h). Every Oerlikon the
    // player is NOT standing at is manned by somebody, and he fights: the
    // SAME slew rates, the SAME lead solver, the SAME drum and reload. Absent
    // (nullptr) or empty => not one line runs, so the arm is bit-identical by
    // construction exactly like fk above.
    //
    // ★ THE DOUBLE-DRIVE GUARD IS THE CALLER'S (main sets ag.fk.manned =
    // false for the gun the player mans), and flak_ai_tick returns on its
    // first line for an unmanned world without touching demand or fire_held.
    // ★ dw may be null (a session with no fleet): then there is nothing to
    // shoot at, and the gun still ticks so its reload timer and its pool of
    // rounds already in the air keep running -- an empty const fleet is the
    // honest input, not a skipped tick.
    if (fk_ai != nullptr && !fk_ai->empty()) {
        const world::TunnelNet* anet = env != nullptr ? env->tunnels : nullptr;
        static const std::vector<drone::DroneState> kNoFleet;
        const std::vector<drone::DroneState>& fleet =
            dw != nullptr ? dw->drones : kNoFleet;
        for (FlakAiGun& ag : *fk_ai) {
            flak_ai_tick(ag.ai, ag.fk, ag.faction, fleet, ap.sim_dt, ap.g,
                         FlakAiParams{},
                         cq != nullptr ? cq->state.player_faction
                                       : combat::CQ_VALLEY);
            flak_tick(ag.fk, grounded_tick, ap.sim_dt, ap.g, ap.R, anet);
            // The same burst seam as the player's gun, and drained the same
            // way (unconditionally, on the tick it was booked).
            for (const FlakBurst& b : ag.fk.bursts)
                if (cw != nullptr)
                    combat::fx_spawn(cw->fx, b.pos, glm::dvec3{0.0},
                                     combat::FxKind::FlakPuff);
            ag.fk.bursts.clear();
        }
    }
    // ★ STING RPAS (the sting lane, 2026-09-03): the flying drone's own tick —
    // the simplified cascade + the frozen plant + the life clocks, all inside
    // app/sting.h. Runs BEFORE the combat sweeps below so this tick's
    // prev->curr segment is the one the ram sweep tests. Defaulted-null =>
    // every existing caller is a strict superset (the FlakWorld pattern). An
    // end reached HERE (battery / turns / manual / ground) draws its boom at
    // the sting's last position; the TARGET end's explosion is the victim's
    // own (combat_tick pass 2) plus the same puff.
    if (sw != nullptr && sw->st.active) {
        // sw->ap is the Sting's OWN derived airframe (main builds it once);
        // the plane's ap below is only the fleet's (respawn path in the
        // sweep).
        const StingEnd se =
            sting_step(sw->st, sw->cmd, sw->sp, sw->ap, cp, env, ap.sim_dt);
        if (se != StingEnd::None && cw != nullptr) {
            combat::fx_spawn(cw->fx, sw->st.curr.position,
                             sw->st.curr.velocity, combat::FxKind::Explosion);
        }
    }
    // Bandit return fire + combat sweeps (bandit_combat_plan.md §3-6). Runs
    // whenever a CombatWorld + fleet are present. PURE / read-only on the
    // player.
    if (cw != nullptr && dw != nullptr) {
        // (5) enemy pool: advance active rounds + retire, THEN spawn this
        // tick's
        //     new rounds for firing bandits (advance-then-spawn, P1-1).
        combat::enemy_fire_tick(*cw, dw->drones, ap.sim_dt, ap.g, ap.R,
                                env != nullptr ? env->tunnels : nullptr);
        // (5b) RUNG S3-GUNS: the COSMETIC pool's advance + retire, same
        //      [t,t+dt] interval, same net-aware retire. Spawning happens
        //      further down at the two abstracted-damage seams (6c and the raid
        //      credit), which is ADVANCE-then-SPAWN, exactly as (5) does it.
        //      This pool is handed to no sweep — see combat/kill.h.
        combat::cosmetic_fire_tick(*cw, ap.sim_dt, ap.g, ap.R,
                                   env != nullptr ? env->tunnels : nullptr);
        // (6a) player rounds -> drones (needs the player battery pool).
        if (gw != nullptr)
            combat::combat_tick(gw->pool, dw->drones, *cw, ap, dw->dparams,
                                ap.sim_dt);
        // (6a-flak) flak rounds -> drones: the same shooter-agnostic sweep;
        // kills route through cw exactly like a player cannon hit. The flak
        // pool is NEVER handed to conquest_tick (own-pump rule, spec §7.4).
        // The two defaulted arms are set HERE and only here: the proximity
        // fuze radius (the shell bursts NEAR the aeroplane, spec §2.2 item 5)
        // and burst_fx (the retire draws a FlakPuff, not a metal HitSpark --
        // a 20 mm HE shell going off in air leaves smoke, and it leaves it
        // whether or not the fuze radius is dialed to 0). The aircraft gun's
        // call above passes neither, so it is bit-identical by construction.
        // clear_kills=false: this is the same player's SECOND sweep this
        // tick -- it must APPEND to killed_spawn_indices, not clear it. The
        // default (clear) here silently erased the aircraft cannon's booked
        // kills before the conquest consumer at (2) read them (Stage D's
        // kill-credit audit; regression window 722957f83..this commit).
        if (fk != nullptr)
            combat::combat_tick(fk->gw.pool, dw->drones, *cw, ap, dw->dparams,
                                ap.sim_dt, fk->prox_radius_m,
                                /*burst_fx=*/true, /*kill_sink=*/nullptr,
                                /*clear_kills=*/false);
        // (6a-sting) STING RAM: the drone's own tick segment swept against
        // the fleet AS a one-round pool — combat_tick verbatim, so the hit
        // geometry, ke_damage, FX, kill credit (the sting is the PLAYER's
        // weapon: pass 2 books killed_spawn_indices / cw.kills exactly like a
        // cannon round, which the conquest consumer below pays), and the
        // maverick inert/respawn law are all the one existing implementation.
        // clear_kills=false: a third sweep of the SAME player this tick —
        // append, never erase (the flak arm's lesson, verbatim). The retired
        // "round" IS the detonation: the sting ends on the target.
        if (sw != nullptr && sw->st.active) {
            sting_ram_round(sw->st, sw->sp, sw->pool);
            combat::combat_tick(sw->pool, dw->drones, *cw, ap, dw->dparams,
                                ap.sim_dt, sw->sp.prox_add_m,
                                /*burst_fx=*/true, /*kill_sink=*/nullptr,
                                /*clear_kills=*/false);
            if (!sw->pool.empty() && !sw->pool[0].active) {
                sw->st.active = false;
                sw->st.end = StingEnd::Target;
            }
        }
        // (6b) enemy rounds -> player, swept vs THIS tick's player segment
        //      [st.prev, st.curr] (a crash/death-reset tick has prev==curr, so
        //      a teleport can't false-hit); skipped while invuln. Decrements
        //      invuln.
        combat::combat_player_tick(*cw, st.prev, st.curr);

        // (6c) game-AI-R4: AI-vs-AI gunnery. A drone in the abstracted
        // AI-vs-AI gun window on a DRONE foe (combat::ai_guns_on — the wide
        // snapshot window, deliberately NOT the player-facing wants_fire
        // discipline; see raid.h) deals the difficulty DPS x kAiVsAiDpsFrac,
        // abstracted (real tracers already fly for the look; rounds only
        // sweep the player). Deaths are booked through on_ai_kill — NOT
        // combat_tick's pass 2 / on_player_kill, which would mis-credit the
        // player's score. Runs AFTER combat_tick so a drone the player
        // already killed this tick is inert and skipped. Damage reads the
        // pre-loop snapshot foes; hp is drained on the LIVE victim.
        if (cq != nullptr) {
            // RUNG E5 (red-team P2-3): the wipe-victory suppression flag must
            // be live BEFORE this sweep's on_ai_kill calls too — the (0)
            // stamp below runs after 6c, so without this line the very first
            // tick of a session could latch a wipe VICTORY that the wave
            // machine would have vetoed. Same dial, idempotent, no-op off.
            // RUNG E6.6: the stamp is now "the ENEMY wing can still be
            // refilled" (pool-aware). pool_n < 0 => identical to the E5 stamp.
            cq->state.reinforcements_live = enemy_waves_live(*cq);
            const combat::DifficultyParams diff =
                combat::difficulty_params(cw->setup.difficulty);
            const double ai_dps =
                diff.bandit_rof_hz * diff.bandit_damage * kAiVsAiDpsFrac;
            for (drone::DroneState& d : dw->drones) {
                if (d.inert || !d.engaged) continue;
                if (d.foe < 0 || d.foe >= static_cast<int>(dw->drones.size()))
                    continue;
                drone::DroneState& victim = dw->drones[d.foe];
                if (victim.inert) continue;
                // E1.2: the AI-vs-AI window's range band is single-sourced
                // from the SAME fire band the player-facing gate uses (the
                // literals in raid.h were a stale copy of it) — the cone stays
                // deliberately wider there.
                if (!combat::ai_guns_on(d.curr, victim.curr,
                                        dw->dparams.fire_range_min,
                                        dw->dparams.fire_range_max))
                    continue;
                // RUNG S3-GUNS: give this point of damage a visible source.
                // Cosmetic only — cadence from the same difficulty table
                // ai_dps is computed from, into a pool no sweep is handed.
                combat::cosmetic_fire(*cw, d, victim.curr.position,
                                      victim.curr.velocity, ap.g, ap.sim_dt);
                victim.hp -= ai_dps * ap.sim_dt;
                // RUNG E7.3: the SECOND damage seam. combat::combat_tick books
                // took_fire for PROJECTILE hits, which is only ever the
                // player's guns — AI-vs-AI fire is this abstract DPS drain and
                // spawns no projectile. Without this line the defensive break
                // arms only against the player, and every drone-on-drone fight
                // in the sky keeps the elevator bob Chad named. Same write-only
                // observation as the other seam: nothing reads took_fire while
                // bfm.defensive_range_m <= 0, so no trajectory moves off.
                victim.took_fire = true;
                if (victim.hp <= 0.0) {
                    combat::fx_spawn(cw->fx, victim.curr.position,
                                     victim.curr.velocity,
                                     combat::FxKind::Explosion);
                    if (cw->respawn_drones) {
                        drone::respawn_in_place(victim, ap, dw->dparams);
                    } else {
                        victim.inert = true;
                        victim.engaged = false;
                    }
                    combat::on_ai_kill(cq->state, victim.spawn_index);
                }
            }
        }
        // (6d) STAGE D: the AI guns' rounds -> drones. Same shooter-agnostic
        // sweep, same prox fuze + burst FX as the player's flak -- but the
        // kills go into a SINK, never into cw.killed_spawn_indices, and are
        // credited through combat::on_ai_kill (the 6c precedent). The player's
        // score, his kill counter and his KILL flash never move for a shot he
        // did not fire. Runs AFTER 6c so a drone already dead this tick is
        // inert and skipped, and after the E5/E6.6 reinforcements_live stamp
        // above so on_ai_kill's outcome eval sees the live flag.
        if (fk_ai != nullptr && !fk_ai->empty()) {
            std::vector<int> ai_flak_kills;
            for (FlakAiGun& ag : *fk_ai) {
                combat::combat_tick(ag.fk.gw.pool, dw->drones, *cw, ap,
                                    dw->dparams, ap.sim_dt,
                                    ag.fk.prox_radius_m, /*burst_fx=*/true,
                                    &ai_flak_kills);
                if (cq != nullptr)
                    for (int idx : ai_flak_kills)
                        combat::on_ai_kill(cq->state, idx);
            }
        }
        combat::fx_tick(cw->fx, ap.sim_dt);

        // (7) Player death -> respawn (P0-A: resolved PER-TICK here, right
        // after the sweeps — NOT after step_frame, else ticks 2..N of a
        // multi-tick frame fly a dead corpse). Death is now COMPONENT-based
        // (Fable Q5): a dead pilot, dead structure, or a broken-off wing kills.
        // Reuses the crash-branch reset so step_frame's existing
        // respawn-neutralization covers the residual ticks; res.respawned
        // signals the caller to reset its device state. Firewall: identical
        // app-level mechanism as a crash — no HP value feeds sim/control.
        if (combat::is_dead(cw->damage)) {
            // ★★★ L6 — THE RESPAWN LOCK, THE OTHER DEATH SITE (Chad's R9).
            // Same law, same resolution, same once-only gate as the crash
            // branch above -- written out rather than factored into a helper
            // because the two branches differ in the death CAUSE they book and
            // in the "DOWNED" HUD flash, and a helper taking both as arguments
            // would be longer than the code it replaced.
            // `is_dead(cw->damage)` stays true forever with no fresh airframe,
            // so the PLAYING gate is what stops the death ledger growing a row
            // a tick.
            const bool respawn_locked_out =
                cq != nullptr && combat::respawns_locked(cq->state);
            if (respawn_locked_out) {
                if (cq->state.outcome == combat::Outcome::PLAYING) {
                    book_player_death(*cw, component_death_cause(*cw), st.curr,
                                      ap, env);
                    cq->state.planes_left = 0;
                    combat::conquest_eval_outcome(cq->state);
                    res.respawn_refused = true;
                    ++cw->deaths;
                    cw->ticks_since_death = 0;  // the HUD "DOWNED" flash, once
                }
            } else {
                // RUNG E6.1: same ledger, the other branch — the cause
                // comes off the damage-source latch (gun / terrain scrape /
                // unattributed).
                book_player_death(*cw, component_death_cause(*cw), st.curr, ap,
                                  env);
                st.curr = player_spawn(PlayerSpawnChoice::Aircraft, ap,
                                       st.spawn_alt, st.spawn_up, st.spawn_fwd);
                st.prev = st.curr;
                st.prev_up = sim::local_up(st.curr.position);
                st.aim.reseed(st.curr.orientation, st.prev_up);
                st.grounded = true;
                res.respawned = true;
                combat::reset_damage(cw->damage);  // fresh airframe (P0-4)
                // derived (= full)
                cw->player_hp = combat::summary_hp(cw->damage);
                cw->player_invuln_ticks = static_cast<int>(
                    cw->setup.respawn_invuln_time / ap.sim_dt + 0.5);
                for (weapon::Projectile& p : cw->enemy_pool)
                    p.active =
                        false;  // deactivate in-flight rounds (anti spawn-camp)
                ++cw->deaths;
                cw->ticks_since_death = 0;  // trigger the HUD "DOWNED" flash
            }
        }
    }

    // CONQUEST wiring (Scarce Skies MASTER_PLAN §4). All tick-driven (AT-9): a
    // pump death rebuilds the live faction bubbles IN THIS TICK so the plant
    // reads the new air next tick regardless of frame rate. cq null => the
    // whole block is skipped => bit-identical (the firewall). Order: player
    // death -> lives; this tick's maverick kills -> score/victory; player
    // rounds -> pumps -> growth -> bubble rebuild.
    if (cq != nullptr) {
        // (0) RUNG E5: stamp the wipe-victory suppression flag FIRST, before
        // any kill is credited this tick — conquest_eval_outcome runs INSIDE
        // on_player_kill/on_ai_kill, so a flag stamped after them would let a
        // same-tick wing wipe latch VICTORY one tick before the wave machine
        // could say the war is not over. Single-sourced from the dial (see
        // ConquestState::reinforcements_live); false when the dial is off, so
        // the write is a no-op and the arm stays bit-identical.
        // RUNG E6.6: pool-aware (see enemy_waves_live). pool_n < 0 => the E5
        // stamp, bit-identical.
        cq->state.reinforcements_live = enemy_waves_live(*cq);
        // (1) A player death/crash respawn spends a life. res.respawned is the
        // single spot BOTH respawn paths share (the crash branch above AND the
        // component-death branch), and it is set ONLY on a death tick (never on
        // the following GROUNDED reset tick) — so this fires once per death.
        if (res.respawned) combat::on_player_death(cq->state);
        // (2) Every maverick combat_tick killed this tick credits a kill (enemy
        // = score, friendly = the fire tax) and latches VICTORY when the last
        // enemy dies. combat_tick already marked the drone inert (no respawn),
        // so the mav_alive book and the drone wreck agree. GUARD MATCHES THE
        // FILL SITE (red-team F1): combat_tick — the only clear+repopulate of
        // killed_spawn_indices — runs under cw && dw && gw, so consuming under
        // a weaker guard would re-credit stale indices every tick in any
        // wiring that omits dw/gw (double score + false VICTORY drive).
        if (cw != nullptr && dw != nullptr && gw != nullptr) {
            for (int idx : cw->killed_spawn_indices)
                combat::on_player_kill(cq->state, idx, cq->params);
        }
        // (2b) RUNG E5 REINFORCEMENT WAVES (Chad 2026-08-20 — the ruling that
        // CONSCIOUSLY supersedes respawn_drones=false; combat/reinforce.h).
        // Runs AFTER (2) so a pilot killed on this very tick is booked dead on
        // the roster BEFORE his own wave clock arms — and after (1) so the
        // player's own death is spent first. Both drone death paths (the player
        // round in combat_tick, the AI-vs-AI sweep above) leave the SAME
        // observable, `inert`, which is what the wave machine arms on, so there
        // is exactly one arming site and the two paths cannot drift.
        // reinforce_delay_s <= 0 => reinforce_tick returns on its first line
        // AND reinforcements_live (0) stays false => bit-identical permanent
        // death.
        if (cw != nullptr && dw != nullptr) {
            const combat::ReinforceParams rfp =
                wave_params(*cq, dw->dparams.hp);
            AirborneWavePolicy shipped_policy;
            shipped_policy.cq = cq;
            shipped_policy.dw = dw;
            shipped_policy.ap = &ap;
            combat::ReinforcePolicy& pol =
                cq->reinforce_policy != nullptr ? *cq->reinforce_policy
                                                : shipped_policy;
            combat::reinforce_tick(cq->reinforce, dw->drones, cq->state, rfp,
                                   pol, ap.sim_dt);
        }
        // (3) Player rounds vs the four pumps (the player's OWN pool; a round
        // already retired by a drone/lamp hit is inactive and skipped). A pump
        // death grows the destroyer's dome — rebuild the live bubbles NOW
        // (tick-driven, not frame-driven) and queue the FX for the app to
        // spawn.
        if (gw != nullptr) {
            const std::size_t n0 = cq->events.size();
            combat::conquest_tick(cq->state, gw->pool, cq->params, &cq->events,
                                  &cq->hit_events);
            if (cq->events.size() != n0) rebuild_conquest_bubbles(*cq);
            // Per-hit sparks (fly-3): visible damage feedback on every pump
            // hit, spawned tick-side into the shared FX pool (cw is non-null
            // in this branch's caller path whenever guns exist; guard anyway).
            if (cw != nullptr) {
                for (const combat::PumpHitEvent& h : cq->hit_events) {
                    combat::fx_spawn(cw->fx, h.pos, glm::dvec3{0.0},
                                     combat::FxKind::HitSpark,
                                     -glm::normalize(h.vel), 0.6);
                }
            }
            cq->hit_events.clear();
        }
        // (4) PUMP RAIDS + DEEP STRIKES (game-AI-R3): every armed raider (both
        // factions, surface pumps) and every striking RUN (deep pumps in the
        // black stope) deals ABSTRACTED on-station DPS (raid_dps_frac x the
        // PLAYER battery DPS — no real enemy ballistics vs pumps) through the
        // SHARED damage_pump death path, the raider's own faction credited as
        // destroyer. Recomputed each tick; tick-driven (AT-9).
        // (3b) SUDDEN-DEATH MATCH CLOCK (Chad, 2026-07-26). Advanced in the SIM
        // TICK (never a frame clock -- AT-9), so 10 minutes is 10 minutes of
        // flight at any frame rate. Armed by damage_pump the moment a faction
        // loses its last pump; crowns the victor at the buzzer. Disarmed
        // (countdown_faction < 0) => a no-op, so a session where nobody loses a
        // bubble is bit-identical to the pre-clock path.
        combat::conquest_countdown_tick(cq->state, ap.sim_dt, cq->params);

        // (3c) ★★★ S4 — DELIBERATELY EMPTY. There used to be a SCRAMBLE ON
        // COLLAPSE hook here: the tick a faction's dome reached 0, every living
        // plane it had was relocated into the surviving faction's dome. Chad
        // 2026-08-26: "this games ai must not teleport but become skilled at
        // deck flying." NOTHING happens on the collapse tick now. Whoever is
        // airborne keeps the air, the orientation and the speed he had; if that
        // combination kills him, it kills him, and that is the ruled outcome,
        // not a defect to be cushioned later. See the S4 banner above this
        // file's placement law.

        cq->pump_under_attack = false;
        cq->raided_pump = -1;
        // Outcome gate (red-team P2): damage_pump's contract makes the caller
        // own the outcome latch — after VICTORY/DEFEAT the AI stops moving
        // score/bubbles.
        if (gw != nullptr && dw != nullptr &&
            cq->state.outcome == combat::Outcome::PLAYING) {
            // game-AI-R3: EVERY armed raider/striker deals on-station DPS to
            // ITS ordered pump — both factions, surface (RaidOrder) and deep
            // (StrikeOrder, only while its RUN is actually diverting in the
            // stope). Same abstracted-DPS model (raid_dps_frac x the player
            // battery DPS), same SHARED damage_pump path, so score / shrink /
            // grow / events / bubble rebuild can never fork. The HUD warning
            // stays keyed to PLAYER-faction pumps (surface or deep) so Chad
            // knows when to defend.
            const int pf = cq->state.player_faction;
            const double per_tick = cq->params.raid_dps_frac *
                                    combat::battery_dps(gw->battery) *
                                    ap.sim_dt;
            const combat::RaidParams rp_surface;
            // RUNG S3-GUNS: non-const now — combat::cosmetic_fire owns a
            // per-drone cosmetic cadence clock (DroneState::cosmetic_cooldown).
            // Nothing else in this loop writes the drone.
            for (drone::DroneState& d : dw->drones) {
                if (d.inert) continue;
                // RUNG E2.2: the DEEP envelope is read off the ORDER this
                // pilot actually flew, so the DPS credit and drone::tick's own
                // E2.3 strafe gate are the same two numbers by construction (a
                // second copy here is exactly how the ai_guns_on band went
                // stale). A default order reproduces combat::strike_params().
                const combat::RaidParams rp_deep = combat::strike_params(
                    d.strike.station_range_m, d.strike.station_cos);
                int pi = -1;
                const combat::RaidParams* rp = nullptr;
                // Arbitration matches the STEERING (red-team P1-1): a
                // committed tunnel run flies the STRIKE, so the raid branch
                // applies only OUTSIDE tunnel modes — else a raider mid-RUN
                // orbits the deep pump crediting nothing (its surface order
                // shadowed the strike, and the surface pump is unreachable
                // underground).
                const bool in_tunnel = maverick::is_tunnel_mode(d.mav.mode);
                if (d.raid.active && !in_tunnel) {
                    pi = d.raid.pump_idx;
                    rp = &rp_surface;
                } else if (d.strike.active && !d.strike_bailed &&
                           d.mav.mode == maverick::MaverickState::Mode::RUN) {
                    pi = d.strike.pump_idx;
                    rp = &rp_deep;
                }
                if (pi < 0 || pi >= 4) continue;
                combat::Pump& tgt = cq->state.pumps[pi];
                if (!tgt.alive) continue;
                if (!combat::raider_on_station(d.curr, tgt.pos, *rp)) continue;
                if (tgt.faction == pf) {
                    cq->pump_under_attack = true;
                    // The raided pump's OWN index (red-team P1-2): the HUD
                    // ring must blink the pump actually under attack, not
                    // send Chad to the surface while the stope is strafed.
                    cq->raided_pump = pi;
                }
                // RUNG S3-GUNS: rounds in the air and sparks on the pump
                // wherever pump HP is actually moving. Cosmetic only: the
                // round carries damage 0.0 and the pool is handed to no sweep
                // — damage_pump below is still the ONLY thing that takes HP
                // off a pump, unchanged, on the same envelope as before.
                if (cw != nullptr) {
                    const glm::dvec3 to_pump = tgt.pos - d.curr.position;
                    const double to_len = glm::length(to_pump);
                    if (combat::cosmetic_fire(*cw, d, tgt.pos, glm::dvec3{0.0},
                                              ap.g, ap.sim_dt) &&
                        to_len > 1e-6) {
                        // Same spark the PLAYER's rounds make on a pump hit
                        // (normal = -dir, scale 0.6), throttled to the round
                        // cadence by cosmetic_fire's own return value.
                        combat::fx_spawn(cw->fx, tgt.pos, glm::dvec3{0.0},
                                         combat::FxKind::HitSpark,
                                         -(to_pump / to_len), 0.6);
                    }
                }
                const std::size_t n0 = cq->events.size();
                combat::damage_pump(
                    cq->state, pi, per_tick,
                    combat::maverick_faction(d.spawn_index,
                                             cq->state.player_faction),
                    cq->params, &cq->events);
                if (cq->events.size() != n0) rebuild_conquest_bubbles(*cq);
            }
        }
    }

    // T5c DESTRUCTIBLE GASLAMPS: shoot out a tunnel gaslamp -> dark spot. Runs
    // AFTER combat_tick so a round that already killed a drone (retired) is not
    // re-tested here (drones win). Uses the player GunWorld pool; a round that
    // hits a lamp is consumed. Lamp deaths PERSIST across crash/respawn (world
    // state — the respawn branch above never touches `lw`). PURE / firewalled:
    // nothing here feeds control/sim. Skipped when no lamp world / gun pool.
    if (gw != nullptr && lw != nullptr && !lw->lamps.empty())
        render::lamp_hits(gw->pool, *lw, render::kLampHitRadius);
    return res;
}

// The per-FRAME input the caller resolves once from the frame-scope device
// sample (SPEC §10: sample devices once per frame). All RESOLVED values — no
// raylib device objects — so step_frame stays PURE/clock-free and the test can
// drive it headless. The per-frame mouse delta is NOT here: it is accrued by
// the caller across frames (no loss when render fps > sim) and passed by
// reference so a 0-tick frame carries it.
struct FrameInput {
    bool raw_mode = false;
    sim::Inputs raw_in{};  // used iff raw_mode
    double throttle = 0.0;
    // MB-flaps device passthrough (see TickInput; forwarded per tick).
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    double wheel_brake = 0.0;  // R4g brake passthrough (see TickInput)
    bool freelook_held = false;
    bool fire_held = false;  // LMB fire, forwarded to every tick this frame
                             // (rig-D guns; neutralized on a mid-frame respawn)
    // S-orient (docs/comfort program Q3): the caller resolves the OrientTap
    // ONCE per frame (from the same freelook press-edge freelook reads) and
    // offers the fire on the FIRST tick of the frame only (step_frame gates
    // it), so a discrete double-tap fires exactly one orient event regardless
    // of how many ticks the frame carries. Default false => bit-identical.
    bool orient_cmd = false;
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    // RMB-zoom mouse-gain scale (forwarded to every tick this frame). Defaulted
    // 1.0 => the pre-zoom frame is bit-identical (strict superset). See
    // TickInput::aim_gain_scale for why it is a binary button function, not the
    // eased fov (frame-rate independence).
    double aim_gain_scale = 1.0;
    // Vestigial (S7-raw removed the F1 screen-relative mouse — the mouse now
    // rotates the RAW carried aim frame §9.1). Retained only so the
    // FrameInput->TickInput forwarding stays uniform; app::tick no longer reads
    // them. The camera-render path uses main.cpp's OWN cam_fwd/cam_up, not
    // these.
    glm::dvec3 cam_fwd{0.0, 0.0, 0.0};
    glm::dvec3 cam_up{0.0, 0.0, 0.0};
};

struct FrameResult {
    int ticks = 0;           // whole fixed ticks stepped this frame
    bool respawned = false;  // a crash-reset fired during THIS frame
    // ★★★ L6: some tick of THIS frame refused the player a respawn (the
    // respawn lock). The caller draws the reason once.
    bool respawn_refused = false;
    // S-orient (docs/comfort program Q3): the ORIENT verb fired on some tick of
    // THIS frame — the caller hard-cuts its lagged cam_fwd :=
    // loop.aim.forward() (research REC-1: angular snap, position/FOV keep
    // smoothing). Default false.
    bool orient_fired = false;
    double consumed_dx = 0.0;  // mouse delta consumed into the aim this frame
    double consumed_dy = 0.0;  //   (0 on a 0-tick frame) — for the caller's
                               //   freelook-orbit camera (cosmetic, §9.2)
    // S-aimff instrument (the AT-9 companion): SUM over this frame's ticks of
    // the controller-seen aim rate * sim_dt — the smear INVARIANT (equal for
    // the same total mouse delta at any frame/tick partition). Consumed by
    // the frame-rate-integral test leg (test_aim_ff), never by the app/HUD —
    // an asserted instrument field, not an M1-class dead report.
    glm::dvec3 aim_rate_dt_sum{0.0};
    // No telem here: the HUD reads SimState (control::extract +
    // sim::load_factor on last_vhat, S5), never the frame's last-tick telem — a
    // FrameResult.telem would be a dead report field (the M1-class hole waiting
    // to happen).
    //
    // rig-B (render-only, RA9): the Inputs FED TO sim::step on the LAST tick of
    // this frame — the commanded surface deflection the Fleet Rig poses its
    // control surfaces from (fleet_rig_plan.md rig-B; also consumed by the v5
    // render/rig-D port + the felt-flight recorder). Captured from
    // TickResult.inputs, which is MODE-COMMON (set in BOTH the raw and
    // instructor branches, and neutralized on a mid-frame respawn — Fable
    // before-consult P1: capture at the step feed, not an instructor-only
    // value). A 0-tick frame leaves it default {}; the caller carries the
    // previous command. A pure report write — cannot touch LoopState, so the
    // AT-9 mirror stays bit-identical (pinned in test_at9).
    sim::Inputs last_inputs{};
    // R4-FLY-6: touchdown report aggregated across this frame's ticks (a
    // landing is a once-per-approach event — if two capture edges somehow
    // land in one frame, the last wins; same report-only discipline).
    bool touchdown = false;
    glm::dvec3 touchdown_pos{0.0};
    double touchdown_speed = 0.0;
};

// One render frame (SPEC §10, the accumulator loop): advance the fixed-dt
// accumulator by frame_dt and run that many whole ticks through app::tick. The
// plant NEVER sees frame_dt — this is the frame-rate independence AT-9 verifies
// end-to-end (only the accumulator MECHANISM was unit-pinned before, in
// test_accumulator).
//
// The per-frame mouse delta (`pending_*`, by reference) is OFFERED on the one
// consuming tick and zeroed there; a 0-tick frame leaves it to carry to the
// next (no loss when render fps > sim). app::tick's CQ2 gate drops the aim's
// copy when freelook holds it; the caller routes `consumed_*` to the
// freelook-orbit camera.
//
// Crash-reset neutralization stays INSIDE the seam (Fable AT-9 consult P0-2):
// if a crash fires on tick 1 of a 4-tick frame, the remaining ticks of THIS
// frame must fly the fresh airframe on a DEAD stick (F2) — reborn at cruise
// power, not steered by the pre-crash input. Deferring that to the caller would
// leave those residual ticks flying the dead life's stick, and HOW MANY
// residual ticks there are is frame-rate dependent (3 at 30 fps, 0 at 240) —
// the exact frame-rate dependence AT-9 exists to forbid. `respawned` is still
// returned so the caller resets its PERSISTENT device state (the virtual
// stick), which step_frame, being device-free, cannot touch.
// F9 RECORD (v5 kernel-v5-reconcile): an OPTIONAL per-TICK hook, defaulted
// null — a strict superset (every existing caller/test is unchanged, the
// AT-9 frame tests stay bit-identical: the hook fires nowhere in any of
// them). step_frame hides the tick loop from main.cpp, and the felt-flight
// recorder taps PER TICK (SPEC-mandated: AT-9 forbids a frame-quantized
// control signal, and a per-FRAME tap under-samples/misrepresents a
// multi-tick frame) — so the hook, not a per-frame report field, is the
// seam. Called once per tick with the TickInput actually used and the
// LoopState AFTER that tick (post-tick(), including any respawn this tick
// caused) — never the neutralized `cur` (that is the NEXT tick's input, not
// this one's).
// v2 (S-rollmix graft): the hook carries the tick's Telemetry so the felt-
// flight recorder can pin blend/held_bank (recorder v2 columns) — the tape
// A/B instrument for the blend-band threads.
using TickHook = void (*)(const TickInput&, const LoopState&,
                          const control::Telemetry&, void*);

// CONQUEST TAPE (docs/conquest_tape_spec.md §1): a SECOND, independent
// per-tick hook alongside TickHook above — added for the AI primary-data
// recorder, never touching the felt-flight seam. Called once at the END of
// each tick iteration (after tick() returns and after the existing TickHook
// call), with the SAME dw/cw/cq/env pointers step_frame itself received (not
// the neutralized `cur` — this tick's own view). Defaulted null => zero
// behavior change (every existing call site compiles/behaves unchanged).
using ConquestTapeHook = void (*)(const TickInput& in, const LoopState& st,
                                  const DroneWorld* dw,
                                  const combat::CombatWorld* cw,
                                  const ConquestWorld* cq,
                                  const sim::Environment* env, void* ctx);

inline FrameResult step_frame(
    LoopState& st, Accumulator& accum, double frame_dt, const FrameInput& fin,
    double& pending_dx, double& pending_dy, const sim::AircraftParams& ap,
    const control::ControllerParams& cp, const sim::Environment* env,
    DroneWorld* dw = nullptr, weapon::GunWorld* gw = nullptr,
    combat::CombatWorld* cw = nullptr, render::TunnelLampWorld* lw = nullptr,
    TickHook tick_hook = nullptr, void* tick_hook_ctx = nullptr,
    ConquestWorld* cq = nullptr, ConquestTapeHook cq_hook = nullptr,
    void* cq_hook_ctx = nullptr, FlakWorld* fk = nullptr,
    std::vector<FlakAiGun>* fk_ai = nullptr, StingWorld* sw = nullptr) {
    FrameResult res;
    res.ticks = accum.advance(frame_dt);

    // A mutable copy of the frame input: a mid-frame respawn neutralizes it in
    // place so the remaining ticks fly neutralized (F2 / P0-2, above).
    FrameInput cur = fin;
    bool mouse_consumed = false;
    // S-aimff: the consuming tick's computed aim rate, forwarded (ZOH) into
    // every remaining tick of THIS frame — the smear that keeps the FF demand
    // integral frame-rate independent. Zeroed on a mid-frame respawn with the
    // rest of the resolved input (a dead life's hand must not steer the fresh
    // airframe).
    glm::dvec3 frame_aim_rate{0.0};
    for (int t = 0; t < res.ticks; ++t) {
        TickInput in;
        in.raw_mode = cur.raw_mode;
        in.raw_in = cur.raw_in;
        in.throttle = cur.throttle;
        in.flap_cmd = cur.flap_cmd;  // MB-flaps forward (mirror leg in AT-9)
        in.gear_cmd = cur.gear_cmd;
        in.wheel_brake = cur.wheel_brake;  // R4g brake, per-tick forward
        in.freelook_held = cur.freelook_held;
        in.fire_held =
            cur.fire_held;  // LMB fire (rig-D guns), per-tick forward
        for (int i = 0; i < 3; ++i) {
            in.override_mask[i] = cur.override_mask[i];
            in.override_sign[i] = cur.override_sign[i];
        }
        in.cam_fwd =
            cur.cam_fwd;  // vestigial (S7-raw); forwarded for uniformity
        in.cam_up = cur.cam_up;
        in.aim_gain_scale = cur.aim_gain_scale;  // RMB-zoom mouse-gain scale
        // S-aimff: ticks after the consuming one carry the frame's smeared
        // rate (0 until the consuming tick computes it below).
        in.aim_rate_ff = frame_aim_rate;
        // Consume the accrued mouse on the FIRST tick of the frame (per-frame
        // delta, per-tick transport — the S5 mouse/tick split). Report it for
        // the caller's orbit; app::tick's CQ2 gate owns whether it reaches aim.
        const bool consuming = !mouse_consumed;
        if (consuming) {
            in.aim_dx = pending_dx;
            in.aim_dy = pending_dy;
            res.consumed_dx = pending_dx;
            res.consumed_dy = pending_dy;
            pending_dx = pending_dy = 0.0;
            // S-aimff: the consuming tick spreads its applied rotation over
            // the WHOLE frame (rate = rotation/(N*sim_dt), app::tick).
            in.frame_ticks = res.ticks;
            // S-orient: offer the double-tap fire on the FIRST tick of the
            // frame only (a discrete event fires exactly once, not once per
            // interleaved tick). `cur.orient_cmd` is cleared on a mid-frame
            // respawn below with the rest of the resolved input.
            in.orient_cmd = cur.orient_cmd;
            mouse_consumed = true;
        }

        const TickResult r =
            tick(st, in, ap, cp, env, dw, gw, cw, lw, cq, fk, fk_ai, sw);
        if (tick_hook != nullptr) tick_hook(in, st, r.telem, tick_hook_ctx);
        // CONQUEST TAPE: fires AFTER the felt hook above, once per tick
        // iteration, with the SAME dw/cw/cq/env pointers this step_frame call
        // received (docs/conquest_tape_spec.md §1). Null by default => a
        // strict superset, structurally unreachable on every existing caller.
        if (cq_hook != nullptr) cq_hook(in, st, dw, cw, cq, env, cq_hook_ctx);
        // rig-B (render-only): the Inputs actually fed to sim::step this tick
        // (mode-common; on a respawn tick `r.inputs` is the neutralized dead
        // stick, matching the caller's respawn reset). Last tick wins.
        res.last_inputs = r.inputs;
        if (r.touchdown) {
            res.touchdown = true;
            res.touchdown_pos = r.touchdown_pos;
            res.touchdown_speed = r.touchdown_speed;
        }
        // v4 S-orient / S-aimff (grafted onto the R6+ballistics tick call):
        res.orient_fired = res.orient_fired || r.orient_fired;
        // S-aimff: forward the consuming tick's computed rate to the rest of
        // the frame; every tick's controller-seen rate integrates into the
        // smear-invariant instrument. Red-team P2-3: the forwarded WORLD
        // vector is ZOH'd, not re-transported across ticks 2..N — the frame
        // rotates under it by <= ~4e-4 rad of misalignment over 3 ticks at
        // V = 220 (curvature + maneuver rate), far under the FF's own
        // resolution — accepted.
        if (consuming) frame_aim_rate = r.aim_rate_ff;
        res.aim_rate_dt_sum += r.aim_rate_ff * ap.sim_dt;
        if (r.respawn_refused) res.respawn_refused = true;  // ★ L6
        if (r.respawned) {
            res.respawned = true;
            // Neutralize the resolved input for the rest of THIS frame: fresh
            // airframe, dead stick, reborn at cruise power (st.curr is now the
            // spawn state — read its throttle, single source, F2).
            cur.raw_in = sim::Inputs{};
            cur.raw_in.throttle = static_cast<float>(st.curr.throttle);
            cur.throttle = st.curr.throttle;
            cur.flap_cmd = 0.0;  // spawn state is clean (flap/gear up)
            cur.gear_cmd = 0.0;
            cur.wheel_brake = 0.0;  // reborn stick holds no brake
            cur.freelook_held = false;
            cur.fire_held = false;  // dead trigger: no shot from a reborn stick
            cur.orient_cmd = false;  // a respawn cancels a pending orient event
            for (int i = 0; i < 3; ++i) {
                cur.override_mask[i] = false;
                cur.override_sign[i] = 0.0;
            }
            pending_dx = pending_dy = 0.0;
            frame_aim_rate = glm::dvec3{0.0};  // dead hand off the fresh life
        }
    }
    return res;
}

// Focus loss (SPEC §9.5): drop the freelook hold and aim := nose — via
// snap_forward_to_nose, which KEEPS the carried-up holonomy (NOT reseed, which
// would re-seed up from local_up and level the camera roll; the S5 lesson). The
// caller additionally clears held device keys and the pending mouse delta.
inline void instructor_focus_loss(LoopState& st) {
    st.fl.reset();
    st.recov.reset();   // an alt-tab mid-roll must not resume the roll (S7-hrz)
    st.aim_rest = 0.0;  // nor bank a rest dwell across the alt-tab (v13)
    st.recov_armed = false;  // the next rest edge re-captures honestly
    st.prev_path = glm::dvec3{0.0};  // path-rate memory dropped too (v13d)
    st.aim.snap_forward_to_nose(st.curr.orientation);
}

// The §9.2 chase-camera call site. `cam_forward` is the caller's lagged,
// velocity-anchored camera-forward (S7-cam Phase 1) — decoupled from the aim,
// so a keyboard yank no longer snaps the view. `cam_up` is the caller's CARRIED
// aim-frame up (S7-cam3, 2026-07-07 — reverses the S7-cam2 horizon-lock):
// `cam_up = loop.aim.up()`, so the camera shows the world from the aim/mouse
// frame's orientation and mouse-up == screen-up at any attitude (the
// post-maneuver mouse-inversion was the horizon-locked camera diverging from
// the carried mouse frame). It rolls with the aim through a loop; pole-free
// (never degenerates at the zenith). aim_chase_camera re-orthogonalizes it
// against cam_forward. Both are DOWNSTREAM of mouse->aim (the aim frame stays
// the raw §9.1 mouse basis), so RA9 holds. Extracting the call keeps the
// camera-up source pinnable.
inline render::CameraPose instructor_camera(const sim::SimState& draw_state,
                                            const glm::dvec3& cam_forward,
                                            const glm::dvec3& cam_up,
                                            const sim::AircraftParams& ap,
                                            const render::ChaseParams& chase,
                                            const render::CameraOrbit& orbit,
                                            bool underground = false) {
    return render::aim_chase_camera(draw_state, cam_forward, cam_up, ap, chase,
                                    orbit, underground);
}

}  // namespace app
