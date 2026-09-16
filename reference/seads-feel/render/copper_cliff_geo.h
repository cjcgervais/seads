#pragma once
// CC6 — Copper Cliff slag-dump + track geometry, the SINGLE SOURCE for the dump
// anchor, the closed slag spur, and the pour-face aim. Both render/slag.cpp (the
// ridge frame) and render/train.cpp (the spur) read THESE — previously the dirs
// were DUPLICATED in both files, so a reposition was three edits and could fork.
//
// Placement (Chad, 2026-07-13): the dump sits in the OPEN area NORTH of the stack
// (south of the northern trunk road), the pour face aims NW, and the track leads
// FROM the smelter. Real reference: slag departed the EAST end of the Copper Cliff
// smelter on trolley pot-trains to a ~200 ha terraced black slag range; Superstack
// 46.4801,-81.0565, smelter ~46.4786,-81.0542. The anchor (46.494,-81.0455) was
// chosen by an OSM-openness scan (building/road-free = the slag flats).
//
// Regenerate: offline_tool/.venv/Scripts/python.exe offline_tool/cc_track.py — it
// runs lon/lat through the ONE locked aeqd projection (assets/projection.lock is
// UNTOUCHED; this only reads it). kSpur[2],[3],[4] are collinear on the NE crest
// axis so the ridge crest runs along the track and the pour face aims NW.
#include <glm/vec3.hpp>

namespace render {

// The CLOSED spur (smelter yard -> dump edge -> return), unit sphere dirs.
inline constexpr glm::dvec3 kCcSpur[6] = {
    {0.264876572751874, -0.563088983346666, 0.782797035023025},  // [0] smelter yard (46.474,-81.054)
    {0.277638543372109, -0.507386967442816, 0.815766697348817},  // [1] approach1 (46.483,-81.052)
    {0.294705403968304, -0.452510414878069, 0.841654946696540},  // [2] approach2 (46.4915,-81.049)
    {0.311916754816889, -0.435394994626706, 0.844475657860255},  // [3] DUMP edge / kMoundDir (46.494,-81.0455)
    {0.329086904107322, -0.418149213028582, 0.846671745831072},  // [4] return (46.4965,-81.042)
    {0.261346900268151, -0.463865126392232, 0.846479144596811},  // [5] return2 (46.49,-81.056)
};

// The dump anchor (= kCcSpur[3]) and the crest-axis straddle points ([2] approach,
// [4] return) — the ridge frame's axis = normalize([4]-[2]) projected onto the
// tangent plane, so the crest runs ALONG the track.
inline constexpr glm::dvec3 kCcDumpDir = kCcSpur[3];
inline constexpr glm::dvec3 kCcAxisApproach = kCcSpur[2];
inline constexpr glm::dvec3 kCcAxisReturn = kCcSpur[4];

// A point NW of the dump: the pour-face sign is chosen so the face aims toward it
// (NW, toward the highway/open view — Chad's "viewable from the highway NW").
inline constexpr glm::dvec3 kCcFaceAim{0.296872651503308, -0.409938436432000,
                                       0.862448321422845};

// The Superstack hero location (Copper Cliff smelter) — reference only.
inline constexpr glm::dvec3 kCcSuperstack{0.26561950711033627, -0.55885612114843386,
                                          0.7855737478412762};

}  // namespace render
