#pragma once

// =============================================================================
// SEADS FELT-FLIGHT RECORDER / REPLAYER  (drop-in PROPOSAL — not yet grafted)
// -----------------------------------------------------------------------------
// A header-only recorder + replayer for capturing one of Chad's HAND-FLOWN test
// flights as tick-indexed input, so a felt flight can be replayed bit-for-bit
// through the kernel later and diffed after a kernel change. It is written to
// live at  test/harness/recorder.h  in the seads-feel tree and compile against
// the same headers the harness already uses (app/instructor_tick.h, sim/state.h).
//
// WHY TICK-LEVEL, NOT FRAME-LEVEL (AT-9, SPEC §10). Replay determinism requires
// recording at the SIM-TICK level: the resolved app::TickInput consumed at each
// whole sim_dt tick, captured at the fixed-dt accumulator seam. Recording mouse
// deltas per RENDER frame (as the Roblox recorder must, lacking a fixed-dt seam)
// would replay differently across frame rates — exactly the divergence
// test_at9.cpp forbids. app::tick never sees frame time, so a tick-indexed input
// stream replays to the bit-identical trajectory at ANY fps. See INTEGRATION.md
// for the one-line seam this hooks.
//
// WHERE THE FELT-FLIGHT GOLDENS SIT. This is a THIRD golden layer, alongside the
// two existing bit-level pins — it does NOT replace them:
//   1. test/golden/golden_flight.h       plant trajectory pin (scripted input)
//   2. test/golden/controller_golden.h   closed-loop controller pin
//   3. (this)  felt-flight recordings     Chad's real hand-flown runs, replayed
// Layers 1-2 are synthetic scripts that gate the math. Layer 3 pins how the
// kernel answers a REAL human's stick, on the same double-precision SimState and
// with a content signature (fnv1a below) so a tampered/aliased recording is loud.
// =============================================================================

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "app/instructor_tick.h"  // app::TickInput, app::LoopState, app::tick
#include "sim/params.h"
#include "sim/state.h"

namespace seads_replay {

// One sim-tick of resolved input — the AT-9 seam payload (every dynamics-bearing
// field app::step_frame forwards to app::tick), plus a SimState pin for
// bit-exact verification. `aim_dx/aim_dy` are POST-CURVE: the input/aim_curve.h
// rate-keyed sensitivity is applied UPSTREAM at the device accrual in main.cpp
// (test_at9.cpp header note), so what lands on the consuming tick is already the
// curve output. Recording it here makes the stream curve-independent by
// construction — a replay reproduces the flight without re-deriving the curve.
struct TickRecord {
    long tick = 0;  // sim-tick index since record start (monotone, gap = paused)

    // --- resolved app::TickInput (the exact fields step_frame builds) ---
    bool raw_mode = false;
    sim::Inputs raw_in{};
    double throttle = 0.0;
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    bool freelook_held = false;
    bool orient_cmd = false;
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    double aim_dx = 0.0;  // consumed on the consuming tick; 0 elsewhere
    double aim_dy = 0.0;
    double aim_gain_scale = 1.0;
    glm::dvec3 aim_rate_ff{0.0};  // ZOH-forwarded smear on ticks 2..N of a frame
    int frame_ticks = 1;

    // --- SimState pin AFTER this tick (verification; not fed on replay) ---
    // Bit-comparing this on replay is the determinism proof (S1: assert the
    // double state, never a rendered float — at |p|=R a float ulp is a tick of
    // gravity). Optional: leave zeroed and skip the compare for a lean file.
    bool has_state_pin = false;
    glm::dvec3 pin_position{0.0};
    glm::dvec3 pin_velocity{0.0};
    glm::dquat pin_orientation{1.0, 0.0, 0.0, 0.0};
    glm::dvec3 pin_angular_vel{0.0};
    double pin_throttle = 0.0;
};

// Reconstruct the app::TickInput to feed back into app::tick on replay.
inline app::TickInput to_tick_input(const TickRecord& r) {
    app::TickInput in;
    in.raw_mode = r.raw_mode;
    in.raw_in = r.raw_in;
    in.throttle = r.throttle;
    in.flap_cmd = r.flap_cmd;
    in.gear_cmd = r.gear_cmd;
    in.freelook_held = r.freelook_held;
    in.orient_cmd = r.orient_cmd;
    for (int i = 0; i < 3; ++i) {
        in.override_mask[i] = r.override_mask[i];
        in.override_sign[i] = r.override_sign[i];
    }
    in.aim_dx = r.aim_dx;
    in.aim_dy = r.aim_dy;
    in.aim_gain_scale = r.aim_gain_scale;
    in.aim_rate_ff = r.aim_rate_ff;
    in.frame_ticks = r.frame_ticks;
    return in;
}

// Capture the resolved input + post-tick state pin. Call this INSIDE the
// step_frame tick loop, immediately after `tick(st, in, ...)` returns (see
// INTEGRATION.md); `st` is the LoopState after the tick.
inline TickRecord record_tick(long tick, const app::TickInput& in,
                              const app::LoopState& st) {
    TickRecord r;
    r.tick = tick;
    r.raw_mode = in.raw_mode;
    r.raw_in = in.raw_in;
    r.throttle = in.throttle;
    r.flap_cmd = in.flap_cmd;
    r.gear_cmd = in.gear_cmd;
    r.freelook_held = in.freelook_held;
    r.orient_cmd = in.orient_cmd;
    for (int i = 0; i < 3; ++i) {
        r.override_mask[i] = in.override_mask[i];
        r.override_sign[i] = in.override_sign[i];
    }
    r.aim_dx = in.aim_dx;
    r.aim_dy = in.aim_dy;
    r.aim_gain_scale = in.aim_gain_scale;
    r.aim_rate_ff = in.aim_rate_ff;
    r.frame_ticks = in.frame_ticks;
    r.has_state_pin = true;
    r.pin_position = st.curr.position;
    r.pin_velocity = st.curr.velocity;
    r.pin_orientation = st.curr.orientation;
    r.pin_angular_vel = st.curr.angular_vel;
    r.pin_throttle = st.curr.throttle;
    return r;
}

// -----------------------------------------------------------------------------
// SERIALIZATION — a text .seadsrec file at full double round-trip precision
// (max_digits10), one whitespace-separated row per tick, a named-column header,
// and a content signature (fnv1a over the body) as line 1 for tamper/alias
// detection (mirrors the goldens' signature-verification discipline).
// -----------------------------------------------------------------------------

inline std::uint64_t fnv1a(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628257ull;
    }
    return h;
}

inline const char* kRecColumns =
    "tick raw_mode raw_pitch raw_yaw raw_roll raw_throttle throttle flap_cmd "
    "gear_cmd freelook orient om0 om1 om2 os0 os1 os2 aim_dx aim_dy "
    "aim_gain_scale ffx ffy ffz frame_ticks "
    "px py pz vx vy vz qw qx qy qz wx wy wz pin_throttle";

// Emit the body (no signature line) so the writer can hash it, then prepend the
// signature. Kept separate so the reader can re-hash and compare.
inline std::string serialize_body(const std::vector<TickRecord>& recs,
                                  const std::string& version_tag) {
    std::ostringstream o;
    o << std::setprecision(std::numeric_limits<double>::max_digits10);
    o << "# seads-felt-flight v1  tag=" << version_tag << '\n';
    o << "# columns: " << kRecColumns << '\n';
    for (const TickRecord& r : recs) {
        o << r.tick << ' ' << (r.raw_mode ? 1 : 0) << ' ' << r.raw_in.pitch
          << ' ' << r.raw_in.yaw << ' ' << r.raw_in.roll << ' '
          << r.raw_in.throttle << ' ' << r.throttle << ' ' << r.flap_cmd << ' '
          << r.gear_cmd << ' ' << (r.freelook_held ? 1 : 0) << ' '
          << (r.orient_cmd ? 1 : 0) << ' ' << (r.override_mask[0] ? 1 : 0) << ' '
          << (r.override_mask[1] ? 1 : 0) << ' ' << (r.override_mask[2] ? 1 : 0)
          << ' ' << r.override_sign[0] << ' ' << r.override_sign[1] << ' '
          << r.override_sign[2] << ' ' << r.aim_dx << ' ' << r.aim_dy << ' '
          << r.aim_gain_scale << ' ' << r.aim_rate_ff.x << ' ' << r.aim_rate_ff.y
          << ' ' << r.aim_rate_ff.z << ' ' << r.frame_ticks << ' '
          << r.pin_position.x << ' ' << r.pin_position.y << ' '
          << r.pin_position.z << ' ' << r.pin_velocity.x << ' '
          << r.pin_velocity.y << ' ' << r.pin_velocity.z << ' '
          << r.pin_orientation.w << ' ' << r.pin_orientation.x << ' '
          << r.pin_orientation.y << ' ' << r.pin_orientation.z << ' '
          << r.pin_angular_vel.x << ' ' << r.pin_angular_vel.y << ' '
          << r.pin_angular_vel.z << ' ' << r.pin_throttle << '\n';
    }
    return o.str();
}

inline bool write_records(const std::string& path,
                          const std::vector<TickRecord>& recs,
                          const std::string& version_tag) {
    const std::string body = serialize_body(recs, version_tag);
    std::ofstream out(path);
    if (!out.good()) return false;
    out << "# sig fnv1a=" << fnv1a(body) << '\n' << body;
    return out.good();
}

// Parse a .seadsrec back into TickRecords and (optionally) verify the signature.
inline bool read_records(const std::string& path, std::vector<TickRecord>& out,
                         std::string* version_tag = nullptr,
                         bool* sig_ok = nullptr) {
    std::ifstream in(path);
    if (!in.good()) return false;
    std::string line;
    std::uint64_t stored_sig = 0;
    bool have_sig = false;
    std::ostringstream body;
    std::vector<std::string> data_lines;
    while (std::getline(in, line)) {
        if (line.rfind("# sig fnv1a=", 0) == 0) {
            stored_sig = std::stoull(line.substr(12));
            have_sig = true;
            continue;  // the sig line is not part of the hashed body
        }
        body << line << '\n';
        if (line.rfind("# tag", 0) == 0 && version_tag) {
            // "# seads-felt-flight v1  tag=..." handled below via the header line
        }
        if (!line.empty() && line[0] == '#') {
            if (version_tag) {
                const auto pos = line.find("tag=");
                if (pos != std::string::npos)
                    *version_tag = line.substr(pos + 4);
            }
            continue;  // comment/header row
        }
        if (!line.empty()) data_lines.push_back(line);
    }
    if (sig_ok) *sig_ok = have_sig && (fnv1a(body.str()) == stored_sig);
    for (const std::string& dl : data_lines) {
        std::istringstream s(dl);
        TickRecord r;
        int rawm = 0, fl = 0, orient = 0, om0 = 0, om1 = 0, om2 = 0;
        s >> r.tick >> rawm >> r.raw_in.pitch >> r.raw_in.yaw >> r.raw_in.roll >>
            r.raw_in.throttle >> r.throttle >> r.flap_cmd >> r.gear_cmd >> fl >>
            orient >> om0 >> om1 >> om2 >> r.override_sign[0] >>
            r.override_sign[1] >> r.override_sign[2] >> r.aim_dx >> r.aim_dy >>
            r.aim_gain_scale >> r.aim_rate_ff.x >> r.aim_rate_ff.y >>
            r.aim_rate_ff.z >> r.frame_ticks >> r.pin_position.x >>
            r.pin_position.y >> r.pin_position.z >> r.pin_velocity.x >>
            r.pin_velocity.y >> r.pin_velocity.z >> r.pin_orientation.w >>
            r.pin_orientation.x >> r.pin_orientation.y >> r.pin_orientation.z >>
            r.pin_angular_vel.x >> r.pin_angular_vel.y >> r.pin_angular_vel.z >>
            r.pin_throttle;
        r.raw_mode = rawm != 0;
        r.freelook_held = fl != 0;
        r.orient_cmd = orient != 0;
        r.override_mask[0] = om0 != 0;
        r.override_mask[1] = om1 != 0;
        r.override_mask[2] = om2 != 0;
        r.has_state_pin = true;
        out.push_back(r);
    }
    return true;
}

// -----------------------------------------------------------------------------
// A minimal in-process recorder the app can own (accumulate, then flush once).
// -----------------------------------------------------------------------------
class Recorder {
   public:
    void reset() {
        recs_.clear();
        tick_ = 0;
    }
    // Call after each app::tick in the step_frame loop (INTEGRATION.md seam).
    void on_tick(const app::TickInput& in, const app::LoopState& st) {
        recs_.push_back(record_tick(tick_++, in, st));
    }
    bool flush(const std::string& path, const std::string& version_tag) {
        return write_records(path, recs_, version_tag);
    }
    const std::vector<TickRecord>& records() const { return recs_; }

   private:
    std::vector<TickRecord> recs_;
    long tick_ = 0;
};

}  // namespace seads_replay
