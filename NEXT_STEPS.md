# SEADS 2026 — Next Steps (handoff)

> ## ►► CURRENT STATE (2026-07-02): **YAK-3 / LA-7 ENVTAB ENTRIES — THE FULL SEALED ROSTER FLIES DONE ✅** (no-seal, rides **ATM-Sphere v1.19r0**)
> **Latest: the mesh-variant arc's last gap closes — Yak-3 and La-7 (the two roster types whose
> silhouettes existed but could never appear in a recording) now have generated envelope-table
> entries and fly in the built-in dogfight demo.** Three small pieces (data/tooling + presentation):
> **(a) `tools/gen_envelope_tables.py` emits the FULL roster:** previously it emitted only
> scenario-referenced envelopes (6 of 8); now it emits every `data/tuning/envelopes/*.json`
> (sorted; with an assert that every scenario-referenced envelope still exists). Both JSONs were
> already complete 15-field B4-retuned tuning data — only the generator's filter excluded them.
> `envelope_tables.h` gains `constexpr Envelope YAK3` + `LA7`; the diff is **purely additive
> (+14 lines — the 6 existing entries byte-identical)**, and unused constexpr constants are inert
> ⇒ lockstep/predict/scenario vector headers verified in sync, untouched.
> **(b) The recorder maps them** (`record_main.cpp type_code_of`): `&envtab::YAK3` → code 4,
> `&envtab::LA7` → code 5 (the STABLE .seadsrec v3 presentation codes that already existed in
> `AircraftType`); the stale "Yak-3 / La-7 simply can't appear" comment is gone.
> **(c) The `--dogfight` demo grows a third staggered hunter/prey pair** (southern lane, lat −3):
> a Yak-3 (tas 230) hunts a La-7 (tas 172) with the proven GOLDEN-SK-Hit-001 tail-chase geometry,
> fires t=450–750 (4×28-dmg rounds kill the 100-hp La-7 at t=514, TAIL region, overkill-clamped
> final round), then breaks. **6 ships, 3 kills, 18 per-round journal events** (was 4 ships / 2
> kills / 14 events). `trajectory.js` demo data refreshed — it had been the STALE protocol-4
> DEMO-GUNKILL since v1.12r0; now the protocol-7 6-ship dogfight incl. the `types` name array.
> **(d) Follow-up rider (code `255eafb`): the recorder seeds per-airframe `ammo_start`** —
> `seed()` passes `env->ammo_start` into `Kernel::add` beside `hp_start`, matching the canonical
> scenario runner (`scenario_main.cpp`) and the net layers (session/event) instead of the
> kernel's 500-round default. A `seads_record --id GOLDEN-SK-Winchester-001` replay is now
> MAGAZINE-FAITHFUL (A6M2 ammo 100 → 0, gun silent — verified by selfcheck; previously it
> recorded a 500-round magazine that never emptied). The dogfight demo's outcome is unchanged
> (every burst ≪ any magazine).
> **DATA/TOOLING + PRESENTATION ONLY: envtab additions are value-inert generated constants; no
> `src/det_math/**`, `src/net/**`, `config/rails/**`, wire bytes, or tuning VALUES touched ⇒ ALL
> 11 GOLDENS BYTE-IDENTICAL, no digest moved, no new ctest target ⇒ guardian.yml UNCHANGED. No seal.**
> **Gates: 15/15 receipt PASS (`receipt-ATM-Sphere_v1.19r0-223ebd5.yml`), ctest 17/17 GCC + 17/17
> Clang, 166 property tests (unchanged), all 4 gen-header `--check`s in sync, replay selfcheck over
> the fresh 6-ship recording echoes `types: … #4=Yak-3 #5=La-7` + the Yak-3's 4-round TAIL kill
> (kills=1, victim lhb=4, tail=0.00 — per-airframe hp/damage provably flow from the new entries),
> fly selfcheck green, 10 s GUI smoke of BOTH modes clean.**
> **GIT: pushed to `origin/main` (envtab code `223ebd5` + receipt `0232a17` — guardian CI run
> [28613165952](https://github.com/cjcgervais/seads/actions/runs/28613165952) GREEN; ammo rider
> `255eafb` + receipt `receipt-ATM-Sphere_v1.19r0-255eafb.yml` 15/15 PASS — verify its guardian
> run GREEN)** — Python gates + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash
> aggregation gate all reproduce all 11 goldens bit-for-bit (data/presentation-only changes move
> no golden; the client is off the gate).
> **NEXT (free pick, none blocking):** **B5** ISA atmosphere (a seal); an open-ended live frame
> SOURCE feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelope
> scalars + a kernel consumer — its own ADR, would move goldens); or a sealed SCENARIO exercising
> Yak-3/La-7 (they fly in demos now, but no golden covers their envelope interpolation in C++ ↔
> Python lockstep).
> **NOTE FOR THE NEXT AGENT:** `gen_envelope_tables.py` now emits ALL tuning JSONs — dropping a
> new JSON into `data/tuning/envelopes/` auto-adds an envtab entry on regen (keep tuning_probe
> green over it). If a future scenario references a newly-emitted envelope, lockstep/predict
> vectors will move — that is the scenario change, not this pattern. The recorder now seeds BOTH
> `hp_start` and `ammo_start` from the envelope (rider (d)) — keep `seed()` aligned with
> `scenario_main.cpp` if `Kernel::add` ever grows another per-airframe arg. Practical gotcha:
> `seads_viewer --selfcheck` REQUIRES a count argument (`--selfcheck 8`); without it the viewer
> silently enters GUI mode and hangs a headless run.
>
> ## ►► PRIOR STATE (2026-07-02): **PER-AIRFRAME MESH VARIANTS — ROSTER SILHOUETTES IN THE VIEWER DONE ✅** (no-seal, rides **ATM-Sphere v1.19r0**)
> **Latest: the one-size fighter becomes eight — each aircraft now draws its ROSTER TYPE's silhouette (radial vs inline nose, wing plan, tail, size), fed by a new `.seadsrec` v3 per-aircraft type trailer. The named blocker ("the wire carries no type field") is resolved the honest way: the type is STATIC per flight, so it rides the recording META, not the sealed 20 Hz wire — no reseal, nothing kernel-side.**
> Five pieces, ALL `src/client` + web (downstream-only presentation):
> **(a) `aircraft_mesh.{h,cpp}` — the builder is PARAMETERIZED** (`Proportions`, internal): the same
> loft/slab assembly driven by per-type dimensions — fuselage radii + nose stations (radials P-47D/
> A6M2/La-7 get a blunt wide cowl + short nose; inlines Bf 109/Ki-61/Yak-3/Spitfire/P-51 a slender
> pointed one), a TWO-PANEL wing plan (root→mid→tip chord factors + sweep ⇒ straight taper, squared
> Bf 109/P-51 tips, the A6M2's long rounded taper, the Spitfire ELLIPSE), stab span/fin height,
> canopy footprint (P-47 razorback / P-51 bubble / A6M2 greenhouse), blade reach, a P-51 ventral
> scoop slab, and overall scale (P-47D 1.14 … Yak-3 0.86). New public `enum AircraftType` with
> **STABLE presentation codes 0–7 in sealed roster order** (+ GENERIC=255 fallback = the original
> model), `aircraft_type_name`, `aircraft_type_from_code`. Still headless — pure float arrays, no
> raylib types.
> **(b) `.seadsrec` v3** (`seadsrec.{h,cpp}`, `SEADSREC_VERSION 2→3`): a second append-only trailer
> after the event journal — `u32 n_types` then one u32 AircraftType code per aircraft slot. Same
> back-compat pattern as v2: a v1/v2 file loads with an EMPTY type list (every aircraft → generic
> mesh), a truncated trailer is rejected; old 3/4-arg `write_recording` overloads delegate with
> empty types. `Playback` exposes `types()` + `type_code_of(id)` (out-of-range → generic).
> **(c) The recorder emits it** (`record_main.cpp`): `Envelope*` → code by pointer identity against
> the generated envtab (P-47D/Bf 109/Ki-61/A6M2/Spitfire/P-51 — the 6 roster types with generated
> envelopes; Yak-3/La-7 have tuning JSONs but no envtab entry yet, so they can't appear in a
> recording), written into the v3 trailer + a `"types"` display-name array in the trajectory.js
> meta + the console summary ("aircraft 1 (A6M2): hp 0 *** KILLED ***").
> **(d) The viewer draws per-type variants** (`viewer_main.cpp`): `FighterModel::init(type)` +
> a `FighterModelSet` (9 uploads post-InitWindow, indexed by code); BOTH replay and fly pick each
> aircraft's variant via `Playback::type_code_of`; the fly OWN ship is the Ki-61 variant (matches
> `kFlyEnv`). HUD rows + the scoreboard + `--selfcheck` now carry airframe names ("#0 P-47D",
> "types: #0=P-47D #1=A6M2 …" — headless proof); a pre-v3 recording renders EXACTLY as before
> (generic mesh, no names). The web viewer HUD rows show `meta.types` names, guarded for old files.
> **(e) Headless gates extended** (`seads_client_test`): the full structural suite (winding==stored
> normal, unit normals, no degenerates, z-mirror symmetry, shade band) now runs over ALL 8 roster
> variants + generic with the layout claims made RELATIVE (engine = forward-most vertex, tail =
> aft-most, wings out-span every part — absolute thresholds don't survive per-type scale); plus
> pairwise variant DISTINCTNESS (every pair differs in wing or engine vertex data), signature
> proportions (P-47D out-sizes the Yak-3, A6M2 out-spans it), and the v3 trailer tests (round-trip,
> Playback exposure + fallbacks, journal-only recordings keep an empty type list, truncation
> rejected, version stamped v3).
> **PRESENTATION-ONLY: no `src/kernel/**`, `src/det_math/**`, `src/net/**`, `config/rails/**`,
> `data/tuning/**`, or wire bytes touched ⇒ ALL 11 GOLDENS BYTE-IDENTICAL, no digest moved, no new
> ctest target ⇒ guardian.yml UNCHANGED. No seal.** (The `.seadsrec` container is a local
> presentation format, not a sealed wire — its version bump is not a reseal.)
> **Gates: ctest 17/17 GCC + 17/17 Clang, replay + fly selfchecks green over a fresh `--dogfight`
> recording (types echoed headless), 12 s GUI smoke of BOTH modes clean, fly own-ship
> SCREENSHOT-VERIFIED (the Ki-61 variant's long slender inline nose reads on screen).**
> **GIT: pushed to `origin/main` (code `210e9b6` + receipt `51fc737`,
> `receipt-ATM-Sphere_v1.19r0-210e9b6.yml` — 15/15 gates PASS); guardian CI run
> [28610490818](https://github.com/cjcgervais/seads/actions/runs/28610490818) GREEN** — Python
> gates + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce
> all 11 goldens bit-for-bit (a presentation-only change moves no golden; the client is off the gate).
> **NEXT (free pick, none blocking):** **B5** ISA atmosphere (a seal); an open-ended live frame
> SOURCE feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelope
> scalars + a kernel consumer — its own ADR, would move goldens); or generate the missing Yak-3/La-7
> envtab entries (data/tooling — the generated envelope table covers 6 of the 8 roster tuning JSONs)
> so those two variants can actually fly in a recording.
> **NOTE FOR THE NEXT AGENT:** the AircraftType codes are STABLE presentation constants (written
> into every v3 recording) — never renumber them; add new codes only. The type trailer is META, not
> wire: keep it off the sealed snapshot (a per-tick type field would be a reseal for a static fact).
> `Proportions` is deliberately internal to aircraft_mesh.cpp — tests gate the OUTPUT (structure +
> distinctness), so silhouettes can be retuned freely without an API break. The GENERIC model keeps
> the original silhouette but now uses the two-panel wing (slightly different tri count than the old
> single-slab wing); nothing pins tri counts.
>
> ## ►► PRIOR STATE (2026-07-02): **AIRCRAFT MESHES — PROCEDURAL LOW-POLY FIGHTER, REGION-DAMAGE TINTED, IN THE VIEWER DONE ✅** (no-seal, rides **ATM-Sphere v1.19r0**)
> **Latest: the last renderer cosmetic lands — the sphere+lines marker is now a real low-poly WWII prop fighter, and the airframe itself shows the v1.18r0 damage state: a knocked-out region's part tints dark straight from the decoded WEAPON-001 pools, and the prop stops on a dead engine.**
> Four pieces, ALL `src/client` (downstream-only presentation):
> **(a) `aircraft_mesh.{h,cpp}` — a pure vertex-data builder** (new files, in the headless
> `seads_client` lib; NO raylib types, NO GPU, NO asset files): a generic WWII fighter from two
> primitives — octagonal-ring LOFTS (tapered fuselage + cowl + spinner/tail fans) and convex
> 8-corner SLABS (wings, stabilizers, fin, canopy, two crossed prop blades). Face winding is
> DERIVED, not hand-authored (every face wound to agree with an outward hint: radial for lofts,
> centroid-out for slabs), and a fixed body-frame key light is BAKED into per-vertex shade so the
> low-poly form reads without a lighting shader. Body frame +X nose / +Y canopy / +Z starboard.
> **(b) The model is split into the wire's REGION parts** — ENGINE / WING / TAIL (v1.18r0 order)
> + the BODY hull — so `draw_aircraft` tints a knocked-out region's part dark (`REGION_OUT_C`)
> straight from the decoded pools (same all-zero-baseline back-compat guard as the bars), and the
> ENGINE part alone spins about the nose axis (the cowl is a solid of revolution ⇒ only the blades
> visibly turn) — **frozen when the engine pool is out or the plane is dead** (the "engine out →
> thrust 0" seal state is visible on the airframe). Own fly ship: no wire weapons ⇒ plain colour,
> prop always spinning.
> **(c) `FighterModel` uploads the four parts post-InitWindow** (`viewer_main.cpp`): baked shade
> rides the vertex colours (default shader multiplies them by the material tint); the transform is
> a rigid re-orthogonalized frame (X = pitched nose, Z = rolled wingspan, Y = their cross — the
> old sheared b_up is NOT used). BOTH the replay GUI (remotes @1.2 scale) and fly (remotes @1.0,
> own @1.6) draw it; the original line marker survives as the `!ready` headless/pre-init fallback.
> **(d) Headless structural gates** (`seads_client_test` gains `test_aircraft_mesh`): array
> shapes, stored-normal == winding-recomputed normal per triangle, unit normals, no degenerate
> tris, bilateral z-mirror symmetry (set-based), region-part LAYOUT (engine reaches the nose +
> stays forward, tail reaches the tip + stays aft, nothing out-spans the wings), shade band.
> **PRESENTATION-ONLY: no `src/kernel/**`, `src/det_math/**`, `src/net/**`, `config/rails/**`,
> `data/tuning/**`, or wire bytes touched ⇒ ALL 11 GOLDENS BYTE-IDENTICAL, no digest moved, no new
> ctest target ⇒ guardian.yml UNCHANGED. No seal.**
> **Gates: 15/15 receipt PASS (`receipt-ATM-Sphere_v1.19r0-7b02417.yml`), determinism lint PASS,
> ctest 17/17 GCC + 17/17 Clang, property tests 166 (unchanged — no reference/wire change), replay
> + fly selfchecks green over the `--dogfight` recording, 8 s GUI smoke of BOTH modes clean, and
> the fly own-ship SCREENSHOT-VERIFIED (readable fighter: tapered wings, tailplane + fin, canopy,
> spinner + blades, banking with input).**
> **GIT: pushed to `origin/main` (code `7b02417` + receipt `2797b5c`); guardian CI run
> [28608248338](https://github.com/cjcgervais/seads/actions/runs/28608248338) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce all 11
> goldens bit-for-bit (a presentation-only change moves no golden; the client is off the gate).
> **NEXT (free pick, none blocking):** **B5** ISA atmosphere (a seal); an open-ended live frame
> SOURCE feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelope
> scalars + a kernel consumer — its own ADR, would move goldens); or per-airframe mesh VARIANTS
> (needs an aircraft-type field on the wire or in the .seadsrec meta — the wire carries none today,
> which is why this model is deliberately generic).
> **NOTE FOR THE NEXT AGENT:** the mesh builder is deliberately in the HEADLESS lib (pure float
> arrays) — keep GPU types out of `aircraft_mesh.{h,cpp}` so `test_aircraft_mesh` stays runnable
> everywhere. The engine/wing/tail part split is index-aligned with the wire region order 0/1/2 —
> keep that alignment if you add parts. `FighterModel::init()` must run after `InitWindow` (GL
> context); headless paths never touch it. Prop spin reads the wall clock (presentation-only, like
> the feed fades) and deliberately keeps spinning while paused.
>
> ## ►► PRIOR STATE (2026-07-02): **EVENT-JOURNAL KILL-FEED — PER-ROUND COMBAT EVENTS AT 100 Hz IN THE VIEWER DONE ✅** (no-seal, rides **ATM-Sphere v1.19r0**)
> **Latest: the viewer's kill-feed is now driven by the layer-6 per-round hit journal at the full 100 Hz physics rate — exact-tick attributed kills + per-round floating damage numbers — instead of inferring kills from 20 Hz wire-state transitions.**
> The follow-up the last renderer handoff named ("wire the layer-6 event.h journal into the viewer
> for per-round granularity") lands. The kernel's per-round hit queue (`Kernel::hit_events()` — one
> `HitEvent` per connecting round, observable output, never hashed) is the SAME source the reliable
> event channel consumes; the recorder, which already drives the sealed kernel at 100 Hz, now
> captures it every tick. Four pieces, ALL `src/client` (downstream-only presentation):
> **(a) The `.seadsrec` container carries the journal** (`seadsrec.{h,cpp}`, `SEADSREC_VERSION 1→2`):
> a new trailing section after the frames — `u32 n_events` then per event
> `{tick, target, attacker, damage_milli, hp_after_milli, killed, region}` (each i64 LE). Quantized
> to milli-hp EXACTLY as `src/net/event.cpp` (`damage_milli` = post-clamp effective loss). A v2
> reader parses it; **a v1 file still loads with an empty journal** (the 3-arg `write_recording`
> delegates to the new 4-arg form with an empty list); a truncated trailer is rejected.
> **(b) The recorder captures it** (`record_main.cpp`): in the 100 Hz step loop, after `k.step`, it
> appends one `RecEvent` per `k.hit_events()` record at the exact physics tick. The dogfight demo
> yields **14 exact per-round events** (two 6/8-round bursts, each ending in a KILL; every round on
> its own tick 194/197/200/… — the granularity the 20 Hz path smears).
> **(c) `Playback` exposes `events()`** and **a new `CombatFeed`** replays them cursor-based (NO
> downward-crossing wrap heuristic — the cursor repositions cleanly on a replay loop): **floating
> per-round damage numbers** pinned to the struck aircraft's screen position, **region-coloured**
> (ENGINE red / WING orange / TAIL gold), plus **exact-tick attributed kill lines** ("#0 downed #1
> (TAIL)"). Two shooters landing on the SAME tick render as two distinct numbers (the transition
> path lumps them). Used in BOTH `run_gui` and `run_fly`; the v1.19r0 transition `KillFeed` stays as
> the fallback for a journal-less recording. `--selfcheck` echoes the whole journal (headless proof).
> **(d)** The damage-state BARS (wire region pools) still show current damage state; the journal
> feed shows the per-round HITS as they land — a coherent split (state vs. events).
> **PRESENTATION-ONLY: the hit queue is observable kernel output, never hashed; no `src/kernel/**`,
> `src/det_math/**`, `src/net/**`, `config/rails/**`, `data/tuning/**`, or wire bytes touched ⇒ ALL
> 11 GOLDENS BYTE-IDENTICAL, no digest moved, no new ctest target ⇒ guardian.yml UNCHANGED. No seal.**
> **Gates: 15/15 receipt PASS (`receipt-ATM-Sphere_v1.19r0-f68c09d.yml`), determinism lint PASS,
> ctest 17/17 GCC + 17/17 Clang (`seads_client_test` gains `test_event_journal` — v2 round-trip
> incl. signed `hp_after` + distinct same-tick attackers, v1 back-compat, truncation reject),
> property tests 166 (unchanged — no reference/wire change), selfcheck over the fresh `--dogfight`
> recording prints all 14 events with exact ticks/regions/kills, 8 s GUI smoke of BOTH modes clean.**
> **GIT: pushed to `origin/main` (code `f68c09d` + receipt `463177e`); guardian CI run
> [28606846227](https://github.com/cjcgervais/seads/actions/runs/28606846227) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce all 11
> goldens bit-for-bit (a presentation-only change moves no golden; the client is off the gate).
> **NEXT (free pick, none blocking):** **B5** ISA atmosphere (a seal); an open-ended live frame
> SOURCE feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelope
> scalars + a kernel consumer — its own ADR, would move goldens); or aircraft meshes (the remaining
> renderer cosmetic).
> **NOTE FOR THE NEXT AGENT:** the journal is captured at 100 Hz but the demo bursts happen to land
> one round per few ticks; a denser rof would show multiple numbers stacking (handled — `jitter()`
> spreads same-tick numbers deterministically, no RNG). The `.seadsrec` v2 trailer is append-only
> after the frames, so a v1 reader that stops at `n_frames` is unaffected and a v2 reader tolerates a
> v1 file. Keep the hit queue OFF any wire — it is a presentation capture from the recorder's own
> kernel, not a sealed transport (putting it on the snapshot wire would be a reseal for no gain).
>
> ## ►► PRIOR STATE (2026-07-02): **RENDERER POLISH — DAMAGE STATE + KILL-FEED + SCOREBOARD IN THE LIVE `--fly` PATH DONE ✅** (no-seal, rides **ATM-Sphere v1.19r0**)
> **Latest: the viewer finally SHOWS the fight the wire has carried since v1.19r0 — damage state, an attributed kill-feed, and a scoreboard, in both the live `--fly` path and the replay GUI, all drawn purely from decoded WEAPON-001 bytes.**
> The v1.19r0 handoff's named free pick lands. Four pieces, ALL `src/client` + web (downstream-only
> presentation — outside the determinism gate by construction):
> **(a) `Playback::sample_weapons` surfaces the FULL wire** (`src/client/playback.{h,cpp}`):
> `RenderHp` grows `ammo`, `last_hit_by` (i64, -1 = never hit), `engine_hp/wing_hp/tail_hp`, `kills`
> (i64) — every field was already DECODED (protocol 7) and then dropped at this exact seam; now it
> flows through. Integer-valued wire fields (`last_hit_by`/`kills`) are `llround`ed, not truncated.
> **(b) The `--fly` path draws the fight** (`viewer_main.cpp run_fly`): WEAPON-001 tracer rounds
> (previously NOT drawn at all in fly mode — the remotes' gunfire was invisible), dead remotes grey
> out (matching the replay GUI), per-remote screen-projected **hp bar + E/W/T region segments** (a
> knocked-out region fills dark red, its letter lights up; a pre-protocol-7 recording with all-zero
> baseline pools draws the hp bar only — back-compat), a top-right **kills/ammo SCOREBOARD**
> (kills-desc; KIA / WINCHESTER status tags), and a rolling **attributed KILL-FEED** ("#0 downed #1",
> "#2 knocked out #3's TAIL", 8 s fade). The fly HUD line gains a rounds-airborne count.
> **(c) The feed is TRANSITION-derived, loop-safe** (`KillFeed`, shared): total hp crossing >0→≤0 ⇒
> a kill (attribution = the victim's `last_hit_by`, by construction the killer); a region pool
> crossing >0→≤0 on a LIVING plane ⇒ a knock-out (region events are suppressed on the death frame
> itself; dead planes are pass-through in the kernel so pools can't move after death). ONLY downward
> crossings fire ⇒ the looping replay/fly timeline never emits false events on the wrap; `R` resets.
> The replay GUI (`run_gui`) shares all three helpers (`draw_damage_bars` / `draw_scoreboard` /
> `KillFeed`), its text HUD rows gain `rgn EWT` / ammo / kills + "KILLED by #N" attribution, and
> `--selfcheck` prints every field (headless proof of the whole data path).
> **(d) The web viewer HUD reads what trajectory.js already emits** (`web/viewer.js`): the per-frame
> `ammo`/`kills` arrays (`seads_record` has emitted them since v1.14r0/v1.19r0) now render as ammo +
> kills per craft row, guarded for old recordings.
> **PRESENTATION-ONLY: no `src/kernel/**`, `src/det_math/**`, `src/net/**`, `config/rails/**`,
> `data/tuning/**`, or wire bytes touched ⇒ ALL 11 GOLDENS BYTE-IDENTICAL, no digest moved, no new
> ctest target ⇒ guardian.yml UNCHANGED. No seal.**
> **Gates: 15/15 receipt PASS (`receipt-ATM-Sphere_v1.19r0-52a38a4.yml`), determinism lint PASS,
> ctest 17/17 GCC + 17/17 Clang (`seads_client_test` gains 8 checks — the full v1.19r0 field set
> flows through `sample_weapons`: ammo/attribution/region-pools/kills at full + after a tail-kill),
> property tests 166 (unchanged — no reference/wire change), replay selfcheck over a fresh
> `--dogfight` recording shows the whole arc (ammo 500→406, victims tail=0.00 lhb=0/2, killers
> kills=1), fly selfcheck green, 8 s GUI smoke runs of BOTH modes clean.**
> **GIT: pushed to `origin/main` (code `52a38a4` + receipt `df1eecb`); guardian CI run
> [28605668208](https://github.com/cjcgervais/seads/actions/runs/28605668208) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce all 11
> goldens bit-for-bit (a presentation-only change moves no golden; the client is off the gate).
> **NEXT (free pick, none blocking):** **B5** ISA atmosphere (a seal); an open-ended live frame
> SOURCE feeding `broadcast_async` incrementally; per-airframe region toughness (data-only envelope
> scalars + a kernel consumer — its own ADR, would move goldens); or further renderer cosmetics
> (aircraft meshes — the one renderer item deliberately left).
> **NOTE FOR THE NEXT AGENT:** `seads_viewer` builds only with `-DSEADS_CLIENT=ON` (the
> `build-client/` Ninja+gcc tree on this machine has raylib 5.5 fetched; `build-gcc`/`build-clang`
> cover the headless `seads_client_test`). The kill-feed is deliberately STATE-derived (wire-frame
> transitions), not event-channel-derived — `src/net/event.h` still has no client consumer; wiring
> the layer-6 journal into the viewer would give per-round granularity at 100 Hz instead of 20 Hz
> transitions and is a natural follow-up if wanted. Keep everything here downstream-only: nothing in
> `src/client`/web may feed bits back into the kernel or the wire.
>
> ## ►► PRIOR STATE (2026-07-02): seal **ATM-Sphere v1.19r0** — **REGION POOLS + KILL TALLY ON THE WEAPON-001 WIRE DONE ✅ — REGION-DAMAGE ARC CLOSED END-TO-END**
> **Latest (SEAL v1.19r0): the damage model now replicates — a remote client draws the DAMAGE STATE and a SCOREBOARD purely from bytes.**
> The follow-up v1.18r0 named lands: the region sub-pools **engine_hp/wing_hp/tail_hp** (12th–14th
> canonical f64s) + the victory tally **`kills`** (15th) join the WEAPON-001 snapshot section as
> the **12th–15th per-aircraft wire fields** (**snapshot protocol 6→7**). Scales follow the
> canonical precedents exactly: the pools ride **milli (1e3, like hp)** — every reachable value is
> a quarter-integer (integer hp_start × exact-binary 0.375/0.5/0.25 fractions, drained by integer
> damage), so milli is EXACT — and `kills` rides **unit scale (1e0, like ammo)** — a pure integer
> counter, exact + compact. New rail fields `wire.weapon.{enginehp,winghp,tailhp}_scale=1000` +
> `wire.weapon.kills_scale=1`; rails 280→290. **Fifth instance of the proven wire-reseal pattern**
> (KIN-001 v1.4r0 → WEAPON-001 v1.12r0 → ammo v1.14r0 → last_hit_by v1.17r0 → this): reference
> edited FIRST (`snapshot_ref.py` gates on `protocol>=7`; protocol-6 back-compat proven by
> self-test + property test), C++ (`snapshot.{h,cpp}`) mirrored bit-for-bit, **4 vector headers
> regenerated** (snapshot/weapon/session/framing — framing moves ONLY because its example payloads
> are whole default-protocol frames; the length-prefix envelope codec is byte-unchanged), the
> other 9 generated headers verified byte-identical (lockstep/predict untouched — the inverse
> fingerprint of a kernel seal, exactly as expected for transport-only).
> **TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, or `data/tuning/**` touched ⇒ ALL 11
> GOLDENS BYTE-IDENTICAL, no new golden, no new ctest target ⇒ guardian.yml UNCHANGED.**
> Digests that legitimately move (regenerated net-layer artifacts, not goldens): **session**
> `24f71845…c332` → `7e275f2b…49eb` (the client view + `FINAL_WEAPON` now surface the four fields
> — the astern-killed A6M2 reads **tail_milli = 0** with engine/wing intact, the P-47 reads
> **kills = 1**, bystanders keep full pools; the session self-test asserts all of it, i.e. the
> DAMAGE STATE + SCOREBOARD replicate over the lossy wire). **Event digest `06629a69…` DID NOT
> MOVE** (unlike v1.17r0 — the layer-6 `Event` record is untouched; `HitEvent.region` stays a
> kernel-side observable with no event-wire consumer yet). Riders: `seads_record` passes the four
> kernel accessors into the wire frames + emits a `"kills"` scoreboard array beside "hp"/"ammo".
> **Gates: 15/15 receipt PASS, property tests 164 → 166 (+2: `test_protocol6_omits_regions`
> back-compat; `test_region_damage_and_scoreboard_replicate_under_loss` — under ANY extra
> packet-loss pattern the client's freshest frame reconstructs AC1 tail-out + AC0 kills=1
> exactly), ctest 17/17 GCC + 17/17 Clang (all five socket bridges reconstruct the NEW sealed
> session digest over real 127.0.0.1 sockets), determinism lint PASS, all 13 generated headers in
> sync, all 11 goldens byte-identical (validate_snapshot + validate_scenarios PASS).**
> Ledger: **ADR-Step7-Guns-WireTransport-RegionDamage-v1.19r0**, SEAL_CARD v1.19r0 (wire row +
> history), CLAUDE.md header/rails/roadmap current.
> **GIT: pushed to `origin/main` (code `9e75d77` + receipt `5bc27f7`); guardian CI run
> [28582213041](https://github.com/cjcgervais/seads/actions/runs/28582213041) GREEN** — Python
> gates + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce
> **all 11 goldens bit-for-bit** on every leg AND run the protocol-7 parity legs
> (snapshot/weapon/session + the five socket bridges against the new session digest).
> **NEXT (free pick, none blocking):** renderer polish (damage state + kill-feed + scoreboard in
> the live `--fly` path — every field now rides the wire); **B5** ISA atmosphere (a seal); an
> open-ended live frame SOURCE feeding `broadcast_async` incrementally; or per-airframe region
> toughness (data-only envelope scalars + a kernel consumer — its own ADR, would move goldens).
> **NOTE FOR THE NEXT AGENT:** protocol lowering (…5/6) still produces valid, shorter frames —
> both ends share `SNAPSHOT_PROTOCOL`; keep any new per-aircraft field INSIDE the per-aircraft
> weapon loop and gate it on a protocol bump (a protocol-6 decoder reading extra varints would
> misalign the projectile count that follows). The framing vectors regenerate whenever the default
> snapshot protocol changes — that is expected, not a framing-codec change.
>
> ## ►► PRIOR STATE (2026-07-02): seal **ATM-Sphere v1.18r0** — **REGION DAMAGE + KILL TALLY DONE ✅**
> **Latest (SEAL v1.18r0): airframes now break by REGION, and the kernel keeps score.** The roadmap's
> long-deferred "component/region damage" seal lands, plus the victory tally attribution made possible.
> **(a) Region sub-pools** (`tools/ref_kernel.py` ↔ `src/kernel/kernel.{h,cpp}`, mirrored bit-for-bit):
> each airframe carries **ENGINE (0.375×hp_start), WING (0.5×), TAIL (0.25×)** sub-pools beside the
> total hp — global exact-binary fractions (every roster hp_start is an integer ⇒ every pool exact in
> f64), independent thresholds, NOT a partition (a connecting round's damage books into the total hp,
> unchanged op order, AND the struck region, clamped at 0; pass-through once dead). The region is pure
> **approach aspect**: `rel = wrap_pi(round.psi − target.psi)` — |rel| < π/4 == fired from astern →
> TAIL, |rel| > 3π/4 == head-on → ENGINE, else beam → WING (exactly π/4 lands in WING). **wrap_pi +
> compares only ⇒ ZERO new det_math** (the guns arc's streak holds through its SEVENTH seal).
> **(b) Region effects — a dead region degrades a LIVING plane** (hp≤0 death unchanged, dominates):
> **engine out → thrust forced 0** at any throttle (a decelerating glider); **wing out → n_aero
> HALVED** (half the lifting surface — binds below the corner speed exactly like the B3 accelerated
> stall); **tail out → control authority lost** (commanded bank/g overridden to **(0.0, 1.0)** — a
> straight 1-g mush; throttle untouched). The tail-out override was CHOSEN so a target already flying
> straight-and-level at 1 g is bit-identical through it — every sealed victim (Hit-001's and
> SESSION-SK-001's astern-shot A6M2s) is exactly that, which makes the reseal provably additive.
> **(c) Kill tally:** per-aircraft **`kills`** — +1 on the ATTACKER exactly on the round that crossed
> the target hp>0→≤0 (the HitEvent `killed` round ⇒ shared kills credit exactly the crossing round's
> owner, exact via the per-round queue); persists through the attacker's own death (posthumous kills
> count). GOLDEN-SK-Hit-001's p47d now reads **kills = 1** on a byte-identical trajectory.
> **State/snapshot:** engine_hp/wing_hp/tail_hp/kills = the **12th–15th per-aircraft snapshot f64s**
> ⇒ **ALL 10 PRIOR GOLDENS MOVE — PROVEN ADDITIVE** (pre-reseal dry strip-diff: removing the 4 new
> f64s from each v1.18r0 snapshot reproduces its v1.17r0 hash byte-for-byte, 10/10 PASS — Sphere now
> `6914a994…2b13eb20`). `HitEvent` gains **`region`** (0/1/2; observable output, still never hashed).
> **NEW GOLDEN-SK-EngineOut-001** (700 ticks): P-47D and A6M2 meet HEAD-ON; a 4-round burst connects
> at ticks 29/31/33/35, every round → ENGINE (|rel| = π), the engine pool drains 26.25→14.25→2.25→0
> (clamped, 3rd round) while total hp only reaches 22 ⇒ a LIVING aircraft with a dead engine that
> decelerates thrustless 150→~131 m/s in level 1-g flight — `e9617633…fc3ba078`, C++ ≡ Python
> bit-for-bit GCC+Clang. **OFF-WIRE this seal** (deferred exactly as ammo at G4 / last_hit_by at
> v1.16r0): only `lockstep_vectors.h` + `predict_vectors.h` regenerated (they hash `snapshot()`);
> snapshot/weapon/session/event/interp/geo001/framing vectors ALL byte-identical — the sealed session
> digest `24f71845…c332` and event digest `06629a69…` DID NOT MOVE. `scenario_params.h` regenerated
> (new scenario). Rails 270→280 (`weapons.region_damage` doctrine block).
> **Gates: 15/15 receipt PASS (`receipt-ATM-Sphere_v1.18r0-5a89ef4.yml`), property tests 153 → 164
> (+11 `tests/property/test_region_damage.py`: pool derivation / aspect-selects-region / double-booking
> + clamp / engine-out kills thrust / wing-out halves the ceiling / tail-out strips authority / the
> tail-out-noop STRIP property / kill-tally exactly-once / shared-kill credits the crossing shooter /
> snapshot fields 12–15 / determinism; `test_hit_queue.py` size gate → 15-f64 record), ctest 17/17
> GCC+Clang, all 11 goldens C++≡Python bit-for-bit both toolchains, generated headers in sync.**
> guardian.yml: GOLDEN-SK-EngineOut-001 joins the python-gates loop + the build-matrix `run_one`
> list + the cross-toolchain aggregation list (first golden-list change since v1.13r0).
> Ledger: **ADR-Step7-Guns-RegionDamage-v1.18r0**, SEAL_CARD v1.18r0 (goldens table rewritten — 11
> entries, all new hashes), CLAUDE.md header/roadmap current.
> **GIT: pushed to `origin/main` (code `5a89ef4` + receipt `3a9e76a`); guardian CI run
> [28576545155](https://github.com/cjcgervais/seads/actions/runs/28576545155) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce **all 11
> v1.18r0 goldens bit-for-bit** (10 moved + the new EngineOut) on every leg.
> **NEXT (free pick, none blocking):** put region pools + kills on the WEAPON-001 wire (a
> transport-only reseal, the proven ammo→v1.14r0 / last_hit_by→v1.17r0 pattern ⇒ a remote client
> draws damage state + a scoreboard; protocol 6→7, 4 new unit-or-milli-scale fields); renderer
> polish (guns + kill-feed + damage in the live `--fly` path); **B5** ISA atmosphere (a seal); or an
> open-ended live frame SOURCE feeding `broadcast_async` incrementally.
> **NOTE FOR THE NEXT AGENT:** the region fractions/cones are GLOBAL kernel hex-constants (like
> HIT_RADIUS) — per-airframe region toughness would be a data-only envelope follow-up. The v1.16r0
> GOTCHA held again: a new per-aircraft canonical f64 moves ONLY lockstep+predict vectors. Python
> callers that override `a.hp` after constructing an Aircraft MUST re-derive the three pools (see
> build_scenario / session_ref._build_server_kernel; C++ `add()` derives them from its hp arg).
> The tail-out (0,1g) override, wing-out 0.5 factor, and aspect cone edges are SEALED byte-spec now —
> changing any of them moves goldens ⇒ new seal.
>
> ## ►► PRIOR STATE (2026-07-02): **PER-ROUND HIT GRANULARITY — THE KERNEL HIT EVENT QUEUE DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: combat events are now exact to the ROUND — the kernel records every connecting round as its own attributed event, and the reliable event channel ships it.**
> The gap every handoff since G2 deferred closes: the layer-6 event channel DERIVED hit/kill events
> by observing per-tick hp deltas, which LUMPS two rounds landing on one tick into one event and
> reads attribution off the last-writer `last_hit_by` (the earlier shooter's credit lost, the
> crossing round unknowable). Three pieces:
> **(a) The kernel hit event queue** (`tools/ref_kernel.py` ↔ `src/kernel/kernel.{h,cpp}`,
> mirrored): `Kernel.hit_events` — one `HitEvent{target, attacker, damage, hp_before, hp_after,
> killed}` **per connecting round**, appended at hit time (projectile array order ⇒ deterministic),
> **cleared at the top of every step**. `attacker` comes straight off the striking round's `owner`;
> `hp_before/hp_after` are post-clamp (an overkill round's EFFECTIVE loss = hp_before − hp_after;
> `damage` keeps the as-fired value); `killed`=1 on exactly the round that crossed hp>0→≤0.
> **OBSERVABLE OUTPUT, NOT CANONICAL STATE:** never serialized into `snapshot()` ⇒ the world_hash
> can't see it ⇒ **ALL 10 GOLDENS BYTE-IDENTICAL** (Sphere `f2db95bd…` re-validated) ⇒ **no seal**.
> The hit-branch float ops are untouched; **zero new det_math** (the guns arc's streak holds).
> `last_hit_by` (canonical, on-wire) unchanged.
> **(b) The layer-6 event channel now sources the queue** (`tools/event_ref.py` ↔
> `src/net/event.{h,cpp}`) instead of re-deriving from hp deltas. The `Event` record/wire is
> UNCHANGED (same 7 integer fields, window K=4, journal dedup): `damage_milli` = quantized effective
> loss, so per-tick sums equal the old lumped deltas and — whenever no tick lands two rounds on one
> target (true of SESSION-SK-001) — the stream is bit-for-bit the old one: **the sealed
> EVENT_DIGEST `06629a69…` DID NOT MOVE** (`event_vectors.h` sealed digests untouched; session/
> lockstep/predict all byte-identical). What changed is what the channel CAN carry.
> **(c) The granularity vector EVENT-MULTIHIT-001** (cross-impl, in `gen_event_vectors.py` →
> `event_vectors.h` → `seads_event_test`): twin P-47Ds symmetric about the equator (lat ±0.2° ≈
> ±52 m — inside the target's 60 m hit sphere, outside each other's at ~105 m) fire a 3-volley burst
> (rof 3) at one A6M2 ⇒ by symmetry each volley's two rounds land the SAME tick ⇒ **6 events over 3
> ticks (44/47/50): every tick TWO events on one target from two DIFFERENT attackers**
> (unrepresentable pre-queue), and the kill volley shows the overkill clamp (A6M2 dies at 22 hp to
> 12+12: shooter 0's round 22→10, shooter 1's 10→0, effective 10, killed=1, attacker=1). C++ ==
> Python bit-for-bit (digest `8a071bb0…`, GCC+Clang), structural claims asserted directly.
> **Gates: 15/15 receipt PASS, property tests 145 → 153 (`tests/property/test_hit_queue.py` — queue
> lifecycle / multi-hit SPLIT vs last-writer / killed-marks-the-crossing-round + overkill clamp +
> corpse-unhittable / queue-not-hashed (snapshot size exact) / per-round REDUCES to hp-delta when
> single-hit / determinism), ctest 17/17 GCC+Clang (the event test gains the MULTIHIT leg — no new
> ctest target ⇒ guardian.yml UNCHANGED), all 14 generated headers --check in sync, 10 goldens
> byte-identical.** Ledger: **ADR-Step7-Guns-HitQueue-v1.17r0**. **Seal stays v1.17r0.**
> **GIT: pushed to `origin/main` (code `e21efda` + receipt `9e92c22`); guardian CI run
> [28573440275](https://github.com/cjcgervais/seads/actions/runs/28573440275) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 reproduce the 10 goldens bit-for-bit AND run the extended
> `seads_event_test` (the EVENT-MULTIHIT-001 leg) on every ctest leg.
> **NEXT (free pick, none blocking):** renderer polish (guns + kill-feed in the live `--fly` path —
> per-round impact sparks / split damage numbers now have exact data); component/region damage (a
> seal — the natural consumer of per-round events); **B5** ISA atmosphere (a seal); or an open-ended
> live frame SOURCE feeding `broadcast_async` incrementally.
> **NOTE FOR THE NEXT AGENT:** the queue is deliberately NOT on any wire section — the layer-6
> session message is the transport (the wire is a sealed rail; putting HitEvent fields on the
> snapshot wire would be a reseal for no consumer). The old hp-delta derivation survives as a gated
> INVARIANT (test_per_round_reduces_to_hp_delta_when_single_hit), not a mechanism — don't
> reintroduce it in event.cpp. `hit_events` must stay cleared-per-step + unserialized; if a future
> seal wants persistent event history, that's new canonical state (its own ADR + seal).
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 12 — SEND-BUFFER BYTE-CAP + DROP-SLOWEST DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: the async broadcast is now safe for an open-ended live stream — a client that can't keep up is SHED at a byte-cap instead of growing server memory without bound.**
> Layer 11's named honest boundary ("the per-client buffer is unbounded; add the byte-cap policy
> BEFORE pointing this at an open-ended live stream") closes. **`broadcast_async` gains an opt-in
> `cap_bytes` (final param, default 0 = unbounded = layer-11 behavior EXACTLY**, the same
> opt-in-default pattern `catchup` used): whenever a frame enqueue (opportunistic flush included)
> leaves a client's PENDING userspace backlog above the cap, that client is dropped —
> **drop-slowest** — counted in a new **`Stats.capped`** (and, being live, also as a leave, like a
> send failure). The policy is UNIFORM: a catch-up joiner tripping it during prefix replay is closed
> before ever becoming live (capped only, not a join/leave). The cap decides only **WHO is dropped,
> never WHICH bytes flow**: a survivor's delivered bytes are untouched, a shed client's are always a
> clean byte-PREFIX of the encoded stream (kernel-accepted prefix; the pending tail is discarded
> whole). `flush_client` now compacts the consumed prefix every flush ⇒ per-client memory is
> O(pending backlog) — the quantity the cap bounds — not O(bytes ever flushed). `broadcast_select`
> UNTOUCHED (layers 9/10 verbatim). Three pieces:
> **(a) The policy** (`src/net/broadcast.{h,cpp}`): `cap_bytes` param + `over_cap` check beside the
> enqueue + `Stats.capped`; drain phase needs no check (nothing enqueued there — backlogs only shrink).
> **(b) The byte-cap BRIDGE** (`seads_netcap_test`, ctest `netcap_bridge`): **LEG 1 (drop-slowest)**
> — ~7.5 MiB (512×15 KiB) to FAST + SLOW through cap=1 MiB + pinned 16-KiB SO_SNDBUF. FAST keeps up
> BY CONSTRUCTION: the `on_frame` hook itself drains FAST's client-side socket at the top of every
> frame iteration (server thread reads the CLIENT endpoint the test owns; `wait_readable(0)` poll)
> ⇒ each enqueue lands in an almost-empty kernel pipe, FAST's backlog stays ~one frame ≪ cap, on
> every OS, NO sleeps/parked threads; SLOW reads NOTHING. Server finishes every frame un-wedged,
> `joins=2 capped=1 leaves=1` (exactly SLOW shed); FAST byte-identical; SLOW a STRICT byte-prefix.
> (Which frame SLOW is shed at is OS-timing — deliberately unasserted. `seads_netasync_test` still
> gates the capless side: same shape, ~7 MiB buffered and delivered.)
> **LEG 2 (healthy path unperturbed)** — the layer-11 sealed-session shape (EARLY + CATCHUP at J=20)
> through `cap_bytes=8 MiB`: both receive all 41 frames byte-identically, both reconstruct the sealed
> SESSION-SK-001 digest `24f71845…c332`, **capped=0**. 12/12 (gcc) + 8/8 (clang) stress reruns clean.
> **(c) The demo server exposes it** (`seads_netserver [port] [num_clients] [catchup] [async]
> [cap_bytes]`, default 0): shares the same loop the bridge gates (no untested divergence).
> **TRANSPORT-ONLY — no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, framing envelope, wire
> scales, or goldens touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL**, no new golden, no seal. **Gates: +2
> property tests ⇒ 145 (`tests/property/test_broadcast.py`: `sendbuffer_deliver_capped` mirrors the
> enqueue→flush→over_cap order bit-for-bit — for ANY acceptance pattern and ANY cap a shed client's
> delivery is a byte-PREFIX of `encode_stream(frames)`, a survivor's == the whole stream == the
> layer-11 model, cap=0 == layer-11 bit-for-bit; healthy-client immunity — a kernel that always
> accepts everything is never capped at any cap), ctest 16→17 (`netcap_bridge`, x64 legs), 15/15
> receipt gates PASS, 10 goldens byte-identical.** guardian.yml: `seads_netcap_test`
> build-only-smoked on all 5 legs (default target) + `netcap_bridge` run on the native x64 legs
> (like the layer-7–11 bridges). Ledger: **ADR-Step-Net-Layer12-ByteCap-v1.17r0**. **Seal stays
> v1.17r0** (Tier-2 net layer). Layer-9/10/11 regression: `netdyn_bridge` + `netcatchup_bridge` +
> `netasync_bridge` unchanged + still green.
> **NEXT (free pick, none blocking):** per-round hit granularity (a kernel event QUEUE — its own
> ADR); renderer polish (guns + kill-feed in the live `--fly` path); an optional new seal
> (component/region damage; **B5** ISA atmosphere); or further live-stream rungs (an open-ended
> frame SOURCE feeding broadcast_async incrementally instead of a precomputed list).
> **GIT: pushed to `origin/main` (code `5740a8c` + receipt `8f8bc5c`; Linux fix `4da022e` + receipt
> `96b83f7`); guardian CI run
> [28571959667](https://github.com/cjcgervais/seads/actions/runs/28571959667) GREEN** — Python gates
> + MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce the 10
> goldens bit-for-bit AND run the new `netcap_bridge` on the native x64 legs (first round
> [28571414301](https://github.com/cjcgervais/seads/actions/runs/28571414301) failed exactly there
> on Linux — see the CI lesson below, fixed same session).
> **NOTE FOR THE NEXT AGENT:** layer 12 is green (GCC+Clang, ctest 17/17, 145 property tests, netcap
> bridge PASS + 12/12+8/8 stress, all 10 goldens byte-identical, guardian GREEN). The policy is deliberately BINARY
> (shed at cap) — frame-skipping / priority tiers would break the "delivered bytes are a prefix of
> the same stream" statement and is a different, lossier contract (its own rung). Every existing
> broadcast_async caller passes cap_bytes=0 ⇒ layer-11 path bit-for-bit; keep it that way for the
> sealed-session bridges. All bridge rendezvous stay cv+`notify_all` + `on_frame` (NO sleeps, no
> `std::future`). **CI lesson (first guardian round, fixed same session):** the first leg-1 cut
> PARKED the server in `on_frame` until a free-running FAST thread caught up; its deadlock margin
> assumed a kernel-full send buffer holds ≥ SO_SNDBUF bytes of PAYLOAD — true on Windows, FALSE on
> Linux (SO_SNDBUF accounts sk_buff TRUESIZE = payload+overhead) ⇒ the GCC/Clang x64 legs wedged.
> Never park the producer thread on a consumer it is the sole flusher for — the fix has the hook
> DRAIN FAST's socket itself. Also: bridge watchdogs must `fflush(stdout)` before `std::_Exit`
> (skips stdio flush ⇒ the first CI failure's diagnostic was lost to a full pipe buffer) — now done
> in netcap + netasync.
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 11 — ASYNC SINGLE-THREAD OUTPUT (PER-CLIENT SEND BUFFERS) DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: no client can back-pressure the broadcast — the last blocking send is gone from the fan-out.**
> Layers 9/10 streamed with blocking `send_all`: one stalled reader's full kernel buffers wedged the
> WHOLE broadcast (and the layer-10 catch-up replay was a blocking burst — its named honest boundary).
> Layer 11 closes it with **async output**: every accepted client is non-blocking and owns a **userspace
> send buffer**; frames (and a joiner's catch-up prefix) are **ENQUEUED**, the kernel takes what it can
> now (**`send_some`** — new non-blocking primitive, 0 == EWOULDBLOCK), and the remainder flushes when
> the SAME single select that services JOIN/LEAVE reports the client **writable** (**`select_rw`** — the
> layer-9 `select_readable` generalized with a write set). Delivered BYTES are unchanged — same frames,
> same order, same envelope — so every layer-5–10 byte-stream statement holds verbatim. Three pieces:
> **(a) Socket primitives + the async loop** (`src/net/socket.{h,cpp}`, `src/net/broadcast.{h,cpp}`):
> `netbcast::broadcast_async(...)` — same signature/contract as `broadcast_select` (incl. `on_frame` +
> `catchup`), which is **UNTOUCHED** (layers 9/10 verbatim; their bridges keep gating it). Fast path:
> `enqueue_bytes` flushes opportunistically, so an unclogged client never accumulates a buffer. The
> catch-up prefix is enqueued, not burst. After the last frame, a bounded DRAIN phase (progress-bound +
> ~30 s CONSECUTIVE-idle cap — any readiness resets it, so a slowly-reading client is never dropped)
> flushes stragglers; still-pending at deadline ⇒ dropped as a leave (fail-not-wedge).
> Also new: `set_sndbuf` (bridge instrumentation; listener-inherited). Deliberately NO `set_rcvbuf`
> twin: shrinking SO_RCVBUF post-handshake is a Linux TCP pathology (drops + RTO backoff) — the first
> CI round proved it by wedging leg 1's drain on the GCC/Clang x64 legs (fixed same session).
> **(b) The async-output BRIDGE** (`seads_netasync_test`): **LEG 1 (no back-pressure)** — a synthetic
> **~8 MiB** stream (512×16 KiB) to a SLOW client that reads NOTHING while the server runs, through
> a pinned 16-KiB kernel SEND buffer (a blocking server provably wedges there; the watchdog would FAIL): the
> async frame loop reached the LAST frame with SLOW at **0 bytes read** (on_frame-observed, no sleeps),
> then SLOW drained to EOF **byte-identical**, zero leaves. **LEG 2 (sealed-session fidelity)** — the
> exact layer-10 shape through `broadcast_async(catchup=true)`: EARLY + CATCHUP (joined at frame J=20,
> prefix ENQUEUED) both receive the whole 41 frames byte-identically and reconstruct the SAME sealed
> SESSION-SK-001 digest `24f71845…c332` (GCC+Clang). 12/12 (gcc) + 8/8 (clang) stress reruns clean.
> **(c) The demo server exposes it** (`seads_netserver [port] [num_clients] [catchup] [async]`, async
> default 0): shares the same loops the bridges gate (no untested divergence).
> **TRANSPORT-ONLY — no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, framing envelope, wire
> scales, or goldens touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL**, no new golden, no seal. **Gates: +2
> property tests ⇒ 143 (`tests/property/test_broadcast.py`: the pure send-buffer model
> `sendbuffer_deliver` — for ANY kernel-acceptance pattern incl. all-EWOULDBLOCK and all-greedy the
> delivered bytes == `encode_stream(frames)` exactly; composition with the layer-7 codec — enqueue →
> partial flushes → re-chunking → reassemble == identity), ctest 15→16 (`netasync_bridge`, x64 legs),
> 15/15 receipt gates PASS, 10 goldens byte-identical.** guardian.yml: `seads_netasync_test`
> build-only-smoked on all 5 legs (default target) + `netasync_bridge` run on the native x64 legs (like
> the layer-7/8/9/10 bridges). Ledger: **ADR-Step-Net-Layer11-AsyncOutput-v1.17r0**. **Seal stays
> v1.17r0** (Tier-2 net layer). Layer-9/10 regression: `netdyn_bridge` + `netcatchup_bridge` unchanged
> + still green.
> **NEXT (free pick, none blocking):** a send-buffer byte-cap + drop-slowest policy (live-stream
> hygiene — the honest boundary layer 11 leaves); per-round hit granularity (a kernel event QUEUE — its
> own ADR); renderer polish (guns + kill-feed in the live `--fly` path); or an optional new seal
> (component/region damage; **B5** ISA atmosphere).
> **NOTE FOR THE NEXT AGENT:** layer 11 is code-complete + green locally (GCC+Clang, ctest 16/16, 143
> property tests, async bridge PASS + 12/12+8/8 stress, all 10 goldens byte-identical). Verify guardian
> CI green after push. `broadcast_select` is deliberately UNTOUCHED — keep the two loops separate (the
> old bridges gate the old loop). `std::future` stays avoided (cv+`notify_all`); all bridge rendezvous
> use `on_frame` so there are NO sleeps — keep it that way. The per-client buffer is unbounded by
> design here (stream is precomputed/finite); add the byte-cap policy BEFORE pointing this at an
> open-ended live stream.
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 10 — LATE-JOIN CATCH-UP (PREFIX REPLAY) DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: a client that joins mid-stream now reconstructs the WHOLE fight — the layer-9 honest-scope gap is closed.**
> Layer 9 proved a late joiner receives EXACTLY the contiguous frame suffix `frames[K:]` from its join
> point — but it deliberately couldn't reconstruct the full session (it missed the ticks before K).
> Layer 10 closes that gap with **catch-up**: when the single-thread `select()` broadcast server accepts
> a joiner mid-stream at frame K (with `catchup=true`), it first **REPLAYS the missed prefix `frames[0:K]`**
> (each shipped as one atomically length-prefixed frame, the same envelope the live stream uses) and only
> then feeds it the live suffix `frames[K:]` — so the joiner receives the WHOLE stream `frames[0:]`,
> **byte-identical to a client present from frame 0**, and reconstructs the SAME sealed SESSION-SK-001
> digest `24f71845…c332`. Three pieces:
> **(a) Opt-in catch-up on the broadcast server** (`src/net/broadcast.{h,cpp}`): `broadcast_select(...)`
> gains a final `bool catchup=false` (default preserves layer-9 behavior EXACTLY). `accept_pending` now
> takes the payload list + `upto` (frames already sent) + the flag; a mid-stream joiner is first replayed
> `frames[0:upto]` via a new `catch_up_client()` helper, then enters the live set. Initial-gather clients
> (upto=0) are never replayed (present from frame 0). A joiner that dies during replay is closed and NOT
> counted as a join. The replay is a synchronous burst on the accepting `select` iteration (a slow joiner
> back-pressures that iteration — bounded by the prefix length; async send buffers are the next rung).
> **(b) The catch-up determinism BRIDGE** (`seads_netcatchup_test`): ref = the sealed in-process
> `run_session(...).digest`. Over 41 real 127.0.0.1 frames, **EARLY** (from frame 0) reconstructs
> `24f71845…c332` (GCC+Clang; catch-up mode doesn't perturb it), and **CATCHUP** (rendezvoused to frame
> J=20 via the `on_frame` hook, no sleeps) is replayed `frames[0:20]` then streamed `frames[20:]`, receives
> the WHOLE `frames[0:]` byte-identical to EARLY, and reconstructs the SAME digest. Server `joins=2`,
> every frame sent. Finite watchdog fails-not-wedges; 12/12 (gcc) + 8/8 (clang) stress reruns clean.
> **(c) The demo server exposes it** (`seads_netserver [port] [num_clients] [catchup]`, catchup default 0):
> shares the same loop (no untested divergence).
> **TRANSPORT-ONLY — no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, framing envelope, wire
> scales, or goldens touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL** (Sphere re-validated `f2db95bd…`), no
> new golden, no seal. **Gates: +2 property tests ⇒ 141 (`tests/property/test_broadcast.py`: the pure
> catch-up model — a joiner at K receives `[0,K)`++`[K,leave)` == `range(0,leave)`, identical to a
> from-frame-0 client; composition with the layer-7 framing codec — `encode_stream(frames[0:K]) ++
> encode_stream(frames[K:]) == encode_stream(frames[0:])`), ctest 14→15 (`netcatchup_bridge`, x64 legs),
> 15/15 receipt gates PASS, 10 goldens byte-identical.** guardian.yml: `seads_netcatchup_test`
> build-only-smoked on all 5 legs (default target) + `netcatchup_bridge` run on the native x64 legs (like
> the layer-7/8/9 bridges). Ledger: **ADR-Step-Net-Layer10-CatchUp-v1.17r0**. **Seal stays v1.17r0**
> (Tier-2 net layer). Layer-9 regression: `netdyn_bridge` unchanged + still green (4/4 stress).
> **NEXT (free pick, none blocking):** async single-thread OUTPUT (writability `select` + per-client send
> buffers, so a slow client can't back-pressure the broadcast — the honest boundary layer 10 leaves);
> per-round hit granularity (a kernel event QUEUE — its own ADR); renderer polish (guns + kill-feed in the
> live `--fly` path); or an optional new seal (component/region damage; **B5** ISA atmosphere).
> **NOTE FOR THE NEXT AGENT:** layer 10 is code-complete + green locally (GCC+Clang, ctest 15/15, 141
> property tests, catch-up bridge PASS + 12/12 stress, all 10 goldens byte-identical). Verify guardian CI
> green after push (this session couldn't — `gh` CLI absent locally). Catch-up is a LITERAL replay of the
> frames the server already holds (`build_server_frames`) — no interpolation / state fast-forward; keep it
> that way. `std::future` stays avoided (cv+`notify_all` handshake); the rendezvous uses `on_frame` so
> there are NO sleeps — keep it that way if you extend the bridge.
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 9 — SINGLE-THREAD select() BROADCAST + DYNAMIC JOIN/LEAVE DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: the fan-out is now a genuine single-threaded `select()` event loop with live membership churn — and every membership case is byte-exact.**
> Layer 8 broadcast to N clients that ALL connected before streaming, one blocking `send_all` per
> already-connected client. Layer 9 makes it a real async fan-out: **one thread, one `select()` over
> `{listener} ∪ {clients}`, dynamic JOIN and LEAVE** — and proves the natural determinism statement for
> a live session with churn. Three pieces:
> **(a) `select_readable`** (`src/net/socket`): the layer-8 `wait_readable` generalized to many fds at
> once (Winsock ignores nfds / POSIX `max(fd)+1`), so one loop multiplexes a readable listener (a
> pending **JOIN**) and a readable receive-only client (`recv()==0` EOF ⇒ a **LEAVE**).
> **(b) The single-thread broadcast server** (`src/net/broadcast`, new `seads_broadcast` lib =
> `seads_framing`+`seads_socket`): `netbcast::broadcast_select(...)` waits for `min_initial` clients,
> then ships each payload as **one atomically length-prefixed frame** (joiners stay frame-aligned),
> running one `select` per frame to accept late joiners (they receive every frame **from their join
> point onward**) and drop leavers (EOF / send error) — **no thread-per-client**. An `on_frame(fi)` hook
> lets a test pin a mid-stream join to an EXACT frame with no sleeps. The demo `seads_netserver` now
> uses this same loop (shared code ⇒ no untested divergence; it now tolerates join/leave).
> **(c) The dynamic-membership BRIDGE** (`seads_netdyn_test`): reference = the sealed in-process
> `run_session(...).digest`. Over 41 real 127.0.0.1 frames, three clients with distinct lifetimes:
> **FULL** (present from frame 0) reconstructs the sealed digest `24f71845…c332` (the layer-8 result,
> now driven by the select loop; GCC **and** Clang); **LATE** (rendezvoused to frame J=20 via the hook)
> receives **exactly `frames[20:]`** — its join index computed from its first frame's decoded
> `server_tick`, asserted byte-exact; **LEAVER** (reads 10 frames then closes) receives a clean prefix
> and departs cleanly (server `joins=3 leaves=1`) **without disturbing FULL/LATE**. Finite watchdog
> fails-not-wedges; 12/12 stress reruns clean.
> **TRANSPORT-ONLY — no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, framing envelope, wire
> scales, or goldens touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL** (Sphere re-validated `f2db95bd…`), no
> new golden, no seal. **Gates: +6 property tests ⇒ 139 (`tests/property/test_broadcast.py`: the pure
> membership model — window-exactness / full=whole / late=suffix / leaver=prefix / leave-doesn't-disturb
> / composition with the layer-7 framing codec), ctest 13→14 (`netdyn_bridge`, x64 legs), 15/15 receipt
> gates PASS, 10 goldens byte-identical.** guardian.yml: `seads_netdyn_test` build-only-smoked on all 5
> legs (default target) + `netdyn_bridge` run on the native x64 legs (like the layer-7/8 bridges).
> Ledger: **ADR-Step-Net-Layer9-DynamicBroadcast-v1.17r0**. **Seal stays v1.17r0** (Tier-2 net layer).
> **NEXT (free pick, none blocking):** async single-thread OUTPUT (writability `select` + per-client
> send buffers, so a slow client can't back-pressure the broadcast); late-join CATCH-UP (replay the
> missed prefix to a joiner so it too reconstructs the whole fight); per-round hit granularity (a kernel
> event QUEUE — its own ADR); renderer polish (guns + kill-feed in the live `--fly` path); or an
> optional new seal (component/region damage; **B5** ISA atmosphere).
> **NOTE FOR THE NEXT AGENT:** layer 9 is code-complete + green locally (GCC+Clang, ctest 14/14, 139
> property tests, dynamic bridge PASS + 12/12 stress, 2-process demo PASS, all 10 goldens byte-identical).
> Verify guardian CI green after push (this session couldn't — `gh` CLI absent locally). `std::future`
> stays avoided (cv+`notify_all` handshake); the late-join rendezvous uses the `on_frame` hook so there
> are NO sleeps/timing guesses — keep it that way if you extend the bridge.
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 8 — MULTI-CLIENT FAN-OUT DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: the socket transport now fans one authoritative stream out to N clients — and every client is bit-exact.**
> Layer 7 proved a *single* client over a real 127.0.0.1 socket reconstructs SESSION-SK-001 to the
> sealed in-process digest. Layer 8 proves the fan-out case: **one server broadcasts the frame stream
> to N independent clients (each its own TCP connection + `StreamReassembler`), and ALL N reconstruct
> the byte-identical digest** — fan-out adds zero information and zero nondeterminism per client,
> regardless of connect order or how each client's `recv()` chunks the stream. Three pieces:
> **(a) Non-blocking primitives** (`src/net/socket`): `set_nonblocking` (`ioctlsocket FIONBIO` /
> `fcntl O_NONBLOCK`) + `wait_readable` (portable `select()` readability wait) so a fan-out server
> accepts clients without wedging on an absent one (`wait_readable` gates `accept_one`, bounded by a
> finite deadline ⇒ fail-not-wedge).
> **(b) The multi-client determinism BRIDGE** (`seads_multiclient_test`): reference = the sealed
> in-process `session::run_session(...).digest` (NOT re-derived). Server binds `:0`, publishes the port
> over a `mutex`+`cv` (`notify_all`; no `std::future` — MinGW `call_once` caveat), sets the listener
> non-blocking, accepts **N=3** clients, then **broadcasts** the identical lossless frame stream
> (`build_server_frames`, every frame incl. tick 0) to each. Each client reassembles its OWN stream,
> keys the frame list on decoded `server_tick`, and runs the SAME `run_client()` (reuses the layer-7
> `session.cpp` split — no reimplementation) ⇒ all 3 digests **==** `24f71845…c332` (GCC **and** Clang).
> Finite watchdog fails-not-wedges on any hang.
> **(c) The demo server gains fan-out** (`seads_netserver [port] [num_clients]`, default 1 =
> backward-compatible): with `>1` it accepts that many connections then broadcasts the identical stream.
> **TRANSPORT-ONLY — no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, framing envelope,
> wire scales, or goldens touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL** (Sphere re-validated `f2db95bd…`),
> no new golden, no seal. **Gates: +1 property test ⇒ 133 (`test_fanout_all_clients_identical`:
> N clients, N different chunkings, identical frame lists — the pure-codec form of the socket claim),
> ctest 12→13 (`multiclient_bridge`, x64 legs), 15/15 receipt gates PASS, 10 goldens byte-identical.**
> guardian.yml: `seads_multiclient_test` build-only-smoked on all 5 legs (default target) +
> `multiclient_bridge` run on the native x64 legs (like the layer-7 bridge). Ledger:
> **ADR-Step-Net-Layer8-MultiClient-v1.17r0**. **Seal stays v1.17r0** (Tier-2 net layer).
> **NEXT (free pick, none blocking):** async/`select`-based single-thread broadcast + dynamic
> join/leave over the layer-8 fan-out; per-round hit granularity (a kernel event QUEUE — its own ADR);
> renderer polish (meshes; guns + kill-feed in the live `--fly` path); or an optional new seal
> (component/region damage; **B5** ISA atmosphere).
> **NOTE FOR THE NEXT AGENT:** layer 8 is code-complete + green locally (GCC+Clang, ctest 13/13, 133
> property tests, multi-client bridge PASS, all 10 goldens byte-identical). Verify guardian CI green
> after push (this session couldn't — `gh` CLI absent locally). `std::future` stays avoided in the
> bridge (use the `condition_variable` + `notify_all` handshake if you extend it to more clients).
>
> ## ►► PRIOR STATE (2026-07-01): **NETCODE LAYER 7 — CROSS-PROCESS SOCKET TRANSPORT DONE ✅** (no-seal, rides **ATM-Sphere v1.17r0**)
> **Latest: the replication stack finally crosses a real OS process boundary — and it's bit-exact.**
> Layers 1–6 built the whole server↔client stack but shipped frames through an *in-process* transport
> (a dict / a `frame_at` lookup with integer lag + a drop set). Layer 7 moves them over a **real TCP
> socket** and proves, with a determinism bridge, that sockets + framing add **zero information and
> zero nondeterminism**. Three pieces:
> **(a) Framing** (`src/net/framing`, `tools/framing_ref.py`): a strictly-OUTER length prefix —
> `stream = concat of ( LEB128(len(payload)) || payload )`, payload = a whole protocol-6 snapshot. The
> length prefix reuses the **sealed** GEO-001 `leb128_encode_u64/decode_u64` (C++≡Python free). The
> `StreamReassembler` is a **PURE function of the byte stream**: any partition of a stream reassembles
> to the identical frames as feeding it whole. The one fragile spot — the length prefix itself can
> split across chunks — is handled by buffering a partial PREFIX (**truncated → wait**; **overlong >10
> bytes → error**, mirroring `leb128_decode_u64`'s bound bit-for-bit), never emitting a frame until all
> `len` bytes are present.
> **(b) Sockets** (`src/net/socket`): dependency-free blocking TCP, BSD/Winsock behind one
> `#ifdef _WIN32` (RAII `WsaGuard`, `socket_t`/`is_valid`, `send_all` loop over short writes,
> `recv_some`, `SO_REUSEADDR`, SIGPIPE-safe via `MSG_NOSIGNAL`). Endian-neutral (LEB128) — only
> `sin_port`/`sin_addr` use network order.
> **(c) The determinism BRIDGE** (`seads_netloop_test`): reference = the sealed **in-process**
> `session::run_session(rails, SESSION-SK-001, reconcile=true).digest` (NOT re-derived). `session.cpp`
> was split into `build_server_frames()` + `run_client()` (`run_session` = their composition) so the
> socket path reuses the EXACT reconstruction. The server sends **every** frame losslessly over a real
> `127.0.0.1` socket (incl. the tick-0 frame); the client reassembles, rebuilds the frame list **keyed
> on each frame's decoded `server_tick`**, and applies the sealed lag + drop set purely from
> `server_tick`/`t` (never wall-clock) ⇒ socket-path digest **==** in-process digest (`24f71845…c332`,
> GCC **and** Clang). OS-assigned port via `bind :0` + `getsockname`, handed over a `mutex`+`cv`
> handshake; a finite watchdog fails-not-wedges on a hang. Plus a two-process human demo
> (`seads_netserver`/`seads_netclient`).
> **TRANSPORT-ONLY — the outer envelope never touches the sealed protocol-6 bytes: no
> `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire scales, or goldens touched ⇒ ALL 10
> GOLDENS BYTE-IDENTICAL, no new golden, no seal.** `seads_session_test` digest is UNCHANGED (the
> `run_session` split is behavior-preserving). **Gates: +4 property tests ⇒ 132
> (`tests/property/test_framing.py`: reassembly round-trip / chunk-boundary invariance / partial-frame
> buffered / overlong reject), ctest 10→12 (`framing_byteexact` all legs + `netloop_bridge` x64), all
> generated headers in sync (`framing_vectors.h` added), 10 goldens byte-identical.** guardian.yml:
> `gen_framing_vectors.py --check` + `framing_ref.py` self-test + `seads_framing_test` on all 5 legs +
> build-only-smoke of the socket binaries on all legs + `seads_netloop_test` on the native x64 legs.
> Ledger: **ADR-Step-Net-Layer7-Socket-v1.17r0**. **Seal stays v1.17r0** (this is a Tier-2 net layer,
> like interp/session/events were).
> **NEXT (free pick, none blocking):** non-blocking / multi-client sockets over the layer-7 transport;
> per-round hit granularity (a kernel event QUEUE — its own ADR); renderer polish (meshes; guns +
> kill-feed in the live `--fly` path); or an optional new seal (component/region damage; **B5** ISA
> atmosphere).
> **NOTE FOR THE NEXT AGENT:** layer 7 is code-complete + green locally (GCC+Clang, ctest 12/12, 132
> property tests, framing + bridge PASS). Verify guardian CI is green after push. `std::future` was
> deliberately avoided in the bridge (MinGW libstdc++ `call_once` link quirk) — use the
> `condition_variable` handshake if you extend it.
>
> ## ►► PRIOR STATE (2026-07-01): seal **ATM-Sphere v1.17r0** — **`last_hit_by` ON THE WEAPON-001 WIRE + `Event.attacker` DONE ✅ — ATTRIBUTION ARC CLOSED END-TO-END**
> **Latest (SEAL v1.17r0): attribution now replicates — a remote client renders the attributed kill-feed.**
> The one gap v1.16r0 left open closes, in BOTH replication paths at once. **(a) State path:** the
> per-aircraft attacker index **`last_hit_by` joins the WEAPON-001 snapshot section** as the 11th
> per-aircraft field (**snapshot protocol 5→6**), quantized at **unit scale** (new rail field
> `wire.weapon.lasthitby_scale=1` — an integer-valued index like ammo/ttl/owner, so exact AND compact;
> ZigZag carries the **-1 == never hit** sign natively, one byte). **(b) Event path:** the layer-6
> reliable EVENT channel gains **`Event.attacker`** (7th event field) — the server, which derives each
> hit/kill from the observed hp delta, now also reads the target's **post-step `last_hit_by`** (by
> construction the striking round's owner) and stamps it into the event ⇒ the client's exact-sequence
> journal reconstructs **"AC0 downed AC1"**, not just "AC1 died". Fourth instance of the proven
> wire-reseal pattern (KIN-001 v1.4r0 → WEAPON-001 v1.12r0 → ammo v1.14r0 → this): reference edited
> FIRST (`snapshot_ref.py` gates on `protocol>=6`, protocol-5 back-compat proven by self-test +
> property test), C++ mirrored bit-for-bit, all **4 vector headers regenerated + in sync**
> (snapshot/weapon/session/event). **TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, or
> `data/tuning/**` touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL, no new golden, no new ctest target ⇒
> guardian.yml UNCHANGED.** Digests that legitimately move (regenerated net-layer artifacts, not
> goldens): session `fda717fe…`→`24f71845…` (client view + `FINAL_WEAPON` surface last_hit_by — the
> dead A6M2 reads **last_hit_by=0, the P-47**, never-hit ships read -1); event `dfcc1aaf…`→`06629a69…`
> + blackout `94ae31ea…`→`90d6e67c…` (the canonical event log gains the 7th field — unlike v1.14r0,
> the event digest moves this time BY DESIGN). `seads_record` carries `last_hit_by` into recordings.
> **Gates: 128 property tests (+2: `test_protocol5_omits_lasthitby` back-compat;
> events-are-attributed-and-attribution-replicates under ANY loss pattern), ctest 10/10 GCC+Clang,
> all 4 generated headers in sync, 10 goldens byte-identical.** Rails version 260→270. Ledger:
> **ADR-Step7-Guns-WireTransport-Attribution-v1.17r0**, SEAL_CARD v1.17r0 (+ CLAUDE.md header/rails/
> roadmap brought current for v1.16r0+v1.17r0 — the v1.16r0 session missed CLAUDE.md), receipt
> `…v1.17r0-<sha>.yml`. **The guns arc (G1→G4 + convergence + attribution) is now FULLY canonical AND
> fully replicable — kernel hook + snapshot wire + reliable event journal.**
> **NEXT (free pick, none blocking):** a genuinely cross-PROCESS transport (sockets) over the
> layer-5/6 frames; per-round hit granularity (a kernel event QUEUE, not a last-writer field — its own
> ADR); renderer polish (meshes; guns + kill-feed in the live `--fly` path); or an optional new seal
> (component/region damage; **B5** ISA atmosphere).
> **NOTE FOR THE NEXT AGENT:** the prior session died on an API error AFTER the v1.17r0 code was
> complete and green but BEFORE tests/ledger/commit; this session added the +2 property tests, wrote
> the ledger, and committed. **v1.16r0 had also never been pushed** — one push (`a53c211..571885b`)
> shipped BOTH seals (v1.16r0 code `731dbe0` + receipt `bdfdd82`; v1.17r0 code `f9ee33e` + receipt
> `571885b`).
> **GIT: merged + PUSHED to `origin/main`; guardian CI run
> [28557986304](https://github.com/cjcgervais/seads/actions/runs/28557986304) GREEN** — Python gates +
> MSVC + GCC/Clang × x64/AArch64 + the cross-toolchain hash aggregation gate all reproduce: the 10
> v1.16r0-moved goldens bit-for-bit AND the v1.17r0 geo001/snapshot/weapon/lockstep/interp/predict/
> session/event parity legs (new session + event digests bit-identical cross-toolchain). **Both seals
> fully landed.**
>
> ## ►► PRIOR STATE (2026-07-01): seal **ATM-Sphere v1.16r0** — **ATTACKER ATTRIBUTION (last_hit_by / "who fired the killing round") DONE ✅**
> **Latest (SEAL v1.16r0): the kernel now records WHO killed whom.** The guns/netcode arc could model a
> full dogfight and even reliably replicate the *moments* of combat (layer 6 DERIVES hit/kill events from
> hp deltas over the lossy wire) — but nothing recorded the **attacker**. The kernel knew it at hit time
> (the striking round carries `owner`, used since G2 for the self-hit exclusion) and threw it away. This
> seal is the **kernel-side event hook** every prior handoff deferred. Each aircraft gains **`last_hit_by`**
> — the index of the aircraft whose round most recently damaged it, or **-1 (`NO_ATTACKER`) == never hit** —
> set at hit time from the round's `owner` (`ac.last_hit_by = float(p.owner)`, one line beside the existing
> damage apply). It **persists through death**, so at hp≤0 it names the **killer** ⇒ an attributed kill-feed.
> **`last_hit_by` is the 11th per-aircraft snapshot f64** (appended after `ammo`), so it is canonical hashed
> state ⇒ **ALL 10 goldens move** — but **provably additive**: stripping the 11th f64 from each v1.16r0 golden
> reproduces its v1.15r0 hash **byte-for-byte** (10/10 dry strip-diff), so attribution perturbs **no**
> trajectory/hp/fire_cd/ammo/gamma/projectile. **Meaningful new behavior tied to a golden:** GOLDEN-SK-Hit-001's
> a6m2 now ends `hp 0` **and `last_hit_by = 0`** (the p47d); the p47d, never hit, stays -1. **STILL no new
> det_math** (a pure integer-valued state, like fire_cd/ammo — the guns arc's zero-new-transcendental streak
> holds). **Off-wire this seal** (deferred one seal, exactly as `ammo` was kernel-state at G4 then rode the
> wire at v1.14r0): only the **2 vectors that hash `kernel.snapshot()` move** — `lockstep_vectors.h` (desync
> tripwire) + `predict_vectors.h` (client-prediction canonical digest), regenerated; the wire-based vectors
> (`snapshot`/`weapon`/`session`/`event`/`interp`/`geo001`) are **byte-identical**. **No new golden, no new
> ctest target ⇒ guardian.yml UNCHANGED** (same 10 IDs). New goldens: Sphere `f2db95bd…`, Hit `612e5407…`,
> Winchester `f351ee04…`, Gunfire `0e648539…` (+6 flight goldens). **Gates: 15/15 receipt PASS, 126 property
> tests (+7: `test_attribution.py` — default -1 / attributes-to-firer / no-hit-no-attribution / persists-
> through-death / 11th-snapshot-f64 / Hit-scenario-records-the-kill / deterministic), ctest 10/10 GCC+Clang,
> all generated headers in sync, 10 goldens C++≡Python bit-for-bit GCC+Clang.** Rails version 250→260. Ledger:
> **ADR-Step7-Guns-Attribution-v1.16r0**, SEAL_CARD v1.16r0, receipt `…v1.16r0-<sha>.yml`.
> **NEXT (the natural v1.17r0 follow-up, then free pick):** put `last_hit_by` on the WEAPON-001 wire
> (protocol 5→6, a transport-only reseal like `ammo` v1.14r0) + add `Event.attacker` to the layer-6 channel
> ⇒ an **attributed kill-feed** a remote client can render; then cross-PROCESS sockets over the layer-5/6
> frames; component/region damage; renderer meshes / guns in the live `--fly` path; or **B5** ISA atmosphere.
> **GOTCHA confirmed for the next agent:** a new per-aircraft canonical f64 moves ONLY `lockstep`+`predict`
> vectors (they hash the world_hash); wire-based vectors stay frozen while the field is off-wire. Regenerate
> those two (not all 13) and rebuild — `make_receipt` catches it if you miss one.
> **GIT: pushed with v1.17r0 (`a53c211..571885b`); guardian CI run
> [28557986304](https://github.com/cjcgervais/seads/actions/runs/28557986304) GREEN — MSVC + GCC/Clang ×
> x64/AArch64 reproduce all 10 moved goldens + the parity legs.**
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.15r0** — **GUN CONVERGENCE (BORESIGHT HARMONIZATION) DONE ✅**
> **Latest (SEAL v1.15r0): the guns are now harmonized — rounds are boresight-zeroed to a per-airframe range.**
> Each envelope gains **`convergence_m`** (per-airframe boresight range; a new `envelopes.AERO_FIELDS` scalar,
> appended after `ammo_start` in all 8 JSONs + the C++ `Envelope` struct). SEADS models a single **centerline**
> battery, so lateral convergence is undefined; harmonization is realized as **vertical boresight zeroing** —
> at spawn a fired round's initial **γ is offset UP** by the flat-fire drop-compensation angle
> **`δ = 0.5*g0*convergence_m / (v*v)`** (v = firer TAS + muzzle_v_mps) so its ballistic trajectory crosses the
> aim (sight) line at `convergence_m` instead of always falling below the nose. Mirror C++ `Kernel::spawn_projectile_`
> ↔ `ref_kernel._spawn_projectile` with **identical op order** (bit-exact). **Pure +-*/ ⇒ NO new det_math**
> (the guns arc's zero-new-transcendental streak holds). This is a **kernel spawn-geometry** change ⇒ a golden
> hash moves ⇒ seal. **Surgical golden movement:** only the **3 firing goldens move** — Gunfire `b25bb81c…ba33dc50`,
> Hit `8b8a84be…7d73e15c`, Winchester `14aa488b…5c061196` — the **7 non-firing goldens are BYTE-IDENTICAL** (they
> never hit the spawn path) and keep their v1.13r0 stamp (verified by a dry pre-reseal diff: 7 MATCH / 3 DIFFER,
> and C++ gcc==clang==committed on all 10). **Outcomes preserved:** Hit-001 still kills the A6M2 (hp→0, corpse
> frozen), Winchester-001 still empties to ammo 0 (22 rounds live) — the sub-metre elevation (δ≈1e-3 rad) shifts
> trajectories without changing which rounds hit or when (≪ the 60 m hit gate). **No wire change** (γ already rides
> KIN-002 / the projectile block): weapon-vector parity untouched, **session digest moves** (SESSION-SK-001's P-47
> fires → round positions shift; `session_vectors.h` regenerated), **event digest UNCHANGED** (`event_vectors.h`
> byte-identical — hit/kill events derive from hp deltas, kill sequence identical). Per-airframe convergence_m
> (initial balance, ~180–275 m): p47d 270, p51 275, spitfire/ki61 230, a6m2 250, la7 200, bf109/yak3 180.
> **Gates: 119 property tests (+2: exact spawn formula + monotonicity; the level-fire "compensates bullet drop
> at range" zeroing test), ctest 10/10 GCC+Clang, all 6 generated headers in sync (envelope_tables/scenario_params
> + snapshot/weapon/session/event vectors), 10 goldens C++≡committed GCC+Clang.** No new golden / ctest target ⇒
> guardian.yml unchanged. Rails version 240→250. Ledger: **ADR-Step7-Guns-Convergence-v1.15r0**, SEAL_CARD
> v1.15r0, receipt `…v1.15r0-<sha>.yml`. **NEXT (free pick, none blocking):** cross-PROCESS sockets over the
> layer-5/6 frames; attacker attribution (a kernel event hook, its own ADR); renderer meshes / guns in the live
> `--fly` path; or an optional new seal (component-damage, **B5** ISA atmosphere).
> **GIT: merged + PUSHED to `origin/main`** (code `0c9365b` + receipt `9500ee9`; `8fd08e6..9500ee9`);
> guardian CI run [28498500225](https://github.com/cjcgervais/seads/actions/runs/28498500225) **GREEN** —
> MSVC + GCC/Clang × x64/AArch64 reproduce all 10 goldens bit-for-bit (3 moved: Gunfire/Hit/Winchester;
> 7 frozen) + the geo001/snapshot/weapon/lockstep/interp/predict/session/event parity legs.
> **GOTCHA for the next agent:** a new `envelopes.AERO_FIELDS` scalar also feeds `gen_lockstep_vectors.py`
> + `gen_predict_vectors.py` (they embed the envelope table) — regenerate ALL 13 generators, not just the
> obvious ones. make_receipt caught it here (lockstep/predict gates FAILed until regenerated).
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.14r0** — **`ammo` ON THE WEAPON-001 WIRE DONE ✅ — GUNS ARC G1→G4 FULLY WIRED (canonical + replicable)**
> **Latest (SEAL v1.14r0): the magazine now crosses the wire — a remote client shows rounds-remaining.**
> The one gap G4 left open (ammo was canonical state but OFF the wire) closes. The per-aircraft magazine
> **`ammo` joins the WEAPON-001 snapshot section** as a 10th per-aircraft field (**snapshot protocol 4→5**),
> quantized at **unit scale** (`wire.weapon.ammo_scale=1` — a pure integer counter, so exact AND compact, a
> 2-byte varint for a 340-round belt, exactly like `ttl`/`owner`). This is the **third instance** of the
> proven wire-reseal pattern (KIN-001 v1.4r0 → WEAPON-001 v1.12r0 → this): edit the reference
> `snapshot_ref.py` FIRST (ammo gated on `protocol>=5`; a protocol-4 frame stays byte-identical, proven by a
> back-compat self-test + the new `test_protocol4_omits_ammo`), mirror `snapshot.{h,cpp}` bit-for-bit,
> regenerate `gen_weapon_vectors.py`→`weapon_vectors.h`→`seads_weapon_test`. **TRANSPORT-ONLY: no
> `src/kernel/**`, `src/det_math/**`, or `data/tuning/**` touched ⇒ ALL 10 GOLDENS BYTE-IDENTICAL** (Sphere
> `40ff6dd2…59315881` … Winchester `473bcad9…7630ad66`, gcc==clang==committed), **no new golden, no new
> ctest target ⇒ guardian.yml unchanged.** **Downstream riders (no-seal, bundled):** the layer-5 session
> **client-view now surfaces `ammo`** (so the reconstructed fight shows the P-47 walk its magazine 340→333
> over its 20-tick burst while the never-firing A6M2/Spitfire stay full) — the session **digest moved**
> (`78e013ab…`), `session_ref.py`↔`session.cpp` + `session_vectors.h` regenerated, `FINAL_WEAPON` gains
> `ammo`; the layer-6 **event digest is UNCHANGED** (`event_vectors.h` byte-identical — events derive from
> `hp` deltas, not `ammo`); `seads_record` emits an `"ammo"` array so the web viewer HUD can draw a
> rounds-remaining counter off the decoded wire. **Reseal only because the wire is a sealed rail** (rails
> version 230→240, seal string bumped). **Gates: 117 property tests (+1), ctest 10/10 GCC+Clang, all 4
> generated headers in sync (event byte-identical), 10 goldens C++≡committed under GCC+Clang.** Ledger:
> **ADR-Step7-Guns-WireTransport-Ammo-v1.14r0**, SEAL_CARD v1.14r0, receipt `…v1.14r0-<sha>.yml`.
> **NEXT (free pick, none blocking):** a genuinely cross-PROCESS transport (sockets) over the layer-5/6
> frames; attacker attribution (a kernel event hook, its own ADR); renderer meshes / guns in the live
> `--fly` path; or an optional new seal (gun convergence / component-damage, **B5** ISA atmosphere).
> **GIT: merged + PUSHED to `origin/main`** (code `370334c` + receipt `caefdc2`; `5751ec8..caefdc2`);
> guardian CI run [28496444072](https://github.com/cjcgervais/seads/actions/runs/28496444072) **GREEN** —
> MSVC + GCC/Clang × x64/AArch64 reproduce all 10 goldens bit-for-bit + the geo001/snapshot/weapon/
> lockstep/interp/predict/session/event parity legs + the cross-toolchain aggregation gate.
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.13r0** — **Step 7 guns / G4: FINITE AMMUNITION DONE ✅ — GUNS ARC G1→G4 COMPLETE**
> **Latest (SEAL v1.13r0): guns now run out of bullets.** The last gap in a WWII gun sim closes — every
> airframe carries a finite magazine. Each envelope gains **`ammo_start`** (per-airframe rounds; A6M2
> fewest at 100 = the famous ~60-rpg 20 mm cannon, P-47D most at 340 = eight deep .50-cal belts), loaded
> through the single-source-of-truth `envelopes.AERO_FIELDS`. Firing — already gated on `fire && alive &&
> fire_cd==0` — gains **`&& ammo > 0`**; a shot spawns the round, **decrements ammo** (clamped at 0), and
> resets the cooldown. An empty magazine falls silent (**"Winchester"**): the trigger may stay held and the
> aircraft alive, but no round spawns and the cooldown is not reset. **`ammo` is the 10th per-aircraft
> snapshot f64** (appended after `fire_cd`) — canonical state hashed into the world_hash, so as `gamma`(B2)/
> `hp`(G2)/`fire_cd`(G3) did, **all 9 prior goldens move** (ammo constant/identical — verified the Sphere
> golden's first 9 f64 + projectile block are BYTE-IDENTICAL to v1.12r0, only `ammo=500` inserted). **NEW
> `GOLDEN-SK-Winchester-001`** gates the depletion: an A6M2 flies wings-level holding the trigger, looses
> one round every 9 ticks (ticks 0,9,…,891 = 100 rounds), **empties at tick 891**, and the cannon goes
> silent for the rest of the 950-tick run (22 rounds still airborne at the end). **NO new det_math** (a
> pure integer-valued counter, like fire_cd — det_sin/det_cos + ÷×− only). **NO wire change:** ammo lives
> in the CANONICAL state but is deliberately kept OFF the WEAPON-001 wire this seal (exactly as `fire_cd`
> was kernel-state at G3 before it rode the wire at v1.12r0) ⇒ the sealed `seads_weapon_test` parity gate
> is byte-identical and **the layer-5/6 session & event digests are UNCHANGED** (`25fcc41e…`, `dfcc1aaf…` —
> ammo is off-wire and no airframe depletes in the short SESSION-SK-001 fight). **Reseal because a golden
> world_hash changes** (state grows + a new golden); rails version 220→230, seal string bumped everywhere.
> **10 goldens now, C++ ≡ Python bit-for-bit under GCC + Clang (Winchester `473bcad9…7630ad66`, 22 live
> rounds on both).** **Gates: 15/15 receipt PASS, 116 property tests (+3: ammo character / exact-depletion
> cadence / no-underflow), ctest 10/10 GCC+Clang, 13 generated headers in sync, 10 goldens C++≡Python.**
> Ledger: **ADR-Step7-Guns-G4-v1.13r0**, SEAL_CARD v1.13r0, receipt `…v1.13r0-<sha>.yml`; guardian.yml
> gains the Winchester id in its 3 golden loops. **Guns arc G1→G4 COMPLETE — the kernel gunnery model
> (ballistics + hit/damage + roster/fire-rate + ammunition) is closed.** **NEXT (free pick, none blocking):**
> put `ammo` on the WEAPON-001 wire (a small follow-up reseal ⇒ a remote client shows a rounds-remaining
> counter); cross-PROCESS sockets over the layer-5/6 frames; attacker attribution (a kernel event hook, its
> own ADR); renderer meshes / guns in the live `--fly` path; or an optional new seal (convergence /
> component-damage, **B5** ISA atmosphere). **GIT: committed + PUSHED to `origin/main` (code `2de3fc3` +
> receipt `6d4d842`); guardian CI run [28488783947](https://github.com/cjcgervais/seads/actions/runs/28488783947)
> GREEN** — MSVC + GCC/Clang × x64/AArch64 reproduce all 10 goldens bit-for-bit (incl. the new Winchester)
> + the geo001/snapshot/weapon/lockstep/interp/predict/session/event parity legs + the cross-toolchain
> aggregation gate.
> _(The layer-6 EVENT-channel banner below is retained as history — COMPLETE.)_
>
> ## ►► PRIOR STATE (2026-06-30): seal **ATM-Sphere v1.12r0** — **NETCODE LAYER 6: reliable combat-EVENT channel DONE ✅ (no-seal, rides v1.12r0)**
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
