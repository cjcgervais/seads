#pragma once
// ★★★ L5 — THE WALK-UP (docs/PLAN_20260901_game_loop_millwright.md §4.5,
// docs/FLAK_GUN_SPEC.md §7 F-WALK).
//
// WHY THERE IS A FILE FOR THREE LINES OF ARITHMETIC.
//
// `app/main.cpp` needs the gun's approach mark in TWO places that must agree
// exactly: the interact site the O key is gated on (rebuilt every frame, from
// the placed gun) and the spot the man is put down on when he lets go of it
// (from the MANNED gun, whose train he has just been moving). Written twice
// they would drift the first time anybody touched the transform, and the
// symptom would be a mark you can stand on to get on but not to get off --
// exactly the class of defect this ladder has paid for twice (the KEY_R
// co-attach, the two-adjacent-lines "fix" in app/player_mount.h).
//
// And they cannot be tested where they were: `main()` needs a window. Here
// they are pure, glm-and-flak_gun-only, and `seads_tests` compiles them.
//
// ⚠ `st_approach` IS A TRAIN STATION AND THAT IS THE WHOLE POINT. It is baked
// into the shipped GLB (pinned at y == 0, behind the shoulder pads, by
// test_flak_gun.cpp) and it rides the platform's train angle -- so the mark is
// behind the gunner at EVERY bearing, and "walk up to the gun" means the same
// thing whichever way the barrel is pointing. Reading it at zero train would
// put the mark in front of a gun trained 180 degrees round.
//
// ⚠ NOTHING HERE SAMPLES THE GROUND. The drive radius is handed in, because
// the only place that may query it is `app/main.cpp`'s FRAME block: the sled
// tape's `sample_tap` is armed around the TICK loop and an extra ground record
// inside that window makes every tape of the drive unreplayable (the R4c
// finding). A header that queried for you would take that decision away from
// the one file that can make it.

#include <glm/glm.hpp>

#include "render/flak_gun.h"

namespace app {

// The approach mark, world, under the gun's CURRENT train. `mount` is the
// gun's mount frame; `stations` the table read off the GLB (never retyped).
inline glm::dvec3 flak_approach_world(const render::flak::MountFrame& mount,
                                      const render::flak::Stations& stations,
                                      double train_rad) {
    const render::flak::Pose p{train_rad, 0.0};
    return render::flak::train_station_world(mount, p, stations.approach);
}

// Where the MAN stands on that mark: the same direction from the planet
// centre, re-grounded onto the drive surface at his own standing height.
//
// ★ TWO STEPS, NOT ONE, and for the reason app/player_mount.h's dismount
// states: the station's model y is 0 -- the terrain under the pedestal, not
// the snow surface -- and the walker lives a lie-clearance above the drive
// radius. Comparing the two raw spends better than a metre of a three-metre
// reach on a vertical offset, which is a reach gate that fails standing on the
// mark.
inline glm::dvec3 flak_stand_pos(const glm::dvec3& approach_w, double drive_r,
                                 double stand_h_m) {
    const double r = glm::length(approach_w);
    if (!(r > 0.0)) return approach_w;
    return (approach_w / r) * (drive_r + stand_h_m);
}

// Which way he faces when he lets go: AWAY from the gun. Returned un-normalised
// and un-tangentialised -- `app::walker_place_afoot` tangentialises at his feet,
// which is the one place that knows where his feet are.
inline glm::dvec3 flak_face_away(const glm::dvec3& stand_w,
                                 const glm::dvec3& mount_pos_w) {
    return stand_w - mount_pos_w;
}

}  // namespace app
