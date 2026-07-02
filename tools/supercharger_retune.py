#!/usr/bin/env python3
"""
supercharger_retune.py — ATM-Sphere supercharger critical altitude (data retune solver, v1.22r0)

B5 (v1.21r0) scaled engine power with raw density (T *= sigma) — the conservative choice that
kept every airframe at-or-under its B4 top speed everywhere, and named per-airframe supercharger
critical-altitude modeling as the data-driven follow-up. This tool is that follow-up's solver.

The kernel model (see ADR-Step8-FlightModel-Supercharger-v1.22r0):

    lapse = min(1, sigma(alt) / sigma(crit_alt_m))     # one comparison + one divide
    T     = thr * thrust_static_n * (1 - V/v_max_mps) * lapse

Below the critical altitude the supercharger holds RATED power (lapse = 1); above it power falls
with the density ratio. crit_alt_m is a per-airframe envelope scalar, constrained to a multiple
of 500 m inside [0, 8000] (tuning_probe-enforced) so sigma(crit) is EXACTLY a sealed ISA-LUT
node. crit_alt_m = 0 gives sigma_crit = 1.0 and reproduces the B5 thrust bit-for-bit.

Holding rated power to altitude on the B4 numbers would be unhistorical: B4 anchored the
full-throttle level equilibrium at SEA LEVEL to the HISTORICAL top speed (which real airframes
achieved at their critical altitude, not at sea level). With drag falling as sigma and the
near-flat B4 thrust curve, an un-re-anchored supercharger tops out ~1/sqrt(sigma_crit) hot at
the critical altitude (a ~965 km/h P-51). So this retune RE-ANCHORS the two B4 speed knobs per
airframe, keeping every turn/stall-defining parameter frozen exactly as B4 did:

  Eq1 (top speed):  T0*(1 - V_top/Vmax) = D(V_top, n=1, sigma_crit)   [at the CRITICAL altitude]
  Eq2 (best climb): max_V V*(T(V) - D(V, 1, sigma=1))/(m*g0) = climb  [at SEA LEVEL, lapse = 1]

Same bisection as b4_retune.py (raising Vmax flattens the curve and monotonically lowers
sea-level climb). The emergent result: top TAS now RISES with altitude to a per-airframe peak of
exactly the historical value at crit_alt_m, then falls — the historically correct shape (B5's
was flat-to-falling). Sea-level top speeds land BELOW the historical at-altitude figures
(the flat linear thrust curve overshoots the real-world ~sigma^(-1/3) speed gain, so they run
0-20% low — documented in the ADR; the at-crit anchor is exact).

Targets are the B4 historical (top speed, sea-level climb) pairs, unchanged. v_ne_mps is
untouched (B4's 1.22 dive margin above the SAME top-speed target).

This is *tooling* (libm allowed). Default prints an old->new comparison; --write updates the
JSONs in place (inserting crit_alt_m and bumping the envelope headers to v1.22r0).

Usage:  python tools/supercharger_retune.py            # dry run
        python tools/supercharger_retune.py --write    # rewrite data/tuning/envelopes/*.json
"""
import sys, json, math, re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import envelopes as envmod
import perf_probe as pp
import ref_kernel as rk

ROOT = Path(__file__).resolve().parent.parent
ENV_DIR = ROOT / "data" / "tuning" / "envelopes"

G0 = pp.G0
RHO0 = pp.RHO0

# Historical (top speed m/s, sea-level best climb m/s) — the B4 targets, verbatim. The top speed
# is now anchored AT the critical altitude (where history measured it) instead of at sea level.
TARGETS = {
    "p51":          (195.0, 19.0),   # P-51D  ~703 km/h
    "p47d":         (191.0, 14.0),   # P-47D  ~688 km/h
    "la7":          (184.0, 22.0),   # La-7   ~662 km/h
    "yak3":         (182.0, 21.0),   # Yak-3  ~655 km/h
    "bf109f4":      (176.0, 20.0),   # Bf109F ~634 km/h
    "spitfire_mk5": (167.0, 16.0),   # Spit V ~601 km/h
    "ki61":         (164.0, 14.0),   # Ki-61  ~590 km/h
    "a6m2":         (148.0, 16.0),   # A6M2   ~533 km/h
}

# Per-airframe critical altitude (m). CONTRACT: a multiple of 500 inside [0, 8000] so
# sigma(crit) is exactly a sealed ISA-LUT node (tuning_probe.validate_supercharger).
# Historical provenance (rated / full-throttle heights, rounded to the 500 m grid):
CRIT_ALT = {
    "p47d":         8000.0,  # turbosupercharged R-2800 held rated power past the ATM band top
    "p51":          7500.0,  # two-stage Packard Merlin V-1650, high-blower FTH ~7.4 km
    "bf109f4":      6000.0,  # DB 601E rated altitude ~6.2 km
    "spitfire_mk5": 6000.0,  # Merlin 45 FTH ~5.9 km
    "la7":          4500.0,  # ASh-82FN second-speed FTH ~4.65 km
    "a6m2":         4500.0,  # Sakae 12 rated ~4.55 km
    "ki61":         4000.0,  # Ha-40 rated ~4.2 km
    "yak3":         3000.0,  # VK-105PF2 — the low-altitude brawler of the roster
}


def load_tuning(name):
    """Raw scalar tuning dict straight from the JSON — tolerant of a missing crit_alt_m
    (envelopes.load_envelope requires it, which this tool is the one inserting)."""
    t = json.loads((ENV_DIR / f"{name}.json").read_text(encoding="utf-8"))["tuning"]
    keys = ("mass_kg", "wing_area_m2", "cd0", "induced_k", "thrust_static_n", "v_max_mps",
            "cl_max", "n_max_struct", "n_min_struct")
    e = {k: float(t[k]) for k in keys}
    e["crit_alt_m"] = float(t.get("crit_alt_m", 0.0))
    return e


def drag_at(e, V, n, sigma):
    """Level drag at density ratio sigma: parasitic scales *sigma, induced /sigma (fixed n)."""
    qS = 0.5 * RHO0 * sigma * V * V * e["wing_area_m2"]
    CL = (n * e["mass_kg"] * G0) / qS
    return qS * e["cd0"] + e["induced_k"] * CL * CL * qS


def lapse(e, alt):
    return min(1.0, rk.air_sigma(alt) / rk.air_sigma(e["crit_alt_m"]))


def thrust_at(e, V, alt):
    return pp.thrust(e, V) * lapse(e, alt)


def v_top_at(e, alt):
    """Emergent full-throttle level top speed at altitude (supercharger lapse + sigma drag)."""
    sigma = rk.air_sigma(alt)

    def excess(V):
        return thrust_at(e, V, alt) - drag_at(e, V, 1.0, sigma)
    lo = max(pp.v_stall_1g(e) / math.sqrt(sigma), pp.V_MIN)
    hi = e["v_max_mps"]
    if excess(lo) <= 0.0:
        return lo
    if excess(hi) > 0.0:
        return hi
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        if excess(mid) > 0.0:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def solve(e, v_top_t, climb_t, sig_crit):
    """Solve (T0, Vmax): Eq1 anchors the top speed at the critical altitude (lapse = 1 there,
    drag at sigma_crit); Eq2 is the B4 sea-level best-climb bisection (lapse = 1 there too)."""
    d_top = drag_at(e, v_top_t, 1.0, sig_crit)

    def with_vmax(vmax):
        t0 = d_top / (1.0 - v_top_t / vmax)
        e2 = dict(e); e2["thrust_static_n"] = t0; e2["v_max_mps"] = vmax
        return t0, pp.climb_best(e2)[0]

    lo = v_top_t * 1.05
    hi = min(v_top_t * 8.0, 700.0)
    _, climb_lo = with_vmax(lo)
    _, climb_hi = with_vmax(hi)
    if climb_t >= climb_lo:
        t0, c = with_vmax(lo);  return t0, lo, c
    if climb_t <= climb_hi:
        t0, c = with_vmax(hi);  return t0, hi, c
    for _ in range(100):
        mid = 0.5 * (lo + hi)
        _, c = with_vmax(mid)
        if c > climb_t:
            lo = mid
        else:
            hi = mid
    vmax = 0.5 * (lo + hi)
    t0, c = with_vmax(vmax)
    return t0, vmax, c


def rounded(t0, vmax):
    return round(t0, -1), round(vmax / 5.0) * 5.0   # thrust to 10 N, vmax to nearest 5 m/s


def main():
    write = "--write" in sys.argv[1:]
    print(f"{'aircraft':<12} {'crit':>5} {'T0_old':>8} {'T0_new':>8} {'Vmax_old':>8} {'Vmax_new':>8} "
          f"{'topSL':>6} {'top@crit':>8} {'km/h':>6} {'climbSL':>7}")
    print("-" * 88)
    updates = {}
    for name, (v_top_t, climb_t) in TARGETS.items():
        e = load_tuning(name)
        crit = CRIT_ALT[name]
        sig_crit = rk.air_sigma(crit)
        t0, vmax, _ = solve(e, v_top_t, climb_t, sig_crit)
        t0, vmax = rounded(t0, vmax)
        e2 = dict(e); e2["thrust_static_n"] = t0; e2["v_max_mps"] = vmax; e2["crit_alt_m"] = crit
        top_sl = v_top_at(e2, 0.0)
        top_crit = v_top_at(e2, crit)
        climb_sl = pp.climb_best(e2)[0]
        updates[name] = {"thrust_static_n": t0, "v_max_mps": vmax, "crit_alt_m": crit}
        print(f"{pp.NAMES[name]:<12} {crit:>5.0f} {e['thrust_static_n']:>8.0f} {t0:>8.0f} "
              f"{e['v_max_mps']:>8.1f} {vmax:>8.1f} {top_sl:>6.1f} {top_crit:>8.1f} "
              f"{top_crit*3.6:>6.0f} {climb_sl:>7.1f}")
    if not write:
        print("\n(dry run — pass --write to update the envelope JSONs)")
        return
    for name, upd in updates.items():
        path = ENV_DIR / f"{name}.json"
        txt = path.read_text(encoding="utf-8")

        def setnum(text, key, val):
            pat = re.compile(r'("' + re.escape(key) + r'"\s*:\s*)-?\d+(?:\.\d+)?')
            new, n = pat.subn(lambda m: m.group(1) + repr(float(val)), text)
            if n != 1:
                raise SystemExit(f"{path.name}: expected 1 '{key}', found {n}")
            return new

        txt = setnum(txt, "thrust_static_n", upd["thrust_static_n"])
        txt = setnum(txt, "v_max_mps", upd["v_max_mps"])
        if '"crit_alt_m"' in txt:
            txt = setnum(txt, "crit_alt_m", upd["crit_alt_m"])
        else:
            # insert after tail_frac (the last v1.20r0 field), preserving hand-formatting
            pat = re.compile(r'("tail_frac"\s*:\s*-?\d+(?:\.\d+)?)')
            new, n = pat.subn(lambda m: m.group(1) + ',\n    "crit_alt_m": '
                              + repr(float(upd["crit_alt_m"])), txt)
            if n != 1:
                raise SystemExit(f"{path.name}: expected 1 'tail_frac', found {n}")
            txt = new
        txt = re.sub(r'("version"\s*:\s*)\d+', r'\g<1>220', txt, count=1)
        txt = re.sub(r'("seal"\s*:\s*")ATM-Sphere v\d+\.\d+r\d+(")',
                     r'\g<1>ATM-Sphere v1.22r0\2', txt, count=1)
        txt = re.sub(r'("adr"\s*:\s*")[^"]*(")',
                     r'\g<1>docs/adr/ADR-Step8-FlightModel-Supercharger-v1.22r0.md\2',
                     txt, count=1)
        path.write_text(txt, encoding="utf-8")
        print(f"wrote {path.name}")


if __name__ == "__main__":
    main()
