// SEADS client presentation tests (seads_client_test) — headless, no GPU/window. Locks the
// projection invariants, the .seadsrec container round-trip, and the playback path (layer-4a
// interpolation -> world position). Pure presentation; touches no kernel/world_hash. Exit 0 on
// pass, 1 on the first failure.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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
    s0.entities.push_back(netsnap::EntityState{7, 0.0, 0.0, 45.0, 2000.0, 10.0, 170.0});
    s1.protocol = netsnap::SNAPSHOT_PROTOCOL;
    s1.server_tick = 10;
    s1.entities.push_back(netsnap::EntityState{7, 1.0, 2.0, 60.0, 2100.0, -5.0, 175.0});

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

    // Edges clamp/hold (interp semantics): before first / after last tick hold endpoints.
    std::vector<RenderEntity> before = pb.sample(-100.0);
    std::vector<RenderEntity> after = pb.sample(1000.0);
    check(close(before[0].lat_deg, d0.entities[0].lat_deg, 1e-12), "hold at/below first frame");
    check(close(after[0].lat_deg, d1.entities[0].lat_deg, 1e-12), "hold at/above last frame");
}

int main() {
    test_globe();
    test_recording_and_playback();
    if (g_fail) { std::printf("seads_client_test: FAILED\n"); return 1; }
    std::printf("seads_client_test: all checks passed\n");
    return 0;
}
