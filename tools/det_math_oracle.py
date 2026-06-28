#!/usr/bin/env python3
"""
det_math_oracle.py — verify SEADS det_math REFERENCE against MPFR ground truth.

For each det_math function we sample its SEADS operating domain, compute the
correctly-rounded double result with MPFR (gmpy2, 200-bit working precision) and
measure the ULP distance to detmath_ref. Functions pass under their ULP budget.

This is the metamorphic "test oracle": MPFR is platform-independent by construction,
so it anchors det_math correctness independent of any libm.

Usage:  python tools/det_math_oracle.py [--samples N] [--seed S]
Exit:   0 PASS, 1 FAIL
"""
import argparse, struct, sys

try:
    import gmpy2
    from gmpy2 import mpfr
except Exception as e:  # pragma: no cover
    print(f"gmpy2 required for the oracle (pip install gmpy2): {e}")
    sys.exit(1)

import detmath_ref as dm

PREC = 200


def ulp_diff(a, b):
    """Distance in ULPs between two doubles using their ordered-int representation."""
    if a == b:
        return 0
    ia = struct.unpack('<q', struct.pack('<d', a))[0]
    ib = struct.unpack('<q', struct.pack('<d', b))[0]
    if ia < 0:
        ia = (1 << 63) - ia
    if ib < 0:
        ib = (1 << 63) - ib
    return abs(ia - ib)


def correctly_rounded(fn_mpfr, x):
    with gmpy2.context(precision=PREC):
        r = fn_mpfr(mpfr(x))
    return float(r)


def lin(lo, hi, n):
    return [lo + (hi - lo) * (i / (n - 1)) for i in range(n)]


def check(name, det_fn, mpfr_fn, xs, budget):
    worst = 0
    worst_x = None
    for x in xs:
        d = det_fn(x)
        g = correctly_rounded(mpfr_fn, x)
        u = ulp_diff(d, g)
        if u > worst:
            worst, worst_x = u, x
    ok = worst <= budget
    flag = "PASS" if ok else "FAIL"
    print(f"  [{flag}] {name:<10} max_ulp={worst:>3}  budget={budget}  @x={worst_x}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=4000)
    args = ap.parse_args()
    n = args.samples
    ok = True

    print(f"det_math oracle vs MPFR (prec={PREC}), {n} samples/function")

    # sqrt over altitude/length-ish domain
    ok &= check("sqrt", dm.det_sqrt, gmpy2.sqrt, lin(1e-12, 4.0, n) + lin(1.0, 1e8, n), 0)

    # sin/cos over pre-wrapped angle domain [-2pi, 2pi]
    ang = lin(-dm.TWO_PI, dm.TWO_PI, n)
    ok &= check("sin", dm.det_sin, lambda v: gmpy2.sin(v), ang, 2)
    ok &= check("cos", dm.det_cos, lambda v: gmpy2.cos(v), ang, 2)

    # atan over wide domain
    ok &= check("atan", dm.det_atan, lambda v: gmpy2.atan(v), lin(-50.0, 50.0, n), 2)

    # asin over [-1, 1]
    ok &= check("asin", dm.det_asin, lambda v: gmpy2.asin(v), lin(-0.999999, 0.999999, n), 4)

    # tan away from poles (turn-law domain: |phi| < ~80 deg)
    ok &= check("tan", dm.det_tan, lambda v: gmpy2.tan(v),
                lin(-1.3962634, 1.3962634, n), 4)

    print("RESULT: DET_MATH ORACLE PASS" if ok else "RESULT: DET_MATH ORACLE FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
