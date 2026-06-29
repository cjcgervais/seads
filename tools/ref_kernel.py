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
  offset 32 : per aircraft, 6 x f64 = [lat, lon, psi, phi, alt, tas]  (radians / meters / m s^-1)

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
    __slots__ = ("lat", "lon", "psi", "phi", "alt", "tas")

    def __init__(self, lat, lon, psi, phi, alt, tas):
        self.lat, self.lon, self.psi, self.phi, self.alt, self.tas = lat, lon, psi, phi, alt, tas


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
        """Envelope-driven step. commands[i] = (target_phi_rad, target_climb_mps);
        envelopes[i] = dict of radian LUTs from envelopes.load_envelope. Op order is the
        sealed byte spec and MUST match Kernel::step(cmd,env) in kernel.cpp exactly."""
        dt = self.dt
        for i, ac in enumerate(self.aircraft):
            V = ac.tas
            e = envelopes[i]
            phimax = dm.lut_eval(e["phi_max"][0], e["phi_max"][1], V)
            rollrate = dm.lut_eval(e["roll_rate"][0], e["roll_rate"][1], V)
            cmdphi = clamp(commands[i][0], -phimax, phimax)
            step_max = rollrate * dt
            delta = cmdphi - ac.phi
            delta = clamp(delta, -step_max, step_max)
            ac.phi = ac.phi + delta
            ac.phi = clamp(ac.phi, -phimax, phimax)
            climbmax = dm.lut_eval(e["climb_max"][0], e["climb_max"][1], V)
            climbmin = dm.lut_eval(e["climb_min"][0], e["climb_min"][1], V)
            req = clamp(commands[i][1], climbmin, climbmax)
            self._advance(ac, req)

    def run(self, ticks):
        for _ in range(ticks):
            self.step()

    def snapshot(self, tick_count):
        buf = bytearray()
        buf += struct.pack("<HHIddII", 1, 0, tick_count & 0xFFFFFFFF,
                           self.dt, self.R, len(self.aircraft), 0)
        for ac in self.aircraft:
            buf += struct.pack("<6d", ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas)
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
            cmds.append((deg2rad(ph["bank_deg"]), float(ph["climb_mps"])))
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
