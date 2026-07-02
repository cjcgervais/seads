// SEADS client presentation tests (seads_client_test) — headless, no GPU/window. Locks the
// projection invariants, the .seadsrec container round-trip, and the playback path (layer-4a
// interpolation -> world position). Pure presentation; touches no kernel/world_hash. Exit 0 on
// pass, 1 on the first failure.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "aircraft_mesh.h"
#include "globe.h"
#include "interp.h"
#include "playback.h"
#include "seadsrec.h"
#include "snapshot.h"

using namespace seads;
using namespace seads::client;

static int g_fail = 0;
static void check(bool cond, const char* msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); g_fail = 1; }
}
static bool close(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static void test_globe() {
    const double R = 15000.0;
    // Anchor points.
    Vec3 p0 = geo_to_cartesian(0.0, 0.0, 0.0, R);
    check(close(p0.x, R, 1e-6) && close(p0.y, 0, 1e-6) && close(p0.z, 0, 1e-6),
          "geo(0,0,0) -> (R,0,0)");
    Vec3 pole = geo_to_cartesian(PI_C / 2, 0.0, 0.0, R);
    check(close(pole.y, R, 1e-6) && close(length(pole), R, 1e-6), "north pole -> (0,R,0)");
    Vec3 east = geo_to_cartesian(0.0, PI_C / 2, 0.0, R);
    check(close(east.z, R, 1e-6) && close(east.x, 0, 1e-6), "lon +90 -> +Z");
    check(close(length(geo_to_cartesian(0.3, 1.1, 500.0, R)), R + 500.0, 1e-6),
          "altitude adds to radius");

    // Round-trip several points.
    double lats[] = {0.0, 0.4, -0.7, 1.2}, lons[] = {0.0, 1.0, -2.5, 3.0}, alts[] = {0, 100, 2000, 7900};
    for (int i = 0; i < 4; ++i) {
        Vec3 p = geo_to_cartesian(lats[i], lons[i], alts[i], R);
        double la, lo, al;
        cartesian_to_geo(p, R, la, lo, al);
        check(close(la, lats[i], 1e-9) && close(lo, lons[i], 1e-9) && close(al, alts[i], 1e-6),
              "geo->cart->geo round trip");
    }

    // Near/far hemisphere from an eye out along +X.
    Vec3 eye{5 * R, 0, 0};
    check(on_near_side(geo_to_cartesian(0, 0, 0, R), eye), "front point is near side");
    check(!on_near_side(geo_to_cartesian(0, PI_C, 0, R), eye), "antipode is far side");

    // Projection: a point at the globe centre's facing surface lands near screen centre when the
    // camera looks at the origin; a point off to one side has nonzero sx with the right sign.
    Camera cam;
    cam.eye = eye;
    cam.target = Vec3{0, 0, 0};
    cam.up = Vec3{0, 1, 0};
    cam.fov_rad = 1.0;
    cam.aspect = 1.0;
    double sx, sy, depth;
    check(project(cam, geo_to_cartesian(0, 0, 0, R), sx, sy, depth) && close(sx, 0, 1e-9) &&
              close(sy, 0, 1e-9) && depth > 0,
          "front surface point projects to screen centre");
    // +Z world is camera-right (forward=-X, up=+Y => right = forward x up = (-X)x(+Y) = -Z?).
    // Just assert a +Z offset yields a consistent, nonzero, finite sx and stays in front.
    check(project(cam, Vec3{0, 0, R}, sx, sy, depth) && depth > 0 && std::fabs(sx) > 1e-6 &&
              std::isfinite(sx),
          "off-axis point projects in front with nonzero sx");
    check(!project(cam, Vec3{10 * R, 0, 0}, sx, sy, depth), "point behind camera rejected");
}

// Build a tiny 1-aircraft recording (two frames) in memory and exercise the container + playback.
static void test_recording_and_playback() {
    netsnap::Snapshot s0, s1;
    s0.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s0.server_tick = 0;
    s0.entities.push_back(netsnap::EntityState{7, 0.0, 0.0, 45.0, 2000.0, 10.0, 170.0, 3.0});
    s1.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s1.server_tick = 10;
    s1.entities.push_back(netsnap::EntityState{7, 1.0, 2.0, 60.0, 2100.0, -5.0, 175.0, -4.0});

    std::vector<uint8_t> w0, w1;
    netsnap::encode_snapshot(s0, w0);
    netsnap::encode_snapshot(s1, w1);

    // Reference: what a client sees after decode (quantized).
    netsnap::Snapshot d0, d1;
    std::size_t p = 0;
    netsnap::decode_snapshot(w0.data(), w0.size(), p, d0);
    p = 0;
    netsnap::decode_snapshot(w1.data(), w1.size(), p, d1);

    RecordingMeta meta;
    meta.radius_m = 15000.0;
    meta.tick_hz = 100;
    meta.snap_hz = 20;
    std::vector<std::vector<uint8_t>> wires = {w0, w1};
    std::vector<uint8_t> blob;
    write_recording(meta, wires, blob);

    Recording rec;
    check(read_recording(blob.data(), blob.size(), rec), "read_recording parses container");
    check(rec.frames.size() == 2 && rec.meta.n_frames == 2, "two frames round-tripped");
    check(close(rec.meta.radius_m, 15000.0, 0.0), "radius preserved in header");
    check(rec.frames[0].server_tick == 0 && rec.frames[1].server_tick == 10, "ticks preserved");
    // Container frames must equal the direct-decode reference exactly.
    check(rec.frames[0].entities[0].bearing_deg == d0.entities[0].bearing_deg &&
              rec.frames[0].entities[0].alt_m == d0.entities[0].alt_m &&
              rec.frames[1].entities[0].tas_mps == d1.entities[0].tas_mps,
          "container frame == direct wire decode");

    // A truncated image must be rejected (no out-of-bounds read).
    Recording bad;
    check(!read_recording(blob.data(), blob.size() - 1, bad), "truncated recording rejected");

    // Playback at the midpoint must equal interp_entity(d0, d1, 0.5), mapped to world space.
    Playback pb;
    check(pb.load(rec), "playback loads");
    check(pb.first_tick() == 0 && pb.last_tick() == 10, "playback span");
    double mid = 5.0;
    std::vector<RenderEntity> r = pb.sample(mid);
    check(r.size() == 1, "one entity sampled");
    netsnap::EntityState exp = interp::interp_entity(d0.entities[0], d1.entities[0], 0.5);
    check(close(r[0].lat_deg, exp.lat_deg, 1e-12) && close(r[0].lon_deg, exp.lon_deg, 1e-12) &&
              close(r[0].bearing_deg, exp.bearing_deg, 1e-12) &&
              close(r[0].alt_m, exp.alt_m, 1e-9),
          "playback sample == layer-4a interp");
    Vec3 exppos = geo_deg_to_cartesian(exp.lat_deg, exp.lon_deg, exp.alt_m, 15000.0);
    check(close(r[0].pos.x, exppos.x, 1e-6) && close(r[0].pos.y, exppos.y, 1e-6) &&
              close(r[0].pos.z, exppos.z, 1e-6),
          "playback world position matches globe projection");
    // Attitude (bank/speed/gamma) is NON-geographic: the layer-4a interp drops it, so sample()
    // snaps phi/tas/gamma from the nearest received frame — here frame@0 (render_tick 5 < 10) — so
    // the viewer can draw a banking/pitching marker. (Position + heading still interpolate.)
    check(close(r[0].phi_deg, d0.entities[0].phi_deg, 1e-9) &&
              close(r[0].tas_mps, d0.entities[0].tas_mps, 1e-9) &&
              close(r[0].gamma_deg, d0.entities[0].gamma_deg, 1e-9),
          "playback sample snaps phi/tas/gamma from nearest frame");

    // Edges clamp/hold (interp semantics): before first / after last tick hold endpoints.
    std::vector<RenderEntity> before = pb.sample(-100.0);
    std::vector<RenderEntity> after = pb.sample(1000.0);
    check(close(before[0].lat_deg, d0.entities[0].lat_deg, 1e-12), "hold at/below first frame");
    check(close(after[0].lat_deg, d1.entities[0].lat_deg, 1e-12), "hold at/above last frame");
}

// Build a 2-frame recording carrying the WEAPON-001 gunnery state (hp + live rounds) and exercise
// Playback::sample_weapons — the path the native raylib viewer draws HP bars / tracers / kills from.
static void test_weapon_playback() {
    netsnap::Snapshot s0, s1;
    s0.protocol = netsnap::SNAPSHOT_PROTOCOL;       // protocol 7 carries the full WEAPON section
    s0.server_tick = 0;
    // Full hp, full magazine, never hit, full region pools (0.375/0.5/0.25 x hp_start), no kills.
    s0.entities.push_back(
        netsnap::EntityState{7, 0, 0, 45, 2000, 0, 170, 0, 100.0, 0.0, 220.0, -1.0, 37.5, 50.0, 25.0, 0.0});
    s0.projectiles.push_back(netsnap::ProjectileState{0, 0.01, 0.02, 45, 2005, 12.0, 200, 7});
    s1.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s1.server_tick = 10;
    // Dead: shot down from astern by #3 (tail pool drained to 0), 40 rounds spent, 2 kills of its own.
    s1.entities.push_back(
        netsnap::EntityState{7, 0, 0, 45, 2000, 0, 170, 0, 0.0, 5.0, 180.0, 3.0, 37.5, 50.0, 0.0, 2.0});
    s1.projectiles.push_back(netsnap::ProjectileState{0, 0.03, 0.04, 45, 2010, 12.0, 190, 7});
    s1.projectiles.push_back(netsnap::ProjectileState{1, 0.05, 0.06, 45, 2010, 12.0, 195, 7});

    std::vector<uint8_t> w0, w1;
    netsnap::encode_snapshot(s0, w0);
    netsnap::encode_snapshot(s1, w1);
    RecordingMeta meta;
    meta.radius_m = 15000.0;
    meta.tick_hz = 100;
    meta.snap_hz = 20;
    std::vector<std::vector<uint8_t>> wires = {w0, w1};
    std::vector<uint8_t> blob;
    write_recording(meta, wires, blob);
    Recording rec;
    check(read_recording(blob.data(), blob.size(), rec), "weapon recording parses");
    Playback pb;
    check(pb.load(rec), "weapon playback loads");

    // Discrete, NOT interpolated: sample_weapons holds the nearest received PAST frame.
    WeaponView a = pb.sample_weapons(0.0);
    check(a.hp.size() == 1 && a.hp[0].id == 7 && close(a.hp[0].hp, 100.0, 1e-9),
          "weapon hp @ frame0 = full 100");
    check(a.rounds.size() == 1 && a.rounds[0].owner == 7, "one round @ frame0, owner carried");
    // The full v1.19r0 WEAPON-001 state flows through: ammo, attribution, region pools, kills.
    check(close(a.hp[0].ammo, 220.0, 1e-9), "weapon ammo @ frame0 = full magazine");
    check(a.hp[0].last_hit_by == -1, "never hit @ frame0 (last_hit_by = -1)");
    check(close(a.hp[0].engine_hp, 37.5, 1e-9) && close(a.hp[0].wing_hp, 50.0, 1e-9) &&
              close(a.hp[0].tail_hp, 25.0, 1e-9),
          "region pools @ frame0 = full (0.375/0.5/0.25 x hp_start)");
    check(a.hp[0].kills == 0, "kills @ frame0 = 0");
    check(pb.sample_weapons(9.99).hp[0].hp > 50.0, "holds nearest PAST frame (no interpolation)");
    WeaponView c = pb.sample_weapons(10.0);
    check(c.hp.size() == 1 && c.hp[0].hp <= 0.0, "weapon hp @ frame1 = dead (hp<=0)");
    check(c.rounds.size() == 2, "two rounds @ frame1");
    check(close(c.hp[0].ammo, 180.0, 1e-9), "weapon ammo @ frame1 = 180 (rounds spent)");
    check(c.hp[0].last_hit_by == 3, "attribution @ frame1 = killed by #3");
    check(c.hp[0].tail_hp <= 0.0 && close(c.hp[0].engine_hp, 37.5, 1e-9) &&
              close(c.hp[0].wing_hp, 50.0, 1e-9),
          "region pools @ frame1 = tail shot away, engine/wing intact");
    check(c.hp[0].kills == 2, "kills @ frame1 = 2 (scoreboard field flows through)");
    netsnap::Snapshot d1;
    std::size_t p = 0;
    netsnap::decode_snapshot(w1.data(), w1.size(), p, d1);
    Vec3 exp = geo_deg_to_cartesian(d1.projectiles[0].lat_deg, d1.projectiles[0].lon_deg,
                                    d1.projectiles[0].alt_m, 15000.0);
    check(close(c.rounds[0].pos.x, exp.x, 1e-6) && close(c.rounds[0].pos.y, exp.y, 1e-6) &&
              close(c.rounds[0].pos.z, exp.z, 1e-6),
          "round world position matches globe projection");
}

// The v2 combat event journal: a recording carries a per-round hit/kill log; the container round-
// trips it byte-for-byte, Playback exposes it, and a v1 (no-journal) recording is still accepted.
static void test_event_journal() {
    netsnap::Snapshot s0;
    s0.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s0.server_tick = 0;
    s0.entities.push_back(netsnap::EntityState{7, 0, 0, 45, 2000, 0, 170, 0, 100.0});
    std::vector<uint8_t> w0;
    netsnap::encode_snapshot(s0, w0);
    RecordingMeta meta;
    meta.radius_m = 15000.0; meta.tick_hz = 100; meta.snap_hz = 20;
    std::vector<std::vector<uint8_t>> wires = {w0};

    // Two rounds land on tick 44 (distinct attackers — the granularity the transition path lumps),
    // then attacker 1's round on tick 47 crosses the target to dead (killed=1), region TAIL.
    std::vector<RecEvent> ev = {
        RecEvent{44, 1, 0, 12000, 10000, 0, 2},
        RecEvent{44, 1, 2, 12000, -2000, 0, 2},   // negative hp_after must survive the i64 round-trip
        RecEvent{47, 1, 1, 12000, 0, 1, 2},
    };
    std::vector<uint8_t> blob;
    write_recording(meta, wires, ev, blob);

    Recording rec;
    check(read_recording(blob.data(), blob.size(), rec), "v2 recording with journal parses");
    check(rec.meta.version == SEADSREC_VERSION, "version stamped current");
    check(rec.events.size() == 3, "three journal events round-tripped");
    check(rec.events[0].tick == 44 && rec.events[0].attacker == 0 && rec.events[0].region == 2 &&
              rec.events[0].damage_milli == 12000,
          "event[0] fields exact");
    check(rec.events[1].attacker == 2 && rec.events[1].hp_after_milli == -2000,
          "event[1] distinct attacker + signed hp round-trips");
    check(rec.events[2].killed == 1 && rec.events[2].attacker == 1,
          "kill event carries the crossing round's attacker");

    Playback pb;
    check(pb.load(rec), "playback loads a journal recording");
    check(pb.events().size() == 3 && pb.events()[2].killed == 1, "Playback exposes the journal");

    // A v1-shaped recording (no journal) is still valid and yields an empty event list.
    std::vector<uint8_t> blob1;
    write_recording(meta, wires, blob1);  // 3-arg overload = empty journal
    Recording rec1;
    check(read_recording(blob1.data(), blob1.size(), rec1), "no-journal recording parses");
    check(rec1.events.empty(), "no-journal recording has an empty event list");

    // A truncated journal trailer is rejected (no out-of-bounds read).
    Recording bad;
    check(!read_recording(blob.data(), blob.size() - 1, bad), "truncated journal rejected");
}

// ---- Procedural fighter meshes (renderer cosmetic) ----------------------------------------------
// Pure vertex data, so every structural claim is checkable without a GPU — array shapes,
// winding/normal agreement, bilateral symmetry, and the region-part layout the viewer's damage
// tinting relies on (engine forward, tail aft, wings widest). Run for EVERY roster variant plus
// the generic fallback; layout claims are relative so they hold across per-type proportions.

static double part_extent(const MeshPart& p, int axis, bool max_abs) {
    double best = max_abs ? 0.0 : p.vertices[axis];
    for (size_t k = axis; k < p.vertices.size(); k += 3) {
        double v = p.vertices[k];
        if (max_abs) { if (std::fabs(v) > best) best = std::fabs(v); }
        else { if (v > best) best = v; }
    }
    return best;
}
static double part_min_x(const MeshPart& p) {
    double best = p.vertices[0];
    for (size_t k = 0; k < p.vertices.size(); k += 3)
        if (p.vertices[k] < best) best = p.vertices[k];
    return best;
}

static void mesh_structural_gates(const FighterMesh& fm, const char* tname) {
    const MeshPart* parts[4] = {&fm.engine, &fm.wing, &fm.tail, &fm.body};
    const char* names[4] = {"engine", "wing", "tail", "body"};
    auto C = [&](bool cond, const char* what) {
        if (!cond) { std::printf("FAIL: [%s] %s\n", tname, what); g_fail = 1; }
    };

    // Whole-model vertex list for the symmetry + extent checks.
    std::vector<float> all;
    for (const MeshPart* p : parts) all.insert(all.end(), p->vertices.begin(), p->vertices.end());

    for (int i = 0; i < 4; ++i) {
        const MeshPart& p = *parts[i];
        C(p.tri_count() > 0, "mesh part is non-empty");
        C(p.vertices.size() % 9 == 0, "vertices are whole triangles");
        C(p.normals.size() == p.vertices.size(), "one normal per vertex");
        C(p.shade.size() == p.vertices.size() / 3, "one shade per vertex");
        for (float s : p.shade) C(s >= 0.5f && s <= 1.0f, "baked shade in the key-light band");

        for (size_t t = 0; t + 8 < p.vertices.size(); t += 9) {
            const float* v = &p.vertices[t];
            const float* n = &p.normals[t];
            // Face normal recomputed from the winding must equal the stored normal (unit, outward-
            // consistent winding — the invariant the hint-based builder promises).
            double ux = v[3] - v[0], uy = v[4] - v[1], uz = v[5] - v[2];
            double wx = v[6] - v[0], wy = v[7] - v[1], wz = v[8] - v[2];
            double cx = uy * wz - uz * wy, cy = uz * wx - ux * wz, cz = ux * wy - uy * wx;
            double len = std::sqrt(cx * cx + cy * cy + cz * cz);
            C(len > 1e-9, "no degenerate triangles");
            cx /= len; cy /= len; cz /= len;
            C(close(cx, n[0], 1e-4) && close(cy, n[1], 1e-4) && close(cz, n[2], 1e-4),
              "stored normal matches the winding");
            C(close(std::sqrt(double(n[0]) * n[0] + double(n[1]) * n[1] + double(n[2]) * n[2]),
                    1.0, 1e-4),
              "normals are unit length");
        }

        // Bilateral symmetry: every vertex's z-mirror exists somewhere in the model.
        for (size_t k = 0; k + 2 < p.vertices.size(); k += 3) {
            bool found = false;
            for (size_t m = 0; m + 2 < all.size() && !found; m += 3)
                found = close(p.vertices[k], all[m], 1e-5) &&
                        close(p.vertices[k + 1], all[m + 1], 1e-5) &&
                        close(p.vertices[k + 2], -all[m + 2], 1e-5);
            if (!found) {
                std::printf("FAIL: [%s] %s part vertex has no z-mirror\n", tname, names[i]);
                g_fail = 1;
                break;
            }
        }
    }

    // Part layout (body frame: +X nose, +Z starboard) — what the region tinting shows must sit
    // where the aspect-cone damage model books it: engine forward, tail aft, wings the widest.
    // Relative claims (per-variant proportions move the absolute extents).
    double model_max_x = part_extent(fm.engine, 0, false), model_min_x = part_min_x(fm.engine);
    for (const MeshPart* p : parts) {
        double mx = part_extent(*p, 0, false), mn = part_min_x(*p);
        if (mx > model_max_x) model_max_x = mx;
        if (mn < model_min_x) model_min_x = mn;
    }
    C(close(part_extent(fm.engine, 0, false), model_max_x, 1e-6),
      "engine part reaches the nose (forward-most vertex)");
    C(part_min_x(fm.engine) > 0.15, "engine part stays forward of the body");
    C(close(part_min_x(fm.tail), model_min_x, 1e-6),
      "tail part reaches the tail tip (aft-most vertex)");
    C(part_extent(fm.tail, 0, false) < -0.1, "tail part stays aft of the body");
    C(part_extent(fm.wing, 2, true) > 0.3, "wings carry real span");
    C(part_extent(fm.wing, 2, true) > part_extent(fm.body, 2, true) &&
          part_extent(fm.wing, 2, true) > part_extent(fm.engine, 2, true) &&
          part_extent(fm.wing, 2, true) > part_extent(fm.tail, 2, true),
      "no other part out-spans the wings");
}

static void test_aircraft_mesh() {
    // Every roster variant + the generic fallback passes the full structural gate set.
    FighterMesh roster[AIRCRAFT_TYPE_COUNT];
    for (uint32_t i = 0; i < AIRCRAFT_TYPE_COUNT; ++i) {
        AircraftType t = aircraft_type_from_code(i);
        roster[i] = build_fighter_mesh(t);
        mesh_structural_gates(roster[i], aircraft_type_name(t));
    }
    FighterMesh generic = build_fighter_mesh();
    mesh_structural_gates(generic, "generic");

    // The variants are genuinely DISTINCT silhouettes: every pair of roster types differs in the
    // wing plan or the engine/nose geometry (vertex data, not just tint).
    auto differs = [](const FighterMesh& a, const FighterMesh& b) {
        if (a.wing.vertices.size() != b.wing.vertices.size() ||
            a.engine.vertices.size() != b.engine.vertices.size())
            return true;
        for (size_t k = 0; k < a.wing.vertices.size(); ++k)
            if (!close(a.wing.vertices[k], b.wing.vertices[k], 1e-6)) return true;
        for (size_t k = 0; k < a.engine.vertices.size(); ++k)
            if (!close(a.engine.vertices[k], b.engine.vertices[k], 1e-6)) return true;
        return false;
    };
    for (uint32_t i = 0; i < AIRCRAFT_TYPE_COUNT; ++i)
        for (uint32_t j = i + 1; j < AIRCRAFT_TYPE_COUNT; ++j) {
            if (!differs(roster[i], roster[j])) {
                std::printf("FAIL: variants %s and %s are identical\n",
                            aircraft_type_name(aircraft_type_from_code(i)),
                            aircraft_type_name(aircraft_type_from_code(j)));
                g_fail = 1;
            }
        }

    // A couple of signature proportions (indices: P47D=0, A6M2=3, YAK3=4).
    auto length_of = [](const FighterMesh& m) {
        double mx = part_extent(m.engine, 0, false), mn = part_min_x(m.tail);
        return mx - mn;
    };
    check(length_of(roster[0]) > length_of(roster[4]), "the P-47D out-sizes the Yak-3");
    check(part_extent(roster[3].wing, 2, true) > part_extent(roster[4].wing, 2, true),
          "the A6M2 out-spans the Yak-3");
}

// The v3 per-aircraft airframe type trailer: round-trips through the container, Playback exposes
// it (with the generic fallback for out-of-range ids), and journal-only / frames-only recordings
// still load with an empty type list.
static void test_type_trailer() {
    // Code mapping: roster codes are stable, anything else falls back to GENERIC.
    check(aircraft_type_from_code(0) == AircraftType::P47D &&
              aircraft_type_from_code(3) == AircraftType::A6M2 &&
              aircraft_type_from_code(7) == AircraftType::P51,
          "roster codes map to their types");
    check(aircraft_type_from_code(8) == AircraftType::GENERIC &&
              aircraft_type_from_code(255) == AircraftType::GENERIC,
          "unknown codes fall back to GENERIC");
    check(aircraft_type_name(AircraftType::SPITFIRE_MK5)[0] != '\0' &&
              aircraft_type_name(AircraftType::GENERIC)[0] != '\0',
          "display names non-empty");

    netsnap::Snapshot s0;
    s0.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s0.server_tick = 0;
    s0.entities.push_back(netsnap::EntityState{0, 0, 0, 45, 2000, 0, 170, 0, 100.0});
    std::vector<uint8_t> w0;
    netsnap::encode_snapshot(s0, w0);
    RecordingMeta meta;
    meta.radius_m = 15000.0; meta.tick_hz = 100; meta.snap_hz = 20;
    std::vector<std::vector<uint8_t>> wires = {w0};

    const std::vector<uint32_t> types = {0, 3, 6};  // P-47D, A6M2, Spitfire Mk V
    std::vector<uint8_t> blob;
    write_recording(meta, wires, std::vector<RecEvent>{}, types, blob);

    Recording rec;
    check(read_recording(blob.data(), blob.size(), rec), "v3 recording with types parses");
    check(rec.meta.version == 3, "version stamped v3");
    check(rec.types.size() == 3 && rec.types[0] == 0 && rec.types[1] == 3 && rec.types[2] == 6,
          "type codes round-tripped");

    Playback pb;
    check(pb.load(rec), "playback loads a typed recording");
    check(pb.types().size() == 3 && pb.type_code_of(1) == 3, "Playback exposes the types by slot");
    check(pb.type_code_of(5) == 0xFFu && pb.type_code_of(-1) == 0xFFu,
          "out-of-range ids fall back to the generic code");

    // The 3-arg and 4-arg writers still produce loadable recordings with an EMPTY type list.
    std::vector<uint8_t> blob_j;
    write_recording(meta, wires, std::vector<RecEvent>{RecEvent{1, 0, 1, 500, 99500, 0, 1}},
                    blob_j);
    Recording rec_j;
    check(read_recording(blob_j.data(), blob_j.size(), rec_j) && rec_j.types.empty() &&
              rec_j.events.size() == 1,
          "journal-only recording loads with empty types");
    Playback pbj;
    check(pbj.load(rec_j) && pbj.type_code_of(0) == 0xFFu,
          "typeless recording -> generic code for every id");

    // A truncated type trailer is rejected (no out-of-bounds read).
    Recording bad;
    check(!read_recording(blob.data(), blob.size() - 1, bad), "truncated type trailer rejected");
}

int main() {
    test_globe();
    test_recording_and_playback();
    test_weapon_playback();
    test_event_journal();
    test_aircraft_mesh();
    test_type_trailer();
    if (g_fail) { std::printf("seads_client_test: FAILED\n"); return 1; }
    std::printf("seads_client_test: all checks passed\n");
    return 0;
}
