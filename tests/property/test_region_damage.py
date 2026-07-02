"""Metamorphic properties for region damage + the kill tally (ATM-Sphere v1.18r0).

Each airframe carries ENGINE/WING/TAIL sub-pools (fixed exact-binary fractions of starting hp)
beside the total hp; a connecting round drains the region chosen purely by its APPROACH ASPECT
(wrap_pi(round psi - target psi): astern < pi/4 -> TAIL, head-on > 3pi/4 -> ENGINE, else WING).
A dead region degrades a LIVING aircraft (engine out -> T=0, wing out -> n_aero halved, tail out
-> commands overridden to a straight 1-g mush) and `kills` tallies +1 on the attacker per killing
round. All four are canonical hashed state (12th-15th per-aircraft snapshot f64s), and the effects
are chosen so a straight-and-level 1-g target flies identically through tail-out — the additive
strip-proof the reseal relies on. Bit-exact C++<->reference parity is gated by the 11 goldens.
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


def _two_ship(target_psi_deg=90.0):
    """Shooter (p47d, idx 0) at (0,0) due east; target (a6m2, idx 1) ahead heading target_psi."""
    k = rk.Kernel(RAILS)
    sh = rk.Aircraft(0.0, 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0, hp=P47["hp_start"])
    tg = rk.Aircraft(0.0, rk.deg2rad(1.5), rk.deg2rad(target_psi_deg), 0.0, 4000.0, 150.0,
                     hp=A6M["hp_start"])
    k.aircraft.append(sh)
    k.aircraft.append(tg)
    return k, [P47, A6M]


def _inject_round(k, target, damage, owner=0, psi_deg=90.0):
    """Plant a live round ON the target flying along psi_deg so it connects on the next step."""
    k.projectiles.append(rk.Projectile(
        lat=target.lat, lon=target.lon, psi=rk.deg2rad(psi_deg), alt=target.alt,
        tas=600.0, gamma=0.0, damage=damage, ttl=100, owner=owner))


def _quiet_cmds(n):
    return [(0.0, 1.0, 0.5, False)] * n


def test_pools_derive_from_hp_start():
    # Region sub-pools size from the starting hp via the exact global fractions, per airframe.
    for env in (P47, A6M):
        a = rk.Aircraft(0.0, 0.0, 0.0, 0.0, 4000.0, 200.0, hp=env["hp_start"])
        assert a.engine_hp == rk.ENGINE_FRAC * env["hp_start"]
        assert a.wing_hp == rk.WING_FRAC * env["hp_start"]
        assert a.tail_hp == rk.TAIL_FRAC * env["hp_start"]
        assert a.kills == 0.0
    # and the golden numbers are exact in binary (integer hp x power-of-two-sum fractions)
    assert (rk.ENGINE_FRAC * 70.0, rk.WING_FRAC * 70.0, rk.TAIL_FRAC * 70.0) == (26.25, 35.0, 17.5)


def test_aspect_selects_the_region():
    # Astern round (same heading as target) -> TAIL; head-on (opposite) -> ENGINE; beam -> WING.
    for psi_deg, want in ((90.0, "tail"), (270.0, "engine"), (0.0, "wing"), (180.0, "wing")):
        k, env = _two_ship(target_psi_deg=90.0)
        tg = k.aircraft[1]
        eng0, wng0, tal0 = tg.engine_hp, tg.wing_hp, tg.tail_hp
        _inject_round(k, tg, damage=12.0, psi_deg=psi_deg)
        k.step_scenario(_quiet_cmds(2), env)
        (h,) = k.hit_events
        drained = {"engine": tg.engine_hp < eng0, "wing": tg.wing_hp < wng0,
                   "tail": tg.tail_hp < tal0}
        assert drained.pop(want), f"psi={psi_deg}: expected the {want} pool to drain"
        assert not any(drained.values()), f"psi={psi_deg}: only the {want} pool may drain"
        assert h.region == {"engine": rk.REGION_ENGINE, "wing": rk.REGION_WING,
                            "tail": rk.REGION_TAIL}[want]


def test_damage_double_books_and_pool_clamps_at_zero():
    # A hit drains total hp AND the struck region; the pool clamps at 0 while hp keeps falling
    # (pass-through damage on an already-dead region).
    k, env = _two_ship()
    tg = k.aircraft[1]
    tail0 = tg.tail_hp                       # 17.5
    for i in range(3):                       # 3 x 12 astern: tail 17.5 -> 5.5 -> 0 (clamp) -> 0
        _inject_round(k, tg, damage=12.0)
        k.step_scenario(_quiet_cmds(2), env)
    assert tg.tail_hp == 0.0
    assert tg.hp == A6M["hp_start"] - 36.0   # total hp took every point
    assert tg.engine_hp == rk.ENGINE_FRAC * A6M["hp_start"]   # untouched pools stay full
    assert tg.wing_hp == rk.WING_FRAC * A6M["hp_start"]
    assert tail0 - tg.tail_hp == 17.5        # the pool lost only what it had


def test_engine_out_kills_thrust():
    # Same aircraft, same throttle: engine-out decelerates where healthy holds/accelerates.
    def run(engine_dead):
        k, env = _two_ship()
        tg = k.aircraft[1]
        if engine_dead:
            tg.engine_hp = 0.0
        for _ in range(300):
            k.step_scenario([(0.0, 1.0, 0.85, False), (0.0, 1.0, 0.7, False)], env)
        return tg.tas
    assert run(True) < 150.0 - 5.0 < run(False)


def test_wing_out_halves_the_aero_ceiling():
    # A wing-out aircraft commanding max g achieves a materially lower flight-path pull. The
    # halving must BIND, so fly slow: at 80 m/s the a6m2's n_aero (~4.4) is under its structural
    # limit (7), and half a wing caps the pull near 2.2 g instead.
    def run(wing_dead):
        k, env = _two_ship()
        tg = k.aircraft[1]
        tg.tas = 80.0
        if wing_dead:
            tg.wing_hp = 0.0
        for _ in range(5):
            k.step_scenario([(0.0, 1.0, 0.85, False), (0.0, 6.0, 0.7, False)], env)
        return tg.gamma
    assert 0.0 < run(True) < 0.6 * run(False)   # halved n_aero -> roughly halved (n - cos g) pull


def test_tail_out_strips_control_authority():
    # A tail-out aircraft ignores bank/g commands (straight 1-g mush): phi never leaves 0 and the
    # heading holds; a healthy one rolls into the commanded turn.
    def run(tail_dead):
        k, env = _two_ship()
        tg = k.aircraft[1]
        if tail_dead:
            tg.tail_hp = 0.0
        for _ in range(200):
            k.step_scenario([(0.0, 1.0, 0.85, False),
                             (rk.deg2rad(40.0), 1.4, 0.7, False)], env)
        return tg
    dead, alive = run(True), run(False)
    assert dead.phi == 0.0 and abs(dead.psi - rk.deg2rad(90.0)) < 1e-9
    assert alive.phi > rk.deg2rad(30.0) and alive.psi != dead.psi


def test_tail_out_is_a_noop_for_a_straight_level_target():
    # THE strip-proof property: a target already flying (bank 0, 1 g) is bit-identical through
    # tail-out — this is why GOLDEN-SK-Hit-001 / SESSION-SK-001 trajectories didn't move.
    def run(tail_dead):
        k, env = _two_ship()
        tg = k.aircraft[1]
        if tail_dead:
            tg.tail_hp = 0.0
        out = []
        for _ in range(200):
            k.step_scenario([(0.0, 1.0, 0.85, False), (0.0, 1.0, 0.7, False)], env)
            out.append((tg.lat, tg.lon, tg.psi, tg.phi, tg.alt, tg.tas, tg.gamma))
        return out
    assert run(True) == run(False)


def test_kills_tallies_exactly_the_killing_round():
    # Non-killing hits never increment; the crossing round increments the ATTACKER exactly once;
    # a corpse can't be re-killed.
    k, env = _two_ship()
    tg = k.aircraft[1]
    sh = k.aircraft[0]
    tg.hp = 20.0
    _inject_round(k, tg, damage=12.0)        # 20 -> 8, no kill
    k.step_scenario(_quiet_cmds(2), env)
    assert sh.kills == 0.0
    _inject_round(k, tg, damage=12.0)        # 8 -> 0, the kill
    k.step_scenario(_quiet_cmds(2), env)
    assert sh.kills == 1.0
    (h,) = k.hit_events
    assert h.killed == 1
    _inject_round(k, tg, damage=12.0)        # corpse: no hit, no tally
    k.step_scenario(_quiet_cmds(2), env)
    assert sh.kills == 1.0 and k.hit_events == []


def test_shared_kill_credits_only_the_crossing_shooter():
    # Two same-tick rounds from two shooters: only the one whose round crossed hp>0 -> <=0 tallies.
    k, env = _two_ship()
    third = rk.Aircraft(rk.deg2rad(0.4), 0.0, rk.deg2rad(90.0), 0.0, 4000.0, 200.0,
                        hp=P47["hp_start"])
    k.aircraft.append(third)
    env3 = env + [P47]
    tg = k.aircraft[1]
    tg.hp = 20.0
    _inject_round(k, tg, damage=12.0, owner=0)   # 20 -> 8 (array order first)
    _inject_round(k, tg, damage=12.0, owner=2)   # 8 -> 0: the crossing round
    k.step_scenario(_quiet_cmds(3), env3)
    assert k.aircraft[0].kills == 0.0 and k.aircraft[2].kills == 1.0
    a, b = k.hit_events
    assert (a.killed, b.killed) == (0, 1)


def test_new_state_is_snapshot_fields_12_to_15():
    # The 4 new f64s are canonical hashed state, appended after last_hit_by; stripping them
    # reproduces the v1.17r0 per-aircraft layout (the additive reseal proof, in miniature).
    k, env = _two_ship()
    _inject_round(k, k.aircraft[1], damage=12.0)
    k.step_scenario(_quiet_cmds(2), env)
    snap = k.snapshot(1)
    assert len(snap) == 32 + 120 * len(k.aircraft) + 8 + 64 * len(k.projectiles)
    for i, ac in enumerate(k.aircraft):
        f = struct.unpack_from("<15d", snap, 32 + 120 * i)
        assert f[11:15] == (ac.engine_hp, ac.wing_hp, ac.tail_hp, ac.kills)
        # the first 11 are exactly the v1.17r0 record
        assert f[:11] == (ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma,
                          ac.hp, ac.fire_cd, ac.ammo, ac.last_hit_by)


def test_region_damage_is_deterministic():
    # Same engagement -> identical region pools, tallies, and event stream.
    def play():
        k, env = _two_ship()
        out = []
        for t in range(200):
            k.step_scenario([(0.0, 1.0, 0.85, t < 40), (0.0, 1.0, 0.7, False)], env)
            out += [(t, h.target, h.attacker, h.region, h.killed) for h in k.hit_events]
        tg = k.aircraft[1]
        return out, (tg.engine_hp, tg.wing_hp, tg.tail_hp), k.aircraft[0].kills
    a, b = play(), play()
    assert a == b and len(a[0]) > 0
