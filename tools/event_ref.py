#!/usr/bin/env python3
"""
event_ref.py — SEADS reliable combat-EVENT channel REFERENCE (netcode layer 6).

The layer that makes the TRANSIENT combat moments replicate reliably over the SAME lossy wire the
session loop (layer 5) already uses. Layer 5 replicates combat STATE — HP, positions, tracer rounds
— via "nearest frame" semantics: HP is IDEMPOTENT (every later frame re-states it), so a dropped
frame is harmless, the next one heals it. But an EVENT is not idempotent — it is a discrete moment:

  * a HIT: target T lost D hitpoints THIS tick (drives the impact spark / damage number / HP flash),
  * a KILL: T's hp crossed to <=0 at THIS server tick (drives the kill-feed at the right instant).

Reading these off the nearest STATE frame is lossy in a way HP is not: the client can only see the
AGGREGATE hp in the frame that happens to arrive, so it cannot tell one 12-damage hit from two, it
learns of the kill LATE (at the next delivered frame, not the tick it happened), and if the frame
carrying the moment is dropped the moment is smeared or lost. So layer 6 adds a RELIABLE EVENT
CHANNEL: the server DERIVES events from the authoritative kernel and ships them with REDUNDANCY so
the client reconstructs the EXACT event sequence — every hit + the kill, each at its precise tick —
even though the transport drops frames.

The mechanism — a REDUNDANT EVENT JOURNAL (Tribes/Quake-style eventual-reliable delivery)
---------------------------------------------------------------------------------------------
  * The server keeps a monotonic event log: event `seq` counts up from 0; each event is
    (seq, tick, target, damage_milli, hp_after_milli, killed) — all INTEGERS (hp/damage quantized
    by the sealed HP_SCALE=1e3; hp is integer-valued so 1e3 carries it losslessly).
  * Every 20 Hz frame carries an EVENT WINDOW: the last EVENT_WINDOW_K events by seq (fewer while
    the log is short). This rides ALONGSIDE the snapshot in the session transport — it is a
    SESSION-LAYER message, NOT a new section of the sealed snapshot wire, so there is NO wire reseal
    (exactly the boundary layer 5 respects: net code composes the wire, it does not change the rail).
  * The client keeps `next_seq` (0 initially) and an append-only applied log. On each DELIVERED
    frame it applies, in ascending seq, every windowed event with seq >= next_seq, advancing
    next_seq past it. Events with seq < next_seq are already applied — deduped.

Why this is RELIABLE-ish and what the failure bound is
------------------------------------------------------
An event with seq S is (re)sent by every frame from its creation until K NEWER events push it out of
the window — so it rides UP TO K emitted frames' worth of redundancy (FEWER when a dense burst
creates newer events quickly: the two earliest SESSION-SK-001 hits ride only 3 frames; and once
firing stops and fewer than K newer events exist, the tail — including the kill — rides EVERY later
frame forever, so it is essentially always delivered). So S is lost ONLY if EVERY frame carrying it
is dropped — a burst blackout covering its whole in-window lifetime (at most K consecutive frames).
Under isolated single-frame drops (SESSION-SK-001 drops emits 30/55/80, never two in a row) every
event is delivered and the client reconstructs the FULL, EXACT log. The journal also AVOIDS head-of-line blocking: a permanently-lost early event does not
stall later ones — on the next window the client resyncs from the freshest seq it can see (it skips
the aged-out gap and keeps going). The synthetic bound test below proves both halves: single drops
recover; a K-consecutive blackout loses exactly the events that aged out during it, then resyncs.

Why it is deterministic (bit-exactly gateable) even though the transport is LOSSY
---------------------------------------------------------------------------------
Event derivation reads the kernel's PER-ROUND hit queue (ref_kernel.HitEvent — appended at hit time
in projectile array order, deterministic; the kernel is det_math, bit-exact cross-toolchain by the
golden promise) and quantizes hp to integers (the sealed 1e3). Window selection, the transport's
integer lag/drop, and the client's seq dedup are pure integer logic. So the reconstructed event log
serializes to identical bytes on MSVC/GCC/Clang x64/AArch64, and its whole-run SHA-256 EVENT_DIGEST
is the cross-impl parity artifact the C++ mirror (src/net/event.{h,cpp}) reproduces BIT-FOR-BIT —
same construction as geo001 / snapshot / session.

Boundaries (doctrine): the hit queue is the ONE piece that must live kernel-side — only the kernel
knows WHICH round resolved a hit (the hp-delta view this layer used through v1.17r0 lumps same-tick
multi-round damage and reads attribution off the last-writer field). It is observable OUTPUT, not
canonical state: cleared each step, appended on hit, NEVER serialized into snapshot() -> the
world_hash and ALL 10 goldens are byte-identical (verified). Wherever no tick lands two rounds on
one target (true of SESSION-SK-001) the per-round stream reduces bit-for-bit to the old hp-delta
stream, so the sealed EVENT_DIGEST is unchanged. No rail / golden / wire change — still composes the
EXISTING session, riding seal v1.17r0 (see ADR-Step7-Guns-HitQueue-v1.17r0).

Usage:  python tools/event_ref.py    # self-test: derivation + full reconstruction + kill + bound
"""
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ref_kernel as rk
import envelopes as envmod
import geo001_ref as g
import snapshot_ref as snap
import session_ref as sr   # reuse the SESSION-SK-001 scenario, kernel build, command + transport

_RAILS = sr._RAILS

# How many most-recent events every frame re-sends (the redundancy depth). An event rides UP TO K
# emitted frames before newer events push it out, so K=4 tolerates a blackout of up to 3 consecutive
# frames for a well-spaced event (~150 ms at the 20 Hz cadence over snap_every=5) — comfortably
# covering the isolated single drops in SESSION-SK-001 while giving the synthetic bound test a clean
# blackout to defeat. The wire is INTEGER-cheap: a full window is K*(6 varints).
EVENT_WINDOW_K = 4

HP_SCALE = snap.HP_SCALE   # sealed 1e3 quantum shared with the snapshot weapon section

# ---------------------------------------------------------------------------------------
# EVENT-MULTIHIT-001 — the per-round GRANULARITY scenario (cross-impl vector). Twin P-47Ds fly
# symmetric about the equator (lat ±0.2 deg ≈ ±52 m — inside the 60 m hit radius of a target ON the
# equator, outside it of each other at ~105 m) with an A6M2 dead ahead on the equator. Both fire a
# 3-volley burst (rof = 3 ticks); by symmetry each volley's two rounds strike the target on the SAME
# tick. This is exactly the case the pre-hit-queue hp-delta derivation could not represent: it lumped
# each volley into ONE 24-damage event credited (via the last-writer last_hit_by) to whichever round
# applied second — shooter 0's credit was provably lost. Per-round events give each round its own
# attributed event; the kill volley additionally shows the overkill clamp (the A6M2 dies at 22 hp to
# 12+12: shooter 0's round 22->10, shooter 1's round 10->0 with EFFECTIVE loss 10) and "a corpse
# can't be hit" (any later round flies past). Timeline: rounds close ~393 m at ~9 m/tick => hits
# land around tick ~45, well inside ticks=120 minus the lag. One isolated drop (emit 50) keeps the
# redundancy machinery exercised; full reconstruction is still expected.
# ---------------------------------------------------------------------------------------
MULTIHIT = {
    "id": "EVENT-MULTIHIT-001",
    "ticks": 120,
    "snap_every": 5,
    "lag_ticks": 10,
    "render_delay": 15,
    "drop_emit_ticks": [50],
    "aircraft": [
        # AC0 — shooter A (P-47D), north of the equator line of fire.
        {"envelope": "p47d",
         "start": {"lat_deg": 0.2, "lon_deg": 0.0, "psi_deg": 90.0,
                   "phi_deg": 0.0, "alt_m": 3000.0, "tas_mps": 200.0},
         "schedule": [
             {"start_tick": 0, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": True},
             {"start_tick": 9, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": False},
         ]},
        # AC1 — shooter B (P-47D), the mirror image south of it.
        {"envelope": "p47d",
         "start": {"lat_deg": -0.2, "lon_deg": 0.0, "psi_deg": 90.0,
                   "phi_deg": 0.0, "alt_m": 3000.0, "tas_mps": 200.0},
         "schedule": [
             {"start_tick": 0, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": True},
             {"start_tick": 9, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": False},
         ]},
        # AC2 — target (A6M2) on the equator, straight and level, ~393 m ahead.
        {"envelope": "a6m2",
         "start": {"lat_deg": 0.0, "lon_deg": 1.5, "psi_deg": 90.0,
                   "phi_deg": 0.0, "alt_m": 3000.0, "tas_mps": 150.0},
         "schedule": [
             {"start_tick": 0, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.7, "fire": False},
         ]},
    ],
}


# ---------------------------------------------------------------------------------------
# Event record. All fields integer (hp/damage in milli-hp via HP_SCALE). `seq` is the monotonic
# reliable-delivery key; `tick` is the server tick the damage was observed (1..ticks).
# ---------------------------------------------------------------------------------------
class Event:
    # v1.17r0: `attacker` = the aircraft index that dealt this tick's damage (the target's last_hit_by
    # observed AFTER the step), or -1 if unknown. This is what turns "AC1 died" into "AC0 downed AC1"
    # — an ATTRIBUTED kill-feed. Optional (defaults -1) so the synthetic bound-test constructions and
    # any pre-attribution caller still build. See ADR-Step7-Guns-Attribution.
    __slots__ = ("seq", "tick", "target", "damage_milli", "hp_after_milli", "killed", "attacker")

    def __init__(self, seq, tick, target, damage_milli, hp_after_milli, killed, attacker=-1):
        self.seq = int(seq)
        self.tick = int(tick)
        self.target = int(target)
        self.damage_milli = int(damage_milli)
        self.hp_after_milli = int(hp_after_milli)
        self.killed = int(killed)
        self.attacker = int(attacker)

    def as_tuple(self):
        return (self.seq, self.tick, self.target, self.damage_milli,
                self.hp_after_milli, self.killed, self.attacker)


# ---- server: drive the authoritative kernel, DERIVE events from hp deltas ----------------
def run_event_server(scenario=sr.SESSION):
    """Step the authoritative kernel over the whole timeline (identical driving to
    session_ref.run_server) and, by OBSERVING each aircraft's hp before/after the step, derive the
    combat event log. Returns:
      events   : [Event, ...] in (tick asc, target asc) order, seq = list index.
      windows  : dict emit_tick -> event-window bytes (last K events with tick <= emit_tick),
                 emitted at the SAME 20 Hz cadence + tick-0 frame as the snapshot frames.
      ticks    : the scenario tick count (echoed for convenience).
    PER-ROUND granularity (rides v1.17r0): events are read off the kernel's per-round hit queue
    (k.hit_events — one HitEvent per CONNECTING ROUND, appended at hit time in projectile array
    order), not inferred from per-tick hp deltas. Two rounds striking one target on one tick are two
    distinct events (the old hp-delta observation lumped them into one); killed=1 marks the exact
    round that crossed hp >0 -> <=0. damage_milli is the round's EFFECTIVE loss (quantized hp_before
    - hp_after, post-clamp — an overkill round reports what it actually removed), so per-tick sums
    equal the old lumped deltas and, whenever no tick lands two rounds on one target (true of
    SESSION-SK-001), the event stream — and the sealed EVENT_DIGEST — is bit-for-bit unchanged."""
    ticks = int(scenario["ticks"])
    snap_every = int(scenario["snap_every"])
    k, scheds, envs = sr._build_server_kernel(scenario)

    events = []

    def window_at(emit_tick):
        avail = [e for e in events if e.tick <= emit_tick]
        return avail[-EVENT_WINDOW_K:]

    def encode_window(win):
        out = bytearray()
        out += g.encode_i64(len(win))
        for e in win:
            out += g.encode_i64(e.seq)
            out += g.encode_i64(e.tick)
            out += g.encode_i64(e.target)
            out += g.encode_i64(e.damage_milli)
            out += g.encode_i64(e.hp_after_milli)
            out += g.encode_i64(e.killed)
            out += g.encode_i64(e.attacker)          # v1.17r0: attributed kill-feed
        return bytes(out)

    windows = {0: encode_window(window_at(0))}   # tick-0 frame (no events yet -> empty window)
    for t in range(1, ticks + 1):
        cmds = [sr.server_command_at(sched, t - 1) for sched in scheds]
        k.step_scenario(cmds, envs)
        # PER-ROUND events off the kernel's hit queue (this step's hits only; projectile array
        # order = deterministic). attacker comes straight from the striking round's owner — no
        # last-writer field read. damage_milli = the round's effective (post-clamp) hp removal.
        for h in k.hit_events:
            d_milli = g.quantize(h.hp_before, HP_SCALE) - g.quantize(h.hp_after, HP_SCALE)
            events.append(Event(len(events), t, h.target, d_milli,
                                g.quantize(h.hp_after, HP_SCALE), h.killed, h.attacker))
        if t % snap_every == 0:
            windows[t] = encode_window(window_at(t))
    return events, windows, ticks


# ---- client: apply the redundant journal from the delivered windows ----------------------
def decode_window(data):
    """Event-window bytes -> [Event, ...] (ascending seq, as the server emitted them)."""
    pos = 0
    n, pos = g.decode_i64(data, pos)
    win = []
    for _ in range(n):
        seq, pos = g.decode_i64(data, pos)
        tick, pos = g.decode_i64(data, pos)
        target, pos = g.decode_i64(data, pos)
        dmg, pos = g.decode_i64(data, pos)
        hp_after, pos = g.decode_i64(data, pos)
        killed, pos = g.decode_i64(data, pos)
        attacker, pos = g.decode_i64(data, pos)   # v1.17r0: attributed kill-feed
        win.append(Event(seq, tick, target, dmg, hp_after, killed, attacker))
    return win


def apply_windows(delivered_windows):
    """Reconstruct the client event log from the DELIVERED windows (already in delivery order,
    which the fixed-lag transport keeps ascending by emit tick). Redundant-journal dedup: append
    every windowed event with seq >= next_seq in ascending seq, advancing next_seq. Skips an
    aged-out gap without head-of-line blocking (resyncs from the freshest seq available).
    Returns the append-only applied [Event, ...]."""
    next_seq = 0
    applied = []
    for wbytes in delivered_windows:
        for e in decode_window(wbytes):
            if e.seq >= next_seq:
                applied.append(e)
                next_seq = e.seq + 1
    return applied


def run_event_client(scenario, windows, drop_emit_ticks=None):
    """Ship the event windows through the SESSION transport (fixed lag + packet loss) and apply the
    journal. drop_emit_ticks overrides the scenario's drop set (used by the bound test). Returns
    (applied, delivered_count)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    drops = scenario["drop_emit_ticks"] if drop_emit_ticks is None else drop_emit_ticks
    transport = sr.Transport(windows, lag, drops)
    delivered = []
    for t in range(1, ticks + 1):
        for (_st, wbytes) in transport.delivered_at(t):
            delivered.append(wbytes)
    return apply_windows(delivered), len(delivered)


def run_events(scenario=sr.SESSION, drop_emit_ticks=None):
    events, windows, ticks = run_event_server(scenario)
    applied, delivered = run_event_client(scenario, windows, drop_emit_ticks)
    return {"events": events, "windows": windows, "applied": applied,
            "delivered": delivered, "n_windows": len(windows), "ticks": ticks}


# ---- canonical serialization of the reconstructed event log (the parity artifact) --------
def encode_event_log(applied):
    """Serialize the client's reconstructed event log to canonical bytes (count + each event's seven
    integer fields, in applied order) — all through the SAME GEO-001 ZigZag/LEB128 the wire uses, so
    the digest is byte-identical in C++ and Python (no float bit-format assumptions). v1.17r0 added
    the 7th field `attacker` (attributed kill-feed) -> the EVENT_DIGEST moves this seal."""
    out = bytearray()
    out += g.encode_i64(len(applied))
    for e in applied:
        out += g.encode_i64(e.seq)
        out += g.encode_i64(e.tick)
        out += g.encode_i64(e.target)
        out += g.encode_i64(e.damage_milli)
        out += g.encode_i64(e.hp_after_milli)
        out += g.encode_i64(e.killed)
        out += g.encode_i64(e.attacker)   # v1.17r0
    return bytes(out)


def event_digest(applied):
    return hashlib.sha256(encode_event_log(applied)).hexdigest()


def kill_event(applied):
    """The applied event with killed==1 (the reconstructed kill), or None."""
    for e in applied:
        if e.killed:
            return e
    return None


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0
    scenario = sr.SESSION
    res = run_events(scenario)
    events, applied = res["events"], res["applied"]

    # 1) DETERMINISM: a second full run reproduces the identical reconstructed-log digest.
    res2 = run_events(scenario)
    if event_digest(applied) != event_digest(res2["applied"]):
        print("FAIL event digest not reproducible"); fails += 1

    # 2) NON-VACUOUS: the fight produced a real multi-event sequence (the P-47's burst walks the
    #    A6M2's hp down over several ticks) culminating in a kill.
    if len(events) < 2:
        print(f"FAIL expected a multi-event sequence, got {len(events)}"); fails += 1
    kev = kill_event(events)
    if kev is None:
        print("FAIL server derived no KILL event (A6M2 should die)"); fails += 1
    elif kev.target != 1:
        print(f"FAIL kill target {kev.target}, expected AC1 (A6M2)"); fails += 1
    elif kev.attacker != 0:
        print(f"FAIL kill attacker {kev.attacker}, expected AC0 (P-47) — v1.17r0 attribution"); fails += 1
    # every derived HIT/KILL event on AC1 is attributed to the P-47 (the only shooter here)
    if any(e.attacker != 0 for e in events):
        print("FAIL an event was not attributed to AC0 (P-47)"); fails += 1

    # 3) FULL RECONSTRUCTION under the standard (isolated single) drop set: redundancy delivers
    #    EVERY event — the client's applied log equals the server's log exactly (same digest).
    if event_digest(applied) != event_digest(events):
        srv_seqs = [e.seq for e in events]
        cli_seqs = [e.seq for e in applied]
        print(f"FAIL not fully reconstructed under standard drops: "
              f"server {len(srv_seqs)} vs client {len(cli_seqs)} events "
              f"(missing {sorted(set(srv_seqs) - set(cli_seqs))})"); fails += 1
    # and the KILL crossed the wire with its precise tick + final hp intact
    kcli = kill_event(applied)
    if kcli is None or kev is None or kcli.as_tuple() != kev.as_tuple():
        print("FAIL kill event not faithfully reconstructed on the client"); fails += 1

    # 4) RELIABILITY BOUND (negative control), on a controlled synthetic stream so the bound is
    #    exact and independent of kernel timing:
    #      (a) an ISOLATED single drop recovers fully (redundancy works);
    #      (b) a K-consecutive blackout of an event's whole window-lifetime PERMANENTLY loses exactly
    #          that event, and the channel RESYNCS (a later event still lands).
    fails += _bound_test()

    # 5) PER-ROUND GRANULARITY (EVENT-MULTIHIT-001): twin shooters land both rounds of every volley
    #    on the SAME tick -> each volley is TWO events with DISTINCT attackers (the hp-delta view
    #    lumped them into one, credited to the last writer); the kill volley shows the overkill
    #    clamp (effective loss < the round's carried damage); reconstruction is still complete.
    mh = run_events(MULTIHIT)
    mev, mapp = mh["events"], mh["applied"]
    by_tick = {}
    for e in mev:
        by_tick.setdefault(e.tick, []).append(e)
    if len(mev) != 6 or len(by_tick) != 3:
        print(f"FAIL multihit: expected 6 events over 3 ticks, got {len(mev)} over {len(by_tick)}")
        fails += 1
    else:
        for t, tev in by_tick.items():
            if len(tev) != 2 or {e.attacker for e in tev} != {0, 1} or any(e.target != 2 for e in tev):
                print(f"FAIL multihit tick {t}: want 2 events on target 2 from attackers {{0,1}}")
                fails += 1
        mkill = mev[-1]
        pre_kill_hp = mev[-2].hp_after_milli
        if not (mkill.killed == 1 and mkill.attacker == 1 and mkill.hp_after_milli == 0
                and mkill.damage_milli == pre_kill_hp < 12000):
            print("FAIL multihit kill: want shooter 1's overkill round clamped to the remaining hp")
            fails += 1
        if sum(e.damage_milli for e in mev) != 70000:   # a6m2 hp_start walked exactly to 0
            print("FAIL multihit: effective damage must sum to the target's full hp"); fails += 1
        if event_digest(mapp) != event_digest(mev):
            print("FAIL multihit: not fully reconstructed under the isolated drop"); fails += 1

    if fails == 0:
        seqs = [e.seq for e in events]
        hitcount = sum(1 for e in events if not e.killed)
        print("RESULT: EVENT REFERENCE SELFTEST PASS "
              f"(K={EVENT_WINDOW_K}, {res['ticks']} ticks, drops {scenario['drop_emit_ticks']})")
        print(f"  server events={len(events)} ({hitcount} hits + 1 kill), "
              f"applied={len(applied)}, windows delivered={res['delivered']}/{res['n_windows']}")
        print(f"  kill: seq={kev.seq} tick={kev.tick} attacker={kev.attacker} -> target={kev.target} "
              f"hp_after_milli={kev.hp_after_milli}  (AC{kev.attacker} downed AC{kev.target})")
        print(f"  event_digest={event_digest(applied)}")
        return 0
    print(f"RESULT: EVENT REFERENCE SELFTEST FAIL ({fails})")
    return 1


def _make_windows_from_events(events, emit_ticks):
    """Build {emit_tick: window_bytes} for a HAND-MADE event stream over the given emit schedule —
    the same last-K windowing the server uses. Lets the bound test drive the journal directly."""
    windows = {}
    for et in emit_ticks:
        avail = [e for e in events if e.tick <= et]
        win = avail[-EVENT_WINDOW_K:]
        out = bytearray()
        out += g.encode_i64(len(win))
        for e in win:
            for f in e.as_tuple():
                out += g.encode_i64(f)
        windows[et] = bytes(out)
    return windows


def _bound_test():
    """Exercise apply_windows against a controlled stream: 10 events, one created on each of the
    first 10 emitted frames (a new event every frame => each is pushed out of the K-window after
    exactly K newer frames, so each has a bounded lifetime). Emits at ticks 1..10 (snap_every=1),
    lag 0."""
    fails = 0
    events = [Event(seq=i, tick=i + 1, target=1, damage_milli=1000,
                    hp_after_milli=(10 - i) * 1000, killed=1 if i == 9 else 0)
              for i in range(10)]
    emit_ticks = list(range(1, 11))
    windows = _make_windows_from_events(events, emit_ticks)

    class _Sc:
        pass
    sc = {"ticks": 10, "lag_ticks": 0, "drop_emit_ticks": []}

    # (a) isolated single drop of emit 3 -> event seq 2 still rides emits 4,5,6 (K=4) -> full recovery
    applied_a, _ = run_event_client(sc, windows, drop_emit_ticks=[3])
    if [e.seq for e in applied_a] != list(range(10)):
        print(f"FAIL bound(a): single drop should recover all; got {[e.seq for e in applied_a]}")
        fails += 1

    # (b) blackout emits 3,4,5,6 (K=4 consecutive). Event seq 2 (created tick 3) rides ONLY windows
    #     of emits 3,4,5,6 (it ages out at emit 7, replaced by seqs 3..6) -> its entire lifetime is
    #     in the blackout -> PERMANENTLY lost. seq 0,1 landed on emits 1,2 (before). seq 3+ land on
    #     emit 7+ (after) -> channel RESYNCS. Expected applied seqs: {0,1} U {3..9}, i.e. 2 missing.
    applied_b, _ = run_event_client(sc, windows, drop_emit_ticks=[3, 4, 5, 6])
    got = [e.seq for e in applied_b]
    expect = [0, 1, 3, 4, 5, 6, 7, 8, 9]
    if got != expect:
        print(f"FAIL bound(b): K-consecutive blackout; got {got}, expected {expect}")
        fails += 1
    if 2 in got:
        print("FAIL bound(b): aged-out event seq 2 should be permanently lost"); fails += 1
    if 9 not in got:
        print("FAIL bound(b): channel must resync (kill seq 9 delivered after blackout)"); fails += 1
    return fails


if __name__ == "__main__":
    sys.exit(_selftest())
