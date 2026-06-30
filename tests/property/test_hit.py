"""Metamorphic properties for G2 hit detection + G3 per-airframe weapon roster (Step 7 guns).

Bit-exact C++<->reference parity for the kill kernel is proven by GOLDEN-SK-Hit-001 (a gun kill
reproduced cross-toolchain) and by the canonical snapshot carrying hp + fire_cd on every aircraft.
Here we prove the *rules* of the reference hold: a round within the hit cylinder of an ALIVE enemy
deals its (per-airframe) damage and despawns; HP accumulates damage and a burst kills; a dead
aircraft freezes, can't fire, and stops absorbing rounds; the firer is immune to its own rounds;
the altitude gate blocks vertically-separated rounds; firing is gated by the per-airframe fire-rate;
and the whole thing is deterministic.
"""
import json
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
    """Shooter (p47d) at (0,0) due east; target (a6m2) ahead at target_lon_deg. HP is set from each
    airframe's envelope (p47d 150, a6m2 70), like build_scenario does."""
    k = rk.Kernel(RAILS)
    sh = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, shooter_alt, 200.0)
    sh.hp = P47["hp_start"]
    tg = rk.Aircraft(0.0, rk.deg2rad(target_lon_deg), rk.deg2rad(90.0), 0.0, target_alt, 150.0)
    tg.hp = A6M["hp_start"]
    k.aircraft.append(sh)
    k.aircraft.append(tg)
    return k, [P47, A6M]


def _run(k, env, ticks, shooter_fires_until):
    """Drive the two-ship: shooter fires while tick < shooter_fires_until (the kernel's fire-rate
    cooldown gates the actual rounds); neither banks; target never fires."""
    for t in range(ticks):
        fire = t < shooter_fires_until
        k.step_scenario([(0.0, 1.0, 0.85, fire), (0.0, 1.0, 0.7, False)], env)


def test_unobstructed_round_eventually_hits_and_damages():
    # A co-altitude tail-chase burst connects: the target loses HP; the shooter is never self-hit.
    k, env = _two_ship(1.5)
    _run(k, env, ticks=120, shooter_fires_until=40)
    assert k.aircraft[1].hp < A6M["hp_start"]         # target took damage
    assert k.aircraft[0].hp == P47["hp_start"]        # shooter untouched (no self-hit)


def test_burst_kills_target_and_it_freezes():
    # Enough rounds drive HP to exactly 0 (clamped, never negative); the dead target then FREEZES.
    k, env = _two_ship(1.5)
    pos_at_death = None
    for t in range(200):
        fire = t < 40
        k.step_scenario([(0.0, 1.0, 0.85, fire), (0.0, 1.0, 0.7, False)], env)
        if pos_at_death is None and k.aircraft[1].hp <= 0.0:
            pos_at_death = (k.aircraft[1].lat, k.aircraft[1].lon, k.aircraft[1].alt, k.aircraft[1].tas)
    assert pos_at_death is not None                   # the target died
    assert k.aircraft[1].hp == 0.0                    # clamped at 0, not negative
    frozen = (k.aircraft[1].lat, k.aircraft[1].lon, k.aircraft[1].alt, k.aircraft[1].tas)
    assert frozen == pos_at_death                     # dead => no further integration (frozen)


def test_hit_count_matches_per_airframe_damage():
    # While the target SURVIVES, HP lost is an exact whole multiple of the shooter's per-round damage
    # (p47d 12) — each hit is one discrete round. Use a very tough target so the final hit never
    # clamps (a kill would lose the overflow and break the exact-multiple property).
    k, env = _two_ship(1.5)
    k.aircraft[1].hp = 100000.0                        # effectively invulnerable -> never clamps
    _run(k, env, ticks=200, shooter_fires_until=40)
    lost = 100000.0 - k.aircraft[1].hp
    assert lost > 0.0                                  # it was hit
    n_hits = round(lost / P47["damage_per_round"])
    assert abs(lost - n_hits * P47["damage_per_round"]) < 1e-9
    assert k.aircraft[1].hp > 0.0                      # survived (so no clamp)


def test_altitude_gate_blocks_vertically_separated_rounds():
    # Same horizontal track but the target sits 2 km above the bullet stream: the |Δalt| gate
    # rejects every pass, so the target takes NO damage.
    k, env = _two_ship(1.5, target_alt=6000.0, shooter_alt=4000.0)
    _run(k, env, ticks=200, shooter_fires_until=40)
    assert k.aircraft[1].hp == A6M["hp_start"]         # never hit


def test_no_self_hit():
    # A lone aircraft firing forward never damages itself (owner == j excluded).
    k = rk.Kernel(RAILS)
    ac = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0)
    ac.hp = P47["hp_start"]
    k.aircraft.append(ac)
    for t in range(120):
        k.step_scenario([(0.0, 1.0, 0.85, t < 40)], [P47])
    assert k.aircraft[0].hp == P47["hp_start"]


def test_dead_target_stops_absorbing_rounds():
    # After the kill, surviving rounds of the burst pass THROUGH the corpse (dead = excluded from
    # hit tests): there is still at least one live projectile downrange at the end.
    k, env = _two_ship(1.5)
    _run(k, env, ticks=200, shooter_fires_until=40)
    assert k.aircraft[1].hp == 0.0
    assert len(k.projectiles) > 0


def test_fire_rate_gates_round_count():
    # The per-airframe fire-rate cooldown limits rounds: holding fire for N ticks yields about
    # N / rof_interval rounds, far fewer than 1-per-tick. p47d rof_interval=3 over 30 ticks => ~10.
    k = rk.Kernel(RAILS)
    ac = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0)
    ac.hp = P47["hp_start"]
    k.aircraft.append(ac)
    spawned = 0
    prev = 0
    for t in range(30):
        k.step_scenario([(0.0, 1.0, 0.85, True)], [P47])
        # count cumulative spawns (rounds only leave, never return, in a lone flight upward... they
        # fly away; ttl is long so none despawn in 30 ticks) -> live count == spawned count
    rof = P47["rof_interval_ticks"]
    expected = 30 // int(rof)
    # allow +/-1 for the boundary tick
    assert abs(len(k.projectiles) - expected) <= 1
    assert len(k.projectiles) < 30                     # definitely rate-limited (not 1/tick)


def test_a6m2_hits_harder_per_round_than_p47d():
    # Per-airframe lethality: one A6M2 20mm round removes more HP than one P-47D .50cal round.
    assert A6M["damage_per_round"] > P47["damage_per_round"]
    # ...but the A6M2 is more fragile and fires slower (the glass-cannon trade).
    assert A6M["hp_start"] < P47["hp_start"]
    assert A6M["rof_interval_ticks"] > P47["rof_interval_ticks"]


def test_kill_is_deterministic():
    # Same engagement -> identical outcome (HP, positions, projectile set): no RNG/wall-clock leak.
    def play():
        k, env = _two_ship(1.5)
        _run(k, env, ticks=200, shooter_fires_until=40)
        return (k.aircraft[1].hp,
                tuple((p.lat, p.lon, p.alt, p.tas, p.gamma, p.damage, p.ttl, p.owner)
                      for p in k.projectiles))
    assert play() == play()
