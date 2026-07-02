"""Metamorphic properties for attacker attribution (Step 7 guns, ATM-Sphere v1.16r0).

`last_hit_by` is the kernel-side event hook the guns arc deferred: each aircraft records the index
of the aircraft whose round most recently damaged it (NO_ATTACKER == -1 == never hit). It is set at
hit time from the striking round's owner and PERSISTS through death, so at hp<=0 it names the KILLER.
It is the 11th per-aircraft snapshot f64 (a pure integer-valued state, like fire_cd/ammo — no new
det_math). Bit-exact C++<->reference parity is proven by the 10 goldens moving together (all reproduce
cross-toolchain); here we prove the *rules* of the reference hold.
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))
P47 = envmod.load_envelope("p47d")
A6M = envmod.load_envelope("a6m2")


def _two_ship(target_lon_deg, target_alt=4000.0, shooter_alt=4000.0):
    """Shooter (p47d, idx 0) at (0,0) due east; target (a6m2, idx 1) ahead at target_lon_deg."""
    k = rk.Kernel(RAILS)
    sh = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, shooter_alt, 200.0)
    sh.hp = P47["hp_start"]
    tg = rk.Aircraft(0.0, rk.deg2rad(target_lon_deg), rk.deg2rad(90.0), 0.0, target_alt, 150.0)
    tg.hp = A6M["hp_start"]
    k.aircraft.append(sh)
    k.aircraft.append(tg)
    return k, [P47, A6M]


def _run(k, env, ticks, shooter_fires_until):
    for t in range(ticks):
        fire = t < shooter_fires_until
        k.step_scenario([(0.0, 1.0, 0.85, fire), (0.0, 1.0, 0.7, False)], env)


def test_default_is_never_hit():
    # A freshly constructed aircraft has no attacker.
    ac = rk.Aircraft(0.0, 0.0, 0.0, 0.0, 4000.0, 200.0)
    assert ac.last_hit_by == rk.NO_ATTACKER == -1.0


def test_hit_attributes_to_the_firer():
    # A connecting burst tags the target with the shooter's index (0); the shooter, never hit, stays -1.
    k, env = _two_ship(1.5)
    _run(k, env, ticks=120, shooter_fires_until=40)
    assert k.aircraft[1].hp < A6M["hp_start"]        # target took damage
    assert k.aircraft[1].last_hit_by == 0.0          # attributed to the p47d (idx 0)
    assert k.aircraft[0].last_hit_by == rk.NO_ATTACKER   # shooter never hit


def test_no_hit_no_attribution():
    # The altitude gate rejects every pass -> the target is never hit and keeps NO_ATTACKER.
    k, env = _two_ship(1.5, target_alt=6000.0, shooter_alt=4000.0)
    _run(k, env, ticks=200, shooter_fires_until=40)
    assert k.aircraft[1].hp == A6M["hp_start"]        # never damaged
    assert k.aircraft[1].last_hit_by == rk.NO_ATTACKER


def test_attribution_persists_through_death():
    # After the kill, the corpse keeps naming its killer (attribution is not cleared on death).
    k, env = _two_ship(1.5)
    killer_at_death = None
    for t in range(200):
        k.step_scenario([(0.0, 1.0, 0.85, t < 40), (0.0, 1.0, 0.7, False)], env)
        if killer_at_death is None and k.aircraft[1].hp <= 0.0:
            killer_at_death = k.aircraft[1].last_hit_by
    assert k.aircraft[1].hp == 0.0
    assert killer_at_death == 0.0                     # the p47d landed the killing blow
    assert k.aircraft[1].last_hit_by == 0.0           # still names the killer at run end


def test_attribution_is_the_11th_per_aircraft_snapshot_f64():
    # Decode the canonical snapshot and confirm last_hit_by is the 11th per-aircraft double, after
    # [lat, lon, psi, phi, alt, tas, gamma, hp, fire_cd, ammo].
    k, env = _two_ship(1.5)
    _run(k, env, ticks=120, shooter_fires_until=40)
    snap = k.snapshot(120)
    off = 32                                          # header HHIddII = 32 bytes
    for i, ac in enumerate(k.aircraft):
        fields = struct.unpack_from("<11d", snap, off)
        assert fields[10] == ac.last_hit_by           # 11th f64 == last_hit_by
        off += 88                                     # 11 doubles per aircraft
    # target's decoded attribution names the shooter
    tgt = struct.unpack_from("<11d", snap, 32 + 88)
    assert tgt[10] == 0.0


def test_golden_hit_scenario_records_the_kill():
    # End-to-end tie to GOLDEN-SK-Hit-001: the a6m2 is killed AND its attribution names the p47d (0).
    scen = json.loads((ROOT / "config" / "scenarios" / "GOLDEN-SK-Hit-001.json").read_text(encoding="utf-8"))
    k, ticks, gid, sched, env = rk.build_scenario(RAILS, scen)
    rk.run_scenario(k, ticks, sched, env)
    assert k.aircraft[1].hp == 0.0                    # the a6m2 is dead
    assert k.aircraft[1].last_hit_by == 0.0           # killed by aircraft 0 (the p47d)
    assert k.aircraft[0].last_hit_by == rk.NO_ATTACKER    # the p47d was never hit


def test_attribution_is_deterministic():
    # Same engagement -> identical attribution (no RNG / wall-clock leak).
    def play():
        k, env = _two_ship(1.5)
        _run(k, env, ticks=200, shooter_fires_until=40)
        return tuple(ac.last_hit_by for ac in k.aircraft)
    assert play() == play()
