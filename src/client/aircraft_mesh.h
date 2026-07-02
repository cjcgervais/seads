// SEADS procedural fighter mesh (Step 5 client, renderer cosmetic). A pure vertex-data builder
// for low-poly WWII prop fighters, split into the three damage REGIONS the WEAPON-001 wire
// replicates (ENGINE / WING / TAIL — v1.18r0 order) plus the central BODY hull, so the viewer
// can tint a knocked-out region straight from the decoded region pools. PRESENTATION-ONLY and
// GPU-free: plain float arrays, no raylib types, fully unit-testable headless — the viewer
// uploads these into GPU meshes once at startup.
//
// Per-airframe VARIANTS: one silhouette per sealed roster type (radial vs inline nose, wing
// plan/span, tail proportions, overall size), selected by AircraftType. The type code is
// PRESENTATION METADATA carried in the .seadsrec v3 trailer — it is not on any sealed wire, and
// an unknown/absent code falls back to the GENERIC model (the pre-variant fighter).
//
// Body frame: +X = nose, +Y = canopy up, +Z = starboard. Origin near the CG; overall length
// ~1.1 units, wingspan ~0.9 (the viewer scales at draw time).
#pragma once
#include <cstdint>
#include <vector>

namespace seads {
namespace client {

// Sealed-roster airframe types. The numeric values are the STABLE presentation codes written into
// the .seadsrec v3 per-aircraft type trailer (roster order from config/rails/atm.json); GENERIC
// is the explicit fallback for a recording with no trailer or an unknown code.
enum class AircraftType : uint32_t {
    P47D = 0,
    BF109F4 = 1,
    KI61 = 2,
    A6M2 = 3,
    YAK3 = 4,
    LA7 = 5,
    SPITFIRE_MK5 = 6,
    P51 = 7,
    GENERIC = 255,
};
constexpr uint32_t AIRCRAFT_TYPE_COUNT = 8;  // roster variants (GENERIC excluded)

// Decode a .seadsrec type code (unknown -> GENERIC) and a short display name for HUD/feed text.
AircraftType aircraft_type_from_code(uint32_t code);
const char* aircraft_type_name(AircraftType t);

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
    MeshPart body;    // central fuselage + canopy (+ P-51 ventral scoop)
};

// Build the airframe variant for `type` (GENERIC = the original one-size fighter).
FighterMesh build_fighter_mesh(AircraftType type);
inline FighterMesh build_fighter_mesh() { return build_fighter_mesh(AircraftType::GENERIC); }

}  // namespace client
}  // namespace seads
