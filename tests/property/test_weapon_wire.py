"""Round-trip / framing properties for the WEAPON-001 snapshot section (sealed v1.12r0; the G4
magazine `ammo` joined the block at v1.14r0, protocol 5; attacker attribution `last_hit_by`
joined at v1.17r0, protocol 6; the v1.18r0 region sub-pools engine_hp/wing_hp/tail_hp + the kill
tally `kills` joined at v1.19r0, protocol 7).

Byte-exact C++<->reference parity is proven by the generated-vector gate
(src/net/weapon_test_main.cpp). Here we prove the reference framing is self-consistent: the
gunnery state (per-aircraft hp/fire_cd/ammo/last_hit_by/region pools/kills + the live ballistic
rounds) round-trips within one quantum, ttl/owner are carried EXACTLY, the protocol gates the
section (a protocol-3 frame omits the weapon block entirely; a protocol-4 frame omits ammo; a
protocol-5 frame omits last_hit_by; a protocol-6 frame omits the region pools + kills), and the
section stays self-delimiting + id-aligned."""
import sys
from pathlib import Path

import pytest
from hypothesis import given, strategies as st

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import geo001_ref as g
import snapshot_ref as s

LAT = st.floats(min_value=-90.0, max_value=90.0, allow_nan=False, allow_infinity=False)
LON = st.floats(min_value=-180.0, max_value=180.0, allow_nan=False, allow_infinity=False)
BEAR = st.floats(min_value=0.0, max_value=360.0, allow_nan=False, allow_infinity=False)
ALT = st.floats(min_value=-100.0, max_value=8000.0, allow_nan=False, allow_infinity=False)
PHI = st.floats(min_value=-90.0, max_value=90.0, allow_nan=False, allow_infinity=False)
TAS = st.floats(min_value=0.0, max_value=400.0, allow_nan=False, allow_infinity=False)
GAMMA = st.floats(min_value=-89.0, max_value=89.0, allow_nan=False, allow_infinity=False)
HP = st.floats(min_value=-50.0, max_value=200.0, allow_nan=False, allow_infinity=False)  # neg = exercise ZigZag sign on a corpse frame
FIRECD = st.floats(min_value=0.0, max_value=30.0, allow_nan=False, allow_infinity=False)
DAMAGE = st.floats(min_value=0.0, max_value=60.0, allow_nan=False, allow_infinity=False)
AMMO = st.floats(min_value=0.0, max_value=500.0, allow_nan=False, allow_infinity=False)  # magazine rounds (unit-scale quantized)
LASTHITBY = st.integers(min_value=-1, max_value=7).map(float)  # attacker index or -1 == never hit (exact int-valued f64)
REGIONHP = st.floats(min_value=0.0, max_value=80.0, allow_nan=False, allow_infinity=False)  # region sub-pool (clamped >= 0 in the kernel)
KILLS = st.integers(min_value=0, max_value=7).map(float)  # victory tally (exact int-valued f64, like ammo)
TTL = st.integers(min_value=0, max_value=250)
OWNER = st.integers(min_value=0, max_value=7)
ID = st.integers(min_value=0, max_value=(1 << 31) - 1)
TICK = st.integers(min_value=0, max_value=(1 << 40))

ENTITY = st.builds(s.EntityState, ID, LAT, LON, BEAR, ALT, PHI, TAS, GAMMA, HP, FIRECD, AMMO,
                   LASTHITBY, REGIONHP, REGIONHP, REGIONHP, KILLS)
PROJ = st.builds(s.ProjectileState, ID, LAT, LON, BEAR, ALT, DAMAGE, TTL, OWNER)


@given(TICK, st.lists(ENTITY, max_size=8), st.lists(PROJ, max_size=16))
def test_weapon_roundtrip(tick, entities, projectiles):
    snap = s.Snapshot(tick, entities, projectiles)
    wire = s.encode_snapshot(snap)
    dec, pos = s.decode_snapshot(wire)
    assert pos == len(wire)
    assert dec.protocol == s.SNAPSHOT_PROTOCOL  # 7
    assert dec.server_tick == tick
    assert len(dec.entities) == len(entities)
    assert len(dec.projectiles) == len(projectiles)
    for a, b in zip(entities, dec.entities):
        assert a.id == b.id
        assert abs(a.hp - b.hp) <= 1.0 / s.HP_SCALE
        assert abs(a.fire_cd - b.fire_cd) <= 1.0 / s.FIRECD_SCALE
        assert abs(a.ammo - b.ammo) <= 1.0 / s.AMMO_SCALE
        assert a.last_hit_by == b.last_hit_by  # int-valued at unit scale -> carried EXACTLY (incl. -1)
        assert abs(a.engine_hp - b.engine_hp) <= 1.0 / s.ENGINEHP_SCALE  # region pools (v1.19r0)
        assert abs(a.wing_hp - b.wing_hp) <= 1.0 / s.WINGHP_SCALE
        assert abs(a.tail_hp - b.tail_hp) <= 1.0 / s.TAILHP_SCALE
        assert a.kills == b.kills  # int-valued at unit scale -> carried EXACTLY
    for a, b in zip(projectiles, dec.projectiles):
        assert a.id == b.id
        assert abs(a.lat_deg - b.lat_deg) <= 1.0 / g.LATLON_SCALE
        assert abs(a.lon_deg - b.lon_deg) <= 1.0 / g.LATLON_SCALE
        assert abs(a.bearing_deg - b.bearing_deg) <= 1.0 / g.BEARING_SCALE
        assert abs(a.alt_m - b.alt_m) <= 1.0 / g.ALT_SCALE
        assert abs(a.damage - b.damage) <= 1.0 / s.DAMAGE_SCALE
        assert a.ttl == b.ttl       # integer counters carried EXACTLY
        assert a.owner == b.owner


@given(st.lists(ENTITY, min_size=1, max_size=4), st.lists(PROJ, max_size=4))
def test_protocol3_omits_weapon_section(entities, projectiles):
    # a protocol-3 (KIN-002) frame carries GEO+KIN but NOT the weapon block: it is strictly
    # shorter than the protocol-4 wire and decode leaves hp/fire_cd at 0 with no projectiles.
    p3 = s.Snapshot(9, entities, projectiles, protocol=3)
    p4 = s.Snapshot(9, entities, projectiles, protocol=4)
    w3, w4 = s.encode_snapshot(p3), s.encode_snapshot(p4)
    assert len(w3) < len(w4)
    d3, pos3 = s.decode_snapshot(w3)
    assert pos3 == len(w3) and d3.protocol == 3
    assert d3.projectiles == []
    for e in d3.entities:
        assert e.hp == 0.0 and e.fire_cd == 0.0


@given(st.lists(ENTITY, min_size=1, max_size=4), st.lists(PROJ, max_size=4))
def test_protocol4_omits_ammo(entities, projectiles):
    # a protocol-4 (pre-v1.14r0 WEAPON-001) frame carries hp/fire_cd + rounds but NOT ammo: it is
    # strictly shorter than the protocol-5 wire (one extra LEB128 per aircraft) and decode leaves
    # ammo at 0 while hp/fire_cd still round-trip — the older gunnery wire stays readable.
    p4 = s.Snapshot(9, entities, projectiles, protocol=4)
    p5 = s.Snapshot(9, entities, projectiles, protocol=5)
    w4, w5 = s.encode_snapshot(p4), s.encode_snapshot(p5)
    assert len(w4) < len(w5)
    d4, pos4 = s.decode_snapshot(w4)
    assert pos4 == len(w4) and d4.protocol == 4
    for a, b in zip(entities, d4.entities):
        assert b.ammo == 0.0
        assert abs(a.hp - b.hp) <= 1.0 / s.HP_SCALE
        assert abs(a.fire_cd - b.fire_cd) <= 1.0 / s.FIRECD_SCALE


@given(st.lists(ENTITY, min_size=1, max_size=4), st.lists(PROJ, max_size=4))
def test_protocol5_omits_lasthitby(entities, projectiles):
    # a protocol-5 (pre-v1.17r0 WEAPON-001) frame carries hp/fire_cd/ammo + rounds but NOT
    # last_hit_by: it is strictly shorter than the protocol-6 wire (one extra LEB128 per aircraft)
    # and decode leaves last_hit_by at the -1 "never hit" default while hp/fire_cd/ammo still
    # round-trip — the older gunnery wire stays readable.
    p5 = s.Snapshot(9, entities, projectiles, protocol=5)
    p6 = s.Snapshot(9, entities, projectiles, protocol=6)
    w5, w6 = s.encode_snapshot(p5), s.encode_snapshot(p6)
    assert len(w5) < len(w6)
    d5, pos5 = s.decode_snapshot(w5)
    assert pos5 == len(w5) and d5.protocol == 5
    for a, b in zip(entities, d5.entities):
        assert b.last_hit_by == -1.0
        assert abs(a.hp - b.hp) <= 1.0 / s.HP_SCALE
        assert abs(a.ammo - b.ammo) <= 1.0 / s.AMMO_SCALE


@given(st.lists(ENTITY, min_size=1, max_size=4), st.lists(PROJ, max_size=4))
def test_protocol6_omits_regions(entities, projectiles):
    # a protocol-6 (pre-v1.19r0 WEAPON-001) frame carries hp/fire_cd/ammo/last_hit_by + rounds
    # but NOT the region sub-pools / kill tally: it is strictly shorter than the protocol-7 wire
    # (four extra LEB128s per aircraft) and decode leaves engine_hp/wing_hp/tail_hp/kills at
    # their 0 defaults while hp/ammo/last_hit_by still round-trip — the older gunnery wire stays
    # readable.
    p6 = s.Snapshot(9, entities, projectiles, protocol=6)
    p7 = s.Snapshot(9, entities, projectiles, protocol=7)
    w6, w7 = s.encode_snapshot(p6), s.encode_snapshot(p7)
    assert len(w6) < len(w7)
    d6, pos6 = s.decode_snapshot(w6)
    assert pos6 == len(w6) and d6.protocol == 6
    for a, b in zip(entities, d6.entities):
        assert b.engine_hp == 0.0 and b.wing_hp == 0.0 and b.tail_hp == 0.0 and b.kills == 0.0
        assert abs(a.hp - b.hp) <= 1.0 / s.HP_SCALE
        assert abs(a.ammo - b.ammo) <= 1.0 / s.AMMO_SCALE
        assert a.last_hit_by == b.last_hit_by


@given(st.lists(ENTITY, min_size=1, max_size=3), st.lists(PROJ, min_size=1, max_size=4))
def test_weapon_snapshot_is_self_delimiting(entities, projectiles):
    # decoding stops exactly at the snapshot's end even with trailing bytes following.
    wire = s.encode_snapshot(s.Snapshot(7, entities, projectiles))
    dec, pos = s.decode_snapshot(wire + b"\xde\xad\xbe\xef")
    assert pos == len(wire)
    assert len(dec.entities) == len(entities)
    assert len(dec.projectiles) == len(projectiles)


@given(LAT, LON, BEAR, ALT, DAMAGE, TTL, OWNER)
def test_proj_from_kernel_exact_counters(lat_deg, lon_deg, bearing_deg, alt, damage, ttl, owner):
    # proj_from_kernel maps psi(rad)->bearing(deg) and carries ttl/owner unchanged; on the wire
    # the integer counters survive exactly (a kill must replicate with the right attribution).
    import math
    lat_r, lon_r, psi_r = math.radians(lat_deg), math.radians(lon_deg), math.radians(bearing_deg)
    p = s.proj_from_kernel(42, lat_r, lon_r, psi_r, alt, damage, ttl, owner)
    snap = s.Snapshot(3, [], [p])
    dec, _ = s.decode_snapshot(s.encode_snapshot(snap))
    assert len(dec.projectiles) == 1
    b = dec.projectiles[0]
    assert b.id == 42 and b.ttl == ttl and b.owner == owner
    assert abs(b.bearing_deg - bearing_deg) <= 1.0 / g.BEARING_SCALE
    assert abs(b.damage - damage) <= 1.0 / s.DAMAGE_SCALE


def test_weapon_id_mismatch_rejected():
    # the WEAPON per-aircraft block repeats each id (like KIN); a decoder rejects a wire whose
    # weapon id does not match the GEO id at the same index (guards against misaligned sections).
    e = s.EntityState(5, 1.0, 2.0, 3.0, 100.0, 10.0, 50.0, 4.0, 120.0, 2.0)
    out = bytearray()
    out += g.encode_i64(4)          # protocol
    out += g.encode_i64(3)          # server_tick
    out += g.encode_i64(1)          # n
    out += g.encode_i64(e.id)       # GEO id
    out += g.encode_point(e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m)
    out += g.encode_i64(e.id)       # KIN id (matches)
    out += g.encode_i64(g.quantize(e.phi_deg, s.PHI_SCALE))
    out += g.encode_i64(g.quantize(e.tas_mps, s.SPEED_SCALE))
    out += g.encode_i64(g.quantize(e.gamma_deg, s.GAMMA_SCALE))
    out += g.encode_i64(e.id + 1)   # WEAPON id — deliberately MISMATCHED
    out += g.encode_i64(g.quantize(e.hp, s.HP_SCALE))
    out += g.encode_i64(g.quantize(e.fire_cd, s.FIRECD_SCALE))
    out += g.encode_i64(0)          # projectile count
    with pytest.raises(ValueError):
        s.decode_snapshot(bytes(out))


def test_negative_projectile_count_rejected():
    out = bytearray()
    out += g.encode_i64(4)          # protocol
    out += g.encode_i64(0)          # server_tick
    out += g.encode_i64(0)          # n (no aircraft)
    out += g.encode_i64(-1)         # projectile count — invalid
    with pytest.raises(ValueError):
        s.decode_snapshot(bytes(out))
