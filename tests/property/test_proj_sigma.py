"""Metamorphic properties for projectile sigma-drag (Step 7 guns / B5 closure, ATM-Sphere v1.24r0).

v1.24r0 closes B5's named deferral: a fired round now flies in the SAME thin air as the
airframes — the lumped global PROJ_DRAG_K is scaled by the sealed ISA density ratio at the
round's PRE-step altitude (Vdot = -k*sigma(alt)*V^2 - g0*sin(gamma); one air_sigma per round
per tick, the aircraft step's pre-step sigma convention; ZERO new det_math). These tests pin:
(1) the sea-level identity (sigma(0) = 1.0 exactly => a sea-level round is bit-identical to
the pre-v1.24r0 law), (2) the exact op order through the kernel, (3) the pre-step-altitude
convention, (4) the physics (a high round keeps more speed; first-tick drag scales with
sigma), and (5) the re-measured hit-journal claims of the sealed Hit/EngineOut stories.
"""
import json
import sys
from pathlib import Path

from hypothesis import assume, given, settings, strategies as st

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ref_kernel as rk          # noqa: E402
import detmath_ref as dm         # noqa: E402

RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))


def _one_tick(alt0, tas0, gamma0, psi_deg=90.0):
    """Advance a single directly-spawned round one tick; return (tas, gamma, alt) after."""
    k = rk.Kernel(RAILS)
    k.projectiles.append(rk.Projectile(
        lat=0.0, lon=0.0, psi=rk.deg2rad(psi_deg), alt=alt0,
        tas=tas0, gamma=gamma0, damage=10.0, ttl=rk.PROJ_TTL_TICKS, owner=0))
    k._advance_projectiles()
    assert k.projectiles, "round unexpectedly despawned on its first advance"
    p = k.projectiles[0]
    return p.tas, p.gamma, p.alt


def _hand_advance(alt0, tas0, gamma0, sigma):
    """The v1.24r0 projectile speed/gamma/alt update, op-for-op, with an EXPLICIT sigma."""
    k = rk.Kernel(RAILS)
    dt, g0 = k.dt, k.g0
    V = tas0
    sg = dm.det_sin(gamma0)
    Vdot = -rk.PROJ_DRAG_K * sigma * V * V - g0 * sg
    Vnew = V + Vdot * dt
    if Vnew < rk.V_MIN:
        Vnew = rk.V_MIN
    cg = dm.det_cos(gamma0)
    gdot = (g0 / Vnew) * (-cg)
    ngamma = gamma0 + gdot * dt
    sgN = dm.det_sin(ngamma)
    nalt = alt0 + (Vnew * sgN) * dt
    return Vnew, ngamma, nalt


def test_sigma_one_at_sea_level_reproduces_pre_v124_round():
    # sigma(0) = 1.0 EXACTLY (the first sealed LUT node), and x*1.0 == x in IEEE, so a
    # sea-level round advances bit-identically to the pre-v1.24r0 law -k*V*V - g0*sin(gamma).
    assert rk.air_sigma(0.0) == 1.0
    for tas0 in (420.0, 733.25, 1051.0):
        gamma0 = rk.deg2rad(3.0)             # slight climb so alt stays > 0 (no ground despawn)
        got = _one_tick(0.0, tas0, gamma0)
        k = rk.Kernel(RAILS)
        V, dt, g0 = tas0, k.dt, k.g0
        old_vdot = -rk.PROJ_DRAG_K * V * V - g0 * dm.det_sin(gamma0)   # the pre-v1.24r0 law
        assert got[0] == V + old_vdot * dt   # bit-for-bit


@given(alt0=st.floats(min_value=100.0, max_value=7900.0),
       tas0=st.floats(min_value=400.0, max_value=1100.0),
       gdeg=st.floats(min_value=-20.0, max_value=20.0))
@settings(max_examples=40, deadline=None)
def test_one_tick_advance_replicates_kernel_op_order(alt0, tas0, gdeg):
    # The sealed byte spec: sigma is evaluated from the round's PRE-step altitude and enters
    # the Vdot product exactly as -k*sigma*V*V. Hand-computing that op sequence reproduces the
    # kernel round bit-for-bit.
    gamma0 = rk.deg2rad(gdeg)
    got = _one_tick(alt0, tas0, gamma0)
    want = _hand_advance(alt0, tas0, gamma0, rk.air_sigma(alt0))
    assert got == want


@given(alt0=st.floats(min_value=500.0, max_value=7000.0),
       tas0=st.floats(min_value=500.0, max_value=1100.0))
@settings(max_examples=25, deadline=None)
def test_round_sigma_uses_prestep_alt(alt0, tas0):
    # A steeply climbing round moves ~4 m up within its first tick; the drag it felt must be
    # the PRE-step sigma. Replicating with the post-step sigma diverges (unless the two sigmas
    # happen to collide, which the assume() filters).
    gamma0 = rk.deg2rad(30.0)
    got = _one_tick(alt0, tas0, gamma0)
    pre = _hand_advance(alt0, tas0, gamma0, rk.air_sigma(alt0))
    post_sigma = rk.air_sigma(got[2])
    assume(post_sigma != rk.air_sigma(alt0))
    assert got == pre
    assert got != _hand_advance(alt0, tas0, gamma0, post_sigma)


@given(alt_lo=st.floats(min_value=0.0, max_value=3000.0),
       gap=st.floats(min_value=600.0, max_value=4000.0),
       tas0=st.floats(min_value=400.0, max_value=1100.0))
@settings(max_examples=25, deadline=None)
def test_high_round_keeps_more_speed(alt_lo, gap, tas0):
    # The seal's point: the same round fired level in thinner air bleeds LESS speed — after any
    # number of ticks the high round is strictly faster (sigma is strictly monotone over the band).
    alt_hi = min(alt_lo + gap, 7900.0)
    assume(rk.air_sigma(alt_hi) < rk.air_sigma(alt_lo))
    lo_tas, hi_tas = tas0, tas0
    klo, khi = rk.Kernel(RAILS), rk.Kernel(RAILS)
    for k, alt in ((klo, alt_lo), (khi, alt_hi)):
        k.projectiles.append(rk.Projectile(
            lat=0.0, lon=0.0, psi=rk.deg2rad(90.0), alt=alt,
            tas=tas0, gamma=0.0, damage=10.0, ttl=rk.PROJ_TTL_TICKS, owner=0))
    for _ in range(100):
        klo._advance_projectiles()
        khi._advance_projectiles()
        if not klo.projectiles or not khi.projectiles:
            break
        assert khi.projectiles[0].tas > klo.projectiles[0].tas


@given(alt1=st.floats(min_value=100.0, max_value=7500.0),
       alt2=st.floats(min_value=100.0, max_value=7500.0),
       tas0=st.floats(min_value=400.0, max_value=1100.0))
@settings(max_examples=25, deadline=None)
def test_first_tick_drag_scales_with_sigma(alt1, alt2, tas0):
    # Fired LEVEL (det_sin(0) == 0 => no gravity-along-path term), the first-tick speed loss is
    # purely k*sigma*V^2*dt, so the loss ratio between two altitudes tracks the sigma ratio to
    # rounding error.
    s1, s2 = rk.air_sigma(alt1), rk.air_sigma(alt2)
    assume(s1 != s2)
    loss1 = tas0 - _one_tick(alt1, tas0, 0.0)[0]
    loss2 = tas0 - _one_tick(alt2, tas0, 0.0)[0]
    assert loss1 > 0.0 and loss2 > 0.0
    assert abs(loss1 / loss2 - s1 / s2) < 1e-9 * (s1 / s2)


def _run_journal(gid):
    """Drive a sealed scenario start-to-end; return [(tick, target, damage, hp_after, killed,
    region), ...] — the per-round hit journal the descriptions' tick claims rest on."""
    scen = json.loads((ROOT / "config" / "scenarios" / f"{gid}.json").read_text(encoding="utf-8"))
    k, ticks, _, scheds, envs = rk.build_scenario(RAILS, scen)
    out = []
    for t in range(ticks):
        cmds = []
        for sched in scheds:
            idx = 0
            for j, ph in enumerate(sched):
                if int(ph["start_tick"]) <= t:
                    idx = j
                else:
                    break
            ph = sched[idx]
            cmds.append((rk.deg2rad(ph["bank_deg"]), float(ph["g_cmd"]),
                         float(ph.get("throttle", 0.0)), bool(ph.get("fire", False))))
        k.step_scenario(cmds, envs)
        for ev in k.hit_events:
            out.append((t, ev.target, ev.damage, ev.hp_after, ev.killed, ev.region))
    return k, out


def test_hit_story_ticks_pinned_under_sigma_drag():
    # GOLDEN-SK-Hit-001's re-measured v1.24r0 description claims: six 12-hp TAIL connects at
    # ticks 39/41/44/47/50/53, hp walking 70 -> 0, kill on the 6th at tick 53.
    k, ev = _run_journal("GOLDEN-SK-Hit-001")
    assert [e[0] for e in ev] == [39, 41, 44, 47, 50, 53]
    assert all(e[1] == 1 and e[2] == 12.0 and e[5] == rk.REGION_TAIL for e in ev)
    assert [e[3] for e in ev] == [58.0, 46.0, 34.0, 22.0, 10.0, 0.0]
    assert [e[4] for e in ev] == [0, 0, 0, 0, 0, 1]
    assert k.aircraft[0].kills == 1.0 and k.aircraft[1].hp == 0.0


def test_engineout_story_ticks_pinned_under_sigma_drag():
    # GOLDEN-SK-EngineOut-001's re-measured v1.24r0 description claims: four 12-hp ENGINE
    # connects at ticks 28/31/33/35 (the FIRST pulled a tick earlier by sigma-drag), the pool
    # 35 -> 23 -> 11 -> 0 dying on the 3rd round at tick 33, the aircraft ALIVE at hp 22.
    k, ev = _run_journal("GOLDEN-SK-EngineOut-001")
    assert [e[0] for e in ev] == [28, 31, 33, 35]
    assert all(e[1] == 1 and e[2] == 12.0 and e[5] == rk.REGION_ENGINE for e in ev)
    assert not any(e[4] for e in ev)                       # nobody dies
    a6m2 = k.aircraft[1]
    assert a6m2.hp == 22.0 and a6m2.engine_hp == 0.0       # alive, engine dead
    assert a6m2.wing_hp == 26.25 and a6m2.tail_hp == 17.5  # untouched pools
