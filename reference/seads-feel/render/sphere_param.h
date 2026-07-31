#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

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
//   - the future GL CUBEMAP texel param (the §2 seam fix, not built yet) is RAW
//     gnomonic in GL's OWN face-axis convention — NOT face_basis() below.
//     Reusing this table for the cubemap bakes each face flipped/rotated.
//
// Precision: directions/heights are computed in DOUBLE (the float atan2/asin
// the old inline path used moved the 15 km mesh sub-mm; going double is the
// accepted tightening — see the commit note). Mesh vertices are emitted as
// float (render precision after the eye-relative rebase; the double->float seam
// is in draw.cpp).

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

// Equirectangular map coords for a unit direction (the ONE C++ source; the GLSL
// fragment shader keeps its own copy until the cubemap bake deletes it). u
// wraps to [0,1) with u_offset (longitude alignment); v in [0,1], pole-clamped.
glm::dvec2 equirect_uv(glm::dvec3 d, double u_offset);

// Persistent CPU height field: the retained, PROCESSED (post-blur, later
// post-lake-flatten) heightmap red channel. Built once from the same buffer the
// mesh consumed — NEVER a file re-read (that samples the un-blurred field and
// later the un-flattened lakes, so props float/sink by the delta). Captures R +
// relief_scale + u_offset at construction, so world.toml stays R-free (R is the
// sim's) and mesh/props/landing all read one value.
struct HeightField {
    std::vector<std::uint8_t> px;  // red channel, row-major, size w*h
    int w = 0, h = 0;
    double R = 0.0;             // sea-level radius (DEM 0 == R)
    double relief_scale = 0.0;  // metres at full-white (255)
    double u_offset = 0.0;      // longitude alignment (matches the map)

    // Bilinear sample in [0,1] uv; u repeats (longitude), v clamps (poles).
    // 0..1.
    double sample01(double u, double v) const;
    // R at a unit direction: sea level + up to relief_scale. This IS the ground
    // elevation query the mesh, props, and airstrip contact all share.
    double radius_at(glm::dvec3 d) const;
};

// Pure mesh fill for one warped face at subdiv N (N verts per edge). No raylib:
// returns flat arrays the caller uploads (render/planet.cpp). positions/normals
// are 3*N*N floats; indices are 3*2*(N-1)*(N-1). No texcoords (the shader
// samples from the interpolated direction; per-vertex UVs were dead).
struct FaceMesh {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<unsigned short> indices;
};
FaceMesh fill_face(const HeightField& hf, int f, int N);

}  // namespace render
