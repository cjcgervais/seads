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

# Scalar aero params, in the exact order they appear on the C++ Envelope struct (flight_types.h)
# and are emitted by gen_envelope_tables.py / gen_{lockstep,predict}_vectors.py. JSON key ->
# struct field. The first six are B1 longitudinal energy (seal v1.5r0); the last three are the B3
# limits & stall block (seal v1.7r0): cl_max (max usable lift coefficient -> accelerated-stall
# ceiling on the load factor), n_max_struct / n_min_struct (per-airframe structural g limits that
# retire the B2 global placeholder clamp [-3, 9]). See ADR-Step8-FlightModel-B3.
AERO_FIELDS = ("mass_kg", "wing_area_m2", "cd0", "induced_k", "thrust_static_n", "v_max_mps",
               "cl_max", "n_max_struct", "n_min_struct",
               # G3 per-airframe weapon roster (Step 7 guns, seal v1.11r0): starting hitpoints
               # (airframe toughness), muzzle velocity (added to firer TAS), damage per round, and
               # the fire-rate interval in ticks (min ticks between shots; the kernel gates firing
               # with a per-aircraft cooldown). See ADR-Step7-Guns-G3.
               "hp_start", "muzzle_v_mps", "damage_per_round", "rof_interval_ticks",
               # G4 finite ammunition (Step 7 guns, seal v1.13r0): the magazine size (abstract
               # rounds). Firing is gated on ammo > 0 (decrement one per spawned round); at 0 the
               # gun falls silent ("Winchester"). Relative WWII endurance (A6M2 fewest — the famous
               # ~60-rpg 20 mm cannon; P-47D most — eight deep .50-cal belts). See ADR-Step7-Guns-G4.
               "ammo_start",
               # Gun convergence / harmonization (Step 7 guns, seal v1.15r0): the per-airframe
               # boresight range (m). SEADS models a single CENTERLINE battery, so harmonization is
               # realized as VERTICAL zeroing — a fired round's initial gamma is offset UP by the
               # flat-fire drop-compensation angle (0.5*g0*convergence_m / v^2, v = firer TAS +
               # muzzle_v) so its trajectory crosses the aim (sight) line at convergence_m. Pure
               # +-*/ (no new det_math). See ADR-Step7-Guns-Convergence.
               "convergence_m")


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
