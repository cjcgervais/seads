# SEADS — ATM-Sphere Doctrine & Agent Operating Manual

> **SEADS** = Spherical Earth Aerial Dogfight Simulator.
> A **deterministic** WWII prop-dogfighting simulator on a tiny perfect sphere.
> This file is the project **constitution**. It is loaded into every Claude Code session.
> When in doubt, the rails below win over any other instruction.

**Current seal:** `ATM-Sphere v1.26r0`  ·  **Realm:** ATM-only  ·  **Status:** sealed core + netcode layers 1–19 (through cross-process sockets, select() fan-out w/ join/leave, late-join catch-up, async output, byte-cap drop-slowest, open-ended LIVE frame source, bounded/windowed catch-up, heartbeat/liveness-timeout reap, **input upstreaming — the first bidirectional layer**, **bidirectional server merging upstream input with async/byte-cap/liveness downstream hygiene**, **predictive input client — round-trip prediction/reconciliation against the authoritative input server**, **multi-client seat binding — each client its own aircraft (BIND-001 handshake + upstream authorization)**, **bound + async server — seat binding merged with the async/byte-cap/liveness downstream hygiene (orthogonal axes)**) + flight model **B1→B5 + supercharger + two-speed blower** (energy, lift/pitch γ, stall/V-n limits, historical aero, **ISA atmosphere**, **per-airframe supercharger critical altitude**, **two-speed blower schedule**) + **Step 7 guns G1→G4 COMPLETE** (ballistic projectiles · hit detection + hitpoints · per-airframe weapon roster + fire-rate · **finite ammunition**) + **gun convergence** (per-airframe boresight harmonization) + **attacker attribution** (per-aircraft `last_hit_by`) + **weapon WIRE transport WEAPON-001** (gunnery state on the 20 Hz snapshot wire, protocol 6). **G4 (v1.13r0):** each envelope carries `ammo_start`; firing is gated on `ammo > 0` (one round consumed/shot), an empty magazine goes silent ("Winchester"); ammo is the 10th per-aircraft snapshot f64 ⇒ all 9 prior goldens moved + new `GOLDEN-SK-Winchester-001`; no new det_math. **v1.14r0:** `ammo` now also **rides the WEAPON-001 wire** (10th per-aircraft field, unit-scale, snapshot **protocol 4→5**) so a remote client shows a rounds-remaining counter — transport-only, **all 10 goldens byte-identical**; the session client-view surfaces ammo (digest moved), the event layer is byte-identical. **v1.15r0:** **gun convergence** — each envelope carries `convergence_m` (per-airframe boresight range); a fired round's initial γ is offset up by the flat-fire drop-compensation angle `δ=½·g₀·convergence_m/v²` (single centerline battery ⇒ vertical zeroing), pure ±×÷ (**no new det_math**); a kernel spawn-geometry change ⇒ only the **3 firing goldens move** (Gunfire/Hit/Winchester; kill + depletion preserved), the 7 non-firing stay byte-identical; no wire change. **v1.16r0:** **attacker attribution** — each aircraft records `last_hit_by` (index of the aircraft whose round most recently damaged it, -1 = never hit; set at hit time from the striking round's `owner`, persists through death ⇒ names the killer); the 11th per-aircraft snapshot f64 ⇒ **all 10 goldens move but provably additive** (strip the 11th f64 ⇒ the v1.15r0 hash byte-for-byte); still no new det_math; off-wire that seal. **v1.17r0:** `last_hit_by` **rides the WEAPON-001 wire** (11th per-aircraft field, unit-scale, snapshot **protocol 5→6**) + the layer-6 event channel gains **`Event.attacker`** (the server stamps the target's post-step last_hit_by onto each derived hit/kill ⇒ an attributed kill-feed a remote client can render) — transport-only, **all 10 goldens byte-identical**; session + event digests moved. **Per-round hit queue (no-seal, rides v1.17r0):** the kernel appends one `HitEvent` per CONNECTING ROUND (cleared each step, never hashed ⇒ goldens untouched); the layer-6 channel sources it instead of hp-delta observation ⇒ same-tick multi-round damage arrives as distinct attributed events (sealed EVENT_DIGEST unchanged; new EVENT-MULTIHIT-001 cross-impl vector). **v1.18r0:** **region damage + kill tally** — each airframe carries ENGINE/WING/TAIL **region sub-pools** (0.375/0.5/0.25 × hp_start, exact-binary global fractions; independent thresholds beside the total hp) drained by the striking round's **approach aspect** (`wrap_pi(round ψ − target ψ)`: astern < π/4 → TAIL, head-on > 3π/4 → ENGINE, else WING); a dead region degrades a LIVING plane (engine out → thrust 0; wing out → n_aero halved; tail out → commanded bank/g forced to 0/1 — a straight 1-g mush) and **`kills`** tallies +1 on the attacker per killing round. 12th–15th per-aircraft snapshot f64s ⇒ **all 10 goldens move but provably additive** (strip the 4 new f64s ⇒ each v1.17r0 hash byte-for-byte) + new **GOLDEN-SK-EngineOut-001** (a head-on burst kills an A6M2's engine, not the plane; the survivor decelerates thrustless); `HitEvent` gains `region`; STILL zero new det_math; off-wire that seal (session/event digests unchanged). **v1.19r0:** the region pools + `kills` **ride the WEAPON-001 wire** (12th–15th per-aircraft fields — pools ×1e3 like hp, kills ×1e0 like ammo; snapshot **protocol 6→7**) ⇒ a remote client draws the **damage state + a scoreboard** — transport-only (5th wire reseal), **all 11 goldens byte-identical**; the session client-view surfaces the four fields (digest moved), the event layer is byte-identical. **Region-damage arc closed end-to-end (kernel + wire).** **GOLDEN-SK-YakLa-001 (no-seal, rides v1.19r0):** a 12th golden pins Yak-3/La-7 envelope interpolation (LUT segments, clamps, weapon scalars) in C++ ↔ Python lockstep — purely additive, 11 prior goldens byte-identical. **v1.20r0:** **per-airframe region toughness** — the global region-pool fractions become envelope scalars `engine_frac/wing_frac/tail_frac` (multiples of 1/8 ≤ 1, tuning_probe-enforced ⇒ pools stay f64- and wire-milli-exact); `Kernel::add` gains three defaulted params (defaults = the sealed globals) and every envelope-seeded caller passes the envelope's values — radials (P-47D 0.625, La-7 0.5) out-tough the liquid-cooled inlines (0.25); the A6M2 keeps engine 0.375 so EngineOut-001's story is byte-preserved. ZERO new det_math. A hashed-state VALUE retune ⇒ **11 envelope-seeded goldens move, Sphere byte-identical** (proven value-only: field-wise snapshot diff shows every changed f64 is a pool re-size; kinematics/hp/ammo/kills + projectiles identical); no wire change (protocol stays 7); session digest moves, event digest byte-identical; guardian.yml unchanged. **v1.21r0:** **flight model B5 — ISA atmosphere:** density is finally altitude-dependent — σ(alt)=ρ/ρ₀ is a **sealed 17-node LUT** (500 m spacing over [0,8000 m]; ICAO ISA provenance via the offline `tools/gen_isa_lut.py`; hex-floats shared bit-for-bit; runtime = the existing `lut_eval`, generalized 5-node→n-node with an identical op sequence ⇒ **ZERO new det_math**, ninth consecutive zero-transcendental seal). Two application points in the envelope-driven step: `q=½ρ₀σ(alt)V²` (drag + the B3 n_aero ceiling become altitude-coupled: stall/corner TAS rise aloft, sustained turns bleed harder up high) and `T×=σ` (engine power falls with density ⇒ no airframe exceeds its B4 historical top speed anywhere in the band; per-airframe supercharger critical-alt modeling deferred). No-arg kinematic path + projectile drag deliberately untouched; **no wire change** (σ derives from `alt`, already on GEO-001; protocol stays 7). A genuine MODEL seal (B2-class, not value-only): **the 12 scenario goldens move, Sphere byte-identical**; every sealed story re-verified under B5 (Hit's kill / Winchester's tick-891 depletion / EngineOut's engine-kill-survivor — now decelerating to ~137 not ~131 m/s); **GOLDEN-SK-YakLa-001 RE-DESIGNED** (its LUT-breakpoint crossings degenerated under σ; new dive-based profiles restore all crossings + a MEASURED structural bind n_aero [8.70,9.00]>8) + NEW **GOLDEN-SK-Altitude-001** (two Spitfires, identical commands, 800 vs 6900 m — divergent acceleration, the same g-6 pull un-clamped low vs aero-clamped high, two σ-node crossings) ⇒ **13 goldens**, guardian.yml +1 in all three lists. Session digest → `21aaab49…` (FINAL_WEAPON facts identical), **event digest byte-identical**; test_stall/test_region_toughness made σ-aware; +5 property tests (`test_isa.py`) ⇒ **182**. Rails 300→310 (`atmosphere_density` block). See ADR-Step8-FlightModel-B5-v1.21r0. **Netcode layer 14 (no-seal, rides v1.21r0):** **bounded/windowed catch-up** — `broadcast_live` gains `catchup_window` (0 = retain all = layer-13 exactly): only the LAST W produced payloads are retained (oldest evicted per new frame, counted in `Stats.trimmed`) ⇒ an open-ended live stream runs catch-up in **O(W) memory**; a mid-stream joiner receives EXACTLY the contiguous suffix `frames[max(0,fi−W):]` (the window bounds only replay depth, never a live client's bytes; fi ≤ W recovers full catch-up + the sealed digest). Transport-only ⇒ all 13 goldens byte-identical; `seads_netwindow_test` (ctest 18→19), +2 property tests ⇒ **184**. See ADR-Step-Net-Layer14-BoundedCatchup-v1.21r0. **v1.22r0:** **supercharger critical altitude** (B5's named follow-up): each envelope gains **`crit_alt_m`** (19th AERO field; a multiple of 500 m in [0,8000], tuning_probe-enforced ⇒ σ(crit) is EXACTLY a sealed LUT node) and the thrust scaling becomes **`T×=lapse`, `lapse=min(1, σ(alt)/σ(crit_alt_m))`** — one comparison + one divide, **ZERO new det_math** (tenth consecutive); crit=0 reproduces B5 bit-for-bit (guarded). Below its critical altitude an engine holds RATED power; above it power falls with σ/σ_crit. Roster (WWII rated heights): P-47D 8000 (turbo) · P-51 7500 · Bf 109/Spitfire 6000 · La-7/A6M2 4500 · Ki-61 4000 · Yak-3 3000. The two B4 speed knobs **re-anchored** (`tools/supercharger_retune.py`: top speed AT the critical altitude = the B4 historical target; sea-level climb preserved) ⇒ top TAS RISES with altitude to the historical value at crit then falls, emergent sea-level tops land on history (P-51 576 / P-47D 549 / La-7 585 / A6M2 457 km/h), v_max_mps drops to 250–360 (steeper, more physical). A MODEL seal: **all 13 scenario goldens move, Sphere byte-identical**; every story re-verified (Hit/Winchester/EngineOut/Stall); **Altitude-001's divergence INVERTS** (the high Spitfire is now the faster one — the supercharger's point; description re-written, schedule unchanged); **YakLa-001 re-measured** (two tail phases g 0.6→1.6 restore the 85 m/s crossing; structural bind [9.45,9.72]>8) + NEW **GOLDEN-SK-Supercharger-001** (a Yak-3 crosses ITS OWN crit in flight — the comparison FLIPS — while a P-51 on the identical schedule crosses the same σ-node with no flip) ⇒ **14 goldens**, guardian.yml +1 in all three lists. No wire change (protocol stays 7). Session digest → `ccc2f504…` (FINAL_WEAPON facts identical), **event digest byte-identical**; +6 property tests (`test_supercharger.py`) ⇒ **190** (test_isa recalibrated). Rails 310→320. See ADR-Step8-FlightModel-Supercharger-v1.22r0. **v1.23r0:** **A6M2 engine-toughness retune (data-only):** `engine_frac 0.375→0.5` — the Sakae radial joins the radial class (La-7-level, under the R-2800's 0.625), retiring v1.20r0's golden-preservation pin; ONE value in ONE JSON, ZERO new det_math (eleventh consecutive). Measured surprise: **EngineOut-001 is BYTE-IDENTICAL** (the bigger pool dies on the SAME 3rd round — 35→23→11→0 — so the thrust-cut tick, glider, and final snapshot are unchanged; only never-serialized mid-run drain values differ, description re-written); **exactly 3 goldens move (Gunfire/Hit/Winchester — value-only proof: ONE f64 each, the live engine_hp constant 26.25→35)**, Sphere + the other 9 byte-identical. Session digest → `f67368e9…`, event digest byte-identical; 190 property tests (toughness pin moved with the retune); rails 320→330 (header only); guardian.yml unchanged. See ADR-Step7-Guns-A6M2EngineToughness-v1.23r0. **v1.24r0:** **projectile σ-drag (B5's last deferral closed):** a fired round finally flies in the SAME thin air — the projectile advance scales `PROJ_DRAG_K` by σ at the round's PRE-step altitude (`Vdot = -k·σ(alt)·V² - g₀·sinγ`; one `air_sigma`/round/tick, the aircraft step's convention; same sealed LUT + `lut_eval` ⇒ **ZERO new det_math**, twelfth consecutive; σ(0)=1.0 exactly ⇒ sea-level rounds bit-identical, guarded). Spawn geometry/hit detection/ttl/no-arg path/wire untouched (**no protocol change**). A kernel MODEL seal: **exactly 4 goldens move (Gunfire/Hit/Winchester/YakLa — in every one the AIRCRAFT state is field-wise byte-identical, only round kinematics moved: thin-air rounds fly faster), 10 byte-identical incl. EngineOut-001** (its first connect moved a tick earlier, 29→28, visible only in the never-hashed journal — it survives unmoved a second seal running). Stories re-measured: Hit's six TAIL connects now ticks 39/41/44/47/50/53 (kill tick 53 + hp ladder unchanged), Winchester's tick-891 depletion exact, YakLa's aircraft/crossings untouched (only its 63 live rounds moved). Session digest → `966aca05…` (FINAL_WEAPON facts byte-identical); **event digest MOVES for the first time since v1.17r0** (→ `2a9ae8a3…`: the event channel reports per-round hit ticks — exactly what σ-drag shifts; kill facts intact); scenario_params/lockstep/predict + all codec vectors in sync untouched; trajectory.js dogfight regenerated (same 3 kills / 18 events). +7 property tests (`test_proj_sigma.py`, incl. the re-measured Hit/EngineOut journals pinned) ⇒ **197**. Rails 330→340 (text only); guardian.yml unchanged. See ADR-Step7-Guns-ProjectileSigmaDrag-v1.24r0. **v1.25r0:** **two-speed blower schedule** (v1.22r0's deferred follow-up): the three airframes with a genuine two-speed supercharger gain **`crit_lo_alt_m`** (20th AERO field, LOW/MS-gear full-throttle height, 500 m grid, **0 = single-speed**) + **`gear2_frac`** (21st, HIGH/FS-gear rated-power fraction, dyadic /16), and the lapse becomes **`max(min(1, σ/σ(crit_lo)), gear2_frac·min(1, σ/σ(crit_alt)))`** (flat–fall–flat–fall). The two-min/max block is entered **ONLY when `crit_lo_alt_m > 0`** ⇒ single-speed thrust is **bit-for-bit v1.22r0**; **ZERO new det_math** (thirteenth consecutive). Roster: P-51 3000/0.875, La-7 1500/0.875, Yak-3 1000/0.9375; the two speed knobs re-anchored (`tools/blower_retune.py`: top speed at crit_alt in HIGH gear = B4 target, SL climb at rated LOW gear). A MODEL seal bounded to the three two-speed airframes: **exactly 4 goldens move (Accel/Pitch P-51 knob retune + Supercharger/YakLa re-measured), 10 byte-identical incl. Sphere** + NEW **GOLDEN-SK-Blower-001** (a Yak-3 traverses all three below-crit regimes — rated LOW, falling MS with a σ-node crossing, gear-shift max() flip t~2227, flat FS 0.9375 EXACTLY — beside a single-speed Bf 109 control) ⇒ **15 goldens**, guardian.yml +1 in all three lists. No wire change (protocol stays 7). Session/lockstep/predict moved ENVELOPE LITERALS ONLY — **no digest moved** (`966aca05…` UNCHANGED, event digest byte-identical). HUD rider: native `hud_power` + web `pwr` gain the max()/gear2 branch, scoreboard crit shows lo/hi. +6 property tests (`test_blower.py`) ⇒ **203**. Rails 340→350. See ADR-Step8-FlightModel-TwoSpeedBlower-v1.25r0. **v1.26r0:** **netcode layer 15b — input upstreaming (the FIRST BIDIRECTIONAL layer):** a client sends tick-stamped `Command`s UP into the authoritative sealed kernel. New sealed upstream wire **INPUT-001** (`rails.wire.command`; fields apply_tick, aircraft, seq, target_phi, target_g, throttle, fire — the three continuous fields ×1e6; **reuses the sealed GEO-001 ZigZag+LEB128 pipeline ⇒ ZERO new det_math**, fourteenth consecutive) + the canonical tick-stamped **`CommandQueue`** ordering contract: **drop-at-ingest-if-stale** (apply_tick below the floor — the next tick still to be stepped, advanced only by the producer's progress, never wall-clock — is rejected, byte-identical to never arriving) **+ hold-last** (a tick with no command reuses the aircraft's last, = `phase_at`) with the `(apply_tick, aircraft)` winner **maximal under a total order on the wire fields** (seq, then quantized phi/g/throttle/fire) ⇒ a **pure function of the command SET, blind to arrival order**. The input-driven **`InputProducer`/`broadcast_input`** (a sibling of `broadcast_live` ⇒ the sealed layer-13/14/15a bridges byte-for-byte untouched; `session::serialize_world` exposed so its frames are byte-identical to `build_server_frames`). **A wire reseal only** (like WEAPON-001 v1.12r0 — a seal only because the wire is a sealed rail): no kernel/det_math/tuning touched ⇒ **all 15 goldens byte-identical**, no protocol-7 change, sealed session/event digests `966aca05…` UNMOVED. Determinism claim (honest scope): given commands delivered before their apply_tick (adequate lead), the produced frame stream is invariant to upstream ORDER/CHUNKING (the input-direction "lossy ≠ nondeterministic"); late arrival is a deterministic DROP. BRIDGE `seads_netinput_test` (ctest 20→21 `netinput_bridge`): codec parity vs `input001_ref.py` (14-byte pin); LEG 1 — a grid-exact INPUT-SK-001 scenario driven by SCRAMBLED (reversed@1B / rotate3@7B) upstream commands produces frames byte-identical to `build_server_frames`; LEG 2 — drop/hold-last/max-seq reproduce the canonical frames in-process. +6 property tests (`test_input001.py`) ⇒ **211**. Rails 350→360 (`wire.command` block); guardian.yml unchanged (ctest-only bridge, like layers 13–15a). See ADR-Step-Net-Layer15b-InputUpstream-v1.26r0. **Netcode layer 16 (no-seal, rides v1.26r0):** **bidirectional server** — `broadcast_bidi` merges layer 15b's upstream input path with the layer-11/12/15a downstream output hygiene (async per-client send buffers + byte-cap drop-slowest + liveness reap), a SIBLING of both (sealed `broadcast.cpp` + `broadcast_input` byte-for-byte untouched; `netinput::Stats` +additive `capped`/`reaped`). The merge touches only the downstream transport ⇒ the kernel's output stays a pure function of the canonical command SET. BRIDGE `seads_netbidi_test` (ctest 21→22 `netbidi_bridge`): LEG A scrambled upstream through the FULL async path == `build_server_frames` byte-for-byte; LEG B liveness reaps a dead client at cap=0 while a hook-drained FAST stays byte-identical + its commands drove the sim; LEG C byte-cap sheds the dead client. +5 property tests (`test_bidi.py` — the two directions ORTHOGONAL) ⇒ **216**. TRANSPORT-ONLY ⇒ all 15 goldens byte-identical, no protocol/digest change, guardian.yml unchanged. See ADR-Step-Net-Layer16-BidiServer-v1.26r0. **Netcode layer 17 (no-seal, rides v1.26r0):** **predictive input client — the round-trip loop closed.** `netpredict::run_predictive_client` (`src/net/inputclient.{h,cpp}`) is the client counterpart to `broadcast_input`: the layer-4b `predict::Predictor` driven against the layer-15b/16 authoritative INPUT server. It predicts the OWN aircraft each tick from the same commands it upstreams and reconciles against the authoritative frames under a client `lag` + downstream loss set. CANONICAL reconcile ⇒ **SEAMLESS** (predicted == authoritative every tick, a zero-correction no-op — the round-trip theorem); WIRE reconcile ⇒ bounded within a few wire quanta, reproducible ⇒ a cross-impl parity digest. BRIDGE `seads_netpredict_test` (ctest 22→23 `netpredict_bridge`), mirroring `tools/inputpredict_ref.py` bit-for-bit: LEG 1 seamless canonical (digest `abecf117…`); LEG 2 socket round-trip — scrambled upstream through `broadcast_input` → frames byte-identical to `build_server_frames` → own ship reconciled vs the lossy wire within 4.6e-9 rad (digest `007c4b9d…`); LEG 3 a stale-DROPPED command mispredicts and HEALS at the next authoritative frame, no-reconcile control diverges forever. +8 property tests (`test_inputpredict.py`) ⇒ **224**. TRANSPORT/CLIENT-ONLY ⇒ all 15 goldens byte-identical, no protocol/digest change, guardian.yml unchanged. See ADR-Step-Net-Layer17-InputPrediction-v1.26r0. **Netcode layer 18 (no-seal, rides v1.26r0):** **multi-client seat binding — each client its own aircraft** (settles the positional/unauthenticated client→aircraft binding every bidirectional layer flagged). `broadcast_bound` (`src/net/boundserver.{h,cpp}`) is a SIBLING of `broadcast_input` (blocking downstream base ⇒ layers 15b/16/17 byte-for-byte untouched) with three transport additions: a join-order **`SeatPolicy`** (lowest free aircraft index in `[0,n)`, spectators when full, reuse on leave — deterministic in the join/leave order), a one-time **`BIND-001`** handshake (`src/net/bind001.{h,cpp}` ↔ `tools/bound_ref.py`; `[version 0x01][ZigZag+LEB128 seat][ZigZag+LEB128 n_aircraft]`, seat -1 = spectator; reuses the sealed GEO-001 i64 codec ⇒ ZERO new det_math; **transport metadata like the layer-7 framing envelope, NOT a sealed `rails.wire` block ⇒ no seal**) sent as the client's FIRST downstream frame so it is TOLD its aircraft, and **upstream authorization** (`seat_authorizes(seat,aircraft) := seat>=0 && aircraft==seat`; a foreign-aircraft command or any spectator command is DROPPED — `Stats.cmds_unauth` — byte-identical to never arriving, the OUT_OF_RANGE reject's determinism class). The kernel's frames stay a pure function of the AUTHORIZED command SET. BRIDGE `seads_netbound_test` (ctest 23→24 `netbound_bridge`): LEG 1 — THREE clients each learn their seat + upstream ONLY that seat's commands (distinct scrambles) ⇒ each downstream byte-identical to `build_server_frames`, distinct BIND seats, `cmds_unauth=0`; LEG 2 — a seat-0 client upstreams EVERY aircraft's commands, only aircraft-0's take effect (`cmds_unauth=3`) ⇒ downstream = the aircraft-0-only world byte-for-byte (foreign commands change nothing); LEG 3 — seat policy (fill/spectator/reuse) + authorization predicate + BIND-001 codec pin `[0x01,0x02,0x06]`. +8 property tests (`test_bound.py`) ⇒ **232**. TRANSPORT-ONLY ⇒ all 15 goldens byte-identical, no protocol/digest change, guardian.yml unchanged. See ADR-Step-Net-Layer18-MultiClientBinding-v1.26r0. **Netcode layer 19 (no-seal, rides v1.26r0):** **bound + async server** — `broadcast_bound_async` (`src/net/boundasyncserver.{h,cpp}`) merges the layer-18 seat binding (SeatPolicy + BIND-001 handshake + upstream authorization) with the layer-16 async/byte-cap/liveness downstream hygiene — the two orthogonal axes composed. A SIBLING of both `broadcast_bound` AND `broadcast_bidi` (it owns a `BoundAsyncClient` = layer-16 `BidiClient` + a `seat`, and its own flush/enqueue/cap/reap/drop helpers ⇒ sealed `broadcast.cpp`, `broadcast_bound`, `broadcast_bidi` all byte-for-byte untouched; no new `Stats` field or accessor — layers 16+18 already added `capped`/`reaped`/`cmds_unauth`/`n_aircraft()`). Three integration seams: the seat is **freed on EVERY drop path** (`drop_client` releases it — EOF, fatal flush, byte-cap shed, liveness reap), **BIND-001 is ENQUEUED as the first downstream bytes** (not layer-18's blocking send_all — the accepted socket is non-blocking, so the BIND rides the same FIFO send buffer as every frame and the async flush delivers it first), and **per-seat authorization** unchanged. The two axes touch disjoint machinery (authorization = upstream admission filter; hygiene = downstream delivery) ⇒ the kernel's frames stay a pure function of the AUTHORIZED command SET, invariant to seat permutation, upstream reorder/chunking, AND any downstream cap/liveness drop. BRIDGE `seads_netboundasync_test` (ctest 24→25 `netboundasync_bridge`, gcc+clang): LEG 1 — three seated clients each fly their own seat (scrambled) THROUGH the async path ⇒ each downstream (after BIND) byte-identical to `build_server_frames`, distinct seats, zero unauth/capped/reaped; LEG 2 — foreign-reject through the async path (`cmds_unauth=3` ⇒ aircraft-0-only world); LEG 3 — a DEAD client reaped (cap=0) / byte-cap shed (liveness=0) + its seat freed while a hook-drained seated FAST stays byte-identical to its seat reference + its own-seat commands drove the sim, DEAD delivered `[BIND | strict frame-prefix]`. +5 property tests (`test_boundasync.py` — the three axes AUTHORIZE × PRODUCE × DELIVER are orthogonal; seat freed on any drop reason) ⇒ **237**. TRANSPORT-ONLY ⇒ all 15 goldens byte-identical, no protocol/digest change, guardian.yml unchanged; bidirectional late-join catch-up now UNBLOCKED on top of this. See ADR-Step-Net-Layer19-BoundAsyncServer-v1.26r0. (Authoritative seal/golden ledger: `docs/SEAL_CARD.md` + `NEXT_STEPS.md`.)

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
| Atmosphere | **Still air** — no wind / Coriolis / weather. **B5 (v1.21r0): ISA density-vs-altitude** — σ(alt)=ρ/ρ₀, a sealed 17-node LUT (500 m spacing, ICAO troposphere provenance, `lut_eval` interp ⇒ zero new det_math) scaling BOTH `q=½ρ₀σV²` AND thrust. **v1.22r0 (supercharger):** thrust carries `lapse=min(1, σ(alt)/σ(crit_alt_m))` — rated power below the per-airframe critical altitude (envelope scalar, multiple of 500 m ⇒ σ(crit) an exact LUT node; 0 = B5 bit-for-bit). **v1.25r0 (two-speed blower):** for the three two-speed airframes the lapse becomes `max(min(1, σ/σ(crit_lo)), gear2_frac·min(1, σ/σ(crit_alt)))` (crit_lo_alt_m a 500 m multiple, gear2_frac dyadic /16); entered ONLY when crit_lo_alt_m > 0 ⇒ single-speed = v1.22r0 bit-for-bit. **v1.24r0 (projectile σ-drag):** the round's lumped drag is σ-scaled too (`Vdot -= k·σ(alt)·V²`, pre-step alt; σ(0)=1 ⇒ sea-level bit-identical); no-arg path untouched |
| Ceiling | **ATM_TOP = 8,000 m**, **SOFT = 100 m** (predamp 7,900–8,000 → hard clamp) |
| Kinematics | Intrinsic S²; closed-form great-circle step. **B2 (v1.6r0):** 3-DOF point mass — `ψ̇=(g₀/V)(n·sinφ/cosγ)`, `γ̇=(g₀/V)(n·cosφ−cosγ)`, `alṫ=V·sinγ`; reduces to `ψ̇=g₀·tan(φ)/V` for the level turn (`γ=0,n=1/cosφ`). No-arg straight golden keeps the pure kinematic tail. **No Cartesian fallback.** |
| Determinism | `det_math` only. **Ban** `std::sin/cos/tan/atan2/asin/acos/sqrt/pow`, fast-math, FMA, x87. |
| Wire/Hash | **GEO-001** — lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128. **+KIN-002** aux block (phi×1e6, tas×1e3, **gamma×1e6**) for prediction. **+WEAPON-001** aux block (per-aircraft hp×1e3, fire_cd×1e3, **ammo×1e0**, **last_hit_by×1e0**, **engine_hp/wing_hp/tail_hp×1e3**, **kills×1e0**; per round: GeoPoint + damage×1e3 + ttl/owner exact i64) for MP gunnery replication; snapshot **protocol 7** (WEAPON-001 v1.12r0; `ammo` added v1.14r0, protocol 4→5; `last_hit_by` added v1.17r0, protocol 5→6; region pools + kills added v1.19r0, protocol 6→7). The wire is lossy/downstream — `Kernel::snapshot()` stays the world_hash source of truth |
| Roster | Sealed **8**: P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, **P-51** |
| State | Per-aircraft 7-tuple `(lat, lon, psi, phi, alt, tas, gamma)` — γ (flight-path angle) added in B2 (v1.6r0). |
| Golden | **GOLDEN-SK-Sphere-001** — 10,000 ticks from (0°,0°), ψ=45°, TAS=250 m/s → world_hash matches cross-toolchain (13 sealed goldens total; see SEAL_CARD) |
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
- **Yak-3/La-7 envtab entries (no-seal, rides v1.19r0):** the mesh-variant arc's last gap —
  `gen_envelope_tables.py` now emits the FULL 8-airframe roster (every `data/tuning/envelopes/*.json`,
  not just scenario-referenced; asserts scenario refs exist) ⇒ `envelope_tables.h` gains YAK3 + LA7
  (purely additive, 6 existing entries byte-identical, value-inert constants); the recorder maps them
  to v3 type codes 4/5 and the `--dogfight` demo gains a third staggered hunter/prey pair (Yak-3
  guns down a La-7: 6 ships, 3 kills, 18 journal events) so every roster silhouette appears on
  screen. trajectory.js demo refreshed (stale protocol-4 GUNKILL → protocol-7 6-ship dogfight).
  Rider: the recorder now seeds per-airframe `ammo_start` beside `hp_start` (matching
  scenario_main.cpp + the net layers) ⇒ `--id` scenario replays are magazine-faithful
  (Winchester-001 records ammo 100→0, not a 500-round default that never empties).
  Data/tooling + presentation only ⇒ all 11 goldens byte-identical.
- **GOLDEN-SK-YakLa-001 (no-seal, rides v1.19r0):** the Yak-3/La-7 arc's sealed capstone — a
  **12th golden** covering the two new envtab entries in C++ ↔ Python lockstep. Two-ship,
  near-antipodal (non-interacting), 3,000 ticks: each airframe traverses its `phi_max`/`roll_rate`
  LUT **interpolation segments** at non-breakpoint TAS with over-limit banks (interpolated clamp +
  roll_rate slew bind; the Yak-3 crosses its 160 m/s breakpoint both directions, the La-7 visits
  three segments and holds a 1 s **structural-limit** g-8 pull with n_aero ∈ [8.20, 9.27] > 8),
  then fires a closing burst (rof 7 vs 6, ammo 140→111 / 170→136, **63 live rounds in the sealed
  snapshot** pinning muzzle_v/convergence/damage). Scenario JSON + generated scenario_params entry
  only — **purely additive: no rail value or existing golden hash changes ⇒ the 11 prior goldens
  byte-identical, no seal bump**; guardian.yml gains the 12th golden in all three lists;
  +3 property tests (`test_yakla_golden.py` non-degeneracy guards) ⇒ 169.
- **v1.20r0** — **Step 7 guns / per-airframe region toughness:** the v1.18r0 global region-pool
  fractions become per-airframe envelope scalars **`engine_frac`/`wing_frac`/`tail_frac`**
  (16th–18th AERO fields; each a positive multiple of 1/8 ≤ 1, tuning_probe-enforced, so every
  pool is exact in f64 AND milli-exact on the WEAPON-001 wire). `Kernel::add` ↔ `ref_kernel.Aircraft`
  gain three defaulted params (defaults = the sealed v1.18r0 globals ⇒ envelope-less callers
  bit-identical); scenario/session/event/record callers pass the envelope's values. Historically
  flavored: radial engines tough (P-47D 0.625, La-7 0.5), liquid-cooled inlines tender (0.25),
  light wings 0.375 (Bf 109/A6M2/Yak-3/La-7), sturdy tails 0.375 (P-47D/P-51); the A6M2 keeps
  engine_frac 0.375 so GOLDEN-SK-EngineOut-001's drain sequence is preserved byte-for-byte.
  **ZERO new det_math** (eighth consecutive zero-transcendental guns seal). A hashed-state VALUE
  retune (like B4) ⇒ **11 envelope-seeded goldens move, Sphere byte-identical** — proven
  value-only via field-wise snapshot diff (every changed f64 is a pool re-size; kinematics/hp/
  ammo/kills + all projectile bytes identical across all 11 ⇒ no sealed trajectory, kill,
  depletion, or engine-out outcome changed). No wire change (snapshot protocol stays 7); session
  digest moves (client view carries the retuned pools), event digest byte-identical;
  guardian.yml unchanged. +6 property tests ⇒ **175**. Rails 290→300. See
  ADR-Step7-Guns-RegionToughness-v1.20r0.
- **Netcode layer 13 (no-seal, rides v1.20r0):** **open-ended LIVE frame source** — the sealed
  kernel is stepped INSIDE the broadcast loop instead of precomputing the stream.
  `session::FrameProducer` (incremental server half; `build_server_frames` reimplemented ON it ⇒
  batch == incremental bytes by construction, sealed session digest unchanged) +
  `netbcast::broadcast_live` (broadcast_async's loop fed by a pull `FrameSource` of UNKNOWN
  length; same buffers/cap/drain — layers 11/12 verbatim; catchup=true retains produced payloads
  on the fly ⇒ layer-10 replay semantics from a stream that never existed as a whole; honest
  boundaries: O(stream) catch-up retention — run open-ended sources with catchup=false — and
  per-frame join service). BRIDGE `seads_netlive_test` (producer==batch byte-for-byte; live
  EARLY reconstructs the sealed digest; rendezvoused joiner gets the exact suffix; catch-up from
  on-the-fly history reconstructs the digest); demo `seads_netserver … [live]`. TRANSPORT-ONLY ⇒
  all 12 goldens byte-identical, ctest 17→18, +2 property tests ⇒ **177**.
  See ADR-Step-Net-Layer13-LiveSource-v1.20r0.
- **v1.21r0** — **flight model B5: ISA atmosphere.** Density is altitude-dependent: σ(alt)=ρ/ρ₀ as a
  sealed 17-node LUT (ICAO troposphere provenance offline in `tools/gen_isa_lut.py`; runtime =
  `lut_eval` ⇒ ZERO new det_math), scaling BOTH `q=½ρ₀σV²` (drag + n_aero become altitude-coupled)
  AND thrust `T×=σ` (no airframe exceeds its B4 top speed anywhere; supercharger modeling
  deferred). 12 scenario goldens move (a genuine MODEL seal), Sphere byte-identical; every sealed
  story re-verified; **YakLa-001 re-designed** (dive-based profiles restore the σ-degenerated LUT
  crossings + a measured structural bind) + new **GOLDEN-SK-Altitude-001** (same airframe/commands,
  800 vs 6900 m) ⇒ 13 goldens. No wire change. 182 property tests. **Flight-model arc B1→B5
  COMPLETE.** See ADR-Step8-FlightModel-B5-v1.21r0.
- **Netcode layer 14 (no-seal, rides v1.21r0):** **bounded/windowed catch-up** — `broadcast_live`
  gains `catchup_window` (0 = retain all = layer-13 exactly): the retained history holds only the
  LAST W produced payloads (oldest evicted per new frame, counted in `Stats.trimmed`) ⇒ catch-up
  in O(W) memory on a stream of any length; a mid-stream joiner is replayed the retained window
  and delivered EXACTLY the contiguous suffix `frames[max(0,fi−W):]` (the window bounds only
  replay depth — a live client's bytes are byte-identical under any window; fi ≤ W recovers full
  catch-up + the sealed digest — the layer-13 degenerate case). Frame-denominated by design (not
  bytes — that's layer 12's `cap_bytes`; not time — no wall-clock in transport). BRIDGE
  `seads_netwindow_test` (W=1 / partial / W≥N legs over a live-stepped FrameProducer); demo
  `seads_netserver … [window]`. TRANSPORT-ONLY ⇒ all 13 goldens byte-identical, ctest 18→19,
  +2 property tests ⇒ **184**. See ADR-Step-Net-Layer14-BoundedCatchup-v1.21r0.
- **v1.22r0** — **supercharger critical altitude** (B5's named follow-up): envelope `crit_alt_m`
  (19th AERO field, multiple of 500 m ⇒ σ(crit) an exact sealed LUT node; 0 = B5 bit-for-bit) +
  thrust lapse `min(1, σ(alt)/σ(crit_alt_m))` (one comparison + one divide, ZERO new det_math,
  tenth consecutive) + the two B4 speed knobs re-anchored at each airframe's critical altitude
  (`tools/supercharger_retune.py` — top TAS peaks at the historical value at crit; emergent
  sea-level tops land on history). All 13 scenario goldens move, Sphere unchanged; Altitude-001's
  divergence inverts (described honestly); YakLa-001 re-measured (tail g 0.6→1.6); NEW
  GOLDEN-SK-Supercharger-001 (an in-flight critical-altitude branch flip) ⇒ 14 goldens.
  190 property tests. See ADR-Step8-FlightModel-Supercharger-v1.22r0.
- **v1.23r0** — **A6M2 engine-toughness retune (data-only):** `engine_frac 0.375→0.5` (the Sakae
  radial gets its due; v1.20r0's golden-preservation pin retired). EngineOut-001 proved
  BYTE-IDENTICAL (same 3rd-round knockout); exactly 3 goldens moved (Gunfire/Hit/Winchester —
  one f64 each, the live engine-pool constant). See ADR-Step7-Guns-A6M2EngineToughness-v1.23r0.
- **v1.24r0** — **projectile σ-drag (B5's last deferral closed):** the round's lumped drag is
  σ-scaled at its pre-step altitude (`Vdot -= k·σ(alt)·V²`; same sealed LUT + lut_eval, ZERO
  new det_math — twelfth consecutive; σ(0)=1 ⇒ sea-level rounds bit-identical). A kernel MODEL
  seal: exactly 4 goldens moved (Gunfire/Hit/Winchester/YakLa — aircraft state field-wise
  byte-identical in all four, only round kinematics), 10 byte-identical incl. EngineOut-001
  (first connect a tick earlier, journal-only). Event digest moved for the first time since
  v1.17r0 (per-round hit ticks shifted 1 tick; kill facts intact). +7 property tests ⇒ 197.
  See ADR-Step7-Guns-ProjectileSigmaDrag-v1.24r0.
- **v1.25r0** — **flight model: two-speed blower schedule (v1.22r0's deferred follow-up):** the
  three airframes with a genuine two-speed supercharger (P-51 3000/0.875, La-7 1500/0.875,
  Yak-3 1000/0.9375) gain `crit_lo_alt_m` (20th AERO field, LOW/MS-gear full-throttle height,
  500 m grid, 0 = single-speed) + `gear2_frac` (21st, HIGH/FS-gear rated-power fraction, dyadic
  /16), and the lapse becomes `max(min(1, σ/σ(crit_lo)), gear2_frac·min(1, σ/σ(crit_alt)))`
  (flat–fall–flat–fall); entered ONLY when crit_lo_alt_m > 0 ⇒ single-speed thrust bit-for-bit
  v1.22r0; ZERO new det_math (thirteenth consecutive). The two speed knobs re-anchored per
  airframe (`tools/blower_retune.py`). A MODEL seal bounded to the three: exactly 4 goldens
  moved (Accel/Pitch P-51 knob retune + Supercharger/YakLa re-measured), 10 byte-identical incl.
  Sphere + NEW GOLDEN-SK-Blower-001 (a Yak-3 through all three below-crit regimes beside a
  single-speed Bf 109 control) ⇒ 15 goldens. No wire change; session/lockstep/predict moved
  envelope literals only (no digest moved). +6 property tests ⇒ 203. Rails 340→350. HUD rider:
  native + web `pwr` gain the two-speed branch. See ADR-Step8-FlightModel-TwoSpeedBlower-v1.25r0.
- **Netcode layer 15a (no-seal, rides v1.25r0):** **heartbeat / liveness-timeout LEAVE** —
  `broadcast_live` gains `liveness_frames` (0 = layer-14 exactly): a client that makes NO receive
  progress (drains zero bytes AND stays pending) for more than `liveness_frames` consecutive
  produced frames — a silently-dead peer that never sends EOF (killed process / network partition;
  no clean TCP EOF for minutes, so neither readable nor writable) — is REAPED (new `Stats.reaped`,
  also a leave). The signal is the client's cumulative kernel-accepted bytes (`BufClient.sent_total`,
  tracked in `flush_client`); the idle counter resets when the buffer empties or bytes move, so a
  slow-but-ALIVE client is never reaped — only a stalled one. ORTHOGONAL to layer 12: `cap_bytes`
  sheds by backlog SIZE, `liveness_frames` reaps by STALENESS ⇒ a dead client is bounded even at
  `cap_bytes=0`. Frame-denominated (no wall-clock — doctrine); `broadcast_live` only (the batch
  paths bound a dead client via the finite stream + drain). BRIDGE `seads_netheartbeat_test`
  (LEG 1: at cap_bytes=0 a never-reading DEAD client reaped/backlog-bounded/byte-prefix while FAST
  is byte-identical; LEG 2: an aggressive liveness=1 never bites a continuous reader — sealed
  digest `966aca05…`, reaped=0); demo `seads_netserver … [liveness]`. TRANSPORT-ONLY ⇒ all 15
  goldens byte-identical, no digest moved, ctest 19→20, +2 property tests ⇒ **205**. See
  ADR-Step-Net-Layer15-Heartbeat-v1.25r0.
- **v1.26r0 — netcode layer 15b: input upstreaming (the FIRST BIDIRECTIONAL layer).** A client sends
  tick-stamped `Command`s UP into the authoritative sealed kernel. New sealed upstream wire
  **INPUT-001** (`src/net/input001.{h,cpp}` ↔ `tools/input001_ref.py`; fields apply_tick, aircraft,
  seq, target_phi, target_g, throttle, fire; the three continuous ×1e6; reuses the sealed GEO-001
  ZigZag+LEB128 ⇒ **ZERO new det_math**, fourteenth consecutive) + the canonical tick-stamped
  **`CommandQueue`** (`src/net/cmdqueue.{h,cpp}` — drop-at-ingest-if-stale + hold-last; the
  `(apply_tick, aircraft)` winner is maximal under a total order on the wire fields ⇒ a pure function
  of the command SET, blind to arrival order) + the input-driven **`InputProducer`/`broadcast_input`**
  (`src/net/inputserver.{h,cpp}` — a SIBLING of `broadcast_live` so layers 13/14/15a are byte-for-byte
  untouched; `session::serialize_world` exposed so frames match `build_server_frames`). A WIRE RESEAL
  ONLY (like WEAPON-001 v1.12r0): no kernel/det_math/tuning touched ⇒ **all 15 goldens byte-identical**,
  no protocol-7 change, sealed session/event digests `966aca05…` unmoved. Determinism claim (honest
  scope): given commands delivered before their apply_tick, the produced frames are invariant to
  upstream ORDER/CHUNKING; late arrival is a deterministic DROP. BRIDGE `seads_netinput_test`
  (ctest 20→21 `netinput_bridge`): codec parity pin; LEG 1 — scrambled (reversed@1B / rotate3@7B)
  upstream commands reproduce `build_server_frames` byte-for-byte; LEG 2 — drop/hold-last/max-seq
  in-process. +6 property tests (`test_input001.py`) ⇒ **211**. Rails 350→360 (`wire.command`).
  See ADR-Step-Net-Layer15b-InputUpstream-v1.26r0.
- **Netcode layer 16 (no-seal, rides v1.26r0):** **bidirectional server — upstream input + downstream
  output hygiene.** `broadcast_bidi` (`src/net/bidiserver.{h,cpp}`) merges layer 15b's UPSTREAM input
  path with the layer-11/12/15a DOWNSTREAM hygiene: it is `broadcast_input`'s loop with
  `broadcast_live`'s per-client userspace send buffers (async output, no back-pressure) + opt-in
  `cap_bytes` byte-cap (drop-slowest) + `liveness_frames` reap (silently-dead peer). A **SIBLING** of
  both (owns its own `BidiClient` + flush/enqueue/cap/reap helpers) ⇒ sealed `broadcast.cpp` and
  `inputserver.cpp`'s `broadcast_input` byte-for-byte untouched; `netinput::Stats` gains additive
  `capped`/`reaped`. `BidiClient` carries an UPSTREAM `StreamReassembler` beside a DOWNSTREAM send
  buffer + the liveness fields; the one place it differs from `broadcast_live` is the readable-client
  handler — a readable client is USUALLY sending commands (drain the burst → CommandQueue), only
  `recv≤0` is a leave (guarded recvs never hit the non-blocking EWOULDBLOCK `<0`). The merge touches
  only the downstream transport (WHEN bytes move, WHICH slow/dead clients shed) ⇒ the kernel's output
  stays a pure function of the canonical command SET. BRIDGE `seads_netbidi_test` (ctest 21→22
  `netbidi_bridge`): LEG A — scrambled upstream (reversed@1B / rotate3@7B) through the FULL async path
  reproduces `build_server_frames` byte-for-byte to a cooperative reader (cap=0/liveness=0); LEG B —
  liveness reaps a never-reading DEAD client at cap_bytes=0 (reaped=1/leaves=1/capped=0) while a
  hook-drained FAST stays byte-identical + its commands drove the sim; LEG C — byte-cap sheds DEAD
  (capped=1/leaves=1/reaped=0), FAST untouched. Honest scope (first bidi-hygiene cut): no late-join
  catch-up (needs the positional client→aircraft binding settled first), binding stays positional +
  unauthenticated. +5 property tests (`test_bidi.py` — upstream canonicalization × downstream
  cap/liveness delivery are ORTHOGONAL) ⇒ **216**. TRANSPORT-ONLY ⇒ all 15 goldens byte-identical
  (Sphere `6914a994…`), no protocol/digest change, guardian.yml unchanged (ctest-only bridge).
  See ADR-Step-Net-Layer16-BidiServer-v1.26r0.
- **Netcode layer 17 (no-seal, rides v1.26r0):** **predictive input client — the round-trip loop
  closed.** `netpredict::run_predictive_client` (`src/net/inputclient.{h,cpp}`, in `seads_netinput`) is
  the client counterpart to `broadcast_input`: the layer-4b `predict::Predictor` driven against the
  layer-15b/16 authoritative INPUT server. It predicts the OWN aircraft forward each tick from the SAME
  commands it upstreams (fire bit dropped — motion only) and reconciles against the authoritative frames
  under an integer client `lag` + a downstream loss set. Two reconcile SOURCES (mirroring `predict_ref`):
  **CANONICAL** — snap to the full-precision own state ⇒ **SEAMLESS** (predicted == authoritative EVERY
  tick, the reconcile a zero-correction no-op: the client's local sim IS the server's, offset only by
  latency — the round-trip theorem); **WIRE** — snap to the DECODED lossy protocol-7 own state (the
  realistic path) ⇒ bounded within a few wire quanta, reproducible ⇒ its own-ship hash sequence is a
  cross-impl parity digest. `authoritative_own_states` gives the reference own(0) trajectory (own
  kinematics are independent of the other aircraft absent a hit). BRIDGE `seads_netpredict_test` (ctest
  22→23 `netpredict_bridge`), mirroring `tools/inputpredict_ref.py` bit-for-bit (both digests pinned in
  both): LEG 1 SEAMLESS canonical (in sync all 150 ticks, digest `abecf117…`); LEG 2 socket round-trip —
  scrambled upstream through `broadcast_input` → frames byte-identical to `build_server_frames` → own
  ship reconciled vs the lossy wire within 4.6e-9 rad, digest `007c4b9d…`; LEG 3 a stale-DROPPED command
  mispredicts (ticks 31..59) and HEALS at tick 60, no-reconcile control diverges forever. +8 property
  tests (`test_inputpredict.py`) ⇒ **224**. TRANSPORT/CLIENT-ONLY ⇒ all 15 goldens byte-identical
  (Sphere `6914a994…`), no protocol/digest change, guardian.yml unchanged (ctest-only bridge).
  See ADR-Step-Net-Layer17-InputPrediction-v1.26r0.
- **Netcode layer 18 (no-seal, rides v1.26r0):** **multi-client seat binding — each client its own
  aircraft.** Settles the positional/unauthenticated client→aircraft binding every bidirectional layer
  flagged. `broadcast_bound` (`src/net/boundserver.{h,cpp}`) is a SIBLING of `broadcast_input` (blocking
  downstream base ⇒ layers 15b/16/17 byte-for-byte untouched): a join-order **`SeatPolicy`** assigns each
  client the lowest free aircraft index (spectators when full, reuse on leave), a one-time **`BIND-001`**
  handshake (`src/net/bind001.{h,cpp}` ↔ `tools/bound_ref.py`; `[version 0x01][ZigZag+LEB128 seat][ZigZag+
  LEB128 n_aircraft]`, seat -1 = spectator; reuses the sealed GEO-001 i64 codec ⇒ ZERO new det_math;
  transport metadata like the layer-7 framing envelope, NOT a sealed `rails.wire` block ⇒ **no seal**)
  tells the client its aircraft, and **upstream authorization** (`seat_authorizes(seat,aircraft)`) drops a
  foreign-aircraft/spectator command as `cmds_unauth` (byte-identical to never arriving). The kernel's
  frames stay a pure function of the AUTHORIZED command SET. BRIDGE `seads_netbound_test` (ctest 23→24
  `netbound_bridge`): LEG 1 — 3 clients each fly their OWN seat (scrambled) ⇒ each downstream byte-identical
  to `build_server_frames`, distinct BIND seats, cmds_unauth=0; LEG 2 — a seat-0 client's foreign commands
  rejected (cmds_unauth=3) ⇒ downstream = the aircraft-0-only world; LEG 3 — seat policy + auth predicate +
  BIND-001 codec pin. +8 property tests (`test_bound.py`) ⇒ **232**. TRANSPORT-ONLY ⇒ all 15 goldens
  byte-identical (Sphere `6914a994…`), no protocol/digest change, guardian.yml unchanged (ctest-only).
  See ADR-Step-Net-Layer18-MultiClientBinding-v1.26r0.
- **Netcode layer 19 (no-seal, rides v1.26r0):** **bound + async server** — the layer-18 seat binding
  merged with the layer-16 downstream hygiene, the two orthogonal axes composed. `broadcast_bound_async`
  (`src/net/boundasyncserver.{h,cpp}`) is `broadcast_bidi`'s async `select_rw()` loop (per-client
  userspace send buffers + opt-in byte-cap + liveness reap) with `broadcast_bound`'s three binding
  additions folded in — a SIBLING of BOTH (owns `BoundAsyncClient` = layer-16 `BidiClient` + a `seat`,
  and its own flush/enqueue/cap/reap/drop helpers ⇒ sealed `broadcast.cpp`, `broadcast_bound`,
  `broadcast_bidi` byte-for-byte untouched; no new `Stats` field/accessor — layers 16+18 already added
  `capped`/`reaped`/`cmds_unauth`/`InputProducer::n_aircraft()`). Three integration seams: (1) the seat is
  freed on EVERY drop path (`drop_client` calls `seats.release` — EOF, malformed framing, fatal flush,
  byte-cap shed, liveness reap all funnel through it); (2) **BIND-001 is ENQUEUED as the first downstream
  bytes** (not layer-18's blocking `send_all` — the accepted socket goes non-blocking first, so the BIND
  rides the same FIFO send buffer as every frame and the async flush delivers it first — same ordering,
  through the userspace buffer); (3) per-seat authorization unchanged. The two axes touch DISJOINT
  machinery (authorization = upstream admission filter; hygiene = downstream delivery, neither touching
  the `CommandQueue`) ⇒ the kernel's frames stay a pure function of the AUTHORIZED command SET, invariant
  to the seat permutation, the upstream reorder/chunking, AND any downstream cap/liveness drop. BRIDGE
  `seads_netboundasync_test` (ctest 24→25 `netboundasync_bridge`, gcc+clang native-x64 like layers 7–18):
  LEG 1 — three seated clients each fly their OWN seat (scrambled) THROUGH the async path ⇒ each
  downstream (after its BIND) byte-identical to `build_server_frames`, distinct seats in `[0,3)`, zero
  unauth/capped/reaped; LEG 2 — a seat-0 client's foreign-aircraft commands rejected through the async
  path (`cmds_unauth=3`) ⇒ aircraft-0-only world byte-for-byte; LEG 3 (long ~1 MB stream, pinned 16 KiB
  buffer) — a DEAD client reaped (liveness, cap=0) / byte-cap shed (liveness=0) + its **seat freed**
  (`leaves=1`), while a hook-drained seated FAST stays byte-identical to its own-seat reference + its
  commands drove the sim, and DEAD's delivery is `[BIND | strict frame-prefix]` (the async buffer sent the
  handshake first). Robust to accept-order: the two hygiene sub-legs independently drew FAST into seats 1
  and 0 and the per-seat reference matched each. +5 property tests (`test_boundasync.py` — the three axes
  AUTHORIZE × PRODUCE × DELIVER are orthogonal; a seat is freed on ANY drop reason under a randomised
  join/mixed-drop interleaving; BIND-first ordering) ⇒ **237**. TRANSPORT-ONLY ⇒ all 15 goldens
  byte-identical (Sphere `6914a994…`), no protocol/digest change, guardian.yml unchanged (ctest-only).
  See ADR-Step-Net-Layer19-BoundAsyncServer-v1.26r0.
- next — free pick (none blocking): **bidirectional late-join catch-up** (now doubly UNBLOCKED — the
  binding is settled AND this async server owns the per-client send buffers a catch-up prefix enqueue
  builds on: a replayed joiner gets a seat + BIND, then the catch-up prefix); **authenticated** binding
  (identity, not just join-order position); input prediction of REMOTE aircraft (predict-others, not just
  interpolate); or renderer polish (surface the assigned seat / the predicted-vs-authoritative correction
  on the HUD).
