#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Baked S4 building massing (the Sudbury town grids), moved OUT of the giant
// generated constexpr header (render/sudbury_gis.gen.h) into a runtime binary
// asset — assets/sudbury_buildings.bin — so the whole-map fill can scale without
// a ~150 MB compiled header (infeasible on MinGW GCC). This module is PURE (no
// raylib) so it lives in seads_render_core and is shared by BOTH the app loader
// (render/buildings.cpp) AND the unit-test validator (test_sudbury_gis.cpp) — one
// parser, no fork. Format: little-endian, sectioned + versioned so the Stage-2
// tile bounds and the Stage-3 collision index can append without a breaking rev.
//
// FILE LAYOUT (all little-endian; this box is LE-only, the 'SBLD' magic is the
// endianness marker):
//   header (24 B): magic[4]='SBLD', u32 version, u64 projection_lock_hash,
//                  u32 hero_batch_count, u32 section_count
//   section table (24 B each): tag[4], u32 struct_size, u64 offset, u64 count
//   section blobs (packed records at the given offsets):
//     "VERT" ss=36: 3×f64 dir + f32 h + f32 bj + f32 ws     (PACKED, no C++ pad)
//     "IDX " ss=2 : u16 (LOCAL 0-based per batch)
//     "BAT " ss>=16: 4×i32 vtx_off,vtx_count,idx_off,idx_count (Stage-2 may append)

namespace render {

// Mirror of the old gen.h structs (now defined ONLY here). The on-disk records
// are PACKED (36 B vert); these in-memory structs may carry compiler padding —
// the parser reads field-by-field, never a raw struct cast.
struct GisBuildingVertex {
    double dir[3];   // unit surface direction (planet-local)
    float h;         // metres above the terrain (0 base .. eave/ridge)
    float bj;        // per-building tone jitter (~1.0)
    float ws;        // wall-perimeter arc-length (night window-grid X; 0 = none)
};

struct GisBuildingBatch {
    int vtx_off;     // first vertex in verts[]
    int vtx_count;
    int idx_off;     // first index in indices[] (LOCAL 0-based within vtx range)
    int idx_count;   // == triangles * 3
    // Stage-2 spatial tile bound (a cone over the batch's building dirs) for the
    // horizon/distance cull — batches are binned by aeqd cell so this is tight. An
    // old ss=16 .bin (no bound) parses to center_dir=+Z, half_angle=pi (never culled).
    double center_dir[3] = {0.0, 0.0, 1.0};  // unit dir to the tile center
    double half_angle = 3.14159265358979324;  // rad, center -> farthest building dir
};

// Stage-3 SIM-NEUTRAL collision index (one per kept town footprint): the future
// solid-ground / targeting features (MASTER_PLAN Phase 1) need building extents
// WITHOUT parsing render triangles. Radius+height only (Fable: no footprint polys).
// Its own section — never read by any render vertex path; NO runtime consumer yet.
struct GisBuildingCollider {
    double center_dir[3];  // unit dir to the footprint representative point (inside)
    float radius_m;        // EQUIVALENT-AREA radius sqrt(area/pi) — NOT a covering
                           // radius (Fable P2): it UNDER-covers a long/thin footprint
                           // (a 60x8 m warehouse -> r~12 m vs 30 m half-length). A
                           // future collision consumer needing CONTAINMENT must inflate
                           // it or recompute a max-extent cover at bake — this is an
                           // extent PROXY, not a guaranteed bound.
    float height_m;        // eave/ridge height ABOVE TERRAIN (m); the record stores no
                           // base elevation, so a consumer re-samples terrain at center_dir.
};

struct BuildingAsset {
    std::vector<GisBuildingVertex> verts;
    std::vector<unsigned short> indices;
    std::vector<GisBuildingBatch> batches;
    std::vector<GisBuildingCollider> colliders;  // Stage-3 (empty if the .bin has no COLL)
    unsigned long long lock_hash = 0;      // must equal kSudburyProjectionLockHash
    std::uint32_t version = 0;
    std::uint32_t hero_batch_count = 0;    // the LAST N batches are tall S5 heroes
    bool ok = false;                       // false = malformed/empty (caller falls back)
};

// The on-disk format version this build writes/expects.
inline constexpr std::uint32_t kBuildingAssetVersion = 1;

// Parse the sectioned little-endian .bin from a raw byte buffer. FULLY bounds-
// checked; returns ok=false on any malformation (bad magic/version, out-of-range
// section, wrong struct_size). Does NOT read files — the caller supplies the
// bytes (raylib LoadFileData in the app; std::ifstream in the test).
BuildingAsset parse_building_asset(const unsigned char* data, std::size_t size);

}  // namespace render
