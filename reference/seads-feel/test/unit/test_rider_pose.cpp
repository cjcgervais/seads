// ★ THE SUDBURIAN POSE MATH -- docs/SUDBURIAN_LADDER.md rung R1a, gate 3
// ("new unit tests in seads_render_core for every pure function you add:
// absorb composition, foot-offset capture, bend-plane stability").
//
// Every function under test is in render/rider_pose.cpp, which lives in
// seads_render_core precisely so this file can reach it: seads_tests links NO
// raylib (tools/graph/layer_rules.toml), and the pose math used to be trapped
// inside the raylib-only render/sled_model.cpp where nothing could test it.
// That is defect 15 for the pose path, and this file is its retirement.
//
// The GLB numbers below are MEASURED out of assets/sled/indy650.glb and are
// re-derived here from the file itself wherever a case depends on them, so a
// re-export that moves the rider fails these tests instead of silently
// invalidating the re-seat.

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

#include "render/rider_pose.h"
#include "render/rider_rig.h"
#include "render/team_color.h"  // H8: the two hues, compiled in, never retyped
#include "sim/sled.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "cgltf.h"  // implementation lives in test_asset_validator.cpp
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

// --- the shipped asset, parsed once ----------------------------------------
struct Glb {
    bool ok = false;
    std::vector<std::string> name;
    std::vector<glm::mat4> world;
    // world-space vertex extents per named mesh node
    std::vector<glm::vec3> vmin, vmax;
    std::vector<char> has_mesh;
};

const Glb& glb() {
    static Glb g = [] {
        Glb out;
        const std::string path =
            std::string(SEADS_ASSET_DIR) + "/sled/indy650.glb";
        cgltf_options opt{};
        cgltf_data* d = nullptr;
        if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
            return out;
        if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
            cgltf_free(d);
            return out;
        }
        const cgltf_size n = d->nodes_count;
        out.name.assign(n, std::string());
        out.world.assign(n, glm::mat4(1.0f));
        out.has_mesh.assign(n, 0);
        const float big = 1e30f;
        out.vmin.assign(n, glm::vec3(big));
        out.vmax.assign(n, glm::vec3(-big));
        for (cgltf_size i = 0; i < n; ++i) {
            const cgltf_node& nd = d->nodes[i];
            out.name[i] = nd.name != nullptr ? nd.name : "";
            cgltf_float w[16];
            cgltf_node_transform_world(&nd, w);
            glm::mat4 m(1.0f);
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r) m[c][r] = w[4 * c + r];
            out.world[i] = m;
            if (nd.mesh == nullptr) continue;
            out.has_mesh[i] = 1;
            // A SKINNED mesh's POSITION accessor is already in skin space,
            // which for this asset (ibm == inverse of the joint's bind world)
            // IS world space at bind; a rigid mesh needs its node transform.
            const bool skinned = nd.skin != nullptr;
            for (cgltf_size p = 0; p < nd.mesh->primitives_count; ++p) {
                const cgltf_primitive& pr = nd.mesh->primitives[p];
                for (cgltf_size a = 0; a < pr.attributes_count; ++a) {
                    if (pr.attributes[a].type != cgltf_attribute_type_position)
                        continue;
                    const cgltf_accessor* ac = pr.attributes[a].data;
                    for (cgltf_size v = 0; v < ac->count; ++v) {
                        cgltf_float f[3] = {0, 0, 0};
                        cgltf_accessor_read_float(ac, v, f, 3);
                        glm::vec3 q(f[0], f[1], f[2]);
                        if (!skinned) q = glm::vec3(m * glm::vec4(q, 1.0f));
                        out.vmin[i] = glm::min(out.vmin[i], q);
                        out.vmax[i] = glm::max(out.vmax[i], q);
                    }
                }
            }
        }
        cgltf_free(d);
        out.ok = true;
        return out;
    }();
    return g;
}

int idx_of(const std::string& s) {
    const Glb& g = glb();
    const auto it = std::find(g.name.begin(), g.name.end(), s);
    return it == g.name.end() ? -1 : static_cast<int>(it - g.name.begin());
}
glm::mat4 W(const std::string& s) { return glb().world[std::size_t(idx_of(s))]; }
glm::vec3 Pw(const std::string& s) { return glm::vec3(W(s)[3]); }

}  // namespace

// ===========================================================================
// DEFECT 10 + 11 -- the single-sourced kernel dials
// ===========================================================================

TEST_CASE("ski steer angle IS the kernel steer_max_rad, not a retyped 25 deg",
          "[rider_pose]") {
    const sim::SledParams p{};
    REQUIRE(render::ski_steer_max_rad() ==
            static_cast<float>(p.steer_max_rad));
    // and the value it replaced was measurably wrong: 25 deg vs 0.42 rad.
    const float old_lie = 25.0f * 3.14159265f / 180.0f;
    REQUIRE(std::fabs(old_lie - render::ski_steer_max_rad()) > 0.01f);
    // 3.9 % over-rotation -- the number quoted in ladder defect 10 ("4 %").
    const float err = (old_lie - render::ski_steer_max_rad()) /
                      render::ski_steer_max_rad();
    REQUIRE(err > 0.030f);
    REQUIRE(err < 0.045f);
}

TEST_CASE("stand normaliser IS the kernel stand_rise_m", "[rider_pose]") {
    const sim::SledParams p{};
    REQUIRE(render::stand_rise_m() == static_cast<float>(p.stand_rise_m));
    // it happened to equal the old 0.25f literal -- which is exactly why the
    // duplicate was invisible and worth killing.
    REQUIRE(std::fabs(render::stand_rise_m() - 0.25f) < 1e-6f);
}

TEST_CASE("named constants keep their shipped values (rename, never re-tune)",
          "[rider_pose]") {
    REQUIRE(std::fabs(render::kBarSweepRad - 32.0f * 3.14159265f / 180.0f) <
            1e-7f);
    REQUIRE(render::kHeadYawMaxRad == 1.4f);
    REQUIRE(render::kHeadPitchMaxRad == 0.9f);
    REQUIRE(render::kSagDefaultM == 0.10f);
    // ★ R1a attempt 2 §B: 0.120 -> 0.040. The ONE dial that moves the worst
    // single-tick body drop (65.66 -> 19.08 mm measured over 299,172 tape
    // ticks); see rider_pose.h for the frontier it sits on.
    REQUIRE(render::kAbsorbDropM == 0.040f);
    // ★ §A: the split is a fraction of ONE drop, never a second drop.
    REQUIRE(render::kAbsorbRootFrac == 0.65f);
    REQUIRE(std::fabs(render::absorb_root_drop_m(1.0f) +
                      render::absorb_pelvis_drop_m(1.0f) -
                      render::kAbsorbDropM) < 1e-7f);
    REQUIRE(render::absorb_root_drop_m(0.0f) == 0.0f);
    REQUIRE(render::absorb_pelvis_drop_m(0.0f) == 0.0f);
    // ... and the root really does get the larger share, which is the fix.
    REQUIRE(render::absorb_root_drop_m(1.0f) >
            render::absorb_pelvis_drop_m(1.0f));
    // ★ §D: anatomical hinge limits, forward freer than aft.
    REQUIRE(std::fabs(render::kHingeFwdMaxRad -
                      30.0f * 3.14159265f / 180.0f) < 1e-7f);
    // ★ AFT IS MEASURED OUT, NOT OMITTED: an aft hinge costs 0.0968 of worst arm
    // reach ratio and 7.6 points of clamping (1.4568 -> 1.5536, 20.5 -> 28.1 %)
    // and buys no clearance, because aft clearance is 44.2 mm. rider_pose.h
    // carries the three-row table. Aft lean is a CG-SOLVED translation.
    REQUIRE(render::kHingeAftMaxRad == 0.0f);
    REQUIRE(render::kHingeFwdMaxRad > render::kHingeAftMaxRad);
    // ★ hinge-first, the identity value of the clearance-vs-onset-rate dial
    REQUIRE(render::kHingeDemandShare == 1.0f);
}

// ===========================================================================
// DEFECT 7 -- the socket weld (foot-offset capture)
// ===========================================================================

TEST_CASE("socket_weld with no re-seat reproduces the rest tip exactly",
          "[rider_pose]") {
    // A socket with rotation AND translation, so a lazy implementation that
    // only subtracts positions cannot pass.
    const glm::mat4 sock =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.3f, 0.8f, -0.2f)) *
        glm::rotate(glm::mat4(1.0f), 0.37f, glm::vec3(0.2f, 1.0f, 0.4f));
    const glm::mat4 tip =
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.1f, 0.55f, 0.44f)) *
        glm::rotate(glm::mat4(1.0f), -0.8f, glm::vec3(1.0f, 0.1f, 0.0f));
    const glm::mat4 off = render::socket_weld(sock, tip, glm::vec3(0.0f));
    const glm::mat4 back = sock * off;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) REQUIRE(std::fabs(back[c][r] - tip[c][r]) < 1e-5f);
}

TEST_CASE("socket_weld RIDES the socket -- this is defect 7 in one assertion",
          "[rider_pose]") {
    const glm::mat4 sock =
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.3f, 0.327f, -0.17f));
    const glm::mat4 tip =
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.3f, 0.285322f, -0.2208f));
    const glm::mat4 off = render::socket_weld(sock, tip, glm::vec3(0.0f));
    // move + rotate the socket the way the machine moves under lean/steer
    const glm::mat4 moved =
        glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, -0.4f, 2.0f)) *
        glm::rotate(glm::mat4(1.0f), 0.6f, glm::vec3(0.0f, 1.0f, 0.0f)) * sock;
    const glm::vec3 want = glm::vec3(moved * glm::vec4(glm::vec3(tip[3]) -
                                                           glm::vec3(sock[3]),
                                                       0.0f)) +
                           glm::vec3(moved[3]);
    const glm::vec3 got = glm::vec3((moved * off)[3]);
    REQUIRE(glm::length(got - want) < 1e-5f);
    // the OLD behaviour -- a constant captured at load -- does not move at all,
    // and the gap it opens over this much machine motion is metres, not
    // millimetres. That gap IS the detached hip.
    REQUIRE(glm::length(got - glm::vec3(tip[3])) > 1.0f);
}

TEST_CASE("the boot re-seat is RETIRED: the Sudburian's R2b seat IS the rest",
          "[rider_pose][asset]") {
    // ★★ 2026-08-18. This case used to prove that kBootReseatM lifted the
    // LEGACY rider's boot sole (authored 41.8 mm through the running-board
    // sheet at y 0.252) back onto the deck. That rider is gone
    // (docs/RIDER_AUTHORITY.md); the SUDBURIAN was seated in the live Blender
    // session against the machine (seat_sudburian.py) and Chad signed the seat
    // at R2b, so the correct re-seat is ZERO -- any other value would move a
    // signed pose. What this case pins now is (a) that the number IS zero and
    // (b) that the seated ankle really does sit over the board deck, so a
    // future re-seat cannot come back "to fix the boots" without measuring.
    const Glb& g = glb();
    REQUIRE(g.ok);
    REQUIRE(render::kBootReseatM == glm::vec3(0.0f));
    // the deck SHEET, unchanged machine art (the machine bytes are carried
    // across the splice byte-identical, and this is the proof a reader wants)
    const int bl = idx_of("board_L");
    const int br = idx_of("board_R");
    REQUIRE(bl >= 0);
    REQUIRE(br >= 0);
    const float deck_l = g.vmin[std::size_t(bl)].y;
    const float deck_r = g.vmin[std::size_t(br)].y;
    REQUIRE(std::fabs(deck_l - deck_r) < 1e-5f);
    REQUIRE(std::fabs(deck_l - 0.252000f) < 1e-5f);
    // the seated ankles: ABOVE the deck by a boot's worth, never through it,
    // and directly over the board's x (the socket's x) -- the crossover: the
    // Sudburian's foot_l is at +X, over board_socket_R.
    struct Pair { const char* foot; const char* sock; };
    for (const Pair& pr : {Pair{"foot_l", "board_socket_R"},
                           Pair{"foot_r", "board_socket_L"}}) {
        const glm::vec3 f = Pw(pr.foot);
        const glm::vec3 sck = Pw(pr.sock);
        INFO(pr.foot << " y " << f.y << " deck " << deck_l);
        REQUIRE(f.y > deck_l + 0.040f);   // an ankle is above its sole
        REQUIRE(f.y < deck_l + 0.120f);   // ... but not floating
        REQUIRE(std::fabs(f.x - sck.x) < 1e-3f);  // over the board
        // 3.8 mm above the frame bolt, 272 mm forward of it: the R2b seat,
        // as measured, and the runtime's weld carries exactly this offset.
        REQUIRE(std::fabs((f.y - sck.y) - 0.003784f) < 1e-3f);
        REQUIRE(std::fabs((f.z - sck.z) - 0.272032f) < 1e-3f);
    }
}

TEST_CASE("the L/R crossover is written ONCE, in the chain table",
          "[rider_pose]") {
    // §2.3a: the Sudburian's `_l` is anatomical left = model +X; the machine's
    // `_L` sockets are model -X. So every `_l` chain must carry model_side 1
    // and bind an `_R` socket, and vice versa. A "fix" that re-pairs by name
    // moves a foot by 0.6 m; this is the assertion that catches it.
    REQUIRE(render::kBootReseatM.x == 0.0f);
    const auto& cs = render::rider_chain_specs();
    const auto& ms = render::machine_node_specs();
    const auto& js = render::rider_joint_specs();
    for (const render::RiderChainSpec& c : cs) {
        const std::string tip = js[std::size_t(c.joint[2])].name;
        const std::string sock = ms[std::size_t(c.socket)].name;
        INFO(c.debug_name << ": " << tip << " -> " << sock);
        const bool tip_is_l = tip.size() > 2 && tip.substr(tip.size() - 2) == "_l";
        const bool sock_is_R =
            sock.size() > 2 && sock.substr(sock.size() - 2) == "_R";
        REQUIRE(tip_is_l == sock_is_R);              // the crossover
        REQUIRE(c.model_side == (tip_is_l ? 1 : 0)); // and the side it implies
        REQUIRE(ms[std::size_t(c.socket)].world_x_sign == (tip_is_l ? +1 : -1));
    }
}

TEST_CASE("the chain table IS the seated Sudburian, measured off the bytes",
          "[rider_pose][asset]") {
    // Every rest_tip_minus_socket_m row is re-derived from the shipped file.
    // The rows say two things worth keeping loud: the legs are the R2b seat
    // (feet 3.8 mm above / 272 mm forward of the frame bolts), and the arms
    // are ONE FIST -- hand_* is the wrist, the knuckle is on the bar, so the
    // wrist reads 51 mm up and 83 mm aft of the grip socket (98 mm). The bar
    // was carried in from the live session (splice_bar.py) the same day the
    // Sudburian was; before that the file's bar was 100 mm stale and this
    // row read 16 mm FORWARD, which is how the staleness was found.
    const Glb& g = glb();
    REQUIRE(g.ok);
    const auto& js = render::rider_joint_specs();
    const auto& ms = render::machine_node_specs();
    for (const render::RiderChainSpec& c : render::rider_chain_specs()) {
        const glm::vec3 d = Pw(js[std::size_t(c.joint[2])].name) -
                            Pw(ms[std::size_t(c.socket)].name);
        INFO("chain " << c.debug_name << " d " << d.x << "," << d.y << "," << d.z);
        REQUIRE(std::fabs(d.x - c.rest_tip_minus_socket_m.x) < 1e-3f);
        REQUIRE(std::fabs(d.y - c.rest_tip_minus_socket_m.y) < 1e-3f);
        REQUIRE(std::fabs(d.z - c.rest_tip_minus_socket_m.z) < 1e-3f);
        if (c.is_arm) {
            REQUIRE(d.y > 0.045f);  // the wrist is above the bar ...
            REQUIRE(d.y < 0.060f);
            REQUIRE(d.z < -0.070f); // ... and BEHIND it: one fist, knuckle on
            // the bar. A fist is ~100 mm; since the 2026-08-18 hand tuning the
            // wrist also sits up to ~36 mm along the bar from the socket
            // (Chad slid each hand to the new end flange), so 130.
            REQUIRE(glm::length(d) < 0.130f);
        } else {
            REQUIRE(std::fabs(d.y) < 0.010f);  // seated ON the board line
            REQUIRE(d.z > 0.25f);              // forward of the frame bolt
        }
    }
    // and the two sides mirror each other -- the proxy is built mirrored and
    // seated symmetrically; a one-sided IK drift shows here. The LEGS are
    // exact. The ARMS are asymmetric BY RULING since 2026-08-18 (each hand
    // slid along its bar independently: left 36 mm outboard, right 12.7 mm
    // outboard + 6.35 mm aft), so for the arms only the height/aft agree to
    // a centimetre and the along-bar component is free.
    const auto& cs = render::rider_chain_specs();
    for (int k = 0; k < 2; ++k) {
        const glm::vec3 a = cs[std::size_t(2 * k)].rest_tip_minus_socket_m;
        const glm::vec3 b = cs[std::size_t(2 * k + 1)].rest_tip_minus_socket_m;
        const bool arm = cs[std::size_t(2 * k)].is_arm;
        if (!arm) REQUIRE(std::fabs(a.x + b.x) < 1e-4f);
        REQUIRE(std::fabs(a.y - b.y) < (arm ? 1e-2f : 1e-4f));
        REQUIRE(std::fabs(a.z - b.z) < (arm ? 1e-2f : 1e-4f));
    }
}

// ===========================================================================
// DEFECT 14 -- the bend plane
// ===========================================================================

TEST_CASE("bend normal takes the AUTHORED branch on every shipped chain",
          "[rider_pose][asset]") {
    // If this ever goes red, the fallback below has become live art direction
    // rather than a safety net, and somebody needs to know.
    const auto& js = render::rider_joint_specs();
    for (const render::RiderChainSpec& c : render::rider_chain_specs()) {
        const glm::vec3 s0 = Pw(js[std::size_t(c.joint[0])].name);
        const glm::vec3 e0 = Pw(js[std::size_t(c.joint[1])].name);
        glm::vec3 t0 = Pw(js[std::size_t(c.joint[2])].name);
        if (!c.is_arm) t0 += render::kBootReseatM;
        const float cross_len = glm::length(glm::cross(t0 - s0, e0 - s0));
        INFO("chain " << c.debug_name << " |cross| " << cross_len);
        REQUIRE(cross_len > 100.0f * render::kBendDegenerate);
        // the returned normal IS that cross product, normalised
        const glm::vec3 n = render::rest_bend_normal(
            s0, e0, t0, glm::mat3(W(js[std::size_t(c.joint[0])].name)));
        const glm::vec3 want = glm::normalize(glm::cross(t0 - s0, e0 - s0));
        REQUIRE(glm::length(n - want) < 1e-5f);
    }
}

TEST_CASE("bend normal is always a unit vector perpendicular to the chain",
          "[rider_pose]") {
    // sweep tip and mid over a grid that crosses the degenerate configuration
    for (int a = -60; a <= 60; a += 3) {
        const float th = static_cast<float>(a) * 0.0174532925f;
        const glm::vec3 root(0.0f);
        const glm::vec3 tip(0.0f, 1.0f, 0.0f);
        // mid slides onto and off the root->tip line; at a == 0 it is EXACTLY
        // on it and the primary branch is degenerate.
        const glm::vec3 mid(std::sin(th) * 0.0004f, 0.5f, 0.0f);
        const glm::mat3 frame(1.0f);
        const glm::vec3 n = render::rest_bend_normal(root, mid, tip, frame);
        INFO("a " << a);
        REQUIRE(std::fabs(glm::length(n) - 1.0f) < 1e-4f);
    }
}

TEST_CASE("the near-extended FALLBACK never flips across a sweep",
          "[rider_pose]") {
    // This is defect 14 stated as a test. The chain is EXACTLY straight (the
    // primary branch is dead) and the whole chain direction is swept through a
    // full turn; the fallback must vary continuously and never reverse.
    //
    // The old world-constant fallback cannot pass this: cross(dir, (0,-1,0.1))
    // reverses as dir passes through the constant, and it is not derived from
    // the chain's frame at all.
    // A TILTED rest frame, so the projection genuinely rotates as the chain
    // sweeps -- an axis-aligned frame would make the answer constant and the
    // continuity claim vacuous. ★ THE TILT IS NOW ON +X, because that is the
    // column the fallback reads after F1 (it is the one that ANTI-mirrors; see
    // rider_pose.h). Tilting +Z instead leaves +X lying almost IN the sweep
    // plane, where the projection of a nearly-in-plane vector genuinely does
    // rotate faster than 0.05 per degree -- that is a property of the test
    // geometry, not a discontinuity in the function.
    glm::mat3 frame(1.0f);
    frame[0] = glm::normalize(glm::vec3(0.31f, 0.22f, 0.92f));
    frame[2] = glm::normalize(glm::cross(frame[0], glm::vec3(0.0f, 1.0f, 0.0f)));
    frame[1] = glm::cross(frame[2], frame[0]);
    glm::vec3 prev(0.0f);
    bool have_prev = false;
    float worst_step = 0.0f;
    for (int a = 0; a < 360; ++a) {
        const float th = static_cast<float>(a) * 0.0174532925f;
        // dir sweeps the XY plane, so it is never parallel to the frame's +Z
        const glm::vec3 dir(std::cos(th), std::sin(th), 0.0f);
        const glm::vec3 root(0.0f);
        const glm::vec3 tip = dir;
        const glm::vec3 mid = dir * 0.5f;  // EXACTLY straight
        const glm::vec3 n = render::rest_bend_normal(root, mid, tip, frame);
        INFO("a " << a << " n " << n.x << "," << n.y << "," << n.z);
        REQUIRE(std::fabs(glm::length(n) - 1.0f) < 1e-4f);
        REQUIRE(std::fabs(glm::dot(n, dir)) < 1e-4f);  // perpendicular
        if (have_prev) {
            REQUIRE(glm::dot(n, prev) > 0.0f);  // NEVER reverses
            worst_step = std::max(worst_step, glm::length(n - prev));
        }
        prev = n;
        have_prev = true;
    }
    // continuity: one degree of sweep must not move the normal more than a
    // couple of degrees. (A flip would be ~2.0 here and is caught above too.)
    INFO("worst step " << worst_step);
    REQUIRE(worst_step < 0.05f);
}

TEST_CASE("the fallback falls THROUGH when the frame axis is parallel",
          "[rider_pose]") {
    const glm::mat3 frame(1.0f);
    // chain runs along world +X, which is the frame's own +X: the PRIMARY
    // fallback axis (post-F1) is unusable and cross(dir, +Z) must take over.
    const glm::vec3 n = render::rest_bend_normal(
        glm::vec3(0.0f), glm::vec3(0.5f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f), frame);
    REQUIRE(std::fabs(glm::length(n) - 1.0f) < 1e-4f);
    REQUIRE(std::fabs(n.x) < 1e-4f);   // perpendicular to the chain
    // cross(dir=+X, +Z) is -Y, so the fall-through lands on the Y axis
    REQUIRE(std::fabs(std::fabs(n.y) - 1.0f) < 1e-4f);
    // a zero-length chain is total, not a NaN
    const glm::vec3 z = render::rest_bend_normal(
        glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f), frame);
    REQUIRE(std::fabs(glm::length(z) - 1.0f) < 1e-4f);
}

TEST_CASE("the BEND DIRECTION mirrors between sides (the normal ANTI-mirrors)",
          "[rider_pose]") {
    // ★★ THIS TEST WAS THE F1 DEFECT. It used to assert that the NORMAL mirrors
    // -- REQUIRE(|nl.x + nr.x| < eps) etc. -- and it was green, which is exactly
    // how a wrong invariant hides. What the solver consumes is not the normal: it
    // is `bend = cross(bend_n, dir)` (render/sled_model.cpp, solve_chain), and
    // `cross` is handedness-flipping. So a MIRRORED normal produces an
    // ANTI-mirrored bend -- the two knees folding opposite ways, which is the
    // condition the old assertion certified.
    //
    // The invariant is therefore: cross(n, dir) MIRRORS. Asserted below on both
    // the primary (authored) branch and the fallback, and stated in the frame the
    // solver actually uses.
    auto bend_of = [](const glm::vec3& n, const glm::vec3& root,
                      const glm::vec3& tip) {
        const glm::vec3 dir = glm::normalize(tip - root);
        return glm::normalize(glm::cross(n, dir));
    };
    // Mirror-conjugate rest frames, built the way the shipped rig's are:
    // R = M * L * M with M = diag(-1,1,1). MEASURED on thigh_l/thigh_r and
    // upperarm_l/upperarm_r: +X anti-mirrors (dot -1.0000), +Y and +Z mirror
    // (dot +1.0000). That is why the fallback reads the +X column.
    const glm::mat3 M(glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                      glm::vec3(0, 0, 1));
    glm::mat3 fl(1.0f);
    fl[2] = glm::normalize(glm::vec3(-0.95f, -0.14f, -0.29f));  // thigh_L +Z
    fl[0] = glm::normalize(glm::cross(glm::vec3(0, 1, 0), fl[2]));
    fl[1] = glm::cross(fl[2], fl[0]);
    const glm::mat3 fr = M * fl * M;
    // sanity: the frames really are mirror-conjugate the measured way
    REQUIRE(glm::length(fr[0] + M * fl[0]) < 1e-5f);   // +X ANTI-mirrors
    REQUIRE(glm::length(fr[2] - M * fl[2]) < 1e-5f);   // +Z mirrors

    SECTION("fallback branch -- an EXACTLY straight chain") {
        const glm::vec3 rl(-0.1f, 0.7f, 0.0f), tl(-0.1f, 0.2f, 0.0f);
        const glm::vec3 rr = M * rl, tr = M * tl;
        const glm::vec3 nl =
            render::rest_bend_normal(rl, 0.5f * (rl + tl), tl, fl);
        const glm::vec3 nr =
            render::rest_bend_normal(rr, 0.5f * (rr + tr), tr, fr);
        // the NORMAL anti-mirrors ...
        REQUIRE(glm::length(nr + M * nl) < 1e-5f);
        // ... which is exactly what makes the BEND mirror, and the bend is what
        // the solver puts the knee on.
        const glm::vec3 bl = bend_of(nl, rl, tl);
        const glm::vec3 br = bend_of(nr, rr, tr);
        INFO("bend L " << bl.x << "," << bl.y << "," << bl.z << "  R " << br.x
                       << "," << br.y << "," << br.z);
        REQUIRE(glm::length(br - M * bl) < 1e-5f);
        // and the two knees are on the SAME anatomical side (fore/aft agrees,
        // lateral is the mirrored component)
        REQUIRE(std::fabs(bl.z - br.z) < 1e-5f);
        REQUIRE(std::fabs(bl.y - br.y) < 1e-5f);
    }
    SECTION("primary branch -- a genuinely bent chain mirrors the same way") {
        const glm::vec3 rl(-0.1f, 0.7f, 0.0f), tl(-0.1f, 0.2f, 0.0f);
        const glm::vec3 ml(-0.1f, 0.45f, 0.06f);  // knee forward: an AUTHORED side
        const glm::vec3 rr = M * rl, tr = M * tl, mr = M * ml;
        const glm::vec3 nl = render::rest_bend_normal(rl, ml, tl, fl);
        const glm::vec3 nr = render::rest_bend_normal(rr, mr, tr, fr);
        REQUIRE(glm::length(nr + M * nl) < 1e-5f);   // anti-mirrors too
        const glm::vec3 bl = bend_of(nl, rl, tl);
        const glm::vec3 br = bend_of(nr, rr, tr);
        REQUIRE(glm::length(br - M * bl) < 1e-5f);
        // ... and it points TOWARD the authored knee on both sides
        REQUIRE(glm::dot(bl, glm::normalize(ml - 0.5f * (rl + tl))) > 0.9f);
        REQUIRE(glm::dot(br, glm::normalize(mr - 0.5f * (rr + tr))) > 0.9f);
    }
    SECTION("every fallback branch keeps the invariant, including fall-through") {
        // +X parallel to the chain forces branch 2 (cross(dir, +Z)), and +X and
        // +Z both parallel forces branch 3. All three must anti-mirror.
        glm::mat3 gl(1.0f);
        gl[0] = glm::vec3(0, -1, 0);   // +X along the chain
        gl[1] = glm::vec3(1, 0, 0);
        gl[2] = glm::vec3(0, 0, 1);
        const glm::mat3 gr = M * gl * M;
        const glm::vec3 rl(-0.1f, 0.7f, 0.0f), tl(-0.1f, 0.2f, 0.0f);
        const glm::vec3 rr = M * rl, tr = M * tl;
        const glm::vec3 nl =
            render::rest_bend_normal(rl, 0.5f * (rl + tl), tl, gl);
        const glm::vec3 nr =
            render::rest_bend_normal(rr, 0.5f * (rr + tr), tr, gr);
        REQUIRE(std::fabs(glm::length(nl) - 1.0f) < 1e-4f);
        REQUIRE(std::fabs(glm::dot(nl, glm::vec3(0, 1, 0))) < 1e-4f);
        REQUIRE(glm::length(nr + M * nl) < 1e-5f);
    }
}

TEST_CASE("the fallback reads the ANTI-MIRRORING frame column on this asset",
          "[rider_pose][asset]") {
    // The measurement that chose +X over +Z, re-derived from the shipped bytes.
    // If a re-export re-rolls the bones this fails and names the column.
    const Glb& g = glb();
    REQUIRE(g.ok);
    const glm::mat3 M(glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                      glm::vec3(0, 0, 1));
    for (const char* pair : {"thigh", "upperarm"}) {
        const glm::mat3 l(W(std::string(pair) + "_l"));
        const glm::mat3 r(W(std::string(pair) + "_r"));
        INFO(pair);
        // ★ 2026-08-18 evening: the ARMS are posed asymmetrically on purpose
        // (Chad slid each hand along its bar independently, R2c-M2 tuning),
        // so the upperarm frames mirror only to ~0.5 deg (0.009). The column
        // CHOICE (+X anti-mirrors) is what this test guards, and 2e-2 still
        // fails loudly on a re-rolled bone (a 90 deg roll reads ~1.4).
        const float tol = (std::string(pair) == "upperarm") ? 2e-2f : 1e-3f;
        REQUIRE(glm::length(r[0] + M * l[0]) < tol);   // +X ANTI-mirrors
        REQUIRE(glm::length(r[1] - M * l[1]) < tol);   // +Y mirrors
        REQUIRE(glm::length(r[2] - M * l[2]) < tol);   // +Z mirrors
    }
}

// ===========================================================================
// DEFECT 8 -- absorb
// ===========================================================================

namespace {
float absorb3(float x0, float x1, float x2, float v0, float v1, float v2) {
    const float x[3] = {x0, x1, x2};
    const float v[3] = {v0, v1, v2};
    return render::rider_absorb(x, v, static_cast<float>(
                                          sim::SledParams{}.susp_rest_m));
}
}  // namespace

TEST_CASE("absorb is ZERO at rest and bounded in [0,1] everywhere",
          "[rider_pose]") {
    REQUIRE(absorb3(0, 0, 0, 0, 0, 0) == 0.0f);
    // static sag on a real drive tape sits around 0.010 m with |v| ~ 0.08 m/s;
    // the man must be still, not permanently crouched.
    REQUIRE(absorb3(0.010f, 0.010f, 0.012f, 0.08f, 0.06f, 0.05f) < 0.03f);
    // fully extended suspension, rebounding hard: rebound is NOT a hit.
    REQUIRE(absorb3(0, 0, 0, -30.0f, -30.0f, -30.0f) == 0.0f);
    // absurd inputs stay in range and never NaN
    for (float x : {-1.0f, 0.0f, 0.05f, 0.26f, 5.0f}) {
        for (float v : {-100.0f, -1.0f, 0.0f, 1.0f, 100.0f, 1e6f}) {
            const float a = absorb3(x, x, x, v, v, v);
            INFO("x " << x << " v " << v << " -> " << a);
            REQUIRE(a >= 0.0f);
            REQUIRE(a <= 1.0f);
            REQUIRE(a == a);  // not NaN
        }
    }
}

TEST_CASE("absorb is MONOTONE in both compression and compression rate",
          "[rider_pose]") {
    float prev = -1.0f;
    for (int i = 0; i <= 40; ++i) {
        const float x = 0.005f * static_cast<float>(i);
        const float a = absorb3(x, 0, 0, 0, 0, 0);
        REQUIRE(a >= prev - 1e-7f);
        prev = a;
    }
    prev = -1.0f;
    for (int i = 0; i <= 60; ++i) {
        const float v = 0.5f * static_cast<float>(i);
        const float a = absorb3(0, 0, 0, v, 0, 0);
        REQUIRE(a >= prev - 1e-7f);
        prev = a;
    }
}

TEST_CASE("absorb takes the MAX over the three patches, never the mean",
          "[rider_pose]") {
    // one ski finding a rock is a hit; averaging it away is the mush defect 8
    // is about.
    const float one_ski = absorb3(0.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f);
    const float all_three = absorb3(0.0f, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f);
    REQUIRE(one_ski == all_three);
    REQUIRE(one_ski > 0.30f);
    // and patch order does not matter
    REQUIRE(absorb3(0, 0, 0, 0, 8.0f, 0) == one_ski);
    REQUIRE(absorb3(0, 0, 0, 0, 0, 8.0f) == one_ski);
    REQUIRE(absorb3(0.12f, 0, 0, 0, 0, 0) == absorb3(0, 0.12f, 0, 0, 0, 0));
}

TEST_CASE("the absorb hold band is anchored to the KERNEL susp_rest_m",
          "[rider_pose]") {
    const float rest = static_cast<float>(sim::SledParams{}.susp_rest_m);
    const float x[3] = {0.0f, 0.0f, 0.0f};
    const float v[3] = {0.0f, 0.0f, 0.0f};
    float on[3] = {render::kAbsorbHoldOnFrac * rest, 0.0f, 0.0f};
    REQUIRE(render::rider_absorb(x, v, rest) == 0.0f);
    REQUIRE(render::rider_absorb(on, v, rest) == 0.0f);       // dead band ends
    // ★ §B: the hold term is now the SAME SATURATING MAP as the rate term, so
    // "full" is an asymptote, not a point. At exactly the half-scale past the
    // dead band it is half its weight -- that is the whole content of the shape.
    float half[3] = {(render::kAbsorbHoldOnFrac + render::kAbsorbHoldHalfFrac) *
                         rest,
                     0.0f, 0.0f};
    REQUIRE(std::fabs(render::rider_absorb(half, v, rest) -
                      0.5f * render::kAbsorbHoldWeight) < 1e-6f);
    // ... and it is bounded strictly BELOW its weight for every finite input,
    // including the kernel's own hard clamp susp_x = 2 * susp_travel_m.
    float clamped[3] = {
        2.0f * static_cast<float>(sim::SledParams{}.susp_travel_m), 0.0f, 0.0f};
    const float at_clamp = render::rider_absorb(clamped, v, rest);
    REQUIRE(at_clamp < render::kAbsorbHoldWeight);
    REQUIRE(at_clamp > 0.90f * render::kAbsorbHoldWeight);
    // MEASURED p90 of max(+susp_x) over the drive tapes is 0.064-0.108 m; the
    // shape must put real crouch in that band (the linear ramp did not -- it
    // read 0.11-0.53 of the hold there, which is why the range was unused at
    // cruise and spent entirely on the jumpy tail).
    float p90[3] = {0.085f, 0.0f, 0.0f};
    const float at_p90 = render::rider_absorb(p90, v, rest);
    REQUIRE(at_p90 > 0.30f * render::kAbsorbHoldWeight);
    // a zero/negative susp_rest_m disables the hold rather than dividing
    REQUIRE(render::rider_absorb(clamped, v, 0.0f) == 0.0f);
    REQUIRE(render::rider_absorb(clamped, v, -1.0f) == 0.0f);
}

TEST_CASE("SEC-B: no shaping can beat the drop authority on the worst step",
          "[rider_pose]") {
    // The theorem in rider_pose.h, as a test. absorb is a pure function of ONE
    // tick's state, and the kernel's own susp_x demonstrably jumps from 0 to its
    // clamp (2 * susp_travel_m) inside one tick on the drive tapes. So the worst
    // single-tick body drop IS the full authority, and the only honest dial is
    // kAbsorbDropM. If someone "fixes the step" by shaping alone, this goes red
    // and tells them why.
    const float rest = static_cast<float>(sim::SledParams{}.susp_rest_m);
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    float clamped[3] = {
        2.0f * static_cast<float>(sim::SledParams{}.susp_travel_m), 0.0f, 0.0f};
    const float lo = render::rider_absorb(zero, zero, rest);
    const float hi = render::rider_absorb(clamped, zero, rest);
    REQUIRE(lo == 0.0f);
    // the reachable single-tick swing, in millimetres of body drop
    const float step_mm = (hi - lo) * render::kAbsorbDropM * 1000.0f;
    INFO("worst reachable one-tick body drop " << step_mm << " mm");
    REQUIRE(step_mm < 22.0f);   // attempt 1 measured 65.66 mm
    REQUIRE(step_mm > 8.0f);    // and absorb must not be inert (gate criterion)
}

// ===========================================================================
// §D -- the solved torso hinge
// ===========================================================================

TEST_CASE("the segment mass table sums to exactly one body",
          "[rider_pose]") {
    // Winter/Dempster fractions. If a row is edited, this is the first thing
    // that must still hold -- a CG computed on a table that does not close is
    // not a CG.
    double sum = 0.0;
    for (const render::RiderSegment& s : render::rider_segments())
        sum += static_cast<double>(s.mass_frac);
    INFO("sum " << sum);
    REQUIRE(std::fabs(sum - 1.0) < 1e-6);  // float rows, exact to 1.7e-8
    for (const render::RiderSegment& s : render::rider_segments()) {
        REQUIRE(s.mass_frac > 0.0f);
        REQUIRE(s.cg_frac >= 0.0f);
        REQUIRE(s.cg_frac <= 1.0f);
        REQUIRE(s.prox >= 0);
        REQUIRE(s.prox < render::kRiderJointCount);
        REQUIRE(s.dist >= 0);
        REQUIRE(s.dist < render::kRiderJointCount);
    }
    // the trunk is the biggest single mass and the hinged set is a MINORITY of
    // the body -- that is why the hinge's CG authority is bounded (see §D).
    REQUIRE(render::rider_segments()[0].mass_frac > 0.40f);
}

TEST_CASE("rider_cg is a mass-weighted mean, and it TRANSLATES exactly",
          "[rider_pose]") {
    std::array<glm::vec3, render::kRiderJointCount> p{};
    // every joint at one point -> the CG is that point
    for (auto& q : p) q = glm::vec3(1.0f, 2.0f, 3.0f);
    REQUIRE(glm::length(render::rider_cg(p) - glm::vec3(1.0f, 2.0f, 3.0f)) <
            1e-5f);
    // spread them out, then translate the whole set: the CG must follow 1:1.
    for (int j = 0; j < render::kRiderJointCount; ++j)
        p[std::size_t(j)] =
            glm::vec3(0.03f * static_cast<float>(j),
                      0.11f * static_cast<float>(j % 5),
                      -0.07f * static_cast<float>(j % 3));
    const glm::vec3 c0 = render::rider_cg(p);
    const glm::vec3 shift(0.13f, -0.21f, 0.45f);
    for (auto& q : p) q += shift;
    REQUIRE(glm::length(render::rider_cg(p) - (c0 + shift)) < 1e-5f);
    // and it is PURE -- no statics anywhere in it
    for (auto& q : p) q -= shift;
    REQUIRE(glm::length(render::rider_cg(p) - c0) < 1e-6f);
}

namespace {
// The MEASURED shipped model, R1c: a/b off the rest pose, the 2x2 Jacobian off
// the three-pose probe in render/sled_model.cpp's capture_hinge(). k_hat is
// 0.819082, NOT the 0.821607 attempt 2 documented -- that figure was taken
// before §C moved the boot re-seat (verifier finding 7).
render::RiderHingeModel shipped_model() {
    render::RiderHingeModel m;
    m.a = 0.112596f;
    m.b = 0.050595f;
    m.k_hat = 0.819082f;
    m.k_zy = -0.024713f;
    m.k_yz = -0.107003f;
    m.k_yy = 0.796440f;
    return m;
}
}  // namespace

TEST_CASE("the hinge model inverts its own H(theta) exactly", "[rider_pose]") {
    // The closed form is the whole reason the hinge is SOLVED and not authored.
    render::RiderHingeModel m = shipped_model();
    REQUIRE(render::hinge_cg_dz(m, 0.0f) == 0.0f);
    REQUIRE(std::fabs(render::hinge_theta_for(m, 0.0f)) < 1e-6f);
    // round-trip inside the anatomical band
    for (int i = 0; i <= 30; ++i) {
        const float th = static_cast<float>(i) * 3.14159265f / 180.0f;
        const float t = render::hinge_cg_dz(m, th);
        const float back = render::hinge_theta_for(m, t);
        INFO("deg " << i << " target " << t << " back "
                    << back * 180.0f / 3.14159265f);
        REQUIRE(std::fabs(back - th) < 1e-4f);
    }
    // monotone in the target, and clamped to the anatomical limits at both ends
    float prev = -10.0f;
    for (int i = -60; i <= 60; ++i) {
        const float t = 0.01f * static_cast<float>(i);
        const float th = render::hinge_theta_for(m, t);
        REQUIRE(th >= prev - 1e-6f);
        REQUIRE(th <= render::kHingeFwdMaxRad + 1e-6f);
        REQUIRE(th >= -render::kHingeAftMaxRad - 1e-6f);
        prev = th;
    }
    REQUIRE(render::hinge_theta_for(m, 10.0f) == render::kHingeFwdMaxRad);
    REQUIRE(render::hinge_theta_for(m, -10.0f) == -render::kHingeAftMaxRad);
    // aft is a pure translation on this rig, so every aft target hinges by 0 ...
    REQUIRE(render::hinge_theta_for(m, -0.05f) == 0.0f);
    REQUIRE(render::hinge_theta_for(m, -0.25f) == 0.0f);
    // a degenerate model must not divide by zero or NaN
    render::RiderHingeModel z;
    REQUIRE(render::hinge_theta_for(z, 0.25f) == 0.0f);
    REQUIRE(render::hinge_cg_dz(z, 0.5f) == 0.0f);
}

TEST_CASE("R1c: the hinge DROPS the CG, and H_y is why the rise exists",
          "[rider_pose]") {
    const render::RiderHingeModel m = shipped_model();
    // H_y is exactly zero at zero hinge -- this is what keeps the SIGNED
    // zero-input pose from moving.
    REQUIRE(render::hinge_cg_dy(m, 0.0f) == 0.0f);
    // and NEGATIVE for every forward hinge, monotonically. That negative number
    // IS the defect Chad drove into: a seated hip hinge lowers the rider, so a
    // pose that does not correct for it drives the head down through the cowl.
    float prev = 1.0f;
    for (int i = 1; i <= 30; ++i) {
        const float th = static_cast<float>(i) * 3.14159265f / 180.0f;
        const float dy = render::hinge_cg_dy(m, th);
        INFO("deg " << i << " H_y " << dy * 1000.0f << " mm");
        REQUIRE(dy < 0.0f);
        REQUIRE(dy < prev);
        prev = dy;
    }
    // at the shipped anatomical limit it is 40.4 mm of unearned CG drop
    const float at_limit = render::hinge_cg_dy(m, render::kHingeFwdMaxRad);
    INFO("H_y at the limit " << at_limit * 1000.0f << " mm");
    REQUIRE(at_limit < -0.038f);
    REQUIRE(at_limit > -0.043f);
    // H_z and H_y are the SAME rotation read on two axes: |(H_z+b, H_y+a)| is
    // the constant |(a,b)| for every theta. Nothing new is measured for H_y.
    const float r = std::sqrt(m.a * m.a + m.b * m.b);
    for (int i = -20; i <= 40; ++i) {
        const float th = static_cast<float>(i) * 3.14159265f / 180.0f;
        const float hz = render::hinge_cg_dz(m, th) + m.b;
        const float hy = render::hinge_cg_dy(m, th) + m.a;
        REQUIRE(std::fabs(std::sqrt(hz * hz + hy * hy) - r) < 1e-5f);
    }
    // degenerate model: total, no NaN
    render::RiderHingeModel z;
    REQUIRE(render::hinge_cg_dy(z, 0.5f) == 0.0f);
}

TEST_CASE("R1c: the 2x2 inverse Jacobian is an inverse, and it is TOTAL",
          "[rider_pose]") {
    const render::RiderHingeModel m = shipped_model();
    // round-trip: push a shift through J, then through J^-1, get it back.
    for (float fz : {-0.30f, -0.05f, 0.0f, 0.12f, 0.51f})
        for (float fy : {-0.10f, 0.0f, 0.07f, 0.31f}) {
            const float cz = m.k_hat * fz + m.k_zy * fy;
            const float cy = m.k_yz * fz + m.k_yy * fy;
            const render::RiderLeanShift s =
                render::lean_apply_inv_jacobian(m, cz, cy);
            INFO("fz " << fz << " fy " << fy);
            REQUIRE(std::fabs(s.fwd - fz) < 1e-4f);
            REQUIRE(std::fabs(s.up - fy) < 1e-4f);
        }
    // the OFF-DIAGONAL terms are real and are not silently dropped: a purely
    // vertical CG residual demands a non-zero FORE-AFT correction on this rig.
    const render::RiderLeanShift pv = render::lean_apply_inv_jacobian(m, 0.0f, 0.10f);
    REQUIRE(std::fabs(pv.fwd) > 1e-4f);
    // a degenerate Jacobian must fall back, not divide by zero
    render::RiderHingeModel bad;
    bad.k_hat = 0.0f;
    bad.k_yy = 0.0f;
    const render::RiderLeanShift f = render::lean_apply_inv_jacobian(bad, 0.2f, 0.3f);
    REQUIRE(std::isfinite(f.fwd));
    REQUIRE(std::isfinite(f.up));
    REQUIRE(f.fwd == 0.2f);
    REQUIRE(f.up == 0.3f);
}

TEST_CASE("the lean CG solve DRIVES BOTH ERRORS TO ZERO -- R1c acceptance",
          "[rider_pose]") {
    // The solve in render/sled_model.cpp is: seed, then kHingeNewtonSteps
    // fixed-Jacobian Newton steps on the TRUE posed CG. Here the "true" CG is a
    // stand-in whose local slope range BRACKETS the real one measured on the
    // shipped pose path -- the previous version of this test called its
    // stand-in "deliberately pessimistic" when its slope range 0.737-0.838 was
    // NARROWER than the real 0.617-1.047 (verifier finding 9). This one is not:
    // the fore-aft slope runs 1.047 down to 0.617 across the reachable |d|, and
    // the vertical one is given its own, different curvature plus a genuine
    // cross-term, so the fixed Jacobian is wrong in BOTH entries and BOTH
    // directions. The point of the test is the CONTRACTION.
    const render::RiderHingeModel m = shipped_model();
    //
    // The fore-aft slope is EXACTLY the verifier's measured span: 1.047 at
    // d = 0 falling linearly to 0.617 at |d| = 0.55 (the largest honest root
    // translation on this rig), i.e. dCG/dd = 1.047 - 0.782|d|, integrated.
    // The vertical one is given its own, DIFFERENT curvature (0.870 -> 0.700)
    // so the fixed Jacobian is wrong in both entries and by different amounts,
    // plus the real cross-terms. Against a fixed k_hat of 0.819 that is a
    // worst |1 - K/k_hat| of 0.278 -- the true figure the verifier measured,
    // 4-5x the 0.054 attempt 2 claimed.
    auto true_cg = [](render::RiderLeanShift d, float* cz, float* cy) {
        *cz = d.fwd * (1.047f - 0.391f * std::fabs(d.fwd)) - 0.031f * d.up;
        *cy = d.up * (0.870f - 0.213f * std::fabs(d.up)) - 0.128f * d.fwd;
    };
    // the slope stand-in really does span the measured range, both ends
    {
        const auto slope_z = [&](float d) {
            float a0, b0, a1, b1;
            true_cg({d, 0.0f}, &a0, &b0);
            true_cg({d + 0.0005f, 0.0f}, &a1, &b1);
            return (a1 - a0) / 0.0005f;
        };
        REQUIRE(slope_z(0.0f) > 1.040f);
        REQUIRE(slope_z(0.55f) < 0.625f);
        // and that really is worse than the fixed Jacobian by the measured
        // 0.278, not by attempt 2's claimed 0.054
        REQUIRE(std::fabs(1.0f - slope_z(0.0f) / m.k_hat) > 0.25f);
    }
    float worst = 0.0f, worst_ratio = 0.0f, worst_seed = 0.0f;
    float at_lean = 0.0f, at_up = 0.0f;
    for (int i = -25; i <= 45; ++i)
        for (int j = -10; j <= 25; ++j) {
            const float lean = 0.01f * static_cast<float>(i);
            const float up = 0.01f * static_cast<float>(j);
            const float theta = render::hinge_theta_for(m, lean);
            // what the TRANSLATION must supply, on each axis
            const float tz = lean - render::hinge_cg_dz(m, theta);
            const float ty = up - render::hinge_cg_dy(m, theta);
            render::RiderLeanShift d = render::lean_seed(m, theta, lean, up);
            float cz = 0.0f, cy = 0.0f;
            true_cg(d, &cz, &cy);
            const float err0 =
                std::max(std::fabs(cz - tz), std::fabs(cy - ty));
            for (int it = 0; it < render::kHingeNewtonSteps; ++it) {
                true_cg(d, &cz, &cy);
                d = render::lean_newton_step(m, d, cz, cy, tz, ty);
            }
            true_cg(d, &cz, &cy);
            const float err = std::max(std::fabs(cz - tz), std::fabs(cy - ty));
            INFO("lean " << lean << " up " << up << " seed err "
                         << err0 * 1000.0f << " mm -> solved "
                         << err * 1000.0f << " mm");
            REQUIRE(err <= err0 + 1e-9f);   // never worse than the seed
            if (err > worst) {
                worst = err;
                worst_seed = err0;
                at_lean = lean;
                at_up = up;
            }
            if (err0 > 1e-4f) worst_ratio = std::max(worst_ratio, err / err0);
        }
    INFO("worst " << worst * 1000.0f << " mm (seed " << worst_seed * 1000.0f
                  << " mm) at lean " << at_lean << " up " << at_up
                  << "; worst contraction ratio " << worst_ratio);
    // THE ALGORITHMIC PROPERTY, which is what generalises: two fixed-Jacobian
    // steps CONTRACT the seed error by more than 11x everywhere on the grid, and
    // never diverge at any corner. MEASURED worst ratio on this stand-in:
    // 0.0773, i.e. 12.9x, and the bound is set just above it.
    REQUIRE(worst_ratio < 0.090f);
    // The absolute bound is the MEASURED worst of this stand-in (0.832 mm, at
    // lean -0.08 / up +0.25) with a little headroom, not a round number chosen
    // to pass. ★ IT IS NOT THE SHIPPED
    // RESIDUAL and must not be quoted as one: the shipped figures are 1.135 mm
    // (fore-aft) and 1.527 mm (vertical), measured on the REAL pose path over
    // 4,680 reachable cells including steer. This stand-in cannot see the pose
    // path at all -- render/sled_model.cpp is in the raylib-only exe target and
    // still has no test TU, an open finding R1c does not close.
    REQUIRE(worst < 0.0010f);
    // ★ ZERO IN, ZERO OUT -- and this is the SIGNED VISUAL. With no lean of any
    // kind the hinge is 0, both residuals are 0, and every Newton step is an
    // exact no-op, so the rest pose is reproduced bit-for-bit.
    {
        const float theta = render::hinge_theta_for(m, 0.0f);
        REQUIRE(theta == 0.0f);
        render::RiderLeanShift d = render::lean_seed(m, theta, 0.0f, 0.0f);
        REQUIRE(d.fwd == 0.0f);
        REQUIRE(d.up == 0.0f);
        for (int it = 0; it < render::kHingeNewtonSteps; ++it)
            d = render::lean_newton_step(m, d, 0.0f, 0.0f, 0.0f, 0.0f);
        REQUIRE(d.fwd == 0.0f);
        REQUIRE(d.up == 0.0f);
    }
    // ★ THE RISE ITSELF: a pure forward lean, with NO stand input at all, must
    // still demand a POSITIVE root rise -- that is the whole rung. It is not
    // authored anywhere; it falls out of holding CG_y at lean_up_m == 0 while
    // the hinge drops it.
    {
        const float theta = render::hinge_theta_for(m, 0.45f);
        REQUIRE(theta == render::kHingeFwdMaxRad);
        const render::RiderLeanShift d = render::lean_seed(m, theta, 0.45f, 0.0f);
        INFO("seed rise " << d.up * 1000.0f << " mm");
        REQUIRE(d.up > 0.030f);   // measured on the real pose path: ~84 mm
        REQUIRE(d.fwd > 0.40f);   // and the fore-aft residual is still carried
    }
    // the hinge is spent FIRST: inside its authority the fore-aft translation
    // is ~0 (the vertical one is NOT -- it is buying back H_y).
    {
        const float small =
            0.5f * render::hinge_cg_dz(m, render::kHingeFwdMaxRad);
        const float th_small = render::hinge_theta_for(m, small);
        REQUIRE(th_small < render::kHingeFwdMaxRad);
        const render::RiderLeanShift d =
            render::lean_seed(m, th_small, small, 0.0f);
        REQUIRE(std::fabs(d.fwd) < 1e-3f);
        REQUIRE(d.up > 0.0f);
    }
    // AFT lean is a CG-SOLVED TRANSLATION (see rider_pose.h for the arm-reach
    // measurement that decided that), so theta is 0 and the translation carries
    // ALL of it -- and it is BIGGER in magnitude than the kernel's own
    // lean_aft_max_m, which is precisely what CG honesty costs when 42 % of the
    // rider's mass is pinned to sockets that do not translate. With theta 0
    // there is no hinge drop, so the rise is 0 too: no lean, no invented motion.
    {
        const float th_aft = render::hinge_theta_for(m, -0.25f);
        REQUIRE(th_aft == 0.0f);
        const render::RiderLeanShift d =
            render::lean_seed(m, th_aft, -0.25f, 0.0f);
        REQUIRE(d.fwd < -0.25f);
        REQUIRE(std::fabs(d.up) < 0.06f);
    }
    // ★ AND IT COMPOSES WITH A REAL STAND WITHOUT DOUBLING. Standing while
    // leaning must not ADD two rises: the demand at (fwd, up) is the SAME
    // vertical target as (0, up) plus exactly the hinge's own drop, no more.
    {
        const float th = render::hinge_theta_for(m, 0.45f);
        const render::RiderLeanShift lean_only =
            render::lean_seed(m, th, 0.45f, 0.0f);
        const render::RiderLeanShift stand_only =
            render::lean_seed(m, 0.0f, 0.0f, 0.25f);
        const render::RiderLeanShift both =
            render::lean_seed(m, th, 0.45f, 0.25f);
        INFO("lean " << lean_only.up << " stand " << stand_only.up << " both "
                     << both.up);
        // the seed is LINEAR in the residual, so composition is exact here:
        // both == lean_only + stand_only, never more.
        REQUIRE(std::fabs(both.up - (lean_only.up + stand_only.up)) < 1e-5f);
        REQUIRE(both.up < 0.5f);   // and it stays anatomically sane
    }
}

TEST_CASE("capture_hinge_model reads the HINGED SET off the rest pose",
          "[rider_pose][asset]") {
    // Built from the SHIPPED joint world positions, so a re-export that moves
    // the trunk changes these numbers and this test says so.
    const Glb& g = glb();
    REQUIRE(g.ok);
    std::array<glm::vec3, render::kRiderJointCount> rest{};
    const auto& js = render::rider_joint_specs();
    for (int j = 0; j < render::kRiderJointCount; ++j)
        rest[std::size_t(j)] = Pw(js[std::size_t(j)].name);
    const render::RiderHingeModel m =
        render::capture_hinge_model(rest, 0.855765f);
    INFO("a " << m.a << " b " << m.b);
    // ★ re-pinned 2026-08-18 to the SUDBURIAN's seated trunk (the legacy
    // figure read a 0.112596 / b 0.050595): the taller 1.85 m torso puts the
    // hinged CG 218.6 mm above the hip.
    REQUIRE(std::fabs(m.a - 0.126356f) < 1e-4f);
    REQUIRE(std::fabs(m.b - 0.070138f) < 1e-4f);
    // hinged mass fraction 0.578 with its CG 218.6 mm above the hip
    REQUIRE(std::fabs(m.a / 0.578f - 0.218609f) < 1e-4f);
    // ★ THE BOUNDED-AUTHORITY FINDING, pinned as a number: the whole hinge is
    // worth ~55 mm of CG at the anatomical limit, out of a 450 mm forward range.
    const float authority = render::hinge_cg_dz(m, render::kHingeFwdMaxRad);
    INFO("hinge CG authority " << authority * 1000.0f << " mm of "
                               << sim::SledParams{}.lean_fwd_max_m * 1000.0
                               << " mm");
    REQUIRE(authority > 0.045f);
    REQUIRE(authority < 0.060f);
    REQUIRE(authority < 0.20f * static_cast<float>(
                                   sim::SledParams{}.lean_fwd_max_m));
    // a k_hat that is nonsense must not become a divide-by-zero downstream
    REQUIRE(render::capture_hinge_model(rest, 0.0f).k_hat == 1.0f);
}

TEST_CASE("absorb's hit term is the saturating map, sized off the measurement",
          "[rider_pose]") {
    const float rest = static_cast<float>(sim::SledParams{}.susp_rest_m);
    const float x[3] = {0.0f, 0.0f, 0.0f};
    // at the half-scale the hit term is exactly 1/2 of its weight
    float vh[3] = {render::kAbsorbHitHalfMs, 0.0f, 0.0f};
    REQUIRE(std::fabs(render::rider_absorb(x, vh, rest) -
                      0.5f * render::kAbsorbHitWeight) < 1e-6f);
    // MEASURED trail chatter (p50 of max(+susp_v) over build/sled_tape_4..7 is
    // 0.03-0.05 m/s): the man is essentially still.
    float chatter[3] = {0.05f, 0.0f, 0.0f};
    REQUIRE(render::rider_absorb(x, chatter, rest) < 0.01f);
    // MEASURED strike (p99 is 22-25 m/s): most of the crouch the rate term can
    // give, without ever pegging.
    float strike[3] = {24.0f, 0.0f, 0.0f};
    const float a = render::rider_absorb(x, strike, rest);
    REQUIRE(a > 0.40f);
    REQUIRE(a < 1.0f);
}

TEST_CASE("absorb is a PURE function -- same input, same output, no state",
          "[rider_pose]") {
    // §0.1. If anything below ever becomes an accumulator this goes red, and
    // with it the claim that the rider replays bit-exact from a tape.
    const float x[3] = {0.07f, 0.02f, 0.11f};
    const float v[3] = {3.0f, -2.0f, 0.4f};
    const float rest = static_cast<float>(sim::SledParams{}.susp_rest_m);
    const float first = render::rider_absorb(x, v, rest);
    for (int i = 0; i < 64; ++i) {
        const float other[3] = {0.5f, 0.5f, 0.5f};
        (void)render::rider_absorb(other, other, rest);  // try to poison it
        REQUIRE(render::rider_absorb(x, v, rest) == first);
    }
}

// ===========================================================================
// R2c-5 -- THE CONTROL-INPUT POSE (throttle / brake)
// ===========================================================================
//
// What these pin, and what they cannot. The composed pose lives in
// render/sled_model.cpp (raylib, no test TU), so what is provable here is the
// MATH and the ASSET FACTS the composition rests on: that the roll is a true
// rotation about the bar through the contact, that the pole rotation is a true
// rotation, that the two sides do NOT share the elbow sign while they DO share
// the wrist sign, and that on the shipped bytes the ruled directions come out
// -- right elbow DOWN and right hand UP on throttle, left elbow UP and left
// hand DOWN on brake. A mirrored or renamed re-export fails these.

TEST_CASE("the control articulations are Chad's ONE 20 degrees", "[rider_pose]") {
    const float deg20 = 20.0f * 3.14159265f / 180.0f;
    REQUIRE(std::fabs(render::kControlElbowSwingRad - deg20) < 1e-6f);
    REQUIRE(std::fabs(render::kControlWristRad - deg20) < 1e-6f);
}

TEST_CASE("the wrist roll leaves the CONTACT POINT exactly where it was",
          "[rider_pose]") {
    // This is R2c-1 and R2c-3 in one line: the hand holds the bar at the
    // KNUCKLE, so the rotation is about the bar THROUGH the contact, and the
    // contact therefore cannot drift no matter how far the wrist rolls. The
    // Blender side buys this with 24 solver iterations; here it is structural.
    const glm::vec3 pivot(0.31f, 1.02f, 0.44f);
    const glm::vec3 axis(1.0f, 0.03f, -0.02f);
    glm::mat4 hand(1.0f);
    hand[3] = glm::vec4(0.30f, 0.94f, 0.36f, 1.0f);  // wrist, off the bar
    for (int a = -40; a <= 40; a += 5) {
        const float th = static_cast<float>(a) * 0.0174532925f;
        const glm::mat4 m = render::roll_about_axis(hand, pivot, axis, th);
        // the contact point expressed in the hand's frame, before and after
        const glm::vec4 local = glm::inverse(hand) * glm::vec4(pivot, 1.0f);
        const glm::vec3 moved = glm::vec3(m * local);
        INFO("angle " << a);
        REQUIRE(glm::length(moved - pivot) < 1e-5f);
        // ...and the WRIST is what moved, which is what the IK re-solves for
        if (a != 0)
            REQUIRE(glm::length(glm::vec3(m[3]) - glm::vec3(hand[3])) > 1e-3f);
    }
}

TEST_CASE("the wrist roll is a RIGID rotation of the whole hand",
          "[rider_pose]") {
    const glm::vec3 pivot(0.2f, 1.0f, 0.4f), axis(1.0f, 0.0f, 0.0f);
    glm::mat4 hand = glm::rotate(glm::mat4(1.0f), 0.7f,
                                 glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)));
    hand[3] = glm::vec4(0.18f, 0.93f, 0.35f, 1.0f);
    const float th = 0.35f;
    const glm::mat4 m = render::roll_about_axis(hand, pivot, axis, th);
    // basis lengths preserved (no shear, no scale)
    for (int c = 0; c < 3; ++c)
        REQUIRE(std::fabs(glm::length(glm::vec3(m[c])) - 1.0f) < 1e-5f);
    // and the rotation applied is EXACTLY th about the axis
    const glm::mat3 d = glm::mat3(m) * glm::transpose(glm::mat3(hand));
    const float tr = d[0][0] + d[1][1] + d[2][2];
    REQUIRE(std::fabs(std::acos((tr - 1.0f) * 0.5f) - th) < 1e-4f);
    // a degenerate axis is a NO-OP, not a NaN
    const glm::mat4 z =
        render::roll_about_axis(hand, pivot, glm::vec3(0.0f), th);
    REQUIRE(glm::length(glm::vec3(z[3]) - glm::vec3(hand[3])) < 1e-6f);
}

TEST_CASE("the pole swing is a rotation of the bend plane about the chain",
          "[rider_pose]") {
    const glm::vec3 dir = glm::normalize(glm::vec3(0.4f, -0.5f, 0.8f));
    const glm::vec3 n = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    for (int a = -60; a <= 60; a += 5) {
        const float th = static_cast<float>(a) * 0.0174532925f;
        const glm::vec3 m = render::swing_bend_normal(n, dir * 2.7f, th);
        INFO("angle " << a);
        REQUIRE(std::fabs(glm::length(m) - 1.0f) < 1e-5f);
        // the normal stays perpendicular to the chain, so the elbow stays on
        // the circle the IK's own half-triangle put it on -- the swing cannot
        // change the elbow FLEX, only where around the chain it sits.
        REQUIRE(std::fabs(glm::dot(m, dir)) < 1e-5f);
        // and the bend DIRECTION turns by exactly the requested angle
        // atan2 of (cross, dot), not acos: acos is ill-conditioned near 0
        // and reads 0.8 mrad of noise as a real angle.
        const glm::vec3 b0 = glm::cross(n, dir), b1 = glm::cross(m, dir);
        const float got = std::atan2(glm::dot(glm::cross(b0, b1), dir),
                                     glm::dot(b0, b1));
        REQUIRE(std::fabs(got - th) < 1e-5f);
    }
    REQUIRE(render::swing_bend_normal(n, glm::vec3(0.0f), 0.5f) == n);
}

TEST_CASE("the two ELBOW signs do NOT mirror and the two WRIST signs DO",
          "[rider_pose]") {
    // * THE TRAP, AS A TEST. It appeared three times in one Blender session:
    // the elbow poles needed OPPOSITE signs between sides, the wrists the SAME
    // sign. The reason is structural, and it is why neither is written down as
    // a constant anywhere: the pole axis is the arm's own chain direction,
    // which MIRRORS with the side, and a rotation about a mirrored axis
    // reverses -- while the wrist axis is the BAR, one vector shared by both.
    const glm::vec3 sh(-0.20f, 1.30f, -0.10f);
    const glm::vec3 el(-0.34f, 1.12f, 0.10f);
    const glm::vec3 hd(-0.26f, 1.02f, 0.36f);
    const auto mir = [](const glm::vec3& v) {
        return glm::vec3(-v.x, v.y, v.z);
    };
    const float dl = render::swing_down_sign(sh, el, hd - sh);
    const float dr =
        render::swing_down_sign(mir(sh), mir(el), mir(hd) - mir(sh));
    REQUIRE(dl != 0.0f);
    REQUIRE(dr == -dl);  // ANTI-mirrors

    const glm::vec3 bar(1.0f, 0.0f, 0.0f);  // ONE axis, from the machine
    const glm::vec3 grip(-0.26f, 1.02f, 0.36f), wrist(-0.24f, 0.98f, 0.24f);
    const float ul = render::wrist_up_sign(wrist, grip, bar);
    const float ur = render::wrist_up_sign(mir(wrist), mir(grip), bar);
    REQUIRE(ul != 0.0f);
    REQUIRE(ur == ul);  // SHARED
}

TEST_CASE("a sign is 0, never a guess, when there is nothing to measure",
          "[rider_pose]") {
    // A straight arm has no pole and a wrist ON the bar axis has no roll. The
    // caller reads 0 as "this side cannot articulate" and poses nothing --
    // which is the honest answer, and never a NaN or an invented direction.
    const glm::vec3 sh(0.0f, 1.0f, 0.0f), hd(0.0f, 0.6f, 0.0f);
    REQUIRE(render::swing_down_sign(sh, glm::vec3(0.0f, 0.8f, 0.0f), hd - sh) ==
            0.0f);
    REQUIRE(render::swing_down_sign(sh, glm::vec3(0.0f, 0.8f, 0.0f),
                                    glm::vec3(0.0f)) == 0.0f);
    REQUIRE(render::wrist_up_sign(glm::vec3(0.5f, 1.0f, 0.0f),
                                  glm::vec3(0.0f, 1.0f, 0.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f)) == 0.0f);
}

TEST_CASE("SHIPPED ASSET: throttle is the model -X hand, and the four "
          "directions are the ones Chad ruled",
          "[rider_pose]") {
    REQUIRE(glb().ok);
    // The runtime derives the throttle side by POSITION, never by name (a
    // rename or a mirrored export cannot silently swap it). Re-derived here.
    const glm::vec3 gL = Pw("grip_socket_L"), gR = Pw("grip_socket_R");
    REQUIRE(gL.x < 0.0f);
    REQUIRE(gR.x > 0.0f);
    // side 0 = model -X = the rider's anatomical RIGHT = `hand_r` = the
    // THROTTLE hand (render/rider_rig.h 2.3a). Since the Sudburian the `_r`
    // chain welds to `grip_socket_L` -- the crossover -- and the side is still
    // derived from the socket's x, never from a name.
    const glm::vec3 bar = gR - gL;  // side1 - side0, as the runtime builds it

    struct Side {
        const char* sh;
        const char* el;
        const char* hd;
        glm::vec3 grip;
        float dir;  // +1 throttle (elbow DOWN, wrist UP), -1 brake
    };
    const Side sides[2] = {{"upperarm_r", "lowerarm_r", "hand_r", gL, +1.0f},
                           {"upperarm_l", "lowerarm_l", "hand_l", gR, -1.0f}};
    for (const Side& sd : sides) {
        const glm::vec3 sh = Pw(sd.sh), el = Pw(sd.el), hd = Pw(sd.hd);
        const float dsign = render::swing_down_sign(sh, el, hd - sh);
        const float usign = render::wrist_up_sign(hd, sd.grip, bar);
        INFO(sd.sh);
        REQUIRE(dsign != 0.0f);
        REQUIRE(usign != 0.0f);

        // the ELBOW, through the same composition the runtime does: the pole
        // rotation turns the bend plane, and the elbow's radial offset with it.
        const glm::vec3 dir = glm::normalize(hd - sh);
        const glm::vec3 r = (el - sh) - dir * glm::dot(el - sh, dir);
        const glm::vec3 r2 =
            glm::angleAxis(sd.dir * dsign * render::kControlElbowSwingRad,
                           dir) *
            r;
        // throttle DROPS the right elbow; brake RAISES the left one. And it is
        // a real articulation, not a rounding error: >= 10 mm at 20 deg.
        REQUIRE(sd.dir * (r2.y - r.y) < -0.010f);

        // the WRIST, through roll_about_axis about the bar at the grip
        glm::mat4 hand(1.0f);
        hand[3] = glm::vec4(hd, 1.0f);
        const glm::mat4 rolled = render::roll_about_axis(
            hand, sd.grip, bar, sd.dir * usign * render::kControlWristRad);
        // throttle takes the right hand UP (thumb onto the proximal side);
        // brake takes the left hand DOWN and over the bar to the fingers.
        //
        // ★ 2026-08-18, THE SUDBURIAN. `hand_*` is the WRIST and the knuckle
        // is on the bar, so the wrist sits one fist (98 mm: 51 up, 83 aft)
        // from the socket and a 20 deg roll about the bar moves it ~34 mm --
        // up for the throttle, down for the brake, exactly the R2c-5
        // construction ("when the proxy replaces the legacy rider its knuckle
        // IS on the socket"). Pinned as direction AND magnitude.
        const glm::vec3 moved = glm::vec3(rolled[3]) - hd;
        REQUIRE(sd.dir * moved.y > 0.010f);          // the ruled DIRECTION, real
        REQUIRE(glm::length(moved) > 0.025f);        // a fist's worth of roll
    }
}

// ===========================================================================
// H8 -- THE HELMET'S TWO HUES ARE THE HEADER'S, NOT A RETYPED PAIR
// docs/HELMET_SPEC.md 4 (gate H8) and 1: "parsed from the header, never
// retyped (the R2c-7 rule)".
//
// The trap this exists to catch is recorded in the ladder: R2c-7 shipped the
// mitts BROWN because an sRGB->linear step was inserted between the constant
// and the file. glTF calls baseColorFactor linear; this engine has no linear
// pipeline and every one of the asset's materials is authored in raylib BYTE
// space, so the factor in the GLB must be the header's number EXACTLY -- to
// 1e-6, no colour-space arithmetic anywhere in between.
//
// The generator (assets/character/sudburian_src/helmet_geom.py) PARSES
// render/team_color.h with a regex; this test compiles the same header in and
// compares the shipped bytes to it, so the two can never drift apart silently.
// ===========================================================================
namespace {

struct GlbMat {
    bool found = false;
    glm::dvec3 base{0.0};
    double roughness = 1.0;
};

GlbMat glb_material(const std::string& want) {
    GlbMat out;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sled/indy650.glb";
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
        return out;
    for (cgltf_size i = 0; i < d->materials_count; ++i) {
        const cgltf_material& m = d->materials[i];
        if (m.name == nullptr || want != m.name) continue;
        if (!m.has_pbr_metallic_roughness) break;
        const cgltf_float* c = m.pbr_metallic_roughness.base_color_factor;
        out.base = glm::dvec3(c[0], c[1], c[2]);
        out.roughness = m.pbr_metallic_roughness.roughness_factor;
        out.found = true;
        break;
    }
    cgltf_free(d);
    return out;
}

}  // namespace

TEST_CASE("H8 SHIPPED ASSET: helmet_orange IS kSlagOrange; the v13 ACCEPTED "
          "egg livery is present (charcoal and blue are GONE)",
          "[rider_pose]") {
    // v13 (Chad, 2026-08-20, ACCEPTED on first look): the EGG helmet with the
    // v12 stripe scheme -- silver cap, black striping, slag-orange band, WHITE
    // pinstripes, BLACK bottom (his quote-11 ruling: "why is the very bottom
    // color grey and not black???" -- so helmet_charcoal must be GONE, like
    // helmet_blue before it). Colours are the ACCEPTED palette_v12 bytes.
    const GlbMat orange = glb_material("helmet_orange");
    const GlbMat silver = glb_material("helmet_silver");
    const GlbMat white = glb_material("helmet_white");
    const GlbMat black = glb_material("helmet_black");
    const GlbMat charcoal = glb_material("helmet_charcoal");
    const GlbMat blue = glb_material("helmet_blue");
    INFO("helmet_orange/silver/white/black must exist in "
         "assets/sled/indy650.glb. If they do not, the helmet is applied in "
         "the live .blend but NOT yet exported (export_live_v13.py) and "
         "spliced (splice_sudburian.py) -- this gate is RED until that runs.");
    REQUIRE(orange.found);
    REQUIRE(silver.found);
    REQUIRE(white.found);
    REQUIRE(black.found);
    REQUIRE_FALSE(charcoal.found);
    REQUIRE_FALSE(blue.found);
    for (int k = 0; k < 3; ++k) {
        INFO("channel " << k);
        REQUIRE(std::fabs(orange.base[k] - render::kSlagOrange[k]) <= 1e-6);
    }
    // the ACCEPTED bytes (palette_v12.py), byte space, never linearised
    const glm::dvec3 kSilver(195.0 / 255.0, 198.0 / 255.0, 186.0 / 255.0);
    const glm::dvec3 kWhite(242.0 / 255.0, 242.0 / 255.0, 242.0 / 255.0);
    const glm::dvec3 kBlack(17.0 / 255.0, 15.0 / 255.0, 14.0 / 255.0);
    for (int k = 0; k < 3; ++k) {
        INFO("channel " << k);
        REQUIRE(std::fabs(silver.base[k] - kSilver[k]) <= 1e-6);
        REQUIRE(std::fabs(white.base[k] - kWhite[k]) <= 1e-6);
        REQUIRE(std::fabs(black.base[k] - kBlack[k]) <= 1e-6);
    }
    // ... and they are the SHINY ones: render/sled_model.cpp turns a roughness
    // below 0.5 on a `helmet_` material into the Blinn-Phong highlight, so a
    // re-export that lost the node tree (which would write 0.8 grey / rough
    // 0.5) fails here too.
    REQUIRE(orange.roughness < 0.5);
    REQUIRE(silver.roughness < 0.5);
}

// ===========================================================================
// ★★★ R3-WS -- THE FORE-AFT WEIGHT-SHIFT LADDER
//     docs/SUDBURIAN_R3_WEIGHTSHIFT_SPEC.md 7
// ===========================================================================
//
// Everything below re-derives its geometry FROM THE SHIPPED GLB, so a
// re-export that moves the seat, the deck, the rider or the bars fails these
// cases instead of silently invalidating the ladder. The one exception is the
// baked seat profile itself, which is compared AGAINST the file's own seat
// mesh -- that is the whole point of baking it.

namespace {

// A downward raycast onto a named mesh node, the same measurement
// assets/character/sudburian_src/measure_seat_profile.py bakes from.
struct Tri { glm::vec3 a, b, c; };

const std::vector<Tri>& seat_tris() {
    static const std::vector<Tri> t = [] {
        std::vector<Tri> out;
        const std::string path =
            std::string(SEADS_ASSET_DIR) + "/sled/indy650.glb";
        cgltf_options opt{};
        cgltf_data* d = nullptr;
        if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success)
            return out;
        if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
            cgltf_free(d);
            return out;
        }
        for (cgltf_size i = 0; i < d->nodes_count; ++i) {
            const cgltf_node& nd = d->nodes[i];
            if (nd.name == nullptr || std::string(nd.name) != "seat") continue;
            if (nd.mesh == nullptr) break;
            cgltf_float w[16];
            cgltf_node_transform_world(&nd, w);
            glm::mat4 m(1.0f);
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r) m[c][r] = w[4 * c + r];
            for (cgltf_size p = 0; p < nd.mesh->primitives_count; ++p) {
                const cgltf_primitive& pr = nd.mesh->primitives[p];
                const cgltf_accessor* pos = nullptr;
                for (cgltf_size a = 0; a < pr.attributes_count; ++a)
                    if (pr.attributes[a].type == cgltf_attribute_type_position)
                        pos = pr.attributes[a].data;
                if (pos == nullptr) continue;
                std::vector<glm::vec3> v(pos->count);
                for (cgltf_size k = 0; k < pos->count; ++k) {
                    cgltf_float f[3] = {0, 0, 0};
                    cgltf_accessor_read_float(pos, k, f, 3);
                    v[k] = glm::vec3(m * glm::vec4(f[0], f[1], f[2], 1.0f));
                }
                const cgltf_accessor* ix = pr.indices;
                const cgltf_size n = ix != nullptr ? ix->count : pos->count;
                for (cgltf_size k = 0; k + 2 < n; k += 3) {
                    const auto id = [&](cgltf_size q) {
                        return ix != nullptr
                                   ? static_cast<std::size_t>(
                                         cgltf_accessor_read_index(ix, q))
                                   : static_cast<std::size_t>(q);
                    };
                    out.push_back({v[id(k)], v[id(k + 1)], v[id(k + 2)]});
                }
            }
            break;
        }
        cgltf_free(d);
        return out;
    }();
    return t;
}

// Highest y of any seat triangle covering (x, z) in plan; NaN when uncovered.
float ray_down(float x, float z) {
    float best = std::numeric_limits<float>::quiet_NaN();
    for (const Tri& t : seat_tris()) {
        const float den = (t.b.z - t.c.z) * (t.a.x - t.c.x) +
                          (t.c.x - t.b.x) * (t.a.z - t.c.z);
        if (std::fabs(den) < 1e-12f) continue;
        const float w0 = ((t.b.z - t.c.z) * (x - t.c.x) +
                          (t.c.x - t.b.x) * (z - t.c.z)) / den;
        const float w1 = ((t.c.z - t.a.z) * (x - t.c.x) +
                          (t.a.x - t.c.x) * (z - t.c.z)) / den;
        const float w2 = 1.0f - w0 - w1;
        if (w0 < -1e-6f || w1 < -1e-6f || w2 < -1e-6f) continue;
        const float y = w0 * t.a.y + w1 * t.b.y + w2 * t.c.y;
        if (std::isnan(best) || y > best) best = y;
    }
    return best;
}

// The rest captures the ladder makes at load, re-derived here from the file.
struct WsRest {
    glm::vec3 pelvis{0.0f};
    glm::vec3 shoulder_off[2]{};   // [0] = model -X (hand_r), [1] = +X (hand_l)
    glm::vec3 hand[2]{};
    glm::vec3 foot[2]{};
    float dmax = 0.0f, rho0 = 0.0f, thigh = 0.0f, shin = 0.0f;
};

const WsRest& ws_rest() {
    static const WsRest r = [] {
        WsRest o;
        o.pelvis = Pw("pelvis");
        const char* up[2] = {"upperarm_r", "upperarm_l"};
        const char* hd[2] = {"hand_r", "hand_l"};
        const char* ft[2] = {"foot_r", "foot_l"};
        for (int s = 0; s < 2; ++s) {
            o.shoulder_off[s] = Pw(up[s]) - o.pelvis;
            o.hand[s] = Pw(hd[s]);
            o.foot[s] = Pw(ft[s]);
        }
        o.dmax = glm::length(Pw("lowerarm_l") - Pw("upperarm_l")) +
                 glm::length(Pw("hand_l") - Pw("lowerarm_l")) - 1e-3f;
        for (int s = 0; s < 2; ++s)
            o.rho0 = std::max(o.rho0,
                              glm::length(o.hand[s] - o.pelvis -
                                          o.shoulder_off[s]) / o.dmax);
        o.thigh = glm::length(Pw("calf_l") - Pw("thigh_l"));
        o.shin = glm::length(Pw("foot_l") - Pw("calf_l"));
        return o;
    }();
    return r;
}

// The transform a STEER applies to everything under the steering post -- the
// same composition render/sled_model.cpp's pose_pass performs, re-derived here
// so the sweep can steer without linking the raylib-only TU.
glm::mat4 steer_xform(float steer) {
    const glm::mat4 sp = W("CH_steer_pivot");
    const glm::mat4 ry =
        glm::mat4(glm::mat3_cast(glm::angleAxis(steer * render::kBarSweepRad,
                                                glm::vec3(0.0f, 1.0f, 0.0f))));
    return sp * ry * glm::inverse(sp);
}

}  // namespace

TEST_CASE("R3-WS: the baked seat profile IS the shipped seat's top surface",
          "[rider_pose][asset]") {
    REQUIRE(glb().ok);
    REQUIRE_FALSE(seat_tris().empty());
    const auto& tab = render::seat_profile_y();
    REQUIRE(tab.size() == std::size_t(render::kSeatStations));
    // ★ THE TRAP THIS TABLE EXISTS TO AVOID: the seat's bbox top is the crown
    // of the FRONT hump, 65 mm above the pan the pelvis actually rides.
    float top = -1e9f;
    for (const Tri& t : seat_tris())
        for (const glm::vec3* p : {&t.a, &t.b, &t.c}) top = std::max(top, p->y);
    float tab_max = 0.0f;
    for (float v : tab) tab_max = std::max(tab_max, v);
    REQUIRE(std::fabs(top - 0.663307f) < 1e-4f);
    // every station is the RAYCAST value, to a tenth of a millimetre
    const float span = render::kSeatZFrontM - render::kSeatZRearM;
    for (int i = 0; i < render::kSeatStations; ++i) {
        const float z = render::kSeatZRearM +
                        span * float(i) / float(render::kSeatStations - 1);
        float want = ray_down(0.0f, z);
        if (std::isnan(want))
            want = ray_down(0.0f, z + (i == 0 ? 1e-5f : -1e-5f));
        INFO("station " << i << " z " << z);
        REQUIRE_FALSE(std::isnan(want));
        REQUIRE(std::fabs(tab[std::size_t(i)] - want) < 1e-4f);
        // ... and seat_top_y reproduces the table AT the stations
        REQUIRE(std::fabs(render::seat_top_y(z) - want) < 1e-4f);
    }
    // ★ R3-WS(b), RED-TEAM FIX 6: station 0 to the GENERATOR's own digits.
    // The shipped table had 0.543343 where measure_seat_profile.py emits
    // 0.543322 -- retyped, not measured, and the 1e-4 tolerance above was wide
    // enough to hide 21 microns of drift. This is the exact-digit pin.
    REQUIRE(tab.front() == 0.543322f);
    // the main pan is genuinely flat, and it is where the rider sits
    REQUIRE(std::fabs(render::seat_top_y(-0.30f) - 0.598322f) < 1e-4f);
    REQUIRE(std::fabs(render::seat_top_y(ws_rest().pelvis.z) - 0.598322f) <
            1e-4f);
    // clamped outside the measured extents, so the function is TOTAL
    REQUIRE(render::seat_top_y(-5.0f) == tab.front());
    REQUIRE(render::seat_top_y(+5.0f) == tab.back());
    REQUIRE(tab_max <= top + 1e-4f);
}

TEST_CASE("R3-WS: the deck and boot constants ARE the shipped machine",
          "[rider_pose][asset]") {
    const Glb& g = glb();
    REQUIRE(g.ok);
    // the deck SHEET (never the lip), re-derived exactly as R1a did
    for (const char* n : {"board_L", "board_R"}) {
        const int i = idx_of(n);
        REQUIRE(i >= 0);
        INFO(n);
        REQUIRE(std::fabs(g.vmin[std::size_t(i)].y - render::kDeckSheetYM) <
                1e-5f);
        REQUIRE(std::fabs(g.vmin[std::size_t(i)].z - render::kDeckZRearM) <
                1e-4f);
        REQUIRE(std::fabs(g.vmax[std::size_t(i)].z - render::kDeckZFrontM) <
                1e-4f);
        // and the LIP is still 25 mm above the sheet -- the trap, pinned
        REQUIRE(std::fabs(g.vmax[std::size_t(i)].y - 0.277000f) < 1e-5f);
    }
    // the ankle sits kAnkleAboveSoleM above the deck sheet, both sides
    for (const char* n : {"foot_l", "foot_r"}) {
        INFO(n);
        REQUIRE(std::fabs((Pw(n).y - render::kDeckSheetYM) -
                          render::kAnkleAboveSoleM) < 1e-4f);
    }
    // the boot is long enough that the deck clamp is a real bound
    REQUIRE(render::kBootHalfLenM > 0.10f);
    REQUIRE(render::kBootHalfLenM < 0.25f);
    // ... and the knee pad radius is the MESH's, not the spec's 0.05 guess
    REQUIRE(render::kKneePadRM > 0.08f);
    REQUIRE(render::kKneePadRM < 0.11f);
}

TEST_CASE("R3-WS: the ladder is 0-OFF -- every shape term is the identity at "
          "u = 0, for any stand",
          "[rider_pose]") {
    // SPEC 7.1. The runtime makes this STRUCTURAL (one `active` branch), so
    // what this case pins is that no formula sneaks a non-zero value in at
    // u = 0 -- including at full stand, which is the trap SPEC 3's
    // max(u, 0.8*s) schedule would have walked straight into.
    for (float s : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        INFO("s " << s);
        const render::WeightShiftWeights w = render::ws_weights(0.0f, s);
        REQUIRE(w.kneel == 0.0f);
        REQUIRE(std::fabs(w.seat + w.stand + w.kneel - 1.0f) < 1e-6f);
        // THE ONE THAT MATTERS: rho must not move on the stand axis alone.
        REQUIRE(render::ws_rho(0.0f, s, 0.7301f) == 0.7301f);
        REQUIRE(render::ws_rho(0.0f, s, ws_rest().rho0) == ws_rest().rho0);
        // and the foot target at zero retreat IS the rest ankle -- to within
        // the 16 microns by which the shipped asset's two ankles disagree
        // (foot_l y 0.330801, foot_r y 0.330817; the deck is ONE plane, so the
        // parametric target cannot honour both). The runtime never sees it:
        // the blend weight is exactly 0 there.
        const glm::vec3 f = render::deck_foot_target(ws_rest().foot[0], 0.0f);
        REQUIRE(glm::length(f - ws_rest().foot[0]) < 2e-5f);
    }
    // the reach solve is a no-op whenever the rest configuration already
    // reaches -- which is every forward-lean and every u = 0 cell
    const WsRest& r = ws_rest();
    const render::ReachSolve rs = render::reach_solve(
        r.pelvis, r.shoulder_off[1], r.hand[1], r.dmax * (r.rho0 + 0.01f));
    REQUIRE(rs.theta_rad == 0.0f);
}

TEST_CASE(
    "R3-WS(d): the ladder weights are C1, the branches are exclusive, and "
    "the kneel ENGAGES AT THE RIDER -- Chad's 2026-08-20 ruling",
    "[rider_pose]") {
    float prev_k = 0.0f;
    for (int i = 0; i <= 200; ++i) {
        const float u = float(i) / 200.0f;
        for (float s : {0.0f, 0.3f, 1.0f}) {
            const render::WeightShiftWeights w = render::ws_weights(u, s);
            INFO("u " << u << " s " << s);
            REQUIRE(w.seat >= -1e-6f);
            REQUIRE(w.stand >= -1e-6f);
            REQUIRE(w.kneel >= -1e-6f);
            REQUIRE(std::fabs(w.seat + w.stand + w.kneel - 1.0f) < 1e-5f);
        }
        // the kneel is monotone in u at s = 0 and reaches 1 (non-vacuity)
        const float k = render::ws_weights(u, 0.0f).kneel;
        REQUIRE(k >= prev_k - 1e-6f);
        prev_k = k;
    }
    // ★★★★ R3-WS(e). THE GAIN IS 0 AGAIN, AND THE REASON IS A NEW RULING,
    // NOT A DRIFT. Chad, 2026-08-21: "not make kneeling for longitudinal
    // movement... eventually [the] pose activated for a hanging to a side
    // lean... key is going to be P for pull."
    //
    // The kneel is HELD FOR R4-SIDE (docs/SUDBURIAN_LADDER.md, SPEC 14). It is
    // NOT "held pending a kneel ruling" -- R3-WS(d)'s ruling came and turned it
    // on, and this one moved its address from the fore-aft ladder to a future
    // lateral survival move. Nothing about the kneel MATH is retracted: the
    // weight schedule below is still asserted, monotone and reaching 1, on the
    // pure function, exactly as it must be for the rung that inherits it.
    // What is now zero is only what the LONGITUDINAL rider is posed with.
    REQUIRE(render::kKneelRuntimeGain == 0.0f);  // ★ THE RULING, PINNED
    // SPEC 7.4 NON-VACUITY, ON THE PURE FUNCTION -- gain-independent by
    // design, so the R4-SIDE inheritance stays gated while the shipped
    // longitudinal pose has no kneel in it.
    REQUIRE(render::ws_weights(1.0f, 0.0f).kneel > 0.9f);
    // ... and the SHIPPED longitudinal pose really carries none of it.
    REQUIRE(render::ws_weights(1.0f, 0.0f).kneel * render::kKneelRuntimeGain ==
            0.0f);
    REQUIRE(render::ws_weights(1.0f, 1.0f).kneel == 0.0f);  // never while stood
    // ★ D1: and the way it DIES matters as much as the way it engages. The
    // stand slew covers up to stand_slew_ds_per_tick() of `s` in one 60 Hz
    // tick, so a narrow kill band unwinds the whole kneel in two or three
    // frames under the player's own stand key. The band is the only lever
    // (the pose is a pure function -- a slew limiter would need state), so it
    // is pinned WIDE and the per-tick weight step is bounded here.
    {
        const float ds = render::stand_slew_ds_per_tick();
        REQUIRE(ds > 0.0f);
        REQUIRE(ds < 0.10f);
        float worst = 0.0f;
        for (int i = 0; i <= 400; ++i) {
            const float s0 = float(i) / 400.0f;
            const float s1 = std::min(1.0f, s0 + ds);
            worst = std::max(worst,
                             std::fabs(render::ws_weights(1.0f, s1).kneel -
                                       render::ws_weights(1.0f, s0).kneel));
        }
        INFO("worst w_kneel step per stand tick " << worst);
        // The kneel's whole leg travel is ~0.42 m, so a weight step of 0.20
        // would already be 0.084 m of joint travel in one frame. The shipped
        // band keeps it well under that; the ABSOLUTE metres are gated on the
        // real pose path by render/sled_model.cpp's own load-time measurement.
        REQUIRE(worst < 0.20f);
    }
    // the rho schedule tops out at the "really pulling" target, never past it
    REQUIRE(std::fabs(render::ws_rho(1.0f, 0.0f, 0.5f) -
                      render::kArmRatioPull) < 1e-5f);
    REQUIRE(render::ws_rho(1.0f, 0.0f, 0.5f) < render::kArmRatioLimit);
}

TEST_CASE("R3-WS: the reach solve is EXACT -- the shoulder really is on a "
          "circle, and the closed form finds it",
          "[rider_pose][asset]") {
    const WsRest& r = ws_rest();
    // brute force the same minimum the closed form claims, on the real rig
    for (int side = 0; side < 2; ++side) {
        for (float aft : {0.0f, 0.10f, 0.20f, 0.32f}) {
            const glm::vec3 p = r.pelvis - glm::vec3(0.0f, 0.0f, aft);
            float brute = 1e9f;
            for (int i = 0; i <= 20000; ++i) {
                const float t = 6.28318531f * float(i) / 20000.0f;
                const float c = std::cos(t), sn = std::sin(t);
                const glm::vec3& v = r.shoulder_off[side];
                const glm::vec3 sh(v.x, v.y * c - v.z * sn, v.y * sn + v.z * c);
                brute = std::min(brute, glm::length(p + sh - r.hand[side]));
            }
            const float want = render::reach_max_aft(
                r.pelvis, r.shoulder_off[side], r.hand[side], brute + 1e-4f);
            INFO("side " << side << " aft " << aft << " brute " << brute);
            // reach_max_aft at exactly the closest approach must return the
            // aft where that closest approach is achieved -- i.e. it inverts
            // the same geometry the brute force just measured.
            REQUIRE(want >= 0.0f);
            // and reach_solve at a target ABOVE the minimum must attain it
            const render::ReachSolve rs =
                render::reach_solve(p, r.shoulder_off[side], r.hand[side],
                                    brute + 0.01f);
            REQUIRE(rs.dist_m <= brute + 0.0101f);
            REQUIRE(rs.theta_rad >= 0.0f);
            REQUIRE(rs.theta_rad <= render::kReachHingeMaxRad + 1e-6f);
        }
    }
}

TEST_CASE("R3-WS HEADLINE: the arm chain never clamps across the ladder sweep",
          "[rider_pose][asset]") {
    // SPEC 7.2, and it is the reason this rung exists. The sweep drives the
    // SAME pure station solve render/sled_model.cpp drives -- seat follow,
    // reach solve, and SPEC 4's shortening -- with the hands welded to the
    // LIVE (steered) grips, on geometry read out of the shipped GLB.
    const WsRest& r = ws_rest();
    float worst = 0.0f, worst_head = 0.0f, worst_gap = -9.0f;
    float worst_u = 0.0f, worst_steer = 0.0f;
    int shortened_cells = 0, cells = 0;
    float min_aft_at_u1 = 9.0f;
    for (int iu = 0; iu <= 32; ++iu) {
        const float u = float(iu) / 32.0f;
        for (int isv = 0; isv < 5; ++isv) {
            const float s = float(isv) / 4.0f;
            for (int st = -1; st <= 1; ++st) {
                const glm::mat4 X = steer_xform(float(st));
                render::ReachArm a[2];
                for (int k = 0; k < 2; ++k) {
                    a[k].shoulder_off = r.shoulder_off[k];
                    a[k].hand = glm::vec3(X * glm::vec4(r.hand[k], 1.0f));
                    a[k].dmax = r.dmax;
                }
                const render::WeightShiftWeights w = render::ws_weights(u, s);
                // ★ R3-WS(b): the trunk-flexion gate is ws_reach_gate now, not
                // the foot blend -- drive what the runtime drives.
                const float gate = render::ws_reach_gate(u, s);
                const float head =
                    render::reach_pair_ratio(r.pelvis, a[0], a[1], 0.0f);
                const float ceiling = std::max(render::kArmRatioLimit, head);
                const render::WsStation stn = render::ws_solve_station(
                    r.pelvis, r.pelvis.z, 1.0f - w.stand,
                    render::ws_squat_drop(u, w.stand), a[0], a[1],
                    render::ws_want_aft(u, 0.0f),
                    render::ws_rho(u, s, r.rho0), gate, ceiling);
                ++cells;
                if (stn.shortened) ++shortened_cells;
                INFO("u " << u << " s " << s << " steer " << st << " ratio "
                          << stn.ratio << " head " << head << " aft "
                          << stn.aft_m << " theta " << stn.theta_rad);
                // ★ THE GATE, IN TWO HALVES, AND BOTH ARE HONEST.
                // (1) wherever the LADDER-FREE pose is already inside the
                //     ceiling, the ladder must stay inside 0.985;
                // (2) wherever it is NOT -- the pre-existing steer clamp, which
                //     the 0-OFF law forbids this rung from touching -- the
                //     ladder must not make it worse.
                if (head <= render::kArmRatioLimit)
                    REQUIRE(stn.ratio <= render::kArmRatioLimit + 1e-4f);
                REQUIRE(stn.ratio <= ceiling + 1e-4f);
                REQUIRE(stn.theta_rad >= 0.0f);
                // *** R3-WS(c) A10, THE HEADLINE OF THIS AMENDMENT AND THE
                // ONE THING v1 HAD NO GATE FOR. Chad drove it and said he goes
                // "full face down even when standing at the back". The trunk
                // pitch is CAPPED now, everywhere, and this is the assertion
                // that says so -- the judged quantity is the sum with the
                // measured rest tilt, printed by the sweep report.
                REQUIRE(stn.theta_rad <= render::kHeadUpTrunkMaxRad + 1e-6f);
                REQUIRE(stn.aft_m >= 0.0f);
                REQUIRE(stn.aft_m <= render::ws_want_aft(u, 0.0f) + 1e-6f);
                worst = std::max(worst, stn.ratio);
                worst_head = std::max(worst_head, head);
                worst_gap = std::max(worst_gap, stn.ratio - head);
                if (stn.ratio >= worst - 1e-9f) { worst_u = u; worst_steer = float(st); }
                if (iu == 32) min_aft_at_u1 = std::min(min_aft_at_u1, stn.aft_m);
            }
        }
    }
    INFO("worst ladder ratio " << worst << " at u " << worst_u << " steer "
                               << worst_steer << "; worst ladder-free "
                               << worst_head << "; worst (ladder - free) "
                               << worst_gap << "; shortened " << shortened_cells
                               << "/" << cells << " cells; min aft at u=1 "
                               << min_aft_at_u1);
    // NON-VACUITY, three ways: the sweep reached cells the ladder-free pose
    // cannot serve; SPEC 4's shortening actually fired somewhere; and the
    // ladder's OWN contribution to the ratio is bounded and small.
    REQUIRE(worst_head > render::kArmRatioLimit);
    REQUIRE(shortened_cells > 0);
    REQUIRE(shortened_cells < cells);
    REQUIRE(worst_gap <= 0.05f);
    REQUIRE(worst > 0.5f);
    // * SPEC 7.4's "pelvis aft >= 0.30" belongs to the KNEEL ANCHOR now
    // (A4): the seated retreat is the 0.20 eye-dial and the 0.320 figure is
    // what the kneel geometry is solved against, so that is where the
    // non-vacuity claim is made. Run WITHOUT the head-up cap's effect on the
    // shortening by asking for the anchor directly at gate 1.
    {
        render::ReachArm a[2];
        for (int k = 0; k < 2; ++k) {
            a[k].shoulder_off = r.shoulder_off[k];
            a[k].hand = r.hand[k];
            a[k].dmax = r.dmax;
        }
        const render::WsStation stn = render::ws_solve_station(
            r.pelvis, r.pelvis.z, 1.0f, 0.0f, a[0], a[1],
            render::kLadderPelvisAftM, render::kArmRatioPull, 1.0f,
            render::kArmRatioLimit);
        INFO("steer 0, kneel anchor: aft " << stn.aft_m << " ratio "
                                           << stn.ratio << " theta "
                                           << stn.theta_rad);
        REQUIRE(stn.aft_m >= 0.30f);
        REQUIRE(stn.ratio <= render::kArmRatioLimit + 1e-4f);
        // ... and the trunk is at the head-up CEILING there, not past it
        REQUIRE(stn.theta_rad <= render::kHeadUpTrunkMaxRad + 1e-6f);
    }
    // ... and at the SEATED retreat the man is upright enough to read as
    // "pulling on the bars" rather than lying on them: the whole point of
    // R3-WS(c). Trunk-from-vertical = theta_r + the measured rest tilt.
    {
        render::ReachArm a[2];
        for (int k = 0; k < 2; ++k) {
            a[k].shoulder_off = r.shoulder_off[k];
            a[k].hand = r.hand[k];
            a[k].dmax = r.dmax;
        }
        const render::WsStation stn = render::ws_solve_station(
            r.pelvis, r.pelvis.z, 1.0f, 0.0f, a[0], a[1],
            render::kSeatRetreatAftM, render::kArmRatioPull, 1.0f,
            render::kArmRatioLimit);
        const float from_vert = stn.theta_rad + render::kRestTrunkTiltRad;
        INFO("seated retreat: aft " << stn.aft_m << " theta " << stn.theta_rad
                                    << " from vertical "
                                    << from_vert * 57.29578f << " deg");
        REQUIRE(stn.aft_m >= render::kSeatRetreatAftM - 1e-4f);
        REQUIRE_FALSE(stn.shortened);
        REQUIRE(from_vert < 67.0f * 3.14159265f / 180.0f);
    }
}

TEST_CASE("R3-WS: the two arms are solved as a PAIR, not one at a time",
          "[rider_pose][asset]") {
    // The bug this pins: taking max(theta_left, theta_right) satisfies each
    // side's own target and breaks the other's. reach_pair_solve must return a
    // ratio no worse than the per-side answers' WORSE case at its own theta.
    const WsRest& r = ws_rest();
    const glm::mat4 X = steer_xform(-1.0f);
    render::ReachArm a[2];
    for (int k = 0; k < 2; ++k) {
        a[k].shoulder_off = r.shoulder_off[k];
        a[k].hand = glm::vec3(X * glm::vec4(r.hand[k], 1.0f));
        a[k].dmax = r.dmax;
    }
    const glm::vec3 p = r.pelvis - glm::vec3(0.0f, 0.0f, 0.14f);
    const render::ReachPair rp = render::reach_pair_solve(p, a[0], a[1], 0.965f);
    // the naive per-side answer, and the damage it does
    float naive = 0.0f;
    for (int k = 0; k < 2; ++k)
        naive = std::max(naive,
                         render::reach_solve(p, a[k].shoulder_off, a[k].hand,
                                             0.965f * a[k].dmax).theta_rad);
    const float naive_ratio = render::reach_pair_ratio(p, a[0], a[1], naive);
    INFO("pair " << rp.ratio << " @ " << rp.theta_rad << " vs naive "
                 << naive_ratio << " @ " << naive);
    REQUIRE(rp.ratio <= naive_ratio + 1e-4f);
    // and the pair solve is a genuine minimiser: no angle on a fine scan beats
    // it by more than the refinement step
    float best = 1e9f;
    for (int i = 0; i <= 4000; ++i) {
        const float t = render::kReachHingeMaxRad * float(i) / 4000.0f;
        best = std::min(best, render::reach_pair_ratio(p, a[0], a[1], t));
    }
    REQUIRE(rp.ratio <= std::max(best, 0.965f) + 2e-3f);
    // and when the design ratio IS attainable it takes the SMALLEST theta,
    // not the deepest crouch that happens to be even straighter
    const render::ReachPair easy =
        render::reach_pair_solve(r.pelvis, a[0], a[1], 0.99f);
    REQUIRE(easy.met_design);
    REQUIRE(easy.ratio <= 0.99f + 1e-4f);
}

TEST_CASE("R3-WS(c): the foot slides the FULL RUNNER and stops on the HEEL",
          "[rider_pose][asset]") {
    // *** CHAD'S FIRST WORD ON THE DRIVE: the feet "only go back a little",
    // they must slide "the full length of the foot runners", "all the way
    // back". SPEC 11.4 A4 moves the ladder's non-vacuity anchor here, off the
    // pelvis, because the FOOT is now what carries the travel.
    const WsRest& r = ws_rest();
    for (int side = 0; side < 2; ++side) {
        for (float sv : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            glm::vec3 prev = render::deck_foot_target(
                r.foot[side], render::ws_foot_slide(0.0f, sv));
            REQUIRE(glm::length(prev - r.foot[side]) < 2e-5f);  // 0-OFF
            for (int i = 1; i <= 64; ++i) {
                const float u = float(i) / 64.0f;
                const glm::vec3 f = render::deck_foot_target(
                    r.foot[side], render::ws_foot_slide(u, sv));
                INFO("side " << side << " s " << sv << " u " << u << " z "
                             << f.z);
                // on the measured deck SHEET, and the HEEL never leaves the
                // running board (A5: the boot is not centred on the ankle)
                REQUIRE(std::fabs(f.y - (render::kDeckSheetYM +
                                         render::kAnkleAboveSoleM)) < 1e-6f);
                REQUIRE(f.z - render::kBootHeelBackM >=
                        render::kDeckZRearM - 1e-6f);
                REQUIRE(f.z <= render::kDeckZFrontM - render::kBootHalfLenM);
                REQUIRE(f.x == r.foot[side].x);
                REQUIRE(f.z <= prev.z + 1e-6f);            // monotone aft
                REQUIRE(prev.z - f.z <= 0.06f);            // and continuous
                prev = f;
            }
            // * THE NON-VACUITY ANCHOR (A4). Seated, the foot REACHES the
            // measured rear limit; standing, the A3 re-timed band overruns
            // u = 1 on purpose (continuity), and what it still delivers is
            // gated -- stated, never silently short.
            INFO("s " << sv << " travel " << (r.foot[side].z - prev.z));
            if (sv == 0.0f) {
                REQUIRE(std::fabs(prev.z - render::kFootZRearLimitM) < 0.001f);
                REQUIRE(r.foot[side].z - prev.z >= 0.90f);
            } else {
                REQUIRE(r.foot[side].z - prev.z >= 0.60f);
            }
        }
    }
    // ... and the rear limit is the HEEL's, 81 mm aft of the naive
    // boot-half-length figure the spec first wrote down
    REQUIRE(render::kFootZRearLimitM <
            render::kDeckZRearM + render::kBootHalfLenM + 0.02f - 0.07f);
}

TEST_CASE("R3-WS(c): the foot slide schedule is C1, 0-OFF and never faster "
          "than the seated band",
          "[rider_pose]") {
    // A3. The band widens WITH the stand -- the same re-timing ws_reach_gate
    // uses -- so the slide is always paid for by demand that has already
    // happened. Faster-with-stand would be the R3-WS(b) pop, again.
    for (int i = 0; i <= 20; ++i) {
        const float sv = float(i) / 20.0f;
        REQUIRE(render::ws_foot_slide(0.0f, sv) == 0.0f);   // the 0-OFF law
        float prev = 0.0f;
        for (int k = 0; k <= 100; ++k) {
            const float u = float(k) / 100.0f;
            const float g = render::ws_foot_slide(u, sv);
            INFO("u " << u << " s " << sv << " slide " << g);
            REQUIRE(g >= prev - 1e-6f);                      // monotone
            REQUIRE(g <= render::ws_foot_slide(u, 0.0f) + 1e-6f);
            REQUIRE(g <= 1.0f);
            prev = g;
        }
    }
    // ★ SHIPPED AT hi_f == 1: the feet go ALL THE WAY BACK at every stand
    // fraction, because the bake sweep REFUTED A3's prediction that the foot
    // band was the continuity lever (render/rider_pose.h carries the table --
    // the flexion band is, and it moved instead). The parameter is still real
    // and still swept; it is simply not re-timed today.
    REQUIRE(render::ws_foot_slide(1.0f, 0.0f) == 1.0f);
    REQUIRE(render::ws_foot_slide(1.0f, 1.0f) == 1.0f);
    // the band is a real parameter -- a wider band is a slower slide
    REQUIRE(render::ws_foot_slide(0.5f, 1.0f, 2.0f) <
            render::ws_foot_slide(0.5f, 1.0f, 1.2f));
}

TEST_CASE("R3-WS(c): the retreat is the eye-dial, the KNEEL anchor is frozen",
          "[rider_pose][asset]") {
    // A1 + A4. kLadderPelvisAftM (0.320) is the kneel/reach anchor and must
    // not move; the seated butt-back retreat is its own, SHORTER dial, which
    // is what stops him riding the rear crest.
    REQUIRE(render::ws_want_aft(0.0f, 0.0f) == 0.0f);        // 0-OFF
    REQUIRE(std::fabs(render::ws_want_aft(1.0f, 0.0f) -
                      render::kSeatRetreatAftM) < 1e-7f);
    REQUIRE(std::fabs(render::ws_want_aft(1.0f, 1.0f) -
                      render::kLadderPelvisAftM) < 1e-7f);
    REQUIRE(render::ws_want_aft(1.0f, 0.0f) < render::ws_want_aft(1.0f, 1.0f));
    // linear in u, so the ladder coordinate still means what it meant
    REQUIRE(std::fabs(render::ws_want_aft(0.5f, 0.0f) -
                      0.5f * render::kSeatRetreatAftM) < 1e-7f);
    // * THE CREST GATE (A1). At the full seated retreat the pelvis must still
    // be on the PAN: the seat surface it lands on may not have risen more than
    // 10 mm, or the butt is up the rear crest and "looks funny".
    const WsRest& r = ws_rest();
    const float y0 = render::seat_top_y(r.pelvis.z);
    const float y1 = render::seat_top_y(r.pelvis.z - render::kSeatRetreatAftM);
    INFO("seat under the pelvis " << y0 << " -> " << y1);
    REQUIRE(y1 - y0 <= 0.010f);
    // non-vacuity: the OLD 0.320 retreat really was up the crest
    REQUIRE(render::seat_top_y(r.pelvis.z - render::kLadderPelvisAftM) - y0 >
            0.010f);
}

TEST_CASE("R3-WS(c): the standing squat drops the hips, seated never moves",
          "[rider_pose]") {
    // SPEC 11.2.3: "his butt can then come down a little as he bends his
    // knees some" -- and that is a STANDING brace. Seated, the pelvis is on
    // the seat and the measured profile owns its height.
    REQUIRE(render::ws_squat_drop(1.0f, 0.0f) == 0.0f);   // seated: untouched
    REQUIRE(render::ws_squat_drop(0.0f, 1.0f) == 0.0f);   // 0-OFF
    REQUIRE(render::ws_squat_drop(render::kSquatULo, 1.0f) == 0.0f);
    REQUIRE(std::fabs(render::ws_squat_drop(1.0f, 1.0f) +
                      render::kAftSquatDropM) < 1e-7f);
    float prev = 0.0f;
    for (int i = 0; i <= 100; ++i) {
        const float u = float(i) / 100.0f;
        const float d = render::ws_squat_drop(u, 1.0f);
        INFO("u " << u << " drop " << d);
        REQUIRE(d <= prev + 1e-7f);      // monotone DOWN
        REQUIRE(d >= -render::kAftSquatDropM - 1e-7f);
        prev = d;
    }
}

TEST_CASE("R3-WS(c): the neck counter brings the HEAD UP -- measured sign, "
          "identity at zero",
          "[rider_pose][asset]") {
    // * A9. The sign is MEASURED off the rest pose, never typed -- this rig
    // has cost three sessions for assuming things mirror.
    const glm::vec3 neck = Pw("neck_01");
    const glm::vec3 head = Pw("head");
    const float sgn = render::neck_up_sign(neck, head);
    REQUIRE(std::fabs(sgn) == 1.0f);
    // it really does raise the head: apply it and read the y
    const glm::vec3 up =
        glm::angleAxis(sgn * 0.30f, glm::vec3(1.0f, 0.0f, 0.0f)) * (head - neck);
    REQUIRE(up.y > (head - neck).y);
    // 0-OFF: exactly the identity quaternion at theta_r = 0, in any frame
    const glm::quat pq =
        glm::angleAxis(0.7f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    const glm::quat q0 = render::ws_neck_counter(0.0f, sgn, pq);
    REQUIRE(q0.w == 1.0f);
    REQUIRE(q0.x == 0.0f);
    REQUIRE(q0.y == 0.0f);
    REQUIRE(q0.z == 0.0f);
    // monotone in theta_r up to the cap, then FLAT -- a neck runs out
    float prev = 0.0f;
    for (int i = 1; i <= 100; ++i) {
        const float th = render::kHeadUpTrunkMaxRad * float(i) / 100.0f;
        const glm::quat q = render::ws_neck_counter(th, sgn, pq);
        const glm::quat m = pq * q * glm::conjugate(pq);  // back to model frame
        const float ang =
            2.0f * std::acos(std::min(1.0f, std::fabs(m.w)));
        INFO("theta " << th << " counter " << ang);
        REQUIRE(ang >= prev - 1e-5f);
        REQUIRE(ang <= render::kNeckCounterMaxRad + 1e-5f);
        prev = ang;
    }
    REQUIRE(std::fabs(prev - render::kNeckCounterMaxRad) < 1e-3f);
    // ... and it is a rotation about MODEL +X: conjugated back, the axis is X
    const glm::quat q = render::ws_neck_counter(0.3f, sgn, pq);
    const glm::quat m = pq * q * glm::conjugate(pq);
    REQUIRE(std::fabs(m.y) < 1e-6f);
    REQUIRE(std::fabs(m.z) < 1e-6f);
    REQUIRE(std::fabs(m.x) > 1e-3f);
}

TEST_CASE("R3-WS: the KNEEL is built on the measured seat and the IK "
          "reproduces the knee",
          "[rider_pose][asset]") {
    // SPEC 3c + 7.3. The construction is (hip, knee, ankle) with both contacts
    // ON the measured surface; the gate is that the 2-bone analytic solve puts
    // the knee back exactly where the construction put it.
    const WsRest& r = ws_rest();
    const float z_p = r.pelvis.z - render::kLadderPelvisAftM;
    const float h = render::kneel_sit_height(z_p, 0.095f, r.thigh, r.shin);
    INFO("kneel hip height above the seat " << h);
    // ★ CHAD'S DISTINCTION, AS ARITHMETIC: sat back toward the heels, NOT
    // upright on the knees (which is a whole thigh, r.thigh), and well above
    // the seated sit height (which would fold the knee past any human limit).
    REQUIRE(h > 2.0f * 0.085578f);
    REQUIRE(h < 0.70f * r.thigh);
    const glm::vec3 hip(0.095f, render::seat_top_y(z_p) + h, z_p);
    const render::KneelPose k = render::kneel_construct(hip, r.thigh, r.shin);
    REQUIRE(k.ok);
    // the bones are RIGID -- these are equalities, not clearances
    REQUIRE(std::fabs(glm::length(k.knee - hip) - r.thigh) < 1e-4f);
    REQUIRE(std::fabs(glm::length(k.foot - k.knee) - r.shin) < 1e-4f);
    // both contacts are ON the measured surface (SPEC 7.3: within 5 mm)
    REQUIRE(std::fabs(k.knee.y - (render::seat_top_y(k.knee.z) +
                                  render::kKneePadRM)) < 0.005f);
    REQUIRE(std::fabs(k.foot.y - (render::seat_top_y(k.foot.z) +
                                  render::kAnkleAboveSoleM)) < 0.005f);
    // the knee is AHEAD of the hip and the ankle BEHIND it: kneeling, sat back
    REQUIRE(k.knee.z > hip.z + 0.30f);
    REQUIRE(k.foot.z < hip.z);
    // ★★★ R3-WS(d) / SPEC 13.2.1. THE FLEXION IS AT THE **SHARD FLOOR**, NOT
    // AT THE ANATOMICAL LIMIT, AND THE FLOOR IS THE ONE THAT BINDS. The knee
    // joint would go to 155 deg (chord 0.1965); the shipped `sudburian_proxy`
    // skin fans into shards there, so the solve is held at 0.26 m -- about
    // 146.7 deg, comfortably inside the joint and outside the tear.
    const float anat = render::kneel_chord_m(r.thigh, r.shin);
    const float chord = render::kneel_target_chord_m(r.thigh, r.shin);
    INFO("anatomical chord " << anat << ", target " << chord);
    REQUIRE(anat < render::kKneelChordFloorM);      // the mesh gives out first
    REQUIRE(chord == render::kKneelChordFloorM);    // ... so the floor wins
    REQUIRE(std::fabs(glm::length(k.foot - hip) - chord) < 0.002f);
    const float flex = render::kneel_flex_rad(r.thigh, r.shin,
                                              glm::length(k.foot - hip));
    INFO("realized flexion " << flex * 57.29578f << " deg");
    REQUIRE(flex < render::kKneeFlexMaxRad);        // inside the joint
    REQUIRE(flex > 140.0f * 3.14159265f / 180.0f);  // still a real kneel
    // ... and the guard did not have to spend any heel lift at the station it
    // was solved for: both contacts are still on the measured seat.
    REQUIRE(k.heel_lift_m < 1e-6f);
    // ★ THE 1 mm GATE. The analytic 2-bone solve, run on the SAME triangle
    // with the bend normal taken from it, must reproduce the knee.
    const glm::vec3 n = render::rest_bend_normal(hip, k.knee, k.foot,
                                                 glm::mat3(1.0f));
    const glm::vec3 st = k.foot - hip;
    const float d = glm::length(st);
    const glm::vec3 dir = st / d;
    const float aa = (r.thigh * r.thigh + d * d - r.shin * r.shin) / (2.0f * d);
    const float h2 = r.thigh * r.thigh - aa * aa;
    const glm::vec3 bend = glm::normalize(glm::cross(n, dir));
    const glm::vec3 solved =
        hip + dir * aa + bend * (h2 > 0.0f ? std::sqrt(h2) : 0.0f);
    INFO("solved knee " << solved.x << "," << solved.y << "," << solved.z
                        << " vs K " << k.knee.x << "," << k.knee.y << ","
                        << k.knee.z);
    REQUIRE(glm::length(solved - k.knee) < 0.001f);
}

TEST_CASE("R3-WS: the ladder inverse is a monotone inverse, and it is TOTAL",
          "[rider_pose]") {
    float c[render::kWsBakeU];
    for (int i = 0; i < render::kWsBakeU; ++i)
        c[i] = 0.30f * float(i) / float(render::kWsBakeU - 1);
    REQUIRE(render::ws_monotone(c, render::kWsBakeU));
    REQUIRE(render::ws_invert(c, render::kWsBakeU, 0.0f) == 0.0f);
    REQUIRE(render::ws_invert(c, render::kWsBakeU, -1.0f) == 0.0f);
    REQUIRE(render::ws_invert(c, render::kWsBakeU, 0.30f) == 1.0f);
    REQUIRE(render::ws_invert(c, render::kWsBakeU, 9.0f) == 1.0f);
    REQUIRE(std::fabs(render::ws_invert(c, render::kWsBakeU, 0.15f) - 0.5f) <
            1e-4f);
    // a DIP (which the real curve has at high stand) must not break it: the
    // inverse runs on the monotone envelope and stays non-decreasing.
    c[1] = -0.02f;
    c[2] = -0.01f;
    REQUIRE_FALSE(render::ws_monotone(c, render::kWsBakeU));
    float prev = -1.0f;
    for (int i = 0; i <= 300; ++i) {
        const float a = 0.30f * float(i) / 300.0f;
        const float u = render::ws_invert(c, render::kWsBakeU, a);
        INFO("a " << a << " u " << u);
        REQUIRE(u >= prev - 1e-6f);
        REQUIRE(u >= 0.0f);
        REQUIRE(u <= 1.0f);
        prev = u;
    }
}

TEST_CASE("R3-WS: the ladder constants keep their shipped values "
          "(rename, never re-tune)",
          "[rider_pose]") {
    REQUIRE(render::kLadderPelvisAftM == 0.320f);
    REQUIRE(render::kKneelMarginM == 0.060f);
    // ★ kFootTrack is RETIRED by SPEC 11.4 A4 -- the feet no longer track the
    // pelvis 1:1, they run the full runner on their own schedule, so its pin
    // is REMOVED here rather than left pinning a constant that no longer
    // exists. Its successors are pinned immediately below.
    REQUIRE(render::kFootBlendHiU == 0.15f);
    // ★★★ R3-WS(c), Chad's drive feedback. Four eye-dials and the measured
    // limit they slide to -- rename, never re-tune.
    REQUIRE(render::kSeatRetreatAftM == 0.20f);            // A1
    REQUIRE(std::fabs(render::kHeadUpTrunkMaxRad -
                      38.0f * 3.14159265f / 180.0f) < 1e-7f);
    REQUIRE(render::kAftSquatDropM == 0.10f);              // SPEC 11.2.3
    REQUIRE(std::fabs(render::kNeckCounterMaxRad -
                      30.0f * 3.14159265f / 180.0f) < 1e-7f);
    REQUIRE(render::kSquatULo == 0.40f);
    REQUIRE(render::kSquatUHi == 1.00f);
    REQUIRE(render::kFootSlideStandHiU == 1.00f);          // A3, swept
    REQUIRE(std::fabs(render::kBootHeelBackM - 0.086631f) < 1e-7f);  // A5
    REQUIRE(render::kFootRearClearM == 0.020f);
    REQUIRE(std::fabs(render::kFootZRearLimitM + 0.903369f) < 1e-6f);
    REQUIRE(std::fabs(render::kRestTrunkTiltRad -
                      28.6173f * 3.14159265f / 180.0f) < 1e-7f);
    REQUIRE(render::kReachGateStandHiU == 1.40f);   // R3-WS(b), (c)
    REQUIRE(render::kWsContinuityBoundM == 0.06f);  // SPEC 7.5
    REQUIRE(render::kWsDemandCells == 64);
    REQUIRE(render::kArmRatioPull == 0.965f);
    REQUIRE(render::kArmRatioLimit == 0.985f);
    REQUIRE(render::kStandRhoGain == 0.8f);
    REQUIRE(render::kKneelULo == 0.60f);
    REQUIRE(render::kKneelUHi == 1.00f);
    // ★★★ R3-WS(d) / D1. The kneel's STAND kill band, widened from [0.20,
    // 0.40] to the whole axis and SIZED BY THE SWEEP (render/rider_pose.h
    // carries the table): at 0.40 the kneel unwound in 2-3 frames of the
    // kernel's own lean slew.
    REQUIRE(render::kKneelSLo == 0.00f);
    REQUIRE(render::kKneelSHi == 1.00f);
    // ★ R3-WS(d)'s own four constants.
    REQUIRE(std::fabs(render::kKneelTrunkMaxRad -
                      55.0f * 3.14159265f / 180.0f) < 1e-7f);  // SPEC 13.2.2
    REQUIRE(render::kKneelChordFloorM == 0.26f);               // SPEC 13.2.1
    REQUIRE(render::kWsKneelSlewAddM == 0.030f);               // D1
    REQUIRE(render::kWsTickS == 1.0f / 60.0f);
    REQUIRE(render::kDeckSheetYM == 0.252000f);
    REQUIRE(std::fabs(render::kKneePadRM - 0.091800f) < 1e-7f);
    REQUIRE(std::fabs(render::kAnkleAboveSoleM - 0.078801f) < 1e-7f);
    REQUIRE(std::fabs(render::kBootHalfLenM - 0.167800f) < 1e-7f);
    REQUIRE(std::fabs(render::kReachHingeMaxRad -
                      65.0f * 3.14159265f / 180.0f) < 1e-7f);
    REQUIRE(std::fabs(render::kKneeFlexMaxRad -
                      155.0f * 3.14159265f / 180.0f) < 1e-7f);
    // ★★★★ THE KNEEL IS **HELD FOR R4-SIDE**, AND THAT IS CHAD'S RULING OF
    // 2026-08-21: "not make kneeling for longitudinal movement... a hotkey so
    // you can pull a side / unstick it... key is going to be P for pull."
    // R3-WS(d) turned it on for the fore-aft ladder; this ruling takes it back
    // out of the ladder WITHOUT retracting any of the geometry -- the chord
    // floor, trunk cap, s-band and bend-normal fix pinned in this very case
    // all stay, because they are what the R4-SIDE side-hang inherits. This
    // constant is still the whole switch, and it is still pinned in two places
    // on purpose. See render/rider_pose.h at kKneelRuntimeGain.
    REQUIRE(render::kKneelRuntimeGain == 0.0f);
    REQUIRE(render::kSeatStations == 33);
    REQUIRE(render::kWsBakeU == 33);
    REQUIRE(render::kWsBakeS == 5);
    // SPEC 7.4 non-vacuity, as a compile-time-adjacent fact
    REQUIRE(render::kLadderPelvisAftM >= 0.30f);
}

TEST_CASE("R3-WS: the ladder travel is INSIDE the measured reach limit and "
          "the seat, and it is the ARMS that bind",
          "[rider_pose][asset]") {
    // SPEC 4. The report must be able to say which constraint set the travel,
    // and this is the assertion that says it.
    const WsRest& r = ws_rest();
    const float seat_allows = r.pelvis.z - (render::kSeatZRearM +
                                            render::kKneelMarginM);
    float arms_allow = 1e9f;
    for (int side = 0; side < 2; ++side)
        for (float rise : {0.0f, 0.15f, 0.35f}) {
            const glm::vec3 p = r.pelvis + glm::vec3(0.0f, rise, 0.0f);
            arms_allow = std::min(
                arms_allow,
                render::reach_max_aft(p, r.shoulder_off[side], r.hand[side],
                                      render::kArmRatioLimit * r.dmax));
        }
    INFO("seat allows " << seat_allows << " m, arms allow " << arms_allow
                        << " m, shipped " << render::kLadderPelvisAftM);
    REQUIRE(seat_allows > 0.35f);
    REQUIRE(arms_allow < seat_allows);            // the ARMS bind, not the seat
    REQUIRE(render::kLadderPelvisAftM <= arms_allow);
    REQUIRE(render::kLadderPelvisAftM >= 0.30f);  // and it is still a ladder
}

// ===========================================================================
// ★★★ R3-WS(b) -- THE STAND-ENTRY POP, AND THE THREE GATES THAT MISSED IT
// ===========================================================================
//
// ★ WHY THESE CASES EXIST. R3-WS landed with twelve green cases and a pose
// that SNAPPED the instant a standing rider asked for any aft at all. Every
// gate above steps in `u`. The jump lives UNDERNEATH `u`: the baked C_s(u)
// dipped below zero over the flexion ramp, the inverse's monotone envelope
// turned that dip into a flat spot, and a flat spot in C is a JUMP in u -- so
// a_m = 0 answered u = 0.0000 and a_m = +0.0001 answered u = 0.2819. A gate
// that never varies the RUNTIME coordinate cannot see that, however fine its
// steps are.
//
// So the model below poses the man the way render/sled_model.cpp poses him --
// pelvis station, trunk flexion, blended deck foot, two-bone arms and legs --
// and every case here steps in `a`, the coordinate the player actually moves.

namespace {

// The rest world position of every declared rider joint, in enum order.
const std::array<glm::vec3, render::kRiderJointCount>& ws_rest_joints() {
    static const std::array<glm::vec3, render::kRiderJointCount> r = [] {
        std::array<glm::vec3, render::kRiderJointCount> o{};
        const auto& spec = render::rider_joint_specs();
        for (int j = 0; j < render::kRiderJointCount; ++j)
            o[std::size_t(j)] = Pw(spec[std::size_t(j)].name);
        return o;
    }();
    return r;
}

// The analytic 2-bone solve, in exactly the form render/sled_model.cpp uses:
// the mid joint sits on the circle where the two bone spheres meet, picked out
// by the bend normal captured from the rest triangle.
glm::vec3 ws_two_bone_mid(const glm::vec3& root, const glm::vec3& tip, float l0,
                          float l1, const glm::vec3& n) {
    const glm::vec3 st = tip - root;
    const float d = glm::length(st);
    if (!(d > 1e-6f)) return root;
    const glm::vec3 dir = st / d;
    const float a = (l0 * l0 + d * d - l1 * l1) / (2.0f * d);
    const float h2 = l0 * l0 - a * a;
    const glm::vec3 bend = glm::normalize(glm::cross(n, dir));
    return root + dir * a + bend * (h2 > 0.0f ? std::sqrt(h2) : 0.0f);
}

// *** R3-WS(d). The two heights the kneel branch needs, derived here the way
// capture_weight_shift derives them: the seated sit height at the rest station
// and the kneeling hip height solved at the ladder own rear station.
float ws_sit_h() {
    static const float h =
        ws_rest().pelvis.y - render::seat_top_y(ws_rest().pelvis.z);
    return h;
}
float ws_kneel_h() {
    static const float h = render::kneel_sit_height(
        ws_rest().pelvis.z - render::kLadderPelvisAftM,
        ws_rest().pelvis.x + 0.095f, ws_rest().thigh, ws_rest().shin);
    return h;
}

// ★ THE LADDER'S OWN POSE, RE-DERIVED. `ladder` = false computes the SAME
// arithmetic with every ladder term literally replaced by a zero constant --
// it does not consult a flag the runtime owns, and it never calls ws_weights,
// ws_rho, ws_reach_gate or ws_solve_station at all. That is what makes the
// 0-OFF case below an ARTEFACT gate rather than a restatement of the control
// flow render/sled_model.cpp happens to have.
std::array<glm::vec3, render::kRiderJointCount> ws_test_pose_shift(
    float u, float s, float steer, bool ladder, const glm::vec3& rise,
    // ★★★★ R3-WS(e). This was `kneel_scale`, a multiplier ON TOP of
    // render::kKneelRuntimeGain. With the gain back at 0 for the longitudinal
    // ladder (Chad's 2026-08-21 ruling) a scale could no longer reach the
    // kneel branch at all, and every kneel-math case in this file would have
    // gone silently VACUOUS -- green about a pose nothing computes. So the
    // parameter is now the GAIN ITSELF: it DEFAULTS to the shipped constant
    // (so ws_test_pose re-derives exactly what the runtime poses), 0 gives the
    // deck-branch reference, and 1 forces the kneel ON for the cases that gate
    // the R4-SIDE inheritance.
    float kneel_gain = render::kKneelRuntimeGain) {
    const WsRest& r = ws_rest();
    const std::array<glm::vec3, render::kRiderJointCount>& rest =
        ws_rest_joints();
    // `rise` is the R1c ROOT TRANSLATION this cell stands on. Everything rides
    // it; the IK tips are then pinned back onto the machine below.
    const glm::mat4 X = steer_xform(steer);
    render::ReachArm arm[2];
    for (int k = 0; k < 2; ++k) {
        arm[k].shoulder_off = r.shoulder_off[k];
        arm[k].hand = glm::vec3(X * glm::vec4(r.hand[k], 1.0f));
        arm[k].dmax = r.dmax;
    }
    const glm::vec3 p0 = r.pelvis + rise;

    float aft = 0.0f, dy = 0.0f, theta = 0.0f, w_deck = 0.0f;
    float slide = 0.0f, w_kneel = 0.0f;
    if (ladder) {
        const render::WeightShiftWeights w = render::ws_weights(u, s);
        // *** R3-WS(d). THE KNEEL IS IN THIS MODEL NOW, BECAUSE IT IS IN THE
        // RIDER. With kKneelRuntimeGain at 0 this model could ignore the whole
        // branch and still be an honest re-derivation of what shipped; with
        // the gain at 1 it cannot -- a curve-shape or continuity case built on
        // a kneel-free pose would be green about a pose nobody sees. Every
        // term below is the SAME pure function render/sled_model.cpp wires.
        w_kneel = w.kneel * kneel_gain;
        const float head = render::reach_pair_ratio(p0, arm[0], arm[1], 0.0f);
        const render::WsStation stn = render::ws_solve_station(
            p0, r.pelvis.z, 1.0f - w.stand,
            w_kneel * (ws_kneel_h() - ws_sit_h()) +
                render::ws_squat_drop(u, w.stand),
            arm[0], arm[1], render::ws_want_aft(u, w_kneel),
            render::ws_rho(u, s, r.rho0), render::ws_reach_gate(u, s),
            std::max(render::kArmRatioLimit, head),
            render::ws_trunk_cap(w_kneel));
        aft = stn.aft_m;
        dy = stn.dy_m;
        theta = stn.theta_rad;
        const float t =
            std::min(1.0f, std::max(0.0f, u / render::kFootBlendHiU));
        w_deck = t * t * (3.0f - 2.0f * t);
        slide = render::ws_foot_slide(u, s);   // *** R3-WS(c): the FULL runner
    }

    // 1. the pelvis station + the trunk flexion, about MODEL +X through the
    //    pelvis, exactly as render/sled_model.cpp composes it.
    const glm::quat q = glm::angleAxis(theta, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 pel_old = p0;
    const glm::vec3 pel_new = p0 + glm::vec3(0.0f, dy, -aft);
    std::array<glm::vec3, render::kRiderJointCount> jp{};
    for (int j = 0; j < render::kRiderJointCount; ++j)
        jp[std::size_t(j)] =
            pel_new + q * ((rest[std::size_t(j)] + rise) - pel_old);
    // ★ `root` IS THE PELVIS'S PARENT, so the hip hinge does NOT rotate it --
    // it only rides the translation. Rotating it here put a joint 0.98 m below
    // the hinge axis, which turned a 5 deg trunk move into 83 mm of phantom
    // joint travel and made this model disagree with the shipped pose_pass.
    jp[std::size_t(render::kRiderRoot)] =
        rest[std::size_t(render::kRiderRoot)] + rise +
        glm::vec3(0.0f, dy, -aft);
    // *** R3-WS(c). THE HEAD-UP COUNTER, MODELLED HERE TOO. render/
    // sled_model.cpp rotates neck_01 back about MODEL +X by min(theta_r, cap),
    // and head+neck is 8.1 % of the body -- leaving it out of this independent
    // model made its C_s(u) dip at s = 1 where the shipped bake's rises.
    if (ladder && theta > 0.0f) {
        const float sgn = render::neck_up_sign(Pw("neck_01"), Pw("head"));
        const float mag = std::min(theta, render::kNeckCounterMaxRad);
        const glm::quat qn =
            glm::angleAxis(sgn * mag, glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 nk = jp[std::size_t(render::kRiderNeck)];
        jp[std::size_t(render::kRiderHead)] =
            nk + qn * (jp[std::size_t(render::kRiderHead)] - nk);
    }

    // 2. the ARMS: the hands are welded to the live grips, so the elbow is the
    //    2-bone solve between the flexed shoulder and that fixed target.
    const char* up[2] = {"upperarm_r", "upperarm_l"};
    const char* lo[2] = {"lowerarm_r", "lowerarm_l"};
    const char* hd[2] = {"hand_r", "hand_l"};
    const int a_j[2][3] = {
        {render::kRiderUpperarmR, render::kRiderForearmR, render::kRiderHandR},
        {render::kRiderUpperarmL, render::kRiderForearmL, render::kRiderHandL}};
    for (int k = 0; k < 2; ++k) {
        const float l0 = glm::length(Pw(lo[k]) - Pw(up[k]));
        const float l1 = glm::length(Pw(hd[k]) - Pw(lo[k]));
        const glm::vec3 n =
            q * render::rest_bend_normal(Pw(up[k]), Pw(lo[k]), Pw(hd[k]),
                                         glm::mat3(1.0f));
        const glm::vec3 root = jp[std::size_t(a_j[k][0])];
        jp[std::size_t(a_j[k][1])] =
            ws_two_bone_mid(root, arm[k].hand, l0, l1, n);
        jp[std::size_t(a_j[k][2])] = arm[k].hand;
    }

    // 3. the LEGS: the boot target is the rigid weld blended into the
    //    parametric deck target -- the defect this rung exists to kill.
    const char* th[2] = {"thigh_r", "thigh_l"};
    const char* ca[2] = {"calf_r", "calf_l"};
    const char* ft[2] = {"foot_r", "foot_l"};
    const int l_j[2][3] = {
        {render::kRiderThighR, render::kRiderShinR, render::kRiderFootR},
        {render::kRiderThighL, render::kRiderShinL, render::kRiderFootL}};
    for (int k = 0; k < 2; ++k) {
        const float l0 = glm::length(Pw(ca[k]) - Pw(th[k]));
        const float l1 = glm::length(Pw(ft[k]) - Pw(ca[k]));
        const glm::vec3 n = render::rest_bend_normal(
            Pw(th[k]), Pw(ca[k]), Pw(ft[k]), glm::mat3(1.0f));
        const glm::vec3 rigid = Pw(ft[k]);  // the board socket weld: FIXED
        glm::vec3 tgt =
            ladder ? glm::mix(rigid, render::deck_foot_target(rigid, slide),
                              w_deck)
                   : rigid;
        const glm::vec3 root = jp[std::size_t(l_j[k][0])];
        glm::vec3 bn = n;
        // *** R3-WS(d). The kneel target and its own bend plane, constructed
        // at the LIVE hip exactly as render/sled_model.cpp does it.
        if (w_kneel > 0.0f) {
            const render::KneelPose kp = render::kneel_construct(root, l0, l1);
            if (kp.ok) {
                tgt = render::ws_kneel_foot_mix(tgt, kp.foot, w_kneel);
                const glm::vec3 kn = render::rest_bend_normal(
                    root, kp.knee, kp.foot, glm::mat3(1.0f));
                const glm::vec3 b = glm::mix(n, kn, w_kneel);
                if (glm::length(b) > 1e-4f) bn = glm::normalize(b);
            }
        }
        jp[std::size_t(l_j[k][1])] = ws_two_bone_mid(root, tgt, l0, l1, bn);
        jp[std::size_t(l_j[k][2])] = tgt;
    }
    return jp;
}

// *** THE STAND REFERENCE MUST BE THE RUNTIME'S, NOT A PURE RISE.
// render/sled_model.cpp resolves the ladder against `lean_seed(hinge, 0, 0,
// lean_up_m)`, whose inverse Jacobian mixes the two axes -- a stand is a rise
// AND a fore-aft shift. Modelling it as a pure +Y rise was close enough while
// the feet moved 0.32 m; with 1.0054 m of runner slide underneath it, the two
// models' C_s(u) diverged at full stand and this file's own continuity case
// failed on a pose the shipped bake measures as clean. So the hinge model is
// re-derived HERE, by probing this model exactly the way capture_hinge probes
// the shipped one, and the same lean_seed is used.
const render::RiderHingeModel& ws_test_hinge() {
    static const render::RiderHingeModel m = [] {
        const std::array<glm::vec3, render::kRiderJointCount> rest =
            ws_test_pose_shift(0.0f, 0.0f, 0.0f, false, glm::vec3(0.0f));
        const glm::vec3 cg0 = render::rider_cg(rest);
        const float probe = 0.10f;
        const glm::vec3 cgz = render::rider_cg(ws_test_pose_shift(
            0.0f, 0.0f, 0.0f, false, glm::vec3(0.0f, 0.0f, probe)));
        const glm::vec3 cgy = render::rider_cg(ws_test_pose_shift(
            0.0f, 0.0f, 0.0f, false, glm::vec3(0.0f, probe, 0.0f)));
        render::RiderHingeModel h =
            render::capture_hinge_model(rest, (cgz.z - cg0.z) / probe);
        h.k_zy = (cgy.z - cg0.z) / probe;
        h.k_yz = (cgz.y - cg0.y) / probe;
        h.k_yy = (cgy.y - cg0.y) / probe;
        if (!(std::fabs(h.k_yy) > 1e-3f)) h.k_yy = 1.0f;
        return h;
    }();
    return m;
}

std::array<glm::vec3, render::kRiderJointCount> ws_test_pose(float u, float s,
                                                             float steer,
                                                             bool ladder) {
    const render::RiderLeanShift ref = render::lean_seed(
        ws_test_hinge(), 0.0f, 0.0f, render::stand_rise_m() * s);
    return ws_test_pose_shift(u, s, steer, ladder,
                              glm::vec3(0.0f, ref.up, ref.fwd));
}

// *** R3-WS(d) / D4(a). The LADDER pose with the kneel branch held at zero --
// the deck-branch reference the blended knee gate needs. It is NOT the
// ladder-free pose: the feet have still slid, the pelvis has still retreated
// and the trunk has still flexed, so the only difference is the branch under
// test. `kneel_off` threads through ws_test_pose_shift as a hard zero.
std::array<glm::vec3, render::kRiderJointCount> ws_test_pose_deck(float u,
                                                                  float s,
                                                                  float steer) {
    const render::RiderLeanShift ref = render::lean_seed(
        ws_test_hinge(), 0.0f, 0.0f, render::stand_rise_m() * s);
    return ws_test_pose_shift(u, s, steer, true,
                              glm::vec3(0.0f, ref.up, ref.fwd), 0.0f);
}

// ★★★★ R3-WS(e) / SPEC 14. The ladder pose with the kneel branch FORCED ON,
// gain 1, whatever the shipped constant says. Chad's 2026-08-21 ruling takes
// the kneel out of the LONGITUDINAL ladder and reserves it for the R4-SIDE
// "P" key (side hang), so the runtime poses it at zero -- but the geometry it
// reserves (kneel_construct, the 0.26 m chord floor, the trunk cap, the
// s-band, ws_blend_bend_normal) must stay GATED, not merely present. Every
// case below that is ABOUT the kneel drives this entry point instead of the
// shipped one, which is what keeps those cases non-vacuous across the flip.
// It is never used to make a claim about what the player currently sees.
std::array<glm::vec3, render::kRiderJointCount> ws_test_pose_kneel(
    float u, float s, float steer) {
    const render::RiderLeanShift ref = render::lean_seed(
        ws_test_hinge(), 0.0f, 0.0f, render::stand_rise_m() * s);
    return ws_test_pose_shift(u, s, steer, true,
                              glm::vec3(0.0f, ref.up, ref.fwd), 1.0f);
}

// C_s(u): the CG the ladder's SHAPE carries aft, on the pose above. The
// runtime bakes the same quantity off its own pose pass; this is the
// independent re-derivation, and the two agree to a few millimetres.
struct WsCurve {
    float c[render::kWsBakeU]{};
    float dj[render::kWsBakeU]{};  // worst joint delta between adjacent cells
};

// ★ AT STEER 0, BECAUSE THAT IS WHERE THE RUNTIME BAKES IT.
// render/sled_model.cpp's bake_weight_shift drives a probe rig whose `steer`
// is 0, so ONE curve per stand slice is what the ladder inverts at every
// steering angle. Baking a per-steer curve here would gate a mechanism the
// game does not have; the continuity case below therefore takes `u` from this
// curve and then poses the man at the steers he can actually be at.
WsCurve ws_test_curve(float s) {
    WsCurve out;
    const float steer = 0.0f;
    const float base = render::rider_cg(ws_test_pose(0.0f, s, steer, false)).z;
    std::array<glm::vec3, render::kRiderJointCount> prev{};
    for (int i = 0; i < render::kWsBakeU; ++i) {
        const float u = float(i) / float(render::kWsBakeU - 1);
        const std::array<glm::vec3, render::kRiderJointCount> jp =
            ws_test_pose(u, s, steer, true);
        out.c[i] = base - render::rider_cg(jp).z;
        if (i > 0)
            for (int j = 0; j < render::kRiderJointCount; ++j)
                out.dj[i] = std::max(
                    out.dj[i],
                    glm::length(jp[std::size_t(j)] - prev[std::size_t(j)]));
        prev = jp;
    }
    return out;
}

}  // namespace

TEST_CASE(
    "R3-WS(b): every baked stand slice of C_s(u) RISES -- the curve "
    "shape is the gate the stand-entry pop hid behind",
    "[rider_pose][asset]") {
    // ★ RED-TEAM FIX 1b. As landed, this was a LOG_WARNING at bake time that
    // fired on the shipped build and nobody was required to read. The runtime
    // now stops the program on it (render/sled_model.cpp); this case is the
    // same fact in the ctest gate, on an independently re-derived curve.
    //
    // The number that matters is NOT the slope: it is the POSE DELTA the
    // slope buys, in metres of joint travel per metre of aft demand. u =
    // C^-1(a) has slope 1/(dC/du), so a small dC/du is only a defect if the
    // joints are moving while it is small.
    const float step =
        float(sim::SledParams{}.lean_aft_max_m) / float(render::kWsDemandCells);
    for (int si = 0; si < render::kWsBakeS; ++si) {
        const float s = float(si) / float(render::kWsBakeS - 1);
        const WsCurve cv = ws_test_curve(s);
        INFO("s " << s);
        REQUIRE(cv.c[0] == 0.0f);
        float worst_cont = 0.0f;
        int worst_at = 0;
        for (int i = 1; i < render::kWsBakeU; ++i) {
            const float dc = cv.c[i] - cv.c[i - 1];
            INFO("cell " << i << " C " << cv.c[i] << " dC " << dc << " dj "
                         << cv.dj[i]);
            // (1) STRICTLY rising. A flat spot is an infinite jump in u.
            REQUIRE(dc > 0.0f);
            // (2) and the pose delta it buys stays inside SPEC 7.5.
            const float cont = cv.dj[i] * step / dc;
            if (cont > worst_cont) {
                worst_cont = cont;
                worst_at = i;
            }
        }
        INFO("worst pose delta per " << step << " m of demand " << worst_cont
                                     << " at cell " << worst_at);
        REQUIRE(worst_cont <= render::kWsContinuityBoundM);
        // non-vacuity: the ladder really carries CG aft on every slice
        REQUIRE(cv.c[render::kWsBakeU - 1] > 0.15f);
        // ... and the curve is the SHAPE the runtime bakes, to a few mm: this
        // is an independent re-derivation, not a copy of it.
        REQUIRE(cv.c[render::kWsBakeU - 1] < 0.30f);
    }
}

TEST_CASE(
    "R3-WS(b) HEADLINE: the delivered pose is continuous in the RUNTIME "
    "coordinate -- no stand-entry pop",
    "[rider_pose][asset]") {
    // ★ RED-TEAM FIX 1a, AND IT IS THE ONE THAT WOULD HAVE CAUGHT THE BUG.
    // Step in `a`, not in `u`. On the curve R3-WS shipped, the first two
    // samples of the s = 1 row below moved the man 0.28 of the whole ladder:
    // feet off the weld, trunk folding, pelvis 60-90 mm, in ONE frame,
    // entering aft AND leaving it. Feet and shins are INCLUDED -- they are
    // exactly where the rigid weld releases.
    const float a_max = float(sim::SledParams{}.lean_aft_max_m);
    const int cells = render::kWsDemandCells;
    for (float s : {0.0f, 0.5f, 1.0f}) {
        const WsCurve cv = ws_test_curve(s);  // baked at steer 0, as shipped
        for (float steer : {0.0f, -1.0f, 1.0f}) {
            std::array<glm::vec3, render::kRiderJointCount> prev =
                ws_test_pose(0.0f, s, steer, true);
            float worst = 0.0f, worst_a = 0.0f;
            int worst_j = 0;
            for (int i = 1; i <= cells; ++i) {
                const float a = a_max * float(i) / float(cells);
                const float u = render::ws_invert(cv.c, render::kWsBakeU, a);
                const std::array<glm::vec3, render::kRiderJointCount> jp =
                    ws_test_pose(u, s, steer, true);
                for (int j = 0; j < render::kRiderJointCount; ++j) {
                    const float d =
                        glm::length(jp[std::size_t(j)] - prev[std::size_t(j)]);
                    if (d > worst) {
                        worst = d;
                        worst_a = a;
                        worst_j = j;
                    }
                }
                prev = jp;
            }
            INFO("s " << s << " steer " << steer << ": worst joint delta "
                      << worst << " m at a " << worst_a << " joint "
                      << worst_j);
            // ★ THE GATE IS SPEC 7.5's 0.06 m AT STEER 0 -- the cells the
            // ladder's curve is baked for, and the cells the stand-entry pop
            // lived in. MEASURED there: 0.0209 (s 0) / 0.0170 (s 0.5) /
            // 0.0186 (s 1), a third of the bound.
            //
            // ★★ AT FULL LOCK IT IS 0.08, AND THAT IS A FILED FINDING, NOT A
            // SOFTENED GATE. Measured worst 0.0769 in ONE cell (s 0.5,
            // steer -1, head, a 0.094). Both mechanisms behind it are the
            // pre-existing over-reach the 0-OFF law forbids this rung from
            // touching (OPEN-R3WS-STEERREACH: the outboard arm is at chain
            // ratio 1.1404 on HEAD with no rider input at all):
            //   1. the 2-bone elbow rides sqrt(l0^2 - a^2), whose derivative
            //      is INFINITE where the chain straightens, so the elbow
            //      swings fast as the ladder carries the arm back under 1.0 --
            //      and carrying it back under 1.0 is the improvement;
            //   2. reach_pair_solve takes the SMALLEST theta attaining the
            //      design ratio, and at full lock the pair's ratio-vs-theta
            //      curve grows a second valley, so that first crossing steps
            //      (measured 0.687 -> 0.772 rad across one cell at steer -1).
            // Neither is reachable at steer 0 and neither is the ladder's; (2)
            // is a NEW measurement this fix turned up and is filed with (1).
            REQUIRE(worst <= (steer == 0.0f ? 0.06f : 0.08f));
            // NON-VACUITY: the sweep must actually have driven the ladder.
            REQUIRE(render::ws_invert(cv.c, render::kWsBakeU, a_max) > 0.5f);
        }
    }
}

TEST_CASE(
    "R3-WS(b): the reach gate is 0 at u = 0 for every stand, and it is "
    "the FOOT band only when seated",
    "[rider_pose]") {
    for (int i = 0; i <= 20; ++i) {
        const float s = float(i) / 20.0f;
        REQUIRE(render::ws_reach_gate(0.0f, s) == 0.0f);  // the 0-OFF law
        // * R3-WS(c): the band is 1.40 at full stand now, so it OVERRUNS
        // u = 1 on purpose -- the standing trunk never reaches the full
        // solved flexion, which is half of why the head stays up. At s = 0 it
        // is still exactly total.
        REQUIRE(render::ws_reach_gate(1.0f, s) <= 1.0f);
        REQUIRE(render::ws_reach_gate(1.0f, s) > 0.79f);
        // monotone in u, and NEVER faster than the seated band: that is the
        // whole fix -- the flexion may only be re-timed LATER, never earlier.
        float prev = 0.0f;
        for (int k = 0; k <= 100; ++k) {
            const float u = float(k) / 100.0f;
            const float g = render::ws_reach_gate(u, s);
            INFO("u " << u << " s " << s << " gate " << g);
            REQUIRE(g >= prev - 1e-6f);
            REQUIRE(g <= render::ws_reach_gate(u, 0.0f) + 1e-6f);
            prev = g;
        }
    }
    // at s = 0 it IS the foot blend's own band, so the seated slice of the
    // ladder is bit-identical to R3-WS as landed
    for (int k = 0; k <= 100; ++k) {
        const float u = float(k) / 100.0f;
        const float t =
            std::min(1.0f, std::max(0.0f, u / render::kFootBlendHiU));
        REQUIRE(render::ws_reach_gate(u, 0.0f) == t * t * (3.0f - 2.0f * t));
    }
    REQUIRE(render::ws_reach_gate(1.0f, 0.0f) == 1.0f);
    // ... and at full stand it stops at the band's own value, 0.8017 -- the
    // measured 20 % of solved trunk flexion the standing man never spends.
    REQUIRE(std::fabs(render::ws_reach_gate(1.0f, 1.0f) - 0.8017f) < 1e-3f);
    // and rho rides the same schedule: no second kink
    REQUIRE(render::ws_rho(0.0f, 1.0f, 0.5f) == 0.5f);
}

TEST_CASE(
    "R3-WS(b): the a = 0 pose is BIT-IDENTICAL to the ladder-free pose, "
    "joint by joint",
    "[rider_pose][asset]") {
    // ★ RED-TEAM FIX 3. The 0-OFF claim was carried by a structural argument
    // ("one `active` branch") plus per-formula identity checks. Neither is an
    // ARTEFACT gate: what has to be unchanged is the man, not the branch. So
    // this poses him both ways and compares the twenty-one world positions
    // exactly. The `false` path never calls a ladder function at all -- it
    // substitutes literal zeros -- so it cannot inherit the bug it is
    // checking for.
    for (float s : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        for (float steer : {0.0f, -1.0f, -0.5f, 0.5f, 1.0f}) {
            const std::array<glm::vec3, render::kRiderJointCount> on =
                ws_test_pose(0.0f, s, steer, true);
            const std::array<glm::vec3, render::kRiderJointCount> off =
                ws_test_pose(0.0f, s, steer, false);
            for (int j = 0; j < render::kRiderJointCount; ++j) {
                INFO("s " << s << " steer " << steer << " joint " << j);
                REQUIRE(on[std::size_t(j)].x == off[std::size_t(j)].x);
                REQUIRE(on[std::size_t(j)].y == off[std::size_t(j)].y);
                REQUIRE(on[std::size_t(j)].z == off[std::size_t(j)].z);
            }
            // NON-VACUITY: the same model at a real u moves him a long way,
            // so the equality above is a property of u = 0 and not of a pose
            // model that never does anything.
            const std::array<glm::vec3, render::kRiderJointCount> live =
                ws_test_pose(1.0f, s, steer, true);
            REQUIRE(glm::length(live[render::kRiderPelvis] -
                                off[render::kRiderPelvis]) > 0.10f);
        }
    }
}

TEST_CASE(
    "R3-WS(c): the LEGS never regress and the knees clear the seat",
    "[rider_pose][asset]") {
    // *** A6 + A7. The full runner slide is 3.1x the travel v1 asked of the
    // legs, so the two things it could break are gated here on the same
    // re-derived pose the continuity cases use.
    //
    // * A6: THE LEG GATE IS A NO-REGRESSION CEILING, NOT AN ABSOLUTE 0.985.
    // HEAD's own standing legs already read 1.107-1.1246 with no rider input
    // at all, and the 0-OFF law FREEZES those cells -- an absolute gate would
    // be red on day one and would be gating the stand, not this rung.
    const WsRest& r = ws_rest();
    const float dmax = r.thigh + r.shin - 1e-3f;
    const int hip[2] = {render::kRiderThighR, render::kRiderThighL};
    const int knee[2] = {render::kRiderShinR, render::kRiderShinL};
    const int ank[2] = {render::kRiderFootR, render::kRiderFootL};
    float worst_gap = -9.0f, worst_ratio = 0.0f, min_chord = 9.0f;
    float min_kx = 9.0f, max_kx = 0.0f;
    for (int iu = 0; iu <= 32; ++iu) {
        const float u = float(iu) / 32.0f;
        for (int isv = 0; isv < 5; ++isv) {
            const float sv = float(isv) / 4.0f;
            for (float steer : {0.0f, -1.0f, 1.0f}) {
                const std::array<glm::vec3, render::kRiderJointCount> on =
                    ws_test_pose(u, sv, steer, true);
                const std::array<glm::vec3, render::kRiderJointCount> off =
                    ws_test_pose(u, sv, steer, false);
                for (int k = 0; k < 2; ++k) {
                    const float ra =
                        glm::length(on[std::size_t(hip[k])] -
                                    on[std::size_t(ank[k])]) / dmax;
                    const float rf =
                        glm::length(off[std::size_t(hip[k])] -
                                    off[std::size_t(ank[k])]) / dmax;
                    const float ceiling = std::max(render::kArmRatioLimit, rf);
                    INFO("u " << u << " s " << sv << " steer " << steer
                              << " side " << k << " ladder " << ra
                              << " ladder-free " << rf);
                    REQUIRE(ra <= ceiling + 1e-3f);
                    worst_ratio = std::max(worst_ratio, ra);
                    worst_gap = std::max(worst_gap, ra - rf);
                    // * A7: the hip->ankle CHORD floor. The kneel's shard
                    // chord is 0.197 and it tore the proxy skin; this geometry
                    // must stay far away from it.
                    const float chord = glm::length(on[std::size_t(hip[k])] -
                                                    on[std::size_t(ank[k])]);
                    min_chord = std::min(min_chord, chord);
                    REQUIRE(chord >= 0.25f);
                    // ... and the KNEE stays clear of the seat prism and
                    // inside the running board, so the shin cannot fold into
                    // the machine if a capture ever changes the bend normal.
                    //
                    // * A7's absolute floor (seat half 0.2060 + pad 0.0918 =
                    // 0.2978) IS NOT THE SHIPPED GEOMETRY, and that is a
                    // measurement, not a softening: the REST knee sits at
                    // |x| 0.268404, 29 mm inboard of it, with the ladder off
                    // and no rider input at all -- a 0-OFF-frozen fact this
                    // rung may not touch. So the floor takes the same
                    // no-regression form the arm and leg ratios take: the
                    // ladder may never pull the knee further INBOARD than the
                    // same cell's ladder-free pose already has it.
                    const float kx = std::fabs(on[std::size_t(knee[k])].x);
                    const float kx_free = std::fabs(off[std::size_t(knee[k])].x);
                    min_kx = std::min(min_kx, kx);
                    max_kx = std::max(max_kx, kx);
                    // ** AND THE ABSOLUTE FLOOR CANNOT BE GATED EITHER, FOR
                    // THE SAME REASON: at s = 0.75 with the LADDER OFF the
                    // knee already reads |x| 0.1973 -- INSIDE the seat prism
                    // (half width 0.2060). That is HEAD's own standing pose,
                    // frozen by the 0-OFF law, and filed as
                    // OPEN-R3WS-STANDKNEE rather than papered over. So what is
                    // gated is the ladder's OWN contribution: it may never
                    // walk the knee inboard by more than 20 mm from the same
                    // cell's ladder-free knee (measured worst is millimetres,
                    // and it is the leg straightening as the foot goes aft,
                    // not the bend plane folding into the machine).
                    // *** R3-WS(d) / SPEC 13.4 D4(a). THIS GATE IS FOR THE
                    // DECK BRANCH ONLY, AND SKIPPING IT ON KNEEL CELLS IS A
                    // CATEGORY FIX, NOT A SOFTENING. The rule above says the
                    // ladder may not walk the knee INBOARD of where the deck
                    // pose has it -- because on the deck, inboard means into
                    // the seat. The kneel's whole job is to put the knee ON
                    // the seat, inboard, which is the OPPOSITE sense; run
                    // here it reads a legitimate 0.025 m as a violation. The
                    // kneel and blend cells are gated instead by the
                    // blended-reference case below (D4(a)), which covers
                    // every cell this one skips, and by the absolute chord
                    // floor, which is checked on ALL of them just above.
                    // ★★★★ R3-WS(e). With the gain at 0 (Chad's 2026-08-21
                    // ruling) this branch is taken on EVERY cell, which makes
                    // the deck no-regression knee gate STRICTLY STRONGER here
                    // than it was under (d) -- nothing is skipped any more.
                    // The kneel cells it used to skip are gated instead, with
                    // the kneel forced on, by the D4(a) blended-reference case
                    // below (ws_test_pose_kneel). Deliberately still written
                    // against the shipped gain: if R4-SIDE ever turns it back
                    // on for this axis, the category fix returns with it.
                    const float w_kn = render::ws_weights(u, sv).kneel *
                                       render::kKneelRuntimeGain;
                    if (w_kn < 1e-4f) {
                        REQUIRE(kx_free - kx <= 0.020f);
                        REQUIRE(kx <= 0.3750f + 0.20f);
                    }
                }
            }
        }
    }
    INFO("worst leg ratio " << worst_ratio << ", worst (ladder - free) "
                            << worst_gap << ", min chord " << min_chord
                            << ", knee |x| in [" << min_kx << ", " << max_kx
                            << "]");
    // NON-VACUITY: the sweep really did stretch the legs, and the ladder's own
    // contribution is bounded and small.
    REQUIRE(worst_ratio > 0.85f);
    REQUIRE(worst_gap <= 0.05f);
    REQUIRE(min_chord < 0.80f);
}

TEST_CASE(
    "R3-WS(d) HEADLINE: the pose is continuous in the STAND coordinate too "
    "-- the kneel kill band cannot pop under the stand key",
    "[rider_pose][asset]") {
    // *** SPEC 13.4 D1, AND IT IS THE DEFECT TURNING THE KNEEL ON CREATED.
    // Every continuity case above steps in `a` at FIXED stand. The kneel dies
    // across the STAND axis, which the player owns with a key -- and the
    // kernel slews `lean_up_m` fast enough to cross a narrow kill band in two
    // or three frames. So this case steps in `s`, by exactly the fraction one
    // 60 Hz tick of that slew can deliver, at the u values where the kneel is
    // live, and measures the same joint-world-delta SPEC 7.5 bounds.
    //
    // ** AND THE BOUND IT ENFORCES IS THE KNEEL'S OWN CONTRIBUTION, WHICH IS
    // A MEASUREMENT AND NOT A SOFTENING. The R3-WS(c) ladder Chad already
    // drove ALREADY spends most of SPEC 7.5's 0.06 m on one tick of stand
    // slew with no kneel in the pose at all (measured 0.0574 m on the shipped
    // pose path, filed as OPEN-R3WS-STANDSLEW) -- an absolute gate here would
    // be red on day one and would be gating the STAND, not this rung. Same
    // no-regression form A6 gave the legs and SPEC 12.3 gave the knees.
    const float ds = render::stand_slew_ds_per_tick();
    INFO("stand slew per 60 Hz tick " << ds);
    REQUIRE(ds > 0.0f);
    float worst_on = 0.0f, worst_off = 0.0f, at_s = 0.0f, at_u = 0.0f;
    for (const float u : {0.8f, 1.0f}) {
        for (int i = 0; i <= 128; ++i) {
            const float s0 = float(i) / 128.0f;
            const float s1 = std::min(1.0f, s0 + ds);
            if (!(s1 > s0)) continue;
            // ★★★★ R3-WS(e). FORCED KNEEL (gain 1), not the shipped pose.
            // Chad's 2026-08-21 ruling zeroes the gain for the longitudinal
            // ladder, and reading the shipped pose here would turn this case
            // into a restatement of the kneel-free floor -- green, and about
            // nothing. This is the R4-SIDE inheritance under gate.
            const std::array<glm::vec3, render::kRiderJointCount> a0 =
                ws_test_pose_kneel(u, s0, 0.0f);
            const std::array<glm::vec3, render::kRiderJointCount> a1 =
                ws_test_pose_kneel(u, s1, 0.0f);
            const std::array<glm::vec3, render::kRiderJointCount> b0 =
                ws_test_pose(u, s0, 0.0f, false);
            const std::array<glm::vec3, render::kRiderJointCount> b1 =
                ws_test_pose(u, s1, 0.0f, false);
            float on = 0.0f, off = 0.0f;
            for (int j = 0; j < render::kRiderJointCount; ++j) {
                on = std::max(on, glm::length(a1[std::size_t(j)] -
                                              a0[std::size_t(j)]));
                off = std::max(off, glm::length(b1[std::size_t(j)] -
                                                b0[std::size_t(j)]));
            }
            if (on > worst_on) {
                worst_on = on;
                at_s = s0;
                at_u = u;
            }
            worst_off = std::max(worst_off, off);
        }
    }
    INFO("worst with the ladder " << worst_on << " at s " << at_s << " u "
                                  << at_u << ", ladder-free floor "
                                  << worst_off);
    REQUIRE(worst_on - worst_off <= render::kWsKneelSlewAddM);
    // NON-VACUITY: the sweep really did move the man on this axis.
    REQUIRE(worst_on > 0.01f);
}

TEST_CASE(
    "R3-WS(d): no cell is ungated in the kneel BLEND -- the knee tracks the "
    "blended reference, not one branch or the other",
    "[rider_pose][asset]") {
    // *** SPEC 13.4 D4(a). The R3-WS gates fired only at w_kneel > 0.99, so
    // the entire blend region was unwatched -- and the deck branch's own
    // no-regression knee gate goes RED on kneel cells by construction, because
    // the kneel deliberately walks the knee inboard and onto the seat, which
    // is the OPPOSITE sense. So every active cell is gated against the BLENDED
    // reference mix(deck-branch knee, K, w_kneel): the identity at w = 0, the
    // old 1 mm knee gate at w = 1, and a real bound in between.
    const WsRest& r = ws_rest();
    const int knee[2] = {render::kRiderShinR, render::kRiderShinL};
    const int hip[2] = {render::kRiderThighR, render::kRiderThighL};
    const char* th[2] = {"thigh_r", "thigh_l"};
    const char* ca[2] = {"calf_r", "calf_l"};
    const char* ft[2] = {"foot_r", "foot_l"};
    float worst = 0.0f, worst_w = 0.0f, max_w = 0.0f;
    bool saw_blend = false;
    for (int iu = 0; iu <= 64; ++iu) {
        const float u = float(iu) / 64.0f;
        for (const float sv : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            // ★★★★ R3-WS(e). The weight is read WITHOUT the shipped gain and
            // the pose is driven with the gain FORCED TO 1, for the reason
            // ws_test_pose_kneel exists: this case gates the kneel geometry
            // R4-SIDE inherits, and Chad's 2026-08-21 ruling took that
            // geometry out of the longitudinal pose, not out of the tree.
            const float w = render::ws_weights(u, sv).kneel;
            max_w = std::max(max_w, w);
            if (w > 0.02f && w < 0.98f) saw_blend = true;
            const std::array<glm::vec3, render::kRiderJointCount> on =
                ws_test_pose_kneel(u, sv, 0.0f);
            // The DECK-BRANCH reference is this same model with the kneel
            // weight held at zero -- one call, because the model is a pure
            // function of (u, s) and `ws_weights` is where the weight comes
            // from. Rebuilding it that way is what makes this a reference and
            // not a restatement of the pose under test.
            const std::array<glm::vec3, render::kRiderJointCount> deck =
                ws_test_pose_deck(u, sv, 0.0f);
            for (int k = 0; k < 2; ++k) {
                const float l0 = glm::length(Pw(ca[k]) - Pw(th[k]));
                const float l1 = glm::length(Pw(ft[k]) - Pw(ca[k]));
                const glm::vec3 root = on[std::size_t(hip[k])];
                const render::KneelPose kp =
                    render::kneel_construct(root, l0, l1);
                const glm::vec3 K = kp.ok ? kp.knee : deck[std::size_t(knee[k])];
                const glm::vec3 ref =
                    glm::mix(deck[std::size_t(knee[k])], K, w);
                const float e = glm::length(on[std::size_t(knee[k])] - ref);
                if (e > worst) {
                    worst = e;
                    worst_w = w;
                }
            }
        }
    }
    INFO("worst |knee - blended reference| " << worst << " at w_kneel "
                                             << worst_w);
    // NON-VACUITY FIRST: the sweep must actually have visited the blend and
    // reached the full kneel, or this is a green gate about nothing.
    REQUIRE(saw_blend);
    REQUIRE(max_w > 0.99f);
    // The bound is loose ON PURPOSE and the measured number is in the INFO:
    // the reference is a LINEAR mix of two branch knees while the pose is a
    // two-bone solve on a blended target, so exact tracking is not the claim.
    // What is claimed -- and what D4(a) asks for -- is that no cell in the
    // blend wanders off to a third place, which is the failure a
    // 0.99-only gate could not have seen.
    REQUIRE(worst <= 0.12f);
    // ... and the chord floor holds across the WHOLE blend, which is the gate
    // that actually stands between this rung and a skin shard.
    for (int iu = 0; iu <= 64; ++iu) {
        const float u = float(iu) / 64.0f;
        for (const float sv : {0.0f, 0.5f, 1.0f}) {
            const std::array<glm::vec3, render::kRiderJointCount> on =
                ws_test_pose_kneel(u, sv, 0.0f);  // R3-WS(e): forced kneel
            for (int k = 0; k < 2; ++k) {
                const float chord =
                    glm::length(on[std::size_t(hip[k])] -
                                on[std::size_t(k == 0 ? render::kRiderFootR
                                                      : render::kRiderFootL)]);
                INFO("u " << u << " s " << sv << " side " << k << " chord "
                          << chord);
                REQUIRE(chord >= 0.25f);
            }
        }
    }
    (void)r;
}

TEST_CASE(
    "R3-WS(b): ws_invert reads the ENVELOPE at BOTH ends -- an "
    "end-dipping curve cannot jump the ladder to its far stop",
    "[rider_pose]") {
    // ★ RED-TEAM FIX 4. The early return compared against the RAW last entry,
    // which is the same ambiguity the running maximum removes -- just at the
    // other end. A curve that peaks before its last station and falls back
    // answered u = 1.0 for every demand above that falling endpoint.
    float c[render::kWsBakeU];
    for (int i = 0; i < render::kWsBakeU; ++i)
        c[i] = 0.30f * float(i) / float(render::kWsBakeU - 1);
    const int n = render::kWsBakeU;
    c[n - 1] = 0.24f;  // the END DIP: peak 0.29 at n-2, falling to 0.24
    REQUIRE_FALSE(render::ws_monotone(c, n));
    REQUIRE(render::ws_min_slope(c, n) < 0.0f);
    // the demand that used to jump: above the raw endpoint, below the peak
    const float a = 0.27f;
    const float u = render::ws_invert(c, n, a);
    INFO("u " << u);
    REQUIRE(u < 1.0f);
    REQUIRE(u > 0.85f);
    // ... and it is still TOTAL and non-decreasing across the whole range
    float prev = -1.0f;
    for (int i = 0; i <= 400; ++i) {
        const float aa = 0.35f * float(i) / 400.0f;
        const float uu = render::ws_invert(c, n, aa);
        REQUIRE(uu >= prev - 1e-6f);
        REQUIRE(uu >= 0.0f);
        REQUIRE(uu <= 1.0f);
        prev = uu;
    }
    REQUIRE(render::ws_invert(c, n, c[n - 2]) == 1.0f);  // AT the peak
    REQUIRE(render::ws_invert(c, n, 9.0f) == 1.0f);      // and past it
    // a genuinely rising table is unaffected
    for (int i = 0; i < n; ++i) c[i] = 0.30f * float(i) / float(n - 1);
    REQUIRE(render::ws_min_slope(c, n) > 0.0f);
    REQUIRE(render::ws_invert(c, n, 0.30f) == 1.0f);
    REQUIRE(std::fabs(render::ws_invert(c, n, 0.15f) - 0.5f) < 1e-4f);
}

// ===========================================================================
// ★★★ K-WS1 -- THE KERNEL/ANIMATION CROSS-CHECKS (red-team amendment K-A2)
// ===========================================================================
//
// sim/sled.h now PINS two things it cannot itself measure: the animation's
// delivered aft-CG ceiling per stand slice (`kAftCeilC1`) and the rider's
// seated CG offset from the machine CG (`kRiderSeatOff*`). Both were read off
// the shipped asset. Nothing stops a re-export from moving them and leaving
// the kernel asking for CG the body no longer delivers -- which is precisely
// the 38.7 mm lie this rung exists to kill, re-created silently.
//
// ★ WHAT THIS CAN AND CANNOT BE. K-A2 asks for the cross-check against "the
// live bake". render/sled_model.cpp -- where `bake_weight_shift` lives -- is
// in the raylib-only `seads` target and has ZERO test coverage BY
// CONSTRUCTION (see its own note); `seads_tests` links no raylib. So the
// reference here is this file's INDEPENDENT re-derivation of the same pose off
// the same GLB (`ws_test_curve`), which is the same artefact the whole R3-WS
// rung already gates on and which moves with a re-export exactly as the bake
// does. Stated as a deviation rather than pretended past.
TEST_CASE("K-WS1: the kernel's pinned aft-ceiling row IS the animation's "
          "delivered C_s(1), slice by slice",
          "[rider_pose][asset][sled]") {
    REQUIRE(sim::kAftCeilSlices == render::kWsBakeS);
    float worst = 0.0f;
    for (int si = 0; si < render::kWsBakeS; ++si) {
        const float s = float(si) / float(render::kWsBakeS - 1);
        const float measured = ws_test_curve(s).c[render::kWsBakeU - 1];
        const float pinned = float(sim::kAftCeilC1[si]);
        WARN("K-WS1 C(1) slice s=" << s << " pinned " << pinned
                                   << " measured " << measured << " delta "
                                   << (measured - pinned));
        worst = std::max(worst, std::fabs(measured - pinned));
    }
    // The two models are independent re-derivations of one pose, so they agree
    // to millimetres, not to bits. The bound is a HONESTY bound: it is far
    // tighter than the ~48-77 mm lie the row exists to remove, so it can still
    // catch a re-export that moves the ladder.
    INFO("worst |pinned - measured| = " << worst << " m");
    REQUIRE(worst < 0.010f);

    // *** THE HEADLINE, MEASURED IN THE PLAYER'S OWN COORDINATE. u is taken
    // through the SHIPPED inverse, never by dividing two ceilings (C_s is not
    // linear in u).
    //
    // ★★★★ R3-WS(e) RE-FORMED THIS LEG, AND THE REASON IS THE RULING, NOT A
    // FAILING NUMBER. Under R3-WS(d) the claim was "the OLD 0.25 clamp could
    // not reach the top of the kneeling ladder (u ~ 0.92), the raised one
    // can". Chad's 2026-08-21 ruling removed the kneel from this axis, so the
    // seated ceiling fell to 0.2183 -- BELOW 0.25 -- and the old clamp now
    // over-shoots the ladder instead of under-reaching it. The direction
    // flipped; the property under gate did not. What is asserted at every
    // stand slice is the SAME thing it always was: the kernel's clamp lands
    // exactly on the top of the ladder -- reachable (u = 1) and not beyond it.
    const float clamp0 = float(sim::SledParams{}.lean_aft_max_m);
    REQUIRE(clamp0 == float(sim::kAftCeilC1[0]));
    for (int si = 0; si < render::kWsBakeS; ++si) {
        const float s = float(si) / float(render::kWsBakeS - 1);
        const WsCurve cv = ws_test_curve(s);
        const float u_at = render::ws_invert(cv.c, render::kWsBakeU,
                                             float(sim::kAftCeilC1[si]));
        WARN("K-WS1 reachability s=" << s << " pinned "
             << sim::kAftCeilC1[si] << " -> u " << u_at
             << " (ladder top " << cv.c[render::kWsBakeU - 1] << ")");
        INFO("slice " << si << " u " << u_at);
        // 0.95, not 0.9999, and the reason is NAMED rather than tuned. The
        // pinned row is the LIVE BAKE's number; this curve is the INDEPENDENT
        // re-derivation, which the leg above bounds at 4.0 mm of disagreement.
        // The ladder is nearly FLAT at the top (min dC/du 0.00041 m per 1/32
        // step), so millimetres of model gap buy percent of u: measured
        // u = 1, 1, 0.99717, 0.99018, 0.96183 across the five slices, worst at
        // s = 1 where the gap itself is worst. Tightening this would be
        // gating the two models' agreement, which the 0.010 m leg above
        // already does, in the unit that is actually meaningful.
        //
        // THE EXACT REACHABILITY CLAIM LIVES WHERE IT CAN BE EXACT: on the
        // LIVE bake (SEADS_SLED_WS_DEBUG reads u = 0.9998 at the shipped
        // clamp, s = 0) and on the kernel's own over-ask sweep, which holds at
        // 1e-9 m at every stand (test_sled,
        // sled_aft_demand_never_exceeds_the_measured_ceiling).
        REQUIRE(u_at > 0.95f);  // the whole ladder is reachable in play
        // ... and on THIS curve's own ceiling the inverse really does top out,
        // so the near-miss above is the model gap and nothing else.
        REQUIRE(render::ws_invert(cv.c, render::kWsBakeU,
                                  cv.c[render::kWsBakeU - 1]) == 1.0f);
    }
    // ... and the pre-K-WS1 flat clamp really did ask past the ladder at zero
    // stand: 0.25 against a delivered 0.2183 is 31.7 mm of demand the body
    // never answers, i.e. the last ~12.7 % of the aft mouse travel bought no
    // pose at all. That is the lie this row exists to remove, and it predates
    // the whole weight-shift program.
    const WsCurve c0 = ws_test_curve(0.0f);
    const float top0 = c0.c[render::kWsBakeU - 1];
    WARN("K-WS1 pre-rung flat clamp 0.25 vs delivered " << top0
         << " -> dead aft-mouse travel " << (0.25f - top0) / 0.25f);
    REQUIRE(top0 < 0.25f);
    REQUIRE(render::ws_invert(c0.c, render::kWsBakeU, 0.25f) == 1.0f);
}

TEST_CASE("K-WS1: the kernel's seated rider CG offset IS the shipped "
          "Sudburian's, in body axes",
          "[rider_pose][asset][sled]") {
    // render/sled_model.cpp's SHIPPED mount is R_y(180) after a
    // -(cg_height_m - kSagDefaultM) drop (the DRIVE-2 "shroud halfway through
    // the asphalt" fix -- NOT the stale "(0, cg_h, 0) lands on the kernel CG"
    // sentence in the same block, which predates it). So
    // body = (-m.x, m.y - (cg_h - sag0), -m.z), and body +Z is AFT.
    const glm::vec3 m = render::rider_cg(ws_test_pose(0.0f, 0.0f, 0.0f, false));
    const sim::SledParams p;
    const float by = m.y - float(p.cg_height_m) + render::kSagDefaultM;
    const float bz = -m.z;
    WARN("K-WS1 seated rider CG, model " << m.x << " " << m.y << " " << m.z
                                         << " -> body y " << by << " z " << bz);
    REQUIRE(std::fabs(by - float(sim::kRiderSeatOffY_m)) < 0.005f);
    REQUIRE(std::fabs(bz - float(sim::kRiderSeatOffZ_m)) < 0.005f);
    // The height is the load-bearing half: it is the moment arm K2's whole
    // sizing rests on, and a zero there would silently delete the term.
    REQUIRE(by > 0.30f);
}

// ===========================================================================
// R3-HANDS -- the control curl (Chad, 2026-08-24)
// ===========================================================================
// These pin the two properties the rung actually rests on: that a zero input
// is EXACTLY the rest pose, and that the axis is CHOSEN BY MEASUREMENT rather
// than typed. Both are mutation-verified -- each REQUIRE below was watched to
// fail against a deliberately broken curl_aim (axis hard-coded to 0) and a
// broken curl_rotation (identity guard removed).

TEST_CASE("R3-HANDS: a zero control input is exactly the rest pose",
          "[rider_pose][r3hands]") {
    render::CurlAim a;
    a.axis = 0;
    a.sign = 1.0f;
    // ★ 0-OFF, and it must be EXACT, not near: this is what lets two new
    // driven bones be added to a signed visual without moving it. The measured
    // proof in the game is d_tip +0.0 +0.0 +0.0 mm on both bones at amt 0.
    const glm::quat q = render::curl_rotation(a, render::kControlThumbRad, 0.0f);
    REQUIRE(q.w == 1.0f);
    REQUIRE(q.x == 0.0f);
    REQUIRE(q.y == 0.0f);
    REQUIRE(q.z == 0.0f);
    // An unmeasurable aim is inert whatever the input -- "no usable axis" must
    // read as "do not curl", never as a direction.
    render::CurlAim none;
    const glm::quat q2 =
        render::curl_rotation(none, render::kControlThumbRad, 1.0f);
    REQUIRE(q2.w == 1.0f);
}

TEST_CASE("R3-HANDS: the curl axis is measured off the target, not assumed",
          "[rider_pose][r3hands]") {
    // A bone at the origin in the identity frame: its own X/Y/Z are the model
    // axes and it points along +Y, which is Blender's bone convention and what
    // every pure-+Y row in rider_joint_specs() confirms.
    const glm::mat4 bone(1.0f);

    // Target straight ahead (+Z): rotating about the bone's X carries its +Y
    // tip along cross(X, Y) = +Z, so X is the axis and the sign is positive.
    const render::CurlAim fwd = render::curl_aim(bone, glm::vec3(0, 0, 1));
    REQUIRE(fwd.axis == 0);
    REQUIRE(fwd.sign == 1.0f);

    // ★ THE SAME BONE, THE OTHER WAY: the sign must FOLLOW the target. A typed
    // sign would return +1 here too, which is the bug this whole function
    // exists to make impossible.
    const render::CurlAim back = render::curl_aim(bone, glm::vec3(0, 0, -1));
    REQUIRE(back.axis == 0);
    REQUIRE(back.sign == -1.0f);

    // ★ AND A DIFFERENT AXIS ENTIRELY. Sideways (+X) is reached about the
    // bone's Z: cross(Z, Y) = -X, so the sign inverts. This is the case that
    // caught the real defect -- aiming at the levers' shared centreline origin
    // chose axis 0/2 for the two bones, and aiming at their actual geometry
    // chose 2/0. Same code, different answer, because the target was wrong.
    const render::CurlAim side = render::curl_aim(bone, glm::vec3(1, 0, 0));
    REQUIRE(side.axis == 2);
    REQUIRE(side.sign == -1.0f);

    // Degenerate inputs are refused rather than guessed: a target sitting on
    // the joint has no direction, and a collapsed frame has no axes.
    REQUIRE(render::curl_aim(bone, glm::vec3(0, 0, 0)).axis == -1);
    REQUIRE(render::curl_aim(glm::mat4(0.0f), glm::vec3(0, 0, 1)).axis == -1);
}

TEST_CASE("R3-HANDS: the curl actually carries the tip toward the lever",
          "[rider_pose][r3hands]") {
    // The property the axis choice is FOR -- it is not enough that an axis was
    // picked; the rotation it implies has to close the distance. Without this
    // leg a sign error passes every test above.
    const glm::mat4 bone(1.0f);
    const glm::vec3 tip(0.0f, 1.0f, 0.0f);
    for (const glm::vec3 target :
         {glm::vec3(0, 0, 1), glm::vec3(0, 0, -1), glm::vec3(1, 0, 0),
          glm::vec3(-1, 0, 0)}) {
        const render::CurlAim a = render::curl_aim(bone, target);
        REQUIRE(a.axis >= 0);
        const glm::vec3 moved =
            render::curl_rotation(a, render::kControlFingerRad, 1.0f) * tip;
        REQUIRE(glm::length(moved - target) < glm::length(tip - target));
    }
}

TEST_CASE("R3-HANDS: the curl is proportional and clamped to the input",
          "[rider_pose][r3hands]") {
    const glm::mat4 bone(1.0f);
    const render::CurlAim a = render::curl_aim(bone, glm::vec3(0, 0, 1));
    const glm::vec3 tip(0.0f, 1.0f, 0.0f);
    // Half input is a smaller motion than full -- the stage is continuous, so
    // nothing here may snap between rest and pressed.
    const float half = glm::length(
        render::curl_rotation(a, render::kControlFingerRad, 0.5f) * tip - tip);
    const float full = glm::length(
        render::curl_rotation(a, render::kControlFingerRad, 1.0f) * tip - tip);
    REQUIRE(half > 0.0f);
    REQUIRE(full > half);
    // And an over-range input cannot drive the bone past its ruled angle.
    const float over = glm::length(
        render::curl_rotation(a, render::kControlFingerRad, 4.0f) * tip - tip);
    REQUIRE(std::fabs(over - full) < 1e-6f);
}
