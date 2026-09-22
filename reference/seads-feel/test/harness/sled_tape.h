// ★★ THE SLED TAPE (GI_DRIVE1_HANDOFF §3, SLED_TAPE_CONSULT_REPLY R1).
// Record Chad's actual drive — resolved SledInputs + per-tick cold params +
// a FULL SledState pin after every tick + every (dir, GroundSample) pair the
// kernel consumed, in consumption order — so his roll becomes a reproducible
// test case and no gate ever again certifies a scenario nobody drives.
//
// The consult's rulings, all binding here:
//  - Q1: the ground log is keyed — replay serves taped samples only after an
//    EXACT dir match, and the first mismatch is the first-divergence
//    attribution. Taped-ground replay is a SAME-KERNEL instrument only.
//  - Q2: full-struct pin, every tick. New SledState fields are pinned by
//    construction (the X-macro below), never by someone remembering.
//  - Q3: columnar text, doubles at max_digits10 (17) / floats at 9 — IEEE
//    round-trip makes parse-then-== bit-level. The writer REFUSES non-finite
//    values loudly. Params are DUMPED whole in the header and replay
//    reconstructs them FROM the tape, never from the build's defaults.
//  - Q4: any write to the state outside step_sled is an O (override) record
//    carrying a full post-mutation pin; replay applies it verbatim.
//  - conquest lessons: single append() hash funnel, header excluded from the
//    hash, footer-optional (a tape without `# sig` is a valid crash tape),
//    checked snprintf, and the STREAM must be opened std::ios::binary by the
//    caller (recorder.h:163 is the counterexample not to copy).
//
// FIREWALL: every entry point takes const refs and returns nothing the sim
// can read. The proof is the on/off bit-identity leg in
// test/unit/test_sled_tape.cpp, re-run every build — never this comment.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

#include "sim/sled.h"
#include "world/snowpack.h"

namespace seads_sledtape {

inline constexpr const char* kMagic = "seads-sled-tape v1";

// --- the three field rosters (write, read and compare all share these — the
// single-source rule that makes "pinned by construction" true) --------------

// Every double dial on SledParams, in file order (sim/sled.h:259-659).
#define SLEDTAPE_PARAMS_D(X)                                                  \
    X(mass_kg) X(cg_height_m) X(stance_m) X(ski_fwd_m) X(track_aft_m)         \
    X(ski_len_m) X(ski_width_m) X(track_len_m) X(track_width_m)               \
    X(track_rail_half_m) X(track_pitch_half_m) X(bite_at_contact_frac)        \
    X(track_clearance_m) X(track_lug_height_m) X(rider_mass_kg)               \
    X(lean_lat_seated_m) X(lean_lat_stand_m) X(lean_fwd_max_m)                \
    X(lean_aft_max_m) X(aft_ceiling_curve) X(k_air_shift)                     \
    X(stand_rise_m) X(tuck_drop_m) X(lean_tau_s)                              \
    X(lean_rate_ms) X(lean_return_tau_s) X(stand_cda_add_m2)                  \
    X(clutch_engage_ms) X(engine_brake_n) X(track_belt_circumference_m)       \
    X(steer_rate_per_s) X(susp_k) X(susp_c) X(susp_rest_m) X(susp_travel_m)   \
    X(susp_stop_k) X(bekker_kc) X(bekker_kphi) X(bekker_n)                    \
    X(pack_ref_depth_m) X(pack_soften) X(snow_hardness) X(air_temp_c)         \
    X(cold_t_ref_c) X(warm_drag_gain) X(creep_frac) X(tau_sink_s)             \
    X(refresh_len_m) X(shear_K_m) X(track_speed_max_ms) X(engine_power_w)     \
    X(max_thrust_n) X(traction_mu) X(brake_force_n)                        \
    X(roost_ref_depth_m) X(roost_gain)                                        \
    X(plane_gain) X(plane_lat_gain) X(plane_lat_lean_gain)                    \
    X(plane_lat_load_frac) X(plane_lift_split_frac)                           \
    X(plane_draft_m) X(plan_area_m2) X(ski_rake_rad)                          \
    X(alpha_max_rad) X(plow_cd) X(air_cda) X(air_rho) X(steer_max_rad)        \
    X(slip_ref_rad) X(track_lat_mu) X(track_lat_slip_shed)                 \
    X(plane_fit_load_weight)                                              \
    X(inertia.x) X(inertia.y) X(inertia.z)                                    \
    X(grip.unseat_gain) X(grip.pose_hz) X(grip.capacity)                      \
    X(grip_buck.buck_gain) X(grip_buck.decay_per_s) X(grip_buck.g0)

// Every double dial on SledComfort (sim/sled.h:109-257).
#define SLEDTAPE_COMFORT_D(X)                                                 \
    X(rolled_persist_s) X(rolled_grace_s) X(rolled_throttle_frac)          \
    X(roll_stiff_nm) X(roll_ref_rad)                                       \
    X(right_assist_nm) X(right_assist_max_ms)                                \
    X(right_assist_rearm_frac) X(right_assist_min_tilt_rad)                  \
    X(right_charge_push_s) X(right_charge_rest_s) X(right_dir_eps)         \
    X(right_seed_frac) X(right_tilt_lo_rad) X(right_tilt_hi_rad)           \
    X(right_speed_lp_s)                                                    \
    X(right_pump_omega_eps)                                                \
    X(right_stand_shift_frac)                                              \
    X(roll_release_lo_rad) X(roll_release_hi_rad) X(roll_damp_nms)            \
    X(roll_ref_blend) X(lean_bite_gain) X(lean_sat_gain_rad)                  \
    X(assist_hull_frac) X(roll_stiff_vgain) X(release_floor_frac)             \
    X(release_floor_hi_rad) X(assist_v_lo_ms) X(assist_v_hi_ms)               \
    X(side_k) X(side_c) X(side_mu) X(hull_shear_width_m) X(side_yaw_mu)       \
    X(side_right_gain_nm) X(side_right_vmin_ms) X(side_right_vref_ms)         \
    X(side_right_wref_rads)                                                \
    X(ice_bite_mu) X(leg_work_nm)

// The full SledState pin, doubles (sim/sled.h:661-727). surface + rolled are
// the two non-doubles, handled explicitly beside this list everywhere.
#define SLEDTAPE_PIN_D(X)                                                     \
    X(position.x) X(position.y) X(position.z) X(velocity.x) X(velocity.y)     \
    X(velocity.z) X(orientation.w) X(orientation.x) X(orientation.y)          \
    X(orientation.z) X(angular_vel.x) X(angular_vel.y) X(angular_vel.z)       \
    X(susp_x[0]) X(susp_x[1]) X(susp_x[2]) X(susp_v[0]) X(susp_v[1])          \
    X(susp_v[2]) X(creep_m[0]) X(creep_m[1]) X(creep_m[2]) X(sink_m[0])       \
    X(sink_m[1]) X(sink_m[2]) X(rider_lat_m) X(rider_fwd_m) X(rider_up_m)     \
    X(steer_actual) X(belt_speed_ms) X(engine_rpm) X(plane_frac)              \
    X(track_slip) X(roost_flux) X(thrust_n) X(ground_speed_ms)                \
    X(depth_under_m) X(rolled_hold_s) X(air_s) X(assist_nm) X(hull_engage_lp)

// --- fnv1a64 — local copy per the conquest precedent (the tape must not
// share code with anything that could couple it to the sim) -----------------
inline std::uint64_t fnv1a(const char* s, std::size_t n, std::uint64_t h) {
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<unsigned char>(s[i]);
        h *= 1099511628211ULL;
    }
    return h;
}
inline constexpr std::uint64_t kFnvBasis = 1469598103934665603ULL;

// --- records ----------------------------------------------------------------
struct TickRec {
    long tick = 0;
    sim::SledInputs in;
    // The three per-tick cold writes (app/main.cpp:2151-2153) — kernel inputs
    // in params clothing (consult Q4 item 4), taped as columns.
    double air_temp_c = 0.0, snow_hardness = 0.0, cold_t_ref_c = 0.0;
    sim::SledState pin;  // state AFTER this tick
};
struct OverrideRec {
    long tick = 0;  // applied verbatim BEFORE stepping this tick
    sim::SledState pin;
};
struct GroundRec {
    long tick = 0;  // informational; consumption ORDER is the contract
    glm::dvec3 dir{0.0};
    world::SnowpackField::GroundSample g;
};

// --- writer ------------------------------------------------------------------
class Writer {
  public:
    // Header (unhashed) + the initial state as the first body record (an O at
    // first_tick — replay starts from it; the pre-first-tick state is state).
    void begin(const std::string& tag, double dt, const sim::SledParams& p,
               const std::string& world_note, long first_tick,
               const sim::SledState& initial) {
        char line[256];
        hdr("# %s\n", kMagic);
        hdr("# tag=%s\n", tag.c_str());
        std::snprintf(line, sizeof line, "# dt %.17g substeps %d\n", dt,
                      p.substeps);
        buf_ += line;
        hdr("# world %s\n", world_note.c_str());
#define SLEDTAPE_W(f)                                                         \
    {                                                                         \
        std::snprintf(line, sizeof line, "# param %s %.17g\n", #f, p.f);      \
        buf_ += line;                                                         \
    }
        SLEDTAPE_PARAMS_D(SLEDTAPE_W)
#undef SLEDTAPE_W
#define SLEDTAPE_W(f)                                                         \
    {                                                                         \
        std::snprintf(line, sizeof line, "# cparam %s %.17g\n", #f,           \
                      p.comfort.f);                                           \
        buf_ += line;                                                         \
    }
        SLEDTAPE_COMFORT_D(SLEDTAPE_W)
#undef SLEDTAPE_W
        std::snprintf(line, sizeof line,
                      "# iparam substeps %d engine_brake_stacks %d "
                      "side_hull_points %d\n",
                      p.substeps, p.engine_brake_stacks ? 1 : 0,
                      p.comfort.side_hull_points);
        buf_ += line;
        for (int i = 0; i < static_cast<int>(world::Surface::kCount); ++i) {
            const sim::SurfaceDials& d = p.dials[i];
            std::snprintf(line, sizeof line,
                          "# dial %d %.17g %.17g %.17g %.17g %.17g %d %.17g "
                          "%.17g\n",
                          i, d.mu_kin, d.mu_lat, d.c_snow_pa, d.phi_deg,
                          d.rho_eff, d.sinkable ? 1 : 0, d.pack_k_scale,
                          d.mu_brake);
            buf_ += line;
        }
        buf_ +=
            "# columns T tick in6 cold3 pin41 surf rolled | O tick pin41 "
            "surf rolled | G tick dir3 drive_r depth_m surf\n";
        on_override(first_tick, initial);
    }

    void on_tick(long tick, const sim::SledInputs& in,
                 const sim::SledParams& p, const sim::SledState& pin) {
        std::string l;
        l.reserve(1400);
        l += 'T';
        num(l, static_cast<double>(tick), true);
        // floats at 9 digits, reconstructed AS float on read (consult F3)
        fnum(l, in.throttle);
        fnum(l, in.brake);
        fnum(l, in.steer);
        fnum(l, in.lean_lat);
        fnum(l, in.lean_fwd);
        fnum(l, in.stand);
        num(l, p.air_temp_c);
        num(l, p.snow_hardness);
        num(l, p.cold_t_ref_c);
        pin_fields(l, pin);
        commit(l);
    }
    void on_override(long tick, const sim::SledState& pin) {
        std::string l;
        l.reserve(1400);
        l += 'O';
        num(l, static_cast<double>(tick), true);
        pin_fields(l, pin);
        commit(l);
    }
    void on_ground(long tick, const glm::dvec3& dir,
                   const world::SnowpackField::GroundSample& g) {
        std::string l;
        l.reserve(160);
        l += 'G';
        num(l, static_cast<double>(tick), true);
        num(l, dir.x);
        num(l, dir.y);
        num(l, dir.z);
        num(l, g.drive_r);
        num(l, g.depth_m);
        char t[16];
        std::snprintf(t, sizeof t, " %d", static_cast<int>(g.surf));
        l += t;
        commit(l);
    }
    // Clean-exit footer. A tape without it is a valid crash tape.
    std::string footer() const {
        char line[64];
        std::snprintf(line, sizeof line, "# sig fnv1a=%llu\n",
                      static_cast<unsigned long long>(hash_));
        return std::string(line);
    }
    bool drain(std::string& out) {
        if (buf_.empty()) return false;
        out = std::move(buf_);
        buf_.clear();
        return true;
    }
    // Non-finite refusals (consult Q3a): a record containing one was DROPPED
    // loudly; the count is the instrument's own honesty readout.
    int nonfinite_refusals() const { return refusals_; }

  private:
    void hdr(const char* fmt, const char* a) {
        char line[512];
        std::snprintf(line, sizeof line, fmt, a);
        buf_ += line;
    }
    // Append one double; flags non-finite instead of writing it.
    void num(std::string& l, double v, bool as_long = false) {
        if (!std::isfinite(v)) {
            bad_ = true;
            return;
        }
        char t[40];
        if (as_long)
            std::snprintf(t, sizeof t, " %ld", static_cast<long>(v));
        else
            std::snprintf(t, sizeof t, " %.17g", v);
        l += t;
    }
    void fnum(std::string& l, float v) {
        if (!std::isfinite(v)) {
            bad_ = true;
            return;
        }
        char t[24];
        std::snprintf(t, sizeof t, " %.9g", static_cast<double>(v));
        l += t;
    }
    void pin_fields(std::string& l, const sim::SledState& s) {
#define SLEDTAPE_W(f) num(l, s.f);
        SLEDTAPE_PIN_D(SLEDTAPE_W)
#undef SLEDTAPE_W
        char t[24];
        std::snprintf(t, sizeof t, " %d %d", static_cast<int>(s.surface),
                      s.rolled ? 1 : 0);
        l += t;
    }
    // The ONE hash site (conquest pattern): whatever reaches the file went
    // through here, so the footer is correct regardless of drain timing.
    void commit(std::string& l) {
        if (bad_) {
            bad_ = false;
            ++refusals_;
            std::fprintf(stderr,
                         "[sled_tape] REFUSED non-finite record (total %d)\n",
                         refusals_);
            return;
        }
        l += '\n';
        hash_ = fnv1a(l.data(), l.size(), hash_);
        buf_ += l;
    }
    std::string buf_;
    std::uint64_t hash_ = kFnvBasis;
    bool bad_ = false;
    int refusals_ = 0;
};

// --- reader ------------------------------------------------------------------
struct Tape {
    std::string tag, world_note;
    double dt = 0.0;
    sim::SledParams params;  // reconstructed FROM the tape (consult F4)
    std::vector<TickRec> recs;
    std::vector<OverrideRec> overrides;  // file order; applied before their tick
    std::vector<GroundRec> ground;       // consumption order
    bool sig_present = false, sig_ok = false;
    // ★★★ THE DIAL GAP. Every comfort dial the CURRENT kernel declares that
    // this tape does not name -- i.e. every dial that DID NOT EXIST when the
    // drive was recorded (the writer emits the whole SLEDTAPE_COMFORT_D list,
    // so absent means absent-at-record-time, never merely unwritten).
    //
    // WHY THIS IS RECORDED RATHER THAN IGNORED (found 2026-08-26 while
    // attributing the tape 32/33 divergence): `load` starts from a
    // default-constructed SledParams and overwrites only what the tape names,
    // so an absent dial SILENTLY takes TODAY'S STRUCT DEFAULT -- not the value
    // in force when Chad drove. Nothing warned. That is safe today only by the
    // coincidence that right_assist_nm defaults to 0.0 (= the mechanic off,
    // which is what the pre-self-right tapes were recorded with). The first
    // dial that ships a NON-INERT struct default silently rewrites the history
    // of every older tape -- and they would still report "bit-exact", because
    // the pins were re-derived under the same wrong assumption.
    //
    // THE LAW, paid for repeatedly: a constant that describes the shipped
    // table stops describing it the moment the table moves. So the gap is
    // measured and surfaced; a verdict read off a tape with a non-empty gap is
    // a verdict against a kernel THE TAPE HAS NEVER SEEN, and must say so.
    std::vector<std::string> dial_gap;
};

// Every SledParams dial the CURRENT build declares, in macro order. Same
// purpose as all_comfort_dials below and the same law behind it.
inline std::vector<std::string> all_params_dials() {
    std::vector<std::string> all;
#define SLEDTAPE_R(f) all.push_back(#f);
    SLEDTAPE_PARAMS_D(SLEDTAPE_R)
#undef SLEDTAPE_R
    return all;
}

// Every comfort dial the CURRENT build declares, in macro order.
inline std::vector<std::string> all_comfort_dials() {
    std::vector<std::string> all;
#define SLEDTAPE_R(f) all.push_back(#f);
    SLEDTAPE_COMFORT_D(SLEDTAPE_R)
#undef SLEDTAPE_R
    return all;
}

inline bool read_pin(std::istringstream& is, sim::SledState& s) {
    double v = 0.0;
#define SLEDTAPE_R(f)                                                         \
    if (!(is >> v)) return false;                                             \
    s.f = v;
    SLEDTAPE_PIN_D(SLEDTAPE_R)
#undef SLEDTAPE_R
    int surf = 0, rolled = 0;
    if (!(is >> surf >> rolled)) return false;
    s.surface = static_cast<world::Surface>(surf);
    s.rolled = rolled != 0;
    return true;
}

inline bool load(std::istream& in, Tape& t, std::string* err) {
    std::vector<std::string> seen_cparams, seen_params;
    // ★ THE TAPE-ABSENT RULE (GI3): params reconstruct FROM the tape — so a
    // dial the tape does not mention must reconstruct the kernel that DROVE
    // the tape, which for any post-recording dial is its OFF value, never
    // the build default of the day. Every new comfort/param dial added after
    // a golden exists gets its OFF value pre-set here; a tape that records
    // the dial overwrites it below. Without this, shipping a new ON-default
    // turns every old tape's replay into a drive that never happened.
    t.params.plane_lat_gain = 0.0;
    t.params.plane_lat_load_frac = 0.0;
    t.params.plane_lift_split_frac = 0.0;
    t.params.plane_lat_lean_gain = 0.0;
    // ★ K-WS1: both new dials are OFF-by-absence. A tape cut before this
    // rung must replay the kernel it was cut on -- flat lean_aft_max_m (the
    // tape carries its own 0.25) and no airborne exchange.
    // ★ GI4 §9.7 item 2: the contact ceiling is OFF-by-absence. A tape cut
    // before this rung was driven by a kernel with no traction limit at all.
    // ⚠⚠ THIS LINE STOPPED BEING FREE ON 2026-09-18. Until SLED KERNEL v2 the
    // struct default was ALSO 0.0, so this preset and the default agreed and
    // nothing could tell them apart. `SledParams{}.traction_mu` is 3.0 now.
    // Deleting this line would silently re-drive every pre-v2 tape at 3.0 --
    // and the replay would still print "bit-exact", because the pins were
    // derived under the same wrong assumption. `sled_tape_absent_dials_replay_
    // at_the_identity_not_the_v2_default` is the leg that reds on the delete.
    // ★ LANDING RED-TEAM P1-2, 2026-09-19: WHEN THAT SENTENCE WAS FIRST
    // WRITTEN THE LEG DID NOT EXIST -- `grep -rn "sled_tape_absent_dials_replay"
    // test/` returned this comment and nothing else. A comment that ADVERTISES
    // a guard is worse than no comment: it invites the tidy-up it claims to
    // catch. The leg exists now (test/unit/test_sled_tape.cpp) and covers all
    // five presets below. MEASURED, one preset deleted at a time with
    // `ctest -R "tape|golden|replay"`: traction_mu is ALSO defended by
    // tape_360_chad_repro and tape_chad_flip_fence, rolled_throttle_frac by
    // tape_360_chad_repro, and track_lat_slip_shed by NOTHING ELSE AT ALL --
    // 40/40 passed with its line deleted. That one is why this leg had to be
    // written rather than the comment merely corrected.
    t.params.traction_mu = 0.0;
    t.params.aft_ceiling_curve = 0.0;
    t.params.k_air_shift = 0.0;
    t.params.comfort.assist_hull_frac = 0.0;
    t.params.comfort.roll_stiff_vgain = 0.0;
    t.params.comfort.release_floor_frac = 0.0;
    // ★ R4a SEATED SELF-RIGHT: OFF-by-absence, and this one MATTERS more
    // than its neighbours. `stand` is a TAPED INPUT, so a tape cut before
    // this rung contains real moments where he was tipped, slow and
    // standing -- with the assist on, those replay as a drive that never
    // happened. 0.0 makes the kernel block a no-op and the corpus exact.
    t.params.comfort.right_assist_nm = 0.0;
    // ★ R4a §7.7 THE GRIP: OFF-by-absence like every dial before it.
    //
    // ⚠⚠ AN EARLIER VERSION OF THIS COMMENT SAID IT "STOPS MATTERING THE MOMENT
    // STAGE 2 WIRES THE RELEASE IN". THAT IS BACKWARDS AND A RED-TEAM CAUGHT IT:
    // that is the moment it STARTS mattering. And as first written it was worse
    // than useless -- the dials were forced OFF here while being absent from
    // `SLEDTAPE_PARAMS_D`, so NO tape could record them and NO tape could turn
    // them back on. Every prior OFF-by-absence dial is in a roster; that is the
    // half that makes the law at the top of this block work ("a tape that
    // records the dial overwrites it below"). The grip dials are now in the
    // roster, and the dial gap covers the params side as well.
    t.params.grip.unseat_gain = 0.0;
    t.params.grip_buck.buck_gain = 0.0;
    // ★★★ SLED KERNEL v2 (Chad 2026-09-18), AND THE FIRST TIME THIS BLOCK HAS
    // HAD TO DO ITS JOB FOR REAL. Five dials took his driven values as SHIPPED
    // DEFAULTS. Every one of them is absent from the goldens in
    // test/golden/sled (34-dial gap) and two of them are absent from his six
    // 09-17 tapes, so without these five lines `load` would start from a
    // default-constructed SledParams carrying 3.0 / 1.4 / 0.15 and replay
    // drives that never happened -- the exact failure the paragraph at the top
    // of this block was written about, arriving from the exact direction it
    // predicted ("the first dial that ships a NON-INERT struct default").
    //
    // THE RULE, RESTATED: an ABSENT dial reconstructs at the IDENTITY -- the
    // value the kernel that CUT the tape had -- never at today's struct
    // default. The identity is written here as a LITERAL CONSTANT on purpose;
    // reading it back out of `sim::SledParams{}` would re-import the bug.
    t.params.track_lat_slip_shed = 0.0;         // v2 default 1.4
    t.params.comfort.rolled_throttle_frac = 0.0;  // v2 default 0.15
    // These two ship from `config/scenario.toml [sled_comfort]`, so the struct
    // default still IS the identity and these lines change nothing today. They
    // are here so the law does not rest on that coincidence a second time:
    // the moment anybody moves the struct defaults to match the TOML, the
    // corpus is already protected.
    t.params.comfort.right_assist_max_ms = 5.0 / 3.6;  // shipped 4.0 (toml)
    t.params.comfort.right_stand_shift_frac = 1.0;     // shipped 0.5 (toml)
    // ★ N2 LAKE-ICE BITE (2026-09-19): ships 0.25 from the toml; the identity
    // is 0.0 and every tape cut before this dial existed replays there.
    t.params.comfort.ice_bite_mu = 0.0;                // shipped 0.25 (toml)
    // ★ N1 THE LEG WORK (2026-09-19): ships 2400 from the toml; the identity
    // is 0.0. `stand` IS taped, so a tape with his SHIFT-while-rolled moments
    // replays at 0.0 the machine that cut it, never the leg ladder.
    t.params.comfort.leg_work_nm = 0.0;                // shipped 2400 (toml)
    std::string line;
    std::uint64_t hash = kFnvBasis;
    auto fail = [&](const char* m, const std::string& l) {
        if (err) *err = std::string(m) + ": " + l;
        return false;
    };
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') {
            std::istringstream is(line.substr(1));
            std::string key;
            is >> key;
            if (key == "sig") {
                std::string rest;
                is >> rest;
                if (rest.rfind("fnv1a=", 0) == 0) {
                    t.sig_present = true;
                    t.sig_ok =
                        std::strtoull(rest.c_str() + 6, nullptr, 10) == hash;
                }
            } else if (key == "tag=" || line.rfind("# tag=", 0) == 0) {
                t.tag = line.substr(6);
            } else if (key == "dt") {
                int ss = 0;
                std::string sub;
                if (!(is >> t.dt >> sub >> ss)) return fail("bad dt", line);
                t.params.substeps = ss;
            } else if (key == "world") {
                t.world_note = line.size() > 8 ? line.substr(8) : "";
            } else if (key == "param") {
                std::string name;
                double v = 0.0;
                if (!(is >> name >> v)) return fail("bad param", line);
                seen_params.push_back(name);
                bool hit = false;
#define SLEDTAPE_R(f)                                                         \
    if (!hit && name == #f) {                                                 \
        t.params.f = v;                                                       \
        hit = true;                                                           \
    }
                SLEDTAPE_PARAMS_D(SLEDTAPE_R)
#undef SLEDTAPE_R
                if (!hit) return fail("unknown param", line);
            } else if (key == "cparam") {
                std::string name;
                double v = 0.0;
                if (!(is >> name >> v)) return fail("bad cparam", line);
                seen_cparams.push_back(name);
                bool hit = false;
#define SLEDTAPE_R(f)                                                         \
    if (!hit && name == #f) {                                                 \
        t.params.comfort.f = v;                                               \
        hit = true;                                                           \
    }
                SLEDTAPE_COMFORT_D(SLEDTAPE_R)
#undef SLEDTAPE_R
                if (!hit) return fail("unknown cparam", line);
            } else if (key == "iparam") {
                std::string n1, n2, n3;
                int v1 = 0, v2 = 0, v3 = 0;
                if (!(is >> n1 >> v1 >> n2 >> v2 >> n3 >> v3))
                    return fail("bad iparam", line);
                t.params.substeps = v1;
                t.params.engine_brake_stacks = v2 != 0;
                t.params.comfort.side_hull_points = v3;
            } else if (key == "dial") {
                int i = 0, sinkable = 0;
                sim::SurfaceDials d;
                if (!(is >> i >> d.mu_kin >> d.mu_lat >> d.c_snow_pa >>
                      d.phi_deg >> d.rho_eff >> sinkable >> d.pack_k_scale >>
                      d.mu_brake))
                    return fail("bad dial", line);
                d.sinkable = sinkable != 0;
                if (i < 0 || i >= static_cast<int>(world::Surface::kCount))
                    return fail("dial index", line);
                t.params.dials[i] = d;
            }
            continue;  // header/footer lines are never hashed
        }
        hash = fnv1a(line.data(), line.size(), hash);
        hash = fnv1a("\n", 1, hash);
        std::istringstream is(line.substr(1));
        if (line[0] == 'T') {
            TickRec r;
            double fv = 0.0;
            if (!(is >> r.tick)) return fail("bad T tick", line);
            float* fp[6] = {&r.in.throttle, &r.in.brake,   &r.in.steer,
                            &r.in.lean_lat, &r.in.lean_fwd, &r.in.stand};
            for (float* f : fp) {
                if (!(is >> fv)) return fail("bad T input", line);
                *f = static_cast<float>(fv);  // 9-digit text -> exact float
            }
            if (!(is >> r.air_temp_c >> r.snow_hardness >> r.cold_t_ref_c))
                return fail("bad T cold", line);
            if (!read_pin(is, r.pin)) return fail("bad T pin", line);
            t.recs.push_back(r);
        } else if (line[0] == 'O') {
            OverrideRec r;
            if (!(is >> r.tick)) return fail("bad O tick", line);
            if (!read_pin(is, r.pin)) return fail("bad O pin", line);
            t.overrides.push_back(r);
        } else if (line[0] == 'G') {
            GroundRec r;
            int surf = 0;
            if (!(is >> r.tick >> r.dir.x >> r.dir.y >> r.dir.z >>
                  r.g.drive_r >> r.g.depth_m >> surf))
                return fail("bad G", line);
            r.g.surf = static_cast<world::Surface>(surf);
            t.ground.push_back(r);
        } else {
            return fail("unknown record", line);
        }
    }
    if (t.recs.empty()) return fail("empty tape", "");
    if (t.dt <= 0.0) return fail("no dt header", "");
    // THE DIAL GAP (see Tape::dial_gap): what this build knows that the tape
    // never did. Computed here so no caller can forget to ask.
    t.dial_gap.clear();
    for (const std::string& d : all_comfort_dials()) {
        bool found = false;
        for (const std::string& s2 : seen_cparams)
            if (s2 == d) {
                found = true;
                break;
            }
        if (!found) t.dial_gap.push_back(d);
    }
    // ★★★ AND THE PARAMS SIDE, ADDED 2026-08-30 BY RED-TEAM FINDING. The gap
    // was computed over the COMFORT roster only, so a dial living on
    // `SledParams` was invisible to it -- and the R4a grip dials are the first
    // ones that live there AND ship a non-inert default. Half a tripwire is
    // worse than none, because it reads as covered. Same rule, same evidence:
    // the writer emits the whole roster, so absent means absent-at-record-time.
    for (const std::string& d : all_params_dials()) {
        bool found = false;
        for (const std::string& s2 : seen_params)
            if (s2 == d) {
                found = true;
                break;
            }
        if (!found) t.dial_gap.push_back(d);
    }
    return true;
}

// --- tier-1 replay: re-simulate over TAPED ground, key-checked --------------
struct ReplayResult {
    long ticks_replayed = 0;
    long first_div_tick = -1;       // -1 == bit-exact throughout
    std::string first_div_field;
    long ground_mismatch_index = -1;  // first key-check failure, -1 == none
    long rolled_tick_tape = -1, rolled_tick_replay = -1;  // the event layer
    bool ok() const {
        return first_div_tick < 0 && ground_mismatch_index < 0;
    }
};

// The observed core (GI3/R3): identical replay, but each tick hands the
// caller the tick's SledDebugSink records + the post-tick state — the
// attribution instrument the consult's Q5 derived-trace layer reads. The
// sink CANNOT change the trajectory (sled_debug_sink_is_write_only), so
// observed and unobserved replay are the same measurement by construction.
template <typename PerTick>
inline ReplayResult replay_taped_ground_observed(const Tape& t,
                                                 PerTick&& per_tick) {
    ReplayResult r;
    world::SnowpackField f;  // NO live terms: override serves everything
    std::size_t gi = 0;
    f.sample_override = [&](const glm::dvec3& dir) {
        if (gi >= t.ground.size()) {
            if (r.ground_mismatch_index < 0)
                r.ground_mismatch_index = static_cast<long>(gi);
            return world::SnowpackField::GroundSample{};
        }
        const GroundRec& g = t.ground[gi];
        // EXACT key check (consult Q1): the first mismatched query IS the
        // first-divergence attribution. Serve the taped sample regardless so
        // the caller can keep stepping to the tick boundary.
        if (r.ground_mismatch_index < 0 &&
            (dir.x != g.dir.x || dir.y != g.dir.y || dir.z != g.dir.z))
            r.ground_mismatch_index = static_cast<long>(gi);
        ++gi;
        return g.g;
    };
    sim::SledParams p = t.params;
    sim::SledState s;
    sim::SledDebugSink sink;
    std::size_t oi = 0;
    for (const TickRec& rec : t.recs) {
        while (oi < t.overrides.size() && t.overrides[oi].tick <= rec.tick)
            s = t.overrides[oi++].pin;
        p.air_temp_c = rec.air_temp_c;
        p.snow_hardness = rec.snow_hardness;
        p.cold_t_ref_c = rec.cold_t_ref_c;
        sink.substeps.clear();
        s = sim::step_sled(s, rec.in, p, f, t.dt, &sink);
        per_tick(rec.tick, s, sink);
        ++r.ticks_replayed;
        if (rec.pin.rolled && r.rolled_tick_tape < 0)
            r.rolled_tick_tape = rec.tick;
        if (s.rolled && r.rolled_tick_replay < 0)
            r.rolled_tick_replay = rec.tick;
        if (r.first_div_tick < 0) {
#define SLEDTAPE_R(f)                                                         \
    if (r.first_div_tick < 0 && !(s.f == rec.pin.f)) {                        \
        r.first_div_tick = rec.tick;                                          \
        r.first_div_field = #f;                                               \
    }
            SLEDTAPE_PIN_D(SLEDTAPE_R)
#undef SLEDTAPE_R
            if (r.first_div_tick < 0 && s.surface != rec.pin.surface) {
                r.first_div_tick = rec.tick;
                r.first_div_field = "surface";
            }
            if (r.first_div_tick < 0 && s.rolled != rec.pin.rolled) {
                r.first_div_tick = rec.tick;
                r.first_div_field = "rolled";
            }
        }
        // Past the first divergence the ground stream desyncs by definition
        // (consult Q1) — stop; everything after is not a measurement.
        if (r.first_div_tick >= 0 || r.ground_mismatch_index >= 0) break;
    }
    return r;
}

inline ReplayResult replay_taped_ground(const Tape& t) {
    return replay_taped_ground_observed(
        t, [](long, const sim::SledState&, const sim::SledDebugSink&) {});
}

// --- tier-2: OPEN-LOOP replay over LIVE-ISH ground (GI3, consult Q7 leg 2) --
// The taped INPUTS fed to a CANDIDATE kernel/params over ground served by
// nearest-taped-dir (locality-windowed) — his actual terrain shape, no exact
// key contract, so a diverging (i.e. fixed) trajectory keeps getting honest
// Road under it. The claim this instrument supports is EXACTLY: "the recorded
// provocation no longer rolls the machine", with the sanity bounds below —
// NEVER "his 360 works" (the claim upgrade is always a new drive). Until R2
// (headless facet_radius_at) this nearest-sample serve is the tier-2 ground;
// its fidelity limit is stated, not hidden: off the taped corridor the served
// ground goes stale, so legs on it must bound speed/height and keep windows
// short.
struct OpenLoopResult {
    long ticks = 0;
    long rolled_tick = -1;     // first rolled latch, -1 = never
    double max_tilt_deg = 0.0;
    double min_v_ms = 1e9, max_v_ms = 0.0;      // ground-speed band sanity
    double max_h_err_m = -1e9, min_h_err_m = 1e9;  // |pos| - served drive_r
    double max_air_s = 0.0;  // longest airborne spell (the flip fence's meat)
};

inline OpenLoopResult replay_open_loop_liveish(const Tape& t,
                                               const sim::SledParams& cand) {
    OpenLoopResult r;
    const std::vector<GroundRec>& G = t.ground;
    if (G.empty() || t.recs.empty()) return r;
    world::SnowpackField f;
    std::size_t cursor = 0;
    auto nearest = [&](const glm::dvec3& dir) {
        const long n = static_cast<long>(G.size());
        const long c = static_cast<long>(cursor);
        const long lo = std::max<long>(0, c - 600);
        const long hi = std::min<long>(n - 1, c + 600);
        double best = -2.0;
        long bi = lo;
        for (long i = lo; i <= hi; ++i) {
            const double d = glm::dot(dir, G[static_cast<std::size_t>(i)].dir);
            if (d > best) {
                best = d;
                bi = i;
            }
        }
        cursor = static_cast<std::size_t>(bi);
        return G[static_cast<std::size_t>(bi)].g;
    };
    f.sample_override = [&](const glm::dvec3& dir) { return nearest(dir); };
    sim::SledParams p = cand;
    sim::SledState s =
        t.overrides.empty() ? sim::SledState{} : t.overrides.front().pin;
    for (const TickRec& rec : t.recs) {
        p.air_temp_c = rec.air_temp_c;
        p.snow_hardness = rec.snow_hardness;
        p.cold_t_ref_c = rec.cold_t_ref_c;
        s = sim::step_sled(s, rec.in, p, f, t.dt);
        ++r.ticks;
        const glm::dvec3 up = glm::normalize(s.position);
        const glm::dvec3 bu = s.orientation * glm::dvec3(0.0, 1.0, 0.0);
        const double tilt =
            glm::degrees(std::acos(std::clamp(glm::dot(bu, up), -1.0, 1.0)));
        r.max_tilt_deg = std::max(r.max_tilt_deg, tilt);
        r.min_v_ms = std::min(r.min_v_ms, s.ground_speed_ms);
        r.max_v_ms = std::max(r.max_v_ms, s.ground_speed_ms);
        const double h_err = glm::length(s.position) - nearest(up).drive_r;
        r.max_h_err_m = std::max(r.max_h_err_m, h_err);
        r.min_h_err_m = std::min(r.min_h_err_m, h_err);
        r.max_air_s = std::max(r.max_air_s, s.air_s);
        if (s.rolled && r.rolled_tick < 0) r.rolled_tick = rec.tick;
    }
    return r;
}

}  // namespace seads_sledtape
