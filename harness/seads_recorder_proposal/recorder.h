#pragma once

// =============================================================================
// SEADS FELT-FLIGHT RECORDER / REPLAYER  (drop-in PROPOSAL — not yet grafted)
// -----------------------------------------------------------------------------
// Header-only recorder + replayer for capturing one of Chad's HAND-FLOWN test
// flights as tick-indexed input, so a felt flight can be replayed bit-for-bit
// through the kernel later and diffed after a kernel change.
//
//   >>> COMPILES ONLY WITHIN THE seads-feel TREE <<<
// The glue section (SECTION 2) references app::/sim::/glm:: and #includes
// seads-feel headers by their in-tree paths. Those paths + every symbol below
// were verified against, at feel/kernel-v5 @ 89447aba5:
//     app/instructor_tick.h   -> app::TickInput, app::LoopState, app::tick,
//                                 TickInput fields (raw_mode, raw_in, throttle,
//                                 flap_cmd, gear_cmd, freelook_held, orient_cmd,
//                                 override_mask[3], override_sign[3], aim_dx,
//                                 aim_dy, aim_gain_scale, aim_rate_ff, frame_ticks)
//     app/loop.h              -> LoopState.curr (sim::SimState)
//     sim/state.h             -> sim::Inputs{pitch,yaw,roll,throttle,flap_cmd,
//                                 gear_cmd}; sim::SimState{position,velocity,
//                                 orientation,angular_vel,throttle}
//     sim/params.h            -> sim::AircraftParams
// A standalone clang invocation WILL error on the glm/app/sim includes — that is
// expected for a graft header; SECTION 1 (the data model + serializer) is
// glm-free and parses on its own.
//
// FIREWALL: this file is APP/RENDER-SIDE ONLY. It taps app::TickInput going IN
// and app::LoopState/sim::SimState coming OUT of the fixed-dt accumulator seam,
// as READ-ONLY (const-ref / by-value) data. It calls NOTHING in sim/ or
// control/ that mutates a trajectory. See INTEGRATION.md for the acceptance
// checklist the reviewing session runs.
//
// WHY TICK-LEVEL (AT-9, SPEC §10): replay determinism requires the resolved
// input at each whole sim_dt tick, captured at the accumulator seam — NOT mouse
// deltas per render frame (which replay differently across frame rates, the
// divergence test_at9.cpp forbids). app::tick never sees frame time.
//
// THIRD GOLDEN LAYER: alongside test/golden/golden_flight.h (plant pin) and
// test/golden/controller_golden.h (controller pin); NOT a replacement. Carries
// an fnv1a content signature (the goldens' tamper-loud discipline).
// =============================================================================

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace seads_replay {

// ===========================================================================
// SECTION 1 — DATA MODEL + SERIALIZER  (glm-free; parses standalone)
// ===========================================================================

// One sim-tick of resolved input (the AT-9 seam payload) + a SimState pin for
// bit-exact replay verification. All POD doubles — no engine types — so the
// record format and its (de)serializer are self-contained.
//
// aim_dx/aim_dy are POST-CURVE: input/aim_curve.h sensitivity is applied
// UPSTREAM at the device accrual in main.cpp (test_at9.cpp header note), so the
// consumed-tick value recorded here is curve-independent by construction.
struct TickRecord {
    long tick = 0;  // sim-tick index since record start (monotone)

    // resolved app::TickInput, flattened to POD:
    int raw_mode = 0;
    double raw_pitch = 0.0, raw_yaw = 0.0, raw_roll = 0.0, raw_throttle = 0.0;
    double throttle = 0.0;
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    int freelook_held = 0;
    int orient_cmd = 0;
    int override_mask[3] = {0, 0, 0};
    double override_sign[3] = {0.0, 0.0, 0.0};
    double aim_dx = 0.0;
    double aim_dy = 0.0;
    double aim_gain_scale = 1.0;
    double aim_rate_ff[3] = {0.0, 0.0, 0.0};
    int frame_ticks = 1;

    // SimState pin AFTER this tick (S1: the double state, never a rendered
    // float — at |p|=R a float ulp is a tick of gravity). Not fed on replay;
    // compared against the live re-simulated state to prove determinism.
    int has_state_pin = 0;
    double pin_position[3] = {0.0, 0.0, 0.0};
    double pin_velocity[3] = {0.0, 0.0, 0.0};
    double pin_orientation[4] = {1.0, 0.0, 0.0, 0.0};  // w, x, y, z
    double pin_angular_vel[3] = {0.0, 0.0, 0.0};
    double pin_throttle = 0.0;
};

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

// Body only (no signature line) so the writer can hash it and the reader
// re-hash and compare — full double round-trip precision (max_digits10).
inline std::string serialize_body(const std::vector<TickRecord>& recs,
                                  const std::string& version_tag) {
    std::ostringstream o;
    o << std::setprecision(std::numeric_limits<double>::max_digits10);
    o << "# seads-felt-flight v1  tag=" << version_tag << '\n';
    o << "# columns: " << kRecColumns << '\n';
    for (const TickRecord& r : recs) {
        o << r.tick << ' ' << r.raw_mode << ' ' << r.raw_pitch << ' '
          << r.raw_yaw << ' ' << r.raw_roll << ' ' << r.raw_throttle << ' '
          << r.throttle << ' ' << r.flap_cmd << ' ' << r.gear_cmd << ' '
          << r.freelook_held << ' ' << r.orient_cmd << ' ' << r.override_mask[0]
          << ' ' << r.override_mask[1] << ' ' << r.override_mask[2] << ' '
          << r.override_sign[0] << ' ' << r.override_sign[1] << ' '
          << r.override_sign[2] << ' ' << r.aim_dx << ' ' << r.aim_dy << ' '
          << r.aim_gain_scale << ' ' << r.aim_rate_ff[0] << ' '
          << r.aim_rate_ff[1] << ' ' << r.aim_rate_ff[2] << ' ' << r.frame_ticks
          << ' ' << r.pin_position[0] << ' ' << r.pin_position[1] << ' '
          << r.pin_position[2] << ' ' << r.pin_velocity[0] << ' '
          << r.pin_velocity[1] << ' ' << r.pin_velocity[2] << ' '
          << r.pin_orientation[0] << ' ' << r.pin_orientation[1] << ' '
          << r.pin_orientation[2] << ' ' << r.pin_orientation[3] << ' '
          << r.pin_angular_vel[0] << ' ' << r.pin_angular_vel[1] << ' '
          << r.pin_angular_vel[2] << ' ' << r.pin_throttle << '\n';
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
        if (!line.empty() && line[0] == '#') {
            if (version_tag) {
                const auto pos = line.find("tag=");
                if (pos != std::string::npos)
                    *version_tag = line.substr(pos + 4);
            }
            continue;  // comment / header row
        }
        if (!line.empty()) data_lines.push_back(line);
    }
    if (sig_ok) *sig_ok = have_sig && (fnv1a(body.str()) == stored_sig);
    for (const std::string& dl : data_lines) {
        std::istringstream s(dl);
        TickRecord r;
        s >> r.tick >> r.raw_mode >> r.raw_pitch >> r.raw_yaw >> r.raw_roll >>
            r.raw_throttle >> r.throttle >> r.flap_cmd >> r.gear_cmd >>
            r.freelook_held >> r.orient_cmd >> r.override_mask[0] >>
            r.override_mask[1] >> r.override_mask[2] >> r.override_sign[0] >>
            r.override_sign[1] >> r.override_sign[2] >> r.aim_dx >> r.aim_dy >>
            r.aim_gain_scale >> r.aim_rate_ff[0] >> r.aim_rate_ff[1] >>
            r.aim_rate_ff[2] >> r.frame_ticks >> r.pin_position[0] >>
            r.pin_position[1] >> r.pin_position[2] >> r.pin_velocity[0] >>
            r.pin_velocity[1] >> r.pin_velocity[2] >> r.pin_orientation[0] >>
            r.pin_orientation[1] >> r.pin_orientation[2] >> r.pin_orientation[3] >>
            r.pin_angular_vel[0] >> r.pin_angular_vel[1] >> r.pin_angular_vel[2] >>
            r.pin_throttle;
        r.has_state_pin = 1;
        out.push_back(r);
    }
    return true;
}

}  // namespace seads_replay

// ===========================================================================
// SECTION 2 — IN-TREE GLUE  (references app::/sim::/glm:: — seads-feel only)
// ---------------------------------------------------------------------------
// Guarded so a standalone parse of SECTION 1 does not require the engine tree.
// Define SEADS_RECORDER_STANDALONE to compile the data model / serializer alone
// (e.g. for an out-of-tree unit check of the file format).
// ===========================================================================
#ifndef SEADS_RECORDER_STANDALONE

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"  // app::TickInput, app::LoopState, app::tick
#include "sim/params.h"           // sim::AircraftParams
#include "sim/state.h"            // sim::Inputs, sim::SimState

namespace seads_replay {

// Capture the resolved input + post-tick state pin. Both parameters are
// CONST references — the recorder structurally cannot write back into the loop
// (firewall criterion (c)). Call INSIDE the step_frame tick loop, immediately
// after `tick(st, in, ...)` returns; `st` is the LoopState after the tick.
inline TickRecord record_tick(long tick, const app::TickInput& in,
                              const app::LoopState& st) {
    TickRecord r;
    r.tick = tick;
    r.raw_mode = in.raw_mode ? 1 : 0;
    r.raw_pitch = in.raw_in.pitch;
    r.raw_yaw = in.raw_in.yaw;
    r.raw_roll = in.raw_in.roll;
    r.raw_throttle = in.raw_in.throttle;
    r.throttle = in.throttle;
    r.flap_cmd = in.flap_cmd;
    r.gear_cmd = in.gear_cmd;
    r.freelook_held = in.freelook_held ? 1 : 0;
    r.orient_cmd = in.orient_cmd ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        r.override_mask[i] = in.override_mask[i] ? 1 : 0;
        r.override_sign[i] = in.override_sign[i];
    }
    r.aim_dx = in.aim_dx;
    r.aim_dy = in.aim_dy;
    r.aim_gain_scale = in.aim_gain_scale;
    r.aim_rate_ff[0] = in.aim_rate_ff.x;
    r.aim_rate_ff[1] = in.aim_rate_ff.y;
    r.aim_rate_ff[2] = in.aim_rate_ff.z;
    r.frame_ticks = in.frame_ticks;
    r.has_state_pin = 1;
    r.pin_position[0] = st.curr.position.x;
    r.pin_position[1] = st.curr.position.y;
    r.pin_position[2] = st.curr.position.z;
    r.pin_velocity[0] = st.curr.velocity.x;
    r.pin_velocity[1] = st.curr.velocity.y;
    r.pin_velocity[2] = st.curr.velocity.z;
    r.pin_orientation[0] = st.curr.orientation.w;
    r.pin_orientation[1] = st.curr.orientation.x;
    r.pin_orientation[2] = st.curr.orientation.y;
    r.pin_orientation[3] = st.curr.orientation.z;
    r.pin_angular_vel[0] = st.curr.angular_vel.x;
    r.pin_angular_vel[1] = st.curr.angular_vel.y;
    r.pin_angular_vel[2] = st.curr.angular_vel.z;
    r.pin_throttle = st.curr.throttle;
    return r;
}

// Reconstruct the app::TickInput to feed back into app::tick on replay.
inline app::TickInput to_tick_input(const TickRecord& r) {
    app::TickInput in;
    in.raw_mode = r.raw_mode != 0;
    in.raw_in.pitch = static_cast<float>(r.raw_pitch);
    in.raw_in.yaw = static_cast<float>(r.raw_yaw);
    in.raw_in.roll = static_cast<float>(r.raw_roll);
    in.raw_in.throttle = static_cast<float>(r.raw_throttle);
    in.throttle = r.throttle;
    in.flap_cmd = r.flap_cmd;
    in.gear_cmd = r.gear_cmd;
    in.freelook_held = r.freelook_held != 0;
    in.orient_cmd = r.orient_cmd != 0;
    for (int i = 0; i < 3; ++i) {
        in.override_mask[i] = r.override_mask[i] != 0;
        in.override_sign[i] = r.override_sign[i];
    }
    in.aim_dx = r.aim_dx;
    in.aim_dy = r.aim_dy;
    in.aim_gain_scale = r.aim_gain_scale;
    in.aim_rate_ff =
        glm::dvec3(r.aim_rate_ff[0], r.aim_rate_ff[1], r.aim_rate_ff[2]);
    in.frame_ticks = r.frame_ticks;
    return in;
}

// A minimal in-process recorder the app owns (accumulate, then flush once).
// READ-ONLY taps: on_tick takes const refs and never returns a mutable handle
// to engine state (firewall criterion (c)).
class Recorder {
   public:
    void reset() {
        recs_.clear();
        tick_ = 0;
    }
    void on_tick(const app::TickInput& in, const app::LoopState& st) {
        recs_.push_back(record_tick(tick_++, in, st));
    }
    bool flush(const std::string& path, const std::string& version_tag) const {
        return write_records(path, recs_, version_tag);
    }
    const std::vector<TickRecord>& records() const { return recs_; }

   private:
    std::vector<TickRecord> recs_;
    long tick_ = 0;
};

}  // namespace seads_replay

#endif  // SEADS_RECORDER_STANDALONE
