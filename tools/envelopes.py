#!/usr/bin/env python3
"""
envelopes.py — SEADS shared envelope loader (ATM-Sphere).

Single source of truth for turning a tuning-envelope JSON
(data/tuning/envelopes/<name>.json) into the radian-unit LUTs the kernel
interpolates. Imported by BOTH tools/ref_kernel.py (the canonical reference) and
tools/gen_envelope_tables.py (the C++ header generator) so the two can never drift.

Determinism note: the deg->rad conversion is baked into the LUT *nodes* here, once,
using the same formula as ref_kernel.deg2rad ( d * (PI/180.0) ). The kernel then
interpolates a pure-radian table. This avoids any convert-vs-interpolate ordering
ambiguity. Values are exact IEEE-754 doubles; float.hex() round-trips them bit-exact
into the generated C++ header.

LUT order emitted per envelope: phi_max (rad), roll_rate (rad/s), climb_max (m/s),
climb_min (m/s). Each is (xs, ys) with xs = TAS breakpoints (m/s, strictly increasing).
"""
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import detmath_ref as dm

ROOT = Path(__file__).resolve().parent.parent
ENV_DIR = ROOT / "data" / "tuning" / "envelopes"

# Canonical field order for the generated table / kernel struct.
LUT_FIELDS = ("phi_max", "roll_rate", "climb_max", "climb_min")

# B1 longitudinal-energy scalar aero params, in the exact order they appear on the C++ Envelope
# struct (flight_types.h) and are emitted by gen_envelope_tables.py. JSON key -> struct field.
AERO_FIELDS = ("mass_kg", "wing_area_m2", "cd0", "induced_k", "thrust_static_n", "v_max_mps")


def deg2rad(d):
    # MUST match ref_kernel.deg2rad and golden_main::deg2rad op-for-op.
    return d * (dm.PI / 180.0)


def _axis(lut):
    return [float(x) for x, _ in lut]


def _vals(lut, conv):
    return [conv(float(y)) for _, y in lut]


def load_envelope(name, root=ROOT):
    """Return {LUT field: (xs, ys)} in radian units (deg->rad baked into nodes) plus the B1 scalar
    aero params (mass_kg, wing_area_m2, cd0, induced_k, thrust_static_n, v_max_mps) as plain floats."""
    path = Path(root) / "data" / "tuning" / "envelopes" / f"{name}.json"
    t = json.loads(path.read_text(encoding="utf-8"))["tuning"]
    env = {
        "phi_max":   (_axis(t["phi_max_deg_vs_tas"]),        _vals(t["phi_max_deg_vs_tas"],        deg2rad)),
        "roll_rate": (_axis(t["roll_rate_degps_vs_tas"]),    _vals(t["roll_rate_degps_vs_tas"],    deg2rad)),
        "climb_max": (_axis(t["climb_rate_max_mps_vs_tas"]), _vals(t["climb_rate_max_mps_vs_tas"], float)),
        "climb_min": (_axis(t["climb_rate_min_mps_vs_tas"]), _vals(t["climb_rate_min_mps_vs_tas"], float)),
    }
    for f in AERO_FIELDS:
        env[f] = float(t[f])
    return env
