#!/usr/bin/env python3
"""
tuning_probe.py — ATM-Sphere v1.2r0 (Roster-8)
Envelope sanity checks + flight probes (math-only) for aircraft tuning.

1) Validates per-aircraft tuning JSONs (monotone TAS domains, bounds, Lipschitz guards).
2) Runs math-only flight probes using the sealed kinematics (100 Hz, R=15 km):
   * 60 s constant-bank small-circle sanity at phi=30 deg, TAS in {140,180,220} m/s.
   * Anti-meridian pass stability.
   * Near-pole pass stability.

This is *tooling* (libm allowed here); the kernel determinism bans do not apply to probes.

Usage:  python tools/tuning_probe.py data/tuning/envelopes/*.json
Exit:   0 on full PASS; 1 on any failure.
"""
import sys, json, math
from pathlib import Path

RADIUS_M = 15000.0
G0 = 9.80665
DT = 0.01
STEPS_60S = int(60.0 / DT)


def wrap_pi(x):
    x = (x + math.pi) % (2.0 * math.pi)
    return x - math.pi


def wrap_2pi(x):
    x = x % (2.0 * math.pi)
    if x < 0:
        x += 2.0 * math.pi
    return x


def great_circle_step(lat, lon, bearing, s):
    alpha = s / RADIUS_M
    sinlat1, coslat1 = math.sin(lat), math.cos(lat)
    cos_alpha, sin_alpha = math.cos(alpha), math.sin(alpha)
    sin_lat2 = sinlat1 * cos_alpha + coslat1 * sin_alpha * math.cos(bearing)
    sin_lat2 = max(-1.0, min(1.0, sin_lat2))
    lat2 = math.asin(sin_lat2)
    y = math.sin(bearing) * sin_alpha * coslat1
    x = cos_alpha - sinlat1 * math.sin(lat2)
    lon2 = wrap_pi(lon + math.atan2(y, x))
    return lat2, lon2


def sph_dist(lat1, lon1, lat2, lon2):
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    a = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(max(0.0, 1 - a)))
    return RADIUS_M * c


def sanitize_lut(name, lut):
    if not isinstance(lut, list) or len(lut) < 2:
        return [f"{name}: must have >= 2 points"]
    errs = []
    last_x = None
    for i, (x, y) in enumerate(lut):
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            errs.append(f"{name}[{i}]: non-numeric pair")
        if last_x is not None and x <= last_x:
            errs.append(f"{name}: TAS not strictly increasing at index {i} (x={x} <= {last_x})")
        last_x = x
    return errs


def lipschitz_guard(name, lut, max_delta_per10):
    errs = []
    for i in range(1, len(lut)):
        x0, y0 = lut[i - 1]
        x1, y1 = lut[i]
        dx = x1 - x0
        if dx <= 0:
            continue
        dy = abs(y1 - y0)
        if dy > max_delta_per10 * (dx / 10.0) + 1e-9:
            errs.append(f"{name}: dy={dy:.3f} too steep over dx={dx:.3f} (limit {max_delta_per10}/10 m/s)")
    return errs


def validate_envelope(doc):
    e = doc.get("tuning", {})
    errs = []
    need = ["v_ne_mps", "stall_tas_mps", "phi_max_deg_vs_tas",
            "climb_rate_max_mps_vs_tas", "climb_rate_min_mps_vs_tas", "roll_rate_degps_vs_tas"]
    for k in need:
        if k not in e:
            errs.append(f"missing {k}")
    if errs:
        return errs
    errs += sanitize_lut("phi_max_deg_vs_tas", e["phi_max_deg_vs_tas"])
    errs += sanitize_lut("climb_rate_max_mps_vs_tas", e["climb_rate_max_mps_vs_tas"])
    errs += sanitize_lut("climb_rate_min_mps_vs_tas", e["climb_rate_min_mps_vs_tas"])
    errs += sanitize_lut("roll_rate_degps_vs_tas", e["roll_rate_degps_vs_tas"])
    for x, y in e["phi_max_deg_vs_tas"]:
        if not (0.0 <= y <= 80.0):
            errs.append(f"phi_max out of bounds at TAS {x}: {y}")
    for x, y in e["climb_rate_max_mps_vs_tas"]:
        if not (-40.0 <= y <= 30.0):
            errs.append(f"climb_rate_max out of bounds at TAS {x}: {y}")
    for x, y in e["climb_rate_min_mps_vs_tas"]:
        if not (-60.0 <= y <= 0.0):
            errs.append(f"climb_rate_min out of bounds at TAS {x}: {y}")
    for x, y in e["roll_rate_degps_vs_tas"]:
        if not (30.0 <= y <= 240.0):
            errs.append(f"roll_rate out of bounds at TAS {x}: {y}")
    errs += lipschitz_guard("phi_max_deg_vs_tas", e["phi_max_deg_vs_tas"], 15.0)
    errs += lipschitz_guard("climb_rate_max_mps_vs_tas", e["climb_rate_max_mps_vs_tas"], 10.0)
    errs += lipschitz_guard("climb_rate_min_mps_vs_tas", e["climb_rate_min_mps_vs_tas"], 10.0)
    try:
        stall = float(e["stall_tas_mps"])
        vne = float(e["v_ne_mps"])
        if stall <= 0 or vne <= stall:
            errs.append("stall/v_ne invalid ordering (require 0 < stall < v_ne)")
    except Exception:
        errs.append("stall/v_ne non-numeric")
    return errs


def probe_small_circle(TAS_mps, phi_deg):
    lat, lon = 0.0, 0.0
    psi = math.radians(45.0)
    phi = math.radians(phi_deg)
    s = TAS_mps * DT
    psi_dot = G0 * math.tan(phi) / TAS_mps
    psi_expected = psi + psi_dot * (STEPS_60S * DT)
    for _ in range(STEPS_60S):
        psi = wrap_2pi(psi + psi_dot * DT)
        lat, lon = great_circle_step(lat, lon, psi, s)

    def unwrap(a, b):
        d = ((b - a + math.pi) % (2 * math.pi)) - math.pi
        return a + d

    psi_u = wrap_2pi(psi)
    psi_exp_u = unwrap(psi_u, wrap_2pi(psi_expected))
    bearing_err = abs(psi_u - psi_exp_u)
    lat2, lon2 = great_circle_step(lat, lon, psi, s)
    step_err = abs(sph_dist(lat, lon, lat2, lon2) - s)
    return bearing_err, step_err


def probe_antimeridian():
    lat, lon = 0.0, math.radians(179.999)
    psi = 0.0
    s = 200.0 * DT
    for _ in range(2000):
        lat, lon = great_circle_step(lat, lon, psi, s)
        if not (-math.pi <= lon <= math.pi):
            return False
    return True


def probe_near_pole():
    lat, lon = math.radians(89.999), 0.0
    psi = math.radians(90.0)
    s = 200.0 * DT
    for _ in range(2000):
        lat, lon = great_circle_step(lat, lon, psi, s)
        if not (-math.pi <= lon <= math.pi) or not (-math.pi / 2 <= lat <= math.pi / 2):
            return False
    return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    paths = []
    for arg in sys.argv[1:]:
        p = Path(arg)
        if any(ch in arg for ch in "*?[]"):
            paths.extend(sorted(Path().glob(arg)))
        elif p.is_dir():
            paths.extend(sorted(p.rglob("*.json")))
        else:
            paths.append(p)
    ok = True
    for p in paths:
        try:
            doc = json.loads(p.read_text(encoding="utf-8"))
        except Exception as e:
            print(f"[{p}] JSON parse error: {e}")
            ok = False
            continue
        for e in validate_envelope(doc):
            ok = False
            print(f"[{p}] {e}")
    for tas in (140.0, 180.0, 220.0):
        bearing_err, step_err = probe_small_circle(tas, 30.0)
        if bearing_err > 1e-6:
            ok = False
            print(f"[probe] bearing error too high at TAS {tas}: {bearing_err:.3e} rad")
        if step_err > 1e-6:
            ok = False
            print(f"[probe] step distance error too high at TAS {tas}: {step_err:.3e} m")
    if not probe_antimeridian():
        ok = False
        print("[probe] anti-meridian wrap failed")
    if not probe_near_pole():
        ok = False
        print("[probe] near-pole step failed")
    print("RESULT: TUNING PROBES " + ("PASS" if ok else "FAIL"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
