#!/usr/bin/env python3
"""
session_ref.py — SEADS server<->client SESSION loop REFERENCE (netcode layer 5).

The layer that finally USES the wire transport between two endpoints. A SERVER runs the
authoritative sealed kernel over a scripted multi-aircraft dogfight and, at the 20 Hz snapshot
cadence, serializes the FULL protocol-4 world into a WEAPON-001 wire frame (GEO + KIN-002 +
WEAPON sections). An in-process TRANSPORT ships those frames server->client with a fixed integer
latency and deterministic packet loss. The CLIENT reconstructs the entire dogfight from the bytes
it receives — tying together the four netcode layers built earlier:

  * OWN ship (id 0): PREDICTED at "now" from the local input timeline (layer 4b, predict_ref),
    reconciled against each decoded wire frame (the realistic LOSSY-decode reseed path — snap the
    own aircraft to the dequantized wire state, drop consumed inputs, replay the rest forward).
  * REMOTE ships (id != 0): INTERPOLATED ~150 ms in the past (layer 4a, interp_ref) from the
    decoded GEO frames, so they move smoothly across the 20 Hz cadence, jitter, and dropped frames.
  * HP / KILLS / tracer ROUNDS (all aircraft): read from the decoded WEAPON section (layer 2 +
    WEAPON-001, snapshot_ref) of the freshest delivered frame — the "nearest frame" semantics the
    renderer's sample_weapons already uses (hp is discrete, rounds transient, so no interpolation).

The reconstructed per-tick CLIENT VIEW (own predicted geometry @ now + remote interpolated geometry
@ render_tick + weapon state from the freshest frame) is serialized to canonical bytes and hashed;
the whole-session SHA-256 digest is the cross-impl parity artifact — the C++ mirror (src/net/session)
must reproduce it BIT-FOR-BIT, exactly as geo001 / snapshot / interp / predict do.

Why this is deterministic (and thus bit-exactly gateable) even though the transport is LOSSY
--------------------------------------------------------------------------------------------
Packet loss and quantization destroy INFORMATION, but they are perfectly REPRODUCIBLE operations.
Every step of the reconstruction is either det_math (the kernel the predictor drives — bit-exact
cross-toolchain by the golden promise), pure IEEE +-*/ (the interp sampler, proven bit-exact under
the strict-FP/no-FMA flags), or integer arithmetic (ZigZag/LEB128 quantize, the transport's integer
lag/drop, the "freshest frame" tick compare). So a given SESSION-SK-001 scenario reconstructs to the
identical bytes on MSVC/GCC/Clang x64/AArch64. This is a STRONGER proof than predict_ref's bit-exact
digest, which reconciled against CANONICAL state: here the own ship reconciles against the DEQUANTIZED
WIRE (the realistic remote/late-join path) and the whole dogfight — kill included — still reproduces
to the bit.

Boundaries (doctrine): net code stays OUTSIDE the kernel. The server DRIVES the kernel (like
lockstep/predict) to produce authoritative state; the wire is lossy and downstream; decoded bits
feed the renderer's client view and the predictor's RESEED, never the canonical sim. No rail /
golden / wire change — this composes the EXISTING protocol-4 wire, so it rides seal v1.12r0 (a
Tier-2 net layer like interp was, not a seal).

Usage:  python tools/session_ref.py        # internal self-test (kill replication + determinism + bounds)
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
import interp_ref as itp
import predict_ref as pred   # reuse the sealed layer-4b Predictor (drives the sealed kernel)

_RAILS = json.loads(
    (Path(__file__).resolve().parent.parent / "config" / "rails" / "atm.json")
    .read_text(encoding="utf-8"))["rails"]

OWN_ID = 0   # the local player's aircraft on the wire (predicted, not interpolated)
R_M = float(_RAILS["geometry"]["radius_m"])   # sphere radius, for arc-length error in metres

# ---------------------------------------------------------------------------------------
# SESSION-SK-001 — the canonical session scenario. The 3-ship gundemo shape (mirrors the
# renderer's --gundemo): a P-47D (the OWN/local ship) guns down an A6M2 from a co-altitude
# tail chase while a Spitfire maneuvers as a live bystander. This exercises, over the wire:
#   * a KILL that must REPLICATE to the client (A6M2 hp -> 0, dead/frozen) purely from bytes,
#   * a moving REMOTE (the banking Spitfire) that must interpolate smoothly across dropped frames,
#   * the OWN ship predicted at "now" and reconciled against the lossy wire each frame.
# Angles are DEGREES here (deg->rad via ref_kernel.deg2rad, the single conversion path). The
# schedule format matches config/scenarios/*.json (integer phase select, largest start_tick<=t).
# ---------------------------------------------------------------------------------------
SESSION = {
    "id": "SESSION-SK-001",
    "ticks": 200,             # 2.0 s at 100 Hz
    "snap_every": 5,          # server snapshot cadence: 20 Hz over the 100 Hz sim
    "lag_ticks": 10,          # ~100 ms transport latency (server tick T delivered at client T+lag)
    "render_delay": 15,       # remotes rendered lag+snap_every behind so interp always brackets
    "drop_emit_ticks": [30, 55, 80],   # deterministic packet loss: these emitted frames never arrive
    "aircraft": [
        # AC0 — OWN / local (P-47D): tail chase, fires a burst over ticks 0-19, then ceases.
        {"envelope": "p47d",
         "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 90.0,
                   "phi_deg": 0.0, "alt_m": 4000.0, "tas_mps": 200.0},
         "schedule": [
             {"start_tick": 0,   "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": True},
             {"start_tick": 20,  "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.85, "fire": False},
         ]},
        # AC1 — REMOTE target (A6M2): straight, co-altitude ~393 m ahead; gets killed from behind.
        {"envelope": "a6m2",
         "start": {"lat_deg": 0.0, "lon_deg": 1.5, "psi_deg": 90.0,
                   "phi_deg": 0.0, "alt_m": 4000.0, "tas_mps": 150.0},
         "schedule": [
             {"start_tick": 0, "bank_deg": 0.0, "g_cmd": 1.0, "throttle": 0.7, "fire": False},
         ]},
        # AC2 — REMOTE bystander (Spitfire): a sustained banking turn (moves in lat/lon/bearing),
        # alive throughout — the moving remote the client must interpolate across dropped frames.
        {"envelope": "spitfire_mk5",
         "start": {"lat_deg": 0.2, "lon_deg": 0.5, "psi_deg": 70.0,
                   "phi_deg": 0.0, "alt_m": 4200.0, "tas_mps": 180.0},
         "schedule": [
             {"start_tick": 0, "bank_deg": 25.0, "g_cmd": 1.4, "throttle": 0.8, "fire": False},
         ]},
    ],
}


# ---- schedule helpers (integer phase select, mirrors ref_kernel.run_scenario) ----------
def _phase_at(sched, t):
    idx = 0
    for j, ph in enumerate(sched):
        if int(ph["start_tick"]) <= t:
            idx = j
        else:
            break
    return sched[idx]


def server_command_at(sched, t):
    """Authoritative 4-tuple command (target_phi_rad, target_g, throttle, fire) for tick t."""
    ph = _phase_at(sched, t)
    return (rk.deg2rad(ph["bank_deg"]), float(ph["g_cmd"]),
            float(ph.get("throttle", 0.0)), bool(ph.get("fire", False)))


def own_kinematic_command_at(sched, t):
    """The OWN ship's 3-tuple KINEMATIC command (target_phi_rad, target_g, throttle) for the
    predictor. Firing never touches kinematics (no recoil in this model) and the own ship's
    hp/rounds are wire-sourced, so the predictor omits the fire bit and predicts pure motion."""
    ph = _phase_at(sched, t)
    return (rk.deg2rad(ph["bank_deg"]), float(ph["g_cmd"]), float(ph.get("throttle", 0.0)))


# ---- server: drive the authoritative kernel, emit protocol-4 wire frames at 20 Hz ----------
def _build_server_kernel(scenario):
    k = rk.Kernel({"rails": _RAILS})
    scheds, envs = [], []
    for ac in scenario["aircraft"]:
        s = ac["start"]
        env = envmod.load_envelope(ac["envelope"])
        a = rk.Aircraft(lat=rk.deg2rad(s["lat_deg"]), lon=rk.deg2rad(s["lon_deg"]),
                        psi=rk.deg2rad(s["psi_deg"]), phi=rk.deg2rad(s["phi_deg"]),
                        alt=float(s["alt_m"]), tas=float(s["tas_mps"]))
        a.hp = env["hp_start"]          # G3: per-airframe starting hitpoints (matches build_scenario)
        a.ammo = env["ammo_start"]      # G4: per-airframe magazine (matches build_scenario)
        k.aircraft.append(a)
        scheds.append(ac["schedule"])
        envs.append(env)
    return k, scheds, envs


def _serialize_world(k, server_tick):
    """Serialize the kernel's FULL world to a protocol-4 wire frame: every aircraft (GEO + KIN-002
    + WEAPON hp/fire_cd) and every live round (GEO + damage + ttl/owner). Aircraft id = SoA index;
    projectile id = SoA index (rounds are transient — a per-frame index is all the client needs to
    draw + count them). Mirrors what seads_record's --gundemo writes, over the sealed codec."""
    ents = []
    for i, ac in enumerate(k.aircraft):
        ents.append(snap.from_kernel(i, ac.lat, ac.lon, ac.psi, ac.alt,
                                     ac.phi, ac.tas, ac.gamma, ac.hp, ac.fire_cd))
    projs = []
    for j, p in enumerate(k.projectiles):
        projs.append(snap.proj_from_kernel(j, p.lat, p.lon, p.psi, p.alt,
                                           p.damage, p.ttl, p.owner))
    return snap.encode_snapshot(snap.Snapshot(server_tick, ents, projs))


def run_server(scenario=SESSION):
    """Step the authoritative kernel over the whole timeline. Returns:
      frames        : dict emit_tick -> wire bytes (a frame every snap_every ticks, incl. tick 0)
      server_states : [0..ticks], server_states[t][i] = aircraft i's (lat,lon,psi,phi,alt,tas,gamma,
                      hp,fire_cd) AFTER t ticks (the authoritative truth the client is judged against)
    """
    ticks = int(scenario["ticks"])
    snap_every = int(scenario["snap_every"])
    k, scheds, envs = _build_server_kernel(scenario)

    def cap():
        return [(ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma, ac.hp, ac.fire_cd)
                for ac in k.aircraft]

    frames = {0: _serialize_world(k, 0)}     # initial world (pre-step), server_tick 0
    server_states = [cap()]
    for t in range(1, ticks + 1):
        cmds = [server_command_at(sched, t - 1) for sched in scheds]
        k.step_scenario(cmds, envs)
        server_states.append(cap())
        if t % snap_every == 0:
            frames[t] = _serialize_world(k, t)
    return frames, server_states


# ---- transport: fixed integer latency + deterministic packet loss ----------------------
class Transport:
    """In-process server->client channel. A frame emitted at server tick T is delivered to the
    client at client tick T + lag, UNLESS T is in the drop set (that frame never arrives). Fixed
    lag preserves order (server_ticks arrive strictly increasing); at most one frame lands per
    tick here (emits are snap_every apart, lag is constant)."""

    def __init__(self, frames, lag, drop_emit_ticks):
        self.frames = frames
        self.lag = int(lag)
        self.drop = set(int(x) for x in drop_emit_ticks)

    def delivered_at(self, client_tick):
        st = client_tick - self.lag
        if st in self.frames and st not in self.drop:
            return [(st, self.frames[st])]
        return []


# ---- client view serialization (canonical, cross-impl byte-reproducible) ---------------
# All fields go through the SAME integer quantize + ZigZag/LEB128 (geo001_ref) the wire uses, so
# the reconstructed-view bytes are byte-identical in C++ and Python (no float bit-format assumptions).
def _encode_geo(out, e):
    out += g.encode_point(e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m)


def encode_client_view(client_tick, render_tick, own_ent, remotes, wframe):
    """Serialize one reconstructed client view to canonical bytes:
      header : client_tick, render_tick
      OWN    : predicted geometry @ now (GEO + KIN phi/tas/gamma) — always present
      REMOTES: count + per-remote (id, interpolated GEO @ render_tick), sorted by id
      WEAPONS: count + per-aircraft (id, hp_q, dead flag, fire_cd_q) from the freshest frame
      ROUNDS : count + per-round (pid, GEO, damage_q, ttl, owner) from the freshest frame
    A freshly-connected client (before any frame arrives) emits empty REMOTES/WEAPONS/ROUNDS but
    still its own predicted ship — a faithful 'I only know myself yet' state."""
    out = bytearray()
    out += g.encode_i64(client_tick)
    out += g.encode_i64(render_tick)
    # OWN — predicted @ now (full kinematic state)
    _encode_geo(out, own_ent)
    out += g.encode_i64(g.quantize(own_ent.phi_deg, snap.PHI_SCALE))
    out += g.encode_i64(g.quantize(own_ent.tas_mps, snap.SPEED_SCALE))
    out += g.encode_i64(g.quantize(own_ent.gamma_deg, snap.GAMMA_SCALE))
    # REMOTES — interpolated @ render_tick (geometry only), ascending id
    rem = sorted(remotes, key=lambda e: e.id)
    out += g.encode_i64(len(rem))
    for e in rem:
        out += g.encode_i64(e.id)
        _encode_geo(out, e)
    # WEAPONS + ROUNDS — from the freshest delivered frame (nearest-frame; None => empty)
    if wframe is None:
        out += g.encode_i64(0)   # no weapon data yet
        out += g.encode_i64(0)   # no rounds
    else:
        wents = sorted(wframe.entities, key=lambda e: e.id)
        out += g.encode_i64(len(wents))
        for e in wents:
            out += g.encode_i64(e.id)
            out += g.encode_i64(g.quantize(e.hp, snap.HP_SCALE))
            out += g.encode_i64(1 if e.hp <= 0.0 else 0)          # dead flag (kill replicated)
            out += g.encode_i64(g.quantize(e.fire_cd, snap.FIRECD_SCALE))
        out += g.encode_i64(len(wframe.projectiles))
        for p in wframe.projectiles:
            out += g.encode_i64(p.id)
            _encode_geo(out, p)
            out += g.encode_i64(g.quantize(p.damage, snap.DAMAGE_SCALE))
            out += g.encode_i64(p.ttl)
            out += g.encode_i64(p.owner)
    return bytes(out)


def _freshest_frame(buf, render_tick):
    """The delivered frame with the largest server_tick <= render_tick (the freshest weapon truth
    the client can show at the rendered moment), or None if none has arrived yet. buf.frames are
    ascending server_tick, so scan for the last one that qualifies."""
    chosen = None
    for f in buf.frames:
        if f.server_tick <= render_tick:
            chosen = f
        else:
            break
    return chosen


# ---- the session loop: run the client against the transported frames -------------------
def run_client(scenario, frames, reconcile=True):
    """Drive the client tick-by-tick over the transported frame stream. Per tick t:
      1) predict the OWN ship one tick from its local kinematic command (layer 4b);
      2) for each frame delivered this tick: decode it, add to the interp buffer, and (if the
         frame carries the own ship) reconcile the predictor against the DEQUANTIZED wire state;
      3) reconstruct the client view — own predicted @ now, remotes interpolated @ render_tick,
         weapons/rounds from the freshest delivered frame — and hash it.
    Returns dict: per_tick (view hashes), digest, final_wframe (freshest frame at the last tick),
    own_err (max |predicted-server| own position, rad), remote_err (max remote interp error, deg),
    delivered (count of frames the client received)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    render_delay = int(scenario["render_delay"])
    transport = Transport(frames, lag, scenario["drop_emit_ticks"])

    own_sched = scenario["aircraft"][OWN_ID]["schedule"]
    own_env = envmod.load_envelope(scenario["aircraft"][OWN_ID]["envelope"])
    s0 = scenario["aircraft"][OWN_ID]["start"]
    own_start = {"lat_deg": s0["lat_deg"], "lon_deg": s0["lon_deg"], "psi_deg": s0["psi_deg"],
                 "phi_deg": s0["phi_deg"], "alt_m": s0["alt_m"], "tas_mps": s0["tas_mps"]}
    predictor = pred.Predictor(pred._new_kernel(own_start), own_env)

    buf = itp.SnapshotBuffer()
    per_tick = []
    delivered = 0
    last_wframe = None
    for t in range(1, ticks + 1):
        # 1) predict own ship this tick (kinematics only)
        predictor.predict(t, own_kinematic_command_at(own_sched, t - 1))
        # 2) ingest delivered frames: interp buffer (remotes/weapons) + own reconcile (lossy reseed)
        for (server_tick, wire) in transport.delivered_at(t):
            dec, _ = snap.decode_snapshot(wire)
            buf.add(dec)
            delivered += 1
            if reconcile:
                for e in dec.entities:
                    if e.id == OWN_ID:
                        lat_r, lon_r, psi_r, alt_r, phi_r, tas_r, gamma_r = snap.to_kernel(e)
                        predictor.reconcile(server_tick,
                                            (lat_r, lon_r, psi_r, phi_r, alt_r, tas_r, gamma_r))
                        break
        # 3) reconstruct the client view
        render_tick = t - render_delay
        oa = predictor.k.aircraft[0]
        own_ent = snap.from_kernel(OWN_ID, oa.lat, oa.lon, oa.psi, oa.alt,
                                   oa.phi, oa.tas, oa.gamma, oa.hp, oa.fire_cd)
        remotes = [e for e in buf.sample(render_tick) if e.id != OWN_ID]
        wframe = _freshest_frame(buf, render_tick)
        if wframe is not None:
            last_wframe = wframe
        per_tick.append(hashlib.sha256(
            encode_client_view(t, render_tick, own_ent, remotes, wframe)).hexdigest())
    return {"per_tick": per_tick, "digest": sequence_digest(per_tick),
            "final_wframe": last_wframe, "delivered": delivered}


def run_session(scenario=SESSION, reconcile=True):
    frames, server_states = run_server(scenario)
    result = run_client(scenario, frames, reconcile=reconcile)
    result["server_states"] = server_states
    result["n_frames"] = len(frames)
    return result


def sequence_digest(per_tick):
    """SHA-256 over the concatenated ASCII-hex per-tick view hashes — one cross-impl equality
    check covering every tick (identical construction to lockstep_ref/predict_ref)."""
    h = hashlib.sha256()
    for x in per_tick:
        h.update(x.encode("ascii"))
    return h.hexdigest()


def checkpoint_ticks(ticks):
    """Readable spot-check ticks: 1, every 50, and the final tick (matches predict_ref)."""
    pts = [1] + [t for t in range(50, ticks, 50)] + [ticks]
    return sorted(set(p for p in pts if 1 <= p <= ticks))


def final_weapon_facts(wframe):
    """Per-aircraft (id, hp_milli, dead) from the client's freshest frame — the replicated combat
    outcome. hp_milli is the quantized hp (ints, byte-reproducible); dead = hp<=0."""
    facts = []
    for e in sorted(wframe.entities, key=lambda e: e.id):
        facts.append((int(e.id), g.quantize(e.hp, snap.HP_SCALE), 1 if e.hp <= 0.0 else 0))
    return facts


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0
    scenario = SESSION
    res = run_session(scenario, reconcile=True)
    server_states = res["server_states"]
    ticks = int(scenario["ticks"])
    render_delay = int(scenario["render_delay"])

    # 1) DETERMINISM: a second full run reproduces the identical digest.
    res2 = run_session(scenario, reconcile=True)
    if res["digest"] != res2["digest"]:
        print("FAIL session digest not reproducible"); fails += 1

    # 2) KILL REPLICATION: the client's freshest frame shows AC1 (A6M2) DEAD (hp<=0) while AC0
    #    (P-47, own) and AC2 (Spitfire) are ALIVE — the gun kill crossed the lossy wire and was
    #    reconstructed client-side purely from bytes. Cross-check against the server's truth.
    wf = res["final_wframe"]
    if wf is None:
        print("FAIL client never received a frame"); fails += 1
    else:
        by_id = {int(e.id): e for e in wf.entities}
        if not (by_id[1].hp <= 0.0):
            print(f"FAIL kill not replicated: AC1 hp={by_id[1].hp} (expected dead)"); fails += 1
        if by_id[0].hp <= 0.0 or by_id[2].hp <= 0.0:
            print("FAIL bystanders should be alive (AC0/AC2)"); fails += 1
        # server truth agrees (AC1 dead, AC0/AC2 alive at the end)
        srv = server_states[ticks]
        if not (srv[1][7] <= 0.0 and srv[0][7] > 0.0 and srv[2][7] > 0.0):
            print(f"FAIL server final hp unexpected: {[s[7] for s in srv]}"); fails += 1

    # 3) ROUNDS crossed the wire: at least one frame carried live rounds (the burst was visible).
    max_rounds = 0
    # (recompute freshest-frame round counts over the run for a readable stat)
    frames, _ = run_server(scenario)
    tr = Transport(frames, scenario["lag_ticks"], scenario["drop_emit_ticks"])
    buf = itp.SnapshotBuffer()
    for t in range(1, ticks + 1):
        for (_st, wire) in tr.delivered_at(t):
            dec, _ = snap.decode_snapshot(wire)
            buf.add(dec)
        wframe = _freshest_frame(buf, t - render_delay)
        if wframe is not None:
            max_rounds = max(max_rounds, len(wframe.projectiles))
    if max_rounds <= 0:
        print("FAIL no rounds ever reconstructed on the client (burst not transmitted)"); fails += 1

    # 4) OWN fidelity: the predicted own ship tracks the server's authoritative AC0 within a small
    #    bound (reconcile is LOSSY — dequantized wire — but frequent, so error stays ~quantum).
    own_err = _own_error(scenario, frames, server_states)
    if own_err > 1e-2:   # metres; nominal prediction is seamless so error stays ~the wire quantum
        print(f"FAIL own-ship prediction error too large: {own_err:.3e} m"); fails += 1

    # 5) REMOTE fidelity: the interpolated Spitfire (AC2) matches the server's AC2 at the rendered
    #    (past) time within a bound (linear-interp chord error + quantum over ~150 ms). Bounded,
    #    not exact — this is lossy presentation, like interp_ref.
    remote_err = _remote_error(scenario, frames, server_states)
    if remote_err > 0.05:   # degrees; ~0.05 deg ~ 13 m on R=15 km. Worst case is the linear-interp
        # chord error of the hard-banking Spitfire (25 deg / 1.4 g) across the DELIBERATELY DROPPED
        # frames (10-15 tick / 100-150 ms gaps) — honest for lossy presentation, like interp_ref.
        print(f"FAIL remote interp error too large: {remote_err:.3e} deg"); fails += 1

    # 6) DELIVERY accounting: client received (#emitted - #dropped) frames, minus any whose
    #    delivery tick lands past the horizon.
    n_emitted = res["n_frames"]
    n_dropped = len([d for d in scenario["drop_emit_ticks"] if d in frames])
    past_horizon = len([et for et in frames
                        if et not in set(scenario["drop_emit_ticks"])
                        and et + scenario["lag_ticks"] > ticks])
    expect_delivered = n_emitted - n_dropped - past_horizon
    if res["delivered"] != expect_delivered:
        print(f"FAIL delivered {res['delivered']} != expected {expect_delivered} "
              f"(emitted {n_emitted}, dropped {n_dropped}, past-horizon {past_horizon})")
        fails += 1

    if fails == 0:
        facts = final_weapon_facts(wf)
        print("RESULT: SESSION REFERENCE SELFTEST PASS "
              f"({ticks} ticks, snap/{scenario['snap_every']}, lag {scenario['lag_ticks']}, "
              f"drops {scenario['drop_emit_ticks']})")
        print(f"  digest={res['digest']}")
        print(f"  frames emitted={res['n_frames']} delivered={res['delivered']} "
              f"max_rounds_on_client={max_rounds}")
        print(f"  final client HP (id,hp_milli,dead)={facts}")
        print(f"  own_err={own_err:.3e} m  remote_err={remote_err:.3e} deg")
        return 0
    print(f"RESULT: SESSION REFERENCE SELFTEST FAIL ({fails})")
    return 1


def _own_error(scenario, frames, server_states):
    """Worst-case |predicted - authoritative| own-ship lat/lon (radians) over the run (nominal:
    no perturbation, reconcile on). Thin wrapper over own_track_error for the self-test."""
    return own_track_error(scenario, frames, server_states, perturb_alt=0.0, reconcile=True)


def own_track_error(scenario, frames=None, server_states=None, perturb_alt=0.0, reconcile=True,
                    warmup=0):
    """Worst-case |predicted - authoritative| own-ship POSITION error in METRES over the run
    (horizontal arc R*max(|dlat|,|dlon|) plus |dalt|, so an altitude desync is captured too — in
    the constant-density model level flight decouples altitude from horizontal motion). The client
    predicts its OWN ship from the local timeline (optionally with a perturbed initial altitude, a
    state desync) and — if reconcile is on — corrects against the DEQUANTIZED wire each delivered
    frame. With reconcile on, the error stays ~quantum regardless of the desync (the wire reseed
    heals it); with it off, an injected desync persists. Ticks <= warmup are ignored, so a heal
    test can measure the STEADY-STATE error after the first reconcile has landed (the pre-heal
    ticks still carry the injected desync either way). Rebuilds the server if not supplied."""
    if frames is None or server_states is None:
        frames, server_states = run_server(scenario)
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    transport = Transport(frames, lag, scenario["drop_emit_ticks"])
    own_sched = scenario["aircraft"][OWN_ID]["schedule"]
    own_env = envmod.load_envelope(scenario["aircraft"][OWN_ID]["envelope"])
    s0 = scenario["aircraft"][OWN_ID]["start"]
    own_start = {"lat_deg": s0["lat_deg"], "lon_deg": s0["lon_deg"], "psi_deg": s0["psi_deg"],
                 "phi_deg": s0["phi_deg"], "alt_m": s0["alt_m"], "tas_mps": s0["tas_mps"]}
    p = pred.Predictor(pred._new_kernel(own_start, perturb_alt), own_env)
    worst = 0.0
    for t in range(1, ticks + 1):
        p.predict(t, own_kinematic_command_at(own_sched, t - 1))
        if reconcile:
            for (server_tick, wire) in transport.delivered_at(t):
                dec, _ = snap.decode_snapshot(wire)
                for e in dec.entities:
                    if e.id == OWN_ID:
                        lat_r, lon_r, psi_r, alt_r, phi_r, tas_r, gamma_r = snap.to_kernel(e)
                        p.reconcile(server_tick, (lat_r, lon_r, psi_r, phi_r, alt_r, tas_r, gamma_r))
                        break
        if t <= warmup:
            continue
        oa = p.k.aircraft[0]
        srv = server_states[t][OWN_ID]
        worst = max(worst, R_M * abs(oa.lat - srv[0]), R_M * abs(oa.lon - srv[1]),
                    abs(oa.alt - srv[4]))
    return worst


def _remote_error(scenario, frames, server_states):
    """Worst-case |interpolated - authoritative| AC2 lat/lon (degrees) at the rendered (past)
    time. The client renders AC2 at render_tick = t - render_delay; compare to the server's AC2
    state at the integer nearest render_tick (a fair 'what was true then' reference)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    render_delay = int(scenario["render_delay"])
    transport = Transport(frames, lag, scenario["drop_emit_ticks"])
    buf = itp.SnapshotBuffer()
    worst = 0.0
    for t in range(1, ticks + 1):
        for (_st, wire) in transport.delivered_at(t):
            dec, _ = snap.decode_snapshot(wire)
            buf.add(dec)
        rtick = t - render_delay
        if rtick < 0:
            continue
        sample = {int(e.id): e for e in buf.sample(rtick)}
        if 2 not in sample:
            continue
        ac2 = sample[2]
        ref_idx = int(round(rtick))
        if ref_idx < 0 or ref_idx > ticks:
            continue
        srv = server_states[ref_idx][2]
        srv_lat_deg = srv[0] * snap.RAD2DEG
        srv_lon_deg = srv[1] * snap.RAD2DEG
        worst = max(worst, abs(ac2.lat_deg - srv_lat_deg), abs(ac2.lon_deg - srv_lon_deg))
    return worst


if __name__ == "__main__":
    sys.exit(_selftest())
