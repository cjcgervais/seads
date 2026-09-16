#pragma once
// ★★★ L1 — THE OUTER PLAYER MODE MACHINE
// (docs/PLAN_20260901_game_loop_millwright.md §3).
//
// WHAT THIS IS, AND WHAT IT IS DELIBERATELY NOT.
//
// Before this file `app/main.cpp` carried the player's whole mode as TWO loose
// bools -- `drive_mode` and `sled_seeded` -- read at a dozen sites across seven
// thousand lines. Two bools give four states, three of which were meaningful
// and one of which ("not driving, but a machine exists") had no name and no
// owner. The millwright loop adds three more states (on foot, repairing, on the
// gun), and four unnamed states would have become thirty-two.
//
// ⚠ IT IS AN OUTER MACHINE AND IT ASKS, IT NEVER ANSWERS. `sim::WalkerMode`
// stays the SOLE authority for the on-foot sub-state (Falling / Buried / Down /
// CrawlProne / CrawlKnees / Afoot). Nothing here ever decides whether the man
// is upright, out of the snow, or able to move -- the walker owns that, it is
// frozen kernel, and a second opinion about it here would be the transcribed
// second copy this repo has paid for twice (sim/walker.h's own banner).
// `PlayerMode::Afoot` means "the player's INPUT goes to the man", not "the man
// is standing".
//
// ⚠ AND IT HOLDS NO GEOMETRY. Every gate in this file is a bool the caller
// computed from a position (app/interact.h resolves those). That is what keeps
// the transition table a pure, glm-free, raylib-free thing a test can walk
// exhaustively -- the mode ladder's own recorded disease is a rung that could
// only be exercised by running the game.

namespace app {

// The OUTER mode. `Pilot` is the whole of the game before this rung.
enum class PlayerMode : int {
    Pilot = 0,  // in the aircraft (the only state that existed before L1)
    Sled,       // on the snowmachine, hands on the bars
    Afoot,      // the Sudburian on his own feet
    Repairing,  // afoot, at a pump, turning the wrench (progress is L2)
    OnGun,      // on the flak gun (the site kind exists; the gun is L5)
    // ★ STING RPAS (the sting lane, 2026-09-03): the keys are the DRONE's.
    // The man's BODY stays exactly where he launched from — like OnGun, this
    // names who the input belongs to, never where anybody is standing.
    Drone,
};

// ★★★ L10 — WHAT HE IS FIXING (Chad 2026-09-07: "fix the airplane engine ...
// the same way the sudburian can fix the pump").
//
// ⚠ AND WHY THIS IS NOT A SIXTH `PlayerMode`. `Repairing` answers "whose keys
// are these and what is the man doing" -- and the man is doing the identical
// thing in both jobs: standing at a broken machine turning a wrench, in the
// same work pose, on the same key, out by the same key, ended by the same walk
// away. A `FixingEngine` mode beside `Repairing` would duplicate every arm of
// the table that mentions repair, and the next reader would have to prove the
// two copies agreed. The MODE is the posture; the TARGET is the noun.
enum class RepairTarget : int {
    None = 0,
    Pump,    // an own surface pump (combat/pump_repair.h)
    Engine,  // the parked aeroplane's engine (combat/engine_repair.h)
};

struct PlayerModeState {
    PlayerMode mode = PlayerMode::Pilot;
    // ★ THE MACHINE PERSISTS. Once a snowmachine has been seeded it has a
    // world position for the rest of the session whether or not anybody is on
    // it -- this bool is "a machine exists in the world", NOT "the player is
    // riding it". Those two were the same bit before L1 and that is exactly
    // what made the fourth state unnameable.
    bool sled_seeded = false;
    // ★★★ THE L2 HOOK, NAMED NOW SO L2 ADDS NO STATE. `repairing` is the outer
    // machine's own bit (mode == Repairing) published under a name the repair
    // rung can read, and `repair_pump` is WHICH pump he is working. L1 sets
    // and clears both; L1 runs no progress clock and writes no hp.
    bool repairing = false;
    int repair_pump = -1;
    // ★★★ L10: WHICH JOB. `repair_pump` stays the pump's index and is -1 for
    // every other target -- it was never a "repair id", it was always "which
    // pump", and widening it into one would make `repair_pump == 0` mean two
    // different things. Set and cleared beside `repairing` at every single
    // site that touches it, which is the invariant `player_mode_force` exists
    // to hold.
    RepairTarget repair_target = RepairTarget::None;
    // ★ STING FROM THE SEAT (Chad 2026-09-04): the launcher deploys from
    // the man's feet OR from the snowmachine's seat, and the flight's end
    // puts him back where he launched from. Written by DeployDrone only.
    PlayerMode drone_home = PlayerMode::Afoot;

    // --- the reads that replace `drive_mode` / `sled_seeded` -----------------
    // ★ `off_aircraft` IS THE OLD `drive_mode`, EXACTLY. The aircraft gets a
    // dead stick in every state but Pilot -- including on foot, where nobody is
    // in the cockpit at all.
    bool off_aircraft() const { return mode != PlayerMode::Pilot; }
    bool driving() const { return mode == PlayerMode::Sled; }
    // On his feet as far as INPUT is concerned. Repairing is a man standing at
    // a pump, so his body is still the thing WASD moves.
    bool on_foot() const {
        return mode == PlayerMode::Afoot || mode == PlayerMode::Repairing;
    }
};

// What the player just pressed. One event per key, so the key table lives in
// `app/main.cpp` and the LAW lives here.
enum class ModeEvent : int {
    MountKey = 0,  // J -- context mount / dismount
    InteractKey,   // F -- fix the pump
    GunKey,        // O -- man the flak gun
    // ★ STING: fired by main.cpp at the LAUNCH CLICK (Afoot -> Drone) and at
    // the flight's END (Drone -> Afoot). The P key itself is app-side UI (it
    // shoulders/cancels the launcher and, in flight, asks the drone to
    // detonate); only the two real mode changes come through the table.
    DroneKey,
};

// What the caller must DO about the transition it just asked for. The state is
// already updated when this comes back; the action is the app-side glue that
// only `main()` can run (seeding a machine, sampling the ground, opening a
// tape). Splitting it this way is what makes the table testable without a
// window: the decision is here, the effect is there.
enum class ModeAction : int {
    None = 0,
    SeedAndMount,          // Pilot -> Sled: today's J, unchanged
    RefuseMountMoving,     // the aircraft is not parked (today's refusal)
    Dismount,              // Sled -> Afoot: place the man beside the machine
    RefuseDismountMoving,  // you do not step off a moving snowmachine
    Mount,                 // Afoot -> Sled: through player_mount_request()
    BoardAircraft,         // Afoot -> Pilot: back into the parked aeroplane
    BeginRepair,           // Afoot -> Repairing
    EndRepair,             // Repairing -> Afoot (F again, or out of reach)
    // ★★★ THE JOB THAT ENDED BECAUSE IT WAS DONE. `EndRepair` and this are
    // the same two writes and a DIFFERENT SENTENCE, and separating them is not
    // cosmetic: the site table drops a pump the instant it is whole, so the
    // frame AFTER a successful fix has `pump_in_reach == false` and the
    // out-of-reach arm below fired -- overwriting "PUMP BACK ON LINE" with
    // "FIX ABANDONED" on the central action of the whole rung. The mode table,
    // not `main()`, owns which sentence is true. (Red-team finding, 2026-09-01.)
    FinishRepair,          // Repairing -> Afoot because the pump reached full
    ManGun,                // Afoot -> OnGun
    LeaveGun,              // OnGun -> Afoot
    DeployDrone,           // Afoot|Sled -> Drone: the sting leaves the stock
    EndDrone,              // Drone -> drone_home: the flight ended (any reason)
};

// Everything the table is allowed to know about the world, resolved by the
// caller. Bools, not positions -- see the banner.
struct ModeContext {
    // The aircraft is on the ground and slow enough to leave (today's
    // `can_mount`). ★ This is the law WINTER_LAW §1 actually rejects -- you may
    // not abandon a flying aeroplane -- and it is preserved verbatim.
    bool aircraft_ready = false;
    bool sled_seeded = false;
    // |v| below the dismount limit. You do not step off a moving machine.
    bool sled_stopped = false;
    // What is within reach of the MAN right now (app/interact.h resolved it).
    bool sled_in_reach = false;
    bool aircraft_in_reach = false;
    bool pump_in_reach = false;
    int pump_index = -1;
    // ★ L10: he is standing at the parked aeroplane AND its engine is not
    // whole. Both halves are the caller's (app/interact.h registers the site
    // only while there is a job); this stays a bool like every other gate.
    bool engine_in_reach = false;
    bool gun_in_reach = false;
    // ★★★ IS THE MAN ON HIS FEET -- ASKED OF THE WALKER, ANSWERED BY THE
    // CALLER. `PlayerMode::Afoot` means "the keys go to the man", NOT "the man
    // is standing" (the banner), and a fall sets it while `sim::WalkerMode` is
    // still Falling / Buried / Down / CrawlProne. Without this bit F admitted a
    // repair from a man face-down in the snow: drop the machine inside 6 m of
    // your own damaged pump, press U while Buried, and the pump came back with
    // nobody ever upright -- which contradicts Chad's R5 ("drives up, GETS
    // OFF, then makes the fix") and app/interact.h's own banner. The caller
    // fills it from `walker.mode == sim::WalkerMode::Afoot`, BY ENUMERATOR
    // NAME, so WalkerMode stays the sole authority and this header stays
    // glm-free. (Red-team finding, 2026-09-01.)
    bool man_upright = false;
    // The pump he is working is whole again. Only meaningful while Repairing;
    // it is what tells a finished job from an abandoned one.
    bool repair_complete = false;
    // ★ STING: the launcher is shouldered, aimed, and a launch remains. The
    // caller resolves all three (the shoulder UI, sting_left > 0) — the table
    // stays bool-only, same as every other gate here.
    bool drone_ready = false;
    // ★ STING FROM THE SEAT: the machine is on its skis (caller: !sled.rolled).
    // A man on a rolled machine has no seat to shoulder from.
    bool sled_upright = false;
};

// ★★★ THE TRANSITION. Pure: it reads `ctx`, writes `st`, returns the glue.
inline ModeAction player_mode_transition(PlayerModeState& st, ModeEvent ev,
                                         const ModeContext& ctx) {
    switch (ev) {
        case ModeEvent::MountKey:
            switch (st.mode) {
                case PlayerMode::Pilot:
                    if (!ctx.aircraft_ready)
                        return ModeAction::RefuseMountMoving;
                    st.mode = PlayerMode::Sled;
                    st.sled_seeded = true;
                    return ModeAction::SeedAndMount;
                case PlayerMode::Sled:
                    if (!ctx.sled_stopped)
                        return ModeAction::RefuseDismountMoving;
                    st.mode = PlayerMode::Afoot;
                    return ModeAction::Dismount;
                case PlayerMode::Afoot:
                    // ★ THE MACHINE FIRST, then the aeroplane. They are seeded
                    // 5 m apart and the two reaches do not overlap, so the
                    // order is only a tie-break -- but an unordered pair is a
                    // coin flip the day somebody shrinks that gap.
                    if (ctx.sled_seeded && ctx.sled_in_reach) {
                        st.mode = PlayerMode::Sled;
                        return ModeAction::Mount;
                    }
                    if (ctx.aircraft_in_reach) {
                        st.mode = PlayerMode::Pilot;
                        return ModeAction::BoardAircraft;
                    }
                    return ModeAction::None;
                case PlayerMode::Repairing:
                case PlayerMode::OnGun:
                case PlayerMode::Drone:
                    // Finish what you are doing first. Each leaves by its own
                    // key, so there is exactly one way out of each and no
                    // transition can strand a half-cleared flag. (A flying
                    // sting is a job in progress exactly like the wrench and
                    // the gun — J does not teleport a pilot off his drone.)
                    return ModeAction::None;
            }
            return ModeAction::None;
        case ModeEvent::InteractKey:
            // ★ IN REACH *AND* ON HIS FEET. See ModeContext::man_upright.
            //
            // ★★★ L10 -- THE PUMP WINS THE TIE, AND THE ORDER IS NAMED RATHER
            // THAN LEFT TO THE READING ORDER OF AN `if`. The two sites can
            // genuinely overlap: land the aeroplane at your own pump with a
            // dead stick and the man is standing inside both reaches. The pump
            // is the one with a CLOCK running against it (a pumpless faction
            // is losing the match while he works), and the aeroplane is not
            // going anywhere, so the pump is the job. Walk two steps off the
            // pump and the engine offer is the one left.
            if (st.mode == PlayerMode::Afoot && ctx.man_upright) {
                if (ctx.pump_in_reach) {
                    st.mode = PlayerMode::Repairing;
                    st.repairing = true;
                    st.repair_target = RepairTarget::Pump;
                    st.repair_pump = ctx.pump_index;
                    return ModeAction::BeginRepair;
                }
                if (ctx.engine_in_reach) {
                    st.mode = PlayerMode::Repairing;
                    st.repairing = true;
                    st.repair_target = RepairTarget::Engine;
                    // ⚠ NOT A PUMP. -1 is the only honest value here, and the
                    // sim tick's pump block is gated on the TARGET, never on
                    // this index being valid.
                    st.repair_pump = -1;
                    return ModeAction::BeginRepair;
                }
            }
            if (st.mode == PlayerMode::Repairing) {
                st.mode = PlayerMode::Afoot;
                st.repairing = false;
                st.repair_target = RepairTarget::None;
                st.repair_pump = -1;
                return ModeAction::EndRepair;
            }
            return ModeAction::None;
        case ModeEvent::GunKey:
            // ★ L5 -- IN REACH *AND* ON HIS FEET, the same law the pump key
            // carries and for the same reason. `PlayerMode::Afoot` only means
            // the keys are the man's; a fall sets it while `sim::WalkerMode`
            // is still Falling / Buried / Down / CrawlProne, and without this
            // bit O mounted the gun from a man face-down in the snow inside
            // the 3 m approach mark. Chad's R5 is "walks up to the flak gun
            // and engages": walking is a posture, and the walker owns it.
            if (st.mode == PlayerMode::Afoot && ctx.gun_in_reach &&
                ctx.man_upright) {
                st.mode = PlayerMode::OnGun;
                return ModeAction::ManGun;
            }
            if (st.mode == PlayerMode::OnGun) {
                st.mode = PlayerMode::Afoot;
                return ModeAction::LeaveGun;
            }
            return ModeAction::None;
        case ModeEvent::DroneKey:
            // ★ STING — the same law the pump and gun keys carry: the keys
            // must be the MAN'S and the man must be ON HIS FEET (the walker
            // answers, by enumerator name, through the caller). drone_ready
            // carries the rest (shouldered + a launch remaining).
            // ★ ... OR ON THE SEAT (Chad 2026-09-04: "press P on the
            // snowmachine ... deployment from the seat to the Sudburian's
            // hands"). No J first: the machine is the stance, and the end of
            // the flight hands him back the bars.
            if (ctx.drone_ready &&
                ((st.mode == PlayerMode::Afoot && ctx.man_upright) ||
                 (st.mode == PlayerMode::Sled && ctx.sled_upright))) {
                st.drone_home = st.mode;
                st.mode = PlayerMode::Drone;
                return ModeAction::DeployDrone;
            }
            if (st.mode == PlayerMode::Drone) {
                st.mode = st.drone_home;
                return ModeAction::EndDrone;
            }
            return ModeAction::None;
    }
    return ModeAction::None;
}

// ★ LEAVING THE PUMP ENDS THE JOB, and it ends it through the SAME two writes
// the F key uses rather than a second place that clears the flags. Called every
// tick; returns the action so the caller cannot miss the edge.
inline ModeAction player_mode_update(PlayerModeState& st,
                                     const ModeContext& ctx) {
    // ★★★ L10: EACH JOB IS ENDED BY ITS OWN SITE LEAVING REACH, never by "a
    // repairable thing left reach". Asking `!pump_in_reach` while he is at the
    // aeroplane would end the engine job the instant he started it, and the
    // reverse would let a man walk off a pump and keep fixing it from the
    // cockpit doorway. A target with no site is a job that cannot be held --
    // `RepairTarget::None` inside `Repairing` is unreachable through the table
    // and is treated as out of reach so it can never latch.
    const bool at_the_job = st.repair_target == RepairTarget::Pump
                                ? ctx.pump_in_reach
                                : (st.repair_target == RepairTarget::Engine &&
                                   ctx.engine_in_reach);
    if (st.mode == PlayerMode::Repairing && !at_the_job) {
        st.mode = PlayerMode::Afoot;
        st.repairing = false;
        st.repair_target = RepairTarget::None;
        st.repair_pump = -1;
        // ★ THE SAME TWO WRITES, TWO DIFFERENT SENTENCES. A pump leaves the
        // site table for exactly two reasons -- he walked away from it, or it
        // is whole -- and only the caller can tell them apart, so it hands the
        // answer in and the table names it.
        return ctx.repair_complete ? ModeAction::FinishRepair
                                   : ModeAction::EndRepair;
    }
    return ModeAction::None;
}

// ★★★ THE MODE WRITES THAT ARE NOT TRANSITIONS -- AND WHY THEY STILL GO
// THROUGH HERE.
//
// Two places in `app/main.cpp` move the player between modes without any key
// the table knows about: the snowmachine SPAWN (he is born on the bars) and
// the KEY_R autoright (the interim remount). Both wrote `player.mode = ...`
// directly, and a direct write is not the same operation as a transition: the
// transitions above never change the mode without also settling the L2 repair
// hook, because every path OUT of `Repairing` is one of them. A raw assignment
// has no such guarantee, so `Repairing` + spawn (you died mid-fix and respawned
// on a machine 300 m away) left `repairing == true` and `repair_pump` pointing
// at a pump nobody is standing at -- and `app/main.cpp`'s repair clock is gated
// on `mode == Repairing`, so the STRANDED FIELDS outlive the job silently
// rather than loudly. (Red-team finding, 2026-09-01.)
//
// The fix is a named write, not a discipline: `player_mode_force` is the only
// way to set the mode from outside the table, and it clears the hook on the way
// past. WRONG IMPLEMENTATION: `st.mode = m;` alone -- the leg
// "player mode: a forced mode never strands the repair hook" is red under it.
inline void player_mode_force(PlayerModeState& st, PlayerMode m) {
    st.mode = m;
    // The same two writes `EndRepair` makes, expressed as the INVARIANT rather
    // than as a copy of them: `repairing` is the mirror of `mode ==
    // Repairing`, and nothing may leave here with the two disagreeing. A
    // forced mode is a job that ended for a reason the table cannot see (a
    // death, a respawn, a righting), and an ended job clears its hook here,
    // once. (Forcing INTO `Repairing` is not something `main()` does -- the F
    // key is the only way in, because only the table knows which pump.)
    st.repairing = (m == PlayerMode::Repairing);
    // ★ L10: the TARGET is part of the same hook and is cleared with it. A
    // forced mode that left `repair_target == Engine` behind would make the
    // next `player_mode_update` ask the engine's reach about a man who is
    // 300 m away on a fresh machine -- the exact stranded-field shape this
    // function was written for.
    if (!st.repairing) {
        st.repair_pump = -1;
        st.repair_target = RepairTarget::None;
    }
}

// ★★★ WHEN THE AUTORIGHT KEY IS LEGAL (app/main.cpp's KEY_R).
//
// R is Chad's scaffolding -- "a key for now that lets me autoright until we get
// the guy running back to the snowmachine" -- and it TELEPORTS the man onto the
// machine at any distance. That was harmless while the only way to be off the
// machine was to have been thrown off it. L1 added a deliberate dismount and a
// 300 m spawn walk, and the unguarded key then made `[interact] sled_reach_m`
// and the whole diegetic mount decorative: step off, walk to the pump, press R,
// be back on the bars. (Red-team finding, 2026-09-01.)
//
// The law: an autoright is for a machine you are ON (righting it under you) or
// one that is actually DOWN (the crash the scaffolding exists for). Walking
// back to an upright machine you chose to leave is the walk-back Chad ruled in,
// and it is not a keypress. Repairing and OnGun are jobs in progress -- R does
// not yank a man out of one.
inline bool autoright_legal(const PlayerModeState& st, bool sled_rolled) {
    if (!st.sled_seeded) return false;
    if (st.mode == PlayerMode::Sled) return true;
    return st.mode == PlayerMode::Afoot && sled_rolled;
}

}  // namespace app
