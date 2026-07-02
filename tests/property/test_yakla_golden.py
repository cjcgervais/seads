"""Non-degeneracy guards for GOLDEN-SK-YakLa-001 (Yak-3 / La-7 envelope interpolation golden).

The golden's whole point is that both new envtab airframes fly at NON-BREAKPOINT TAS with
OVER-LIMIT commanded banks, so the piecewise-linear LUT interpolation (phi_max, roll_rate) and
the phi_max clamp are genuinely exercised in C++ <-> Python lockstep. Bit-exact parity is proven
by the golden hash itself; these properties pin the SETUP against a future edit quietly making it
degenerate (a start TAS moved onto a LUT node, a commanded bank tuned below the envelope limit,
or a retune bending the LUTs flat). A tuning retune that moves the trajectory is a seal anyway —
these tests just make the failure loud at pytest time instead of silently weakening the coverage.
"""
import json
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import detmath_ref as dm         # noqa: E402
import envelopes as envmod       # noqa: E402

SCEN = json.loads(
    (ROOT / "config" / "scenarios" / "GOLDEN-SK-YakLa-001.json").read_text(encoding="utf-8"))
LUTS = ("phi_max", "roll_rate")


def _envs():
    return [(ac, envmod.load_envelope(ac["envelope"])) for ac in SCEN["aircraft"]]


def test_start_tas_strictly_inside_a_lut_segment():
    """Each ship's start TAS sits STRICTLY between two adjacent LUT breakpoints (never on a
    node), inside the table domain — so the very first tick already interpolates."""
    for ac, env in _envs():
        tas = float(ac["start"]["tas_mps"])
        for f in LUTS:
            xs = env[f][0]
            assert xs[0] < tas < xs[-1], (ac["envelope"], f, tas)
            assert all(tas != x for x in xs), (ac["envelope"], f, tas)


def test_commanded_banks_exceed_phi_max_everywhere():
    """The largest scheduled |bank| exceeds the envelope's phi_max at EVERY speed, so the
    interpolated clamp (not the raw command) sets the achieved bank in the maneuver phases."""
    for ac, env in _envs():
        max_cmd = max(abs(envmod.deg2rad(ph["bank_deg"])) for ph in ac["schedule"])
        assert max_cmd > max(env["phi_max"][1]), ac["envelope"]


@settings(max_examples=200)
@given(env_name=st.sampled_from(["yak3", "la7"]),
       f=st.sampled_from(LUTS),
       seg=st.integers(min_value=0, max_value=3),
       t=st.floats(min_value=0.05, max_value=0.95))
def test_lut_eval_interpolates_between_adjacent_nodes(env_name, f, seg, t):
    """Inside any segment of the Yak-3/La-7 LUTs, lut_eval is bounded by the segment's node
    values, and strictly off both nodes wherever the segment isn't flat (true piecewise-linear
    interpolation, not nearest-node snapping)."""
    env = envmod.load_envelope(env_name)
    xs, ys = env[f]
    x = xs[seg] + t * (xs[seg + 1] - xs[seg])
    v = dm.lut_eval(xs, ys, x)
    lo, hi = min(ys[seg], ys[seg + 1]), max(ys[seg], ys[seg + 1])
    assert lo <= v <= hi
    if ys[seg] != ys[seg + 1]:
        assert v != ys[seg] and v != ys[seg + 1]
