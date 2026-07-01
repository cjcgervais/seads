"""Roster sanity for G3 per-airframe weapon stats + G4 finite ammunition (Step 7 guns).

G3 (v1.11r0): the 8 envelopes carry per-airframe weapon scalars (hp_start, muzzle_v_mps,
damage_per_round, rof_interval_ticks). G4 (v1.13r0): each also carries ammo_start (magazine size),
and firing is gated on ammo>0 (one round consumed per shot; an empty magazine falls silent =
"Winchester"). These checks pin that every airframe has sane, present values, that the roster reads
with the intended WWII character (cannon airframes hit harder per round; the A6M2 is the fragile,
lowest-ammo glass cannon; the P-47D is the durable, deepest-magazine gun platform), and that the
depletion gate actually bites in the deterministic kernel.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import envelopes as envmod       # noqa: E402
import ref_kernel as rk          # noqa: E402

ROSTER = ("p47d", "bf109f4", "ki61", "a6m2", "yak3", "la7", "spitfire_mk5", "p51")
WEAPON_FIELDS = ("hp_start", "muzzle_v_mps", "damage_per_round", "rof_interval_ticks", "ammo_start")


def test_every_airframe_has_sane_weapon_stats():
    for name in ROSTER:
        e = envmod.load_envelope(name)
        for f in WEAPON_FIELDS:
            assert f in e, f"{name} missing {f}"
        assert 40.0 <= e["hp_start"] <= 250.0, name           # toughness in a sane band
        assert 400.0 <= e["muzzle_v_mps"] <= 1100.0, name     # muzzle velocity sane
        assert 5.0 <= e["damage_per_round"] <= 60.0, name     # per-round damage sane
        assert 1.0 <= e["rof_interval_ticks"] <= 20.0, name   # >=100 ms? no — 1..20 ticks between shots
        assert 50.0 <= e["ammo_start"] <= 600.0, name         # G4: magazine in a sane band


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


# --- G4 finite ammunition (seal v1.13r0) ------------------------------------------------

def test_ammo_varies_and_reads_with_wwii_character():
    ammo = {n: envmod.load_envelope(n)["ammo_start"] for n in ROSTER}
    assert len({*ammo.values()}) >= 3, f"ammo_start barely varies: {ammo}"
    # the P-47D's eight deep .50-cal belts = the roster's largest magazine; the A6M2's ~60-rpg
    # 20 mm cannon = the smallest (the glass cannon that also runs dry fastest).
    assert ammo["p47d"] == max(ammo.values())
    assert ammo["a6m2"] == min(ammo.values())


def _fire_forever(name, ticks):
    """Drive one aircraft of `name` holding the trigger, wings level, for `ticks`. Returns the list
    of ticks on which a fresh round spawned + the final ammo (via the sealed reference kernel)."""
    rails = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
    env = envmod.load_envelope(name)
    k = rk.Kernel(rails)
    a = rk.Aircraft(lat=0.0, lon=0.0, psi=rk.deg2rad(90.0), phi=0.0, alt=5000.0, tas=150.0)
    a.hp = env["hp_start"]
    a.ammo = env["ammo_start"]
    k.aircraft.append(a)
    cmd = [(0.0, 1.0, 0.8, True)]                       # bank 0, n=1, throttle, fire held
    spawn_ticks = []
    for t in range(ticks):
        k.step_scenario(cmd, [env])
        if any(p.ttl == rk.PROJ_TTL_TICKS for p in k.projectiles):   # a fresh round this tick
            spawn_ticks.append(t)
    return spawn_ticks, a.ammo


def test_firing_consumes_exactly_ammo_start_rounds_then_goes_silent():
    # An aircraft holding the trigger fires exactly ammo_start rounds (one per shot), the magazine
    # clamps to 0, and firing STOPS — no round spawns after depletion (the "Winchester" gate).
    env = envmod.load_envelope("a6m2")
    n0 = int(env["ammo_start"])
    rof = int(env["rof_interval_ticks"])
    # run well past depletion: n0 shots at `rof` ticks apart, plus a long silent tail.
    ticks = n0 * rof + 200
    spawn_ticks, final_ammo = _fire_forever("a6m2", ticks)
    assert len(spawn_ticks) == n0, (len(spawn_ticks), n0)     # exactly one round per unit of ammo
    assert final_ammo == 0.0                                  # magazine clamps at empty
    assert spawn_ticks == list(range(0, n0 * rof, rof))       # cadence = rof_interval, then silent
    # depletion tick is the last shot; nothing fires after it despite the trigger still held
    assert max(spawn_ticks) == (n0 - 1) * rof


def test_ammo_never_goes_negative_even_when_trigger_held_long():
    for name in ("a6m2", "p47d"):
        env = envmod.load_envelope(name)
        _, final_ammo = _fire_forever(name, int(env["ammo_start"]) * int(env["rof_interval_ticks"]) + 50)
        assert final_ammo == 0.0                              # never underflows past 0
