#!/usr/bin/env python3
"""
b4_retune.py — ATM-Sphere flight-model B4 (per-airframe aero retune solver)

Pre-B4 the roster's emergent level top speeds ran ~30% low (109-142 m/s) and compressed; the turn
ordering, however, already read true (A6M2 / Spitfire best, P-47 worst). B4 lifts top speeds to
HISTORICAL values (~148-195 m/s) while PRESERVING that turn balance.

Design (see ADR-Step8-FlightModel-B4): keep every turn/stall-determining parameter FIXED
(mass_kg, wing_area_m2, cl_max, induced_k, n_max_struct, n_min_struct, stall_tas_mps and all four
LUTs) so the instantaneous-turn ordering and the B3 cl_max<->stall coherence are untouched. The only
kernel-affecting knobs that move are **thrust_static_n** and **v_max_mps**, solved per airframe to
hit a (top-speed, best-climb) target pair:

  Eq1 (top speed):  T0*(1 - V_top/Vmax) = D(V_top, n=1)         [full-throttle level equilibrium]
  Eq2 (best climb): max_V  V*(T(V)-D(V,1))/(m*g0)  = climb_tgt  [specific excess power]

For a fixed Vmax, Eq1 fixes T0 = D(V_top)/(1 - V_top/Vmax). Raising Vmax flattens the thrust curve,
which (with T0 re-anchored to Eq1) lowers low-speed thrust and hence best climb monotonically, so we
bisect Vmax to hit climb_tgt. If climb_tgt is below the Vmax->infinity floor it is infeasible for that
top speed; we clamp to the floor and report the achieved climb (the cost of historical top speed).

v_ne_mps is raised to give a dive margin above the new top speed; it is validation-only (the kernel
never loads it), so it does NOT move any golden.

This is *tooling* (libm allowed). Default prints an old->new comparison; --write updates the JSONs.

Usage:  python tools/b4_retune.py            # dry run: print solved params + resulting performance
        python tools/b4_retune.py --write    # rewrite data/tuning/envelopes/*.json in place
"""
import sys, json, math, re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import envelopes as envmod
import perf_probe as pp

ROOT = Path(__file__).resolve().parent.parent
ENV_DIR = ROOT / "data" / "tuning" / "envelopes"

G0 = pp.G0
RHO0 = pp.RHO0

# Historical level top speed (m/s) and best sea-level climb (m/s) targets. Top speeds set the
# energy-fighter spread the owner asked for (~148-195 m/s); climb targets are historical sea-level
# rates (clamped up to the model floor where a high top speed forbids a low climb).
TARGETS = {
    "p51":          (195.0, 19.0),   # P-51D  ~703 km/h
    "p47d":         (191.0, 14.0),   # P-47D  ~688 km/h (heavy; climbs worst, dives best)
    "la7":          (184.0, 22.0),   # La-7   ~662 km/h
    "yak3":         (182.0, 21.0),   # Yak-3  ~655 km/h
    "bf109f4":      (176.0, 20.0),   # Bf109F ~634 km/h
    "spitfire_mk5": (167.0, 16.0),   # Spit V ~601 km/h
    "ki61":         (164.0, 14.0),   # Ki-61  ~590 km/h
    "a6m2":         (148.0, 16.0),   # A6M2   ~533 km/h (slowest, best turn)
}
DIVE_MARGIN = 1.22   # v_ne = round(V_top * DIVE_MARGIN); validation-only, never reaches the kernel


def best_climb(e):
    return pp.climb_best(e)[0]


def solve(e, v_top, climb_tgt):
    """Solve (T0, Vmax) keeping all other aero params fixed. Returns (T0, Vmax, achieved_climb)."""
    d_top = pp.drag(e, v_top, 1.0)

    def with_vmax(vmax):
        t0 = d_top / (1.0 - v_top / vmax)
        e2 = dict(e); e2["thrust_static_n"] = t0; e2["v_max_mps"] = vmax
        return t0, best_climb(e2)

    lo = v_top * 1.05          # steep curve  -> high low-speed thrust -> high climb
    # Vmax is the linear-thrust zero-crossing, NOT a physical Vne (real top speed is emergent and
    # well below it). Cap it at a clean 700 m/s: high enough that the curve is near-flat (so climb
    # stays historical, ~18-23 m/s) without the absurd 4-digit asymptotes an uncapped solve produces.
    hi = min(v_top * 8.0, 700.0)   # flat curve -> climb near the (capped) floor
    _, climb_lo = with_vmax(lo)
    _, climb_hi = with_vmax(hi)
    if climb_tgt >= climb_lo:         # asked for more climb than the steepest curve gives
        t0, c = with_vmax(lo);  return t0, lo, c
    if climb_tgt <= climb_hi:         # below the flat-curve floor: infeasible, take the floor
        t0, c = with_vmax(hi);  return t0, hi, c
    for _ in range(100):
        mid = 0.5 * (lo + hi)
        _, c = with_vmax(mid)
        if c > climb_tgt:
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
    print(f"{'aircraft':<12} {'T0_old':>8} {'T0_new':>8} {'Vmax_old':>8} {'Vmax_new':>8} "
          f"{'top_old':>7} {'top_new':>7} {'km/h':>6} {'climb':>6} {'v_ne':>6}")
    print("-" * 86)
    updates = {}
    for name, (v_top_t, climb_t) in TARGETS.items():
        e = envmod.load_envelope(name)
        top_old = pp.v_top(e)
        t0, vmax, _ = solve(e, v_top_t, climb_t)
        t0, vmax = rounded(t0, vmax)
        e2 = dict(e); e2["thrust_static_n"] = t0; e2["v_max_mps"] = vmax
        top_new = pp.v_top(e2)
        climb_new = best_climb(e2)
        v_ne = round(v_top_t * DIVE_MARGIN)
        updates[name] = {"thrust_static_n": t0, "v_max_mps": vmax, "v_ne_mps": float(v_ne)}
        print(f"{pp.NAMES[name]:<12} {e['thrust_static_n']:>8.0f} {t0:>8.0f} "
              f"{e['v_max_mps']:>8.1f} {vmax:>8.1f} {top_old:>7.1f} {top_new:>7.1f} "
              f"{top_new*3.6:>6.0f} {climb_new:>6.1f} {v_ne:>6}")
    if not write:
        print("\n(dry run — pass --write to update the envelope JSONs)")
        return
    for name, upd in updates.items():
        path = ENV_DIR / f"{name}.json"
        txt = path.read_text(encoding="utf-8")

        def setnum(text, key, val):
            # replace the numeric literal after "key": preserving surrounding formatting
            pat = re.compile(r'("' + re.escape(key) + r'"\s*:\s*)-?\d+(?:\.\d+)?')
            new, n = pat.subn(lambda m: m.group(1) + repr(float(val)), text)
            if n != 1:
                raise SystemExit(f"{path.name}: expected 1 '{key}', found {n}")
            return new

        txt = setnum(txt, "v_ne_mps", upd["v_ne_mps"])
        txt = setnum(txt, "thrust_static_n", upd["thrust_static_n"])
        txt = setnum(txt, "v_max_mps", upd["v_max_mps"])
        # honest provenance: the envelope headers still read v1.2r0 (never bumped through B1-B3);
        # this data reseal sets them to v1.8r0 and points at the B4 ADR. Header fields are NOT loaded
        # by envelopes.py (only the "tuning" block), so this does NOT move any golden.
        txt = re.sub(r'("version"\s*:\s*)\d+', r'\g<1>180', txt, count=1)
        txt = re.sub(r'("seal"\s*:\s*")ATM-Sphere v\d+\.\d+r\d+(")',
                     r'\g<1>ATM-Sphere v1.8r0\2', txt, count=1)
        txt = re.sub(r'("adr"\s*:\s*")[^"]*(")',
                     r'\g<1>docs/adr/ADR-Step8-FlightModel-B4-v1.8r0.md\2', txt, count=1)
        path.write_text(txt, encoding="utf-8")
        print(f"wrote {path.name}")


if __name__ == "__main__":
    main()
