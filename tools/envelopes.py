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
               "convergence_m",
               # Region toughness (Step 7 guns, seal v1.20r0): per-airframe fractions of hp_start
               # sizing the ENGINE/WING/TAIL region sub-pools (v1.18r0 made them global 0.375/0.5/
               # 0.25 hex-constants; those values survive as the envelope-less DEFAULTS, e.g. the
               # no-arg Sphere golden). Each MUST be a positive multiple of 1/8 and <= 1 (enforced
               # by tuning_probe): eighth-granularity keeps every pool exact in f64 AND milli-exact
               # on the WEAPON-001 wire (1000/8 = 125 clears the denominator for integer hp_start,
               # and integer per-round damage preserves the granularity as pools drain). See
               # ADR-Step7-Guns-RegionToughness-v1.20r0.
               "engine_frac", "wing_frac", "tail_frac",
               # Supercharger critical altitude (flight model, seal v1.22r0): the altitude (m) up
               # to which the engine holds RATED power. The kernel thrust lapse is
               # min(1, sigma(alt)/sigma(crit_alt_m)) — one comparison + one divide, no new
               # det_math. MUST be a multiple of 500 inside [0, 8000] (tuning_probe-enforced) so
               # sigma(crit) is EXACTLY a sealed ISA-LUT node; 0 means "no supercharger" and
               # reproduces the B5 thrust (T *= sigma) bit-for-bit. thrust_static_n/v_max_mps were
               # re-anchored with it (top speed at the CRITICAL altitude, sea-level climb) by
               # tools/supercharger_retune.py. See ADR-Step8-FlightModel-Supercharger-v1.22r0.
               "crit_alt_m",
               # Two-speed blower schedule (flight model, seal v1.25r0): crit_lo_alt_m is the LOW
               # (MS) gear's full-throttle height and gear2_frac the HIGH (FS) gear's rated-power
               # fraction (driving the taller gear costs shaft power). The kernel lapse becomes
               # max(min(1, sigma/sigma(crit_lo_alt_m)), gear2_frac*min(1, sigma/sigma(crit_alt_m)))
               # — flat-fall-flat-fall. crit_lo_alt_m MUST be a multiple of 500 in (0, crit_alt_m)
               # (a sealed LUT node) and gear2_frac a multiple of 1/16 strictly between
               # sigma(crit_alt_m)/sigma(crit_lo_alt_m) and 1 (both gears non-degenerate) — OR
               # crit_lo_alt_m = 0 with gear2_frac = 1: single-speed, the v1.22r0 lapse path
               # BIT-FOR-BIT (tuning_probe-enforced). thrust_static_n/v_max_mps re-anchored for
               # two-speed airframes by tools/blower_retune.py.
               # See ADR-Step8-FlightModel-TwoSpeedBlower-v1.25r0.
               "crit_lo_alt_m", "gear2_frac")


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
