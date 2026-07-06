// SEADS raylib globe viewer (seads_viewer) — Step 5, OPTIONAL (built only with -DSEADS_CLIENT=ON).
//
// A thin, DOWNSTREAM-ONLY shell over the seads_client lib: it loads a .seadsrec recording, feeds
// the decoded 20 Hz wire frames through the layer-4a interpolation buffer, and draws the result
// on a 3D globe. It is read-only — it NEVER advances a kernel, hashes, or writes the sim, so it
// lives entirely outside the determinism gate and cannot affect a world_hash (no seal). The only
// thing it does that the kernel may not: read the wall clock (raylib GetTime()) to choose a render
// time ~100 ms in the past, which is exactly the layer-4a presentation delay.
//
// Headless self-check (no window/GPU, runnable anywhere):
//   seads_viewer flight.seadsrec --selfcheck 8
// GUI (replay):
//   seads_viewer flight.seadsrec [--speed 1.0]
//
// --- FLY mode (Track A: live local-input loop, netcode layer 4b) ---------------------------------
// With --fly the OWN aircraft is no longer replayed: it is FLOWN. Local input maps to a
// seads::Command that drives a predict::Predictor — the same client-side prediction harness used in
// netcode layer 4b, running the REAL sealed kernel at a fixed 100 Hz from the wall clock. The
// remotes keep coming from the recording on the layer-4a interpolation path (Playback), so
// prediction (own, crisp, zero-latency) and interpolation (remote, ~100 ms in the past) are visible
// on the same globe at once — the full 4a+4b loop. The M key cycles the REMOTE render mode:
// INTERP (layer 4a, the default), PREDICT (layer 24 — dead-reckon each remote to "now" by seeding
// the sealed no-arg kernel tail from the freshest snapshot), or SMOOTH (layer 25 — blend the drawn
// remote toward each reseed to hide the maneuver pop). The HUD shows the coast-vs-interp "now"
// error live. Both replay and fly draw this; it is a render-only coast (never fed back to the sim). The remotes' WEAPON-001 gunnery state rides the
// same decoded wire (seal v1.19r0 put every field on it): tracer rounds, per-aircraft hp + E/W/T
// region damage bars, a kills/ammo scoreboard, and an attributed kill-feed ("#0 downed #1",
// "#0 knocked out #1's ENGINE") derived from wire-frame transitions. Both HUDs also surface the
// flight-model atmosphere (presentation-side): sigma(alt) (v1.21r0 ISA density ratio) and the
// engine power state pwr = min(1, sigma/sigma_crit) (v1.22r0 supercharger lapse), plus static
// airframe data on the scoreboard — region toughness in eighths (v1.20r0, recovered from the
// wire's first-frame pools) and the critical altitude (from the v3 type trailer's envelope).
//   seads_viewer flight.seadsrec --fly [--speed 1.0]
//   seads_viewer --fly --selfcheck 6        (headless; no recording or GPU needed)
//
// CAMERA + CONTROLS (GUI --fly):
//   * CHASE CAM — the camera rides behind/above the own ship, following its heading; the mouse
//     wheel zooms in/out of the plane focus.
//   * MOUSE-AIM (default) — a central aim reticle follows the mouse inside a circular zone; the
//     plane flies toward it (reticle right/left -> bank, up/down -> climb). Auto-coordinated:
//     bank produces a coordinated turn (heading is slaved to bank by the kernel).
//   * FREE-LOOK (hold SPACE) — mouse-aim is suspended; the mouse pans the camera freely around the
//     plane (all the way around, and up/down). Releasing SPACE restabilizes the chase cam. While
//     in free-look the plane is flown by KEYBOARD ONLY (these keys also work as gross input in
//     mouse-aim mode):
//         A/D = bank left/right   W/S = pitch (push/pull g)   Q/E = yaw   Shift/Ctrl = throttle
//     B2 (seal v1.6r0): the kernel's pitch axis is a commanded LOAD FACTOR g (Command.target_g);
//     W/S pull/push g, the kernel integrates the real flight-path angle gamma (the marker now
//     tilts by the TRUE gamma — the old pitch-cue exaggeration is gone). A/D->bank, W/S->g and
//     Shift/Ctrl->throttle are all real kernel inputs. Yaw (Q/E) still has NO kernel axis (the
//     kernel turns only by banking, auto-coordinated), so it alone is applied DOWNSTREAM by
//     re-seeding the predictor's own-state through the public Kernel API (the same path a netcode
//     reconcile uses) — kernel math, snapshot layout, rails and golden untouched (no seal).
//     P pauses, R resets.
//
// Still DOWNSTREAM-ONLY: input feeds Commands into the kernel-driving Predictor, never the wire;
// no rail/golden/world_hash is touched (no seal). NOTE: there is no authoritative server in this
// single-process viewer, so only Predictor::predict() runs here — reconcile() (snap+replay against
// a server snapshot) is exercised by the layer-4b parity tests and engages against a real server.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "aircraft_mesh.h"
#include "playback.h"
#include "remote_coaster.h"   // seads::client::coast_to_now / RemoteCoasterSet (layer 24/25 on the HUD)
#include "seadsrec.h"
#include "snapshot.h"         // seads::netsnap (decode the freshest snapshot to seed the coast)
#include "predict.h"          // seads::predict::Predictor (netcode layer 4b)
#include "client_frame.h"     // planesphere/local-up frame adapter (mouse-aim instructor graft)
#include "aim_state.h"        // seads::client::AimState (WT-style world-frame aim + transport)
#include "instructor.h"       // seads::client::instructor_step (SOLUTION outer loop -> Command)
#include "envelope_tables.h"  // seads::envtab::* tuning envelopes
#include "golden_params.h"    // sealed rail constants (R, dt, g0, ceiling)
#include "raylib.h"

using namespace seads;
using namespace seads::client;

namespace {

constexpr float DISPLAY_R = 10.0f;  // globe radius in raylib units (world metres are scaled down)

// FLY-mode tuning (presentation-only; the kernel re-clamps every command to the envelope anyway).
constexpr double RAD2DEG_V = 180.0 / PI_C;            // PI_C from globe.h
constexpr double DEG2RAD_V = PI_C / 180.0;
constexpr double MAX_BANK_RAD = 60.0 * DEG2RAD_V;     // commanded bank at full A/D (or reticle x)
// B2 pitch axis: the stick commands a LOAD FACTOR g (Command.target_g), not a climb rate. The
// pitch axis p in [-1,1] (W/S or reticle y) maps to g = 1 + p*G_GAIN (1 g neutral = hold level
// wings-level); clamped at the stick, then the kernel re-clamps to its structural envelope.
constexpr double G_GAIN    = 2.0;                     // pitch axis [-1,1] -> g = 1 + p*2
constexpr double G_MIN_CMD = -1.0;                    // full push-over (negative g)
constexpr double G_MAX_CMD = 4.0;                     // full hard pull
constexpr double YAW_RATE_RAD = 35.0 * DEG2RAD_V;     // Q/E direct heading (yaw) rate, rad/s
constexpr double AIM_SENS = 0.0022;                  // mouse-aim sensitivity (rad of aim per pixel)

// Chase camera (display units; DISPLAY_R = 10 is the globe radius on screen).
constexpr double CHASE_DIST_DEF = 1.3;               // default distance behind the own ship
constexpr double CHASE_DIST_MIN = 0.45;              // closest zoom
constexpr double CHASE_DIST_MAX = 6.0;               // farthest zoom
constexpr double CHASE_ELEV = 0.22;                  // baseline look-down angle (rad) behind plane
constexpr float  RETICLE_ZONE_PX = 150.0f;          // radius of the mouse-aim circle (pixels)

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

// Map a world position (metres, globe frame) to raylib display units (globe radius -> DISPLAY_R).
// NOTE: the globe frame {up=+X, north=+Y, east=+Z} is a LEFT-handed ENU, which would render a
// MIRROR image (a geographic right turn would look like a left turn on screen). We un-mirror by
// reflecting Z here; local_basis() reflects Z to match, so positions and bases stay consistent.
Vector3 to_display(const Vec3& p, double radius_m) {
    float s = DISPLAY_R / static_cast<float>(radius_m);
    return Vector3{static_cast<float>(p.x) * s, static_cast<float>(p.y) * s,
                   -static_cast<float>(p.z) * s};
}

// Look up an aircraft's hp in a WeaponView by id; returns `dflt` if absent (e.g. a frame that
// predates the weapon section, or an id not present in the nearest frame).
double hp_for(const std::vector<RenderHp>& v, int64_t id, double dflt) {
    for (const auto& h : v) if (h.id == id) return h.hp;
    return dflt;
}

// Full WEAPON-001 reading by id; nullptr if absent (old frame / unknown id).
const RenderHp* weap_for(const std::vector<RenderHp>& v, int64_t id) {
    for (const auto& h : v) if (h.id == id) return &h;
    return nullptr;
}

// Airframe display name for a HUD/scoreboard row, or nullptr when the recording carries no type
// trailer (pre-v3) — those rows then stay exactly as before instead of all reading "fighter".
const char* type_name_for(const Playback& pb, int64_t id) {
    if (pb.types().empty()) return nullptr;
    return aircraft_type_name(aircraft_type_from_code(pb.type_code_of(id)));
}

// ---- Atmosphere / airframe HUD data (presentation-only) -----------------------------------------
// sigma(alt) for the HUD: the ICAO ISA troposphere power law — the SAME provenance the kernel's
// sealed 17-node LUT was generated from (tools/gen_isa_lut.py). The sealed table itself stays
// private to kernel.cpp; the law tracks it to < 2.5e-4, far below the 2-decimal HUD display, and
// the viewer is downstream-only so libm pow is fine here (det_math is a kernel rule).
double hud_sigma(double alt_m) {
    if (alt_m < 0.0) alt_m = 0.0;
    if (alt_m > golden::ATM_TOP_M) alt_m = golden::ATM_TOP_M;
    const double T0 = 288.15, L = 0.0065, RS = 287.05287;  // ICAO ISA troposphere constants
    return std::pow((T0 - L * alt_m) / T0, golden::G0 / (RS * L) - 1.0);
}

// Engine power fraction at alt. The v1.22r0 supercharger lapse min(1, sigma(alt)/sigma(crit)) —
// RATED (1.0) below the airframe's critical altitude, falling with the density ratio above it —
// generalized to the v1.25r0 two-speed blower schedule. Mirrors kernel.cpp's op-shape (this HUD
// uses libm pow for sigma, so it is not bit-exact — it is the same MODEL, drawn presentation-side).
// crit_lo_alt_m = 0 (single-speed) never enters the two-speed branch, reproducing the v1.22r0 lapse.
double hud_power(double alt_m, double crit_alt_m, double crit_lo_alt_m, double gear2_frac) {
    double sigma = hud_sigma(alt_m);
    double lapse = sigma / hud_sigma(crit_alt_m);
    if (lapse > 1.0) lapse = 1.0;
    if (crit_lo_alt_m > 0.0) {                            // two-speed: max(low gear, gear2 * high gear)
        double lo_gear = sigma / hud_sigma(crit_lo_alt_m);
        if (lo_gear > 1.0) lo_gear = 1.0;
        double hi_gear = gear2_frac * lapse;
        lapse = (lo_gear > hi_gear) ? lo_gear : hi_gear;
    }
    return lapse;
}

// .seadsrec v3 type code -> the airframe's tuning envelope (crit_alt_m for the power readout);
// nullptr for GENERIC / absent trailer. The inverse of record_main's envelope-pointer -> code map,
// in sealed roster order (the STABLE presentation codes 0-7).
const Envelope* envelope_for_code(uint32_t code) {
    switch (static_cast<AircraftType>(code)) {
        case AircraftType::P47D: return &envtab::P47D;
        case AircraftType::BF109F4: return &envtab::BF109F4;
        case AircraftType::KI61: return &envtab::KI61;
        case AircraftType::A6M2: return &envtab::A6M2;
        case AircraftType::YAK3: return &envtab::YAK3;
        case AircraftType::LA7: return &envtab::LA7;
        case AircraftType::SPITFIRE_MK5: return &envtab::SPITFIRE_MK5;
        case AircraftType::P51: return &envtab::P51;
        default: return nullptr;
    }
}

// Region-toughness eighths recovered from the wire's first-frame pools (pool = frac * hp_start,
// and every sealed fraction is a multiple of 1/8 — the v1.20r0 dyadic contract — so the rounding
// is exact). Wire-derived on purpose: it works on any protocol-7 recording, type trailer or not.
// False when the baseline predates the region fields (pre-protocol-7 all-zero pools).
bool toughness_eighths(const RenderHp& base, int out[3]) {
    if (base.hp <= 0.0) return false;
    if (base.engine_hp <= 0.0 && base.wing_hp <= 0.0 && base.tail_hp <= 0.0) return false;
    out[0] = static_cast<int>(std::lround(base.engine_hp / base.hp * 8.0));
    out[1] = static_cast<int>(std::lround(base.wing_hp / base.hp * 8.0));
    out[2] = static_cast<int>(std::lround(base.tail_hp / base.hp * 8.0));
    return true;
}

// ---- WEAPON-001 presentation helpers (shared by replay + fly) ----------------------------------
// Everything below draws from the DECODED wire only (Playback::sample_weapons) — the same bytes a
// remote client holds — so what the viewer shows is exactly what replicates (seal v1.19r0: hp,
// ammo, last_hit_by, engine/wing/tail region pools and kills all ride the snapshot wire).

const Color TRACER_C{255, 225, 74, 255};   // yellow tracer point cloud (mirrors the web viewer)
const Color FEED_KILL{235, 90, 90, 255};   // kill-feed: an aircraft downed
const Color FEED_RGN{255, 170, 60, 255};   // kill-feed: a region knocked out

// Rolling attributed kill-feed derived from wire-frame TRANSITIONS: total hp crossing to <=0 is a
// kill ("#A downed #B" — attribution from the victim's last_hit_by, by construction the killer);
// a region pool crossing to <=0 on a LIVING plane is a knock-out ("#A knocked out #B's ENGINE").
// Only downward crossings fire, so a looping replay never emits false events on the wrap.
class KillFeed {
public:
    void update(const WeaponView& wv, double now) {
        for (const auto& h : wv.hp) {
            Prev* p = nullptr;
            for (auto& q : prev_) if (q.id == h.id) { p = &q; break; }
            if (!p) {
                prev_.push_back(Prev{h.id, h.hp, h.engine_hp, h.wing_hp, h.tail_hp});
                continue;
            }
            if (p->hp > 0.0 && h.hp <= 0.0) {
                push(now, FEED_KILL, h.last_hit_by, h.id, nullptr);
            } else if (h.hp > 0.0) {  // region knock-outs only matter on a living plane
                if (p->engine > 0.0 && h.engine_hp <= 0.0) push(now, FEED_RGN, h.last_hit_by, h.id, "ENGINE");
                if (p->wing > 0.0 && h.wing_hp <= 0.0) push(now, FEED_RGN, h.last_hit_by, h.id, "WING");
                if (p->tail > 0.0 && h.tail_hp <= 0.0) push(now, FEED_RGN, h.last_hit_by, h.id, "TAIL");
            }
            p->hp = h.hp; p->engine = h.engine_hp; p->wing = h.wing_hp; p->tail = h.tail_hp;
        }
    }

    // Newest at the bottom, fading out over the last 1.5 s of an 8 s life.
    void draw(int x, int y, double now) const {
        for (const auto& it : items_) {
            double age = now - it.born;
            if (age > TTL_S) continue;
            float a = (age < TTL_S - 1.5) ? 1.0f : static_cast<float>((TTL_S - age) / 1.5);
            DrawText(it.text, x, y, 16, Fade(it.color, a));
            y += 20;
        }
    }

    void reset() { prev_.clear(); items_.clear(); }

private:
    static constexpr double TTL_S = 8.0;
    struct Prev { int64_t id; double hp, engine, wing, tail; };
    struct Item { char text[64]; double born; Color color; };

    void push(double now, Color c, int64_t attacker, int64_t victim, const char* region) {
        Item it{};
        if (region)
            std::snprintf(it.text, sizeof(it.text), "#%lld knocked out #%lld's %s",
                          static_cast<long long>(attacker), static_cast<long long>(victim), region);
        else if (attacker >= 0)
            std::snprintf(it.text, sizeof(it.text), "#%lld downed #%lld",
                          static_cast<long long>(attacker), static_cast<long long>(victim));
        else  // never-hit death shouldn't happen, but a pre-attribution frame decodes to -1
            std::snprintf(it.text, sizeof(it.text), "#%lld destroyed",
                          static_cast<long long>(victim));
        it.born = now;
        it.color = c;
        items_.push_back(it);
        if (items_.size() > 6) items_.erase(items_.begin());
    }

    std::vector<Prev> prev_;
    std::vector<Item> items_;
};

// ---- Journal-driven combat feed (layer-6 event journal, per-round @ 100 Hz) --------------------
// When a recording carries the event journal (`Playback::events()`, v2), the feed is driven by the
// EXACT per-round hit/kill records — one per connecting round, at its precise 100 Hz tick — instead
// of inferring kills from 20 Hz wire-state transitions. This gives per-hit floating damage numbers
// (region-coloured), exact attribution on every kill ("#0 downed #1"), and distinct events when two
// shooters land on the same tick — none of which the snapshot-transition path can recover. Replay
// is cursor-based (no downward-crossing heuristic): on a loop wrap the cursor repositions cleanly.
const Color RGN_COL[3] = {{235, 90, 90, 255},    // ENGINE — red
                          {255, 165, 60, 255},   // WING — orange
                          {245, 215, 70, 255}};   // TAIL — gold
const char* const RGN_NAME[3] = {"ENGINE", "WING", "TAIL"};

class CombatFeed {
public:
    void load(const std::vector<RecEvent>* ev) { ev_ = ev; reset(); }
    bool active() const { return ev_ && !ev_->empty(); }

    // Advance to render_tick, spawning feed lines + floating numbers for events newly crossed.
    // `screen_of(id, out)` fills the aircraft's current screen position and returns whether it is
    // on-screen (an off-screen hit's number is simply skipped). now = wall time for fade timing.
    void update(double render_tick, double now,
                const std::function<bool(int64_t, Vector2&)>& screen_of) {
        if (!active()) return;
        const auto& ev = *ev_;
        if (render_tick + 0.5 < last_tick_) {  // loop wrap: reposition WITHOUT emitting the history
            cursor_ = 0;
            while (cursor_ < ev.size() && static_cast<double>(ev[cursor_].tick) <= render_tick)
                ++cursor_;
        }
        while (cursor_ < ev.size() && static_cast<double>(ev[cursor_].tick) <= render_tick) {
            emit(ev[cursor_], now, screen_of);
            ++cursor_;
        }
        last_tick_ = render_tick;
    }

    void draw(int feed_x, int feed_y, double now) const {
        for (const auto& l : lines_) {
            double age = now - l.born;
            if (age > LINE_TTL) continue;
            float a = (age < LINE_TTL - 1.5) ? 1.0f : static_cast<float>((LINE_TTL - age) / 1.5);
            DrawText(l.text, feed_x, feed_y, 16, Fade(l.color, a));
            feed_y += 20;
        }
        for (const auto& f : floats_) {
            double age = now - f.born;
            if (age > FLOAT_TTL) continue;
            float a = static_cast<float>(1.0 - age / FLOAT_TTL);
            int y = static_cast<int>(f.y - age * 34.0);  // rises as it fades
            DrawText(f.text, static_cast<int>(f.x), y, 18, Fade(f.color, a));
        }
    }

    void reset() { cursor_ = 0; last_tick_ = -1e18; lines_.clear(); floats_.clear(); }

private:
    static constexpr double LINE_TTL = 8.0;
    static constexpr double FLOAT_TTL = 1.2;
    struct Line { char text[64]; double born; Color color; };
    struct Float { float x, y; char text[24]; double born; Color color; };

    void emit(const RecEvent& e, double now,
              const std::function<bool(int64_t, Vector2&)>& screen_of) {
        int rgn = (e.region >= 0 && e.region < 3) ? static_cast<int>(e.region) : 1;
        if (e.killed) {
            Line l{};
            if (e.attacker >= 0)
                std::snprintf(l.text, sizeof(l.text), "#%lld downed #%lld  (%s)",
                              static_cast<long long>(e.attacker), static_cast<long long>(e.target),
                              RGN_NAME[rgn]);
            else
                std::snprintf(l.text, sizeof(l.text), "#%lld destroyed",
                              static_cast<long long>(e.target));
            l.born = now; l.color = FEED_KILL;
            lines_.push_back(l);
            if (lines_.size() > 6) lines_.erase(lines_.begin());
        }
        // A floating per-round damage number at the struck aircraft (region-coloured), kill or not.
        Vector2 sp{};
        if (screen_of(e.target, sp)) {
            Float f{};
            f.x = sp.x + jitter(e.tick);   // spread stacked same-tick numbers so both read
            f.y = sp.y - 34.0f;
            double dmg = static_cast<double>(e.damage_milli) / 1000.0;
            std::snprintf(f.text, sizeof(f.text), "-%.0f", dmg);
            f.born = now; f.color = RGN_COL[rgn];
            floats_.push_back(f);
            if (floats_.size() > 32) floats_.erase(floats_.begin());
        }
    }

    // Deterministic small horizontal offset so two numbers on one tick don't overprint (no RNG).
    static float jitter(int64_t tick) { return static_cast<float>((tick % 5 - 2) * 12); }

    const std::vector<RecEvent>* ev_ = nullptr;
    size_t cursor_ = 0;
    double last_tick_ = -1e18;
    std::vector<Line> lines_;
    std::vector<Float> floats_;
};

// Screen-projected damage state for one aircraft: the total-hp bar plus the three E/W/T region
// segments underneath (a knocked-out region fills dark red and its letter lights up). `base` is
// the same aircraft's first-frame reading (full pools); a recording that predates the region
// fields (all-zero baseline pools) draws the hp bar only.
void draw_damage_bars(Vector2 sp, const RenderHp& w, const RenderHp& base) {
    const int bw = 46, bh = 5;
    int bx = static_cast<int>(sp.x) - bw / 2, by = static_cast<int>(sp.y) - 26;
    double mh = (base.hp > 0.0) ? base.hp : 100.0;
    bool dead = w.hp <= 0.0;
    double frac = w.hp / mh;
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    DrawRectangle(bx, by, bw, bh, Color{30, 30, 36, 200});
    Color fill = dead ? Color{120, 120, 120, 255}
                      : (frac > 0.5 ? GREEN : (frac > 0.25 ? ORANGE : RED));
    DrawRectangle(bx, by, static_cast<int>(bw * frac), bh, fill);
    DrawRectangleLines(bx, by, bw, bh, Fade(BLACK, 0.5f));
    if (dead) { DrawText("KILLED", bx - 2, by - 13, 12, FEED_KILL); return; }
    if (base.engine_hp <= 0.0 && base.wing_hp <= 0.0 && base.tail_hp <= 0.0) return;
    struct Seg { double v, v0; const char* tag; } segs[3] = {
        {w.engine_hp, base.engine_hp, "E"},
        {w.wing_hp, base.wing_hp, "W"},
        {w.tail_hp, base.tail_hp, "T"}};
    int sw = (bw - 4) / 3;
    for (int i = 0; i < 3; ++i) {
        int sx = bx + i * (sw + 2), sy = by + bh + 2;
        double f = (segs[i].v0 > 0.0) ? segs[i].v / segs[i].v0 : 0.0;
        if (f < 0) f = 0; if (f > 1) f = 1;
        bool out = segs[i].v <= 0.0;
        DrawRectangle(sx, sy, sw, 3, out ? Color{110, 35, 35, 230} : Color{30, 30, 36, 200});
        DrawRectangle(sx, sy, static_cast<int>(sw * f), 3, Color{120, 200, 140, 255});
        DrawText(segs[i].tag, sx + sw / 2 - 2, sy + 5, 10,
                 out ? FEED_KILL : Color{110, 120, 130, 255});
    }
}

// Top-right scoreboard from the decoded wire: kills / ammo / status per aircraft, kills-desc.
// Rows carry the airframe name when the recording has the v3 type trailer, plus the STATIC
// airframe data the HUD arc surfaces: region toughness E-W-T in eighths (wire-derived from the
// first-frame pools — v1.20r0 dyadic contract) and the supercharger critical altitude
// (v1.22r0 crit_alt_m, from the type trailer's envelope; blank when either is unavailable).
void draw_scoreboard(const Playback& pb, const std::vector<RenderHp>& hp,
                     const std::vector<RenderHp>& maxhp, int screen_w) {
    if (hp.empty()) return;
    std::vector<const RenderHp*> rows;
    rows.reserve(hp.size());
    for (const auto& h : hp) rows.push_back(&h);
    std::sort(rows.begin(), rows.end(), [](const RenderHp* a, const RenderHp* b) {
        if (a->kills != b->kills) return a->kills > b->kills;
        return a->id < b->id;
    });
    int x = screen_w - 470, y = 40;
    DrawText("SCORE                 kills   ammo   tough    crit", x, y, 16, RAYWHITE);
    y += 20;
    char line[112];
    for (const RenderHp* h : rows) {
        const char* tn = type_name_for(pb, h->id);
        char who[24];
        std::snprintf(who, sizeof(who), "#%lld %s", static_cast<long long>(h->id), tn ? tn : "");
        char tough[8] = "  -  ";
        int e8[3];
        const RenderHp* base = weap_for(maxhp, h->id);
        if (base && toughness_eighths(*base, e8))
            std::snprintf(tough, sizeof(tough), "%d-%d-%d", e8[0], e8[1], e8[2]);
        char crit[12] = "     --";
        const Envelope* env = pb.types().empty() ? nullptr
                                                 : envelope_for_code(pb.type_code_of(h->id));
        if (env && env->crit_lo_alt_m > 0.0)  // two-speed blower: low-gear / high-gear crit altitudes
            std::snprintf(crit, sizeof(crit), "%.1f/%.1fk", env->crit_lo_alt_m / 1000.0,
                          env->crit_alt_m / 1000.0);
        else if (env)
            std::snprintf(crit, sizeof(crit), "%5.0fm", env->crit_alt_m);
        std::snprintf(line, sizeof(line), "%-17s %3lld   %4.0f   %s   %s%s", who,
                      static_cast<long long>(h->kills), h->ammo, tough, crit,
                      h->hp <= 0.0 ? "   KIA" : (h->ammo <= 0.0 ? "   WINCHESTER" : ""));
        DrawText(line, x, y, 16, h->hp <= 0.0 ? GRAY : SKYBLUE);
        y += 20;
    }
}

// ---- Fighter models (procedural meshes, renderer cosmetic) -------------------------------------
// The pure vertex data comes from build_fighter_mesh(type) (seads_client, headlessly tested); this
// wrapper uploads each variant as four GPU meshes — ENGINE / WING / TAIL in the wire's region
// order, plus the BODY hull — so a knocked-out region can be tinted straight from the decoded
// WEAPON-001 pools: the aircraft itself becomes the damage display, beside the bars. Which
// aircraft gets which roster variant comes from the .seadsrec v3 type trailer (Playback::
// type_code_of); a recording without it draws every aircraft as the GENERIC fighter.

constexpr double PROP_SPIN_RAD_S = 24.0;      // visual prop spin; freezes when the engine is out
const Color REGION_OUT_C{110, 45, 45, 255};   // tint for a knocked-out region's part

struct FighterModel {
    Mesh mesh[4]{};    // 0=ENGINE 1=WING 2=TAIL (wire region order) 3=BODY
    Material mat{};
    bool ready = false;

    // Upload needs the GL context — call after InitWindow. Headless paths never call it, and
    // draw_aircraft falls back to the original line marker while !ready.
    void init(AircraftType type) {
        FighterMesh fmesh = build_fighter_mesh(type);
        const MeshPart* parts[4] = {&fmesh.engine, &fmesh.wing, &fmesh.tail, &fmesh.body};
        for (int i = 0; i < 4; ++i) {
            const MeshPart& p = *parts[i];
            Mesh& m = mesh[i];
            m.vertexCount = p.tri_count() * 3;
            m.triangleCount = p.tri_count();
            size_t nb = static_cast<size_t>(m.vertexCount) * 3 * sizeof(float);
            m.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(nb)));
            std::memcpy(m.vertices, p.vertices.data(), nb);
            m.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(nb)));
            std::memcpy(m.normals, p.normals.data(), nb);
            // Baked body-frame key light rides the vertex colours (the default shader multiplies
            // them by the material tint) so the low-poly form reads without a lighting shader.
            m.colors = static_cast<unsigned char*>(
                MemAlloc(static_cast<unsigned int>(m.vertexCount) * 4));
            for (int v = 0; v < m.vertexCount; ++v) {
                unsigned char g = static_cast<unsigned char>(p.shade[v] * 255.0f + 0.5f);
                m.colors[4 * v + 0] = g;
                m.colors[4 * v + 1] = g;
                m.colors[4 * v + 2] = g;
                m.colors[4 * v + 3] = 255;
            }
            UploadMesh(&m, false);
        }
        mat = LoadMaterialDefault();
        ready = true;
    }
};

// All roster variants + the generic fallback, uploaded once post-InitWindow. Indexed by the
// .seadsrec type code (unknown/absent -> the generic slot), so both viewer modes can hand
// draw_aircraft the right silhouette per aircraft id.
struct FighterModelSet {
    FighterModel models[AIRCRAFT_TYPE_COUNT + 1];  // [0..7] roster, [8] generic

    void init() {
        for (uint32_t i = 0; i < AIRCRAFT_TYPE_COUNT; ++i)
            models[i].init(static_cast<AircraftType>(i));
        models[AIRCRAFT_TYPE_COUNT].init(AircraftType::GENERIC);
    }
    FighterModel& for_code(uint32_t code) {
        return models[code < AIRCRAFT_TYPE_COUNT ? code : AIRCRAFT_TYPE_COUNT];
    }
};

// Draw a banking/pitching aircraft (definition below, shared by replay + fly): the fighter mesh
// oriented by heading/bank/pitch, region parts tinted from the decoded wire (w = current reading,
// base = first-frame full pools; either may be null — regions then draw in the aircraft colour),
// prop spun by spin_rad (frozen automatically when dead or engine-out).
void draw_aircraft(FighterModel& fm, Vector3 od, double lat, double lon, double psi, double phi,
                   double pitch, Color c, float scale, const RenderHp* w, const RenderHp* base,
                   double spin_rad);

// ---- Remote render mode: interpolate (layer 4a) / predict (layer 24) / smooth (layer 25) --------
// The viewer draws remotes by INTERPOLATION ~100 ms in the past (smooth but structurally late).
// PREDICT dead-reckons each remote to "now" by seeding the SEALED no-arg kernel tail from the
// freshest received snapshot (netcode layer 24); SMOOTH additionally BLENDS the drawn remote toward
// each reseed to hide the maneuver pop (layer 25). Cycled live with the M key; default INTERP keeps
// the original behaviour. All downstream-only — a render-side coast, never fed back to the sim.
enum class RemoteMode { INTERP, PREDICT, SMOOTH };
const char* remote_mode_name(RemoteMode m) {
    switch (m) {
        case RemoteMode::PREDICT: return "PREDICT (layer24 coast-to-now)";
        case RemoteMode::SMOOTH:  return "SMOOTH  (layer25 blended coast)";
        default:                  return "INTERP  (layer4a ~100ms late)";
    }
}
RemoteMode cycle_remote_mode(RemoteMode m) {
    return m == RemoteMode::INTERP  ? RemoteMode::PREDICT
         : m == RemoteMode::PREDICT ? RemoteMode::SMOOTH
                                    : RemoteMode::INTERP;
}
constexpr double REMOTE_SMOOTH = 0.18;  // per-frame layer-25 blend fraction (hides the reseed pop)

// Sealed rail constants -> Rails for the presentation coast (the same numbers the golden/fly use).
Rails viewer_rails() {
    Rails r;
    r.R = golden::R_M; r.dt = golden::DT_S; r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M; r.soft = golden::SOFT_M;
    return r;
}

// What to draw for one remote this frame, plus the interp/coast "now" errors for the HUD readout.
struct RemoteDraw {
    Vec3 pos;                 // world position (metres, globe frame) to draw at
    double lat_rad = 0, lon_rad = 0, psi_rad = 0, phi_rad = 0, gamma_rad = 0;  // attitude to draw
    double interp_err = -1.0; // interp-vs-truth "now" position error (m); -1 if truth unknown
    double coast_err = -1.0;  // coast-vs-truth "now" position error (m); INTERP mode leaves it -1
};

// Build a remote's display state. `interp_e` is the layer-4a sample at render_tick (now - lag) — the
// INTERP-mode draw and the interp error baseline. PREDICT/SMOOTH seed the coast from the freshest
// received snapshot at/behind now-lag and dead-reckon forward to now (layer 24); SMOOTH then blends
// the drawn tuple toward it (layer 25). `truth_pos` (pb.sample(now_tick)) feeds the error readout.
RemoteDraw remote_draw(const Playback& pb, const RenderEntity& interp_e, double now_tick, double lag,
                       const Rails& rails, RemoteMode mode, RemoteCoasterSet& smoother, double R,
                       const Vec3& truth_pos, bool have_truth) {
    RemoteDraw rd;
    rd.pos = interp_e.pos;
    rd.lat_rad = interp_e.lat_deg * DEG2RAD_V;
    rd.lon_rad = interp_e.lon_deg * DEG2RAD_V;
    rd.psi_rad = interp_e.bearing_deg * DEG2RAD_V;
    rd.phi_rad = interp_e.phi_deg * DEG2RAD_V;
    rd.gamma_rad = interp_e.gamma_deg * DEG2RAD_V;
    if (have_truth) rd.interp_err = length(interp_e.pos - truth_pos);
    if (mode == RemoteMode::INTERP) return rd;

    netsnap::EntityState es;
    int64_t st = 0;
    if (!pb.nearest_state(now_tick - lag, interp_e.id, es, st)) return rd;  // no snapshot -> interp
    // NOTE: raylib #defines DEG2RAD, so use the viewer's DEG2RAD_V (not netsnap::DEG2RAD) here.
    Coast7 seed{es.lat_deg * DEG2RAD_V, es.lon_deg * DEG2RAD_V,
                es.bearing_deg * DEG2RAD_V, es.phi_deg * DEG2RAD_V, es.alt_m,
                es.tas_mps, es.gamma_deg * DEG2RAD_V};
    int steps = static_cast<int>(std::lround(now_tick - static_cast<double>(st)));
    if (steps < 0) steps = 0;
    Coast7 c = coast_to_now(rails, seed, static_cast<unsigned>(steps));
    if (mode == RemoteMode::SMOOTH) c = smoother.update(interp_e.id, c, REMOTE_SMOOTH);
    rd.lat_rad = c.lat; rd.lon_rad = c.lon; rd.psi_rad = c.psi; rd.phi_rad = c.phi;
    rd.gamma_rad = c.gamma;
    rd.pos = geo_to_cartesian(c.lat, c.lon, c.alt, R);
    if (have_truth) rd.coast_err = length(rd.pos - truth_pos);
    return rd;
}

// One HUD line summarising the active remote mode + the worst interp/coast "now" error this frame.
void draw_remote_mode_hud(RemoteMode mode, double worst_interp, double worst_coast, int x, int y) {
    char line[160];
    if (mode == RemoteMode::INTERP)
        std::snprintf(line, sizeof(line), "REMOTE: %s   now-lag ~%.0fm   [M] predict",
                      remote_mode_name(mode), worst_interp < 0 ? 0.0 : worst_interp);
    else
        std::snprintf(line, sizeof(line),
                      "REMOTE: %s   coast now-err %.0fm  vs interp %.0fm   [M] cycle",
                      remote_mode_name(mode), worst_coast < 0 ? 0.0 : worst_coast,
                      worst_interp < 0 ? 0.0 : worst_interp);
    DrawText(line, x, y, 16, mode == RemoteMode::INTERP ? LIGHTGRAY : Color{140, 230, 160, 255});
}

// Headless data-path proof: advance render_tick across the recording, print sampled positions.
int run_selfcheck(const Playback& pb, int n) {
    double t0 = static_cast<double>(pb.first_tick());
    double t1 = static_cast<double>(pb.last_tick());
    std::printf("selfcheck: span ticks [%.0f, %.0f], delay=%.1f ticks, %d Hz snaps, %zu journal events\n",
                t0, t1, pb.delay_ticks(), pb.snap_hz(), pb.events().size());
    // Echo the airframe type trailer (v3) — headless proof of the mesh-variant data path.
    if (!pb.types().empty()) {
        std::printf("  types:");
        for (std::size_t i = 0; i < pb.types().size(); ++i)
            std::printf("  #%zu=%s", i,
                        aircraft_type_name(aircraft_type_from_code(pb.types()[i])));
        std::printf("\n");
    }
    // Echo the static airframe HUD data (region toughness in eighths recovered from the wire's
    // first-frame pools + supercharger crit alt from the type trailer's envelope) — headless
    // proof of the scoreboard's toughness/crit data path.
    for (const auto& b : pb.sample_weapons(t0).hp) {
        int e8[3];
        bool ok = toughness_eighths(b, e8);
        const Envelope* env = pb.types().empty() ? nullptr : envelope_for_code(pb.type_code_of(b.id));
        std::printf("  airframe #%lld:  tough E/W/T = ", static_cast<long long>(b.id));
        if (ok) std::printf("%d/%d/%d /8", e8[0], e8[1], e8[2]);
        else std::printf("(no region pools)");
        if (env) std::printf("   crit_alt %.0fm", env->crit_alt_m);
        if (env && env->crit_lo_alt_m > 0.0)  // two-speed blower: low-gear crit + FS-gear fraction
            std::printf("  (two-speed: crit_lo %.0fm, gear2 %.4f)", env->crit_lo_alt_m,
                        env->gear2_frac);
        std::printf("\n");
    }
    // Echo the combat journal (per-round hits + kills, at the full 100 Hz tick) so the data path is
    // provable headless — this is what the GUI feed draws as floating damage numbers + kill lines.
    for (const auto& e : pb.events())
        std::printf("  event t=%lld  #%lld -> #%lld  dmg %.0f  hp_after %.0f  region %lld%s\n",
                    static_cast<long long>(e.tick), static_cast<long long>(e.attacker),
                    static_cast<long long>(e.target), e.damage_milli / 1000.0,
                    e.hp_after_milli / 1000.0, static_cast<long long>(e.region),
                    e.killed ? "  *** KILL ***" : "");
    for (int i = 0; i < n; ++i) {
        double a = (n > 1) ? static_cast<double>(i) / (n - 1) : 0.0;
        double rt = t0 + a * (t1 - t0);
        std::vector<RenderEntity> ents = pb.sample(rt);
        WeaponView wv = pb.sample_weapons(rt);  // WEAPON-001 gunnery state (hp + live rounds)
        std::printf("  t=%.1f:", rt);
        for (const auto& e : ents) {
            const RenderHp* w = weap_for(wv.hp, e.id);
            // sigma at the aircraft's altitude + supercharger power state — the same values the
            // GUI HUD rows draw (pwr needs crit_alt_m, so it appears only with a type trailer).
            char atmos[24];
            const Envelope* env =
                pb.types().empty() ? nullptr : envelope_for_code(pb.type_code_of(e.id));
            if (env)
                std::snprintf(atmos, sizeof(atmos), " sig=%.2f pwr=%.0f%%", hud_sigma(e.alt_m),
                              hud_power(e.alt_m, env->crit_alt_m, env->crit_lo_alt_m, env->gear2_frac) * 100.0);
            else
                std::snprintf(atmos, sizeof(atmos), " sig=%.2f", hud_sigma(e.alt_m));
            std::printf(" [#%lld lat=%.3f lon=%.3f alt=%.0f brg=%.1f hp=%.0f ammo=%.0f "
                        "e/w/t=%.2f/%.2f/%.2f kills=%lld lhb=%lld%s%s]",
                        static_cast<long long>(e.id), e.lat_deg, e.lon_deg, e.alt_m, e.bearing_deg,
                        w ? w->hp : -1.0, w ? w->ammo : -1.0, w ? w->engine_hp : -1.0,
                        w ? w->wing_hp : -1.0, w ? w->tail_hp : -1.0,
                        static_cast<long long>(w ? w->kills : 0),
                        static_cast<long long>(w ? w->last_hit_by : -1), atmos,
                        (w && w->hp <= 0.0) ? " KILLED" : "");
        }
        std::printf("  rounds=%zu\n", wv.rounds.size());
    }
    // Remote coast vs interpolation "now" error (netcode layer 24, presentation-side). For each
    // sample tick treated as "now", seed the coast from the freshest snapshot at now-lag and dead-
    // reckon to now; compare its position error against the layer-4a interpolation baseline (drawn
    // ~1.5 snapshot intervals late). This is exactly what the GUI's PREDICT/SMOOTH modes draw, so
    // the data path is proven headless. On a turning remote coast-err << interp-err.
    {
        const Rails rails = viewer_rails();
        const double lag = pb.delay_ticks();
        const double R = pb.radius_m();
        std::printf("remote coast (layer 24) vs interp, lag=%.1f ticks:\n", lag);
        for (int i = 0; i < n; ++i) {
            double a = (n > 1) ? static_cast<double>(i) / (n - 1) : 0.0;
            double now_t = t0 + a * (t1 - t0);
            std::vector<RenderEntity> truth = pb.sample(now_t);
            std::vector<RenderEntity> interp = pb.sample(now_t - lag);
            double worst_i = 0.0, worst_c = 0.0;
            for (const auto& te : truth) {
                for (const auto& ie : interp)
                    if (ie.id == te.id) {
                        double e = length(ie.pos - te.pos);
                        if (e > worst_i) worst_i = e;
                        break;
                    }
                netsnap::EntityState es;
                int64_t st = 0;
                if (pb.nearest_state(now_t - lag, te.id, es, st)) {
                    // raylib #defines DEG2RAD -> use the viewer's DEG2RAD_V (not netsnap::DEG2RAD).
                    Coast7 seed{es.lat_deg * DEG2RAD_V, es.lon_deg * DEG2RAD_V,
                                es.bearing_deg * DEG2RAD_V, es.phi_deg * DEG2RAD_V,
                                es.alt_m, es.tas_mps, es.gamma_deg * DEG2RAD_V};
                    int steps = static_cast<int>(std::lround(now_t - static_cast<double>(st)));
                    if (steps < 0) steps = 0;
                    Coast7 c = coast_to_now(rails, seed, static_cast<unsigned>(steps));
                    double e = length(geo_to_cartesian(c.lat, c.lon, c.alt, R) - te.pos);
                    if (e > worst_c) worst_c = e;
                }
            }
            std::printf("  t=%.1f  interp-err %6.2fm   coast-err %6.2fm\n", now_t, worst_i, worst_c);
        }
    }
    return 0;
}

void draw_hud(const Playback& pb, double render_tick, const std::vector<RenderEntity>& ents,
              const WeaponView& wv, const std::vector<RenderHp>& maxhp) {
    DrawText("SEADS — ATM-Sphere globe viewer (read-only / layer-4a interp + WEAPON-001 wire)", 12,
             12, 20, RAYWHITE);
    char line[200];
    std::snprintf(line, sizeof(line),
                  "render_tick %.1f  (%.2fs)  delay %.0fms  %dHz snaps   rounds airborne %zu",
                  render_tick, render_tick / pb.tick_hz(), pb.delay_ticks() / pb.tick_hz() * 1000.0,
                  pb.snap_hz(), wv.rounds.size());
    DrawText(line, 12, 38, 16, LIGHTGRAY);
    int y = 70;
    for (const auto& e : ents) {
        double mh = hp_for(maxhp, e.id, 100.0);
        const RenderHp* w = weap_for(wv.hp, e.id);
        double hp = w ? w->hp : mh;
        bool dead = hp <= 0.0;
        char bar[12];
        int n = (mh > 0.0) ? static_cast<int>(std::lround(hp / mh * 10.0)) : 0;
        if (n < 0) n = 0; if (n > 10) n = 10;
        for (int k = 0; k < 10; ++k) bar[k] = (k < n) ? '#' : '.';
        bar[10] = '\0';
        // Region status letters: a knocked-out region's letter goes '-' (E W T = intact).
        char rgn[4] = "???";
        if (w) {
            rgn[0] = w->engine_hp > 0.0 ? 'E' : '-';
            rgn[1] = w->wing_hp > 0.0 ? 'W' : '-';
            rgn[2] = w->tail_hp > 0.0 ? 'T' : '-';
        }
        // Airframe name from the v3 type trailer (blank for an older recording).
        const char* tn = type_name_for(pb, e.id);
        char who[32];
        std::snprintf(who, sizeof(who), "#%lld%s%s", static_cast<long long>(e.id), tn ? " " : "",
                      tn ? tn : "");
        // ISA density ratio at the aircraft's altitude + supercharger power state (v1.21r0 sigma /
        // v1.22r0 lapse, drawn presentation-side): "pwr 100%" = RATED below crit, falling above.
        // The power readout needs crit_alt_m, so it appears only with the v3 type trailer.
        char atmos[32] = "";
        {
            double sig = hud_sigma(e.alt_m);
            const Envelope* env =
                pb.types().empty() ? nullptr : envelope_for_code(pb.type_code_of(e.id));
            if (env)
                std::snprintf(atmos, sizeof(atmos), "   sig %.2f  pwr %3.0f%%", sig,
                              hud_power(e.alt_m, env->crit_alt_m, env->crit_lo_alt_m, env->gear2_frac) * 100.0);
            else
                std::snprintf(atmos, sizeof(atmos), "   sig %.2f", sig);
        }
        if (dead)
            std::snprintf(line, sizeof(line),
                          "%s  *** KILLED by #%lld ***   hp [%s] 0/%.0f   kills %lld", who,
                          static_cast<long long>(w ? w->last_hit_by : -1), bar, mh,
                          static_cast<long long>(w ? w->kills : 0));
        else
            std::snprintf(line, sizeof(line),
                          "%s  alt %5.0fm  brg %5.1f  bank %+5.1f  tas %5.1f   hp [%s] %.0f/%.0f"
                          "   rgn %s   ammo %4.0f   kills %lld%s",
                          who, e.alt_m, e.bearing_deg, e.phi_deg,
                          e.tas_mps, bar, hp, mh, rgn, w ? w->ammo : 0.0,
                          static_cast<long long>(w ? w->kills : 0), atmos);
        DrawText(line, 12, y, 16, dead ? GRAY : SKYBLUE);
        y += 22;
    }
    DrawText("drag: orbit   wheel: zoom   space: pause   M: remote mode   R: restart", 12,
             GetScreenHeight() - 28, 16, GRAY);
}

int run_gui(Playback& pb, double speed) {
    const int W = 1280, H = 720;
    InitWindow(W, H, "SEADS globe viewer");
    SetTargetFPS(60);
    FighterModelSet models;
    models.init();

    Camera3D cam{};
    cam.position = Vector3{0, DISPLAY_R * 1.4f, DISPLAY_R * 2.6f};
    cam.target = Vector3{0, 0, 0};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    const double t0 = static_cast<double>(pb.first_tick());
    const double t1 = static_cast<double>(pb.last_tick());
    const double span = (t1 - t0);
    // Per-aircraft trails (display-space points), keyed by slot order.
    std::vector<std::vector<Vector3>> trails;
    // WEAPON-001 (seal v1.12r0): starting hp + region pools per aircraft (full bars baseline).
    const std::vector<RenderHp> maxhp = pb.sample_weapons(t0).hp;
    // Journal-driven combat feed (per-round @ 100 Hz) when the recording carries the event journal;
    // otherwise fall back to the wire-state transition feed (v1 recordings have no journal).
    CombatFeed cfeed; cfeed.load(&pb.events());
    KillFeed feed;  // fallback: attributed kill/knock-out feed derived from wire-frame transitions
    // Remote render mode (M cycles interp -> predict -> smooth). The coast drives a kernel copy from
    // the sealed rails; the smoother holds per-aircraft blended display tuples (layer 25).
    RemoteMode rmode = RemoteMode::INTERP;
    const Rails coast_rails = viewer_rails();
    const double lag_ticks = pb.delay_ticks();
    RemoteCoasterSet smoother;
    bool paused = false;
    double sim_clock = 0.0;     // seconds of playback elapsed
    double last_wall = GetTime();

    while (!WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last_wall;
        last_wall = now;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_M)) { rmode = cycle_remote_mode(rmode); smoother.reset(); }
        if (IsKeyPressed(KEY_R)) {
            sim_clock = 0.0; trails.clear(); feed.reset(); cfeed.reset(); smoother.reset();
        }
        if (!paused) sim_clock += dt * speed;

        UpdateCamera(&cam, CAMERA_THIRD_PERSON);

        // Render time = playback position MINUS the layer-4a delay, clamped + looped over the span.
        double played_ticks = sim_clock * pb.tick_hz();
        double loop = (span > 0) ? std::fmod(played_ticks, span) : 0.0;
        double render_tick = t0 + loop - pb.delay_ticks();
        if (render_tick < t0) render_tick = t0;

        std::vector<RenderEntity> ents = pb.sample(render_tick);
        WeaponView wv = pb.sample_weapons(render_tick);  // hp + live rounds (WEAPON-001 wire)
        if (!paused && !cfeed.active()) feed.update(wv, now);  // journal feed updates in the 2D pass
        // "Now" (undelayed) truth for the error readout + the per-remote display state under rmode.
        double now_tick = t0 + loop;
        std::vector<RenderEntity> truth = pb.sample(now_tick);
        if (loop < pb.delay_ticks() + 1.0) smoother.reset();  // re-seed the blend cleanly on loop
        std::vector<RemoteDraw> rd(ents.size());
        double worst_interp = -1.0, worst_coast = -1.0;
        for (size_t i = 0; i < ents.size(); ++i) {
            Vec3 tp{}; bool have = false;
            for (const auto& te : truth) if (te.id == ents[i].id) { tp = te.pos; have = true; break; }
            rd[i] = remote_draw(pb, ents[i], now_tick, lag_ticks, coast_rails, rmode, smoother,
                                pb.radius_m(), tp, have);
            if (rd[i].interp_err > worst_interp) worst_interp = rd[i].interp_err;
            if (rd[i].coast_err > worst_coast) worst_coast = rd[i].coast_err;
        }
        if (trails.size() < ents.size()) trails.resize(ents.size());
        for (size_t i = 0; i < ents.size(); ++i) {
            Vector3 d = to_display(rd[i].pos, pb.radius_m());
            if (loop < pb.delay_ticks() + 1.0) trails[i].clear();  // reset trails on loop
            // A dead aircraft is frozen on the wire, so its trail naturally stops growing.
            trails[i].push_back(d);
            if (trails[i].size() > 400) trails[i].erase(trails[i].begin());
        }

        BeginDrawing();
        ClearBackground(Color{8, 10, 18, 255});
        BeginMode3D(cam);
        DrawSphereWires(Vector3{0, 0, 0}, DISPLAY_R, 18, 24, Color{40, 70, 110, 255});
        DrawGrid(0, 0);  // no-op floor; keeps depth nice
        const Color palette[4] = {GOLD, LIME, ORANGE, VIOLET};
        for (size_t i = 0; i < ents.size(); ++i) {
            double hp = hp_for(wv.hp, ents[i].id, hp_for(maxhp, ents[i].id, 100.0));
            bool dead = hp <= 0.0;
            Color c = dead ? Color{90, 90, 96, 255} : palette[i % 4];  // kills grey out
            // Trail.
            for (size_t k = 1; k < trails[i].size(); ++k)
                DrawLine3D(trails[i][k - 1], trails[i][k], Fade(c, 0.6f));
            Vector3 d = to_display(rd[i].pos, pb.radius_m());
            // Attitude-aware fighter mesh (same models as fly): wings roll with bank (phi), nose
            // tilts with the flight-path angle (gamma). Position + attitude come from rd[i] — the
            // layer-4a interp (INTERP) or the layer-24/25 coast-to-now (PREDICT/SMOOTH). Region
            // parts tint from the wire pools; the silhouette is the .seadsrec v3 roster variant.
            draw_aircraft(models.for_code(pb.type_code_of(ents[i].id)), d,
                          rd[i].lat_rad, rd[i].lon_rad, rd[i].psi_rad, rd[i].phi_rad, rd[i].gamma_rad,
                          c, 1.2f, weap_for(wv.hp, ents[i].id),
                          weap_for(maxhp, ents[i].id), now * PROP_SPIN_RAD_S);
        }
        // WEAPON-001 tracer rounds: a yellow point cloud at each live round (from the decoded wire).
        for (const auto& r : wv.rounds)
            DrawSphere(to_display(r.pos, pb.radius_m()), 0.045f, TRACER_C);
        EndMode3D();
        // Per-aircraft damage state (hp bar + E/W/T region segments), projected above each marker.
        for (size_t i = 0; i < ents.size(); ++i) {
            Vector2 sp = GetWorldToScreen(to_display(rd[i].pos, pb.radius_m()), cam);
            if (sp.x < -50 || sp.x > GetScreenWidth() + 50 || sp.y < -50 ||
                sp.y > GetScreenHeight() + 50)
                continue;
            const RenderHp* w = weap_for(wv.hp, ents[i].id);
            const RenderHp* base = weap_for(maxhp, ents[i].id);
            RenderHp fallback; fallback.id = ents[i].id; fallback.hp = 100.0;
            draw_damage_bars(sp, w ? *w : fallback, base ? *base : fallback);
        }
        draw_scoreboard(pb, wv.hp, maxhp, GetScreenWidth());
        // Combat feed: journal-driven per-round events (numbers pinned to on-screen targets), or the
        // transition fallback for a v1 recording. Numbers pin to the DRAWN (rd) position under rmode.
        if (cfeed.active()) {
            auto screen_of = [&](int64_t id, Vector2& out) -> bool {
                for (size_t i = 0; i < ents.size(); ++i)
                    if (ents[i].id == id) {
                        out = GetWorldToScreen(to_display(rd[i].pos, pb.radius_m()), cam);
                        return out.x >= -50 && out.x <= GetScreenWidth() + 50 && out.y >= -50 &&
                               out.y <= GetScreenHeight() + 50;
                    }
                return false;
            };
            if (!paused) cfeed.update(render_tick, now, screen_of);
            cfeed.draw(12, GetScreenHeight() - 190, now);
        } else {
            feed.draw(12, GetScreenHeight() - 190, now);
        }
        draw_hud(pb, render_tick, ents, wv, maxhp);
        draw_remote_mode_hud(rmode, worst_interp, worst_coast, 12, 56);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

// ---- FLY mode (Track A): own ship predicted from local input, remotes interpolated -------------

// Sealed rail constants -> a Rails for the local prediction kernel (same numbers the golden uses).
Rails fly_rails() {
    Rails r;
    r.R = golden::R_M; r.dt = golden::DT_S; r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M; r.soft = golden::SOFT_M;
    return r;
}

// Own aircraft spawn (radians): mid-band altitude, heading east, cruising. KI-61 envelope.
const Envelope& kFlyEnv = envtab::KI61;
predict::OwnState fly_start() {
    // TAS 150 m/s: inside the Ki-61 envelope LUTs. B2: gamma (flight-path angle) starts at 0 (level).
    return predict::OwnState{0.0, 0.0, 90.0 * DEG2RAD_V, 0.0, 2500.0, 150.0, 0.0};
}

// Control axes pulled from the keyboard: A/D -> bank (rad), W/S -> pitch axis in [-1,1].
// bank: A=left(-), D=right(+); a +phi banks right and the kernel turns right (psi increases).
// pitch: W nose-down (push, p<0), S nose-up (pull, p>0). Yaw (Q/E) and throttle (Shift/Ctrl)
// are handled separately in run_fly (yaw has no kernel axis; throttle is a real B1 input).
struct FlyAxes { double bank_rad; double pitch; };
FlyAxes fly_keyboard_axes() {
    FlyAxes ax{0.0, 0.0};
    if (IsKeyDown(KEY_A)) ax.bank_rad -= MAX_BANK_RAD;   // bank left
    if (IsKeyDown(KEY_D)) ax.bank_rad += MAX_BANK_RAD;   // bank right
    if (IsKeyDown(KEY_W)) ax.pitch -= 1.0;               // push (nose down)
    if (IsKeyDown(KEY_S)) ax.pitch += 1.0;               // pull (nose up)
    return ax;
}

// Pitch axis p in [-1,1] -> commanded load factor g (B2). 1 g neutral holds level wings-level.
double g_from_pitch(double p) {
    double g = 1.0 + p * G_GAIN;
    if (g < G_MIN_CMD) g = G_MIN_CMD;
    if (g > G_MAX_CMD) g = G_MAX_CMD;
    return g;
}

// Wrap an angle into (-pi, pi] (used to keep the free-look azimuth bounded for restabilization).
double wrap_pi(double a) {
    while (a >  PI_C) a -= 2.0 * PI_C;
    while (a <= -PI_C) a += 2.0 * PI_C;
    return a;
}

// Local geographic tangent basis at (lat, lon) in radians, in the globe display frame (the same
// right-handed frame as geo_to_cartesian; uniform scaling means unit directions are scale-free).
// up = radial (outward), north = +lat tangent, east = +lon tangent.
void local_basis(double lat, double lon, Vec3& up, Vec3& north, Vec3& east) {
    double cl = std::cos(lat), sl = std::sin(lat), co = std::cos(lon), so = std::sin(lon);
    // Z reflected to match to_display() (un-mirror the left-handed ENU). fwd/right/up2 are then all
    // built in this reflected display space, so the rendered turn matches the bank input.
    up    = Vec3{cl * co, sl, -cl * so};
    north = Vec3{-sl * co, cl, sl * so};
    east  = Vec3{-so, 0.0, -co};
}

// Draw an aircraft that visibly BANKS (rolls about its heading axis) and PITCHES (nose up/down)
// so control input reads instantly, even though the motion itself is gradual on a real sphere.
// od = display position; lat/lon/psi/phi/pitch in radians; scale ~ model size (body units).
void draw_aircraft(FighterModel& fm, Vector3 od, double lat, double lon, double psi, double phi,
                   double pitch, Color c, float scale, const RenderHp* w, const RenderHp* base,
                   double spin_rad) {
    Vec3 up_h, north_h, east_h;
    local_basis(lat, lon, up_h, north_h, east_h);
    Vec3 fwd = normalize(north_h * std::cos(psi) + east_h * std::sin(psi));
    Vec3 right = normalize(cross(fwd, up_h));
    Vec3 up2 = cross(right, fwd);
    Vec3 wing  = right * std::cos(phi) - up2 * std::sin(phi);   // wingspan, rolled by the bank angle
    Vec3 b_up  = up2 * std::cos(phi) + right * std::sin(phi);   // canopy "up", rolled with it
    Vec3 nose  = normalize(fwd * std::cos(pitch) + b_up * std::sin(pitch));  // heading, pitched
    if (!fm.ready) {  // headless / pre-init fallback: the original line marker
        auto P = [&](Vec3 d, float len) {
            return Vector3{od.x + static_cast<float>(d.x) * len,
                           od.y + static_cast<float>(d.y) * len,
                           od.z + static_cast<float>(d.z) * len};
        };
        DrawSphere(od, 0.06f * scale, c);
        DrawLine3D(P(wing, -0.5f * scale), P(wing, 0.5f * scale), c);
        DrawLine3D(od, P(nose, 0.7f * scale), YELLOW);
        DrawLine3D(od, P(b_up, 0.4f * scale), Fade(c, 0.7f));
        return;
    }
    // Rigid body frame: X = pitched nose, Z = rolled wingspan (⟂ nose by construction),
    // Y = their cross — b_up itself is NOT ⟂ the pitched nose, so re-derive.
    Vec3 up_body = normalize(cross(wing, nose));

    // Region tinting from the decoded wire. A pre-protocol-7 recording decodes all-zero baseline
    // pools — regions then draw in the aircraft colour (same back-compat guard as the bars).
    bool dead = w && w->hp <= 0.0;
    bool regions_valid = w && base &&
                         (base->engine_hp > 0.0 || base->wing_hp > 0.0 || base->tail_hp > 0.0);
    Color pc[4] = {c, c, c, c};
    if (!dead && regions_valid) {
        if (w->engine_hp <= 0.0) pc[0] = REGION_OUT_C;
        if (w->wing_hp <= 0.0) pc[1] = REGION_OUT_C;
        if (w->tail_hp <= 0.0) pc[2] = REGION_OUT_C;
    }
    // Prop freezes on a dead engine (or a dead plane) — the v1.18r0 "engine out -> thrust 0"
    // state is visible on the airframe itself, not just in the bars.
    double sp = (dead || (regions_valid && w->engine_hp <= 0.0)) ? 0.0 : spin_rad;

    auto xf_of = [&](Vec3 X, Vec3 Y, Vec3 Z) {
        Matrix m{};
        m.m0 = static_cast<float>(X.x) * scale;
        m.m1 = static_cast<float>(X.y) * scale;
        m.m2 = static_cast<float>(X.z) * scale;
        m.m4 = static_cast<float>(Y.x) * scale;
        m.m5 = static_cast<float>(Y.y) * scale;
        m.m6 = static_cast<float>(Y.z) * scale;
        m.m8 = static_cast<float>(Z.x) * scale;
        m.m9 = static_cast<float>(Z.y) * scale;
        m.m10 = static_cast<float>(Z.z) * scale;
        m.m12 = od.x;
        m.m13 = od.y;
        m.m14 = od.z;
        m.m15 = 1.0f;
        return m;
    };
    Matrix xf = xf_of(nose, up_body, wing);
    // The engine part alone spins about the nose axis (the cowl is a solid of revolution, so only
    // the blades visibly turn).
    Vec3 wing_s = wing * std::cos(sp) + up_body * std::sin(sp);
    Vec3 up_s = up_body * std::cos(sp) - wing * std::sin(sp);
    Matrix xf_eng = xf_of(nose, up_s, wing_s);
    for (int i = 0; i < 4; ++i) {
        fm.mat.maps[MATERIAL_MAP_DIFFUSE].color = pc[i];
        DrawMesh(fm.mesh[i], fm.mat, i == 0 ? xf_eng : xf);
    }
}

void draw_fly_hud(const predict::Predictor& pred, uint32_t tick, size_t n_remote, size_t n_rounds,
                  bool paused, bool freelook, const Command& cmd) {
    DrawText("SEADS — FLY  (own = predicted / layer-4b   remotes = interpolated / layer-4a)", 12,
             12, 20, RAYWHITE);
    const Kernel& k = pred.kernel();
    double brg = k.psi(0) * RAD2DEG_V;
    if (brg < 0) brg += 360.0;
    char line[200];
    std::snprintf(line, sizeof(line),
                  "OWN  t=%u (%.2fs)  lat %+7.3f  lon %+8.3f  alt %5.0fm  hdg %5.1f  bank %+5.1f  "
                  "gamma %+5.1f  tas %5.1f%s",
                  tick, tick * 0.01, k.lat(0) * RAD2DEG_V, k.lon(0) * RAD2DEG_V, k.alt(0), brg,
                  k.phi(0) * RAD2DEG_V, k.gamma(0) * RAD2DEG_V, k.tas(0),
                  paused ? "   [PAUSED]" : "");
    DrawText(line, 12, 40, 16, Color{255, 120, 120, 255});
    std::snprintf(line, sizeof(line), "remotes (interp): %zu    rounds airborne: %zu    mode: %s",
                  n_remote, n_rounds, freelook ? "FREE-LOOK (keyboard fly)" : "MOUSE-AIM");
    DrawText(line, 12, 62, 16, freelook ? GOLD : SKYBLUE);
    std::snprintf(line, sizeof(line), "INPUT  bank %+5.1f deg   g %+4.2f   throttle %3.0f%%",
                  cmd.target_phi * RAD2DEG_V, cmd.target_g, cmd.throttle * 100.0);
    DrawText(line, 12, 84, 16, GREEN);
    // Atmosphere + engine state (v1.21r0 sigma / v1.22r0 supercharger lapse, presentation-side):
    // the air the own ship is flying in, and whether its engine still makes RATED power.
    double alt = k.alt(0);
    bool rated = alt <= kFlyEnv.crit_alt_m;
    std::snprintf(line, sizeof(line),
                  "ATMOS  sigma %.2f   engine pwr %3.0f%%   crit alt %.0fm  [%s]",
                  hud_sigma(alt), hud_power(alt, kFlyEnv.crit_alt_m, kFlyEnv.crit_lo_alt_m, kFlyEnv.gear2_frac) * 100.0, kFlyEnv.crit_alt_m,
                  rated ? "RATED" : "ABOVE CRIT");
    DrawText(line, 12, 106, 16, rated ? LIGHTGRAY : GOLD);
    DrawText("A/D bank  W/S pull/push g  Q/E yaw  Shift/Ctrl throttle   |   mouse: fine aim   hold "
             "SPACE: free-look   wheel: zoom   M: remote mode   P: pause   R: reset", 12,
             GetScreenHeight() - 28, 16, GRAY);
}

// Headless proof that the live-input -> Predictor path drives the real kernel (no window/GPU).
// Flies a fixed climbing-left-turn input for 600 ticks and prints the own state at n samples.
int run_fly_selfcheck(int n) {
    if (n < 1) n = 1;
    Rails rails = fly_rails();
    predict::Predictor pred(rails, &kFlyEnv, fly_start());
    const Command cmd{MAX_BANK_RAD * 0.6, 1.5, 0.8};  // banked + pulling 1.5 g, near-full throttle
    const uint32_t TICKS = 600;
    std::printf("fly selfcheck: %u ticks @ %d Hz, input target_phi=%.4frad g=%.2f  "
                "(env crit_alt %.0fm)\n", TICKS,
                static_cast<int>(1.0 / rails.dt), cmd.target_phi, cmd.target_g,
                kFlyEnv.crit_alt_m);
    const uint32_t every = (TICKS / static_cast<uint32_t>(n)) ? (TICKS / static_cast<uint32_t>(n)) : 1;
    for (uint32_t t = 1; t <= TICKS; ++t) {
        pred.predict(t, cmd);
        if (t % every == 0 || t == TICKS) {
            const Kernel& k = pred.kernel();
            double brg = k.psi(0) * RAD2DEG_V; if (brg < 0) brg += 360.0;
            // sig/pwr: the fly HUD's ATMOS readout (presentation-side sigma + supercharger lapse).
            std::printf("  t=%4u  lat=%+8.4f lon=%+8.4f alt=%7.1f hdg=%6.1f bank=%+6.2f tas=%6.1f "
                        "gamma=%+6.2f sig=%.2f pwr=%3.0f%%\n",
                        t, k.lat(0) * RAD2DEG_V, k.lon(0) * RAD2DEG_V, k.alt(0), brg,
                        k.phi(0) * RAD2DEG_V, k.tas(0), k.gamma(0) * RAD2DEG_V,
                        hud_sigma(k.alt(0)), hud_power(k.alt(0), kFlyEnv.crit_alt_m, kFlyEnv.crit_lo_alt_m, kFlyEnv.gear2_frac) * 100.0);
        }
    }
    return 0;
}

int run_fly(Playback& pb, double speed) {
    const int W = 1280, H = 720;
    InitWindow(W, H, "SEADS fly — predict own / interp remotes");
    SetTargetFPS(60);
    FighterModelSet models;
    models.init();

    Rails rails = fly_rails();
    const predict::OwnState start = fly_start();
    predict::Predictor pred(rails, &kFlyEnv, start);
    const double DT = rails.dt;              // exact kernel tick (0.01 s)
    double accumulator = 0.0;                // wall-time carried toward the next whole tick
    uint32_t own_tick = 0;

    // Remotes ride the recording's own (looping) timeline, sampled the layer-4a delay in the past.
    const double t0 = static_cast<double>(pb.first_tick());
    const double t1 = static_cast<double>(pb.last_tick());
    const double span = t1 - t0;
    double sim_clock = 0.0;
    bool paused = false;
    double last_wall = GetTime();

    // WEAPON-001 baseline (full hp + region pools at the first frame) + the combat feed (journal-
    // driven per-round when the recording has the event journal, transition fallback otherwise).
    const std::vector<RenderHp> maxhp = pb.sample_weapons(t0).hp;
    CombatFeed cfeed; cfeed.load(&pb.events());
    KillFeed feed;
    // Remote render mode for the interpolated bandits (M cycles interp -> predict -> smooth). The
    // OWN ship is always the layer-4b prediction; this only affects the remotes (layer 24/25).
    RemoteMode rmode = RemoteMode::INTERP;
    const Rails coast_rails = viewer_rails();
    const double lag_ticks = pb.delay_ticks();
    RemoteCoasterSet smoother;

    // Chase camera that rides behind/above the own ship. Free-look (hold SPACE) adds an azimuth/
    // elevation offset driven by the mouse; on release it lerps back to the stable behind view.
    double chase_dist = CHASE_DIST_DEF;     // zoom (display units behind the plane)
    double throttle = 0.7;                   // real B1 throttle [0,1], Shift/Ctrl adjust it
    double look_az = 0.0;                   // free-look yaw offset (0 = directly behind)
    double look_el = 0.0;                   // free-look pitch offset added to CHASE_ELEV
    bool freelook = false, prev_freelook = false;
    Camera3D cam{};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Mouse-aim instructor (SOLUTION outer loop grafted onto the kernel Command). The aim is a
    // world-frame unit vector carried by parallel transport; the instructor turns it into a
    // Command each tick. Seed it on the nose from the spawn frame.
    AimState aim;
    InstructorTuning instr_tune;
    InstructorState instr_state;
    {
        AircraftFrame f0 = make_frame(start.lat, start.lon, start.psi, start.phi, start.alt,
                                      start.tas, start.gamma, rails.R);
        aim.seed(f0.nose, f0.up);
        aim.carry(f0.up);   // prime last_up so the first tick's transport is a no-op
    }

    DisableCursor();  // WT-style locked cursor: raw mouse delta drives the aim (and free-look pan)

    std::vector<Vector3> own_trail;
    std::vector<std::vector<Vector3>> rem_trails;

    while (!WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last_wall;
        last_wall = now;
        if (dt > 0.25) dt = 0.25;  // clamp huge stalls (tab background) — no spiral of death
        if (IsKeyPressed(KEY_P)) paused = !paused;
        if (IsKeyPressed(KEY_M)) { rmode = cycle_remote_mode(rmode); smoother.reset(); }
        if (IsKeyPressed(KEY_R)) {
            pred = predict::Predictor(rails, &kFlyEnv, start);
            own_tick = 0; accumulator = 0.0; sim_clock = 0.0;
            own_trail.clear(); rem_trails.clear(); feed.reset(); cfeed.reset(); smoother.reset();
            AircraftFrame f0 = make_frame(start.lat, start.lon, start.psi, start.phi, start.alt,
                                          start.tas, start.gamma, rails.R);
            aim.seed(f0.nose, f0.up); aim.carry(f0.up); instr_state = InstructorState{};
        }

        // Mode: hold SPACE for free-look (mouse pans the camera; the aim is HELD and the instructor
        // keeps flying your last commanded turn). The cursor stays locked in both modes (WT-style):
        // raw mouse delta drives the aim in mouse mode, the camera orbit in free-look.
        freelook = IsKeyDown(KEY_SPACE);
        prev_freelook = freelook;

        // Zoom (both modes): wheel scrolls the chase distance.
        chase_dist *= (1.0 - GetMouseWheelMove() * 0.1);
        if (chase_dist < CHASE_DIST_MIN) chase_dist = CHASE_DIST_MIN;
        if (chase_dist > CHASE_DIST_MAX) chase_dist = CHASE_DIST_MAX;

        // Flight command via the mouse-aim INSTRUCTOR (SOLUTION outer loop grafted onto the kernel
        // Command). Reconstruct the world-frame aircraft frame from the kernel tuple, carry the aim
        // by parallel transport (every mode), route the mouse into aim (mouse mode) or camera
        // (free-look), then let the instructor turn the aim into a bank-to-turn Command.
        Command cmd{0.0, 1.0};              // neutral = wings level, 1 g
        {
            const Kernel& kk = pred.kernel();
            AircraftFrame frame = make_frame(kk.lat(0), kk.lon(0), kk.psi(0), kk.phi(0), kk.alt(0),
                                             kk.tas(0), kk.gamma(0), rails.R);
            aim.carry(frame.up);   // parallel transport EVERY tick, EVERY mode (SPEC §9.1)
            if (freelook) {
                // Mouse pans the camera around the plane; the aim is held (already carried).
                Vector2 md = GetMouseDelta();
                look_az = wrap_pi(look_az + md.x * 0.005);   // all the way around
                look_el += md.y * 0.005;
                if (look_el >  1.3) look_el =  1.3;          // up/down, short of straight over
                if (look_el < -1.3) look_el = -1.3;
            } else {
                // Mouse-aim: raw delta rotates the world-frame aim (nothing smoothed on this path).
                Vector2 md = GetMouseDelta();
                aim.mouse(md.x * AIM_SENS, -md.y * AIM_SENS);  // screen-down -> aim down
                // Restabilize the chase cam toward the behind view.
                double k = 1.0 - std::exp(-dt * 6.0);
                look_az -= look_az * k;
                look_el -= look_el * k;
            }
            InstructorOut io = instructor_step(aim.forward(), frame, kk.phi(0), kk.gamma(0),
                                               instr_tune, instr_state);
            cmd = io.cmd;
            // Keyboard OVERRIDE (slice-1: direct manual command while any of W/S/A/D is held; the
            // mouse still owns the aim/reticle). Refined to an in-envelope nudge in a later slice.
            FlyAxes kb = fly_keyboard_axes();
            if (kb.bank_rad != 0.0 || kb.pitch != 0.0)
                cmd = Command{kb.bank_rad, g_from_pitch(kb.pitch)};
        }

        // THROTTLE (Shift/Ctrl) is now a REAL kernel input (B1, seal v1.5r0): drive it into the
        // Command and let the energy model integrate TAS. (No more re-seed hack for speed.)
        if (IsKeyDown(KEY_LEFT_SHIFT)  || IsKeyDown(KEY_RIGHT_SHIFT))   throttle += 0.6 * dt;
        if (IsKeyDown(KEY_LEFT_CONTROL)|| IsKeyDown(KEY_RIGHT_CONTROL)) throttle -= 0.6 * dt;
        if (throttle < 0.0) throttle = 0.0;
        if (throttle > 1.0) throttle = 1.0;
        cmd.throttle = throttle;

        // YAW (Q/E) still has NO kernel axis (the kernel turns only by banking, auto-coordinated;
        // B2 added a real PITCH axis, not yaw), so it alone is applied DOWNSTREAM by re-seeding
        // heading through the public Kernel API — the path a netcode reconcile uses. No kernel
        // math/rail/golden touched.
        double yaw = 0.0;
        if (IsKeyDown(KEY_Q)) yaw -= YAW_RATE_RAD;        // yaw left
        if (IsKeyDown(KEY_E)) yaw += YAW_RATE_RAD;        // yaw right

        // Fly the own ship: fixed-timestep prediction from the current input.
        if (!paused) {
            accumulator += dt * speed;
            sim_clock += dt * speed;
            if (yaw != 0.0) {
                const Kernel& k = pred.kernel();
                pred = predict::Predictor(rails, &kFlyEnv,
                    predict::OwnState{k.lat(0), k.lon(0), k.psi(0) + yaw * dt * speed, k.phi(0),
                                      k.alt(0), k.tas(0), k.gamma(0)});
            }
            while (accumulator >= DT) {
                pred.predict(++own_tick, cmd);
                accumulator -= DT;
            }
        }

        // Chase camera: build a frame at the own ship (forward = heading) and place the eye behind
        // and above it, offset by the free-look az/el.
        Vec3 up_h, north_h, east_h;
        local_basis(pred.kernel().lat(0), pred.kernel().lon(0), up_h, north_h, east_h);
        double psi = pred.kernel().psi(0);
        Vec3 fwd = normalize(north_h * std::cos(psi) + east_h * std::sin(psi));
        Vec3 right = normalize(cross(fwd, up_h));
        Vec3 up2 = cross(right, fwd);
        double A = look_az, E = CHASE_ELEV + look_el;
        // Direction from the plane to the eye (behind = -fwd at A=0), swung by az/el.
        Vec3 back = fwd * (-std::cos(E) * std::cos(A)) + right * (std::cos(E) * std::sin(A)) +
                    up2 * std::sin(E);
        Vec3 owp_now = geo_to_cartesian(pred.kernel().lat(0), pred.kernel().lon(0),
                                        pred.kernel().alt(0), pb.radius_m());
        Vector3 od_cam = to_display(owp_now, pb.radius_m());
        Vec3 eye = Vec3{od_cam.x, od_cam.y, od_cam.z} + back * chase_dist;
        cam.position = Vector3{static_cast<float>(eye.x), static_cast<float>(eye.y),
                               static_cast<float>(eye.z)};
        cam.target = od_cam;
        cam.up = Vector3{static_cast<float>(up2.x), static_cast<float>(up2.y),
                         static_cast<float>(up2.z)};

        // Remotes: interpolate at (looped playback time - layer-4a delay).
        double played = sim_clock * pb.tick_hz();
        double loop = (span > 0) ? std::fmod(played, span) : 0.0;
        double render_tick = t0 + loop - pb.delay_ticks();
        if (render_tick < t0) render_tick = t0;
        std::vector<RenderEntity> rem = pb.sample(render_tick);
        // WEAPON-001 gunnery state at the same render time: hp / regions / ammo / kills + live
        // rounds, all from the decoded wire (the remotes' fight replicates; the own ship carries
        // no weapons in this single-process loop — there is no server to adjudicate its fire).
        WeaponView wv = pb.sample_weapons(render_tick);
        if (!paused && !cfeed.active()) feed.update(wv, now);  // journal feed updates in the 2D pass
        // Remote display state under rmode (interp / coast-to-now / smoothed) + the "now" errors.
        double now_tick = t0 + loop;
        std::vector<RenderEntity> truth = pb.sample(now_tick);
        if (loop < pb.delay_ticks() + 1.0) smoother.reset();
        std::vector<RemoteDraw> rd(rem.size());
        double worst_interp = -1.0, worst_coast = -1.0;
        for (size_t i = 0; i < rem.size(); ++i) {
            Vec3 tp{}; bool have = false;
            for (const auto& te : truth) if (te.id == rem[i].id) { tp = te.pos; have = true; break; }
            rd[i] = remote_draw(pb, rem[i], now_tick, lag_ticks, coast_rails, rmode, smoother,
                                pb.radius_m(), tp, have);
            if (rd[i].interp_err > worst_interp) worst_interp = rd[i].interp_err;
            if (rd[i].coast_err > worst_coast) worst_coast = rd[i].coast_err;
        }

        Vector3 od = od_cam;  // own-ship display position (already computed for the chase cam)
        if (!paused) {
            own_trail.push_back(od);
            if (own_trail.size() > 600) own_trail.erase(own_trail.begin());
        }
        if (rem_trails.size() < rem.size()) rem_trails.resize(rem.size());
        for (size_t i = 0; i < rem.size(); ++i) {
            Vector3 d = to_display(rd[i].pos, pb.radius_m());
            if (loop < pb.delay_ticks() + 1.0) rem_trails[i].clear();
            rem_trails[i].push_back(d);
            if (rem_trails[i].size() > 400) rem_trails[i].erase(rem_trails[i].begin());
        }

        BeginDrawing();
        ClearBackground(Color{8, 10, 18, 255});
        BeginMode3D(cam);
        DrawSphereWires(Vector3{0, 0, 0}, DISPLAY_R, 18, 24, Color{40, 70, 110, 255});
        const Color pal[4] = {GOLD, LIME, ORANGE, VIOLET};
        for (size_t i = 0; i < rem.size(); ++i) {
            double hp = hp_for(wv.hp, rem[i].id, hp_for(maxhp, rem[i].id, 100.0));
            Color c = (hp <= 0.0) ? Color{90, 90, 96, 255} : pal[i % 4];  // kills grey out
            for (size_t k = 1; k < rem_trails[i].size(); ++k)
                DrawLine3D(rem_trails[i][k - 1], rem_trails[i][k], Fade(c, 0.5f));
            Vector3 d = to_display(rd[i].pos, pb.radius_m());
            // Remotes tilt with their true flight-path angle (gamma). Position + attitude come from
            // rd[i]: layer-4a interp (INTERP) or the layer-24/25 coast-to-now (PREDICT/SMOOTH).
            draw_aircraft(models.for_code(pb.type_code_of(rem[i].id)), d,
                          rd[i].lat_rad, rd[i].lon_rad, rd[i].psi_rad, rd[i].phi_rad, rd[i].gamma_rad,
                          c, 1.0f, weap_for(wv.hp, rem[i].id),
                          weap_for(maxhp, rem[i].id), now * PROP_SPIN_RAD_S);
        }
        // WEAPON-001 tracer rounds — the remotes' gunfire, from the decoded wire.
        for (const auto& r : wv.rounds)
            DrawSphere(to_display(r.pos, pb.radius_m()), 0.045f, TRACER_C);
        // Own ship: hot, larger, with a longer trail. The marker rolls with bank, tilts with pitch.
        for (size_t k = 1; k < own_trail.size(); ++k)
            DrawLine3D(own_trail[k - 1], own_trail[k], Fade(RED, 0.8f));
        // B2 (seal v1.6r0): pitch is REAL — tilt the marker by the kernel's true flight-path angle
        // gamma. The old presentation-only exaggeration band-aid is gone.
        double own_pitch = pred.kernel().gamma(0);
        // Own ship carries no wire weapon state in this single-process loop (no server) — regions
        // draw in the ship colour, prop always spinning. The airframe matches the fly envelope
        // (kFlyEnv is the Ki-61), independent of the recording's type trailer.
        draw_aircraft(models.for_code(static_cast<uint32_t>(AircraftType::KI61)), od,
                      pred.kernel().lat(0), pred.kernel().lon(0), pred.kernel().psi(0),
                      pred.kernel().phi(0), own_pitch, RED, 1.6f, nullptr, nullptr,
                      now * PROP_SPIN_RAD_S);
        EndMode3D();
        // Mouse-aim reticle (2D overlay, only when not in free-look): the projection of the
        // world-frame aim direction (SPEC §9.2). A faint center crosshair marks the nose reference;
        // the gap between them is the instructor's live pointing error.
        if (!freelook) {
            float cx = GetScreenWidth() * 0.5f, cy = GetScreenHeight() * 0.5f;
            DrawLine(static_cast<int>(cx) - 8, static_cast<int>(cy), static_cast<int>(cx) + 8,
                     static_cast<int>(cy), Fade(GREEN, 0.30f));
            DrawLine(static_cast<int>(cx), static_cast<int>(cy) - 8, static_cast<int>(cx),
                     static_cast<int>(cy) + 8, Fade(GREEN, 0.30f));
            Vec3 aim_pt = Vec3{od.x, od.y, od.z} + aim.forward() * (DISPLAY_R * 0.6);
            Vector2 rp = GetWorldToScreen(Vector3{static_cast<float>(aim_pt.x),
                                                  static_cast<float>(aim_pt.y),
                                                  static_cast<float>(aim_pt.z)}, cam);
            DrawCircleLines(static_cast<int>(rp.x), static_cast<int>(rp.y), 10.0f, GREEN);
            DrawLine(static_cast<int>(rp.x) - 16, static_cast<int>(rp.y),
                     static_cast<int>(rp.x) + 16, static_cast<int>(rp.y), Fade(GREEN, 0.8f));
            DrawLine(static_cast<int>(rp.x), static_cast<int>(rp.y) - 16, static_cast<int>(rp.x),
                     static_cast<int>(rp.y) + 16, Fade(GREEN, 0.8f));
        }
        // Per-remote damage state (hp bar + E/W/T region segments), projected above each marker.
        for (size_t i = 0; i < rem.size(); ++i) {
            Vector2 sp = GetWorldToScreen(to_display(rd[i].pos, pb.radius_m()), cam);
            if (sp.x < -50 || sp.x > GetScreenWidth() + 50 || sp.y < -50 ||
                sp.y > GetScreenHeight() + 50)
                continue;
            const RenderHp* w = weap_for(wv.hp, rem[i].id);
            const RenderHp* base = weap_for(maxhp, rem[i].id);
            RenderHp fallback; fallback.id = rem[i].id; fallback.hp = 100.0;
            draw_damage_bars(sp, w ? *w : fallback, base ? *base : fallback);
        }
        draw_scoreboard(pb, wv.hp, maxhp, GetScreenWidth());
        if (cfeed.active()) {
            auto screen_of = [&](int64_t id, Vector2& out) -> bool {
                for (size_t i = 0; i < rem.size(); ++i)
                    if (rem[i].id == id) {
                        out = GetWorldToScreen(to_display(rd[i].pos, pb.radius_m()), cam);
                        return out.x >= -50 && out.x <= GetScreenWidth() + 50 && out.y >= -50 &&
                               out.y <= GetScreenHeight() + 50;
                    }
                return false;
            };
            if (!paused) cfeed.update(render_tick, now, screen_of);
            cfeed.draw(12, GetScreenHeight() - 190, now);
        } else {
            feed.draw(12, GetScreenHeight() - 190, now);
        }
        draw_fly_hud(pred, own_tick, rem.size(), wv.rounds.size(), paused, freelook, cmd);
        draw_remote_mode_hud(rmode, worst_interp, worst_coast, 12, 128);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string path;
    int selfcheck = 0;
    double speed = 1.0;
    bool fly = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selfcheck" && i + 1 < argc) selfcheck = std::atoi(argv[++i]);
        else if (a == "--speed" && i + 1 < argc) speed = std::atof(argv[++i]);
        else if (a == "--fly") fly = true;
        else if (!a.empty() && a[0] != '-') path = a;
    }
    // Fly + headless needs neither a recording nor a GPU — run it before requiring a file.
    if (fly && selfcheck > 0) return run_fly_selfcheck(selfcheck);

    // --fly with no recording: auto-discover a bundled one (for bandits to aim at); if none is
    // found, fly a BARE GLOBE (just your ship). So `seads_viewer --fly` alone always works.
    if (fly && path.empty()) {
        const char* cands[] = {"demo_dogfight.seadsrec", "dogfight.seadsrec", "flight.seadsrec",
                               "flight_demo.seadsrec", "gun.seadsrec",
                               "build-client/dogfight.seadsrec"};
        std::vector<uint8_t> b;
        for (const char* c : cands) if (read_file(c, b)) { path = c; break; }
    }

    if (path.empty()) {
        if (fly) {  // truly no recording anywhere — bare globe, just your ship
            std::printf("fly: no recording found — bare globe (your ship only)\n");
            Playback empty;
            return run_fly(empty, speed);
        }
        std::fprintf(stderr,
                     "usage: seads_viewer <flight.seadsrec> [--fly] [--selfcheck N] [--speed S]\n"
                     "       seads_viewer --fly                           (auto-loads a recording)\n"
                     "       seads_viewer --fly --selfcheck N             (headless, no recording)\n");
        return 2;
    }
    std::vector<uint8_t> blob;
    if (!read_file(path, blob)) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return 2; }
    Recording rec;
    if (!read_recording(blob.data(), blob.size(), rec)) {
        std::fprintf(stderr, "bad recording: %s\n", path.c_str());
        return 2;
    }
    Playback pb;
    if (!pb.load(rec)) { std::fprintf(stderr, "empty recording\n"); return 2; }
    std::printf("loaded %s: %u frames, radius %.0fm, %dHz physics / %dHz snaps\n", path.c_str(),
                rec.meta.n_frames, pb.radius_m(), pb.tick_hz(), pb.snap_hz());

    if (selfcheck > 0) return run_selfcheck(pb, selfcheck);
    if (fly) return run_fly(pb, speed);
    return run_gui(pb, speed);
}
