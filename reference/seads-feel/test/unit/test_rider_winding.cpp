// =============================================================================
// test_rider_winding.cpp -- THE WINDING GATE LEG.
//
// WHY THIS FILE EXISTS, and why it is a ctest and not a script:
//
// The rider's "transparent Sudburian" bugs were all one defect class -- shells
// wound inside-out, backface-culled by an engine that ignores glTF
// doubleSided, so the man reads as a glass case. It was found and fixed FOUR
// separate times (suit boxes, the nine limb prisms a blanket flip inverted,
// four mitt shells, and the source generator that emitted them), and every one
// of those shipped through a green gate.
//
// A winding check DID already exist, and it is worth understanding why it did
// not help, because the reason is the whole point of this file:
// test_asset_validator.cpp's "asset-validator: every ingested GLB has outward
// winding (positive volume)" iterates render::aircraft_node_specs() -- the
// AIRCRAFT meshes -- so it never looks at the rider GLB at all; and it asserts
// on m.signed_volume(), the GRAND TOTAL of a mesh, which is exactly the
// quantity that read +0.01176 while nine limb shells sat inverted underneath.
// Its NAME says "every ingested GLB", which is broader than what it does. That
// gap between a test's name and its reach is how a suite reports coverage it
// does not have.
//
// The fix lived in a repair pass inside patch_scarf.py. That pass is retiring:
// once the source produces what ships, it has nothing to repair. The winter-gi
// lane raised the objection that makes this file necessary -- a validator that
// lives inside a script only runs when someone runs that script, and once the
// mutation is gone nobody has a reason to. The check would then exist without
// ever executing, which is WORSE than a red gate because it looks like
// coverage. So the measurement moves here, where ctest runs it against the
// shipping asset on every build, and outlives the pass that discovered it.
//
// THE LAW BEING ENFORCED, both halves, each learned by getting it wrong:
//   1. PER SHELL, never over an assembly. A positive grand total hides
//      per-piece inversion -- the suit totalled +0.01176 while nine shells sat
//      negative underneath it.
//   2. Signed volume is a winding test ONLY on a surface that is CLOSED and
//      ORIENTABLE. A glTF primitive is a MATERIAL SUBSET, not a shell: the
//      helmet's silver outer skin and black inner liner are one closed body
//      split across two materials, and each half alone integrates to a
//      meaningless number. So this test WELDS BY POSITION, decomposes into
//      connected shells, verifies closed+orientable via DIRECTED edges (the
//      R2c-6 census pattern), and only then takes the sign.
//
// Scope: the rider's own primitives. Machine prims are out of scope -- several
// are known one-sided by construction and that is a separate, older debt.
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "cgltf.h"  // implementation lives in test_asset_validator.cpp

namespace {

struct Shell {
    std::vector<std::size_t> tri;  // triangle indices into the prim's list
};

// A primitive welded by position: triangles over welded vertex ids.
struct Welded {
    std::vector<double> px, py, pz;          // welded vertex positions
    std::vector<std::size_t> a, b, c;        // triangle corners (welded ids)
};

double signed_volume(const Welded& w, const Shell& s) {
    double v = 0.0;
    for (std::size_t t : s.tri) {
        const std::size_t i = w.a[t], j = w.b[t], k = w.c[t];
        v += (w.px[i] * (w.py[j] * w.pz[k] - w.pz[j] * w.py[k])
              - w.py[i] * (w.px[j] * w.pz[k] - w.pz[j] * w.px[k])
              + w.pz[i] * (w.px[j] * w.py[k] - w.py[j] * w.px[k])) / 6.0;
    }
    return v;
}

// Closed AND consistently oriented, by DIRECTED edges: on such a surface every
// directed edge appears exactly once and its opposite supplies the second use
// of that undirected edge. A repeated directed edge = inconsistent winding; an
// unmatched one = a boundary. (Self-intersection is NOT detected -- stated
// rather than silently assumed.)
bool closed_and_orientable(const Welded& w, const Shell& s) {
    std::set<std::pair<std::size_t, std::size_t>> directed;
    std::map<std::pair<std::size_t, std::size_t>, int> undirected;
    for (std::size_t t : s.tri) {
        const std::size_t v[3] = {w.a[t], w.b[t], w.c[t]};
        for (int e = 0; e < 3; ++e) {
            const std::size_t u = v[e], x = v[(e + 1) % 3];
            if (!directed.insert({u, x}).second) return false;
            undirected[u < x ? std::make_pair(u, x) : std::make_pair(x, u)] += 1;
        }
    }
    for (const auto& kv : undirected)
        if (kv.second != 2) return false;
    return true;
}

std::vector<Shell> shells_of(const Welded& w) {
    std::map<std::size_t, std::vector<std::size_t>> vert_tris;
    for (std::size_t t = 0; t < w.a.size(); ++t) {
        vert_tris[w.a[t]].push_back(t);
        vert_tris[w.b[t]].push_back(t);
        vert_tris[w.c[t]].push_back(t);
    }
    std::vector<char> seen(w.a.size(), 0);
    std::vector<Shell> out;
    for (std::size_t t0 = 0; t0 < w.a.size(); ++t0) {
        if (seen[t0]) continue;
        Shell s;
        std::vector<std::size_t> stack{t0};
        seen[t0] = 1;
        while (!stack.empty()) {
            const std::size_t t = stack.back();
            stack.pop_back();
            s.tri.push_back(t);
            const std::size_t v[3] = {w.a[t], w.b[t], w.c[t]};
            for (int e = 0; e < 3; ++e)
                for (std::size_t nb : vert_tris[v[e]])
                    if (!seen[nb]) {
                        seen[nb] = 1;
                        stack.push_back(nb);
                    }
        }
        out.push_back(std::move(s));
    }
    return out;
}

// Quantised key so coincident corners of separate boxes weld together exactly
// as the authoring tools emitted them (1e-5 m, the same tolerance the python
// census uses).
long long qkey(double v) { return static_cast<long long>(std::llround(v * 1e5)); }

bool build_welded(const cgltf_primitive& p, Welded& w) {
    const cgltf_accessor* pos = nullptr;
    for (cgltf_size a = 0; a < p.attributes_count; ++a)
        if (p.attributes[a].type == cgltf_attribute_type_position)
            pos = p.attributes[a].data;
    if (pos == nullptr || p.indices == nullptr) return false;
    if (p.type != cgltf_primitive_type_triangles) return false;

    std::map<std::tuple<long long, long long, long long>, std::size_t> weld;
    std::vector<std::size_t> remap(pos->count);
    for (cgltf_size v = 0; v < pos->count; ++v) {
        cgltf_float t[3] = {0, 0, 0};
        cgltf_accessor_read_float(pos, v, t, 3);
        const auto key = std::make_tuple(qkey(t[0]), qkey(t[1]), qkey(t[2]));
        auto it = weld.find(key);
        if (it == weld.end()) {
            it = weld.emplace(key, w.px.size()).first;
            w.px.push_back(t[0]);
            w.py.push_back(t[1]);
            w.pz.push_back(t[2]);
        }
        remap[v] = it->second;
    }
    for (cgltf_size i = 0; i + 2 < p.indices->count; i += 3) {
        w.a.push_back(remap[cgltf_accessor_read_index(p.indices, i)]);
        w.b.push_back(remap[cgltf_accessor_read_index(p.indices, i + 1)]);
        w.c.push_back(remap[cgltf_accessor_read_index(p.indices, i + 2)]);
    }
    return !w.a.empty();
}

bool is_rider_material(const char* n) {
    if (n == nullptr) return false;
    const std::string s(n);
    return s.find("proxy_grey") != std::string::npos
           || s.find("mitt_leather") != std::string::npos
           || s.find("scarf_blue") != std::string::npos;
}

}  // namespace

TEST_CASE("every rider shell in the shipped GLB is closed and wound outward",
          "[rider][winding]") {
    const std::string path =
        std::string(SEADS_ASSET_DIR) + "/sled/indy650.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    REQUIRE(cgltf_parse_file(&opt, path.c_str(), &d) == cgltf_result_success);
    REQUIRE(cgltf_load_buffers(&opt, d, path.c_str()) == cgltf_result_success);

    int prims_checked = 0;
    int shells_checked = 0;
    for (cgltf_size m = 0; m < d->meshes_count; ++m) {
        const cgltf_mesh& mesh = d->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& prim = mesh.primitives[p];
            if (prim.material == nullptr) continue;
            if (!is_rider_material(prim.material->name)) continue;
            const std::string mat(prim.material->name);

            Welded w;
            if (!build_welded(prim, w)) continue;
            ++prims_checked;

            for (const Shell& s : shells_of(w)) {
                ++shells_checked;
                // Half 2 of the law first: the sign only means something on a
                // closed, consistently-wound surface.
                INFO("material " << mat << ", shell of " << s.tri.size()
                                 << " tris");
                REQUIRE(closed_and_orientable(w, s));
                // Half 1: per shell, never over the assembly.
                REQUIRE(signed_volume(w, s) > 0.0);
            }
        }
    }
    // Non-vacuity: this test has been green while measuring nothing before.
    REQUIRE(prims_checked >= 3);
    REQUIRE(shells_checked >= 30);
    cgltf_free(d);
}
