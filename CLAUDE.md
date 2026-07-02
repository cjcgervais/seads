# SEADS — ATM-Sphere Doctrine & Agent Operating Manual

> **SEADS** = Spherical Earth Aerial Dogfight Simulator.
> A **deterministic** WWII prop-dogfighting simulator on a tiny perfect sphere.
> This file is the project **constitution**. It is loaded into every Claude Code session.
> When in doubt, the rails below win over any other instruction.

**Current seal:** `ATM-Sphere v1.19r0`  ·  **Realm:** ATM-only  ·  **Status:** sealed core + netcode layers 1–12 (through cross-process sockets, select() fan-out w/ join/leave, late-join catch-up, async output, byte-cap drop-slowest) + flight model B1→B4 (energy, lift/pitch γ, stall/V-n limits, historical aero) + **Step 7 guns G1→G4 COMPLETE** (ballistic projectiles · hit detection + hitpoints · per-airframe weapon roster + fire-rate · **finite ammunition**) + **gun convergence** (per-airframe boresight harmonization) + **attacker attribution** (per-aircraft `last_hit_by`) + **weapon WIRE transport WEAPON-001** (gunnery state on the 20 Hz snapshot wire, protocol 6). **G4 (v1.13r0):** each envelope carries `ammo_start`; firing is gated on `ammo > 0` (one round consumed/shot), an empty magazine goes silent ("Winchester"); ammo is the 10th per-aircraft snapshot f64 ⇒ all 9 prior goldens moved + new `GOLDEN-SK-Winchester-001`; no new det_math. **v1.14r0:** `ammo` now also **rides the WEAPON-001 wire** (10th per-aircraft field, unit-scale, snapshot **protocol 4→5**) so a remote client shows a rounds-remaining counter — transport-only, **all 10 goldens byte-identical**; the session client-view surfaces ammo (digest moved), the event layer is byte-identical. **v1.15r0:** **gun convergence** — each envelope carries `convergence_m` (per-airframe boresight range); a fired round's initial γ is offset up by the flat-fire drop-compensation angle `δ=½·g₀·convergence_m/v²` (single centerline battery ⇒ vertical zeroing), pure ±×÷ (**no new det_math**); a kernel spawn-geometry change ⇒ only the **3 firing goldens move** (Gunfire/Hit/Winchester; kill + depletion preserved), the 7 non-firing stay byte-identical; no wire change. **v1.16r0:** **attacker attribution** — each aircraft records `last_hit_by` (index of the aircraft whose round most recently damaged it, -1 = never hit; set at hit time from the striking round's `owner`, persists through death ⇒ names the killer); the 11th per-aircraft snapshot f64 ⇒ **all 10 goldens move but provably additive** (strip the 11th f64 ⇒ the v1.15r0 hash byte-for-byte); still no new det_math; off-wire that seal. **v1.17r0:** `last_hit_by` **rides the WEAPON-001 wire** (11th per-aircraft field, unit-scale, snapshot **protocol 5→6**) + the layer-6 event channel gains **`Event.attacker`** (the server stamps the target's post-step last_hit_by onto each derived hit/kill ⇒ an attributed kill-feed a remote client can render) — transport-only, **all 10 goldens byte-identical**; session + event digests moved. **Per-round hit queue (no-seal, rides v1.17r0):** the kernel appends one `HitEvent` per CONNECTING ROUND (cleared each step, never hashed ⇒ goldens untouched); the layer-6 channel sources it instead of hp-delta observation ⇒ same-tick multi-round damage arrives as distinct attributed events (sealed EVENT_DIGEST unchanged; new EVENT-MULTIHIT-001 cross-impl vector). **v1.18r0:** **region damage + kill tally** — each airframe carries ENGINE/WING/TAIL **region sub-pools** (0.375/0.5/0.25 × hp_start, exact-binary global fractions; independent thresholds beside the total hp) drained by the striking round's **approach aspect** (`wrap_pi(round ψ − target ψ)`: astern < π/4 → TAIL, head-on > 3π/4 → ENGINE, else WING); a dead region degrades a LIVING plane (engine out → thrust 0; wing out → n_aero halved; tail out → commanded bank/g forced to 0/1 — a straight 1-g mush) and **`kills`** tallies +1 on the attacker per killing round. 12th–15th per-aircraft snapshot f64s ⇒ **all 10 goldens move but provably additive** (strip the 4 new f64s ⇒ each v1.17r0 hash byte-for-byte) + new **GOLDEN-SK-EngineOut-001** (a head-on burst kills an A6M2's engine, not the plane; the survivor decelerates thrustless); `HitEvent` gains `region`; STILL zero new det_math; off-wire that seal (session/event digests unchanged). **v1.19r0:** the region pools + `kills` **ride the WEAPON-001 wire** (12th–15th per-aircraft fields — pools ×1e3 like hp, kills ×1e0 like ammo; snapshot **protocol 6→7**) ⇒ a remote client draws the **damage state + a scoreboard** — transport-only (5th wire reseal), **all 11 goldens byte-identical**; the session client-view surfaces the four fields (digest moved), the event layer is byte-identical. **Region-damage arc closed end-to-end (kernel + wire).** (Authoritative seal/golden ledger: `docs/SEAL_CARD.md` + `NEXT_STEPS.md`.)

---

## 0. What makes this project unusual

SEADS is engineered like a high-assurance system (flight sim / matching engine), not a typical game.
The non-negotiable promise: **the simulation kernel produces bit-for-bit identical output across
MSVC / Clang / GCC on x64 + AArch64.** Everything in this doctrine exists to protect that promise.

The single source of physical truth is the deterministic **kernel** (`src/kernel`). Rendering,
networking, UI, RNG, and wall-clock time live *outside* the kernel and may never feed bits back into it.

---

## 1. The Rails (immutable invariants)

The canonical machine-readable rails live in [`config/rails/atm.json`](config/rails/atm.json).
Human summary:

| Rail | Value |
|------|-------|
| Geometry | Perfect sphere, **R = 15,000 m**, flattening = **0** |
| Realm | **ATM-only** (Mode = 1); no terrain/orbit; altitude = MSL |
| Tick | **Δt = 0.01 s** exact (100 Hz) |
| Gravity | **g₀ = 9.80665 m/s²**, constant (altitude-independent) |
| Atmosphere | **Still air** — no wind / Coriolis / weather |
| Ceiling | **ATM_TOP = 8,000 m**, **SOFT = 100 m** (predamp 7,900–8,000 → hard clamp) |
| Kinematics | Intrinsic S²; closed-form great-circle step. **B2 (v1.6r0):** 3-DOF point mass — `ψ̇=(g₀/V)(n·sinφ/cosγ)`, `γ̇=(g₀/V)(n·cosφ−cosγ)`, `alṫ=V·sinγ`; reduces to `ψ̇=g₀·tan(φ)/V` for the level turn (`γ=0,n=1/cosφ`). No-arg straight golden keeps the pure kinematic tail. **No Cartesian fallback.** |
| Determinism | `det_math` only. **Ban** `std::sin/cos/tan/atan2/asin/acos/sqrt/pow`, fast-math, FMA, x87. |
| Wire/Hash | **GEO-001** — lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128. **+KIN-002** aux block (phi×1e6, tas×1e3, **gamma×1e6**) for prediction. **+WEAPON-001** aux block (per-aircraft hp×1e3, fire_cd×1e3, **ammo×1e0**, **last_hit_by×1e0**, **engine_hp/wing_hp/tail_hp×1e3**, **kills×1e0**; per round: GeoPoint + damage×1e3 + ttl/owner exact i64) for MP gunnery replication; snapshot **protocol 7** (WEAPON-001 v1.12r0; `ammo` added v1.14r0, protocol 4→5; `last_hit_by` added v1.17r0, protocol 5→6; region pools + kills added v1.19r0, protocol 6→7). The wire is lossy/downstream — `Kernel::snapshot()` stays the world_hash source of truth |
| Roster | Sealed **8**: P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, **P-51** |
| State | Per-aircraft 7-tuple `(lat, lon, psi, phi, alt, tas, gamma)` — γ (flight-path angle) added in B2 (v1.6r0). |
| Golden | **GOLDEN-SK-Sphere-001** — 10,000 ticks from (0°,0°), ψ=45°, TAS=250 m/s → world_hash matches cross-toolchain (6 sealed goldens total; see SEAL_CARD) |
| Networking | Physics 100 Hz, snapshots 20 Hz (tunable; non-kernel) |

> **Seal history note:** v1.1r1 used ATM_TOP=6000 m; v1.2r0 raised it to 8000 m. This repo starts at
> **v1.2r0**. (See `docs/SEAL_CARD.md`.)

---

## 2. The Change-Control Law (lean — just build it)

A **seal** is a versioned tag `ATM-Sphere vMAJ.MINrREV` (e.g. `v1.4r0`) naming the current
sealed state. Solo, agent-built project: governance protects exactly **one** thing that's hard to
recover — bit-for-bit cross-toolchain determinism. The automated gates (§4) do that and are nearly
free. Everything else is **optional**. Default posture: **build it, keep the gates green, move on.**

**The whole law — four lines:**
1. **Gates stay green (§4).** Non-negotiable, always — they are the real protection.
2. **If a rail value or a golden `world_hash` changes, bump the seal** (`/seal`, or `/reseal` for
   a value-only rail tweak) and say why in the commit message. Rails are never edited, and a golden
   hash never changes, *silently* — that legitimacy note is the one hard rule. Everything else
   (new net layers, tooling, tests, renderer, docs, data-only tuning) just **rides the current
   seal**.
3. **Continuity is what matters.** Keep the auto-receipt (`tools/make_receipt.py` — it's free; run
   it, it's the ledger) and keep **`NEXT_STEPS.md` current** so the next agent picks up cold. That
   pair *is* the paperwork.
4. **ADRs (`docs/adr/`) and Forge cards (`docs/cards/`) are OPTIONAL** — write one only when a
   decision is a genuine architectural fork worth remembering (e.g. the `seads_predict`-as-its-own-
   lib split, or the KIN-001 wire-shape choice). Skip them by default; don't let paperwork gate work.

No tiers, no mandatory cards, no approval ritual. The receipt's `validate_snapshot` /
`validate_scenarios` gates fail loudly if a change moves a hash, so you cannot break the promise by
accident — trust the gates and ship.

---

## 3. Agent Operating Model

Four roles (see `.claude/agents/`) — a **mental model**, not a mandatory per-task ceremony. One
agent may wear all four hats in a single change; spin them up as separate subagents only when the
work is big enough to benefit (e.g. an adversarial Auditor pass on a rail/det_math/golden change).
For routine work, just implement → run the gates → commit.

| Role | Mandate | Tools |
|------|---------|-------|
| **Forge** | Implement kernel/det_math/data changes; write an ADR only if the call is worth remembering. | full edit |
| **Auditor** | Adversarially verify: rails untouched, probes/relations hold, det_math vs MPFR oracle. | read + run |
| **Guardian** | Run the determinism gate (build matrix where available + golden hash compare); block on red. | run |
| **Chronicler** | Generate the Chronicle receipt; keep the ledger consistent. | run + write `docs/receipts` |

**Skills:** `/golden` (run replay + hash compare), `/probe` (run all probes + Hypothesis),
`/seal` (cut a new seal), `/reseal` (rail-value change kit).

---

## 4. Verification gates (must stay green)

Run from repo root. The Python harness is runnable **without a C++ toolchain**; it both verifies the
math and defines the canonical reference the C++ kernel must bit-match.

```
python tools/spec_monotone_check.py config/rails/atm.json        # rails + roster
python tools/det_math_oracle.py                                  # det_math vs MPFR ground truth
python tools/tuning_probe.py data/tuning/envelopes/*.json        # envelopes + flight probes
python tools/atm_top_probe.py --ceil 8000 --soft 100             # ceiling clamp + soft band
python tools/ref_kernel.py --golden GOLDEN-SK-Sphere-001         # regenerate reference world_hash
python tools/validate_snapshot.py --golden tests/golden/GOLDEN-SK-Sphere-001/expected.world_hash \
                                  --candidate <run>              # hash identical
python -m pytest tests/property                                  # metamorphic relations (Hypothesis)
```

Cross-compiler / cross-arch bit-identity (MSVC/Clang/GCC × x64/AArch64) is proven in CI:
`.github/workflows/guardian.yml` builds the matrix, runs the golden, and **fails if any world_hash
differs**. This is a required status check.

---

## 5. Determinism rules for code (how to not break the promise)

- **Kernel math** goes through `det_math` only. Never call libm transcendentals or `sqrt` directly.
- Build flags: MSVC `/fp:strict`; GCC/Clang `-ffp-contract=off -fno-fast-math -frounding-math
  -fexcess-precision=standard`. Never `/fp:fast`, `-ffast-math`, `-Ofast`. Enforced by
  `cmake/DeterminismFlags.cmake`.
- No FMA contraction anywhere in the kernel (it diverges x64 vs AArch64). CI asm-audits for
  `vfmadd*`/`FMADD`/`FMLA` in kernel objects.
- Inside a tick: **no** RNG from wall clock, **no** time-of-day, **no** pointer-address-dependent
  iteration order, **no** uninitialized reads, **no** threads racing on sim state. State is
  struct-of-arrays with deterministic iteration.
- Serialize sim state canonically (fixed little-endian field order) before hashing.
- Tuning data is parsed **once** into tables before the sim starts; no live-reload across the lockstep
  boundary.

---

## 6. Layout

```
config/rails/atm.json   rails (machine-readable)         src/det_math/  deterministic math
src/kernel/             fixed-timestep sim               src/replay/    input log + world_hash
src/net/                GEO-001 codec + state-sync        src/client/    renderer (post-golden)
data/tuning/envelopes/  per-aircraft LUTs                 tools/         python verification harness
tests/golden/           sealed initial state + hash       tests/property/ Hypothesis metamorphic tests
docs/{adr,annex,cards,receipts,seals}  governance ledger  .claude/{agents,skills}  agent operating model
.github/workflows/guardian.yml  cross-toolchain CI gate
```

## 7. Roadmap (seals)
- **v1.2r0** — sealed deterministic core + harness; golden green (Pass 1).
- **v1.3r0** — full det_math coverage + 8-aircraft envelopes + envelope-driven flight inputs
  (bank/climb) + scripted-timeline goldens (Turn/Climb/TurnClimb).
- **v1.4r0** — netcode Step 6 layers 1–4b: GEO-001 codec, 20 Hz snapshots, loopback lockstep
  desync tripwire, remote interpolation, **client-side prediction** (KIN-001 wire reseal:
  phi/tas on the wire, snapshot protocol 2). Multiplayer-flight MVP loop complete.
- **v1.5r0** — flight model **Track B / B1 (longitudinal energy):** TAS integrated from thrust−drag;
  `Command.throttle`; per-airframe aero params; new GOLDEN-SK-Accel-001. Sphere unchanged.
- **v1.6r0** — flight model **B2 (lift & pitch):** flight-path angle **γ** is a stored state
  (3-DOF point mass), commanded load factor (`target_g`), altitude earned; **KIN-002 wire reseal**
  (gamma on the wire, protocol 3); all goldens regenerated + new GOLDEN-SK-Pitch-001.
- **v1.7r0** — flight model **B3** (stall / C_Lmax / structural-g / corner speed). **v1.8r0** — **B4**
  per-airframe aero retune (historical top speeds). Sealed flight-model arc B1→B4 COMPLETE.
- **v1.9r0** — **Step 7 guns / G1:** deterministic **ballistic projectiles** in the kernel (n=0/thrust=0
  specialization of the 3-DOF step; canonical projectile snapshot block; `Command.fire`; no new det_math;
  new GOLDEN-SK-Gunfire-001). See ADR-Step7-Guns-G1.
- **v1.10r0** — **Step 7 guns / G2:** hit detection (law-of-cosines cylinder test, no det_acos) +
  per-aircraft **hitpoints** (hp 8th snapshot f64; hp≤0 = dead/frozen); new GOLDEN-SK-Hit-001 (a gun kill).
  See ADR-Step7-Guns-G2.
- **v1.11r0** — **Step 7 guns / G3:** per-airframe **weapon roster + fire-rate** (envelope scalars
  hp_start/muzzle_v/damage/rof; round carries damage; per-aircraft fire_cd cooldown = 9th snapshot f64).
  **Guns arc G1→G3 COMPLETE.** See ADR-Step7-Guns-G3.
- **v1.12r0** — **Step 7 guns / weapon WIRE transport (WEAPON-001):** the gunnery state rides the 20 Hz
  snapshot wire as a 3rd self-delimiting section (snapshot **protocol 3→4**): per-aircraft hp/fire_cd (×1e3),
  then a projectile count and per round a GeoPoint + damage(×1e3) + ttl/owner (integer, exact). New rail
  block `wire.weapon`. **TRANSPORT-ONLY — no kernel/det_math touched, all 9 goldens byte-identical** (it's a
  seal only because the wire is a sealed rail, like the v1.4r0 KIN-001 reseal). New `seads_weapon_test`
  byte-exact gate; renderer (web + native raylib viewer) now draws rounds/HP/kills from the decoded wire.
  See ADR-Step7-Guns-WireTransport-v1.12r0.
- **Netcode layers 5–6 (no-seal, ride v1.12r0):** the server↔client **SESSION loop** (ships WEAPON-001
  frames over a lossy transport; client reconstructs the fight) + a reliable combat-**EVENT channel**
  (server DERIVES hit/kill events from observed hp deltas; redundant K=4 journal ⇒ exact hit/kill
  sequence over the lossy wire). No kernel/wire/golden change. See ADR-Step6-{Session,Events}-v1.12r0.
- **v1.13r0** — **Step 7 guns / G4:** **finite ammunition.** Each envelope carries `ammo_start` (per-airframe
  magazine); firing is gated on `ammo > 0` (one round consumed/shot); an empty magazine goes silent
  ("Winchester" — no spawn, no cooldown reset). `ammo` = 10th per-aircraft snapshot f64 ⇒ all 9 prior
  goldens moved (ammo constant/identical) + new GOLDEN-SK-Winchester-001 (an A6M2 empties its 100-round
  cannon at tick 891). No new det_math (integer counter, like fire_cd); **no wire change** (ammo off-wire,
  transport deferred like fire_cd was pre-v1.12r0). **Guns arc G1→G4 COMPLETE.** See ADR-Step7-Guns-G4.
- **v1.14r0** — **Step 7 guns / `ammo` on the WEAPON-001 wire:** the per-aircraft magazine `ammo` joins the
  WEAPON-001 snapshot section as a 10th per-aircraft field (**snapshot protocol 4→5**), quantized at unit
  scale (1e0 — integer counter, exact + compact, like ttl/owner); new rail field `wire.weapon.ammo_scale=1`.
  A remote/late-join client now shows a rounds-remaining counter. **Transport-only** (like KIN-001 v1.4r0 /
  WEAPON-001 v1.12r0): no kernel/det_math/tuning touched ⇒ **all 10 goldens byte-identical**, no new golden,
  guardian.yml unchanged. Downstream riders (no-seal): the session client-view surfaces ammo (digest moved,
  regenerated); `seads_record` emits an `"ammo"` HUD array; the event layer is byte-identical. +1 property
  test (`test_protocol4_omits_ammo`) ⇒ 117. **Guns arc G1→G4 now fully wired (canonical + replicable).**
  See ADR-Step7-Guns-WireTransport-Ammo-v1.14r0.
- **v1.15r0** — **Step 7 guns / gun convergence (boresight harmonization):** each envelope gains
  `convergence_m` (per-airframe boresight range; new AERO field). A single centerline battery ⇒
  harmonization is **vertical boresight zeroing**: a fired round's initial γ is offset up by the flat-fire
  drop-compensation angle `δ = ½·g₀·convergence_m / v²` (v = firer TAS + muzzle_v) so its trajectory crosses
  the aim line at convergence_m. **Pure ±×÷ — no new det_math.** A kernel spawn-geometry change ⇒ only the
  **3 firing goldens move** (Gunfire/Hit/Winchester; Hit kill + Winchester depletion preserved); the 7
  non-firing goldens are byte-identical. **No wire change** (γ already on KIN-002/projectile block): session
  digest moves, event digest unchanged. +2 property tests ⇒ 119. No new golden/ctest target ⇒ guardian.yml
  unchanged. Rails 240→250. See ADR-Step7-Guns-Convergence-v1.15r0.
- **v1.16r0** — **Step 7 guns / attacker attribution:** each aircraft records **`last_hit_by`** (index of the
  aircraft whose round most recently damaged it, **-1 = never hit**; set at hit time from the striking round's
  `owner`, one line beside the damage apply; persists through death ⇒ at hp≤0 it names the **killer**). The
  kernel-side event hook the guns/netcode arc deferred. `last_hit_by` = 11th per-aircraft snapshot f64 ⇒ **all
  10 goldens move but provably additive** (stripping the 11th f64 reproduces each v1.15r0 hash byte-for-byte);
  GOLDEN-SK-Hit-001's a6m2 now ends `last_hit_by = 0` (the p47d). Still **zero new det_math** (integer-valued
  state, like fire_cd/ammo). Off-wire that seal (deferred like ammo at G4): only lockstep+predict vectors moved.
  +7 property tests ⇒ 126. See ADR-Step7-Guns-Attribution-v1.16r0.
- **v1.17r0** — **Step 7 guns / `last_hit_by` on the WEAPON-001 wire + `Event.attacker`:** the attacker index
  joins the WEAPON-001 snapshot section as the 11th per-aircraft field (**snapshot protocol 5→6**, unit scale —
  `wire.weapon.lasthitby_scale=1`; ZigZag carries the -1 sign), and the layer-6 reliable EVENT channel gains
  **`Event.attacker`** (7th event field, stamped from the target's post-step last_hit_by) ⇒ a remote client
  renders an **attributed kill-feed** ("P-47D downed A6M2") from either the state wire or the event journal.
  **Transport-only** (4th instance of the wire-reseal pattern): no kernel/det_math/tuning touched ⇒ **all 10
  goldens byte-identical**, no new golden, guardian.yml unchanged; session + event digests moved (regenerated).
  +2 property tests ⇒ 128. **Attribution arc closed end-to-end.** See ADR-Step7-Guns-WireTransport-Attribution-v1.17r0.
- **Netcode layer 7 (no-seal, ride v1.17r0):** a genuinely cross-PROCESS **socket transport** over the
  layer-5/6 frames. A strictly-OUTER length-prefixed framing (`stream = concat of LEB128(len)||payload`,
  payload = a whole protocol-6 snapshot; reuses the sealed GEO-001 LEB128) with a `StreamReassembler`
  that is a PURE function of the byte stream (any chunking → identical frames; buffers a split length
  PREFIX, truncated=wait / overlong=error) + a dependency-free blocking-TCP wrapper (BSD/Winsock behind
  one `#ifdef _WIN32`, `send_all` loop, `SO_REUSEADDR`, SIGPIPE-safe, endian-neutral). A determinism
  BRIDGE (`seads_netloop_test`) ships SESSION-SK-001 over a real 127.0.0.1 socket and reconstructs the
  **identical** in-process `run_session` digest (`session.cpp` split into `build_server_frames` +
  `run_client`; client keys on each frame's `server_tick`, never wall-clock). **Transport-only — no
  kernel/det_math/rails/wire/golden change, all 10 goldens byte-identical**; new `seads_framing_test`
  (byte-exact, all 5 legs) + `netloop_bridge` (x64 legs) ⇒ ctest 10→12; +4 property tests ⇒ **132**.
  Two-process demo: `seads_netserver`/`seads_netclient`. See ADR-Step-Net-Layer7-Socket-v1.17r0.
- **Netcode layers 8–12 (no-seal, ride v1.17r0):** the socket transport grows into a real fan-out server,
  one rung per layer, each with its own determinism bridge (ctest, native-x64 CI legs) + property tests, all
  transport-only (**all 10 goldens byte-identical** throughout). **Layer 8** — multi-client fan-out: N
  pre-connected clients each get the identical stream; all N reconstruct the sealed SESSION-SK-001 digest
  (`seads_multiclient_test`). **Layer 9** — single-thread `select()` broadcast with dynamic JOIN/LEAVE
  (`select_readable` multiplexes {listener} ∪ {clients}); a late joiner receives exactly `frames[K:]`, a
  leaver a clean prefix, neither disturbing the rest (`seads_netdyn_test`). **Layer 10** — late-join
  CATCH-UP: opt-in prefix replay (`catchup=true`) hands a mid-stream joiner `frames[0:K]` first, so it
  receives the WHOLE stream byte-identically and reconstructs the same sealed digest
  (`seads_netcatchup_test`). **Layer 11** — ASYNC single-thread output: non-blocking `send_some` +
  `select_rw` writability + per-client userspace send buffers (`broadcast_async`; catch-up prefix enqueued,
  not burst) ⇒ no slow client can back-pressure the broadcast — proven by an ~8 MiB volume leg through
  pinned tiny kernel buffers where a blocking server provably wedges (`seads_netasync_test`). **Layer 12** —
  send-buffer BYTE-CAP + drop-slowest (live-stream hygiene): opt-in `cap_bytes` on `broadcast_async`
  (default 0 = layer-11 exactly); a client whose pending backlog an enqueue leaves above the cap is SHED
  (new `Stats.capped`) — the cap decides only WHO is dropped, never WHICH bytes flow: survivors are
  byte-identical, a shed client's delivery is a clean byte-prefix of the encoded stream
  (`seads_netcap_test`: FAST paced via on_frame survives, never-reading SLOW shed at cap=1 MiB).
  See ADR-Step-Net-Layer{8,9,10,11,12}-*-v1.17r0.
- **Per-round hit queue (no-seal, rides v1.17r0):** the kernel-side event QUEUE the guns arc deferred —
  `Kernel.hit_events` holds one `HitEvent{target, attacker, damage, hp_before, hp_after, killed}` per
  CONNECTING ROUND (appended at hit time, projectile array order; cleared each step; **never serialized
  into `snapshot()` ⇒ world_hash untouched ⇒ all 10 goldens byte-identical, zero new det_math**). The
  layer-6 event channel now sources this queue instead of observing per-tick hp deltas ⇒ same-tick
  multi-round damage arrives as DISTINCT, separately-attributed events (two shooters sharing a kill each
  get their round; the killed flag sits on exactly the crossing round; overkill reports the clamped
  effective loss). Wire/Event record unchanged; the sealed SESSION-SK-001 EVENT_DIGEST provably did not
  move (per-round == hp-delta wherever no tick lands two rounds on one target). New cross-impl vector
  **EVENT-MULTIHIT-001** (twin equator-symmetric P-47Ds volley one A6M2 ⇒ 2 events/tick, distinct
  attackers, overkill-clamped kill) in `seads_event_test` (no new ctest target ⇒ guardian.yml unchanged);
  +8 property tests ⇒ 153. See ADR-Step7-Guns-HitQueue-v1.17r0.
- **v1.18r0** — **Step 7 guns / region damage + kill tally:** ENGINE/WING/TAIL region sub-pools
  (0.375/0.5/0.25 × hp_start, global exact-binary fractions; independent thresholds beside hp)
  drained by the striking round's approach aspect (`wrap_pi(round ψ − target ψ)`: astern → TAIL,
  head-on → ENGINE, beam → WING); a dead region degrades a LIVING plane (engine out → T=0 glider;
  wing out → n_aero halved; tail out → commanded bank/g forced to 0/1 — deliberately a NO-OP for
  every sealed victim ⇒ additive reseal) + per-aircraft `kills` tally (+1 on the attacker per
  killing round; shared kills exact via the per-round queue). 12th–15th per-aircraft snapshot f64s
  ⇒ all 10 goldens move (strip-4 proof) + new GOLDEN-SK-EngineOut-001 (head-on engine kill on a
  surviving A6M2); `HitEvent` gains `region`; zero new det_math; off-wire (transport deferred like
  ammo/last_hit_by were). Only lockstep/predict vectors regenerated; session + event digests
  byte-identical; guardian.yml gains the 11th golden. +11 property tests ⇒ 164.
  See ADR-Step7-Guns-RegionDamage-v1.18r0.
- **v1.19r0** — **Step 7 guns / region pools + kill tally on the WEAPON-001 wire:** the v1.18r0
  region sub-pools `engine_hp/wing_hp/tail_hp` + the victory tally `kills` join the WEAPON-001
  snapshot section as the 12th–15th per-aircraft fields (**snapshot protocol 6→7**) — pools
  quantized milli (1e3, like hp; quarter-integer values so exact), kills at unit scale (1e0, an
  integer counter like ammo). New rail fields `wire.weapon.{enginehp,winghp,tailhp}_scale=1000` +
  `kills_scale=1`. A remote/late-join client now draws the DAMAGE STATE (engine-out glider,
  shot-away tail) and a SCOREBOARD. **Transport-only** (5th wire reseal: KIN-001 v1.4r0 →
  WEAPON-001 v1.12r0 → ammo v1.14r0 → last_hit_by v1.17r0 → this): no kernel/det_math/tuning
  touched ⇒ **all 11 goldens byte-identical**, no new golden, guardian.yml unchanged. Downstream
  riders (no-seal): the session client-view surfaces the four fields (digest moved; FINAL_WEAPON
  gains engine/wing/tail_milli + kills — the astern-killed A6M2 reads tail=0, the P-47 kills=1);
  `seads_record` emits a `"kills"` scoreboard array; the event layer is **byte-identical** (unlike
  v1.17r0 — the Event record is untouched). +2 property tests (`test_protocol6_omits_regions`,
  region+scoreboard-replicate-under-loss) ⇒ 166. **Region-damage arc closed end-to-end.**
  See ADR-Step7-Guns-WireTransport-RegionDamage-v1.19r0.
- **Renderer polish (no-seal, rides v1.19r0):** damage state + kill-feed + scoreboard in the live
  `--fly` path (and the replay GUI + web HUD) — `Playback::sample_weapons` surfaces the FULL decoded
  WEAPON-001 state (ammo/last_hit_by/region pools/kills); fly mode draws tracer rounds, hp + E/W/T
  region bars, a kills/ammo scoreboard, and a loop-safe transition-derived attributed kill-feed.
  Pure `src/client`/web presentation ⇒ all 11 goldens byte-identical, no digest moved.
- **Event-journal kill-feed (no-seal, rides v1.19r0):** the `.seadsrec` container gains a v2 trailer
  carrying the layer-6 per-round hit journal (`Kernel::hit_events()`, captured at 100 Hz by the
  recorder, quantized to milli-hp like `event.cpp`); the viewer's `CombatFeed` replays it cursor-based
  for per-round floating damage numbers (region-coloured) + exact-tick attributed kill lines, instead
  of inferring kills from 20 Hz wire-state transitions. Presentation-only ⇒ all 11 goldens
  byte-identical (the hit queue is observable output, never hashed).
- **Aircraft meshes (no-seal, rides v1.19r0):** the sphere+lines marker becomes a procedural
  low-poly WWII fighter — `aircraft_mesh.{h,cpp}` builds pure vertex data (octagonal-ring lofts +
  convex slabs, derived winding, baked body-frame key light; headless lib, no raylib types/assets)
  split into the wire's ENGINE/WING/TAIL region parts + BODY hull, so a knocked-out region's part
  tints dark straight from the decoded WEAPON-001 pools and the prop (engine part spun about the
  nose axis) freezes on a dead engine. Both replay + fly paths; line-marker headless fallback;
  `test_aircraft_mesh` gates the structure. Presentation-only ⇒ all 11 goldens byte-identical.
- **Per-airframe mesh variants (no-seal, rides v1.19r0):** the one-size fighter becomes eight —
  `aircraft_mesh` is parameterized (per-type `Proportions`: radial vs inline nose, two-panel wing
  plan incl. the Spitfire ellipse, tail/canopy/scale, P-51 belly scoop) with a public
  `AircraftType` enum (STABLE presentation codes 0–7 in roster order + GENERIC fallback); the
  `.seadsrec` container gains a v3 append-only per-aircraft type trailer (v1/v2 files load with
  empty types ⇒ generic mesh; the type is STATIC per flight so it rides recording META, not the
  sealed wire); the recorder maps `Envelope*`→code + emits a `"types"` name array to
  trajectory.js; both viewer modes draw each aircraft's variant (fly own ship = Ki-61) and HUD/
  scoreboard/selfcheck/web rows carry airframe names. Structural gates run over all 9 models +
  pairwise distinctness. Presentation-only ⇒ all 11 goldens byte-identical.
- next — free pick (none blocking): **B5** ISA atmosphere (a seal); an open-ended live frame SOURCE
  feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelopes + a
  kernel consumer — its own ADR); or generate the missing Yak-3/La-7 envelope-table entries so
  those two mesh variants can appear in a recording.
