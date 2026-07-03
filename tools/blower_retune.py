#!/usr/bin/env python3
"""
blower_retune.py — ATM-Sphere two-speed blower schedule (data retune solver, v1.25r0)

v1.22r0 modeled the supercharger as a SINGLE critical altitude: rated power below crit_alt_m,
falling with the density ratio above it. Real WWII engines with TWO blower gears have a richer
profile: rated power in LOW (MS) gear to its full-throttle height, falling until the pilot shifts
to HIGH (FS) gear, FLAT at the high gear's (slightly lower — the taller gear costs shaft power)
rating to crit_alt_m, then falling. This tool is the v1.22r0-deferred follow-up's solver.

The kernel model (see ADR-Step8-FlightModel-TwoSpeedBlower-v1.25r0):

    lapse = max( min(1, sigma(alt)/sigma(crit_lo_alt_m)),                # LOW gear
                 gear2_frac * min(1, sigma(alt)/sigma(crit_alt_m)) )     # HIGH gear
    T     = thr * thrust_static_n * (1 - V/v_max_mps) * lapse

crit_lo_alt_m = 0 (gear2_frac = 1) means single-speed: the kernel never enters the two-speed
branch and reproduces the v1.22r0 lapse bit-for-bit. CONTRACTS (tuning_probe): crit_lo_alt_m a
multiple of 500 strictly inside (0, crit_alt_m) — a sealed ISA-LUT node; gear2_frac a multiple
of 1/16 with sigma(crit_alt)/sigma(crit_lo) < gear2_frac < 1 (both gears non-degenerate).

RE-ANCHORING: v1.22r0's Eq1 anchored the historical top speed at crit_alt_m with lapse = 1
there. Under the two-speed schedule the lapse at crit_alt_m is gear2_frac < 1, so holding the
v1.22r0 knobs would drop the at-crit top below history. This tool re-solves the two B4 speed
knobs for the two-speed airframes only:

  Eq1 (top speed):  gear2_frac * T0 * (1 - V_top/Vmax) = D(V_top, n=1, sigma_crit)   [at crit]
  Eq2 (best climb): max_V V*(T(V) - D(V, 1, sigma=1))/(m*g0) = climb    [sea level, lapse = 1]

Same bisection as supercharger_retune.py. Emergent: sea-level RATED power (low gear) rises by
~1/gear2_frac over v1.22r0 — the physical story (the low gear wastes less power down low), while
the at-crit anchor stays exactly the B4 historical top speed.

Sealed two-speed roster (WWII gear ratings, rounded to the 500 m grid / sixteenths):
  p51   crit_lo 3000, gear2 14/16  — V-1650-7 two-speed two-stage Packard Merlin
  la7   crit_lo 1500, gear2 14/16  — ASh-82FN two-speed ~1550/4650 m ratings
  yak3  crit_lo 1000, gear2 15/16  — VK-105PF-2 two-speed ~700/2700 m ratings
Single-speed (crit_lo 0, gear2 1): p47d (turbo — flat to the band top), bf109f4 + ki61
(DB 601-family hydraulic variable-speed couplings — no gear kink), spitfire_mk5 (Merlin 45,
single-speed), a6m2 (Sakae 12, single-speed).

This is *tooling* (libm allowed). Default prints an old->new comparison; --write updates the
JSONs in place (inserting crit_lo_alt_m/gear2_frac in all eight, re-anchoring T0/Vmax for the
three two-speed airframes, bumping every envelope header to v1.25r0).

Usage:  python tools/blower_retune.py            # dry run
        python tools/blower_retune.py --write    # rewrite data/tuning/envelopes/*.json
"""
import sys, json, math, re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import perf_probe as pp
import ref_kernel as rk
import supercharger_retune as sr

ROOT = Path(__file__).resolve().parent.parent
ENV_DIR = ROOT / "data" / "tuning" / "envelopes"

G0 = pp.G0

# Per-airframe (crit_lo_alt_m, gear2_frac). CONTRACT: crit_lo a 500 m multiple in (0, crit_alt),
# gear2 a 1/16 multiple with sigma(crit)/sigma(crit_lo) < gear2 < 1 (tuning_probe-enforced).
BLOWER = {
    "p51":          (3000.0, 14.0 / 16.0),
    "la7":          (1500.0, 14.0 / 16.0),
    "yak3":         (1000.0, 15.0 / 16.0),
    "p47d":         (0.0, 1.0),
    "bf109f4":      (0.0, 1.0),
    "ki61":         (0.0, 1.0),
    "spitfire_mk5": (0.0, 1.0),
    "a6m2":         (0.0, 1.0),
}


def lapse2(e, alt):
    """Two-speed thrust lapse (the v1.25r0 kernel model)."""
    sig = rk.air_sigma(alt)
    lo = min(1.0, sig / rk.air_sigma(e["crit_lo_alt_m"])) if e["crit_lo_alt_m"] > 0.0 else 0.0
    hi = e["gear2_frac"] * min(1.0, sig / rk.air_sigma(e["crit_alt_m"]))
    if e["crit_lo_alt_m"] <= 0.0:
        return min(1.0, sig / rk.air_sigma(e["crit_alt_m"]))
    return max(lo, hi)


def v_top_at(e, alt):
    """Emergent full-throttle level top speed at altitude under the two-speed lapse."""
    sigma = rk.air_sigma(alt)

    def excess(V):
        return pp.thrust(e, V) * lapse2(e, alt) - sr.drag_at(e, V, 1.0, sigma)
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


def solve(e, v_top_t, climb_t, sig_crit, g2):
    """Solve (T0, Vmax): Eq1 anchors the top speed at crit_alt_m where the lapse is gear2_frac
    (the HIGH gear's rating); Eq2 is the B4 sea-level best-climb bisection (lapse = 1 — LOW
    gear rated)."""
    d_top = sr.drag_at(e, v_top_t, 1.0, sig_crit)

    def with_vmax(vmax):
        t0 = d_top / (1.0 - v_top_t / vmax) / g2
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


def shift_alt(e):
    """Gear-shift altitude: where the LOW branch falls to the HIGH gear's flat rating
    (sigma(alt) = gear2_frac * sigma(crit_lo)) — scan on a 1 m grid for display only."""
    target = e["gear2_frac"] * rk.air_sigma(e["crit_lo_alt_m"])
    a = e["crit_lo_alt_m"]
    while a < e["crit_alt_m"] and rk.air_sigma(a) > target:
        a += 1.0
    return a


def main():
    write = "--write" in sys.argv[1:]
    print(f"{'aircraft':<12} {'lo':>5} {'g2':>6} {'crit':>5} {'T0_old':>8} {'T0_new':>8} "
          f"{'Vmax_old':>8} {'Vmax_new':>8} {'shift':>6} {'topSL':>6} {'top@crit':>8} {'km/h':>6} {'climbSL':>7}")
    print("-" * 116)
    updates = {}
    for name, (v_top_t, climb_t) in sr.TARGETS.items():
        e = sr.load_tuning(name)
        lo_alt, g2 = BLOWER[name]
        crit = sr.CRIT_ALT[name]
        e["crit_lo_alt_m"], e["gear2_frac"] = lo_alt, g2
        if lo_alt > 0.0:
            sig_crit = rk.air_sigma(crit)
            t0, vmax, _ = solve(e, v_top_t, climb_t, sig_crit, g2)
            t0, vmax = sr.rounded(t0, vmax)
        else:
            t0, vmax = e["thrust_static_n"], e["v_max_mps"]   # single-speed: knobs UNTOUCHED
        e2 = dict(e); e2["thrust_static_n"] = t0; e2["v_max_mps"] = vmax
        top_sl = v_top_at(e2, 0.0)
        top_crit = v_top_at(e2, crit)
        climb_sl = pp.climb_best(e2)[0]
        sh = shift_alt(e2) if lo_alt > 0.0 else 0.0
        updates[name] = {"thrust_static_n": t0, "v_max_mps": vmax,
                         "crit_lo_alt_m": lo_alt, "gear2_frac": g2}
        print(f"{pp.NAMES[name]:<12} {lo_alt:>5.0f} {g2:>6.4f} {crit:>5.0f} "
              f"{e['thrust_static_n']:>8.0f} {t0:>8.0f} {e['v_max_mps']:>8.1f} {vmax:>8.1f} "
              f"{sh:>6.0f} {top_sl:>6.1f} {top_crit:>8.1f} {top_crit*3.6:>6.0f} {climb_sl:>7.1f}")
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

        if BLOWER[name][0] > 0.0:
            txt = setnum(txt, "thrust_static_n", upd["thrust_static_n"])
            txt = setnum(txt, "v_max_mps", upd["v_max_mps"])
        if '"crit_lo_alt_m"' in txt:
            txt = setnum(txt, "crit_lo_alt_m", upd["crit_lo_alt_m"])
            txt = setnum(txt, "gear2_frac", upd["gear2_frac"])
        else:
            # insert after crit_alt_m (the last v1.22r0 field), preserving hand-formatting
            pat = re.compile(r'("crit_alt_m"\s*:\s*-?\d+(?:\.\d+)?)')
            new, n = pat.subn(lambda m: m.group(1) + ',\n    "crit_lo_alt_m": '
                              + repr(float(upd["crit_lo_alt_m"])) + ',\n    "gear2_frac": '
                              + repr(float(upd["gear2_frac"])), txt)
            if n != 1:
                raise SystemExit(f"{path.name}: expected 1 'crit_alt_m', found {n}")
            txt = new
        txt = re.sub(r'("version"\s*:\s*)\d+', r'\g<1>250', txt, count=1)
        txt = re.sub(r'("seal"\s*:\s*")ATM-Sphere v\d+\.\d+r\d+(")',
                     r'\g<1>ATM-Sphere v1.25r0\2', txt, count=1)
        txt = re.sub(r'("adr"\s*:\s*")[^"]*(")',
                     r'\g<1>docs/adr/ADR-Step8-FlightModel-TwoSpeedBlower-v1.25r0.md\2',
                     txt, count=1)
        path.write_text(txt, encoding="utf-8")
        print(f"wrote {path.name}")


if __name__ == "__main__":
    main()
