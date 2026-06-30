"""Metamorphic properties for G2 hit detection + per-aircraft hitpoints (Step 7 guns, v1.10r0).

Bit-exact C++<->reference parity for the kill kernel is proven by GOLDEN-SK-Hit-001 (a 4-hit gun
kill reproduced cross-toolchain) and by the canonical snapshot now carrying hp on every aircraft.
Here we prove the *rules* of the reference hold: a round within the hit cylinder of an ALIVE enemy
deals damage and despawns; HP accumulates damage and a burst kills; a dead aircraft freezes, can't
fire, and stops absorbing rounds; the firer is immune to its own rounds; the altitude gate blocks
vertically-separated rounds; and the whole thing is deterministic.
"""
import json
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))


def _two_ship(target_lon_deg, target_alt=4000.0, shooter_alt=4000.0, target_owner_test=False):
    """Shooter (p47d) at (0,0) due east; target (a6m2) ahead at target_lon_deg. Returns (kernel,
    p47d_env, a6m2_env). The shooter fires via step_scenario commands the caller drives."""
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, shooter_alt, 200.0))   # shooter
    k.aircraft.append(rk.Aircraft(0.0, rk.deg2rad(target_lon_deg), rk.deg2rad(90.0), 0.0,
                                  target_alt, 150.0))                                       # target
    return k, [envmod.load_envelope("p47d"), envmod.load_envelope("a6m2")]


def _run(k, env, ticks, shooter_fires_until):
    """Drive the two-ship: shooter fires (fire=True) while tick < shooter_fires_until; neither banks."""
    for t in range(ticks):
        fire = t < shooter_fires_until
        cmds = [(0.0, 1.0, 0.85, fire), (0.0, 1.0, 0.7, False)]
        k.step_scenario(cmds, env)


def test_unobstructed_round_eventually_hits_and_damages():
    # A co-altitude tail-chase burst connects: the target loses HP (at least one hit lands).
    k, env = _two_ship(1.5)
    _run(k, env, ticks=120, shooter_fires_until=20)
    assert k.aircraft[1].hp < rk.START_HP            # target took damage
    assert k.aircraft[0].hp == rk.START_HP           # shooter untouched (no self-hit)


def test_burst_kills_target_and_it_freezes():
    # Enough rounds drive HP to exactly 0 (clamped, never negative); the dead target then FREEZES —
    # its position stops changing once dead.
    k, env = _two_ship(1.5)
    # run until death, capture the kill position, then keep running and confirm it doesn't move
    pos_at_death = None
    for t in range(200):
        fire = t < 20
        k.step_scenario([(0.0, 1.0, 0.85, fire), (0.0, 1.0, 0.7, False)], env)
        if pos_at_death is None and k.aircraft[1].hp <= 0.0:
            pos_at_death = (k.aircraft[1].lat, k.aircraft[1].lon, k.aircraft[1].alt, k.aircraft[1].tas)
    assert pos_at_death is not None                  # the target died
    assert k.aircraft[1].hp == 0.0                   # clamped at 0, not negative
    frozen = (k.aircraft[1].lat, k.aircraft[1].lon, k.aircraft[1].alt, k.aircraft[1].tas)
    assert frozen == pos_at_death                    # dead => no further integration (frozen)


def test_hit_count_matches_hp_drop():
    # HP lost is a whole multiple of DAMAGE_PER_ROUND (each hit is one discrete round of damage),
    # and the target is killed in exactly ceil(START_HP/DAMAGE) hits' worth of damage.
    k, env = _two_ship(1.5)
    _run(k, env, ticks=200, shooter_fires_until=20)
    lost = rk.START_HP - k.aircraft[1].hp
    n_hits = round(lost / rk.DAMAGE_PER_ROUND)
    assert abs(lost - n_hits * rk.DAMAGE_PER_ROUND) < 1e-9
    assert k.aircraft[1].hp == 0.0                   # a 20-round burst overkills the 4-hit target


def test_altitude_gate_blocks_vertically_separated_rounds():
    # Same horizontal track but the target sits far above the rounds (which fly from the shooter's
    # altitude): the |Δalt| gate rejects every pass, so the target takes NO damage.
    k, env = _two_ship(1.5, target_alt=6000.0, shooter_alt=4000.0)
    _run(k, env, ticks=200, shooter_fires_until=20)
    assert k.aircraft[1].hp == rk.START_HP           # 2 km above the bullet stream => never hit


def test_no_self_hit():
    # A lone aircraft firing forward never damages itself (owner == j is excluded), even though its
    # own rounds briefly share its position at the muzzle.
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0))
    env = [envmod.load_envelope("p47d")]
    for t in range(120):
        k.step_scenario([(0.0, 1.0, 0.85, t < 30)], env)
    assert k.aircraft[0].hp == rk.START_HP


def test_dead_target_stops_absorbing_rounds():
    # After the kill, surviving rounds of the burst pass THROUGH the corpse (a dead aircraft is
    # excluded from hit tests): there are still live projectiles downrange at the end.
    k, env = _two_ship(1.5)
    _run(k, env, ticks=200, shooter_fires_until=20)
    assert k.aircraft[1].hp == 0.0
    assert len(k.projectiles) > 0                    # spent rounds flew past the dead target


def test_kill_is_deterministic():
    # Same engagement -> identical outcome (HP, positions, projectile set): no RNG/wall-clock leak.
    def play():
        k, env = _two_ship(1.5)
        _run(k, env, ticks=200, shooter_fires_until=20)
        return (k.aircraft[1].hp,
                tuple((p.lat, p.lon, p.alt, p.tas, p.gamma, p.ttl, p.owner) for p in k.projectiles))
    assert play() == play()
