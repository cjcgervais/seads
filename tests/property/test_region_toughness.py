"""Per-airframe region toughness (ATM-Sphere v1.20r0).

The v1.18r0 global ENGINE/WING/TAIL pool fractions became per-airframe envelope scalars
(engine_frac/wing_frac/tail_frac). Kernel::add / ref_kernel.Aircraft take them as defaulted
params — the defaults ARE the sealed v1.18r0 globals, so every envelope-less caller (the no-arg
Sphere golden, lockstep/predict vectors) is bit-identical and only envelope-seeded goldens moved.
build_scenario / session_ref / the C++ callers pass the envelope's values. Each fraction must be
a positive multiple of 1/8 and <= 1: eighth-granularity keeps every pool exact in f64 AND
milli-exact on the WEAPON-001 wire (1000/8 = 125 clears the denominator for integer hp_start,
and integer per-round damage preserves the granularity as pools drain).
Bit-exact C++<->reference parity is gated by the 12 goldens.
"""
import json
import sys
from pathlib import Path

from hypothesis import given, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
ROSTER = sorted(p.stem for p in (ROOT / "data" / "tuning" / "envelopes").glob("*.json"))
FRACS = ("engine_frac", "wing_frac", "tail_frac")
INLINE = ("bf109f4", "ki61", "yak3", "spitfire_mk5", "p51")   # liquid-cooled engines


def _scenario_for(name):
    """Minimal one-ship scenario doc referencing envelope `name` (build_scenario shape)."""
    return {
        "header": {"id": f"TOUGH-{name}"},
        "ticks": 1,
        "aircraft": [{
            "envelope": name,
            "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 90.0, "phi_deg": 0.0,
                      "alt_m": 4000.0, "tas_mps": 150.0},
            "schedule": [{"start_tick": 0, "bank_deg": 0.0, "g_cmd": 1.0}],
        }],
    }


def test_roster_fractions_are_dyadic_eighths():
    # Every roster envelope carries the three toughness fields; each is a positive multiple of
    # 1/8 and <= 1 (the exactness contract tuning_probe enforces), and hp_start is an integer,
    # so every pool value is exact in f64 and integral at wire milli-scale.
    for name in ROSTER:
        env = envmod.load_envelope(name)
        assert float(env["hp_start"]) == round(env["hp_start"])
        for f in FRACS:
            v = float(env[f])
            assert 0.0 < v <= 1.0, (name, f, v)
            assert v * 8.0 == round(v * 8.0), (name, f, v)
            assert (v * env["hp_start"]) * 1000.0 == round(v * env["hp_start"] * 1000.0)


def test_pools_size_from_the_envelope():
    # build_scenario derives each ship's pools from ITS envelope's fractions (the per-airframe
    # consumer), exactly frac * hp_start.
    for name in ROSTER:
        env = envmod.load_envelope(name)
        k, _, _, _, _ = rk.build_scenario(RAILS, _scenario_for(name))
        a = k.aircraft[0]
        assert a.engine_hp == env["engine_frac"] * env["hp_start"]
        assert a.wing_hp == env["wing_frac"] * env["hp_start"]
        assert a.tail_hp == env["tail_frac"] * env["hp_start"]


def test_defaults_preserve_the_sealed_baseline():
    # An Aircraft built WITHOUT fractions still sizes its pools from the sealed v1.18r0 globals
    # (0.375/0.5/0.25) — the reason the no-arg Sphere golden did not move at v1.20r0.
    a = rk.Aircraft(0.0, 0.0, 0.0, 0.0, 1000.0, 250.0)
    assert (rk.ENGINE_FRAC, rk.WING_FRAC, rk.TAIL_FRAC) == (0.375, 0.5, 0.25)
    assert a.engine_hp == 0.375 * rk.START_HP
    assert a.wing_hp == 0.5 * rk.START_HP
    assert a.tail_hp == 0.25 * rk.START_HP


def test_toughness_is_actually_per_airframe():
    # Non-degeneracy guard (the data really is a retune, not the old globals under new names):
    # at least one roster airframe departs from the v1.18r0 default in EACH region slot, and the
    # radial-vs-inline flavor holds — the P-47D's engine fraction out-toughs every liquid-cooled
    # inline, and no airframe is uniformly at the old defaults in all three slots... except where
    # chosen deliberately (a6m2 keeps engine_frac=0.375 so GOLDEN-SK-EngineOut-001's drain
    # sequence is preserved byte-for-byte).
    envs = {name: envmod.load_envelope(name) for name in ROSTER}
    defaults = {"engine_frac": rk.ENGINE_FRAC, "wing_frac": rk.WING_FRAC,
                "tail_frac": rk.TAIL_FRAC}
    for f, dv in defaults.items():
        assert any(envs[n][f] != dv for n in ROSTER), f"no airframe departs from default {f}"
    p47_engine = envs["p47d"]["engine_frac"]
    for n in INLINE:
        assert envs[n]["engine_frac"] < p47_engine, (n, "inline should be tenderer than the P-47")
    assert envs["a6m2"]["engine_frac"] == 0.375   # EngineOut-001 story pinned


def test_toughness_binds_through_the_kernel():
    # The kernel CONSUMES the fraction: the same head-on round knocks out a tender engine
    # (frac 1/8 -> pool 8.75 < 30) but not a tough one (frac 5/8 -> pool 43.75 > 30), and only
    # the tender aircraft loses thrust (decelerates at the same throttle).
    def run(engine_frac):
        k = rk.Kernel(RAILS)
        env = envmod.load_envelope("a6m2")
        sh = rk.Aircraft(0.0, rk.deg2rad(30.0), rk.deg2rad(90.0), 0.0, 4000.0, 150.0,
                         hp=env["hp_start"])   # far-away shooter: owns the round (a round
        k.aircraft.append(sh)                  # never strikes its own owner)
        tg = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 150.0,
                         hp=env["hp_start"], engine_frac=engine_frac)
        k.aircraft.append(tg)
        k.projectiles.append(rk.Projectile(
            lat=tg.lat, lon=tg.lon, psi=rk.deg2rad(270.0), alt=tg.alt,
            tas=600.0, gamma=0.0, damage=30.0, ttl=100, owner=0))
        cmds = [(0.0, 1.0, 0.7, False), (0.0, 1.0, 0.7, False)]
        # B5 (v1.21r0): at the 4000 m test altitude sigma ~0.67 scales BOTH the glider's drag
        # and the powered ship's thrust down, so the TAS gap opens ~2/3 as fast as under the
        # old constant-density model — 500 ticks (5 s) restores a decisive margin.
        for _ in range(500):
            k.step_scenario(cmds, [env, env])
        return tg
    tender, tough = run(0.125), run(0.625)
    assert tender.hp == tough.hp > 0.0                    # same total damage, both LIVING
    assert tender.engine_hp == 0.0 and tough.engine_hp > 0.0
    assert tender.tas < tough.tas - 5.0                   # engine-out glider vs powered flight


@given(e8=st.integers(1, 8), hp=st.integers(1, 400),
       dmgs=st.lists(st.integers(1, 60), max_size=20))
def test_dyadic_drain_stays_exact(e8, hp, dmgs):
    # For ANY eighth-fraction, integer hp_start, and integer per-round damage sequence, the pool
    # remains an exact multiple of 1/8 through draining and clamping — so its f64 value and its
    # milli-scale wire quantization are exact at every step (the tuning_probe contract's proof).
    pool = (e8 / 8.0) * hp
    for d in dmgs:
        pool = pool - float(d)
        if pool < 0.0:
            pool = 0.0
        assert pool * 8.0 == round(pool * 8.0)
        assert pool * 1000.0 == round(pool * 1000.0)
