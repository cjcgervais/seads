"""Metamorphic properties for G1 ballistic projectiles (Step 7 guns, ATM-Sphere v1.9r0).

Bit-exact C++<->reference parity for the projectile kernel is proven by GOLDEN-SK-Gunfire-001
(35 live rounds in the world_hash, reproduced cross-toolchain) and by the canonical snapshot now
carrying the projectile block in every golden. Here we prove the *physics* of the reference round
holds: it is the n=0/thrust=0 specialization of the aircraft 3-DOF step, so gravity bends its
flight-path angle down, drag bleeds its speed, it flies forward along the geodesic, and it despawns
on time-to-live OR on hitting the ground — and the whole thing is deterministic.
"""
import json
import math
import sys
from pathlib import Path

from hypothesis import given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import envelopes as envmod       # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))


def _spawn_and_coast(gamma_deg, tas0, ticks, alt0=4000.0, psi_deg=90.0, lat_deg=0.0):
    """Spawn one round directly and advance it `ticks` times (no aircraft, no new fire). Returns a
    trace of (lat, lon, alt, tas, gamma, ttl) while the round is alive, ending with None once it
    despawns. White-box over the reference (the reference defines the golden)."""
    k = rk.Kernel(RAILS)
    k.projectiles.append(rk.Projectile(
        lat=rk.deg2rad(lat_deg), lon=0.0, psi=rk.deg2rad(psi_deg), alt=alt0,
        tas=tas0, gamma=rk.deg2rad(gamma_deg), damage=10.0, ttl=rk.PROJ_TTL_TICKS, owner=0))
    p = k.projectiles[0]
    trace = [(p.lat, p.lon, p.alt, p.tas, p.gamma, p.ttl)]
    for _ in range(ticks):
        k._advance_projectiles()
        if not k.projectiles:
            trace.append(None)
            break
        q = k.projectiles[0]
        trace.append((q.lat, q.lon, q.alt, q.tas, q.gamma, q.ttl))
    return trace


@given(tas0=st.floats(min_value=400.0, max_value=1100.0))
@settings(max_examples=20, deadline=None)
def test_level_round_drag_bleeds_speed(tas0):
    # Fired level (gamma=0): gravity-along-path starts at zero and stays tiny, so drag (k*V^2)
    # dominates and TAS is monotonically non-increasing, ending meaningfully slower (never < V_MIN).
    tr = [s for s in _spawn_and_coast(0.0, tas0, ticks=200) if s is not None]
    tas = [s[3] for s in tr]
    for a, b in zip(tas, tas[1:]):
        assert b <= a + 1e-12
        assert b >= rk.V_MIN - 1e-9
    assert tas[-1] < tas[0] - 10.0


@given(tas0=st.floats(min_value=400.0, max_value=1100.0))
@settings(max_examples=20, deadline=None)
def test_gravity_pulls_the_round_down(tas0):
    # Fired level, the round arcs: flight-path angle goes negative and altitude drops below launch.
    tr = [s for s in _spawn_and_coast(0.0, tas0, ticks=200, alt0=5000.0) if s is not None]
    assert tr[-1][4] < 0.0                    # gamma bent below the horizon
    assert tr[-1][2] < tr[0][2]               # net altitude lost
    # the descent is monotone after launch (gamma only decreases under pure gravity + drag)
    gammas = [s[4] for s in tr]
    for a, b in zip(gammas, gammas[1:]):
        assert b <= a + 1e-12


@given(gamma=st.floats(min_value=-20.0, max_value=20.0), tas0=st.floats(min_value=400.0, max_value=1000.0))
@settings(max_examples=30, deadline=None)
def test_round_flies_forward_downrange(gamma, tas0):
    # An eastbound round (psi=90, start lon=0) advances east every tick while airborne: ground
    # speed V*cos(gamma) > 0, so longitude is strictly increasing (it never flies backwards).
    tr = [s for s in _spawn_and_coast(gamma, tas0, ticks=120) if s is not None]
    lons = [s[1] for s in tr]
    for a, b in zip(lons, lons[1:]):
        assert b > a


def test_ttl_despawn_is_exact():
    # A round lives for exactly PROJ_TTL_TICKS advances, then despawns: count==1 after ttl-1
    # advances, count==0 after ttl. Use a shallow climb so it never hits the ground first.
    ttl = rk.PROJ_TTL_TICKS
    tr = _spawn_and_coast(2.0, 800.0, ticks=ttl, alt0=6000.0)
    assert tr[ttl - 1] is not None            # still alive one tick before expiry
    assert tr[ttl] is None                    # despawned exactly at ttl


def test_ground_hit_despawns_before_ttl():
    # Fired steeply down from low altitude, the round hits the ground (alt 0) and despawns well
    # before its time-to-live elapses.
    tr = _spawn_and_coast(-45.0, 600.0, ticks=rk.PROJ_TTL_TICKS, alt0=120.0)
    end = next(i for i, s in enumerate(tr) if s is None)
    assert end < rk.PROJ_TTL_TICKS            # gone early (ground), not by ttl
    # last LIVE state was diving just above the surface (the grounded alt=0 step is despawned, not
    # recorded, so the final live altitude is one descent-step up from the ground, not exactly 0).
    assert tr[end - 1][4] < 0.0               # was diving
    assert tr[end - 1][2] < 20.0             # within one descent step of the ground


@given(gamma=st.floats(min_value=-30.0, max_value=30.0))
@settings(max_examples=12, deadline=None)
def test_faster_round_has_more_range_per_tick(gamma):
    # At the same launch angle, a faster round covers more downrange in the first tick (ground
    # advance s = V*cos(gamma)*dt grows with V) — the basis of gun lead/convergence.
    slow = _spawn_and_coast(gamma, 500.0, ticks=1)
    fast = _spawn_and_coast(gamma, 900.0, ticks=1)
    assert fast[1][1] > slow[1][1]            # fast round's longitude advanced further


def test_projectile_is_deterministic():
    # Same launch -> identical trace (no wall-clock / RNG / address leakage into the round).
    a = _spawn_and_coast(5.0, 950.0, ticks=200)
    b = _spawn_and_coast(5.0, 950.0, ticks=200)
    assert a == b


def test_fire_command_spawns_at_post_step_muzzle():
    # Driving step_scenario with fire=True spawns exactly one round (fire_cd starts ready), at the
    # firer's POST-step state (it does not advance on its spawn tick), with speed = firer TAS + the
    # firer's per-airframe muzzle velocity, carrying the firer's per-round damage and owner index.
    # This pins the end-to-end G1/G3 spawn path (Command.fire -> _spawn_projectile).
    k = rk.Kernel(RAILS)
    ac = rk.Aircraft(0.0, 0.0, rk.deg2rad(30.0), 0.0, 4000.0, 200.0, rk.deg2rad(3.0))
    k.aircraft.append(ac)
    env = envmod.load_envelope("p47d")
    k.step_scenario([(0.0, 1.2, 0.8, True)], [env])
    assert len(k.projectiles) == 1
    p = k.projectiles[0]
    assert p.owner == 0
    assert p.ttl == rk.PROJ_TTL_TICKS
    # muzzle == the firer's post-step kinematics (the aircraft moved this tick; the round did not)
    assert (p.lat, p.lon, p.psi, p.alt, p.gamma) == (ac.lat, ac.lon, ac.psi, ac.alt, ac.gamma)
    assert p.tas == ac.tas + env["muzzle_v_mps"]        # per-airframe muzzle (G3)
    assert p.damage == env["damage_per_round"]          # carried per-round damage (G3)


def test_no_fire_command_spawns_nothing():
    # Sanity: a fire=False (and a legacy 3-tuple) command never spawns a round.
    k = rk.Kernel(RAILS)
    k.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, 4000.0, 200.0))
    env = envmod.load_envelope("p51")
    k.step_scenario([(0.0, 1.0, 0.8, False)], [env])
    k.step_scenario([(0.0, 1.0, 0.8)], [env])         # legacy 3-tuple, no fire field
    assert len(k.projectiles) == 0
