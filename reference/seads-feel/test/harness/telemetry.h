#pragma once

#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <iomanip>
#include <limits>
#include <string>

#include "control/controller.h"
#include "sim/aero.h"
#include "sim/params.h"
#include "sim/state.h"
#include "sim/world.h"

// Per-tick CSV telemetry (HARNESS §4). Section-2 columns: kinematics +
// orientation + alpha/beta/throttle. Controller columns (e, omega_des,
// Inputs, integ, regime flags, true n alongside the G-proxy per SPEC §16
// CQ1) grow in Section 4.
//
// NOTE: telemetry-derived quantities (altitude, alpha, beta) are recomputed
// from state through the SAME sim helpers the plant uses — the instrument
// obeys the same §6.1 frame rules as the kernel (the S3 lesson) and can
// never disagree with the plant about the v-hat guard (sim::current_vhat).

namespace harness {

class CsvTelemetry {
   public:
    explicit CsvTelemetry(const std::string& path) : out_(path) {
        // Full double round-trip precision: default precision 6 quantizes
        // position to ~0.1 m at |p| ~ 17 km — an instrument blind to the
        // sub-mm motion the double-precision state exists to carry.
        out_ << std::setprecision(std::numeric_limits<double>::max_digits10);
        out_ << "t,px,py,pz,vx,vy,vz,speed,altitude,"
                "qw,qx,qy,qz,wx,wy,wz,throttle,alpha,beta\n";
    }

    bool ok() const { return out_.good(); }

    void write(double t, const sim::SimState& s, const sim::AircraftParams& p) {
        const glm::dvec3 v_body_dir =
            sim::body_dir_of(s.orientation, sim::current_vhat(s, p));
        out_ << t << ',' << s.position.x << ',' << s.position.y << ','
             << s.position.z << ',' << s.velocity.x << ',' << s.velocity.y
             << ',' << s.velocity.z << ',' << glm::length(s.velocity) << ','
             << sim::altitude(s.position, p) << ',' << s.orientation.w << ','
             << s.orientation.x << ',' << s.orientation.y << ','
             << s.orientation.z << ',' << s.angular_vel.x << ','
             << s.angular_vel.y << ',' << s.angular_vel.z << ',' << s.throttle
             << ',' << sim::alpha_of(v_body_dir) << ','
             << sim::beta_of(v_body_dir) << '\n';
    }

   private:
    std::ofstream out_;
};

// Controller telemetry (HARNESS §4, the Section-7 tooling): the closed-loop
// instructor columns the raw-mode CsvTelemetry above cannot carry — pointing
// error, ω_des vs achieved ω, the emitted Inputs, the integrator, the
// regime/push/ballistic/deadzone/pursuit flags, and the extracted attitude.
// Per SPEC §16 CQ1 the TRUE load factor (n = q·S·Cl/mg, sim/aero.h single
// source) is logged ALONGSIDE the flat cosΦθ G-proxy the clamp actually
// credits — so a flight log can SEE the ≤0.35 g bias CQ1 rules on, rather than
// re-argue it. This is an instrument read alongside the flight-log stars
// (HARNESS §8), never a gate. All attitude columns come through the plant's
// own helpers (the S3 rule: the instrument obeys the same §6.1 frame law as
// the kernel), so the CSV can never disagree with the controller about
// bank/AoA.
class CtrlCsvTelemetry {
   public:
    explicit CtrlCsvTelemetry(const std::string& path) : out_(path) {
        out_ << std::setprecision(std::numeric_limits<double>::max_digits10);
        out_ << "t,px,py,pz,vx,vy,vz,speed,altitude,"
                "e,wx,wy,wz,wdx,wdy,wdz,in_pitch,in_yaw,in_roll,"
                "integ_x,integ_y,integ_z,"
                "alpha,aoa_filtered,beta,phi,cos_phi_theta,"
                "regime,push,ballistic,deadzoned,pursuit,"
                "load_factor,n_proxy\n";
    }

    bool ok() const { return out_.good(); }

    // One closed-loop tick's row: `s` after the tick, `in` the Inputs
    // control::step emitted, `integ` the controller's carried integrator
    // (⚠ S-dampff, SPEC §0: on a damp_ff'd axis integ reads ~0 — its
    // sustained-torque job moved into the feedforward; near-zero integ_x is
    // the mechanism working, NOT a dead trim channel),
    // `t` the telemetry it returned, `p` for the shared altitude helper. The
    // G-proxy column `n_proxy` is the flat cosΦθ credit (CQ1): logged beside
    // the true `load_factor` so their difference is the felt gravity-bias,
    // visible per tick.
    void write(double t_sec, const sim::SimState& s, const sim::Inputs& in,
               const glm::dvec3& integ, const control::Telemetry& t,
               const sim::AircraftParams& p) {
        out_ << t_sec << ',' << s.position.x << ',' << s.position.y << ','
             << s.position.z << ',' << s.velocity.x << ',' << s.velocity.y
             << ',' << s.velocity.z << ',' << glm::length(s.velocity) << ','
             << sim::altitude(s.position, p);  // plant helper, not |p|-R (S3)
        out_ << ',' << t.e << ',' << s.angular_vel.x << ',' << s.angular_vel.y
             << ',' << s.angular_vel.z << ',' << t.omega_des.x << ','
             << t.omega_des.y << ',' << t.omega_des.z << ',' << in.pitch << ','
             << in.yaw << ',' << in.roll << ',' << integ.x << ',' << integ.y
             << ',' << integ.z << ',' << t.extracted.alpha << ','
             << t.aoa_filtered << ',' << t.extracted.beta << ','
             << t.extracted.phi << ',' << t.extracted.cos_phi_theta << ','
             << (t.regime == control::Regime::FINE ? 0 : 1) << ','
             << (t.push_mode ? 1 : 0) << ',' << (t.ballistic ? 1 : 0) << ','
             << (t.deadzoned ? 1 : 0) << ',' << (t.pursuit ? 1 : 0) << ','
             << t.load_factor << ',' << t.extracted.cos_phi_theta << '\n';
    }

   private:
    std::ofstream out_;
};

}  // namespace harness
