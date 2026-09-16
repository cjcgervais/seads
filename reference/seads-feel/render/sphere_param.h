#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "world/heightfield.h"  // world::HeightField + world::equirect_uv (re-exported below)

// Cubesphere parameterization + the persistent CPU height field (SPEC §6;
// docs/world_build_plan.md §2). PURE: glm + std only, ZERO raylib — so it lives
// in seads_render_core and the gate can pin it headlessly (render/planet.cpp is
// app-target-only). It is the SINGLE SOURCE for the sphere geometry: the mesh
// build (render/planet.cpp), the future prop scatter (world/, P3), and the
// future airstrip ground-contact query (§1 "Landing-ready") all consume THIS —
// never a re-derived copy (the H1 anti-fork; a second height source
// floats/sinks props).
//
// TWO parameterizations live here and must NEVER be merged:
//   - the MESH param is TANGENT-WARPED (warp(): tan(s·π/4)) so quad/texel
//   density
//     is near-uniform (cuts cube cell-area distortion ~5.1x -> ~1.4x).
//   - the GL CUBEMAP texel param (gl_cube_dir(), the §2 seam fix) is RAW
//   gnomonic
//     in GL's OWN face-axis convention — NOT face_basis() below. Reusing this
//     table for the cubemap bakes each face flipped/rotated.
//
// Precision: directions/heights are computed in DOUBLE (the float atan2/asin
// the old inline path used moved the 15 km mesh sub-mm; going double is the
// accepted tightening — see the commit note). Mesh vertices are emitted as
// float (render precision after the eye-relative rebase; the double->float seam
// is in draw.cpp).

namespace world {
// R1 (BLOCK-VP1): the DRAWN fold provider. Forward-declared so this widely
// included header does not pull in the whole snowpack; the .cpp has it.
struct SnowpackField;
}  // namespace world

namespace render {

// Cubesphere MESH face basis: (normal, right, up), right x up == normal
// (outward), so the grid winds CCW seen from outside. f in [0,6).
struct FaceBasis {
    glm::dvec3 n, r, u;
};
FaceBasis face_basis(int f);

// Nowell tangent warp on a face coord in [-1,1] and its inverse. Monotone, so
// coverage/folds hold. warp() shapes the MESH grid; unwarp() is for the future
// cubemap texel param.
double warp(double s);    // tan(s * pi/4)
double unwarp(double s);  // atan(s) * 4/pi

// Gnomonic face coords (s,t on the face plane) -> unit direction. s,t are the
// already-warped grid coords for the mesh (or raw, for the inverse round-trip).
glm::dvec3 face_dir(int f, double s, double t);

// Unit direction -> face + raw gnomonic face coords (the inverse of face_dir).
// Major-axis select with a deterministic tie-break (lowest face index wins an
// exact edge) so a future scatter never double-counts an edge cell.
struct FaceCoord {
    int face = 0;
    double s = 0.0, t = 0.0;
};
FaceCoord dir_to_face(glm::dvec3 d);

// The equirect sampler + the height field now live in the neutral world/ module
// (both sim and render consume ONE definition; MASTER_PLAN §3.C). Re-exported
// into render:: so existing render callers (planet.cpp, world/props.cpp, the
// bake, tests) are unchanged — a using-alias, NOT a fork.
using world::equirect_uv;

// GL CUBEMAP texel param: (face, sc, tc in [-1,1]) -> unit direction, in GL's
// OWN cube-map face-axis convention (the inverse of the OpenGL major-axis
// selection equations) — NOT face_basis() above (reusing that table bakes faces
// flipped/ rotated; the header's "two params, never merged" warning). face
// order matches GL_TEXTURE_CUBE_MAP_POSITIVE_X + face: 0=+X 1=-X 2=+Y 3=-Y 4=+Z
// 5=-Z, so a cubemap baked with this samples correctly under texture(cube,
// dir).
glm::dvec3 gl_cube_dir(int face, double sc, double tc);

// Resample an equirectangular RGB8 image into 6 GL cubemap face buffers (the §2
// seam fix: runtime sampling becomes texture(cube, fragDir), no (u,v) lat/lon
// anywhere). Returns 6*face_size*face_size*3 bytes, RGB8 row-major, faces
// contiguous in GL order (+X,-X,+Y,-Y,+Z,-Z) — exactly rlLoadTextureCubemap's
// layout. u_offset is baked in (matches equirect_uv), so the shader drops
// uOffset. Its own RGB bilinear (HeightField::sample01 is single-channel): u
// wraps, v clamps, -0.5 texel center. PURE (glm+std): the caller loads/uploads
// via raylib.
std::vector<std::uint8_t> bake_equirect_cubemap(const std::uint8_t* rgb, int w,
                                                int h, double u_offset,
                                                int face_size);

// As bake_equirect_cubemap, but bakes an RGBA cubemap: RGB from `rgb` (w*h*3)
// and A from `mask` (w*h*1, single channel — the lake landmask). Same sampling
// convention (u wraps, v clamps, -0.5 center) so the alpha coverage tracks the
// albedo exactly. The runtime water branch reads .a as the (mip-filtered =
// coverage-preserving) lake mask. Returns 6*face_size*face_size*4 bytes, RGBA8
// row-major, faces contiguous in GL order.
std::vector<std::uint8_t> bake_equirect_cubemap_rgba(const std::uint8_t* rgb,
                                                     const std::uint8_t* mask,
                                                     int w, int h,
                                                     double u_offset,
                                                     int face_size);

// HeightField moved to world/heightfield.h (the neutral module the sim ground
// query also consumes). Aliased here so render callers keep the render::
// spelling — ONE definition, no fork.
using HeightField = world::HeightField;

// T2 PORTAL SURGERY (docs/tunnel_staging.md rung T2): a great-circle "cut disk"
// on the sphere — every terrain triangle with ANY of its three vertex
// DIRECTIONS inside the disk is skipped at fill (so the tunnel mouth openings
// read as holes; the terrain is CCW-outward wound and from underground it
// backface-culls, so the hole is what lets the pilot fly in). `dir` is a unit
// world direction (the mouth normal); a vertex direction v is INSIDE when the
// great-circle angle acos(dot(v,dir)) * hf.R < radius_m — measured against
// hf.R, the SAME base radius the tunnel net builds its mouths from (the H1
// anti-fork: one radius, no second cut-metric). An EMPTY cut list is a
// bit-identical no-op (the fill drops no triangle).
struct CutDisk {
    glm::dvec3 dir;
    double radius_m = 0.0;
};

// Is the unit direction d inside ANY cut disk (great-circle arc against the
// base radius R — the exact fill_face drop metric)? ONE definition, shared by
// the terrain fill, the tree scatter, and the ribbon clip (T24), so every
// surface that yields to an excavation yields at the identical rim.
bool dir_in_any_cut(const glm::dvec3& d, double R,
                    const std::vector<CutDisk>& cuts);

// Pure mesh fill for one warped face at subdiv N (N verts per edge). No raylib:
// returns flat arrays the caller uploads (render/planet.cpp). positions/normals
// are 3*N*N floats; indices are 3*2*(N-1)*(N-1). No texcoords (the shader
// samples from the interpolated direction; per-vertex UVs were dead).
struct FaceMesh {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<unsigned short> indices;
    // ★ R3: per-vertex AMBIENT SNOW DEPTH in metres, one per vertex (same
    // count and order as positions/3). This is the channel WINTER_LAW 3.2/6c.1
    // demands the fragment shade from -- the ONE analytic depth field -- so the
    // planet FS can stop computing a private exposure mask that predates the
    // field and disagrees with it (render/planet.cpp's "PROVISIONAL SNOW STUB").
    //
    // ★ IT IS THE UNMASKED AMBIENT, NOT THE FOLD THE GEOMETRY USED, and the
    // divergence is deliberate -- see SnowpackField::DrawSample. Both come out
    // of one field call per vertex so they cannot fork in anything but the mask.
    //
    // EMPTY when no fold is bound (a test, the Earth, a procedural planet,
    // SEADS_NO_SNOWFOLD). Consumers must treat empty as "no depth channel" and
    // fall back, NOT as "zero snow everywhere" -- upload_face does.
    std::vector<float> depths;
};
FaceMesh fill_face(const HeightField& hf, int f, int N);

// R4d face TILING: the same face split into tiles×tiles sub-meshes, each N
// verts per edge, so the EFFECTIVE grid is tiles*(N-1)+1 verts per face edge —
// past the ushort per-mesh index cap that pinned subdiv at 256. Every vertex
// param is computed from its GLOBAL grid index through the identical double
// expression, so shared tile-edge vertices (and their normals: same eps =
// 2/(tiles*(N-1)), same d) are BIT-IDENTICAL across tiles — watertight by
// construction, pinned in test_sphere_param. fill_face(hf,f,N) ==
// fill_face(hf,f,N,0,0,1) exactly (the legacy single-mesh face).
//
// T25b subdivision-trim depth for the base terrain vs the tunnel cut disks —
// the sibling of tunnel_mesh.h's kMouthSplitDepth. A cut-touched terrain facet
// recurse-subdivides to this depth so a disk chording a facet INTERIOR (all 3
// corners outside the disk — the T23 "terrain cover sheet" kill shape) is
// trimmed instead of surviving whole. split_depth = 0 reproduces the legacy
// whole-facet behavior (no subdivision).
inline constexpr int kPlanetCutSplitDepth = 4;

// T2/T25b: `cuts` (default empty) carves mouth openings. A facet DISJOINT from
// every cut disk is kept whole (its original shared vertices); a facet fully
// inside a cut (all 3 corners in a disk) is dropped; a facet a disk TOUCHES is
// recurse-subdivided at edge midpoints to `split_depth`, and only fully-inside
// leaves drop (a mixed leaf is kept — over-cover, never a void). The subdivided
// leaf vertices are APPENDED to positions/normals; existing grid vertices are
// unchanged, so shared tile edges never crack. Empty `cuts` ⇒ bit-identical to
// the pre-T2 fill (the no-op fast path). `split_depth = 0` ⇒ legacy
// whole-facet (any interior chord survives — the mutation lever).
// ★ R1 (BLOCK-VP1): `fold`, when non-null, adds world::SnowpackField::
// draw_fold_at(dir) to every vertex radius -- the ambient snow depth, masked
// away from plowed corridors. This is what lifts the DRAWN world out of the
// ~0.77 m hole it sits in relative to the driven one. `fold == nullptr` is
// bit-identical to the pre-R1 fill.
FaceMesh fill_face(const HeightField& hf, int f, int N, int tile_x, int tile_y,
                   int tiles, const std::vector<CutDisk>& cuts = {},
                   int split_depth = kPlanetCutSplitDepth,
                   const world::SnowpackField* fold = nullptr);

// R4d: the radius of the RENDERED terrain surface at direction d — the linear
// interpolation of the fill_face(N, tiles) triangle that contains d, through
// the identical corner expression fill_face uses. This is what a draped decal
// (roads) must sit on to read as ON the terrain: radius_at is the FIELD, but
// between mesh vertices the screen shows the facet, and the field can ride
// metres above/below it (measured p99 ≈ 6 m at subdiv 200 untiled — the
// floating-road defect). Same field, the mesh's own interpolation — this is
// the anti-fork, not a second height source. Pinned against fill_face output
// in test_sphere_param.
double facet_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles);

// ★ R1: the DRAWN facet -- facet_radius_at PLUS the folded snow, interpolated
// through the identical corner expression, because fill_face now lays its
// vertices on exactly this. Every DRAPE (roads, banks, rivers) must sit on THIS,
// or it detaches from the mesh by the fold amount.
//
// ★ AND THE DRIVE MUST NOT. facet_radius_at above stays the TERRAIN facet, and
// it is what app/ injects as SnowpackField::facet_radius_fn. [snowpack]
// hf_faceted_ground is TRUE in the shipped config, so the sled reads that
// injection; handing it the drawn facet would add ambient depth a SECOND time
// (drive_radius_at already carries it) and silently move feel by ~0.7 m.
// One body serves both (facet_radius_impl) so they can never fork in shape --
// only in whether the fold term is present.
double drawn_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles,
                       const world::SnowpackField* fold);

// The DRAPE-FACING form. Roads, banks and rivers are built deep inside their
// own modules and have no way to reach a fold provider, so the one the planet
// was actually built with is bound once (render/draw.cpp, right after the
// planet loads) and read here.
//
// ★ A GLOBAL IS SAFE HERE AND WOULD NOT BE FOR THE DRIVE. Nothing on the
// physics path calls this -- the sled reads facet_radius_at, the terrain facet,
// injected explicitly in app/main.cpp. That asymmetry is the whole reason R1
// cannot move feel.
void set_drawn_fold(const world::SnowpackField* fold);
double drawn_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles);

// ★ ROAD-REPAIR P1 (red-team 2026-09-09) -- THE FOLD-CORNER MEMO.
//
// A drawn-facet query evaluates three grid corners, and each one costs a
// lines->nearest() plus an ambient evaluation. The sink floor put that query on
// sample_at's per-substep path, so without a memo one corridor sample costs
// THREE corridor lookups, not one -- the cost W1.1/INV-1 forbid there. The memo
// caches the per-corner value (direct-mapped, thread_local, keyed by provider +
// generation + hf + N + tiles + face + grid i/j); a hit returns the identical
// double, so the drawn surface is unchanged to the bit. Terrain-facet queries
// (facet_radius_at) do not touch it.
//
// ⚠ set_drawn_fold invalidates automatically. Anyone who mutates a BOUND fold
// provider's dials or rasters in place must call invalidate_facet_fold_memo().
void invalidate_facet_fold_memo();
// OFF == the pre-memo behaviour exactly (three lookups per corner set). The
// default is ON, or OFF if SEADS_FACET_MEMO=0 is in the environment; both the
// kill switch and the A/B arm of the SEADS_DECK_COST measurement.
void set_facet_fold_memo(bool on);
bool facet_fold_memo_enabled();

}  // namespace render
