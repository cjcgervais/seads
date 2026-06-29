#!/usr/bin/env python3
"""
ref_kernel.py — SEADS deterministic kernel REFERENCE (ATM-Sphere v1.2r0)

Pure-Python reference implementation of the sealed kinematics, using detmath_ref only
(no libm transcendentals). It defines the canonical behavior the C++ kernel must
bit-match, and it generates the GOLDEN-SK-Sphere-001 world_hash.

Canonical snapshot format (little-endian), so C++ and Python produce identical bytes:
  offset 0  : u16  mode (1 = ATM)
  offset 2  : u16  pad (0)
  offset 4  : u32  tick_count
  offset 8  : f64  dt_s
  offset 16 : f64  R_m
  offset 24 : u32  n_aircraft
  offset 28 : u32  pad (0)
  offset 32 : per aircraft, 7 x f64 = [lat, lon, psi, phi, alt, tas, gamma]
              (radians / meters / m s^-1 / radians)  -- gamma (flight-path angle) added in B2 (v1.6r0)

world_hash = SHA-256 of the snapshot bytes.

Usage:
  python tools/ref_kernel.py [--rails config/rails/atm.json] [--out run.bin]
                             [--seal]   # write sealed golden under tests/golden/<id>/
Exit: 0 on success.
"""
import argparse, json, struct, hashlib, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import detmath_ref as dm
import envelopes as envmod

ROOT = Path(__file__).resolve().parent.parent

# --- B1 longitudinal-energy model constants (ATM-Sphere v1.5r0) -------------------------
# Exact hex-float literals so Python and C++ (kernel.cpp) share identical bit patterns.
# RHO0: constant sea-level ISA air density (the "still air / constant atmosphere" rail; ISA
# density-vs-altitude is deferred to B5). V_MIN: hard speed floor keeping psi_dot = g0*tan(phi)/V
# and the drag solve well-defined (a real stall model is B3). See ADR-Step8-FlightModel-B1.
RHO0 = float.fromhex('0x1.399999999999ap+0')   # 1.225 kg/m^3
V_MIN = float.fromhex('0x1.e000000000000p+4')   # 30.0 m/s

# --- B2 lift-&-pitch constants (ATM-Sphere v1.6r0) --------------------------------------
# Global placeholder structural clamp on the commanded load factor n. Per-airframe C_Lmax /
# accelerated stall / structural-g (and the corner speed that falls out) are deferred to B3;
# here n is only bounded to keep the aero/attitude solve well-defined. Exact hex-floats so
# Python and C++ (kernel.cpp) parse identical bits. See ADR-Step8-FlightModel-B2.
N_MIN = float.fromhex('-0x1.8000000000000p+1')  # -3.0  (push-over / negative g)
N_MAX = float.fromhex('0x1.2000000000000p+3')   # +9.0  (hard pull)


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def great_circle_step(lat, lon, bearing, s, R):
    """Closed-form intrinsic-S2 great-circle step. det_math only."""
    alpha = s / R
    sinlat1 = dm.det_sin(lat)
    coslat1 = dm.det_cos(lat)
    ca = dm.det_cos(alpha)
    sa = dm.det_sin(alpha)
    cb = dm.det_cos(bearing)
    sb = dm.det_sin(bearing)
    sin_lat2 = sinlat1 * ca + coslat1 * sa * cb
    sin_lat2 = clamp(sin_lat2, -1.0, 1.0)
    lat2 = dm.det_asin(sin_lat2)
    y = sb * sa * coslat1
    x = ca - sinlat1 * sin_lat2
    lon2 = dm.wrap_pi(lon + dm.det_atan2(y, x))
    return lat2, lon2


def ceiling_climb_rate(requested_mps, alt, atm_top, soft):
    """Soft-band predamp -> hard clamp. Monotone reduction in the soft band, no overshoot."""
    if requested_mps <= 0.0:
        return requested_mps
    band_lo = atm_top - soft
    if alt >= band_lo:
        frac = (atm_top - alt) / soft
        frac = clamp(frac, 0.0, 1.0)
        return requested_mps * frac
    return requested_mps


class Aircraft:
    # B2 (v1.6r0): gamma (flight-path angle, rad) is a real stored state. Defaults to 0.0 so the
    # no-arg straight golden and any 6-tuple caller still build a level-flying aircraft.
    __slots__ = ("lat", "lon", "psi", "phi", "alt", "tas", "gamma")

    def __init__(self, lat, lon, psi, phi, alt, tas, gamma=0.0):
        self.lat, self.lon, self.psi, self.phi, self.alt, self.tas = lat, lon, psi, phi, alt, tas
        self.gamma = gamma


class Kernel:
    def __init__(self, rails):
        r = rails["rails"]
        self.R = float(r["geometry"]["radius_m"])
        self.dt = float(r["dt_s"])
        self.g0 = float(r["gravity_mps2"])
        self.atm_top = float(r["atm_top_m"])
        self.soft = float(r["atm_soft_band_m"])
        self.aircraft = []

    def _advance(self, ac, req):
        """Kinematic tail for one aircraft: coordinated-turn + great-circle + ceiling-clamped
        vertical. `ac.phi` is already final for this tick. This is the VERBATIM body of the
        original step() inner loop; both the straight golden (req=0, phi=0) and the scenario
        path go through it, so the sealed golden stays byte-identical. C++ mirror: Kernel::advance_."""
        dt, R, g0 = self.dt, self.R, self.g0
        V = ac.tas
        # coordinated-turn law
        psi_dot = g0 * dm.det_tan(ac.phi) / V
        ac.psi = dm.wrap_2pi(ac.psi + psi_dot * dt)
        # great-circle horizontal advance
        s = V * dt
        ac.lat, ac.lon = great_circle_step(ac.lat, ac.lon, ac.psi, s, R)
        # vertical (ceiling-clamped)
        rate = ceiling_climb_rate(req, ac.alt, self.atm_top, self.soft)
        ac.alt = clamp(ac.alt + rate * dt, 0.0, self.atm_top)

    def step(self, climb_inputs=None):
        for i, ac in enumerate(self.aircraft):
            req = 0.0 if climb_inputs is None else climb_inputs[i]
            self._advance(ac, req)

    def step_scenario(self, commands, envelopes):
        """Envelope-driven step. commands[i] = (target_phi_rad, target_g[, throttle]);
        throttle defaults to 0.0 if absent. envelopes[i] = dict from envelopes.load_envelope.
        Op order is the sealed byte spec and MUST match Kernel::step(cmd,env) in kernel.cpp exactly.

        B2 (v1.6r0): full 3-DOF point-mass step. Pitch is now REAL: flight-path angle gamma is a
        stored state, driven by the commanded load factor n (g-command) through the lift vector.
        Altitude is EARNED (alt_dot = V*sin gamma); pulling g raises induced drag and bleeds speed
        (energy-vs-turn in the pitch plane). It strictly generalizes the B1 kinematics:
          - wings level (phi=0), n=1, gamma=0 -> level flight;
          - level coordinated turn n=1/cos(phi), gamma=0 -> psi_dot = g0*tan(phi)/V (the old law).
        Rates use the UPDATED speed Vnew (mirrors B1's advance_); the velocity-vector trig uses the
        OLD gamma, position uses the NEW gamma. cos(gamma)->0 (vertical) is a documented singularity
        in psi_dot; scenarios/viewer stay well inside +/-90 deg (real loop/vertical handling is post-B3).
        See ADR-Step8-FlightModel-B2."""
        dt, g0 = self.dt, self.g0
        R, atm_top, soft = self.R, self.atm_top, self.soft
        for i, ac in enumerate(self.aircraft):
            V = ac.tas
            e = envelopes[i]
            # --- bank dynamics (unchanged from B1): slew toward clamped target at roll_rate(V) ---
            phimax = dm.lut_eval(e["phi_max"][0], e["phi_max"][1], V)
            rollrate = dm.lut_eval(e["roll_rate"][0], e["roll_rate"][1], V)
            cmdphi = clamp(commands[i][0], -phimax, phimax)
            step_max = rollrate * dt
            delta = cmdphi - ac.phi
            delta = clamp(delta, -step_max, step_max)
            ac.phi = ac.phi + delta
            ac.phi = clamp(ac.phi, -phimax, phimax)
            # --- commanded load factor (global placeholder clamp; per-airframe C_Lmax = B3) ---
            n = clamp(commands[i][1], N_MIN, N_MAX)
            # --- trig of NEW phi and OLD gamma (single eval, fixed order) ---
            cphi = dm.det_cos(ac.phi)
            sphi = dm.det_sin(ac.phi)
            cg = dm.det_cos(ac.gamma)
            sg = dm.det_sin(ac.gamma)
            # --- drag/thrust with current V and load factor n (B1 algebra; +,-,*,/ and det_* only) ---
            q = 0.5 * RHO0 * V * V                        # dynamic pressure
            qS = q * e["wing_area_m2"]
            L = n * e["mass_kg"] * g0                     # lift = n * weight
            CL = L / qS
            Dp = qS * e["cd0"]                            # parasitic drag
            Di = e["induced_k"] * CL * CL * qS            # induced drag (rises with n -> g bleeds speed)
            D = Dp + Di
            thr = clamp(commands[i][2] if len(commands[i]) > 2 else 0.0, 0.0, 1.0)
            T = thr * e["thrust_static_n"] * (1.0 - V / e["v_max_mps"])
            if T < 0.0:
                T = 0.0
            # --- speed: gravity now acts along the flight path (uses OLD gamma) ---
            Vdot = (T - D) / e["mass_kg"] - g0 * sg
            Vnew = V + Vdot * dt
            if Vnew < V_MIN:
                Vnew = V_MIN
            ac.tas = Vnew
            # --- flight-path angle integrates (uses Vnew, OLD gamma), then track heading turns ---
            gdot = (g0 / Vnew) * (n * cphi - cg)
            ac.gamma = ac.gamma + gdot * dt
            psidot = (g0 / Vnew) * (n * sphi / cg)
            ac.psi = dm.wrap_2pi(ac.psi + psidot * dt)
            # --- horizontal great-circle advance: ground speed = Vnew*cos(NEW gamma) ---
            cgN = dm.det_cos(ac.gamma)
            s = Vnew * cgN * dt
            ac.lat, ac.lon = great_circle_step(ac.lat, ac.lon, ac.psi, s, R)
            # --- altitude EARNED: vertical rate Vnew*sin(NEW gamma), ceiling soft-band predamp + clamp ---
            sgN = dm.det_sin(ac.gamma)
            w = Vnew * sgN
            w_eff = ceiling_climb_rate(w, ac.alt, atm_top, soft)
            ac.alt = clamp(ac.alt + w_eff * dt, 0.0, atm_top)

    def run(self, ticks):
        for _ in range(ticks):
            self.step()

    def snapshot(self, tick_count):
        buf = bytearray()
        buf += struct.pack("<HHIddII", 1, 0, tick_count & 0xFFFFFFFF,
                           self.dt, self.R, len(self.aircraft), 0)
        for ac in self.aircraft:
            buf += struct.pack("<7d", ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma)
        return bytes(buf)


def deg2rad(d):
    return d * (dm.PI / 180.0)


def build_golden(rails):
    g = rails["golden"]
    k = Kernel(rails)
    k.aircraft.append(Aircraft(
        lat=deg2rad(g["start_lat_deg"]),
        lon=deg2rad(g["start_lon_deg"]),
        psi=deg2rad(g["start_psi_deg"]),
        phi=deg2rad(g["start_phi_deg"]),
        alt=float(g["start_alt_m"]),
        tas=float(g["start_tas_mps"]),
    ))
    return k, int(g["ticks"]), g["id"]


def build_scenario(rails, scenario):
    """Build a Kernel + per-aircraft (schedule, envelope) from a scenario JSON doc.
    Returns (kernel, ticks, gid, schedules, envelopes)."""
    k = Kernel(rails)
    schedules = []
    envelopes = []
    for ac in scenario["aircraft"]:
        s = ac["start"]
        k.aircraft.append(Aircraft(
            lat=deg2rad(s["lat_deg"]), lon=deg2rad(s["lon_deg"]),
            psi=deg2rad(s["psi_deg"]), phi=deg2rad(s["phi_deg"]),
            alt=float(s["alt_m"]), tas=float(s["tas_mps"]),
        ))
        schedules.append(ac["schedule"])
        envelopes.append(envmod.load_envelope(ac["envelope"]))
    ticks = int(scenario["ticks"])
    gid = scenario["header"]["id"]
    return k, ticks, gid, schedules, envelopes


def run_scenario(k, ticks, schedules, envelopes):
    """Drive the scripted-timeline. Phase select is integer-only (largest start_tick <= t)."""
    for t in range(ticks):
        cmds = []
        for sched in schedules:
            idx = 0
            for j, ph in enumerate(sched):
                if int(ph["start_tick"]) <= t:
                    idx = j
                else:
                    break
            ph = sched[idx]
            thr = float(ph.get("throttle", 0.0))
            cmds.append((deg2rad(ph["bank_deg"]), float(ph["g_cmd"]), thr))
        k.step_scenario(cmds, envelopes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rails", default=str(ROOT / "config" / "rails" / "atm.json"))
    ap.add_argument("--scenario", default=None,
                    help="run a config/scenarios/<id>.json scenario instead of the straight golden")
    ap.add_argument("--out", default=None)
    ap.add_argument("--seal", action="store_true",
                    help="write sealed golden snapshot + expected.world_hash")
    args = ap.parse_args()

    rails = json.loads(Path(args.rails).read_text(encoding="utf-8"))
    if args.scenario:
        scenario = json.loads(Path(args.scenario).read_text(encoding="utf-8"))
        k, ticks, gid, schedules, envelopes = build_scenario(rails, scenario)
        run_scenario(k, ticks, schedules, envelopes)
    else:
        k, ticks, gid = build_golden(rails)
        k.run(ticks)
    snap = k.snapshot(ticks)
    world_hash = hashlib.sha256(snap).hexdigest()

    print(f"golden={gid} ticks={ticks}")
    ac = k.aircraft[0]
    print(f"final lat={ac.lat!r} lon={ac.lon!r} psi={ac.psi!r} alt={ac.alt!r}")
    print(f"world_hash={world_hash}")

    out = args.out
    if out:
        Path(out).write_bytes(snap)
        print(f"wrote snapshot -> {out}")

    if args.seal:
        gdir = ROOT / "tests" / "golden" / gid
        gdir.mkdir(parents=True, exist_ok=True)
        (gdir / "golden_snapshot.bin").write_bytes(snap)
        (gdir / "expected.world_hash").write_text(world_hash + "\n", encoding="utf-8")
        meta = {
            "id": gid, "seal": rails["header"]["seal"], "ticks": ticks,
            "world_hash": world_hash,
            "snapshot_format": "see ref_kernel.py docstring",
            "generated_by": "tools/ref_kernel.py (Python det_math reference)",
        }
        (gdir / "golden.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
        print(f"SEALED golden under {gdir}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
