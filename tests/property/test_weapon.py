"""Roster sanity for G3 per-airframe weapon stats (Step 7 guns, ATM-Sphere v1.11r0).

The 8 envelopes now carry per-airframe weapon scalars (hp_start, muzzle_v_mps, damage_per_round,
rof_interval_ticks). These checks pin that every airframe has sane, present values and that the
roster reads with the intended WWII character (cannon airframes hit harder per round; the A6M2 is
the fragile glass cannon; the P-47D is the durable high-volume gun platform).
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import envelopes as envmod       # noqa: E402

ROSTER = ("p47d", "bf109f4", "ki61", "a6m2", "yak3", "la7", "spitfire_mk5", "p51")
WEAPON_FIELDS = ("hp_start", "muzzle_v_mps", "damage_per_round", "rof_interval_ticks")


def test_every_airframe_has_sane_weapon_stats():
    for name in ROSTER:
        e = envmod.load_envelope(name)
        for f in WEAPON_FIELDS:
            assert f in e, f"{name} missing {f}"
        assert 40.0 <= e["hp_start"] <= 250.0, name           # toughness in a sane band
        assert 400.0 <= e["muzzle_v_mps"] <= 1100.0, name     # muzzle velocity sane
        assert 5.0 <= e["damage_per_round"] <= 60.0, name     # per-round damage sane
        assert 1.0 <= e["rof_interval_ticks"] <= 20.0, name   # >=100 ms? no — 1..20 ticks between shots


def test_roster_is_distinct_not_a_single_global_gun():
    # G3's whole point: airframes differ. At least 3 distinct values exist in each weapon dimension
    # (i.e. it is not the old single global gun copied 8x).
    for f in WEAPON_FIELDS:
        vals = {envmod.load_envelope(n)[f] for n in ROSTER}
        assert len(vals) >= 3, f"{f} barely varies across the roster: {vals}"


def test_cannon_airframes_hit_harder_than_mg_airframes():
    # 20mm-cannon airframes (A6M2, La-7) do more damage per round than the .50cal platforms (P-47D,
    # P-51), which instead lean on volume of fire (a faster rate / shorter interval).
    p47, p51 = envmod.load_envelope("p47d"), envmod.load_envelope("p51")
    a6m, la7 = envmod.load_envelope("a6m2"), envmod.load_envelope("la7")
    for cannon in (a6m, la7):
        for mg in (p47, p51):
            assert cannon["damage_per_round"] > mg["damage_per_round"]
    # the .50cal platforms fire faster (smaller interval) than the slow 20mm Zero
    assert p47["rof_interval_ticks"] < a6m["rof_interval_ticks"]
    assert p51["rof_interval_ticks"] < a6m["rof_interval_ticks"]


def test_p47d_is_the_durable_platform_and_a6m2_the_fragile_one():
    hps = {n: envmod.load_envelope(n)["hp_start"] for n in ROSTER}
    assert hps["p47d"] == max(hps.values())                  # toughest
    assert hps["a6m2"] == min(hps.values())                  # most fragile
