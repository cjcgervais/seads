#!/usr/bin/env python3
"""Generate the sealed B5 ISA density-ratio LUT (ATM-Sphere v1.21r0).

PROVENANCE TOOL, not a gate: this script documents where the sealed sigma(h) node values
come from and lets an auditor reproduce them. The kernel NEVER evaluates the ISA power law —
the sealed spec is the emitted hex-float table itself, interpolated at runtime by the existing
deterministic `lut_eval` (pure +,-,*,/ — zero new det_math, exactly like the envelope LUTs).

Model: ICAO International Standard Atmosphere, troposphere (the whole ATM realm sits inside it:
alt is clamped to [0, ATM_TOP=8000 m] << 11 km):
    T(h)     = T0 - L*h                          T0 = 288.15 K, L = 0.0065 K/m
    sigma(h) = rho(h)/rho0 = (T/T0)^(g0/(Rs*L) - 1)
with g0 = 9.80665 m/s^2 (the SEADS gravity rail) and Rs = 287.05287 J/(kg K) (ICAO).
Exponent g0/(Rs*L) - 1 = 4.2558797...

Nodes every 500 m over [0, 8000] -> 17 nodes. Max piecewise-linear error vs the power law is
~2.2e-4 absolute (~0.02% density) near the ground and smaller aloft — far below any physical
fidelity claim; determinism, not the ISA fit, is the contract.

Usage: python tools/gen_isa_lut.py   (prints the Python and C++ literal blocks)
"""

T0 = 288.15
L = 0.0065
G0 = 9.80665
RS = 287.05287
STEP_M = 500.0
TOP_M = 8000.0


def sigma(h):
    return ((T0 - L * h) / T0) ** (G0 / (RS * L) - 1.0)


def main():
    hs = [i * STEP_M for i in range(int(TOP_M / STEP_M) + 1)]
    vals = [sigma(h) for h in hs]
    print("# --- ref_kernel.py block ---")
    print("ISA_SIGMA_ALT = (")
    for h in hs:
        print(f"    float.fromhex('{float(h).hex()}'),   # {h:6.0f} m")
    print(")")
    print("ISA_SIGMA = (")
    for h, v in zip(hs, vals):
        print(f"    float.fromhex('{v.hex()}'),   # sigma({h:.0f}) = {v!r}")
    print(")")
    print()
    print("// --- kernel.cpp block ---")
    print("static constexpr double ISA_SIGMA_ALT[ISA_SIGMA_N] = {")
    for h in hs:
        print(f"    {float(h).hex()},   // {h:6.0f} m")
    print("};")
    print("static constexpr double ISA_SIGMA[ISA_SIGMA_N] = {")
    for h, v in zip(hs, vals):
        print(f"    {v.hex()},   // sigma({h:.0f}) = {v!r}")
    print("};")


if __name__ == "__main__":
    main()
