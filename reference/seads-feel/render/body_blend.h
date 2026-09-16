#pragma once
// ★★★ R4a RUNG 2 -- BLENDING THE DRAWN LEGS ONTO THE CHAIN.
//
// LADDER 7.3 stage 2: "foot load -> 0; boots leave the running boards; leg IK
// releases from board_socket." Stage 3: "legs trail from the hips as a damped
// chain." This file is the arithmetic of that release and nothing else.
//
// ★ IT IS PURE, AND THAT IS NOT TIDINESS. `render/sled_model.cpp` is compiled
// ONLY into the `seads` executable (CMakeLists.txt) -- never into
// seads_render_core, which is what the test binary links. A gate written
// against a mechanism living in that TU cannot execute a single line of it.
// The first draft of this rung put five of its six gate legs there; the review
// that caught it is the same lesson the .blend re-export rung already paid for
// (a gate that never compiled the thing it graded). So the mechanism lives
// here, where ctest can reach it, and sled_model.cpp keeps only gather /
// call / solve / settle with no logic in it.
//
// ★★★ THE SCOPE OF THIS RUNG, RULED BY CHAD 2026-08-28: LEGS ONLY. The
// pelvis, spine, shoulder and arms stay at the signed R3 pose. Stations 6..10
// are exactly the bones the attributed-and-parked COAT SINK lives on, and
// re-posing them by a new rule in the same rung would put a known defect and a
// new mechanism in front of him at once.

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "render/body_chain.h"
#include "render/trail_chain.h"

namespace render {

// One side of the body. [0] is the +X side, [1] the -X side -- the same
// convention BodyChainProbe and BodyChainLegJoint use, and it is a MEASURED
// sign, never an "_l"/"_r" suffix.
struct BodyBlendLeg {
    glm::vec3 knee{0.0f};
    glm::vec3 ankle{0.0f};
    // The weight actually applied after the reach limit below. Equal to the
    // requested weight unless the chain trailed further than the leg is long.
    float w_eff = 0.0f;
    bool reach_limited = false;
    // The seat solid pushed this target out (0 if it was already clear).
    float seat_push_m = 0.0f;
};

struct BodyBlendOut {
    BodyBlendLeg leg[2];
};

struct BodyBlendIn {
    // ---- the CHAIN side -------------------------------------------------
    // Live station positions and the live station frames, both indexed by
    // station. `frame[i]` must be what trail_chain_frames wrote for particle
    // i: a rotation whose columns are the station's (x, y, z) axes. The joint
    // offsets are carried through THIS frame, which is what makes the drawn
    // leg and the graded probe box reconstruct from one rule.
    const glm::vec3* station_p = nullptr;
    const glm::mat4* frame = nullptr;

    // ---- the R3 (drawn, signed) side ------------------------------------
    // Where pose_pass has already put these joints this frame.
    glm::vec3 r3_knee[2]{glm::vec3(0.0f), glm::vec3(0.0f)};
    glm::vec3 r3_ankle[2]{glm::vec3(0.0f), glm::vec3(0.0f)};
    // The leg IK root (the thigh bone's origin) -- solve_chain reads this from
    // the rig and CANNOT move it, so the blend never returns a hip target.
    glm::vec3 hip[2]{glm::vec3(0.0f), glm::vec3(0.0f)};
    // The two bone lengths of each leg, thigh then calf. The reach limit
    // mirrors solve_chain's own dmin/dmax EXACTLY -- see the .cpp.
    float len_thigh[2]{0.0f, 0.0f};
    float len_calf[2]{0.0f, 0.0f};

    // ---- the weight ------------------------------------------------------
    // LADDER 7.3 stage 2's own signal: how much of the share the RUNNING
    // BOARDS carried at the LIVE reference they have now lost. NOT the
    // combined stage weight -- Chad's ruling, 2026-08-28.
    float w = 0.0f;

    // ---- the seat solid, optional ---------------------------------------
    // When both are non-null the blended targets are pushed out of the SAME
    // seat solid the chain's own keep-out uses. Null in a test that wants the
    // raw blend.
    const TrailChainParams* seat_pr = nullptr;
    const TrailChainInput* seat_in = nullptr;
};

// The blend. Deterministic, allocation-free, and at `w == 0` it returns the R3
// inputs BIT-IDENTICALLY by an early return -- not by arithmetic that happens
// to land on zero.
BodyBlendOut body_blend_legs(const BodyBlendIn& in);

// The largest weight in [0, w] whose point on the segment a -> b stays inside
// the spherical shell [dmin, dmax] about `hip`. Exposed for the gate.
//
// ★ THE CLAMP IS ON THE WEIGHT, NOT ON THE POINT. A radial clamp of the point
// into the annulus pulls it TOWARD THE HIP -- i.e. toward the machine -- and
// it engages in the ordinary stage-3 state, because the chain can trail two
// metres while the leg reaches under one. Limiting the weight instead keeps
// every target ON its own R3 -> chain segment, which is what makes the blend
// monotone in w and keeps the fork measurable.
float body_blend_reach_limit(const glm::vec3& a, const glm::vec3& b,
                             const glm::vec3& hip, float dmin, float dmax,
                             float w);

// The bend normal that makes `solve_chain` put the mid-joint at `knee`.
//
// solve_chain places the elbow/knee at `s + dir*a + bend*h`, where
// `bend = normalize(cross(bend_n, dir))`. So aiming the knee is inverting that
// one cross product: take the component of (knee - hip) perpendicular to the
// hip->ankle direction, and hand back the normal whose cross product with
// `dir` points that way.
//
// Returns `fallback` unchanged when the geometry is degenerate (knee on the
// hip->ankle line, or a zero-length chord) -- a straight leg has no bend plane
// to name, and inventing one there is how a limb spins.
glm::vec3 body_blend_bend_normal(const glm::vec3& hip, const glm::vec3& ankle,
                                 const glm::vec3& knee,
                                 const glm::vec3& fallback);

}  // namespace render
