"""Metamorphic properties for the kernel per-round hit event queue (rides ATM-Sphere v1.17r0).

The kernel appends one HitEvent per CONNECTING ROUND at hit time (_advance_projectiles, projectile
array order) into `Kernel.hit_events` — the per-round upgrade of the last-writer `last_hit_by`
field. The queue is OBSERVABLE OUTPUT, not canonical state: cleared at the top of every step, never
serialized into snapshot(), so the world_hash and all goldens are untouched (the receipt's golden
gates prove that side). Here we prove the *rules* of the queue itself, including the one thing the
old hp-delta observation could not represent: two rounds striking one target on one tick are two
distinct, separately-attributed events. Bit-exact C++<->reference parity for the event-channel
consumer is gated by seads_event_test's EVENT-MULTIHIT-001 vector.
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


def _two_ship(target_lon_deg=1.5, target_alt=4000.0, shooter_alt=4000.0):
    """Shooter (p47d, idx 0) at (0,0) due east; target (a6m2, idx 1) ahead at target_lon_deg."""
    k = rk.Kernel(RAILS)
    sh = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, shooter_alt, 200.0)
    sh.hp = P47["hp_start"]
    tg = rk.Aircraft(0.0, rk.deg2rad(target_lon_deg), rk.deg2rad(90.0), 0.0, target_alt, 150.0)
    tg.hp = A6M["hp_start"]
    k.aircraft.append(sh)
    k.aircraft.append(tg)
    return k, [P47, A6M]


def _inject_round(k, target, damage, owner=0, lat=None, lon=None, alt=None):
    """Plant a live round essentially ON the target so it connects on the next step (a round moves
    ~7.5 m/tick here, well inside the 60 m hit sphere)."""
    p = rk.Projectile(lat=target.lat if lat is None else lat,
                      lon=target.lon if lon is None else lon,
                      psi=rk.deg2rad(90.0), alt=target.alt if alt is None else alt,
                      tas=600.0, gamma=0.0, damage=damage, ttl=100, owner=owner)
    k.projectiles.append(p)
    return p


def _quiet_cmds(n):
    return [(0.0, 1.0, 0.5, False)] * n


def test_queue_empty_when_nothing_connects():
    # No round in the air -> the queue is empty after every step (scenario and no-arg paths).
    k, env = _two_ship()
    for _ in range(50):
        k.step_scenario(_quiet_cmds(2), env)
        assert k.hit_events == []
    k2 = rk.Kernel(RAILS)
    k2.aircraft.append(rk.Aircraft(0.0, 0.0, 0.0, 0.0, 1000.0, 250.0))
    k2.step()
    assert k2.hit_events == []


def test_queue_holds_current_step_only():
    # A hit's event lives exactly one step: present the tick it lands, gone the next quiet tick.
    k, env = _two_ship()
    _inject_round(k, k.aircraft[1], damage=10.0)
    k.step_scenario(_quiet_cmds(2), env)
    assert len(k.hit_events) == 1
    k.step_scenario(_quiet_cmds(2), env)
    assert k.hit_events == []


def test_event_matches_the_hit():
    # The event records the right target/attacker/damage and the post-clamp hp bookkeeping.
    k, env = _two_ship()
    hp0 = k.aircraft[1].hp
    _inject_round(k, k.aircraft[1], damage=10.0, owner=0)
    k.step_scenario(_quiet_cmds(2), env)
    (h,) = k.hit_events
    assert (h.target, h.attacker, h.damage, h.killed) == (1, 0, 10.0, 0)
    assert h.hp_before == hp0 and h.hp_after == k.aircraft[1].hp == hp0 - 10.0


def test_multi_hit_same_tick_splits_into_per_round_events():
    # THE granularity claim: two rounds from two different shooters connect on one tick -> TWO
    # events with distinct attackers whose effective losses sum to the tick's hp delta (the old
    # hp-delta view lumped this into one event credited to the last writer).
    k, env = _two_ship()
    third = rk.Aircraft(rk.deg2rad(0.4), 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0)  # 2nd shooter
    third.hp = P47["hp_start"]
    k.aircraft.append(third)
    env3 = env + [P47]
    tg = k.aircraft[1]
    hp0 = tg.hp
    _inject_round(k, tg, damage=10.0, owner=0)
    _inject_round(k, tg, damage=12.0, owner=2)
    k.step_scenario(_quiet_cmds(3), env3)
    assert len(k.hit_events) == 2                          # split, not lumped
    a, b = k.hit_events                                    # projectile array order
    assert (a.target, a.attacker, a.damage) == (1, 0, 10.0)
    assert (b.target, b.attacker, b.damage) == (1, 2, 12.0)
    assert a.hp_after == b.hp_before                       # the per-round hp chain is contiguous
    assert (a.hp_before - a.hp_after) + (b.hp_before - b.hp_after) == hp0 - tg.hp
    assert tg.last_hit_by == 2.0                           # the last-writer field keeps only ONE


def test_killed_marks_the_crossing_round_and_overkill_is_clamped():
    # The killed flag sits on exactly the round that crossed hp>0 -> <=0; its effective loss is the
    # remaining hp, not the round's carried damage (post-clamp reality).
    k, env = _two_ship()
    tg = k.aircraft[1]
    tg.hp = 15.0
    _inject_round(k, tg, damage=10.0, owner=0)             # 15 -> 5, alive
    k.step_scenario(_quiet_cmds(2), env)
    (h1,) = k.hit_events
    assert h1.killed == 0 and tg.hp == 5.0
    _inject_round(k, tg, damage=10.0, owner=0)             # 5 -> 0, the kill (overkill by 5)
    k.step_scenario(_quiet_cmds(2), env)
    (h2,) = k.hit_events
    assert h2.killed == 1 and h2.hp_before == 5.0 and h2.hp_after == 0.0
    assert h2.damage == 10.0                               # carried damage, as fired
    assert h2.hp_before - h2.hp_after == 5.0               # effective loss, clamped
    _inject_round(k, tg, damage=10.0, owner=0)             # a corpse can't be hit
    k.step_scenario(_quiet_cmds(2), env)
    assert k.hit_events == [] and tg.hp == 0.0


def test_queue_is_not_hashed_state():
    # The canonical snapshot's size is exactly header + 11 f64/aircraft + projectile block — the
    # queue contributes NOTHING (so the world_hash, and every golden, cannot see it).
    k, env = _two_ship()
    _inject_round(k, k.aircraft[1], damage=10.0)
    k.step_scenario(_quiet_cmds(2), env)
    assert len(k.hit_events) == 1
    snap = k.snapshot(1)
    n_ac, n_pr = len(k.aircraft), len(k.projectiles)
    assert len(snap) == 32 + 88 * n_ac + 8 + 64 * n_pr
    # and the aircraft block still ends at last_hit_by (11 doubles), nothing appended
    fields = struct.unpack_from("<11d", snap, 32 + 88)
    assert fields[10] == k.aircraft[1].last_hit_by


def test_per_round_reduces_to_hp_delta_when_single_hit():
    # Equivalence: when no tick lands two rounds on one target, the per-round stream carries exactly
    # the per-tick hp-delta information (this is why the sealed SESSION-SK-001 EVENT_DIGEST did not
    # move when the event channel switched sources).
    k, env = _two_ship()
    for t in range(200):
        hp_before = [ac.hp for ac in k.aircraft]
        k.step_scenario([(0.0, 1.0, 0.85, t < 40), (0.0, 1.0, 0.7, False)], env)
        deltas = [(i, hp_before[i], ac.hp) for i, ac in enumerate(k.aircraft) if ac.hp < hp_before[i]]
        assert len(k.hit_events) == len(deltas)
        for h, (i, before, after) in zip(k.hit_events, deltas):
            assert (h.target, h.hp_before, h.hp_after) == (i, before, after)
            assert h.killed == (1 if before > 0.0 and after <= 0.0 else 0)


def test_queue_is_deterministic():
    # Same engagement -> identical event stream (no RNG / wall-clock / order leak).
    def play():
        k, env = _two_ship()
        out = []
        for t in range(200):
            k.step_scenario([(0.0, 1.0, 0.85, t < 40), (0.0, 1.0, 0.7, False)], env)
            out += [(t, h.target, h.attacker, h.damage, h.hp_before, h.hp_after, h.killed)
                    for h in k.hit_events]
        return out
    a, b = play(), play()
    assert a == b and len(a) > 0
