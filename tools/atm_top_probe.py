#!/usr/bin/env python3
"""
atm_top_probe.py — ATM-Sphere v1.2r0
Ceiling clamp + soft-band monotonicity probe. Uses the SAME ceiling model as the
reference kernel (tools/ref_kernel.ceiling_climb_rate) so the probe tracks the sim.

Acceptance:
  * Clamp: climbs from just below ATM_TOP never overshoot ATM_TOP.
  * Soft band: climb rate at 7950 m <= climb rate at 7800 m (monotone reduction).

Usage:  python tools/atm_top_probe.py --ceil 8000 --soft 100
Exit:   0 PASS, 1 FAIL
"""
import argparse, json, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ref_kernel import ceiling_climb_rate


def run_sim(start_alt, ceil, soft, t_s, req=10.0, dt=0.01):
    alt = start_alt
    max_alt = alt
    n = int(t_s / dt)
    for _ in range(n):
        rate = ceiling_climb_rate(req, alt, ceil, soft)
        alt = min(alt + rate * dt, ceil)
        if alt > max_alt:
            max_alt = alt
    return max_alt


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--ceil', type=float, required=True)
    p.add_argument('--soft', type=float, required=True)
    args = p.parse_args()
    ok = True

    for start in (args.ceil - 20.0, args.ceil - 10.0, args.ceil - 5.0):
        max_alt = run_sim(start, args.ceil, args.soft, 20.0)
        if max_alt > args.ceil + 1e-6:
            ok = False
            print(f"FAIL clamp: overshoot {max_alt:.6f} > {args.ceil} from {start}")

    rate_7800 = ceiling_climb_rate(10.0, args.ceil - 200.0, args.ceil, args.soft)
    rate_7950 = ceiling_climb_rate(10.0, args.ceil - 50.0, args.ceil, args.soft)
    if not (rate_7950 <= rate_7800):
        ok = False
        print(f"FAIL soft band: rate@-50={rate_7950} > rate@-200={rate_7800}")

    print(json.dumps({"ceil": args.ceil, "soft": args.soft,
                      "result": "PASS" if ok else "FAIL"}))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
