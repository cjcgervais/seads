#pragma once

// ★ R5 rows 4/5/6 — ROOST / EXHAUST / RIDER BREATH: three instances of ONE
// mechanism (a ring-buffered puff trail off a moving emitter), which is why
// they are one module. Mirrors render/wingtip_smoke.h exactly: pure trail
// bookkeeping (no raylib, no clock), app/main.cpp owns a SledPlumes, updates
// it per render frame from the sled draw state, and render/ draws the puffs
// (render/sled_plumes_draw.*). READ-ONLY off sim::SledState — nothing here
// feeds input/control/sim (SPEC §5); every quantity that keys an emitter is a
// REPORTED field the kernel already publishes:
//
//   ROOST   — sim::SledState::roost_flux. "★★ THE ONE NUMBER, TWO CONSUMERS
//             (§3.5a)": |slip| × available_snow, where available_snow already
//             reads the SAME GroundSample depth the track/thrust read (thin
//             snow over rock throws less) and dies as the tunnel buries. No
//             second depth source here — reading flux IS the anti-fork rule.
//             Additionally speed-keyed to EXACTLY zero at rest (the R4b
//             lesson: a still machine must show nothing, even at full-throttle
//             slip), and the plume length is speed×life by construction:
//             kRoostLife 0.55 s puts the trail at Chad's 6 m (20 ft) by
//             ~11 m/s and 12 m (40 ft) by ~22 m/s.
//   EXHAUST — THROTTLE-keyed (never speed): that is what makes it read as an
//             engine. A faint idle wisp stays (a clutched-out two-stroke at
//             idle does smoke); the rate answers the thumb.
//   BREATH  — PERIODIC at a breathing rate: 15 breaths/min at rest rising to
//             30 at full throttle (resting adult 12–16 bpm, heavy effort
//             ~30 bpm — physiology, not a tuned look), a small white cluster
//             at the rider's head.
//
// No clock, no per-frame randomness: turbulence/jitter is smoke_hash(seed, k)
// off a monotonic emit counter (the wingtip_smoke.h hash — shared, not
// forked), emission is fixed-interval accumulator (frame-rate independent),
// and each puff emitted mid-frame is BACKDATED along the emitter velocity by
// its sub-frame offset so a fast machine lays an even ribbon instead of
// per-frame clumps.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "render/wingtip_smoke.h"  // smoke_hash — the ONE puff hash (no fork)

namespace render {

// --- ROOST (row 4) ----------------------------------------------------------
inline constexpr double kRoostEmitDt = 1.0 / 150.0;  // [s] at intensity 1
inline constexpr double kRoostLife = 0.55;           // [s] short — snow falls back
inline constexpr std::size_t kRoostMax = 240;        // hard ring cap
// Speed key: 0 at rest, full by ~9 m/s (32 km/h). smoothstep’d below so slow
// creeping throws a dusting, not a wall.
inline constexpr double kRoostSpeedLo = 1.5;   // [m/s] first grains
inline constexpr double kRoostSpeedHi = 9.0;   // [m/s] full plume
// --- EXHAUST (row 5) --------------------------------------------------------
inline constexpr double kExhaustEmitDt = 1.0 / 40.0;  // [s] at intensity 1
inline constexpr double kExhaustLife = 0.9;           // [s]
inline constexpr std::size_t kExhaustMax = 80;        // hard ring cap
inline constexpr double kExhaustIdleFrac = 0.15;      // idle wisp fraction
// --- BURST (R4c §7.5, THE POOF) ---------------------------------------------
// ★★★ A FOURTH TRAIL, NOT A FOURTH PARTICLE SYSTEM. §0.3 says reuse the
// roost/spray path, and this does: the same puff, the same `smoke_hash`, the
// same ballistic integrator, the same ring cap, the same sorted batch and the
// same view-space quad shader (which is also why it inherits the fix for this
// project's DrawBillboard-renders-nothing trap for free). What is genuinely
// new is one thing the other three cannot do: every existing emitter is a
// CONTINUOUS RATE keyed to an accumulator, and a body hitting snow is an
// IMPULSE. That is the addition.
//
// ⚠ AND AN IMPULSE HAS NO ACCUMULATOR, WHICH IS WHAT MAKES IT FRAME-RATE
// INDEPENDENT FOR FREE -- the count is a pure function of the request, so the
// same poof emits the SAME number of puffs at 60 Hz and at 240 Hz, exactly,
// not to within a rounding remainder.
inline constexpr double kBurstLife = 2.0;       // [s] -- see the derivation below
inline constexpr std::size_t kBurstMax = 160;   // hard ring cap
inline constexpr int kBurstPuffsFull = 120;     // ★ count at strength 1: LOOK-DIAL
// ★★★ 13.4 m/s IS NOT A CHOSEN NUMBER -- IT IS WHERE HE BOTTOMS OUT.
// Work-energy on a body punching into snow: d = m v^2 / (2 sigma A), with the
// repo's own rider mass 87.5 kg, a frontal area 0.68 m^2 derived from the rig's
// MEASURED 1.85 m stature and 0.46 m suited breadth (docs/SUDBURIAN_LADDER.md
// §3.0-BIS) at ~80 % presented, and sigma = 15 kPa for settled dry seasonal
// snow at impact strain rates (CRREL snow mechanics; ⚠ IMPORTED FROM
// LITERATURE, NOT MEASURED IN THIS REPO -- there is no snow density or strength
// field anywhere in the tree, and every number below scales as 1/sigma).
//
// That gives d = 0.00429 v^2, so d reaches the SIGNED 0.77 m depth law at
// exactly v = 13.4 m/s -- an ordinary trail speed. Below it he half-buries;
// above it the hole cannot get deeper, so the burst saturates there too.
inline constexpr double kBurstVRefMps = 13.4;
// Coming back out he is SHEDDING snow, not throwing it: the energy is muscular
// (torso ~1.2 m/s, apex 0.11 m), so it is a curtain sliding off him and not a
// second explosion at his boots.
inline constexpr double kEmergeFrac = 0.25;

// --- BREATH (row 6) ---------------------------------------------------------
inline constexpr double kBreathLife = 1.1;         // [s]
inline constexpr std::size_t kBreathMax = 24;      // hard ring cap
inline constexpr double kBreathPeriodRest = 4.0;   // [s] 15 breaths/min seated
inline constexpr double kBreathPeriodWork = 2.0;   // [s] 30 breaths/min, WOT
inline constexpr int kBreathCluster = 3;           // puffs per exhale

struct SledPlumePuff {
    glm::dvec3 pos{0.0};
    glm::dvec3 vel{0.0};       // world [m/s] — integrated here, cosmetic only
    double age = 0.0;
    std::uint32_t seed = 0;    // deterministic jitter seed (draw-side too)
};

// ★ ONE POOF, ASKED FOR BY THE APP ON THE KERNEL'S OWN ONE-SHOT EDGE. Fixed
// storage, because at most two edges can be live in one frame and in practice
// never both.
struct PoofRequest {
    glm::dvec3 pos{0.0};             // WORLD, ON THE SNOW SURFACE -- not his
                                     // chest: he is drawn a lie-clearance up.
    glm::dvec3 up{0.0, 1.0, 0.0};    // local outward at his position, unit
    glm::dvec3 fwd{0.0, 0.0, -1.0};  // his travel at impact, unit
    double speed_mps = 0.0;          // impact speed (0 for the emergence edge)
    double depth_frac = 0.0;         // sim::walker_depth_frac, [0,1]
    bool emergence = false;          // the "he stands back up" edge
};

// Everything the update reads, all app-gathered (render/ owns nothing).
struct SledPlumeInputs {
    glm::dvec3 pos{0.0};        // sled CG, world
    glm::dmat3 basis{1.0};      // body->world (+X right, +Y up, -Z fwd)
    glm::dvec3 vel{0.0};        // sled world velocity [m/s]
    glm::dvec3 up{0.0, 1.0, 0.0};  // local up = normalize(pos), app-computed
    double ground_speed = 0.0;  // sim::SledState::ground_speed_ms
    double roost_flux = 0.0;    // sim::SledState::roost_flux (§3.5a)
    double throttle = 0.0;      // [0,1] the thumb the kernel was handed
    // Rider head (breath emitter). head_valid=false → the caller's documented
    // approximation is already folded into head_pos; the flag only exists so a
    // test can see which path ran.
    glm::dvec3 head_pos{0.0};
    glm::dvec3 head_fwd{0.0, 0.0, -1.0};  // face direction (follows the look)
    bool head_valid = false;
    // Live SEADS_* dials, app-read once (warn-and-default there). 0 kills the
    // emitter exactly (the A/B baseline arm); >1 densifies up to the ring cap.
    double roost_gain = 1.0;
    double exhaust_gain = 1.0;
    double breath_gain = 1.0;
    // ★ THE POOF, R4c. Appended by the app INSIDE the sim tick loop, because
    // `sim::WalkerState::poof` is a per-TICK one-shot: a frame can consume
    // several ticks, so a per-frame read of that flag is false by the time the
    // plumes update runs. Cleared by the app at the top of each frame.
    PoofRequest poof[2]{};
    int poof_n = 0;
    double poof_gain = 1.0;
};

struct SledPlumes {
    std::vector<SledPlumePuff> burst;
    std::vector<SledPlumePuff> roost;
    std::vector<SledPlumePuff> exhaust;
    std::vector<SledPlumePuff> breath;
    double roost_accum = 0.0;
    double exhaust_accum = 0.0;
    double breath_phase = 0.0;  // [0,1) breathing cycle
    std::uint32_t emit_count = 0;
};

// Row-4 keying, exposed pure for the unit test: EXACTLY 0.0 at rest whatever
// the flux (full-throttle trenching on the spot shows nothing — the speed key
// is Chad's ask and the R4b still-case lesson), scaling with ground speed and
// with the kernel's one roost product.
inline double sled_roost_intensity(double roost_flux, double ground_speed) {
    if (ground_speed <= kRoostSpeedLo) return 0.0;
    const double t = std::min(
        1.0, (ground_speed - kRoostSpeedLo) / (kRoostSpeedHi - kRoostSpeedLo));
    const double spd = t * t * (3.0 - 2.0 * t);  // smoothstep
    return std::clamp(roost_flux, 0.0, 1.0) * spd;
}

// ★ THE BURST'S SIZE, PURE so a test can walk it with no world. v^2 because
// the displaced volume goes as the penetration depth and the penetration depth
// goes as v^2; the clamp because he bottoms out at the depth law; and
// `depth_frac` because THERE IS NO SNOW TO THROW ON A PLOWED ROAD -- which
// makes the burst structurally zero on hardpack rather than merely small, the
// same no-threshold law `sim/walker.*` is built on.
inline double poof_strength(double speed_mps, double depth_frac) {
    const double v = std::max(0.0, speed_mps) / kBurstVRefMps;
    return std::clamp(v * v, 0.0, 1.0) * std::clamp(depth_frac, 0.0, 1.0);
}

namespace detail {

inline void age_and_expire(std::vector<SledPlumePuff>& v, double life,
                           double frame_dt) {
    for (SledPlumePuff& p : v) p.age += frame_dt;
    v.erase(std::remove_if(
                v.begin(), v.end(),
                [life](const SledPlumePuff& p) { return p.age >= life; }),
            v.end());
}

inline void push_capped(std::vector<SledPlumePuff>& v, std::size_t cap,
                        const SledPlumePuff& p) {
    v.push_back(p);
    if (v.size() > cap) v.erase(v.begin());
}

}  // namespace detail

// Advance all three trails one render frame. Pure and deterministic: an
// identical call sequence reproduces identical puffs BITWISE (no clock, no
// unseeded randomness), and the EMISSION COUNT over a span is independent of
// how that span is sliced into frames (accumulator/phase emission). Emission
// accumulators advance by frame_dt × intensity so intensity 0 emits nothing.
inline void sled_plumes_update(SledPlumes& t, const SledPlumeInputs& in,
                               double frame_dt) {
    if (frame_dt <= 0.0) return;
    const glm::dvec3 up = in.up;
    const glm::dvec3 back = in.basis * glm::dvec3(0.0, 0.0, 1.0);   // aft
    const glm::dvec3 right = in.basis * glm::dvec3(1.0, 0.0, 0.0);

    // ---- integrate live puffs (before aging so a dt covers each once) ------
    // Roost: ballistic snow — gravity along -up, light drag. Cosmetic
    // constants: g 9.81, drag tau 3 s (chunks, not mist).
    // ★ THE BURST RIDES THE SAME LOOP, not a copy of it: it is the same
    // thrown snow, and a second integrator is how two things that should fall
    // identically stop doing so.
    for (std::vector<SledPlumePuff>* v : {&t.roost, &t.burst}) {
        for (SledPlumePuff& p : *v) {
            p.vel += -up * 9.81 * frame_dt;
            p.vel *= std::exp(-frame_dt / 3.0);
            p.pos += p.vel * frame_dt;
        }
    }
    // Exhaust/breath: condensation mist — the carried sled velocity decays
    // fast (tau 0.3 s) toward a faint buoyant rise, so the cloud STOPS in the
    // world and the machine drives out from under it.
    const auto mist = [&](std::vector<SledPlumePuff>& v, double rise) {
        const double k = 1.0 - std::exp(-frame_dt / 0.3);
        const glm::dvec3 target = up * rise;
        for (SledPlumePuff& p : v) {
            p.vel += (target - p.vel) * k;
            p.pos += p.vel * frame_dt;
        }
    };
    mist(t.exhaust, 0.45);
    mist(t.breath, 0.25);

    detail::age_and_expire(t.burst, kBurstLife, frame_dt);
    detail::age_and_expire(t.roost, kRoostLife, frame_dt);

    // ---- R4c §7.5: THE POOF. An IMPULSE, so no accumulator and no frame_dt
    // anywhere in the count -- which is what makes it exactly frame-rate
    // independent instead of independent-to-a-remainder.
    for (int i = 0; i < in.poof_n && i < 2; ++i) {
        const PoofRequest& q = in.poof[i];
        const double str =
            q.emergence ? kEmergeFrac * std::clamp(q.depth_frac, 0.0, 1.0)
                        : poof_strength(q.speed_mps, q.depth_frac);
        const int n = static_cast<int>(
            static_cast<double>(kBurstPuffsFull) * str * in.poof_gain + 0.5);
        // A tangent frame at HIS position, not the machine's: he may be a long
        // way from it and it may be on its roof.
        glm::dvec3 f = q.fwd - q.up * glm::dot(q.fwd, q.up);
        f = glm::length(f) > 1e-9 ? glm::normalize(f)
                                  : glm::dvec3(0.0, 0.0, 0.0);
        const glm::dvec3 rt =
            glm::length(f) > 0.5 ? glm::cross(q.up, f) : glm::dvec3(0.0);
        for (int k = 0; k < n; ++k) {
            const std::uint32_t s = ++t.emit_count;
            const double h1 = smoke_hash(s, 1), h2 = smoke_hash(s, 2),
                         h3 = smoke_hash(s, 3), h4 = smoke_hash(s, 4);
            SledPlumePuff p;
            // Scattered over the crater mouth (r ~ 0.5 m), not from a point.
            p.pos = q.pos + f * (1.0 * (h1 - 0.5)) + rt * (1.0 * (h2 - 0.5));
            if (q.emergence) {
                // A CURTAIN OFF HIS BODY: anchored up a column on him, and
                // mostly sideways and slow. Not a fountain at his boots.
                p.pos += q.up * (1.2 * h3);
                p.vel = q.up * (0.4 * h4) + f * (0.8 * (h1 - 0.5)) +
                        rt * (0.8 * (h2 - 0.5));
            } else {
                // Granular-crater ejecta leaves at ~0.1-0.3 of impact speed.
                // ★ 0.35 IS A LOOK-DIAL (it lands 15 m/s at ~1.5x the roost's
                // signed throw); the CLAMP ends are physical -- below 3 m/s
                // there is no visible throw and above 9 you have left the
                // granular-ejecta band.
                const double v_ej =
                    std::clamp(0.35 * q.speed_mps, 3.0, 9.0);
                p.vel = q.up * (v_ej * (0.55 + 0.45 * h3)) +
                        f * (v_ej * 0.45 * (h1 - 0.5) +
                             0.15 * q.speed_mps) +
                        rt * (v_ej * 0.45 * (h2 - 0.5));
            }
            p.age = 0.0;
            p.seed = s;
            detail::push_capped(t.burst, kBurstMax, p);
        }
    }
    detail::age_and_expire(t.exhaust, kExhaustLife, frame_dt);
    detail::age_and_expire(t.breath, kBreathLife, frame_dt);

    // ---- ROW 4: ROOST off the belt exit ------------------------------------
    // Anchor: rear of the track patch. Body z = track_aft_m(0.52) + the drawn
    // patch half-length (0.61, render/draw.cpp) ≈ 1.13 m aft of CG; body y =
    // just above the running surface, cg_height_m(0.564) below the CG. These
    // are the kernel/draw geometry numbers, not invented offsets.
    const double roost_i =
        sled_roost_intensity(in.roost_flux, in.ground_speed) * in.roost_gain;
    if (roost_i > 0.0) {
        t.roost_accum += frame_dt * roost_i;
        while (t.roost_accum >= kRoostEmitDt) {
            t.roost_accum -= kRoostEmitDt;
            const double t_back = t.roost_accum / std::max(roost_i, 1e-9);
            const std::uint32_t s = ++t.emit_count;
            const double h1 = smoke_hash(s, 1), h2 = smoke_hash(s, 2),
                         h3 = smoke_hash(s, 3);
            const glm::dvec3 anchor =
                in.pos + in.basis * glm::dvec3(0.38 * (h3 - 0.5),  // belt width
                                               -0.45, 1.13);
            // Thrown up-and-back off the paddles: up 3.0–5.5 m/s (arc apex
            // 0.45–1.5 m), back a slice of ground speed + fan.
            const glm::dvec3 v0 = in.vel * 0.15 +
                                  up * (3.0 + 2.5 * h1) +
                                  back * (0.12 * in.ground_speed + 1.5 * h2) +
                                  right * (1.6 * (smoke_hash(s, 4) - 0.5));
            SledPlumePuff p;
            p.pos = anchor - in.vel * t_back;  // sub-frame backdate
            p.vel = v0;
            p.age = t_back;
            p.seed = s;
            detail::push_capped(t.roost, kRoostMax, p);
        }
    } else {
        t.roost_accum = 0.0;
    }

    // ---- ROW 5: EXHAUST -----------------------------------------------------
    // Anchor: DOCUMENTED APPROXIMATION — the indy650 GLB exports no exhaust /
    // muffler node (grepped; the only exhaust nodes in the tree are the Bf 109
    // stacks), so the outlet is placed low on the RIGHT flank at the hood's
    // trailing edge: body (+0.28 right, -0.25, -0.35 fwd of CG).
    const double ex_i =
        (kExhaustIdleFrac + (1.0 - kExhaustIdleFrac) * std::clamp(in.throttle, 0.0, 1.0)) *
        in.exhaust_gain;
    if (ex_i > 0.0) {
        t.exhaust_accum += frame_dt * ex_i;
        while (t.exhaust_accum >= kExhaustEmitDt) {
            t.exhaust_accum -= kExhaustEmitDt;
            const double t_back = t.exhaust_accum / std::max(ex_i, 1e-9);
            const std::uint32_t s = ++t.emit_count;
            const glm::dvec3 anchor =
                in.pos + in.basis * glm::dvec3(0.28, -0.25, -0.35);
            const glm::dvec3 v0 =
                in.vel +
                right * (0.8 + 0.4 * smoke_hash(s, 1)) +
                back * (0.5 * smoke_hash(s, 2)) - up * 0.2;
            SledPlumePuff p;
            p.pos = anchor - in.vel * t_back;
            p.vel = v0;
            p.age = t_back;
            p.seed = s;
            detail::push_capped(t.exhaust, kExhaustMax, p);
        }
    } else {
        t.exhaust_accum = 0.0;
    }

    // ---- ROW 6: RIDER BREATH ------------------------------------------------
    // Periodic exhale; rate rises with throttle as the cheap effort proxy
    // (15 → 30 breaths/min). One cluster of small puffs per wrap.
    if (in.breath_gain > 0.0) {
        const double period =
            kBreathPeriodRest +
            (kBreathPeriodWork - kBreathPeriodRest) * std::clamp(in.throttle, 0.0, 1.0);
        t.breath_phase += frame_dt / period;
        while (t.breath_phase >= 1.0) {
            t.breath_phase -= 1.0;
            const int n = std::clamp(
                static_cast<int>(kBreathCluster * in.breath_gain + 0.5), 1, 9);
            for (int i = 0; i < n; ++i) {
                const std::uint32_t s = ++t.emit_count;
                SledPlumePuff p;
                p.pos = in.head_pos +
                        glm::dvec3(smoke_hash(s, 1) - 0.5,
                                   smoke_hash(s, 2) - 0.5,
                                   smoke_hash(s, 3) - 0.5) *
                            0.06;
                p.vel = in.vel + in.head_fwd * (0.8 + 0.4 * smoke_hash(s, 4)) +
                        up * 0.15;
                p.age = 0.0;
                p.seed = s;
                detail::push_capped(t.breath, kBreathMax, p);
            }
        }
    } else {
        t.breath_phase = 0.0;
    }
}

// Clear on sled reseed / drive exit so a dead run's plumes don't hang.
inline void sled_plumes_reset(SledPlumes& t) {
    t.burst.clear();
    t.roost.clear();
    t.exhaust.clear();
    t.breath.clear();
    t.roost_accum = 0.0;
    t.exhaust_accum = 0.0;
    t.breath_phase = 0.0;
    t.emit_count = 0;
}

}  // namespace render
