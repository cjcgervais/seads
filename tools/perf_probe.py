#!/usr/bin/env python3
"""
perf_probe.py — ATM-Sphere flight-model B4 (emergent-performance analyzer)

Computes, from the EXACT sealed kernel aero model (B1/B2/B3 — see tools/ref_kernel.py
step_scenario), the *emergent* steady-state performance of each aircraft envelope:

  * v_stall_1g   1 g stall speed     V_s   = sqrt(2*m*g0 / (rho0*S*cl_max))
  * corner       corner speed        V*    = V_s * sqrt(n_max_struct)
  * v_top        level top speed     (full throttle, n=1, gamma=0): root of T(V)=D(V)
  * climb_best   best climb rate     max_V Ps(V),  Ps = V*(T-D)/(m*g0)  at n=1   [m/s]
  * turn_sust    best sustained turn (energy-neutral, full throttle): max_V g0*sqrt(n^2-1)/V
                 with n the largest load factor where thrust still balances drag at V,
                 capped by the C_Lmax aero ceiling and the structural g limit       [deg/s]
  * turn_inst    instantaneous turn  at the corner: g0*sqrt(n_corner^2-1)/V*         [deg/s]

This is *tooling* (libm allowed; the kernel determinism bans do not apply to probes) and is a
read-only analysis of the same constants the kernel integrates — it does NOT feed the sim.

Two modes:
  python tools/perf_probe.py                 # print the roster performance table
  python tools/perf_probe.py --check         # assert the B4 relative-ordering targets (gate)

Exit: 0 on PASS; 1 on any failed assertion (--check) or load error.
"""
import sys, math
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import envelopes as envmod

G0 = 9.80665
RHO0 = 1.225
V_MIN = 30.0

ROSTER = ["p47d", "bf109f4", "ki61", "a6m2", "yak3", "la7", "spitfire_mk5", "p51"]
NAMES = {
    "p47d": "P-47D", "bf109f4": "Bf 109 F-4", "ki61": "Ki-61", "a6m2": "A6M2 Zero",
    "yak3": "Yak-3", "la7": "La-7", "spitfire_mk5": "Spitfire V", "p51": "P-51",
}


def thrust(e, V):
    T = e["thrust_static_n"] * (1.0 - V / e["v_max_mps"])
    return T if T > 0.0 else 0.0


def drag(e, V, n):
    qS = 0.5 * RHO0 * V * V * e["wing_area_m2"]
    CL = (n * e["mass_kg"] * G0) / qS
    return qS * e["cd0"] + e["induced_k"] * CL * CL * qS


def v_stall_1g(e):
    return math.sqrt(2.0 * e["mass_kg"] * G0 / (RHO0 * e["wing_area_m2"] * e["cl_max"]))


def n_aero(e, V):
    qS = 0.5 * RHO0 * V * V * e["wing_area_m2"]
    return e["cl_max"] * qS / (e["mass_kg"] * G0)


def v_top(e):
    """Level top speed: largest V with full-throttle excess thrust >= 0 (T(V) - D(V,1) = 0)."""
    def excess(V):
        return thrust(e, V) - drag(e, V, 1.0)
    vs = v_stall_1g(e)
    hi = e["v_max_mps"]
    lo = max(vs, V_MIN)
    if excess(lo) <= 0.0:
        return lo  # cannot even sustain level flight at stall (degenerate)
    # bisect for the upper root (excess is + at lo, - at hi since T->0, D->big near v_max)
    if excess(hi) > 0.0:
        return hi  # thrust never runs out below v_max
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        if excess(mid) > 0.0:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def climb_best(e):
    """Best steady climb rate = max specific excess power Ps(V) = V*(T-D)/(m*g0) at n=1."""
    vs = v_stall_1g(e)
    best, bv = -1e9, vs
    V = vs
    while V <= e["v_max_mps"]:
        ps = V * (thrust(e, V) - drag(e, V, 1.0)) / (e["mass_kg"] * G0)
        if ps > best:
            best, bv = ps, V
        V += 0.25
    return best, bv


def n_sustained(e, V):
    """Largest load factor sustainable at V with full throttle (T == D), capped by aero + struct."""
    qS = 0.5 * RHO0 * V * V * e["wing_area_m2"]
    avail = thrust(e, V) - qS * e["cd0"]       # thrust left for induced drag
    if avail <= 0.0:
        return 0.0
    # k*(n*m*g0)^2/qS = avail  ->  n = sqrt(avail*qS/k)/(m*g0)
    n = math.sqrt(avail * qS / e["induced_k"]) / (e["mass_kg"] * G0)
    n = min(n, n_aero(e, V), e["n_max_struct"])
    return n


def turn_sust(e):
    """Best sustained turn rate (deg/s) and the speed it occurs at."""
    vs = v_stall_1g(e)
    best, bv, bn = 0.0, vs, 1.0
    V = vs
    while V <= e["v_max_mps"]:
        n = n_sustained(e, V)
        if n > 1.0:
            omega = G0 * math.sqrt(n * n - 1.0) / V
            if omega > best:
                best, bv, bn = omega, V, n
        V += 0.25
    return math.degrees(best), bv, bn


def turn_inst(e):
    """Instantaneous turn rate (deg/s) at the corner speed (n = min(n_max_struct, n_aero(V*)))."""
    vstar = v_stall_1g(e) * math.sqrt(e["n_max_struct"])
    n = min(e["n_max_struct"], n_aero(e, vstar))
    if n <= 1.0:
        return 0.0, vstar
    return math.degrees(G0 * math.sqrt(n * n - 1.0) / vstar), vstar


def analyze(name):
    e = envmod.load_envelope(name)
    vs = v_stall_1g(e)
    cb, cbv = climb_best(e)
    ts, tsv, tsn = turn_sust(e)
    ti, tiv = turn_inst(e)
    return {
        "name": name, "vstall": vs, "corner": vs * math.sqrt(e["n_max_struct"]),
        "vtop": v_top(e), "climb": cb, "climb_v": cbv,
        "turn_sust": ts, "turn_sust_v": tsv, "turn_sust_n": tsn,
        "turn_inst": ti, "turn_inst_v": tiv,
    }


def print_table():
    rows = [analyze(n) for n in ROSTER]
    h = (f"{'aircraft':<12} {'stall':>6} {'corner':>7} {'v_top':>7} {'v_top':>8} "
         f"{'climb':>6} {'@V':>5} {'sustT':>6} {'@V':>5} {'instT':>6} {'@V*':>5}")
    print(h)
    print(f"{'':12} {'m/s':>6} {'m/s':>7} {'m/s':>7} {'km/h':>8} {'m/s':>6} {'m/s':>5} "
          f"{'°/s':>6} {'m/s':>5} {'°/s':>6} {'m/s':>5}")
    print("-" * len(h))
    for r in rows:
        print(f"{NAMES[r['name']]:<12} {r['vstall']:>6.1f} {r['corner']:>7.1f} "
              f"{r['vtop']:>7.1f} {r['vtop']*3.6:>8.1f} {r['climb']:>6.1f} {r['climb_v']:>5.0f} "
              f"{r['turn_sust']:>6.2f} {r['turn_sust_v']:>5.0f} "
              f"{r['turn_inst']:>6.2f} {r['turn_inst_v']:>5.0f}")
    return rows


def main():
    check = "--check" in sys.argv[1:]
    try:
        rows = print_table()
    except Exception as ex:
        print(f"perf_probe load/compute error: {ex}")
        sys.exit(1)
    if not check:
        sys.exit(0)
    # --check: B4 relative-ordering / target assertions are added once the retune lands.
    print("\n(--check targets not yet defined; printing only)")
    sys.exit(0)


if __name__ == "__main__":
    main()
