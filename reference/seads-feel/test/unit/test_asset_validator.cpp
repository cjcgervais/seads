// Fleet Rig asset validator (docs/fleet_rig_plan.md "Asset-validator ctest";
// /orchestrate rig-A.3). Fable's #1 fix (the before-consult C7): the ONE
// failure class that is systematic across the whole future Gemini asset
// factory, invisible to the build and to the controller goldens, and plausible
// enough to ship — CONVENTION DRIFT in generated data (a tail-first plane, a
// cm-vs-m 100x unit slip, a hinge whose origin sits at the centroid so the
// surface sweeps THROUGH the wing, a stray clock/state uniform in a shader).
//
// This runs HEADLESSLY against the PURE catalog (render/rig.cpp) + the shader
// SOURCE string (render::mirror_fs_source) — no GL context — so it can gate
// every rig build in seads_tests. It pins the conventions the eventual
// generated meshes must ALSO satisfy; the mesh-AABB / winding checks that need
// real vertex data activate when a mesh FILE is ingested (rig-A.2 draws
// procedural GenMeshCubes, whose AABB == box_dims by construction).

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <fstream>
#include <glm/glm.hpp>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "render/aurora_glsl.h"
#include "render/post_glsl.h"
#include "render/rig.h"
#include "render/scatter_glsl.h"
#include "render/star_glsl.h"
#include "render/sudbury_gis.gen.h"
#include "world/props.h"

// rig-D D.1b: parse the exported Bf 109 GLBs headlessly. cgltf is header-only
// and C; seads_tests links no raylib, so this CGLTF_IMPLEMENTATION is the only
// copy.
#define CGLTF_IMPLEMENTATION
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <cmath>

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// The GLSL uniforms each mirror shader stage is ALLOWED to declare. The
// active-uniform set the GL linker keeps is a SUBSET of what is declared, so
// "declared ⊆ allowlist" is a stronger check than the GL introspection Fable
// specified — and it needs no context. Any addition (a smuggled u_time / state
// backdoor) falls outside this set and fails (f). The VS's set is raylib's
// auto-uploaded transforms (mvp/matModel/matNormal); a clock there is caught
// too.
const std::set<std::string> kFsUniformAllowlist = {
    "env",          "u_planeColor", "u_sunDir",    "u_fresnelPower",
    "u_reflectivity", "u_bodyFloor",  "u_emissive",  "u_whiteMix",
    "u_rimGain",    "u_rimWhite",   "u_desat"};
const std::set<std::string> kVsUniformAllowlist = {"mvp", "matModel",
                                                   "matNormal"};

// The uniform NAMES the Stage-3 scatter() body is permitted to REFERENCE — the
// [atmosphere] block's scatter entries (declared in kSkyUniformsGLSL, not in
// the generated body). The body itself must DECLARE none of its own (that is
// the backdoor guard below); this set documents the sanctioned surface.
const std::set<std::string> kScatterRefAllowlist = {
    "u_rayleigh", "u_mie", "u_scatterStrength", "u_mieG"};

// Extract every uniform NAME declared in a GLSL source. Statement-based (split
// on ';', commas treated as separators) so it catches ';'-glued declarations
// (`...;uniform float u_time;`) and comma lists (`uniform float a, b;`) — the
// minified-generator escapes Fable flagged (P1-1); a whitespace-only tokenizer
// misses both. Strips // and /* */ comments first so a comment can't spoof a
// declaration.
std::vector<std::string> declared_uniforms(const std::string& src) {
    std::string s;
    for (std::size_t i = 0; i < src.size();) {
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
        } else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/'))
                ++i;
            i += 2;
        } else {
            s += src[i++];
        }
    }
    std::vector<std::string> names;
    const auto flush = [&names](const std::string& stmt) {
        std::string t = stmt;
        for (char& c : t)
            if (c == ',' || c == '[') c = ' ';  // list + array-suffix breaks
        std::stringstream ss(t);
        std::vector<std::string> toks;
        std::string tok;
        while (ss >> tok) toks.push_back(tok);
        std::size_t u = std::string::npos;
        for (std::size_t i = 0; i < toks.size(); ++i)
            if (toks[i] == "uniform") {
                u = i;
                break;
            }
        if (u == std::string::npos) return;
        // toks[u+1] is the type; every token after it is a declared name.
        for (std::size_t i = u + 2; i < toks.size(); ++i)
            names.push_back(toks[i]);
    };
    std::string stmt;
    for (char c : s) {
        if (c == ';') {
            flush(stmt);
            stmt.clear();
        } else {
            stmt += c;
        }
    }
    flush(stmt);
    return names;
}

// The only preprocessor directive a rig shader may use is #version — a
// #define/#include could macro-paste a keyword past the tokenizer (P1-1). True
// iff every '#' in the source begins a "#version" directive.
bool only_version_directive(const std::string& src) {
    for (std::size_t i = 0; i < src.size(); ++i) {
        if (src[i] != '#') continue;
        if (src.compare(i, 8, "#version") != 0) return false;
    }
    return true;
}

// Assert a shader's declared uniforms are exactly `allow` and it uses no stray
// preprocessor directive.
void check_uniforms(const std::string& src,
                    const std::set<std::string>& allow) {
    REQUIRE(only_version_directive(src));
    const std::vector<std::string> names = declared_uniforms(src);
    REQUIRE(!names.empty());  // the parser actually found declarations
    for (const std::string& n : names) {
        INFO("declared uniform: " << n);
        REQUIRE(allow.count(n) == 1);
        std::string low = n;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        REQUIRE(low.find("time") == std::string::npos);
        REQUIRE(low.find("clock") == std::string::npos);
    }
    // Every expected uniform present (a dropped one changes the look silently).
    const std::set<std::string> got(names.begin(), names.end());
    REQUIRE(got == allow);
}

// The rig's assembled model-space AABB from the rest pose: 8 corners of every
// node's box (±box_dims/2 in the node's local frame) transformed by its world
// matrix. This is the geometry a generated mesh must reproduce.
struct AABB {
    glm::vec3 lo{1e9f}, hi{-1e9f};
};
AABB rig_aabb() {
    const render::Rig r = render::build_aircraft_rig();
    const auto& specs = render::aircraft_node_specs();
    AABB b;
    for (int i = 0; i < render::kNodeCount; ++i) {
        const glm::vec3 h = specs[i].box_dims * 0.5f;
        for (int c = 0; c < 8; ++c) {
            const glm::vec3 corner{(c & 1) ? h.x : -h.x, (c & 2) ? h.y : -h.y,
                                   (c & 4) ? h.z : -h.z};
            const glm::vec3 w = glm::vec3(r[i].world * glm::vec4(corner, 1.0f));
            b.lo = glm::min(b.lo, w);
            b.hi = glm::max(b.hi, w);
        }
    }
    return b;
}

bool is_canonical_axis(const glm::vec3& a) {
    const glm::vec3 m = glm::abs(a);
    const int ones = (m.x > 0.999f) + (m.y > 0.999f) + (m.z > 0.999f);
    const int zeros = (m.x < 1e-4f) + (m.y < 1e-4f) + (m.z < 1e-4f);
    return ones == 1 && zeros == 2;
}

// --- rig-D D.1b: the ingested-GLB reader (cgltf) --------------------------
// Parses one GLB into local-frame vertex/index data + mesh/node counts + the
// node transform. Everything the real-vertex validator legs need, with NO GL
// context. Returns ok=false on any parse failure (a missing/corrupt asset).
struct GlbMesh {
    bool ok = false;
    int meshes = 0;
    int nodes = 0;
    bool node_identity = false;
    std::vector<glm::vec3> pos;
    std::vector<unsigned> idx;
    std::vector<glm::vec3> nrm;
    glm::vec3 lo{1e9f}, hi{-1e9f};
    glm::vec3 centroid() const {
        glm::vec3 c{0.0f};
        for (const glm::vec3& p : pos) c += p;
        return pos.empty() ? c : c / float(pos.size());
    }
    glm::vec3 dims() const { return hi - lo; }
    // Signed volume (divergence theorem over the indexed triangles). Positive
    // iff the winding is outward (CCW) on a closed mesh — the winding leg.
    float signed_volume() const {
        float v = 0.0f;
        for (std::size_t t = 0; t + 2 < idx.size(); t += 3) {
            const glm::vec3& a = pos[idx[t]];
            const glm::vec3& b = pos[idx[t + 1]];
            const glm::vec3& c = pos[idx[t + 2]];
            v += glm::dot(a, glm::cross(b, c));
        }
        return v / 6.0f;
    }
};

std::string glb_path(const std::string& key) {
    return std::string(SEADS_ASSET_DIR) + "/bf109/" + key + ".glb";
}

GlbMesh load_glb(const std::string& path) {
    GlbMesh m;
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
        return m;
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        return m;
    }
    m.meshes = int(d->meshes_count);
    m.nodes = int(d->nodes_count);
    if (d->nodes_count == 1) {
        cgltf_float t[16];
        cgltf_node_transform_local(&d->nodes[0], t);
        bool id = true;
        for (int i = 0; i < 16; ++i) {
            const float e = (i % 5 == 0) ? 1.0f : 0.0f;  // identity diagonal
            if (std::abs(t[i] - e) > 1e-5f) id = false;
        }
        m.node_identity = id;
    }
    if (d->meshes_count >= 1) {
        for (cgltf_size p = 0; p < d->meshes[0].primitives_count; ++p) {
            const cgltf_primitive& prim = d->meshes[0].primitives[p];
            const cgltf_accessor* posA = nullptr;
            const cgltf_accessor* nrmA = nullptr;
            for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                if (prim.attributes[a].type == cgltf_attribute_type_position)
                    posA = prim.attributes[a].data;
                if (prim.attributes[a].type == cgltf_attribute_type_normal)
                    nrmA = prim.attributes[a].data;
            }
            const unsigned base = unsigned(m.pos.size());
            if (posA) {
                for (cgltf_size i = 0; i < posA->count; ++i) {
                    float v[3];
                    cgltf_accessor_read_float(posA, i, v, 3);
                    const glm::vec3 q(v[0], v[1], v[2]);
                    m.pos.push_back(q);
                    m.lo = glm::min(m.lo, q);
                    m.hi = glm::max(m.hi, q);
                }
            }
            if (nrmA) {
                for (cgltf_size i = 0; i < nrmA->count; ++i) {
                    float v[3];
                    cgltf_accessor_read_float(nrmA, i, v, 3);
                    m.nrm.emplace_back(v[0], v[1], v[2]);
                }
            }
            if (prim.indices) {
                for (cgltf_size i = 0; i < prim.indices->count; ++i)
                    m.idx.push_back(base + unsigned(cgltf_accessor_read_index(
                                               prim.indices, i)));
            }
        }
    }
    m.ok = !m.pos.empty();
    cgltf_free(d);
    return m;
}

}  // namespace

// (f) SHADER-UNIFORM ALLOWLIST — the clock/state backdoor guard, on BOTH the VS
// and FS (the VS is authored, so it too could smuggle a clock). Every declared
// uniform must be on its stage's allowlist; no time/clock uniform can reach
// render/. Mutation: `uniform float u_time;` (even ';'-glued or comma-listed)
// in either source fails the subset check (verified).
TEST_CASE(
    "asset-validator: mirror shader uniforms are a subset of the allowlist") {
    check_uniforms(render::mirror_vs_source(), kVsUniformAllowlist);
    check_uniforms(render::mirror_fs_source(), kFsUniformAllowlist);
}

// The Stage-3 scatter() body — the FIRST Gemini-asset-factory shader output
// ingested into SEADS. It is a pure FUNCTION that REFERENCES the [atmosphere]
// uniform block (declared elsewhere) and must DECLARE none of its own — so the
// clock/state backdoor guard is simply "declares zero uniforms": a smuggled
// `uniform float u_time;` (even ';'-glued or comma-listed) makes the declared
// set non-empty and fails. Plus no macro directive (a #define/#include could
// macro-paste past the tokenizer) and no time/clock token anywhere.
TEST_CASE(
    "asset-validator: generated scatter() body declares no uniform (backdoor "
    "guard)") {
    const std::string body = render::kScatterGLSL;
    REQUIRE(only_version_directive(body));     // no #define/#include smuggle
    REQUIRE(declared_uniforms(body).empty());  // references, never declares
    std::string low = body;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    REQUIRE(low.find("time") == std::string::npos);
    REQUIRE(low.find("clock") == std::string::npos);
    // Document the sanctioned reference surface (the body uses only these).
    REQUIRE(kScatterRefAllowlist.count("u_rayleigh") == 1);
}

// The Stage-4 star pass shaders (hand-authored, kept in the pure render core so
// this headless leg can gate them like the generated scatter body). Same
// clock/state-backdoor guard: every declared uniform is on its stage's
// allowlist, and no time/clock token or stray macro directive. The FS's shared
// [atmosphere]/scatter uniforms are concatenated by stars.cpp from the already-
// gated kSkyUniformsGLSL/kScatterGLSL, so only the star-LOCAL block is checked
// here. matView/matProjection are raylib's auto-uploaded transforms (a clock
// there is caught too).
TEST_CASE(
    "asset-validator: star shader uniforms are a subset of the allowlist") {
    const std::set<std::string> kStarVsAllowlist = {
        "matModel", "matView", "matProjection", "uRStar", "uStarSizeView"};
    const std::set<std::string> kStarFsAllowlist = {
        "uSunDir",   "uUp",      "uEyeAlt",  "uHorizonElev", "uStarBrightness",
        "uMagRef",   "uGlareCos", "uWashLum", "uMoonDir",    "uMoonAngRad",
        // S-starnight: how much clear dome AIR veils stars (ships 0).
        "uStarAirExt"};
    check_uniforms(render::kStarVS, kStarVsAllowlist);
    // The FS uniforms all live in the DECLS block; MAIN must declare none (a
    // smuggled clock uniform there would slip past a decls-only check).
    check_uniforms(render::kStarFsDecls, kStarFsAllowlist);
    REQUIRE(only_version_directive(render::kStarFsMain));
    REQUIRE(declared_uniforms(render::kStarFsMain).empty());
}

// The S1 stereoscope POST-process FS (render::kPostFS) — a STANDALONE
// fullscreen shader (paired with raylib's default VS), so unlike the
// concatenated scatter body it MUST declare its own uniforms. Same clock/state
// backdoor guard: the declared set is EXACTLY the allowlist and carries no
// time/clock token. This is the guard that matters most here — the pass takes a
// per-frame grain SEED (uFrameCount, an app-owned COSMETIC counter); the
// exact-match allowlist proves that ordinal is the ONLY time-like input and
// nothing smuggled a real clock in.
TEST_CASE(
    "asset-validator: post-process shader uniforms are exactly the allowlist") {
    const std::set<std::string> kPostFsAllowlist = {
        "texture0", "uDepth", "uResolution", "uContrast", "uLift", "uGrain",
        "uSplitShadow", "uSplitHi", "uSatC0", "uSatC1", "uSatDark", "uHalation",
        "uHalThresh", "uHalTint", "uVignette", "uFxaa", "uFrameCount",
        "uDebugMode", "uNear", "uFar",
        // S6 far-field-only DoF (view-space focus range + disc radius + sky
        // cap).
        "uFocusStart", "uFocusEnd", "uDofRadius", "uSkyCoc",
        // S6 "printed card" halftone/dither MODE (off by default).
        "uHalftoneMode", "uHalftoneScale", "uHalftoneAngle", "uHalftoneSoft",
        "uHalftoneInk", "uHalftoneGrainMul"};
    // ★ `uFog` (the SC1 helmet fog) was here until Chad's 2026-08-17 ruling
    // deleted the effect. The allowlist is EXACT, so this test is what proves
    // the uniform is really gone from the shader and not merely unfed.
    check_uniforms(render::kPostFS, kPostFsAllowlist);
}

// The Stage-8 aurora curtain — the SECOND Gemini-generated shader body
// ingested. Like scatter(): a pure FUNCTION referencing only its parameters,
// declaring NO uniform (the clock/state backdoor guard). PLUS the periodicity
// contract (Fable-after P1-3/P2-1): the curtain animates via phaseSC =
// vec2(sin,cos) and the sin(k*az+phi) angle-addition identity, so it MUST
// contain NO `atan` (which would recover phi and reintroduce the wrap strobe)
// and no time/clock token.
TEST_CASE(
    "asset-validator: generated aurora curtain declares no uniform, no atan "
    "(periodicity)") {
    const std::string body = render::kAuroraGLSL;
    REQUIRE(only_version_directive(body));  // no #define/#include smuggle
    REQUIRE(
        declared_uniforms(body).empty());  // references params, declares none
    std::string low = body;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    REQUIRE(low.find("time") == std::string::npos);
    REQUIRE(low.find("clock") == std::string::npos);
    REQUIRE(low.find("atan") == std::string::npos);  // the periodicity guard
}

// (a) UNITS — every box dimension is positive and metre-scale, guarded on BOTH
// sides of a unit slip (Fable P1-2): a cm->m INFLATE (x100) blows an 8 m
// fuselage past the 20 m ceiling; a m->cm DEFLATE (/100) shrinks it below the
// fuselage-length floor (an 8 cm fuselage is not a plane). The floor is the
// known-size ruler — the fuselage length — so a one-sided ceiling can't let the
// shrink case sail through.
TEST_CASE("asset-validator: node box dimensions are positive and metre-scale") {
    const auto& specs = render::aircraft_node_specs();
    for (int i = 0; i < render::kNodeCount; ++i) {
        const glm::vec3 d = specs[i].box_dims;
        INFO("node " << i);
        REQUIRE(d.x > 0.0f);
        REQUIRE(d.y > 0.0f);
        REQUIRE(d.z > 0.0f);
        REQUIRE(d.x < 20.0f);  // a light aircraft part, not a runway
        REQUIRE(d.y < 20.0f);
        REQUIRE(d.z < 20.0f);
    }
    // The fuselage length is the ruler: a metre-scale aircraft, not
    // centimetres.
    REQUIRE(specs[render::kFuselage].box_dims.z > 2.0f);
}

// (b) FORWARD AXIS = -Z (SPEC §7). The catalog places the nose group (spinner)
// forward of origin and the tail surfaces aft, and the assembled geometry spans
// the origin on Z. This pins the CATALOG's forward sense (the rest poses I
// author). NOTE (Fable P0-1): the tail-first MESH — a GLB modeled backwards
// while the catalog is correct — is NOT catchable here (a symmetric AABB test
// passes a 180° flip); that is the cgltf CHIRALITY leg, which activates with
// the ingested GLBs (rig-D D.2). This leg is the catalog half of the pair. The
// old "nose reaches farther than tail" AABB assertion was DROPPED: it held for
// the placeholder (prop stuck way out front) but is FALSE for a real airframe
// whose CG is forward, so the tail reaches farther aft than the nose reaches
// forward. Marker + spans-origin is the correct catalog invariant.
TEST_CASE("asset-validator: the aircraft faces -Z (nose forward, tail aft)") {
    const auto& s = render::aircraft_node_specs();
    REQUIRE(s[render::kSpinner].rest_pos.z < 0.0f);  // nose (spinner) forward
    REQUIRE(s[render::kEngineCowl].rest_pos.z < 0.0f);  // engine forward too
    REQUIRE(s[render::kRudder].rest_pos.z > 0.0f);  // tail aft (child-local +Z)
    REQUIRE(s[render::kVertStab].rest_pos.z > 0.0f);       // fin aft
    REQUIRE(s[render::kLeftHorizStab].rest_pos.z > 0.0f);  // tailplane aft
    REQUIRE(s[render::kFuselage].parent == -1);  // fuselage is the root

    const AABB b = rig_aabb();
    REQUIRE(b.lo.z < 0.0f);  // geometry reaches forward of origin (nose)
    REQUIRE(b.hi.z > 0.0f);  // and aft of origin (tail) — spans the origin on Z
}

// (d) HINGE AXES — every control-surface node hinges about a canonical unit
// body axis (elevator/aileron +X pitch/roll, rudder +Y yaw). A procedural
// GenMeshCube is symmetric about its own origin, so the hinge passes through
// the node origin by construction; the mesh-hinge-edge check (origin ON the
// declared hinge line) activates when a generated part MESH is ingested (its
// origin may sit at the centroid, sweeping the surface through the wing with
// signs perfect).
TEST_CASE(
    "asset-validator: driven surfaces hinge about a canonical unit axis") {
    const auto& s = render::aircraft_node_specs();
    int driven = 0;
    for (int i = 0; i < render::kNodeCount; ++i) {
        if (s[i].driven == render::Driven::None) continue;
        ++driven;
        INFO("driven node " << i);
        REQUIRE(is_canonical_axis(s[i].hinge_axis));
    }
    // rig-D driven set (12): 2 ailerons + 2 elevators + rudder + 2 gear struts
    // + 2 gear doors + tailwheel + 2 main wheels (Driven::Wheel, R4 ground-roll
    // spin). Flaps are separable but STATIC (no flap Input).
    REQUIRE(driven == 12);
    // Pin the axis PER driven node — is_canonical_axis alone would pass a
    // rudder hinged +X. The NodeSpec default hinge is {1,0,0}, so a DROPPED
    // aileron assignment is invisible without an explicit +X pin (Fable P2).
    // Elevators + ailerons pitch/roll about +X; rudder yaws about +Y; the mains
    // + doors retract about +Z (swing outboard-up), the tailwheel about +X.
    REQUIRE(is_canonical_axis(s[render::kLeftAileron].hinge_axis));
    REQUIRE(s[render::kLeftAileron].hinge_axis.x > 0.999f);
    REQUIRE(s[render::kRightAileron].hinge_axis.x > 0.999f);
    REQUIRE(s[render::kLeftElevator].hinge_axis.x > 0.999f);
    REQUIRE(s[render::kRightElevator].hinge_axis.x > 0.999f);
    REQUIRE(s[render::kRudder].hinge_axis.y > 0.999f);
    REQUIRE(s[render::kLeftGearStrut].driven == render::Driven::Gear);
    // rig-D D.3b (Fable P0-1): the mains + their fairings retract MIRRORED —
    // the LEFT about +Z, the RIGHT about −Z — else a shared +Z sweep sends the
    // right leg across the belly instead of outboard-up. Pin BOTH signs.
    REQUIRE(s[render::kLeftGearStrut].hinge_axis.z > 0.999f);
    REQUIRE(s[render::kRightGearStrut].hinge_axis.z < -0.999f);
    REQUIRE(s[render::kLeftGearDoor].hinge_axis.z > 0.999f);
    REQUIRE(s[render::kRightGearDoor].hinge_axis.z < -0.999f);
    REQUIRE(s[render::kTailWheel].hinge_axis.x > 0.999f);
    // R4 wheel spin: the main tyres roll about the lateral +X axle.
    REQUIRE(s[render::kLeftWheel].driven == render::Driven::Wheel);
    REQUIRE(s[render::kRightWheel].driven == render::Driven::Wheel);
    REQUIRE(s[render::kLeftWheel].hinge_axis.x > 0.999f);
    REQUIRE(s[render::kRightWheel].hinge_axis.x > 0.999f);
}

// (e) UNIFORM-SCALE PARENTS — a non-uniformly-scaled parent shears every
// rotating child (world = parent.world · local), so every node on the parent
// chain of a driven surface must be uniform-scale (Fable). Shape lives in the
// mesh (box_dims), so all node scales are identity here; the validator pins
// that a future authored leaf scale can never appear on a rotating child's
// ancestor.
TEST_CASE("asset-validator: parents of rotating children are uniform-scale") {
    const render::Rig r = render::build_aircraft_rig();
    const auto& specs = render::aircraft_node_specs();
    for (int i = 0; i < render::kNodeCount; ++i) {
        if (specs[i].driven == render::Driven::None) continue;
        for (int p = r[i].parent; p >= 0; p = r[p].parent) {
            INFO("ancestor " << p << " of driven node " << i);
            const glm::vec3 sc = r[p].scl;
            REQUIRE(sc.x == sc.y);
            REQUIRE(sc.y == sc.z);
        }
    }
}

// ==========================================================================
// rig-D D.1b — the REAL-VERTEX legs, FIRING against the ingested Bf 109 GLBs
// (assets/bf109/*.glb, D.2). These are the checks Fable flagged as the #1
// invisible failure class: convention drift in generated MESH data that the
// build and the controller goldens never see. Each leg loads real vertices.
// ==========================================================================

// (g) ONE MESH + IDENTITY NODE per GLB (Fable P1-1). All shape lives in the
// vertices (identity node transform) so draw.cpp's eye-relative body*node
// matrix is the ONLY transform — a baked node translation/rotation/scale would
// double-apply. And exactly one mesh so a stray extra object can't ride along.
TEST_CASE(
    "asset-validator: every ingested GLB is one mesh under one identity node") {
    const auto& s = render::aircraft_node_specs();
    int checked = 0;
    for (int i = 0; i < render::kNodeCount; ++i) {
        const std::string key = s[i].mesh_key;
        if (key.empty()) continue;
        INFO("node " << i << " key " << key);
        const GlbMesh m = load_glb(glb_path(key));
        REQUIRE(m.ok);             // parsed with vertices
        REQUIRE(m.meshes == 1);    // P1-1: exactly one mesh
        REQUIRE(m.nodes == 1);     // one node
        REQUIRE(m.node_identity);  // identity transform (shape is in the verts)
        ++checked;
    }
    REQUIRE(checked == render::kNodeCount);  // all 30 nodes ingested
}

// (h) UNITS — the real-vertex AABB matches box_dims (Fable P1-2, both sides of
// a unit slip: a cm->m x100 inflate OR m->cm /100 deflate both blow the
// tolerance). box_dims was reconciled to these exported AABBs (rig.cpp D.1b);
// this pins that they stay in lockstep — a re-exported mesh at a different
// scale fails here.
TEST_CASE("asset-validator: each GLB real-vertex AABB matches its box_dims") {
    const auto& s = render::aircraft_node_specs();
    for (int i = 0; i < render::kNodeCount; ++i) {
        const std::string key = s[i].mesh_key;
        if (key.empty()) continue;
        const GlbMesh m = load_glb(glb_path(key));
        REQUIRE(m.ok);
        const glm::vec3 d = m.dims();
        const glm::vec3 box = s[i].box_dims;
        INFO("node " << i << " key " << key << " aabb " << d.x << "," << d.y
                     << "," << d.z << " box " << box.x << "," << box.y << ","
                     << box.z);
        for (int c = 0; c < 3; ++c) {
            const float tol = std::max(0.03f, 0.06f * box[c]);
            REQUIRE(std::abs(d[c] - box[c]) < tol);
        }
    }
}

// (i) HINGE ORIGIN ON THE LINE (Fable: a hinge at the CENTROID sweeps the
// surface THROUGH the wing). Author rule: origin ON the hinge line at the
// surface edge. Control surfaces (pitch/roll/rudder) hinge at the LEADING EDGE
// and extend aft +Z; gear (struts/doors/tailwheel) pivot at the TOP and hang
// -Y. A centroid origin would straddle 0 on that axis — caught here.
TEST_CASE("asset-validator: driven-surface GLB origins sit on the hinge line") {
    const auto& s = render::aircraft_node_specs();
    int checked = 0;
    for (int i = 0; i < render::kNodeCount; ++i) {
        if (s[i].driven == render::Driven::None) continue;
        const GlbMesh m = load_glb(glb_path(s[i].mesh_key));
        REQUIRE(m.ok);
        INFO("driven node " << i << " key " << s[i].mesh_key);
        if (s[i].driven == render::Driven::Gear) {
            REQUIRE(m.hi.y < 0.06f);   // origin at/above the top pivot
            REQUIRE(m.lo.y < -0.15f);  // leg/door hangs below it (not centroid)
        } else if (s[i].driven == render::Driven::Wheel) {
            // R4 wheel spin: a tyre's origin is its HUB — the mesh must be
            // symmetric about the origin in the spin plane (Y/Z), or the
            // wheel orbits its axle instead of rolling on it.
            REQUIRE(std::abs(m.lo.y + m.hi.y) < 0.05f);
            REQUIRE(std::abs(m.lo.z + m.hi.z) < 0.05f);
        } else {
            // rig-D round 2: the surfaces are SET-BACK hinges — a round LE nose
            // concentric with the hinge (constant deflection gap, Fable), so
            // the nose bulges up to ~r_nose (≤0.081) AHEAD of the origin. The
            // origin still sits at the hinge, far from a centroid (~−0.22 for
            // these chords); −0.12 separates "hinge line" from "centroid".
            REQUIRE(m.lo.z >
                    -0.12f);  // origin at the hinge (nose bulges ≤r ahead)
            REQUIRE(m.hi.z > 0.2f);  // chord extends aft (not centroid)
        }
        ++checked;
    }
    REQUIRE(checked == 12);  // the rig-D driven set (+2 R4 spinning wheels)
}

// (j) CHIRALITY (Fable P0-1) — the one leg the pure catalog CANNOT do (a
// tail-first or un-mirrored MESH passes a symmetric AABB test). Every L/R pair
// must be a TRUE mirror across X: mirror the left cloud (x -> -x), sort both,
// and require a vertex-for-vertex match. Catches a swapped dihedral/sweep or a
// right part accidentally copied from the left without the flip.
TEST_CASE("asset-validator: L/R GLB pairs are true mirrors across X") {
    const std::vector<std::pair<std::string, std::string>> pairs = {
        {"wing_l", "wing_r"},           {"aileron_l", "aileron_r"},
        {"flap_l", "flap_r"},           {"wingtip_l", "wingtip_r"},
        {"hstab_l", "hstab_r"},         {"elevator_l", "elevator_r"},
        {"gear_door_l", "gear_door_r"}, {"gear_strut_l", "gear_strut_r"},
        {"wheel_l", "wheel_r"},         {"exhaust_l", "exhaust_r"}};
    for (const auto& pr : pairs) {
        INFO("pair " << pr.first << " / " << pr.second);
        GlbMesh L = load_glb(glb_path(pr.first));
        GlbMesh R = load_glb(glb_path(pr.second));
        REQUIRE(L.ok);
        REQUIRE(R.ok);
        REQUIRE(L.pos.size() == R.pos.size());
        for (glm::vec3& v : L.pos) v.x = -v.x;  // mirror the left cloud
        // Nearest-neighbour match (robust to vertex ordering AND to the exact
        // pairing on x-SYMMETRIC parts, where a sorted index-compare tie-breaks
        // unstably): every right vertex must find a mirrored-left twin.
        for (const glm::vec3& r : R.pos) {
            float best = 1e9f;
            for (const glm::vec3& l : L.pos)
                best = std::min(best, glm::length(l - r));
            INFO("right vertex (" << r.x << "," << r.y << "," << r.z << ")");
            REQUIRE(best < 0.02f);
        }
    }
}

// (k) WINDING — outward face winding on every closed mesh (a globally inverted
// mesh renders inside-out / self-shadows wrong on a MIRROR). Signed volume from
// the indexed triangles (divergence theorem) is positive iff the winding is
// outward CCW. A flipped export makes it negative — caught for all 30.
TEST_CASE(
    "asset-validator: every ingested GLB has outward winding (positive "
    "volume)") {
    const auto& s = render::aircraft_node_specs();
    for (int i = 0; i < render::kNodeCount; ++i) {
        const std::string key = s[i].mesh_key;
        if (key.empty()) continue;
        const GlbMesh m = load_glb(glb_path(key));
        REQUIRE(m.ok);
        REQUIRE(m.idx.size() >= 3);  // indexed triangles present
        INFO("node " << i << " key " << key << " vol " << m.signed_volume());
        REQUIRE(m.signed_volume() > 1e-4f);
    }
}

// ==========================================================================
// stereoscope-sudbury S0 — THE PROJECTION LOCK leg (the ordering-trap made
// mechanical). Every stored sphere direction (lakes/airstrips now; trees/
// ribbons/footprints from S2) is a pure function of the projection params; a
// param change must re-bake all of them. This asserts the generated header's
// embedded fingerprint agrees with assets/projection.lock, so a placement
// artifact baked under a DIFFERENT projection than the checked-in lock fails
// the Stop hook. (The instances>1 leg lands with S2's instance-buffer
// artifact.)
// ==========================================================================
namespace {
std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}
// Fable P1-9 (CRLF-proof): drop every '\r', then strip trailing whitespace —
// the exact normalize() the Python writer hashes through.
std::string normalize_line(std::string v) {
    v.erase(std::remove(v.begin(), v.end(), '\r'), v.end());
    while (!v.empty() &&
           (v.back() == ' ' || v.back() == '\t' || v.back() == '\n'))
        v.pop_back();
    return v;
}
}  // namespace

TEST_CASE(
    "asset-validator: projection.lock fingerprint matches the baked header") {
    // The header self-agrees: FNV of the embedded params == the embedded hash.
    // Catches a hand-edited gen.h (params changed but hash not re-baked).
    REQUIRE(fnv1a64(std::string(render::kSudburyProjectionParams)) ==
            render::kSudburyProjectionLockHash);

    const std::string lock_path =
        std::string(SEADS_ASSET_DIR) + "/projection.lock";
    std::ifstream in(lock_path);
    REQUIRE(in.good());  // the lock is checked in; no lock ⇒ nothing
                         // dir-storing lands
    std::string line, params, hash_field;
    while (std::getline(in, line)) {
        const std::string t = normalize_line(line);
        if (t.rfind("params=", 0) == 0)
            params = t.substr(7);
        else if (t.rfind("hash=", 0) == 0)
            hash_field = t.substr(5);
    }
    REQUIRE(!params.empty());
    REQUIRE(!hash_field.empty());

    // lock ↔ header: the file's params line hashes to the header's embedded
    // hash (a placement baked under a different projection than the lock fails
    // here).
    const std::uint64_t lock_hash = fnv1a64(params);
    INFO("lock params: " << params);
    REQUIRE(lock_hash == render::kSudburyProjectionLockHash);
    // exact string agreement too (belt-and-suspenders on the hash).
    REQUIRE(params == std::string(render::kSudburyProjectionParams));

    // lock internal consistency: its own hash= field (0x..ULL) equals the FNV.
    std::string hx = hash_field;
    if (hx.rfind("0x", 0) == 0) hx = hx.substr(2);
    if (hx.size() >= 3 && hx.substr(hx.size() - 3) == "ULL")
        hx = hx.substr(0, hx.size() - 3);
    const std::uint64_t written = std::stoull(hx, nullptr, 16);
    REQUIRE(written == lock_hash);
}

// The bake-DERIVED probe (Fable P1-10): a stale header beside a fresh lock
// still passes the hash leg, so pin a value that comes FROM the baked equirect
// — the downtown-Sudbury direction + its sampled elevation — and use it as the
// chirality guard (EAST of the midpoint center ⇒ +X; a mirrored planet flips
// it).
TEST_CASE(
    "asset-validator: projection probe is a unit dir, east of center, on the "
    "sphere") {
    const double* d = render::kSudburyProbeDowntownDir;
    const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    REQUIRE(std::abs(len - 1.0) < 1e-6);  // unit surface direction
    REQUIRE(d[0] > 0.0);                  // EAST of center = +X (chirality)
    REQUIRE(render::kSudburyProbeDowntownElev >=
            15000.0 - 1.0);  // at/above sea level
    REQUIRE(render::kSudburyProbeDowntownElev <=
            15000.0 + 2000.0);  // within theatrical relief
}

// ---------------------------------------------------------------------------
// S2 boreal trees: the tree-density raster is the placement artifact whose
// presence FIRES the instances>1 mechanism leg (skill gate stack step 2).
// ---------------------------------------------------------------------------
TEST_CASE(
    "asset-validator: tree-density raster is present with the header dims") {
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/sudbury_treedensity.png";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());  // baked artifact must be checked in beside the lock
    std::array<unsigned char, 24> hdr{};
    in.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
    REQUIRE(in.gcount() == 24);
    // PNG signature + IHDR width/height (big-endian at offsets 16 and 20).
    REQUIRE(hdr[0] == 0x89);
    REQUIRE(hdr[1] == 'P');
    REQUIRE(hdr[2] == 'N');
    REQUIRE(hdr[3] == 'G');
    auto be32 = [&](int o) {
        return (static_cast<int>(hdr[o]) << 24) |
               (static_cast<int>(hdr[o + 1]) << 16) |
               (static_cast<int>(hdr[o + 2]) << 8) |
               static_cast<int>(hdr[o + 3]);
    };
    REQUIRE(be32(16) == render::kSudburyTreeDensityW);
    REQUIRE(be32(20) == render::kSudburyTreeDensityH);
}

// ---------------------------------------------------------------------------
// B2 industrial barrens: the 1970 barren-intensity raster is the offline input
// the albedo lerp and the tree-density multiply both consume. Registering it
// here is what makes the layer CHECKED -- an unregistered asset is an asset
// nobody notices going missing.
//
// The name of this TEST_CASE is deliberately pure ASCII. A non-ASCII character
// in a Catch2 test name makes the case silently never run under ctest; this
// repo has been bitten by that four times and gate.sh now carries a tripwire
// for it. A barrens test that never executes is worse than no test at all.
// ---------------------------------------------------------------------------
TEST_CASE("asset-validator: barrens raster is present with the header dims") {
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/sudbury_barrens.png";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());  // baked artifact must be checked in beside the lock
    std::array<unsigned char, 24> hdr{};
    in.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
    REQUIRE(in.gcount() == 24);
    // PNG signature + IHDR width/height (big-endian at offsets 16 and 20).
    REQUIRE(hdr[0] == 0x89);
    REQUIRE(hdr[1] == 'P');
    REQUIRE(hdr[2] == 'N');
    REQUIRE(hdr[3] == 'G');
    auto be32 = [&](int o) {
        return (static_cast<int>(hdr[o]) << 24) |
               (static_cast<int>(hdr[o + 1]) << 16) |
               (static_cast<int>(hdr[o + 2]) << 8) |
               static_cast<int>(hdr[o + 3]);
    };
    REQUIRE(be32(16) == render::kSudburyBarrensW);
    REQUIRE(be32(20) == render::kSudburyBarrensH);
    // The equirect transport is 2:1 by construction; a barrens raster that is
    // not would land its texels somewhere other than the tree-density texels it
    // has to agree with, which is invisible in a preview and fatal in the air.
    REQUIRE(render::kSudburyBarrensW == 2 * render::kSudburyBarrensH);
}

// The count==1 / attrib-divisor / no-op traps ship GREEN off the build (the
// repo's recurring instancing trap class). Guard the PLACEMENT mechanism at the
// DATA level: it must FIRE (>1) over a dense field, be INERT (0) over a zero
// field (not a no-op that always emits), sit EXACTLY on the height field
// (anti-float), and be byte-deterministic (no per-frame shimmer). Fires because
// the raster exists; a broken placement fails here headlessly.
TEST_CASE(
    "asset-validator: tree placement fires (>1), gates on density, never "
    "floats") {
    render::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.px.assign(
        static_cast<std::size_t>(hf.w) * hf.h,
        static_cast<std::uint16_t>(128 * 257));  // 16-bit store, mid relief
    hf.R = 15000.0;
    hf.relief_scale = 350.0;
    hf.u_offset = 0.0;

    world::TreeParams p;
    p.cells_per_face = 64;  // small + fast; the mechanism is scale-free
    p.chunk_cells = 16;     // G = 4 chunks/face
    p.gain = 1.0;

    world::DensityField dense;
    dense.w = 8;
    dense.h = 4;
    dense.u_offset = 0.0;
    dense.px.assign(static_cast<std::size_t>(dense.w) * dense.h, 255);  // D = 1

    const auto trees = world::place_chunk(4, 0, 0, hf, dense, p);  // +Z face
    REQUIRE(trees.size() > 1);  // <-- the count==1 tripwire

    for (const auto& ti : trees) {
        // anti-float: pos sits on radius_at(dir) with up == dir (the mesh's own
        // height field — a tree can never float/sink relative to the ground).
        const glm::dvec3 dir = glm::normalize(ti.pos);
        REQUIRE(std::abs(glm::length(ti.pos) - hf.radius_at(dir)) < 1e-6);
        REQUIRE(glm::dot(ti.up, dir) > 1.0 - 1e-9);
        REQUIRE(ti.scale >= static_cast<float>(p.min_scale) - 1e-6f);
        REQUIRE(ti.scale <= static_cast<float>(p.max_scale) + 1e-6f);
        REQUIRE(ti.species < 3);
    }

    // density gate: a zero field yields NO trees (not a no-op that always
    // emits).
    world::DensityField zero = dense;
    zero.px.assign(zero.px.size(), 0);  // D = 0
    REQUIRE(world::place_chunk(4, 0, 0, hf, zero, p).empty());

    // determinism: identical output across calls (pure fn of cell id + seed).
    const auto again = world::place_chunk(4, 0, 0, hf, dense, p);
    REQUIRE(again.size() == trees.size());
    REQUIRE(again.front().pos == trees.front().pos);

    // cull: from a low altitude over +Z the visible set is non-empty and a
    // strict subset of all chunks (frustum-independent proximity+horizon, Fable
    // E/G).
    const int G = world::chunks_per_face(p);
    const auto vis =
        world::visible_chunks(glm::dvec3(0, 0, hf.R + 1000.0), hf, p);
    REQUIRE(!vis.empty());
    REQUIRE(vis.size() < static_cast<std::size_t>(6 * G * G));

    // T6 scatter exclusion: a cut disk drops trees inside, leaves trees
    // outside, and an EMPTY cut list is bit-identical (the no-op arm).
    //
    // Strategy:
    //   1. Place chunk 4 (face=+Z) 0,0 with NO cuts to get the baseline set.
    //   2. Centre the disk on the FIRST tree's direction (guaranteed inside).
    //   3. Use a radius of 1 m — tight enough to drop at most a few trees but
    //      never the whole chunk, so the "some survive" arm holds independently
    //      of the chunk geometry.
    //   4. Verify: (a) empty cuts is bit-identical; (b) at least one tree was
    //      dropped; (c) every survivor lies outside the disk.

    // baseline with NO cuts — must equal the original trees set
    const auto no_cut = world::place_chunk(4, 0, 0, hf, dense, p, {});
    REQUIRE(no_cut.size() == trees.size());  // empty == bit-identical
    REQUIRE(no_cut.front().pos == trees.front().pos);

    // Disk centred on the FIRST tree's direction with a 1 m radius. That tree
    // is guaranteed inside (arc = 0 < 1 m). Other trees > 1 m away survive.
    // At ~18 m cell spacing (R=15 km, 64 cells/face) the nearest neighbour is
    // at least one cell away (~1400 m arc), so nearly ALL other trees survive.
    const glm::dvec3 disk_dir = glm::normalize(trees.front().pos);
    const render::CutDisk disk{disk_dir, 1.0};  // 1 m radius
    const std::vector<render::CutDisk> cut_list{disk};

    const auto cut = world::place_chunk(4, 0, 0, hf, dense, p, cut_list);

    // at least the first tree was dropped
    REQUIRE(cut.size() < trees.size());

    // every surviving tree lies OUTSIDE the disk
    for (const auto& ti : cut) {
        const glm::dvec3 d = glm::normalize(ti.pos);
        const double cos_a = std::clamp(glm::dot(d, disk.dir), -1.0, 1.0);
        const double arc_m = std::acos(cos_a) * hf.R;
        REQUIRE(arc_m >= disk.radius_m);
    }

    // some trees must have survived (outside the disk)
    REQUIRE(!cut.empty());
}
