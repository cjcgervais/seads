// SEADS procedural fighter mesh (Step 5 client, renderer cosmetic). A pure vertex-data builder
// for a low-poly generic WWII prop fighter, split into the three damage REGIONS the WEAPON-001
// wire replicates (ENGINE / WING / TAIL — v1.18r0 order) plus the central BODY hull, so the
// viewer can tint a knocked-out region straight from the decoded region pools. PRESENTATION-ONLY
// and GPU-free: plain float arrays, no raylib types, fully unit-testable headless — the viewer
// uploads these into GPU meshes once at startup.
//
// Body frame: +X = nose, +Y = canopy up, +Z = starboard. Origin near the CG; overall length
// ~1.1 units, wingspan ~0.9 (the viewer scales at draw time).
#pragma once
#include <vector>

namespace seads {
namespace client {

// Triangle soup for one part of the fighter: 3 vertices per triangle, CCW wound facing outward.
struct MeshPart {
    std::vector<float> vertices;  // xyzxyz... (3 floats per vertex)
    std::vector<float> normals;   // per-vertex unit normals (constant across each face)
    std::vector<float> shade;     // per-vertex brightness in [0,1] — a fixed body-frame key light
                                  // baked in (the viewer has no lighting shader; a constant
                                  // Lambert term is enough for the low-poly form to read)
    int tri_count() const { return static_cast<int>(vertices.size() / 9); }
};

// The four parts. engine/wing/tail are index-aligned with the wire's region order (0/1/2);
// body is the remaining hull (no damage pool of its own).
struct FighterMesh {
    MeshPart engine;  // cowl + spinner + propeller blades
    MeshPart wing;    // both main planes
    MeshPart tail;    // rear fuselage + horizontal stabilizers + vertical fin
    MeshPart body;    // central fuselage + canopy
};

FighterMesh build_fighter_mesh();

}  // namespace client
}  // namespace seads
