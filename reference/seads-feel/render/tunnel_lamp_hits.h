#pragma once

#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

#include "render/tunnel_mesh.h"  // TunnelLamp (positions + intensity)
#include "weapon/ballistics.h"   // Projectile pool (prev_pos -> pos segments)

// T5c — DESTRUCTIBLE GAS LAMPS (docs/tunnel_staging.md rung T5; Chad: "lights
// should be of the destructible gas lamp type that I can shoot out to produce
// darkness in that spot if I hit the gas lamp"). PURE (glm + std, ZERO raylib,
// READ-ONLY on the projectile pool except retiring a round that hit): the
// swept-segment-vs-lamp hit pass. Lives in seads_render_core so the gate pins
// it headlessly, and it is FIREWALLED from the flight kernel (nothing here
// feeds a control/sim tick — the world-thread's one hard rule, SPEC §5/§6).
//
// Architecture: the APP owns a TunnelLampWorld (built once from the T1
// place_tunnel_lamps), threads it into the fixed tick beside combat_tick, and
// the renderer rebuilds its additive-glow buffers from the ALIVE lamps when a
// kill flips `dirty`. Lamp deaths are WORLD state — they PERSIST across
// crash/respawn within a run (never reset in the respawn path).

namespace render {

// The world-layer destructible-lamp state (app-owned). `lamps` is the fixed
// placement (from place_tunnel_lamps); `alive[i]` gates lamp i's draw; `dirty`
// tells the renderer a kill happened so it rebuilds its buffers.
struct TunnelLampWorld {
    std::vector<TunnelLamp> lamps;
    std::vector<bool> alive;
    bool dirty = false;  // a lamp died this frame -> renderer rebuild pending

    // Perf broad-phase: a SET of small bounding-sphere CLUSTERS, each over a
    // contiguous run of lamp indices, already inflated by the caller's hit
    // radius. `lamp_hits` below tests a round's swept segment against each
    // cluster sphere before paying the per-lamp inner loop over ONLY that
    // cluster's index range; a round nowhere near the tunnel (the common case)
    // skips every cluster cheaply. ONE giant sphere over the whole
    // Errington->Murray lamp string was kilometres wide, so any round near the
    // play area intersected it and the early-out never fired in the near field;
    // small clusters keep the early-out effective everywhere (~16 sphere tests
    // per near-tunnel round instead of ~250 lamp tests). `cloud_stale` is a
    // PRIVATE cache-invalidation bit (deliberately NOT the same latch as
    // `dirty`, which the RENDERER owns and clears) — set on init and whenever a
    // lamp dies, cleared by rebuild_cloud().
    struct LampCluster {
        glm::dvec3 center{0.0};
        double radius = 0.0;   // max dist to an alive member + margin
        std::size_t lo = 0;    // [lo, hi) contiguous lamp-index range
        std::size_t hi = 0;
    };
    std::vector<LampCluster> clusters;
    bool cloud_stale = true;

    // Cluster-close heuristics (named so the tuning is one place). A chunk
    // closes at ~16 alive lamps OR when adding the next alive lamp would push
    // its bounding-sphere radius past ~150 m — the radius guard keeps a chunk
    // from going degenerate (kilometres wide) if placement order ever jumps
    // between distant tunnel segments; the count keeps a dense straight bore
    // from all falling into one big chunk.
    static constexpr std::size_t kClusterMaxLamps = 16;
    static constexpr double kClusterMaxRadius = 150.0;

    // Build from a placement (all lamps alive). Call once at app startup.
    void init(std::vector<TunnelLamp> placed) {
        lamps = std::move(placed);
        alive.assign(lamps.size(), true);
        dirty = false;
        clusters.clear();
        cloud_stale = true;
    }

    // Rebuild the broad-phase clusters from the currently-alive lamps, each
    // inflated by `margin` (the caller's hit radius) so a segment that misses a
    // cluster sphere provably cannot be within `margin` of ANY alive lamp in
    // that cluster: for any alive member p_i and any point x, dist(x,p_i) >=
    // dist(x,center) - dist(center,p_i) >= dist(x,center) - (radius - margin);
    // if dist(x,center) > radius that lower-bounds dist(x,p_i) strictly above
    // margin. Center = the AABB midpoint of the cluster's ALIVE members (cheap,
    // not a true minimal-enclosing-sphere center, but conservative — every
    // alive member's distance to it is measured and folded into the radius, so
    // correctness never depends on centering optimality). Lamps are walked in
    // INDEX order (placement order is spatially coherent along the bore), so a
    // greedy contiguous chunking gives tight clusters. Dead lamps stay in their
    // index range (skipped by the alive[] check in the inner loop, exactly as
    // before); a chunk with zero alive members is dropped.
    void rebuild_cloud(double margin) {
        clusters.clear();
        const std::size_t n = lamps.size();
        std::size_t chunk_lo = 0;
        std::vector<glm::dvec3> members;  // alive positions in the open chunk
        glm::dvec3 aabb_lo{0.0}, aabb_hi{0.0};

        auto close_chunk = [&](std::size_t hi) {
            if (!members.empty()) {  // drop a chunk with zero alive members
                const glm::dvec3 center = 0.5 * (aabb_lo + aabb_hi);
                double r = 0.0;
                for (const glm::dvec3& q : members)
                    r = std::max(r, glm::length(q - center));
                clusters.push_back({center, r + margin, chunk_lo, hi});
            }
            members.clear();
        };

        for (std::size_t i = 0; i < n; ++i) {
            if (!alive[i]) continue;  // dead lamps stay in whatever range spans i
            const glm::dvec3& p = lamps[i].pos;
            if (members.empty()) {  // first alive member of the open chunk
                aabb_lo = aabb_hi = p;
                members.push_back(p);
                continue;
            }
            // Prospective bounding sphere if p joins the chunk.
            const glm::dvec3 nlo = glm::min(aabb_lo, p);
            const glm::dvec3 nhi = glm::max(aabb_hi, p);
            const glm::dvec3 ncenter = 0.5 * (nlo + nhi);
            double nr = glm::length(p - ncenter);
            for (const glm::dvec3& q : members)
                nr = std::max(nr, glm::length(q - ncenter));

            if (members.size() >= kClusterMaxLamps || nr > kClusterMaxRadius) {
                close_chunk(i);      // [chunk_lo, i) tiles up to here
                chunk_lo = i;        // the next chunk starts at this lamp
                aabb_lo = aabb_hi = p;
                members.push_back(p);
            } else {
                aabb_lo = nlo;
                aabb_hi = nhi;
                members.push_back(p);
            }
        }
        close_chunk(n);  // [chunk_lo, n); dropped if no alive tail members
        cloud_stale = false;
    }

    // The ALIVE lamps of a given tier (by intensity threshold), for the
    // renderer rebuild. `bright` picks the >= cut tier; else the < cut tier.
    // T10.1: the EMBER tier (intensity == kCavernEmberIntensity) is EXCLUDED
    // from BOTH the bright and the dim family here — it has its own tier
    // (alive_ember_positions below). So a ceiling ember never draws as a tube
    // lamp and vice-versa.
    std::vector<glm::dvec3> alive_positions(float cut, bool bright) const {
        std::vector<glm::dvec3> out;
        out.reserve(lamps.size());  // one alloc; trimmed by the return move
        for (std::size_t i = 0; i < lamps.size(); ++i) {
            if (!alive[i]) continue;
            if (lamps[i].intensity == kCavernEmberIntensity) continue;  // ember
            const bool is_bright = lamps[i].intensity >= cut;
            if (is_bright == bright) out.push_back(lamps[i].pos);
        }
        return out;
    }

    // T10.1 — the ALIVE ember lamps (its own render tier: a large world-size
    // warm glow so the ceiling ember field reads across the km-scale cavern).
    std::vector<glm::dvec3> alive_ember_positions() const {
        std::vector<glm::dvec3> out;
        out.reserve(lamps.size());  // one alloc; trimmed by the return move
        for (std::size_t i = 0; i < lamps.size(); ++i)
            if (alive[i] && lamps[i].intensity == kCavernEmberIntensity)
                out.push_back(lamps[i].pos);
        return out;
    }
};

// The gas-lamp hit radius [m] — a round whose swept segment passes within this
// of a lamp position kills it. ~8 m matches the dim billboard glow footprint
// (render/tunnel.cpp look.size_m).
inline constexpr double kLampHitRadius = 8.0;

// Swept segment (a->b) vs sphere (center, r): true iff the closest point on the
// segment is within r of center. Catches the TUNNELING case — a round whose
// prev_pos and pos both sit OUTSIDE the sphere but whose path passes THROUGH it
// between ticks (an endpoint-only test would miss it). Standard clamped
// point-to-segment distance.
inline bool seg_sphere_hit(const glm::dvec3& a, const glm::dvec3& b,
                           const glm::dvec3& center, double r) {
    const glm::dvec3 ab = b - a;
    const double denom = glm::dot(ab, ab);
    double t = 0.0;
    if (denom > 0.0) t = glm::clamp(glm::dot(center - a, ab) / denom, 0.0, 1.0);
    const glm::dvec3 closest = a + t * ab;
    const glm::dvec3 d = center - closest;
    return glm::dot(d, d) <= r * r;
}

// The pure lamp-hit pass. For every ACTIVE round, test its swept segment
// [prev_pos, pos] against every ALIVE lamp; the FIRST alive lamp hit (lowest
// index) is killed and the round is RETIRED (consumed — one round kills at most
// one lamp, and a dead round can't also hit a drone). Sets world.dirty when any
// lamp died. Returns the number of lamps killed this call.
//
// Call each sim-side app tick alongside combat_tick, using the existing player
// GunWorld pool (rounds already advanced to their new pos this tick). PURE;
// nothing here feeds control/sim. Order vs combat_tick: run this AFTER the
// player->drone sweep so a round that already killed a drone (retired) is not
// re-tested here (and vice-versa a round consumed by a lamp won't hit a drone
// if run first) — the app runs lamps AFTER combat_tick, so a round that hit a
// drone is already inactive.
inline int lamp_hits(std::vector<weapon::Projectile>& pool,
                     TunnelLampWorld& world, double radius) {
    // Perf: skip the whole pass with zero lamps alive (a cheap O(lamps) scan,
    // not the O(rounds*lamps) the pass would otherwise pay every tick once
    // the tube's lamp string is fully shot out).
    bool any_alive = false;
    for (bool a : world.alive) {
        if (a) {
            any_alive = true;
            break;
        }
    }
    if (!any_alive) return 0;

    // Perf broad-phase: keep the alive-lamp clusters current before scanning
    // rounds against them. Rebuilt lazily (init or a kill upstream marks it
    // stale); NOT rebuilt mid-loop below on a kill mid-call — a stale cluster
    // set built before that kill still ENCLOSES every remaining alive lamp (a
    // dead lamp only ever shrinks the true set), so it stays a valid, if
    // slightly loose, conservative bound for the rest of this call.
    if (world.cloud_stale) world.rebuild_cloud(radius);

    int killed = 0;
    for (weapon::Projectile& p : pool) {
        if (!p.active) continue;
        // Clusters are built in INDEX order, so iterating them in order and
        // breaking on the first hit yields the LOWEST-index alive lamp hit
        // (the one-kill-per-round rule), exactly as the single flat loop did.
        // A genuine hit's lamp is enclosed (by construction, +margin) in its
        // own cluster sphere, so a cluster the round misses can never contain a
        // real hit — the early-out never rejects a true hit. Mutation lever:
        // dropping the `+margin` term in rebuild_cloud (testing against the
        // bare lamp-extent sphere) makes this early-out reject a round that
        // grazes a cluster boundary but is still genuinely within `radius` of
        // the nearest lamp — caught by the "grazes the cloud boundary" test.
        bool hit = false;
        for (const TunnelLampWorld::LampCluster& c : world.clusters) {
            // Broad-phase early-out: a round whose swept segment misses this
            // cluster sphere cannot be within `radius` of any of its alive
            // lamps — skip its per-lamp inner loop entirely.
            if (!seg_sphere_hit(p.prev_pos, p.pos, c.center, c.radius))
                continue;
            for (std::size_t li = c.lo; li < c.hi; ++li) {
                if (!world.alive[li]) continue;
                if (seg_sphere_hit(p.prev_pos, p.pos, world.lamps[li].pos,
                                   radius)) {
                    world.alive[li] = false;  // dark spot at this lamp
                    world.dirty = true;
                    world.cloud_stale = true;  // alive set shrank; rebuild next
                    p.active = false;          // the round is consumed
                    ++killed;
                    hit = true;
                    break;  // one round kills at most one lamp
                }
            }
            if (hit) break;  // stop at the lowest-index cluster with a hit
        }
    }
    return killed;
}

}  // namespace render
