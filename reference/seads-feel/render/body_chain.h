#pragma once
// ★ R4a SUPERMAN -- THE BODY CHAIN'S MEASURED TABLE.
//
// docs/SUDBURIAN_LADDER.md §7.6 is the contract: "Superman is that same solver
// with the anchor moved to the grips and the body as the chain. Build it once
// in R2 for the scarf, re-use it in R4 for the body." render/trail_chain.* IS
// that solver and knows no asset; THIS file is the asset, and nothing else.
//
// Everything here is MEASURED by
// `assets/character/sudburian_src/measure_body_chain.py`, which reads
// assets/sled/indy650.glb's bytes with stdlib python and PRINTS the block
// `body_chain_stations()` returns. Re-run it after any change to the rider and
// paste; never retype a number (CLAUDE.md, NO GUESSING).
//
// ★ THE TWO THINGS THE MEASUREMENT EXISTS TO SETTLE
//
// 1. THE SEGMENT TABLE IS BOUNDED BY THE SEAT PAN. trail_chain's seat keep-out
//    is a VERTEX keep-out: a link longer than the pan is thick lies straight
//    through it with a vertex either side and nothing inside to find. The pan
//    measures 0.218000 m; the longest link below is 0.209254 m. A
//    bone-per-link table would break that bound in four places (upperarm
//    0.286, forearm 0.267, thigh 0.418, shin 0.454 m), so the table SUBDIVIDES
//    -- at the rig's own twist bones where it has them, in thirds down the
//    shin where it does not.
//
// 2. ★★★ THE LEGS ARE GRADED ON THEIR DRAWN SURFACE, NOT ON THE CENTRELINE.
//    The chain is a bone. The drawn legs are not merely thicker than it, they
//    are not ON it: they straddle the machine at |x| ~ 0.27-0.30 m while the
//    seat is 0.206 m half-wide. So each station carries TWO PROBES, one per
//    limb -- the limb's own oriented BOUNDING BOX in the chain's frame, taken
//    as the MAX over that limb's skinned vertices. See render::TrailChainProbe
//    for why a box, and why maxima and not percentiles.
//
// ⚠ THE BIND POSE IS NOT LEGAL AGAINST THIS KEEP-OUT, AND THAT IS NOT A
// DEFECT: he is SITTING in it. Measured penetration of the drawn vertices into
// the seat solid at bind -- pelvis 52.5 mm, thigh 38.6 mm, shin_1 30.4 mm,
// shin_2 21.0 mm, knee 13.9 mm. The radii are deliberately NOT capped to make
// bind legal (that would under-protect superman, the only state this chain is
// ever live in). What it costs is a PRIMING RULE, the same one trail_chain.cpp
// already documents for the back plane: PRIME THE BODY CHAIN WHEN IT ARMS, IN
// THE POSE IT ARMS IN -- never seated at bind, or the least-penetration exit
// takes his pelvis out through the nearest face.

#include <array>
#include <glm/vec3.hpp>

#include "render/trail_chain.h"

namespace render {

// 17 stations, 16 links, grips -> toe. NOT a round number and not a choice:
// it is what the rig's joints and the 0.218 m bound together produce.
inline constexpr int kBodyChainStations = 17;
inline constexpr int kBodyChainSegments = kBodyChainStations - 1;

static_assert(kBodyChainSegments <= kTrailChainMaxSegments,
              "the measured body chain must fit the solver's fixed arrays");

// The limb pair hung off one station: [0] is the +X side, [1] the -X side, in
// the station frame `chain_axes` (render/trail_chain.cpp) builds, and which
// measure_body_chain.py reproduces. It is the limb's own oriented BOUNDING
// BOX -- centre (u, v, w) and half-extents (hx, hy, hz) along the frame's
// +X / +Y (down the chain) / +Z. All-zero extents mean "this station draws
// nothing here".
//
// A BOX and not a ball, because one radius takes the widest half-extent in
// every direction at once: the shoulder's 0.246 m lateral half-span would
// otherwise hold a man's chest that far above a seat his back is a third as
// thick. See render::TrailChainProbe.
struct BodyChainProbe {
    float u_m;
    float v_m;
    float w_m;
    float hx_m;
    float hy_m;
    float hz_m;
};

struct BodyChainStation {
    glm::vec3 rest_model;  // the joint's BIND world position, model frame
    std::array<BodyChainProbe, 2> probe;
};

// The measured table. Pasted from measure_body_chain.py; see the file header.
const std::array<BodyChainStation, kBodyChainStations>& body_chain_stations();

// Link i (1-based, station i-1 -> station i) of the measured body, metres.
float body_chain_link_len(int i);

// The whole body, grips to toe, along the chain. 2.200920 m as measured.
float body_chain_total_len();

// ★★★ THE PER-SIDE JOINT OFFSETS -- R4a rung 2 (the drawn legs).
//
// The probes above are the drawn SURFACE. A probe centre is the centre of a
// FLESH LOBE, not a bone: `measure_body_chain.py` builds them from CPU-skinned
// vertices bucketed to the nearest station. So a probe centre is the WRONG
// quantity for an IK target, and using one displaces the limb by
// (centroid - joint) -- measured at 11.6 mm at the thigh and 9.2 mm at the
// knee. The plan that did that was caught in review before it was built.
//
// These are the RIGHT quantity: each side's JOINT, offset from the midline
// station, in the SAME station frame the probes use. The chain is the body's
// MIDLINE (the station table itself is the midpoint of each L/R joint pair),
// so a side is reconstructed by carrying its offset through the LIVE frame --
// exactly the way a probe box is evaluated, which is the whole point: the leg
// the game DRAWS and the leg the keep-out GRADES are then reconstructed from
// one frame by one rule.
//
// ⚠ ONLY the leg stations, and only those with a real L/R joint pair.
// `shin_1`/`shin_2` are INTERPOLATED stations with no joints, and the rig has
// no bones there -- which is why the drawn shin cannot be pinned to them, and
// why the residual fork off those two stations is MEASURED by the gate
// instead of being silently assumed away.
//
// Index [0] is the +X side, [1] the -X side, ORDERED BY MEASURED SIGN and
// never by an "_l"/"_r" suffix (which side a suffix means is a rig
// convention, and this program does not guess).
inline constexpr int kBodyChainLegTargets = 4;

struct BodyChainLegJoint {
    int station;                       // index into body_chain_stations()
    std::array<glm::vec3, 2> off_m;    // per side, in the station frame
};

// The measured table: thigh, knee, ankle, toe. Pasted from
// measure_body_chain.py, same as everything else in this file.
const std::array<BodyChainLegJoint, kBodyChainLegTargets>&
body_chain_leg_joints();

// The station index of the named leg joint, or -1. Named lookups so a caller
// never hard-codes 12 or 15 -- the station table has already moved once.
int body_chain_station_of(const char* name);

// Fill a TrailChainParams with the measured body: segment count, the
// non-uniform length table, and the drawn-surface probes. Everything ELSE on
// `pr` -- drag, damping, dt, iterations, the keep-out dials -- is the caller's
// (they are feel, and they are not in the asset).
void body_chain_fill(TrailChainParams& pr);

}  // namespace render
