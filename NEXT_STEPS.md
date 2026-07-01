# SEADS 2026 — Next Steps (handoff)

> ## ►► CURRENT STATE (2026-06-30): seal **ATM-Sphere v1.12r0** — **NETCODE LAYER 6: reliable combat-EVENT channel DONE ✅ (no-seal, rides v1.12r0)**
> **Latest (no-seal): the transient combat MOMENTS now replicate RELIABLY over the same lossy wire.**
> Layer 5 replicates combat STATE (HP/positions/rounds) via nearest-frame — HP is *idempotent*, so a
> dropped frame is healed by the next. But a **HIT** ("T lost D hp this tick" → impact spark/damage
> number) and a **KILL** ("T died at *this* tick" → kill-feed) are transient EVENTS, not idempotent:
> read off the nearest STATE frame they smear the exact tick, lump aggregate hp, and vanish if that
> frame drops. New **netcode layer 6 — a reliable EVENT channel** (`tools/event_ref.py` ↔
> `src/net/event.{h,cpp}`, new `seads_event` lib) fixes that. The **server DERIVES events by OBSERVING
> the authoritative kernel's hp deltas** each tick (before/after `step()` — **the kernel is NOT
> modified**, pure observation, so all 9 goldens stay byte-identical); each 20 Hz frame piggybacks the
> **last EVENT_WINDOW_K=4 events** (a session-layer message — **NOT** a new snapshot-wire section, so
> **no wire reseal**); the **client applies a de-duped, append-only journal** by monotonic `seq`. So it
> reconstructs the **exact event sequence** — over SESSION-SK-001 the P-47's burst walks the A6M2's hp
> **70→0 in 5 hits + 1 kill at tick 54** — **bit-for-bit** even though 5 frames drop. **Reliability
> bound is explicit + gated:** an event is lost only if **K consecutive frames carrying it all drop**;
> under the scenario's isolated single drops the reconstruction is COMPLETE (`applied == server log`),
> and the tail (incl. the kill) rides *every* later frame so it's essentially always delivered. A
> **blackout vector** (`BLACKOUT_DROPS={40,45,50}` covers the whole in-window life of the two earliest
> hits) proves the bound cross-impl: the client recovers exactly `{2,3,4,5}` — aged-out hits lost, the
> **journal RESYNCS (no head-of-line block), the kill still delivered** — and both the full digest
> `dfcc1aaf…` and the `BLACKOUT_DIGEST 94ae31ea…` are reproduced by the C++ mirror. **Lossy ≠
> nondeterministic:** derivation is det_math hp quantized to ints, windowing/transport/dedup are pure
> integer, so it reconstructs identically cross-toolchain. **NO rail/golden/wire/kernel/det_math
> change** — composes the EXISTING protocol-4 session, rides v1.12r0 (Tier-2 net layer like layer 5).
> **Gates: 15/15 receipt (new `event`), 113 property tests (+7: determinism/full-recovery/any-single-
> drop/soundness-subsequence/kill-reliability/K-blackout-bound), ctest 10/10 GCC+Clang (new
> `event_reliable`), 13 generated headers in sync, 9/9 goldens byte-identical (C++ Sphere/Gunfire/Hit
> re-validated locally).** Ledger: **ADR-Step6-Events-v1.12r0**, receipt `…v1.12r0-<sha>.yml`;
> guardian.yml gains the event gen-check + ref self-test + parity-test legs (one per matrix cell).
> **Deferred (honest scope):** attacker attribution ("who fired") + per-round granularity would need a
> kernel-side event hook (a kernel touch); wiring events into the live `--fly` viewer (kill-feed/damage
> numbers) is renderer work. **NEXT (free pick, none blocking): cross-PROCESS sockets over the same
> frames; attacker-attribution via a kernel event hook (its own ADR); wire the session+events into the
> live `--fly` viewer; renderer meshes; or an optional new seal (ammo/convergence/component-damage, B5
> ISA atm).** **GIT: layer-6 code `c61f1f6` + receipt `09ea974` committed + PUSHED to `origin/main`;
> guardian CI run [28486934155](https://github.com/cjcgervais/seads/actions/runs/28486934155) GREEN —
> MSVC + GCC/Clang × x64/AArch64 reproduce all 9 goldens + the new `seads_event_test` parity leg (event
> digest bit-identical) + the event reference self-test + the cross-toolchain aggregation gate.**
> _(The layer-5 SESSION-loop banner below is retained as history — COMPLETE.)_
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.12r0** — **NETCODE LAYER 5: server↔client SESSION loop DONE ✅ (no-seal, rides v1.12r0)**
> **Latest (no-seal, `97f0331`): the WEAPON-001 wire transport is now USED end-to-end between two
> endpoints.** A new **netcode layer 5 — the server↔client SESSION loop** (`tools/session_ref.py` ↔
> `src/net/session.{h,cpp}`, new `seads_session` lib) drives the sealed kernel over the canonical
> **SESSION-SK-001** dogfight (the 3-ship gundemo shape: a **P-47D guns down an A6M2 while a Spitfire
> maneuvers**), emits full **protocol-4** frames at 20 Hz, ships them through an in-process **transport
> with fixed latency + deterministic packet loss**, and the **client reconstructs the whole fight from
> the decoded bytes** — composing the earlier layers: **OWN ship PREDICTED @ now** (layer 4b, reconciled
> against the DEQUANTIZED wire each frame — the realistic lossy-decode reseed path), **REMOTES
> INTERPOLATED ~150 ms in the past** (layer 4a), and **HP / KILLS / tracer ROUNDS from the freshest
> delivered frame's WEAPON section** (nearest-frame). The reconstructed per-tick **client view** is
> serialized (every field through the same GEO-001 quantize) and hashed → a whole-session **digest**
> (`25fcc41e…`) that the C++ mirror reproduces **bit-for-bit** (ctest `session_reconstruct`, GCC+Clang).
> **Key result: the gun KILL replicates over the lossy wire** — the client's final HP reads
> `[(P-47 150, alive), (A6M2 0, DEAD), (Spitfire 100, alive)]`, and `test_client_hp_mirrors_server_exactly`
> proves the client HP equals the server's authoritative HP to the quantum (hp is integer-valued ⇒ the
> 1e3 wire carries it losslessly). **Lossy ≠ nondeterministic:** every reconstruction op is det_math
> (predictor's kernel) / IEEE (interp) / integer (quantize+transport), so the fight reconstructs
> identically cross-toolchain — a STRONGER proof than layer 4b's canonical-state digest (this reconciles
> against the WIRE). **NO rail/golden/wire/kernel/det_math change** — composes the EXISTING protocol-4
> wire ⇒ **all 9 goldens byte-identical**, rides v1.12r0 (Tier-2 like layer 4a interp). **Gates:
> 14/14 receipt (new `session`), 106 property tests (+6: determinism/heal/lag-bounded/drop-tolerance/
> kill-replication), ctest 9/9 GCC+Clang (new `session_reconstruct`), 9/9 goldens byte-identical.**
> Adversarial Auditor **APPROVE** (rails/golden/wire untouched; byte-exact C++↔Python parity incl. the
> reconcile field-reorder; determinism; non-vacuous gate; cross-compiler digest reproduced). Ledger:
> **ADR-Step6-Session-v1.12r0**, receipt `…v1.12r0-97f0331.yml`, guardian.yml gains the session
> gen-check + ref self-test + parity-test legs (one per matrix cell). **Committed + PUSHED to
> `origin/main`** (layer `97f0331` + handoff/receipt `d3646c7`); **guardian CI run
> [28485828068](https://github.com/cjcgervais/seads/actions/runs/28485828068) GREEN** (non-kernel rider,
> all 9 goldens unchanged: MSVC + GCC/Clang × x64/AArch64 reproduce the goldens + the new
> `seads_session_test` leg + the cross-compiler digest bit-identity). **NEXT (free pick, none blocking): a genuinely cross-PROCESS transport (sockets) over the
> same frames; hp/round interpolation or explicit kill/impact EVENT messages; wiring this loop into the
> live viewer; aircraft meshes; or an optional new seal (ammo/convergence/component-damage, B5 ISA atm).**
> _(The v1.12r0 weapon-WIRE + attitude-pass summary below is retained as history — those are COMPLETE.)_
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.12r0** — **Step 7 guns / weapon WIRE transport (WEAPON-001) DONE ✅**
> **Latest (v1.12r0): the gunnery state now rides the 20 Hz snapshot wire.** Multiplayer/replay can finally
> REPLICATE the dogfight from the wire (HP bars, tracer rounds, kills) instead of reading it out-of-band from
> the local kernel. A third self-delimiting snapshot section **WEAPON-001** (snapshot **protocol 3 → 4**) carries
> per-aircraft **hp×1e3 + fire_cd×1e3**, then a projectile count and — per live round — a GeoPoint (bearing =
> heading) + **damage×1e3** + **ttl/owner** (integer counters carried **EXACTLY**, not quantized). New rail block
> `wire.weapon`. Built mirror-first like the KIN reseals: `snapshot_ref.py`↔`snapshot.{h,cpp}` extended, new
> `gen_weapon_vectors.py`→`weapon_vectors.h`→`seads_weapon_test` byte-exact gate (13th receipt gate
> `weapon_codec`), `test_weapon_wire.py` (+6 ⇒ **100** property tests). **TRANSPORT-ONLY: no kernel/det_math
> touched — hp/fire_cd/damage already live in the canonical state, so ALL 9 GOLDENS ARE BYTE-IDENTICAL** (no new
> golden; guardian.yml gains only the weapon test leg). **Wire reseal only**, exactly like v1.4r0 (KIN-001).
> **Downstream rider bundled (no-seal):** `seads_record` now sources the renderer's HP + tracer rounds + kills
> from the **DECODED** WEAPON-001 wire (the kernel side-channel `VizFrame`/`capture_viz` is deleted — net −code);
> `--gundemo`'s trajectory.js is now `protocol:4` and shows the A6M2 HP 70→0 + rounds tagged `owner:0` straight
> off the bytes. **13/13** receipt gates PASS, **ctest 8/8** GCC+Clang (new `weapon_byteexact`), 9/9 goldens
> byte-identical GCC+Clang. Ledger: ADR-Step7-Guns-WireTransport-v1.12r0, SEAL_CARD v1.12r0, receipt
> `…v1.12r0-c6dd73e.yml`, rails version 210→220 (+ `wire.weapon`). **NEXT: a real cross-process server/transport
> loop, hp/round interpolation or kill/impact event messages, or B5 ISA atmosphere — all optional.**
> **GIT: attitude-pass code at `7160fd3` + this handoff/receipt commit on `origin/main` (2026-06-30); pushed. guardian CI expected GREEN** (non-kernel rider; goldens unchanged). The v1.12r0 seal landed at
> `e23df32` (CI [28471743367](https://github.com/cjcgervais/seads/actions/runs/28471743367)) — Python gates +
> MSVC x64 + GCC/Clang × x64 + **GCC/Clang arm64** reproduce all 9 goldens bit-for-bit + the new
> `seads_weapon_test` leg + the cross-toolchain hash aggregation gate; Adversarial Auditor APPROVE
> (Python↔C++ byte parity, rail scope = only `wire.weapon` + version/seal, no banned symbols, no hash move).
> **Then a no-seal renderer rider landed at `a04fe97`** (CI [28474978474](https://github.com/cjcgervais/seads/actions/runs/28474978474)
> green): the **native raylib `seads_viewer` now draws the guns from the decoded WEAPON-001 wire** — tracer
> rounds + HP bars + kills — via new `Playback::sample_weapons()`, CI-gated by a `seads_client_test`
> weapon-playback case + the headless `seads_viewer <rec> --selfcheck N` (prints `hp=… KILLED` + `rounds=N`).
> **Then a 2nd no-seal renderer rider — the AIRCRAFT ATTITUDE PASS — landed at `7160fd3`:** both viewers now
> draw aircraft with real attitude from the KIN wire. The **replay** path (`run_gui`) swaps the old
> sphere+radial-stick for the attitude-aware `draw_aircraft` (wings roll with bank **phi**, nose tilts with the
> flight-path angle **gamma**), and **fly-mode remotes** now tilt with their true **gamma** instead of a hardcoded
> `pitch=0` (a climbing/diving bandit reads right on the globe). phi/tas/gamma are NON-geographic, so the layer-4a
> interp (geography-only, behind the parity gate) drops them — `Playback::sample` now **snaps phi/tas/gamma from
> the nearest received frame** via a shared `nearest_frame()` seam, exactly the pattern `sample_weapons` already
> uses for hp/rounds (position+heading still interpolate; attitude reads at 20 Hz, imperceptible for a roll/pitch).
> This also fixed a **latent display bug**: the replay HUD's `bank`/`tas` columns were silently always 0 (interp
> dropped them). **No kernel/wire/det_math** touched ⇒ all 9 goldens byte-identical; a `seads_client_test`
> attitude-snap assertion was added (green GCC+Clang). **The rendered _look_ is unverified beyond geometry/data —
> a GPU run of `seads_viewer <rec>` / `--fly` is the only thing that confirms it visually.**
> **CURRENT GATE STATE: 13/13 receipt gates PASS, 100 property tests, ctest 8/8 GCC+Clang, all 9 goldens
> byte-identical.** **v1.12r0 + the native-viewer-guns rider + the attitude-pass rider are all fully landed.**
> _(The G1→G3 guns-arc summary below is retained as history — those phases are COMPLETE and unchanged.)_
>
> ## ►► PRIOR STATE: seal **ATM-Sphere v1.11r0** — **Step 7 guns / G3 (per-airframe weapon roster + fire-rate) DONE ✅ — GUNS ARC G1→G3 COMPLETE**
> The whole weapons system is now in the kernel: **G1 (v1.9r0)** ballistic projectiles → **G2 (v1.10r0)**
> hit detection + hitpoints (gun kills) → **G3 (v1.11r0)** per-airframe roster + fire-rate. **G3 moves the
> G1/G2 global gun constants into each envelope** so the 8 airframes fight with WWII character. New envelope
> scalars: **hp_start** (toughness), **muzzle_v_mps**, **damage_per_round** (CARRIED by the round so lethality
> is fixed at fire time), **rof_interval_ticks** (fire-rate). Firing is gated by a per-aircraft **fire_cd**
> cooldown (decrement-then-fire ⇒ shots exactly rof_interval ticks apart). So .50-cal platforms (P-47D hp150/
> rof3, P-51 hp110/rof4) = durable + high volume; 20-mm cannons hit harder per round (A6M2 dmg40 but hp70/
> rof9 = glass cannon; La-7 dmg30; Bf109 dmg25). Drag/ttl + the no-arg START_HP stay global. **STILL ZERO new
> det_math** across G1→G3 (det_sin/det_cos + ÷×−), **no wire change** (KIN-002/protocol 3; weapon wire transport
> DEFERRED). **Snapshot now:** aircraft **9×f64** (`…,gamma,hp,fire_cd`); projectile **7×f64** (`…,gamma,damage`)
> + u32 ttl + u32 owner. Appending fire_cd + damage (and the scenario airframes' per-airframe HP/weapon values)
> moves **all 9 goldens** (Sphere fire_cd=0, traj+hp identical; Gunfire/Hit also change *behaviorally* — fire-rate
> gating cuts the round count, a6m2's 70 HP changes the kill). **No new golden** (Gunfire/Hit now exercise the
> roster) ⇒ **guardian.yml unchanged**. **12/12** receipt gates PASS, **9/9 goldens C++≡Python bit-for-bit GCC+Clang
> (18/18)**, **ctest 7/7** both, **94** property tests (+6: `test_weapon.py` roster sanity/character + fire-rate &
> per-airframe lethality in `test_hit.py`; projectile/hit updated for per-env muzzle/damage). **NEXT: renderer polish
> (draw rounds + kills + HP; meshes; offline web — all NO SEAL).** Future seals (optional): weapon wire transport
> (a netcode layer for MP), ammo/convergence/component-damage, **B5** ISA atmosphere.
> Goldens (v1.11r0 — all moved; fire_cd appended to every aircraft + damage to every round; scenario airframes carry per-airframe HP/armament):
> - **Sphere `f28ac561…a0d4a475` (moved: fire_cd appended; traj+hp identical)** · Turn `0029f24c…563c55` · Climb `ba2cae43…5a52a27a`
> - TurnClimb `2ea8cc26…fe733e03` · Accel `d7601bec…5c55be75` · Pitch `c7d2dd53…2d57adbd` · Stall `9e53be3f…6b3b7e8c`
> - Gunfire `bdfa8f95…3b7b8c9e` (G3-armed, fire-rate gated) · Hit `1a460976…4ec901ee` (p47d .50cal downs a 70-HP a6m2)
> Ledger: ADR-Step7-Guns-G3-Roster-v1.11r0, SEAL_CARD v1.11r0, receipt `…v1.11r0-*.yml`, rails version 200→210
> (+ a `weapons.roster` note), guardian.yml **unchanged** (same 9 golden IDs). 8 envelope JSONs gained the weapon block.
> Files: `envelopes.py` (AERO_FIELDS +4), 8 `data/tuning/envelopes/*.json` (weapon block), `flight_types.h`
> (Envelope +4), `kernel.{h,cpp}` (fire_cd_ + p_damage_ SoA; per-env spawn; carried-damage hit; fire-rate cooldown;
> snapshot), `ref_kernel.py` (Aircraft.fire_cd + Projectile.damage + mirror), `scenario_main.cpp` (per-airframe hp),
> `tests/property/{test_weapon.py(new),test_hit.py,test_projectile.py}`.
> **GIT: committed + pushed to `origin/main` at `5741203` (2026-06-30). guardian CI GREEN**
> (run [28467504772](https://github.com/cjcgervais/seads/actions/runs/28467504772)) — Python gates +
> MSVC x64 + GCC/Clang × x64 + **GCC/Clang arm64** reproduce all 9 goldens bit-for-bit + the
> cross-toolchain hash aggregation gate. **v1.11r0 (Step 7 guns G3) is fully landed — GUNS ARC G1→G3 COMPLETE.**
> Adversarial Auditor clean (AERO_FIELDS↔struct order, fire-rate op-order parity, Sphere fire_cd-strip proof).
> v1.10r0 (G2) landed at `c900241`+`674c62f` (green 28464526804); v1.9r0 (G1) at `dbb3de7`+`e4aeb25` (green 28462840475).
> Branch protection / required-check setup is still the deferred owner task.
>
> **Remaining roadmap (guns arc DONE):** the kernel now models a full deterministic dogfight-gunnery loop
> (ballistics + hit/damage + per-airframe roster/fire-rate). Optional future seals: weapon wire transport (netcode
> MP), ammo/convergence/component-damage, **B5** (ISA atmosphere). Flight-model arc **B1→B4 COMPLETE**; guns **G1→G3 COMPLETE**.
>
> **Renderer DRAWS the guns — now FROM THE WIRE (no-seal, updated v1.12r0).** `seads_record --gundemo` (a P-47D
> guns down an A6M2 while a Spitfire maneuvers) now sources projectile + HP state from the **DECODED WEAPON-001
> wire** (per the v1.12r0 reseal above) — the kernel side-channel is gone — into per-frame `hp[]` + `p[]` arrays.
> The web viewer (`src/client/web/viewer.js`) draws **tracer rounds** (a yellow point cloud), **HP bars**, **kills**
> (dead aircraft grey out + freeze, HUD shows ☠ KILLED), and **auto-frames** the action on load. Verified in Chrome
> (screenshot: P-47 tracer stream + greyed dead A6M2 + KILLED HUD). To view: `seads_record --gundemo --js
> src/client/web/trajectory.js --snap-every 2`, then serve `src/client/web/` (`python -m http.server`) and open
> index.html. Downstream-only, rides v1.12r0.
>
> **Native raylib viewer now DRAWS the guns too (no-seal, 2026-06-30).** `seads_viewer <rec>.seadsrec`
> (recording-replay path) now renders the gunnery straight from the **decoded WEAPON-001 wire**:
> **tracer rounds** (a yellow point cloud), **per-aircraft HP bars** projected above each marker (green→
> orange→red, plus a text bar + hp in the HUD), and **kills** (dead aircraft grey out, HP bar greys, a
> `KILLED` label + `*** KILLED ***` HUD row). Sourced from new `Playback::sample_weapons()` (the nearest
> decoded frame — hp is discrete, rounds transient, so NO interpolation), CI-gated by a new
> `seads_client_test` weapon-playback case (ctest `client_presentation`, GCC+Clang) + the headless
> `seads_viewer <rec> --selfcheck N` (prints per-sample `hp=… KILLED` + `rounds=N`, no GPU). To view:
> `seads_record --gundemo --out gun.seadsrec --snap-every 2` then `seads_viewer gun.seadsrec` (GUI) or
> `… --selfcheck 8` (headless). **Still-pending renderer polish:** aircraft meshes (vs marker spheres);
> guns in the live `--fly` path (own ship would need `Command.fire`); vendor Three.js for fully-offline web.
>
> _(Prior: v1.6r0 B2 lift & pitch — γ stored state, KIN-002 wire reseal, all goldens regenerated + Pitch;
> committed + pushed, guardian green on `12a1830` run 28392491160.)_
>
> ---
>
> Resume doc for a fresh session. (Pre-B2 history below.) State was seal **ATM-Sphere v1.4r0**, git `main` at the
> **Step 5 renderer commit `e4ef652`** (Steps 1–6 done + a working downstream renderer) — pushed to
> `origin/main`, guardian CI **GREEN** (run [28355716358](https://github.com/cjcgervais/seads/actions/runs/28355716358):
> MSVC + GCC/Clang × x64/AArch64 reproduce all 4 goldens + the geo001/snapshot/lockstep/interp/predict
> parity vectors bit-for-bit **and now the client-presentation test on every leg**). **The Track A
> live-input loop is now DONE** (the native viewer's `--fly` mode flies the own ship through
> `seads_predict`); **next: Step 5 polish (meshes / chase cam / offline web) or track B (energy/drag
> model) — see START HERE + §5/§6.** Read `CLAUDE.md` first (the constitution — governance is now
> lean, §2). Background facts also live in Claude memory (`seads-canon`, `seads-harness`).
>
> ## ► START HERE (next task)
> **⚠ COLD-START READ THE BANNER AT THE TOP FIRST — it is the authoritative current state (seal
> ATM-Sphere v1.12r0; flight model B1→B4 + guns G1→G3 COMPLETE; weapon WIRE transport WEAPON-001
> COMPLETE; BOTH the web AND native viewers now draw rounds/kills/HP from the wire). Everything in this
> `START HERE` block and §1–§8 below is HISTORY** (how each layer shipped), kept for context. **The actual
> next task now that flight + guns + the weapon wire are all done is a free pick (none blocking):**
> - (a) **Networked server↔client loop — ✅ DONE (`97f0331`, no-seal; see the TOP banner).** Netcode
>   **layer 5** (`session_ref.py` ↔ `src/net/session.{h,cpp}`, `seads_session`) is the in-process
>   server→transport→client loop that finally USES the WEAPON-001 wire between two endpoints and
>   reconstructs the full dogfight clientside (own predicted + remotes interpolated + HP/kills/rounds
>   from the wire). Built to the standard net-layer pattern (mirror + `gen_session_vectors` parity test +
>   CI leg). **Explicit kill/impact EVENT messages — ✅ DONE (netcode LAYER 6, no-seal; see the TOP
>   banner).** `event_ref.py` ↔ `src/net/event.{h,cpp}` (`seads_event`) ships a reliable, redundant
>   event journal (HIT/KILL, K=4 window) over the same lossy transport — the client reconstructs the
>   exact hit/kill sequence bit-for-bit, with a gated K-consecutive-drop failure bound. **Remaining
>   stretch on THIS axis:** a genuinely cross-PROCESS transport (real sockets, not in-process) over the
>   same frames; **attacker attribution** ("who fired the killing round") + per-round granularity, which
>   need a kernel-side event hook (a kernel touch → its own ADR); hp/round interpolation (vs
>   nearest-frame); wiring the session loop + event channel into the live `--fly` viewer (kill-feed /
>   damage numbers off a real transport instead of a recording).
> - (b) **More renderer polish (no-seal):** the **aircraft attitude pass is now DONE** (`7160fd3` — replay +
>   remotes bank/pitch from the wire). Remaining: aircraft **meshes** (vs the marker stick-figure, web + native);
>   **guns in the live `--fly` path** (the own ship would need `Command.fire` wired into the keyboard input +
>   predictor — combat visuals currently exist only in replay `run_gui`); **tracer streaks + muzzle flash** (vs
>   the flat point cloud); vendor **Three.js** for a fully-offline web viewer (currently CDN) + read the web
>   ceiling shell from the `ATM_TOP` rail (hardcoded `+8000` in `viewer.js`).
> - (c) **An optional new seal:** ammo counts / gun convergence / component (region) damage; or **B5** ISA
>   atmosphere (§8.5 — the doc explicitly recommends deferring B5; biggest lift, forces det_exp/det_pow).
>
> Read `CLAUDE.md` first (the constitution; governance is lean, §2), then run the **"Verify everything still
> works"** sweep below to confirm the green baseline (**15** receipt gates, **113** property tests, ctest
> **10/10**, 9 goldens) BEFORE and AFTER any change. Memory: `seads-canon`, `seads-harness`,
> `seads-flight-model-roadmap`, `seads-guns-roadmap`, `seads-netcode-session`. **Git: netcode-layer-5
> SESSION loop committed + PUSHED to `origin/main` (layer `97f0331` + handoff/receipt `d3646c7`);
> guardian CI run 28485828068 GREEN (non-kernel rider; all 9 goldens unchanged).**
>
> _(Everything below is the original v1.4r0-era handoff, retained as history.)_
> **Steps 1–6 are DONE and Step 5 (renderer) now has a working first cut.** The deterministic
> core, the full netcode stack (layers 1–4b), AND a downstream renderer all exist. The renderer
> ships a kernel-driven **trajectory recorder** (`seads_record` → `.seadsrec` GEO-001/KIN-001 wire
> stream + `trajectory.js`), a pure unit-tested **client lib** (`globe`/`playback`, consuming the
> 4a interpolation), a **verified web globe viewer** (`src/client/web/`, screenshot-proven), and
> an optional **raylib viewer** (`-DSEADS_CLIENT=ON`, off in CI). All read-only, outside the
> `world_hash`, **no seal** (rides v1.4r0). See **§5**.
>
> **Track A (a live local-input loop) is DONE ✅ (2026-06-29, no seal).** The native viewer's new
> **`--fly`** mode (`src/client/viewer_main.cpp`, `-DSEADS_CLIENT=ON`) flies the OWN ship from live
> keyboard input (A/D → `target_phi`, W/S → `target_climb`) through the already-built
> **`seads_predict`** (`predict::Predictor`, layer 4b) at a fixed 100 Hz from wall-clock, while
> remotes stay on the `Playback`/interp path (layer 4a) — prediction (own) + interpolation (remote)
> on the same globe at once, the full 4a+4b loop finally visible. Headless proof:
> `seads_viewer --fly --selfcheck 6` (no GPU/recording needed) drives the real sealed kernel and
> prints the own state. Downstream-only: input feeds `Command`s into the kernel-driving Predictor,
> never the wire; no rail/golden/`world_hash` touched → no seal; ctest 7/7, golden
> `529c6a05…9218fe16` unchanged, receipt `…v1.4r0-89a1974.yml`. CMake now links `seads_predict` +
> kernel/replay/det_math into `seads_viewer` (guarded by `SEADS_CLIENT`, so CI is untouched).
> *(Note: this single-process viewer has no authoritative server, so only `Predictor::predict()`
> runs in `--fly`; `reconcile()` (snap+replay) stays exercised by the layer-4b parity tests and
> engages against a real server.)*
>
> **Chase cam + flight-control scheme is now DONE ✅ (2026-06-29, no seal)** — see §5. The `--fly`
> viewer now rides a **chase camera** behind/above the own ship (follows heading, wheel zooms),
> with **mouse-aim** (a central reticle drives bank/climb) as the default and **hold-SPACE
> free-look** (mouse pans freely around the plane; A/D bank, W/S pitch, Q/E yaw, Shift/Ctrl
> throttle by keyboard; release restabilizes). P pauses, R resets. Yaw + throttle are applied
> DOWNSTREAM by re-seeding the predictor (the kernel has no such axis). All presentation-only;
> golden `529c6a05…` unchanged, 12/12 gates + ctest 7/7 green.
>
> ### ⇒ Flight model Track B: **B1 DONE ✅ (v1.5r0) · B2 DONE ✅ (v1.6r0) · B3 DONE ✅ (2026-06-29, seal v1.7r0)**. NEXT: **§8.4 Phase B4 (per-airframe aero retune — mostly data)**.
> **B1 (longitudinal energy) is shipped and sealed.** TAS is now a real integrated state: thrust −
> drag (parasitic + induced from the bank load factor) − climb cost. `Command` carries **throttle
> [0,1]**; per-airframe aero params (mass, S, cd0, k, T₀, V_max) live on the envelope. Energy lives
> in `step(cmd,env)` only, so **GOLDEN-SK-Sphere-001 is unchanged** (kinematic anchor) while
> Turn/Climb/TurnClimb regenerated + new **GOLDEN-SK-Accel-001**; C++≡Python bit-for-bit under
> GCC+Clang, 12/12 gates + ctest 7/7 + 57 property tests green, receipt `…v1.5r0-*.yml`. The viewer's
> Shift/Ctrl is now a **real throttle** (re-seed hack retired). Details in **§8.1**.
>
> **What's still a band-aid (fixed by B3):** **pitch is now REAL** (B2 — γ is a kernel state, the
> viewer marker tilts by the true γ, W/S = real g-command, the pitch-cue exaggeration is gone). What
> remains: **no stall / unbounded C_L** — `n` is only globally clamped to `[-3,9]` (placeholder);
> per-airframe `C_Lmax`, accelerated stall, structural-g and the corner speed are **B3**. The
> **vertical singularity** (`ψ̇` has `cosγ` in the denominator → undefined at γ=±90°) is documented,
> not handled — scenarios/viewer stay well inside ±90°; full loops/verticals need a different
> formulation (post-B3). **Yaw (Q/E) in the viewer remains a downstream re-seed** by design (the
> coordinated-flight kernel has no independent yaw axis). Every phase stays a seal with the full
> ritual (det_math+MPFR if needed, Auditor, ADR, `/seal`). **Start at §8.3.**
>
> **Smaller no-seal alternatives if you're not ready for core work:** Track A stretch — aircraft
> **meshes** (vs the marker sphere) and **vendoring Three.js** for offline web (§5). **Step 7**
> (guns/projectiles) is a *new* seal, best deferred until the flight model lands. Whatever you pick,
> run the §4 gates first to confirm the green baseline, then again before committing.

## Where things stand (DONE)

Deterministic core + governance harness up; bit-for-bit promise **proven in CI**; aircraft now
maneuver within their tuning envelopes. Roadmap steps **1, 2, 3, 4 are DONE**; **Step 5 (renderer)
has a working first cut** (recorder + pure client lib + web globe viewer + optional raylib viewer —
all downstream-only, ctest 7/7, no seal; §5); **Step 6 (netcode) is COMPLETE through layer 4b** — layer 1 (GEO-001 wire codec), layer 2 (20 Hz snapshot
serialization), layer 3 (loopback lockstep desync tripwire), layer 4a (remote interpolation
buffer), and **layer 4b (client-side prediction)** are all **DONE** (details in §6 below). Layer
4b carried a **Tier-1 reseal** (v1.3r0 → **v1.4r0**) to put `phi`/`tas` on the wire via the new
auxiliary **KIN-001** block. The multiplayer-flight MVP loop is complete. **Step 5 (renderer) now
has a working first cut** — the downstream consumer of the 4a interpolated remote states (the 4b
predicted own state is the next polish step). See §5.

- **Remote:** `origin` = `https://github.com/cjcgervais/seads` (public). Single branch `main` (feature
  branches merged + deleted). `guardian.yml` is **green on `main` at `e4ef652`**
  (run [28355716358](https://github.com/cjcgervais/seads/actions/runs/28355716358)) — MSVC + GCC +
  Clang × x64 + AArch64 reproduce **all 4 sealed goldens** bit-for-bit AND the
  `seads_{geo001,snapshot,lockstep,interp,predict,client}_test` parity/presentation tests, with a
  per-golden cross-toolchain aggregation
  gate. (No `gh` CLI here; watch CI via the public Actions API — `curl -s
  ".../actions/runs/<id>/jobs"` — and use a GCM token from `git credential fill` for log downloads.)
- **Seal:** ATM-Sphere **v1.4r0** (v1.3r0 + KIN-001 wire reseal for prediction). Four goldens,
  all cross-toolchain-verified and **unchanged** by the reseal:
  - GOLDEN-SK-Sphere-001 (straight) `529c6a05…9218fe16` — unchanged since Pass 1
  - GOLDEN-SK-Turn-001 `6160540c…13f152ee` · Climb-001 `74b9d556…2d9b6682` · TurnClimb-001 `f7193b99…7cedd413`
- **Roster:** all 8 tuning envelopes exist (`data/tuning/envelopes/`); the kernel consumes them for
  bank/climb limits via `Kernel::step(cmd,env)`.
- **Gates:** all Python gates green; **52** Hypothesis property tests pass (incl. 7 geo001 + 7
  snapshot + 4 lockstep + 8 interp + 6 predict); det_math ≤2 ULP vs MPFR; C++ det_math + geo001 +
  snapshot byte-exact + lockstep/predict digest-exact + interp bit-exact vs reference; **10**
  generated headers in sync (`gen_*.py --check`). ctest is **6/6** under GCC + Clang (added
  `predict_equal`).
- **Netcode (Step 6):** `src/net/` holds the GEO-001 wire codec (`geo001.{h,cpp}`), the 20 Hz
  snapshot framing (`snapshot.{h,cpp}`, **now protocol 2** with the KIN section), and the remote
  interpolation buffer (`interp.{h,cpp}`) — all in the `seads_net` lib (pure transport: no
  det_math/kernel) — plus two **kernel-driving** libs: the loopback lockstep harness
  (`lockstep.{h,cpp}` → `seads_lockstep`) and the client-side prediction harness
  (`predict.{h,cpp}` → `seads_predict`), both linking `seads_kernel`+`seads_replay`. Each mirrors
  a Python reference (`geo001_ref`, `snapshot_ref`, `lockstep_ref`, `interp_ref`, `predict_ref`)
  with a generated-vector parity gate (`seads_{geo001,snapshot,lockstep,interp,predict}_test`).
- **Ledger:** receipts in `docs/receipts/` (latest `…v1.4r0-11e07d4.yml`), all `overall: PASS`.
- **Deferred (owner's call, not blocking):** (a) make `Cross-toolchain hash aggregation` a **required
  status check** on `main` (branch protection — needs a PAT or `gh`); (b) `hash_sign_json.py` signing
  of the 8 envelopes.

## Environment notes (important for a cold start)

- This machine has **Python 3.11 + git**; Python deps installed: `gmpy2 hypothesis pytest`.
- C++ toolchain (installed locally) is **NOT on PATH by default**. Prepend it:
  ```
  $bin="$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
  $ninja="$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
  $env:PATH="$bin;$ninja;$env:PATH"
  ```
  (g++, clang++, cmake, ninja all live in `$bin`/`$ninja`.)
- Git on D:\ needed `git config --global --add safe.directory D:/SEADS_2026` (already done).
  **Also set `git config --global --add safe.directory '*'`** — the D: filesystem doesn't record
  ownership, so git flags every nested repo as "dubious ownership". Without the wildcard, the
  renderer's `-DSEADS_CLIENT=ON` raylib **FetchContent** clone fails at the tag checkout (the clone
  lands but `git checkout 5.5` is refused). Already applied.
- PowerShell gotcha: do **not** redirect native-exe streams with `*>`/`2>&1` (wraps stderr as
  errors and aborts). Pipe stdout normally; use `| Out-Null` to silence. (For build logs, the Bash
  tool with `> log 2>&1` works fine — that's how the raylib build was captured.)

## Verify everything still works (2 min)

```bash
# Python side (no compiler needed)
python tools/lint_determinism.py
python tools/det_math_oracle.py --samples 8000
python tools/spec_monotone_check.py config/rails/atm.json
python tools/tuning_probe.py data/tuning/envelopes/*.json
python tools/atm_top_probe.py --ceil 8000 --soft 100
python tools/geo001_ref.py                  # GEO-001 codec reference self-test
python tools/snapshot_ref.py                # GEO-001 snapshot reference self-test
python tools/lockstep_ref.py                # loopback lockstep reference self-test (+ negative control)
python tools/predict_ref.py                 # client-side prediction reference self-test
python tools/session_ref.py                 # server<->client session loop reference self-test (layer 5)
python tools/event_ref.py                   # reliable combat-EVENT channel reference self-test (layer 6)
for g in gen_coeffs gen_golden_params gen_detmath_vectors gen_envelope_tables gen_scenario_params \
         gen_geo001_vectors gen_snapshot_vectors gen_weapon_vectors gen_lockstep_vectors \
         gen_interp_vectors gen_predict_vectors gen_session_vectors gen_event_vectors; do \
  python tools/$g.py --check; done          # all 13 generated headers in sync (event added layer 6)
python -m pytest tests/property -q          # 113 pass (scenario/energy/pitch/stall + projectile/hit/weapon/weapon_wire + net layers incl. session + event)
python tools/make_receipt.py                # runs all 15 gates -> overall: PASS (writes docs/receipts/...yml)
```
```powershell
# C++ side (PATH set as above). Builds seads_golden (Sphere) + seads_scenario (8 scenario goldens).
cmake -S . -B build-gcc   -G Ninja -DCMAKE_CXX_COMPILER=g++     -DCMAKE_BUILD_TYPE=Release; cmake --build build-gcc
cmake -S . -B build-clang -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release; cmake --build build-clang
.\build-gcc\seads_golden.exe   --out run_gcc.bin
python tools\validate_snapshot.py --golden tests\golden\GOLDEN-SK-Sphere-001\expected.world_hash --candidate run_gcc.bin
foreach ($id in "GOLDEN-SK-Turn-001","GOLDEN-SK-Climb-001","GOLDEN-SK-TurnClimb-001","GOLDEN-SK-Accel-001","GOLDEN-SK-Pitch-001","GOLDEN-SK-Stall-001","GOLDEN-SK-Gunfire-001","GOLDEN-SK-Hit-001") {
  .\build-gcc\seads_scenario.exe --id $id --out run_scen.bin
  python tools\validate_snapshot.py --golden "tests\golden\$id\expected.world_hash" --candidate run_scen.bin }
# (repeat the two scenario/validate blocks with build-clang to confirm cross-compiler parity)
# SEE THE SIM (web): .\build-gcc\seads_record.exe --gundemo --js src\client\web\trajectory.js --snap-every 2
#              then: cd src\client\web; python -m http.server 8753  -> open http://localhost:8753/index.html
# SEE THE SIM (native raylib viewer — draws guns from the WEAPON-001 wire; needs -DSEADS_CLIENT=ON, see below):
#   .\build-gcc\seads_record.exe --gundemo --out gun.seadsrec --snap-every 2
#   .\build-client\seads_viewer.exe gun.seadsrec              # GUI: tracers + HP bars + KILLED
#   .\build-client\seads_viewer.exe gun.seadsrec --selfcheck 8   # headless: prints hp/KILLED + rounds=N (no GPU)
# Net codec + client parity tests (also built by the same cmake): fastest full check is ctest.
ctest --test-dir build-gcc --output-on-failure    # 10/10: detmath, geo001, snapshot, weapon, lockstep, interp, predict, session, event, client
ctest --test-dir build-clang --output-on-failure   # 10/10 under Clang too

# OPTIONAL — the renderer (Step 5). Record a flight, then view it:
.\build-gcc\seads_record.exe --demo --out flight.seadsrec --js src\client\web\trajectory.js
#   web viewer:    open src\client\web\index.html  (trajectory.js sits beside it; file:// works)
#   native viewer: cmake -S . -B build-client -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DSEADS_CLIENT=ON
#                  cmake --build build-client --target seads_viewer
#                  .\build-client\seads_viewer.exe flight.seadsrec            # GUI
#                  .\build-client\seads_viewer.exe flight.seadsrec --selfcheck 6   # headless, no GPU
```

## Next steps — pick up here (in priority order)

> Steps **1/2/3/4 DONE**, **Step 6 (netcode) DONE through layer 4b**, **Step 5 (renderer) first cut
> DONE** (§5). The entries below (§1–§6) are the **history** of what shipped — read them for context
> on how each layer was built and gated. **The actual next task is in `► START HERE` at the top:**
> recommended Track A (a live local-input loop feeding `seads_predict` so the own ship is *flown*,
> not replayed — closes the visual MVP, no seal). Alternatives: Track B (energy/drag model — a real
> reseal that moves all 4 goldens) or Step 7 (guns). Anything touching a rail or a golden hash →
> follow the seal ritual (`/seal` or `/reseal`; Governance reminder at the bottom). The renderer
> tracks don't touch a rail or a golden.

### 1. Finish the cross-arch determinism proof (MSVC + AArch64)  ✅ DONE (2026-06-28)
**Cross-toolchain bit-identity is now PROVEN in CI.** Remote `origin` =
`https://github.com/cjcgervais/seads` (public). `guardian.yml` green on `main` —
all six legs reproduce the seal `529c6a05…9218fe16`:
MSVC x64 · GCC x64 · Clang x64 · **GCC arm64** · **Clang arm64** + the aggregation gate.
Green run: https://github.com/cjcgervais/seads/actions/runs/28340968855
- The AArch64 legs (highest divergence risk: FMA/libm) reproduce the seal exactly — no
  divergence ever occurred. All failures en route were harness/CI bugs, not the kernel:
  (a) `-lwinpthread` was hardcoded for all compilers → scoped to `if(MINGW)`, portable
  threads on Linux/macOS (`CMakeLists.txt`);
  (b) MSVC generator `VS 17 2022` vs the new `windows-2025-vs2026` runner → pinned the MSVC
  leg to `windows-2022` (`guardian.yml`);
  (c) aggregation hash compare used `tr -d ' \n'` which left a stray `\r` from the CRLF
  `expected.world_hash` → now `tr -d '[:space:]'` (`guardian.yml`).
  All three ride seal v1.2r0 (no rail/golden/kernel change).
- **STILL TODO (deferred by owner):** make **`Cross-toolchain hash aggregation`** a required
  status check (Settings → Branches → branch protection on `main`). If AArch64 ever diverges
  later: binary-search to the first divergent tick (add per-tick hashing in `src/replay`);
  SoftFloat is the fallback.

### 2. Broaden C++ det_math coverage  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed)
`tools/gen_detmath_vectors.py` rewritten: per-function **structured boundary rows** (Cody-Waite
quadrant edges k·π/2, fdlibm atan regions 7/16·11/16·19/16·39/16, asin ±1 clamp, atan2 axis cases,
wrap edges ±k·2π, tiny/large magnitudes) **+ 512 seeded random rows/group** (integer SplitMix64 +
`+−*/` only → header is byte-reproducible cross-platform, so CI `--check` stays green). Domains
mirror `det_math_oracle.py`. **64 → 4878 vectors**, tolerance exact (bit-equal). Added previously-
**uncovered** `det_atan2`, `wrap_pi`, `wrap_2pi` to the parity gate (all live kernel calls,
`kernel.cpp:29,66`). `detmath_vectors.h` `Vec` gained a `double y` second-operand column;
`detmath_test_main.cpp` dispatcher extended. **PASS** under GCC + Clang locally; golden hashes
unchanged (no kernel/det_math/reference edit). Ledger: ADR-Step2-DetMathParity-v1.3r0, Forge card
Step2, receipt `…v1.3r0-d5e5e44.yml`. CI (guardian.yml L25 `--check`, L102 runs `seads_detmath_test`
per leg) exercises all 4878 vectors cross-toolchain/cross-arch on push.
- To re-tune breadth: change `N_RANDOM` (or a function's sub-ranges) in `gen_detmath_vectors.py`,
  regenerate, rebuild. Mirrors determinism rules (det_math-only, no FMA, hex-float constants).

### 3. Complete the 8-aircraft tuning envelopes (data-only)  ✅ DONE (2026-06-28)
All 8 roster envelopes now exist under `data/tuning/envelopes/` (p47d, bf109f4, a6m2, yak3, la7,
spitfire_mk5, p51 + existing ki61). Data-only — golden unchanged (`529c6a05…9218fe16`), seal v1.2r0.
Ledger: `docs/adr/ADR-roster-envelopes-v1.2r0.md`, `docs/cards/FORGE_AUDIT_CARD-roster-envelopes-v1.2r0.md`,
receipt `docs/receipts/receipt-ATM-Sphere_v1.2r0-0a7a258.yml` (overall PASS). CI green (run 28341209641);
`tuning_probe.py` validates all 8; `make_receipt.py` tuning gate now globs all envelopes (was ki61-only).
- Values are an initial balance pass (relative airframe strengths; retuning later stays data-only).
- Optional follow-up: `hash_sign_json.py` to sign each envelope (not yet applied).

### 4. Wire the kernel to use envelopes + climb/bank inputs  ✅ DONE (2026-06-28, seal v1.3r0)
`Kernel::step(cmd,env)` now banks toward a commanded angle at the envelope roll_rate (clamped to
phi_max(TAS)) and climbs at a commanded rate (clamped to the envelope band + ceiling predamp).
Straight golden preserved byte-for-byte via shared `advance_(i,req)`; Sphere hash unchanged. Three
new sealed goldens with scripted-timeline inputs: GOLDEN-SK-{Turn,Climb,TurnClimb}-001
(`config/scenarios/*.json` → `tests/golden/<id>/`). Runner `seads_scenario --id <id>`. Generators
`gen_envelope_tables.py`/`gen_scenario_params.py` → `src/kernel/{envelope_tables,scenario_params}.h`
(+ `flight_types.h`); `lut_eval` shared bit-identical in detmath_ref.py + kernel.cpp (no new det_math).
Guardian green for all 4 goldens × MSVC/GCC/Clang × x64/AArch64 (run 28342235633). Ledger:
ADR-Step4-Scenarios-v1.3r0, Forge card, SEAL_CARD v1.3r0, receipt -8b85a32.
- TAS held constant (no energy/drag model) — a documented step-4 approximation to revisit in a later seal.
- New goldens use scripted step-function schedules; richer maneuver scripting can extend the schema.

### 5. Custom renderer (post-core, decision 1A)  ✅ FIRST CUT DONE (2026-06-28, seal v1.4r0 — no seal)
Downstream-only renderer subsystem under **`src/client/`** (full map: `src/client/README.md`).
Built this session, all read-only / outside the `world_hash` (no rail/golden/kernel touched), gates
green, **lean ledger** (commit + receipt `…v1.4r0-*.yml`, no ADR per owner's call):
- **Recorder** `seads_record` (`record_main.cpp`) — drives the **real sealed kernel** and captures
  the flight as the exact 20 Hz **GEO-001/KIN-001** wire stream (protocol 2), written as a
  `.seadsrec` container (`seadsrec.{h,cpp}`) for the native viewer **and** a `trajectory.js` for the
  web viewer, both from the *same decoded* frames. Built-in 3-ship `--demo` (KI-61/Spitfire/Bf109,
  banking+climbing — bank auto-clamped to envelope `phi_max`) plus sealed-scenario replay (`--id`).
  Proves the layer 1/2/4a path end to end.
- **Pure client lib** `seads_client` — `globe.h` (sphere→cartesian, orbit camera, perspective
  project, hemisphere cull; libm OK, `lint_determinism` excludes `src/client`) + `playback.{h,cpp}`
  (decoded recording → `interp::SnapshotBuffer`, sampled ~100 ms in the past = the 4a delay).
- **Headless gate** `seads_client_test` — **ctest 7/7** under GCC+Clang (was 6/6; added
  `client_presentation`): globe invariants + `.seadsrec` round-trip + playback == layer-4a interp.
  Built in CI (no GPU); `SEADS_CLIENT` stays OFF so guardian never pulls a GUI lib.
- **Web globe viewer** `src/client/web/` — zero-build Three.js globe (CDN), JS interpolation
  mirroring `interp_ref` (linear lat/alt, shortest-arc lon/bearing, hold at edges), HUD +
  playback controls + orbit camera + the 8 km ceiling shell. **Screenshot-verified** rendering the
  3-ship demo. (`dt` clamp guards tab-background rAF leaps.)
- **Optional native viewer** `seads_viewer` (`viewer_main.cpp`, `-DSEADS_CLIENT=ON`) — raylib 3D
  globe, wall-clock render-interpolation, trails, HUD, orbit camera, `--selfcheck N` headless mode.
- **Live local-input loop (track A)**  ✅ DONE (2026-06-29, no seal). The native viewer's `--fly`
  mode flies the OWN ship from keyboard input through `predict::Predictor` (layer 4b) at a fixed
  100 Hz wall-clock step; remotes stay on the layer-4a interp path — prediction + interpolation on
  one globe. Input maps A/D→`target_phi`, W/S→`target_climb` into a `seads::Command` (kernel
  re-clamps to the envelope). Headless `--fly --selfcheck N` proves the input→prediction path with
  no GPU/recording. `seads_viewer` now links `seads_predict`+kernel/replay/det_math (guarded by
  `SEADS_CLIENT`, CI untouched). ctest 7/7; golden `529c6a05…` unchanged; receipt `…-89a1974.yml`.
- **Chase camera + flight-control scheme (track A stretch)**  ✅ DONE (2026-06-29, no seal).
  `src/client/viewer_main.cpp` `run_fly` now builds a **chase camera** from the own ship's local
  tangent frame (`local_basis` at lat/lon + heading psi → forward; eye placed behind/above along
  `-forward`, offset by a free-look az/el). Two input modes:
    - **Mouse-aim (default):** a central reticle (clamped to a unit disk inside `RETICLE_ZONE_PX`)
      maps x→bank, y→climb (`fly_reticle_command`); drawn as a 2D overlay. Keyboard works too as
      gross input.
    - **Free-look (hold SPACE):** cursor is disabled so mouse delta pans the camera all the way
      around + up/down the plane; flight is keyboard-only.
  **Controls:** A/D bank, W/S pitch→climb (`fly_keyboard_command` → kernel `Command`); **Q/E yaw**
  and **Shift/Ctrl throttle** are NOT kernel commands (the kernel has no yaw axis and constant TAS)
  so they are applied DOWNSTREAM by **re-seeding `predict::Predictor`** through the public Kernel API
  (the reconcile path) — no kernel/rail/golden touched. Wheel zooms `chase_dist`; P pauses, R resets.
  The marker (`draw_aircraft`) visibly rolls with bank and tilts with pitch; remotes drawn the same.
  Presentation-only: golden `529c6a05…` unchanged, 12/12 gates + ctest 7/7 (GCC). Receipt
  `…v1.4r0-6d3b06b.yml`.
  - **⚠ INTERIM BAND-AIDS to remove once Track B (§8) lands** (flagged in code): (a) the bank/turn
    is real but **pitch is NOT** — `target_climb` is a clamped vertical rate, so the marker's pitch
    is an **exaggerated presentation cue** (`own_pitch = cmd fraction × 25°`), not a flight-path
    angle; (b) **yaw and throttle are re-seed hacks**, not physics; (c) the own ship spawns at TAS
    150 to stay inside the Ki-61 LUTs. All three exist only because the kernel is a constant-TAS
    placeholder — **§8 B1/B2 make pitch, throttle and energy real in the kernel and these come out.**
- **Remaining polish (track A stretch, no seal):** aircraft-model meshes (vs the marker sphere);
  vendor Three.js for fully-offline web; an optional `--fly` web path. None touch a rail/golden.

### 6. Netcode state-sync (multiplayer flight MVP)  ← IN PROGRESS (option B: recommended)
Server-authoritative state synchronization: kernel both ends, predict own aircraft, interpolate
remotes ~100 ms, `world_hash` as desync tripwire, snapshots for correction/late-join. Build out
`src/net/` GEO-001 codec first (lat/lon×1e7, bearing×1e6, h×1e3, ZigZag+LEB128) + loopback tests.
Suggested first moves for the next agent (build bottom-up, each layer gated before the next):
- **GEO-001 codec first, in isolation.**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  `src/net/geo001.{h,cpp}` (`seads_net` lib) mirrors `tools/geo001_ref.py` bit-for-bit: ZigZag +
  LEB128 over fixed-point i64, quantize/dequantize (round half away from zero), and a GeoPoint record
  in fixed field order (lat, lon, bearing, alt). Cross-impl parity gate mirrors det_math exactly:
  `gen_geo001_vectors.py` → `src/net/geo001_vectors.h` (integer-driven, byte-reproducible);
  `seads_geo001_test` asserts byte-identical wire on encode + exact round-trip on decode (307 i64,
  68 point, 15 quant). 7 new Hypothesis tests (`tests/property/test_geo001.py`; 20 → 27). Gates wired:
  guardian.yml (gen `--check` + reference self-test + `seads_geo001_test` per leg), make_receipt.py
  (`geo001_codec` gate). PASS under GCC + Clang locally (ctest 2/2); golden unchanged
  (`529c6a05…9218fe16`). Ledger: ADR-Step6-GEO001-Codec-v1.3r0, Forge card Step6, receipt
  `…v1.3r0-f2d7e94.yml`. **No seal** (implements the GEO-001 rail to spec; any *deviation* would reseal).
  *Note for next agent:* `seads_net` deliberately does NOT link det_math (no transcendentals; keep it
  off the sim-feeding path). Decoded wire values are lossy by quantization — never feed back as canonical.
- **Snapshot serialization** of `KernelState` over GEO-001 at the 20 Hz snapshot cadence.
  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed). `src/net/snapshot.{h,cpp}` (in `seads_net`)
  frames a world snapshot over the geo001 codec: header `(protocol, server_tick, n)` then
  `n × (id, GeoPoint)`, self-delimiting. `from_kernel` maps kernel **radians → wire degrees**
  (psi heading → GEO-001 bearing) via hex-float `RAD2DEG`. The canonical LE-f64 `Kernel::snapshot()`
  stays the world_hash source of truth; this wire snapshot is a separate **quantized** transport.
  Parity gate mirrors layer 1: `gen_snapshot_vectors.py` → `src/net/snapshot_vectors.h`;
  `seads_snapshot_test` byte-exact (4 snapshots, 5 conv) under GCC+Clang (ctest 3/3). 5 new
  Hypothesis tests (`tests/property/test_snapshot.py`; 27 → 32). Gates wired (guardian.yml,
  make_receipt `snapshot_codec`). Ledger: ADR-Step6-Snapshot-Serialization-v1.3r0, Forge card,
  receipt `…v1.3r0-13726a4.yml`. **Field-scope deferral (documented, not silent):** `phi`/`tas`
  are NOT on the wire — GEO-001 has no scale for them, so adding them is a reseal. Layer 2 supports
  remote **interpolation**; full prediction (bank/energy) awaits a later layer.
- **Loopback lockstep harness (layer 3)**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  Two in-process kernels stepped from one shared input timeline produce an identical per-tick
  `world_hash` every tick — the desync tripwire. `tools/lockstep_ref.py` is the canonical reference
  (inline `LOCKSTEP-SK-001` scenario: 3 aircraft / 600 ticks / turns+climbs+ceiling predamp; two ref
  kernels, shared timeline, per-tick canonical hash, stop at first divergence, **negative control**
  proving the tripwire trips). `src/net/lockstep.{h,cpp}` mirrors it (`tick_hash`,
  `apply_inputs(a,b,cmd,env,tick)`, `run(...)`). Cross-impl parity is proven **exhaustively**: a
  SHA-256 digest over *every* per-tick hash (+ per-tick checkpoints) must match the reference —
  `gen_lockstep_vectors.py` → `src/net/lockstep_vectors.h` (self-contained hex-float scenario, byte-
  reproducible); `seads_lockstep_test` asserts in-sync + digest==reference + tripwire-trips. 4 new
  Hypothesis tests (`tests/property/test_lockstep.py`; 32 → 36). Gates wired: guardian.yml (gen
  `--check` + reference self-test + `seads_lockstep_test` per leg), make_receipt.py (`lockstep` gate).
  PASS under GCC + Clang locally (ctest 4/4); all 4 goldens unchanged (`529c6a05…9218fe16` et al.).
  Ledger: ADR-Step6-Lockstep-v1.3r0, Forge card Step6-Lockstep, receipt `…v1.3r0-8d5fad8.yml`.
  *Key boundaries:* the tripwire compares the **canonical** hashing snapshot (`Kernel::snapshot()`,
  raw LE f64), NOT the lossy GEO-001 wire; the timeline carries sim `Command`s (bank/climb), never
  wire bits; snapshot byte layout is untouched (only hashed per tick). **Deviation from this doc's
  earlier suggestion (documented):** lockstep is its **own** lib `seads_lockstep` (not folded into
  `seads_net`) because it drives the kernel — folding it in would make the pure wire-codec lib pull
  det_math+kernel transitively. See ADR §4.
- **Remote interpolation buffer (layer 4a)**  ✅ DONE (2026-06-28, seal v1.3r0 — no seal needed).
  Client-side, downstream-only: a buffer of decoded GEO-001 snapshots that interpolates remote
  aircraft at a render time ~100 ms in the past (smooth motion despite 20 Hz / jitter / loss).
  `tools/interp_ref.py` is the canonical reference (`SnapshotBuffer.sample(render_tick)`; LINEAR
  lat/alt, SHORTEST-ARC lon across the antimeridian + bearing across 360°, CLAMP/HOLD at edges).
  `src/net/interp.{h,cpp}` mirrors it in `seads_net` (pure IEEE +−×÷, no det_math/kernel — so it
  reproduces **bit-for-bit** cross-toolchain under the strict-FP/no-FMA flags, like the codecs).
  `gen_interp_vectors.py` → `src/net/interp_vectors.h` (10 cases, hex-float, byte-reproducible);
  `seads_interp_test` asserts bit-exact vs reference. 8 new Hypothesis tests
  (`tests/property/test_interp.py`; 36 → 44). Gates wired: guardian.yml + make_receipt.py
  (`interp` gate). PASS under GCC + Clang (ctest 5/5); all 4 goldens unchanged. Tier-2 ledger
  (CLAUDE.md §2): commit message + receipt `…v1.3r0-733b8a3.yml`, no ADR/seal. *Uses only
  lat/lon/bearing/alt — all on the wire today, so NO reseal.* It NEVER feeds the sim (downstream
  presentation only). Step 5's renderer is the natural consumer.
- **Client-side prediction (layer 4b)**  ✅ DONE (2026-06-28, seal **v1.4r0** — Tier-1 reseal).
  Predict the OWN aircraft from local input each tick (running the **real sealed kernel**) and
  reconcile against authoritative snapshots: snap to authoritative @ server_tick, drop inputs ≤
  that tick, **replay** the buffered local inputs forward. `tools/predict_ref.py` is the canonical
  reference (`PREDICT-SK-001`: 1 own aircraft, 300 ticks, 20 Hz snapshots, ~100 ms lag);
  `src/net/predict.{h,cpp}` mirrors it in its **own** `seads_predict` lib (drives the kernel, like
  lockstep). Reconcile re-seeds via the public `Kernel::add()`+`step()` — kernel math + snapshot
  layout **untouched** (only additive read-only getters `Kernel::phi()/tas()`). Cross-impl parity
  is digest-exact: `gen_predict_vectors.py` → `predict_vectors.h`; `seads_predict_test` asserts
  seamless (predicted == truth every tick) + digest == reference + **heal control** (a 2⁻²⁰ m
  initial-alt desync diverges at tick 1, reconciles back at tick 15) + **negative control** (no
  reconcile ⇒ stays broken). 6 new Hypothesis tests (`tests/property/test_predict.py`; 44 → 52,
  incl. +2 snapshot for phi/tas). **The reseal:** GEO-001 stays geography-only; `phi`/`tas` ride a
  new auxiliary **KIN-001** block (phi×1e6, tas×1e3) as a 2nd self-delimiting snapshot section
  (`protocol 1→2`) — the chosen wire shape (vs extending the GeoPoint). The **bit-exact** path
  reconciles against CANONICAL state (layer-3 doctrine); the **lossy-wire** reseed (real
  remote/late-join) is bounded by the quantum (property test) — *that* is why phi/tas are on the
  wire. PASS under GCC + Clang (ctest 6/6); all 4 goldens unchanged (`529c6a05…` et al.). Ledger:
  ADR-Step6-Prediction-v1.4r0, Forge card, SEAL_CARD v1.4r0, receipt `…v1.4r0-11e07d4.yml`.
- The kernel itself should not change; if it must, that's a seal event — keep net code strictly
  outside the kernel boundary (no feeding bits back in). Ledger per layer (ADR + card + receipt).

### 7. (Post-MVP) Guns/projectiles — new seal, deferred per doctrine.

## 8. Track B — the REAL flight model (the "flight kernel")  ← THE NEXT BIG TASK

> **Why:** SEADS today flies a *constant-TAS kinematic placeholder*. To be an actual dogfight sim
> the kernel must model **energy and aerodynamics**: thrust vs drag (so speed changes and there is a
> throttle), lift vs weight at an angle of attack (so **pitch is real** and altitude is *earned*),
> and load factor (so turn rate trades against speed and you can stall/black out). This is the heart
> of the product. **It must live in the kernel** — physical truth has to be bit-for-bit identical
> across toolchains/arch for lockstep multiplayer (CLAUDE.md §0/§1). Therefore **every phase below is
> a SEAL** (moves all four goldens) and runs the **full ritual**: det_math additions proven vs the
> MPFR oracle first, an adversarial **Auditor** pass, an **ADR**, then `/seal`. Do NOT shortcut this
> with `/fp:fast`, FMA, or libm — that breaks the one promise the whole project exists to keep (§5).

### 8.0 Design decisions to lock FIRST (write an ADR — this is a genuine architectural fork)
- **State vector.** Today `(lat, lon, psi, phi, alt, tas)`. The model adds longitudinal state. Pick
  the variable set and FIX it: recommended **add flight-path angle `gamma`** (vertical channel) and
  keep **`tas` as a true integrated state**; treat **bank `phi`** as before; derive **load factor
  `n`** and **angle-of-attack `alpha`** per tick from the aero solve (don't store what you can
  recompute — fewer states = fewer wire fields = smaller reseal). Decide explicitly whether `gamma`
  and `alpha` are *states* or *derived*; the golden hashes depend on it.
- **Inputs.** `Command` grows from `{target_phi, target_climb}` to roughly
  `{target_phi, pitch_or_g_cmd, throttle}`. Decide the pitch axis semantics NOW: **commanded load
  factor `n`** (arcade, stable, easy to clamp to a structural/`CLmax` limit) **vs commanded AoA**
  (sim-y, needs a stall model). Recommended: **commanded-g** for B2, add an AoA/stall layer in B3.
- **Atmosphere/density.** Rails say "still air". DECIDE: keep **constant sea-level density ρ₀**
  (avoids `det_exp`/`det_pow` entirely — strongly recommended for the first cut) **or** ISA density
  vs altitude (forces new det_math transcendentals + full MPFR coverage — defer, and if adopted it
  is itself a rail/seal change). Document the choice in the ADR; constant-ρ keeps B1–B4 to algebra +
  `sqrt`.
- **Integration scheme.** Δt is fixed at 0.01 s (rail). Pick the integrator and FREEZE the operation
  order (semi-implicit Euler is fine and cheap). The golden is defined by the *exact* op sequence,
  so `ref_kernel.py` and `kernel.cpp` must share it bit-for-bit (same trick as `lut_eval` today).
- **det_math budget.** Energy/aero needs at minimum **`det_sqrt`** (true airspeed terms, stall
  speed, `n`↔bank). If you keep constant density and commanded-g you can likely avoid `exp/log/pow`.
  Every new det_math primitive: add to `det_math.{h,cpp}`, mirror in `detmath_ref.py`, extend
  `det_math_oracle.py` + `gen_detmath_vectors.py` (structured boundaries + seeded random), prove
  ≤ target ULP vs MPFR, and asm-audit for FMA — BEFORE any kernel call uses it.

### 8.1 Phase B1 — Longitudinal energy: thrust/drag ⇒ real TAS + throttle  ✅ DONE (2026-06-29, seal v1.5r0)
**Shipped.** Energy model in `Kernel::step(cmd,env)` + `ref_kernel.step_scenario` (bit-identical op
order): `n=1/cos φ`, `q=½ρ₀V²`, `D = qS·cd0 + k·CL²·qS` (CL from `L=n·m·g₀`), `T = thr·T₀·(1−V/Vmax)`,
`Vdot=(T−D)/m − g₀·req/V`, floor `V_MIN=30`. Constants `RHO0`/`V_MIN` as shared hex-floats; **no new
det_math** (`+−×÷` + `det_cos`). `Command.throttle` added (default 0.0). Aero params on every
envelope (`mass_kg, wing_area_m2, cd0, induced_k, thrust_static_n, v_max_mps`) via the shared
`envelopes.py` loader → `gen_envelope_tables.py` + `gen_lockstep/predict_vectors.py` (all emit the
scalars). Goldens: **Sphere unchanged** `529c6a05…`; Turn `1d15c57a…`, Climb `4119a280…`, TurnClimb
`a1d9ce03…`, **Accel (new)** `225d5c13…`. Gates: 12/12 receipt PASS, ctest 7/7 GCC+Clang, 57 property
tests (+5 `test_energy.py`). guardian.yml extended with Accel in all 3 golden lists. Ledger:
ADR-Step8-FlightModel-B1-v1.5r0, SEAL_CARD v1.5r0, receipt `…v1.5r0-*.yml`. Viewer Shift/Ctrl now a
real throttle. **Initial aero is a balance pass — top speeds run low; retune is data-only in B4.**
The ORIGINAL B1 plan (kept for reference):
Make speed a real integrated state. Per tick: `T(throttle) − D(V) − W·sin(gamma)` drives `V̇`;
`V += V̇·dt`. Drag `D = ½·ρ₀·V²·S·C_d` (parasitic `C_d0` + induced `k·C_L²`); thrust from a simple
prop curve `T = throttle · T_static · (1 − V/V_max)` (or a small LUT). Add per-airframe **mass,
`S_ref`, `C_d0`, `k`, `T_static`, `V_max`** to the envelope schema (extends `data/tuning/envelopes/`
+ `gen_envelope_tables.py`). `Command` gains `throttle`. **Wire/reseal:** `tas` is already on the
KIN-001 block; **`throttle` need not be on the wire** (it's an input, not a state) — but confirm the
snapshot still captures enough to reconstruct (it does: `tas` is the state). Likely **no wire format
change** in B1 → the reseal is "golden moved", not "protocol bumped". Deliverables: kernel + `ref_
kernel.py` mirror, `det_sqrt` (if used) through the oracle, **regenerate all 4 goldens** + a new
**GOLDEN-SK-Accel-001** (throttle step → speed up/slow down), property tests (energy monotonic under
zero throttle = decel; level flight equilibrium), ADR + `/seal` (→ v1.5r0).

### 8.2 Phase B2 — Lift & pitch: flight-path angle from commanded-g  ✅ DONE (2026-06-29, seal v1.6r0)
**Shipped.** γ (flight-path angle) is a stored kernel state; `step(cmd,env)` is a full 3-DOF point mass
(Vdot=(T−D)/m−g0·sinγ; γ̇=(g0/V)(n·cosφ−cosγ); ψ̇=(g0/V)(n·sinφ/cosγ); alṫ=V·sinγ; ground speed V·cosγ).
`Command.target_climb`→**`target_g`** (commanded load factor n; global clamp [−3,9]). Generalizes B1
(γ=0,n=1/cosφ ⇒ old ψ̇=g0·tanφ/V). **No new det_math.** State 6→7-tuple; canonical snapshot +γ ⇒ **all
goldens moved incl. Sphere** (trajectory identical, γ=0 appended). **Wire reseal KIN-001→KIN-002** (γ×1e6,
snapshot protocol 2→3). New **GOLDEN-SK-Pitch-001**. C++≡Python bit-for-bit GCC+Clang; 12/12 gates, ctest
7/7, 64 property tests. Viewer band-aids retired. Ledger: ADR-Step8-FlightModel-B2-v1.6r0, SEAL_CARD
v1.6r0, receipt `…v1.6r0-*.yml`. **Documented limits → B3:** no stall/C_Lmax (n only globally clamped);
cosγ→0 vertical singularity (kept out of scenarios). The ORIGINAL B2 plan (kept for reference):
Make pitch real. Vertical channel becomes `alṫ = V·sin(gamma)`, `gammȧ` from the net normal force:
commanded load factor `n_cmd` → required `C_L` → if within `C_Lmax` the lift turns the velocity
vector (`gammȧ = g·(n·cos(phi) − cos(gamma))/V` for the pitch plane; bank splits `n` between turn and
climb). Replace `target_climb` with **`target_g`** (or pitch). Now W/S commands actual nose
attitude/energy trade, not a teleported climb rate, and the viewer's nose tilt becomes *physical*
(remove the exaggeration band-aid in `viewer_main.cpp`). Couples to B1: pulling g adds induced drag →
bleeds speed. **Wire/reseal:** if `gamma` becomes a *state* it should join KIN-001 (→ KIN-002 or a
protocol bump) so remotes/late-join reconstruct attitude — this is a **wire reseal** like layer 4b
was. Deliverables: kernel + ref mirror, goldens regenerated + **GOLDEN-SK-Loop/Pitch-001**, property
tests (sustained-vs-instantaneous turn, energy bleed in a pull), ADR + `/seal`.

### 8.3 Phase B3 — Limits & stall: C_Lmax, accelerated stall, structural g  ✅ DONE (2026-06-29, seal v1.7r0)
**Shipped.** Achievable load factor `n` bounded per-airframe by BOTH the structural g limits
(`n_max_struct`/`n_min_struct`) AND the C_Lmax aerodynamic ceiling `n_aero=cl_max*qS/(m*g0)`:
`n=clamp(n_cmd, max(n_min_struct,-n_aero), min(n_max_struct,n_aero))`. Accelerated stall (turn
collapses below the corner speed `V*=stall*sqrt(n_max_struct)`) + the 1 g stall speed are emergent;
B2's global `[-3,9]` placeholder retired; post-stall lift held at C_Lmax (no departure — deferred).
**No new state/wire/det_math** (n derived). New envelope scalars `cl_max,n_max_struct,n_min_struct`
(x8, coherent with `stall_tas_mps`; balance pass -> retune in B4). **Conservative extension: the 6
prior goldens are byte-identical** (limits never bind; verified C++=Python GCC+Clang); only NEW
**GOLDEN-SK-Stall-001** (`1a57b6d1...a0526fc6`, ki61 accelerated stall + p47d structural-g zoom).
12/12 gates, ctest 7/7, 72 property tests (+8 `test_stall.py`), `tuning_probe` extended. Ledger:
ADR-Step8-FlightModel-B3-v1.7r0, SEAL_CARD v1.7r0. The ORIGINAL B3 plan (kept for reference):
Clamp `C_L` to `C_Lmax(M?)`; exceeding → stall (lift breaks, `gammȧ`/turn collapse, optional
departure). Add a **structural g limit** and a **corner speed** that falls out naturally (max
sustained turn where `n·V` is maximized). This is what makes airframes *feel* different and makes
energy fighting emergent. Deliverables: stall/corner property tests per airframe, goldens
regenerated, ADR + `/seal`.

### 8.4 Phase B4 — Per-airframe tuning pass  ✅ DONE (2026-06-29, seal v1.8r0)
**Shipped.** Re-tuned all **8** envelopes so the roster reads true. Emergent level top speeds were
~30 % low (109–142 m/s); B4 lifts them to **historical** (148–195 m/s ≈ 533–702 km/h) by moving
**only `thrust_static_n`/`v_max_mps`** per airframe (`tools/b4_retune.py` solves a (top-speed,
best-climb) fit); all turn/stall params held fixed ⇒ **iconic turn ordering preserved exactly**
(A6M2/Spitfire best sustained, P-47 worst; instantaneous turn byte-identical) and B3 C_Lmax↔stall
coherence untouched. `v_max_mps` is now the flat-curve thrust-zero asymptote (~700, not Vne); `v_ne`
validation-only. **No new det_math/state/wire** — but it moves the **6 scenario goldens → reseal**
(Sphere unchanged). New `tools/perf_probe.py` (emergent-perf analyzer). 12/12 gates, 7/7 goldens
C++≡Python GCC+Clang, ctest 7/7, 72 property tests (`test_energy` settle horizon 80→140 s). Ledger:
ADR-Step8-FlightModel-B4-v1.8r0, SEAL_CARD v1.8r0, receipt. The ORIGINAL B4 plan (kept for reference):
Re-tune all **8** envelopes to the new aero parameters so the roster's relative strengths read true
(Spitfire turns, P-47 dives/zooms, A6M2 low-speed turn, etc.). Mostly **data-only** (rides the
current seal) like §3 was — *unless* you change a shared kernel default. Add the energy/aero columns
to each `data/tuning/envelopes/*.json`, re-run `tuning_probe.py` (extend it with energy checks),
optionally `hash_sign_json.py` to sign them.
> **Note (B4 reality vs the plan):** post-B1/B2/B3 the kernel *integrates* envelope aero, and 5 of
> the 8 airframes are baked into goldens, so a holistic retune **did** move goldens (a reseal) — the
> "rides the seal" hope was written pre-B1. The energy *columns* already existed (added in B1); B4 was
> a value retune, done as the cheapest-possible reseal (only 2 scalars/airframe moved).

### 8.5 Phase B5 — (optional, defer) ISA atmosphere vs altitude  (rail + seal)
Only if you want altitude to change performance (thinner air up high). Forces `det_exp`/`det_pow`
into det_math with full MPFR coverage, and changes the "still air / constant" atmosphere rail →
**rail reseal** (`/reseal` for the rail value, `/seal` for the model). Big lift; not needed for a
compelling dogfight. Keep constant-ρ until proven necessary.

### Cross-cutting (every phase)
- **Mirror-first determinism:** write/extend `tools/ref_kernel.py` (and `detmath_ref.py`) to the new
  math, get the Python gate green, THEN bit-match it in `kernel.cpp`. The Python harness *defines*
  the golden; C++ must reproduce it (CLAUDE.md §4).
- **Goldens:** every phase regenerates the 4 existing goldens (their hashes WILL change — that's the
  seal) and should ADD at least one scenario that exercises the new dynamic (accel, pitch/loop,
  stall). Use the existing `config/scenarios/*.json` → `seads_scenario` machinery.
- **Gates:** run the full §4 + §-verify sweep and `make_receipt.py` before and after; CI
  (`guardian.yml`) must stay green across MSVC/GCC/Clang × x64/AArch64 — the AArch64 legs are the
  real test of any new det_math (FMA/libm divergence risk).
- **Wire & viewer:** if a new variable becomes wire state, bump the snapshot protocol (KIN-002) and
  update `snapshot.{h,cpp}` + refs (reseal). The `--fly` viewer is the live test bed — after B2,
  delete the pitch-cue exaggeration and the yaw/throttle re-seed hacks in `viewer_main.cpp` and feed
  the real `Command` instead.
- **Seal sequence:** v1.4r0 → **v1.5r0** (B1) → v1.6r0 (B2) → v1.7r0 (B3) → data (B4) → optional
  v1.8r0 (B5). Don't batch phases into one seal; small seals keep the golden diffs auditable.

## Governance reminder (lean — see CLAUDE.md §2)
Governance is now minimal. **The whole law:** (1) keep the §4 gates green; (2) if a rail value or a
golden `world_hash` changes, bump the seal (`/seal` or `/reseal`) and say *why* in the commit
message — never silently; (3) keep the auto-receipt (`make_receipt.py`) and **this file** current —
that pair is the ledger and the continuity that actually matters; (4) ADRs/Forge cards are
**optional** (write one only for a genuine architectural fork). Everything else just rides the
current seal. The `validate_snapshot`/`validate_scenarios` gates fail loudly if anything moves a
hash, so trust the gates and ship.
