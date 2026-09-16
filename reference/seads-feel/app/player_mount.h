#pragma once
// ★★★ L1 — THE MOUNT SEAM (docs/PLAN_20260901_game_loop_millwright.md §3).
//
// ★★★ THE GRIP AND THE MAN ARE ONE FACT AND MUST BE WRITTEN TOGETHER.
//
// This file exists because they once were not. `app/main.cpp`'s KEY_R autoright
// re-attached `sled.grip` and left the man wherever he had fallen: the camera
// stayed anchored at the old crash site while the machine drove away, W/S/A/D
// drove BOTH bodies, and -- the silent half -- `walker_throw` returns early on
// a man who is already off, so EVERY LATER FALL DID NOTHING, FOREVER. It was
// found by a red team on 2026-09-01 and fixed by adding one line next to the
// other one. Two adjacent lines in one function is not a fix; it is the same
// bug waiting for a third call site.
//
// So there is now exactly ONE place where a rider is put back on a machine and
// exactly ONE place where he is taken off, and every path -- KEY_R's autoright,
// the KEY_J mount seed, the walk-up mount -- goes through them. R4e (the r4a
// lane) replaces the BODY of these functions when the real remount lands; it
// does not have to find the call sites, because there is only this.
//
// ⚠ THESE FUNCTIONS DO NOT GATE ON DISTANCE. Reach is `app/interact.h`'s law
// and the mode table's decision; a seam that also decided WHEN would give the
// autoright (which must always work, at any distance, on a machine the man is
// nowhere near) a reason to bypass the seam -- which is how the second call
// site gets born again.

#include <glm/glm.hpp>

#include "app/walker_place.h"
#include "sim/rider_grip.h"
#include "sim/sled.h"
#include "sim/walker.h"

namespace app {

// Put the rider back on the machine. Idempotent: calling it on a mounted rider
// is a no-op in every observable, which is what lets the KEY_J seed (which has
// already reset the whole `SledState`) route through it without a guard.
//
// ⚠ `sim::GripState{}` -- the WHOLE struct, not `attached = true`. `grip` is
// not in the sled tape's pin roster, so a replay rebuilds it from struct
// defaults; an override that set only the latch would leave `load_lp` carrying
// the load that broke the grip and record and replay would fork on the next
// substep (the same argument `ws_exch_l` already carries at the autoright).
inline void player_mount_request(sim::SledState& sled,
                                 sim::WalkerState& walker) {
    sled.grip = sim::GripState{};
    sim::walker_remount(walker);
}

// Take the rider off the machine, standing, at `pos_w` facing `heading_w`.
// The mirror image of the mount, and for the same reason: the latch and the
// man's mode are one fact.
//
// ⚠ THE ORDER MATTERS AND IT IS NOT ARBITRARY. `app/main.cpp` reads
// `walker.mode == Riding && !sled.grip.attached` inside the tick loop as "the
// grip broke THIS tick" and answers it with `walker_throw` + a helmet dent. A
// dismount that detached the grip and left the mode at `Riding` for even one
// tick would be read as a CRASH. Written together, that window does not exist.
inline void player_dismount_request(sim::SledState& sled,
                                    sim::WalkerState& walker,
                                    const glm::dvec3& pos_w,
                                    const glm::dvec3& heading_w) {
    sled.grip.attached = false;
    walker_place_afoot(walker, pos_w, heading_w);
}

// ★★★ MAY THIS MACHINE STATE BE WRITTEN INTO A SLED TAPE AS AN OVERRIDE?
//
// THE TAPE PINS 41 DOUBLES PLUS `surface` AND `rolled` (test/harness/
// sled_tape.h, SLEDTAPE_PIN_D) AND NOTHING ELSE. Replay applies an override by
// WHOLE-STRUCT ASSIGNMENT from a pin that was read into a DEFAULT-constructed
// `sim::SledState`, so every field outside the roster is silently reset to its
// struct default at every override. That is safe for the fields that
// re-converge from the taped inputs -- `ws_exch_l` is the worked example at
// app/main.cpp's KEY_R -- and it is NOT safe for a latch.
//
// `sim::GripState::attached` defaults to TRUE and is a ONE-WAY latch: nothing
// in the kernel can turn it back on, so it structurally cannot re-converge.
// `sim/sled.cpp` reads it as `hands_on`, which gates `rider_frac` -> `cg_off`
// -> the patch geometry. An override recorded with the rider OFF therefore
// replays as a machine WITH a rider from that tick onward. Measured on this
// tree: 240 ticks at dt = 1/120 from one pinned leaning pose, zero inputs,
// gives |dpos| = 4.7e-3 m between the two arms.
//
// So: an override may only pin a machine whose UNPINNED, NON-CONVERGING state
// is already at the default a replay will rebuild. Today that is exactly one
// bit. A caller holding a state this refuses must END THE TAPE EPISODE rather
// than record a machine it cannot replay. (Red-team finding, 2026-09-01.)
inline bool sled_override_replayable(const sim::SledState& s) {
    return s.grip.attached == sim::GripState{}.attached;
}

// Where he steps off: `side_m` to the LEFT of the machine's centreline, on the
// ground, facing the way the machine faces.
//
//   sled_pos_w   the machine's CG, world
//   sled_right_w the machine's own +X (right) in world, unit (sim/sled.h's
//                body frame: +X right, -Z forward)
//   drive_r      the drive-surface radius under the dismount point, which the
//                CALLER samples -- this header owns no snowpack.
//   stand_h_m    how far his own origin sits above that surface
//                (`WalkerParams::lie_clearance_m`).
//
// ★ TWO STEPS, NOT ONE: the side offset is taken first and the result is then
// re-grounded, because a tangential step on a 15 km ball leaves the sphere
// (chord versus arc) and a man placed on the chord starts underground.
inline glm::dvec3 dismount_dir(const glm::dvec3& sled_pos_w,
                               const glm::dvec3& sled_right_w, double side_m) {
    const glm::dvec3 p = sled_pos_w - sled_right_w * side_m;  // LEFT
    const double r = glm::length(p);
    return r > 0.0 ? p / r : glm::dvec3{0.0, 1.0, 0.0};
}

inline glm::dvec3 dismount_pos(const glm::dvec3& dir_w, double drive_r,
                               double stand_h_m) {
    return dir_w * (drive_r + stand_h_m);
}

}  // namespace app
