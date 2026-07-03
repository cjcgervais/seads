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
RHO0 = 1.225
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
    errs += validate_b3_limits(e)
    errs += validate_region_toughness(e)
    errs += validate_supercharger(e)
    return errs


def _isa_sigma(alt_m):
    """ICAO ISA troposphere density ratio — the SAME law tools/gen_isa_lut.py sealed into the
    17-node LUT (test_isa proves node == law bit-for-bit), so at a 500 m grid point this IS the
    sealed node value. Tooling only (libm allowed)."""
    T0, L, RS = 288.15, 0.0065, 287.05287
    return ((T0 - L * alt_m) / T0) ** (G0 / (RS * L) - 1.0)


def validate_supercharger(e):
    """Supercharger critical altitude (v1.22r0): crit_alt_m is the altitude up to which the
    engine holds rated power (kernel lapse = min(1, sigma(alt)/sigma(crit_alt_m))). It MUST be a
    multiple of 500 inside [0, 8000] so sigma(crit_alt_m) is EXACTLY a sealed ISA-LUT node (the
    17-node table has 500 m spacing over the ATM band; lut_eval returns the node value with t=0
    at a breakpoint, so the divide's denominator is a sealed constant, not an interpolant).

    Two-speed blower schedule (v1.25r0): crit_lo_alt_m (the LOW/MS gear's full-throttle height)
    and gear2_frac (the HIGH/FS gear's rated-power fraction) extend the lapse to
    max(min(1, sigma/sigma(crit_lo)), gear2_frac*min(1, sigma/sigma(crit_alt))). Either
    crit_lo_alt_m = 0 AND gear2_frac = 1 (single-speed — the v1.22r0 path bit-for-bit), or:
    crit_lo_alt_m a multiple of 500 strictly inside (0, crit_alt_m) (a sealed LUT node), and
    gear2_frac a multiple of 1/16 with sigma(crit_alt)/sigma(crit_lo) < gear2_frac < 1 — the
    lower bound makes the HIGH gear win somewhere below crit_alt (a real gear shift exists),
    the upper bound makes the LOW gear win at sea level (the low gear is not vestigial)."""
    if "crit_alt_m" not in e:
        return ["missing crit_alt_m"]
    try:
        v = float(e["crit_alt_m"])
    except Exception:
        return ["crit_alt_m non-numeric"]
    errs = []
    if not (0.0 <= v <= 8000.0):
        errs.append(f"crit_alt_m out of the ATM band: {v} (expect 0 <= crit <= 8000)")
    if v / 500.0 != round(v / 500.0):
        errs.append(f"crit_alt_m not a multiple of 500: {v} (sigma(crit) must be a sealed LUT node)")
    for k in ("crit_lo_alt_m", "gear2_frac"):
        if k not in e:
            errs.append(f"missing {k}")
    if errs:
        return errs
    try:
        lo = float(e["crit_lo_alt_m"])
        g2 = float(e["gear2_frac"])
    except Exception:
        return ["crit_lo_alt_m/gear2_frac non-numeric"]
    if lo == 0.0:
        if g2 != 1.0:
            errs.append(f"gear2_frac must be exactly 1 when crit_lo_alt_m = 0 (single-speed): {g2}")
        return errs
    if lo / 500.0 != round(lo / 500.0):
        errs.append(f"crit_lo_alt_m not a multiple of 500: {lo} (sigma(crit_lo) must be a sealed LUT node)")
    if not (0.0 < lo < v):
        errs.append(f"crit_lo_alt_m must sit strictly inside (0, crit_alt_m): lo={lo} crit={v}")
    if g2 * 16.0 != round(g2 * 16.0):
        errs.append(f"gear2_frac not a multiple of 1/16: {g2}")
    if not errs:
        floor = _isa_sigma(v) / _isa_sigma(lo)
        if not (floor < g2 < 1.0):
            errs.append(f"gear2_frac degenerate: {g2} (need sigma(crit)/sigma(crit_lo)={floor:.4f} < gear2_frac < 1)")
    return errs


def validate_region_toughness(e):
    """Region toughness (v1.20r0): per-airframe engine_frac/wing_frac/tail_frac size the
    ENGINE/WING/TAIL sub-pools as fractions of hp_start. Each must be a positive multiple of 1/8
    and <= 1 — eighth-granularity keeps every pool value exact in f64 AND milli-exact on the
    WEAPON-001 wire (1000/8 = 125 clears the denominator for any integer hp_start, and integer
    per-round damage preserves the granularity as pools drain)."""
    errs = []
    for k in ("engine_frac", "wing_frac", "tail_frac"):
        if k not in e:
            errs.append(f"missing {k}")
    if errs:
        return errs
    for k in ("engine_frac", "wing_frac", "tail_frac"):
        try:
            v = float(e[k])
        except Exception:
            errs.append(f"{k} non-numeric")
            continue
        if not (0.0 < v <= 1.0):
            errs.append(f"{k} out of bounds: {v} (expect 0 < frac <= 1)")
        if v * 8.0 != round(v * 8.0):
            errs.append(f"{k} not a multiple of 1/8: {v} (pools must stay f64- and milli-exact)")
    try:
        hp = float(e["hp_start"])
        if hp != round(hp):
            errs.append(f"hp_start must be an integer for exact region pools, got {hp}")
    except Exception:
        errs.append("hp_start missing/non-numeric (region pools derive from it)")
    return errs


def validate_b3_limits(e):
    """B3 (v1.7r0) V-n limit params: cl_max + structural g limits. Checks bounds, ordering, and
    that the model is internally consistent — the C_Lmax-implied 1 g stall speed matches the
    declared stall_tas_mps, and the corner speed stall*sqrt(n_max_struct) stays below v_ne (so the
    aircraft can actually reach full structural g before overspeeding)."""
    errs = []
    for k in ("cl_max", "n_max_struct", "n_min_struct"):
        if k not in e:
            errs.append(f"missing {k}")
    if errs:
        return errs
    try:
        cl_max = float(e["cl_max"]); nmx = float(e["n_max_struct"]); nmn = float(e["n_min_struct"])
        mass = float(e["mass_kg"]); S = float(e["wing_area_m2"])
        stall = float(e["stall_tas_mps"]); vne = float(e["v_ne_mps"])
    except Exception:
        return ["B3 limit params non-numeric"]
    if not (0.8 <= cl_max <= 2.0):
        errs.append(f"cl_max out of bounds: {cl_max} (expect 0.8..2.0)")
    if not (4.0 <= nmx <= 12.0):
        errs.append(f"n_max_struct out of bounds: {nmx} (expect 4..12 g)")
    if not (-6.0 <= nmn < 0.0):
        errs.append(f"n_min_struct out of bounds: {nmn} (expect -6..0 g)")
    # internal consistency: cl_max-implied 1 g stall speed == declared stall_tas_mps
    v_stall_1g = math.sqrt(2.0 * mass * G0 / (RHO0 * S * cl_max))
    if abs(v_stall_1g - stall) > 0.5:
        errs.append(f"cl_max implies 1g stall {v_stall_1g:.2f} m/s, declared stall_tas_mps={stall} "
                    f"(incoherent; |diff| {abs(v_stall_1g - stall):.2f} > 0.5)")
    # corner speed must be reachable below v_ne
    corner = stall * math.sqrt(nmx)
    if corner >= vne:
        errs.append(f"corner speed {corner:.1f} >= v_ne {vne} (cannot pull full structural g)")
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
