# SESSION HANDOFF 2026-09-12 — road-repair lane: the Onaping pass LANDED, next = drive on the drawn triangle

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260912_road_repair.md; do §1."

Worktree `D:\seads_sandboxes\road-repair`, branch `sandbox/road-repair` (== main `ca73158fe` at this
handoff, plus this doc). Never work in `D:\flight_sim2\seads-recon` — that is Chad's fly tree; the
sentinel session (`flight-sim2-ae`) merges main into it and rebuilds `build-play`. You never touch it.

This pass was run as **guide (Fable) + Opus builders**, at Chad's ask ("use opus so as to not rack up
too many tokens, be the guide"). It worked: the guide reads the builder's reasoning, not just its
answer, and both real defects of the pass were caught that way (§6). Keep the shape.

The previous handoff, `docs/SESSION_HANDOFF_20260910_road_repair.md`, is still the record of the
five rungs before this one and of the landing SOP; §5 below only adds what changed.

## §1 What to do first

1. `git fetch origin`; confirm `origin/main` is at or past `ca73158fe`. If it moved, read what landed
   (`git log --oneline ca73158fe..origin/main`) before you branch anything.
2. Chad's verdict on this pass was **"okay good to push it"** (2026-09-12, after riding the hill north
   of the Valley pump on the lane's `build-play`). The pass is landed. **Do not reopen its three rungs
   unprompted.**
3. The next rung is **§3**. It needs **one ruling from Chad before anyone builds** (it changes the
   driven surface, which is snow-lane physics). Ask the question in §3 as one line, then build on his
   answer. Everything else in §3 that does not depend on the ruling (the instrument) can start now.
4. **Always hand Chad a `build-play` exe.** He could barely test the Debug build ("no build-play").
   Configure it once in the lane:
   `cmake -B build-play -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/seads_sandboxes/road-repair/build/_deps/raylib-src`
   then `cmake --build build-play --target seads`. The gate stays on `build/` (Debug).
5. Before landing anything: the 2026-09-10 handoff §5 verbatim, plus §5 below.

## §2 What landed (three rungs on one lane, `ca73158fe`)

| rung | commits | what it is | kill / identity |
|---|---|---|---|
| (6) road-edge instrument | `05acea451`, `06eea5b45` | census columns `skirt_drop_m`/`skirt_side`, `float_2x/3x/4x_m`/`float_side`; `tools/road_repair/onaping_rank.py`; `SEADS_ONAPING_SMOKE=1` one-paste spawn at the Valley pump (ungates `SEADS_SMOKE_SPAWN_DIR` for a live ride) | — |
| (7) drawn apron | `b46c6de1d`, `c15938dc4`, `1a3bc2b98` | the bank strip continued past the burial skirt along `drive_r` wherever the ground falls ≥ 0.5 m across the skirt, lit like the banks; `render/bank_mesh.*`, `render/draw.*` | `[bank_mesh] apron_m` 12.0 → 0.0; `SEADS_NO_APRON=1` |
| (8) the sink (ribbon chord sag) | `312643a45` (ruler), `6ed179ce7` (along), `c5572c83e` (across) | the road ribbon was flat chords between baked GIS rungs **52.7 m apart at the median, 79.9 m max**, bridging over concave grade breaks; now subdivided at drape time; `render/ribbon_subdiv.h` (new), `render/ribbons.*` | `[ribbons] max_seg_m` 8.0 → 0.0, `max_tr_m` 5.0 → 0.0; `SEADS_RIBBON_MAXSEG=0`, `SEADS_RIBBON_MAXTR=0` |

Then docs, the merge of main `b74d35e44` (`4f48920c0`, 23 commits under us) and `ed6a6ba7f`
(`1f15b20a1`, docs-only), sentinel housekeeping (`39ffe6e8c` PNGs halved, `68c2db628` gate log
trimmed), and the LANES stamp `ca73158fe`.

**Headline numbers.** Rung 6: within 2.5 km of the Valley pump 12.3 % of plowed stations drop ≥ 1 m
across the 6 m skirt (3.1× planet), worst +4.266 m (a 33° shelf); the fold float (`float_4x_m`) is
NOT it (median negative, worst +0.88 m); no trail within 9 km. Rung 7: Valley shelves p90 drawn-vs-
driven gap 1.528 → 0.000 m (partly self-fulfilling: the apron is placed on `drive_r` and measured
against it — the pixel A/B is the honest evidence), +3.59 % bank verts. Rung 8: near Valley, worst
chord above `drive_r` **+7.224 → +1.016 m**, segments over the 0.10 m sink threshold **41.2 % → 8.1 %**,
transverse term 0.000; the north sector (his hill) +6.233 → +1.016; ribbon verts 100,604 → 1,406,148
(13.98×, 24 u16 meshes), release startup +0.77 s.

**Gate** (whole suite, detached, on code tip `c5572c83e`): `6 failed of 2045`, red set EXACTLY the
baseline six by name (`probe P-F`, `E12.1`, and the four sled legs — now numbered 1243/1244/1282/1285;
the runner grades by NAME). Everything after `c5572c83e` on main is docs/LANES only. Sentinel audit of
`ca73158fe`: PASS. Fly tree `seads-recon` merged as `266a4e651`.

**Docs of record:** `docs/road_repair/onaping_edge.md` (rungs 6–7, incl. the null-instrument
story in §2.5 and the fly checklist in §2.6/§3), `docs/road_repair/onaping_sink.md` (rung 8: §1 the
ruler and verdict, §2 the along cut, §3 the across cut and the stop rule, the fly paste), the 2.5 km
census slices `onaping_valley*.tsv` (DATA, query them, never read whole).

## §3 NEXT RUNG — drive on the drawn triangle (needs Chad's ruling)

**What is left of the sink, measured (onaping_sink.md §3):** after both cuts, 43 plowed segments
within 2.5 km of Valley still carry +0.10 to **+1.016 m** of ribbon above `drive_r`. It is not the
transverse term (0.000), not the deck floor (sag-vs-function == sag-vs-drive to 0.02 m). Halving the
cells once more moved the worst from 1.016 to 0.604 — a factor of 1.68, not 4 — so the shape is a
**slope discontinuity in `drawn_radius_at`** (the facet/fold kinks that rung 6 measured as 33°
shelves), not smooth curvature. Reaching 0.5 m by subdivision alone costs ~56× the baked vertex count.
The builder stopped there, correctly.

**The candidate fix, the other way round:** instead of drawing finer so the chord follows the
function, make the machine stand on the chord — floor `drive_r` to the **actual drawn ribbon
triangle** under the machine (the mesh `render/ribbons.cpp` uploads), not to `drawn_radius_at(d)`.
Then drawn == driven by construction at any cell size, and `max_seg_m`/`max_tr_m` become purely
visual dials. Cost: the physics needs a corridor→triangle lookup (the corridor query already gives
the run, station and lateral; the subdivided grid is regular per rung, so the cell is arithmetic,
not a search), and it touches `world/snowpack.cpp` (`apply_deck_floor`, ~501) — the **snow lane's
file**, announced never owned.

**The one question for Chad (ask it as one line):** *"May the deck floor stand the machine on the
drawn road triangle itself (the mesh you see) instead of the terrain function under it? It closes
the last metre of sink at any cell size; it is a physics change in the snow lane's file."*

**Instrument first, ruling or not:** extend `SEADS_RIBBON_SAG` (`render/road_census.*`, the ruler of
rung 8) with `tri_minus_drive_m` = actual uploaded triangle radius at the sample minus `drive_r`, so
the after-arm of this rung reads 0.000 by the same ruler that read +1.016. Pick the ruler before the
cut. Rung shape as always: instrument, one bounded fix with an identity dial, its doc
(`docs/road_repair/onaping_drive.md`), the lane ctest subset, a red-team, full gate detached on the
merged tip, `build-play` for his ride, sentinel, push on his word.

**If Chad says no:** the residual stays at ~1 m at 43 stations; document it as accepted in
onaping_sink.md and move to §4 in priority order.

## §4 Debts and the critic

Owned by this lane, in priority order after §3:

- **Frame cost of 1.41 M ribbon vertices** (24 `DrawMesh` calls vs 4) is **unmeasured** — only build
  time was clocked. If Chad says roads feel slower, `[ribbons] max_tr_m = 0` (→ 6×) then
  `max_seg_m = 0` (→ 1×) are the two retreats, in that order.
- **Startup timing** of the +72 % bank mesh (corner rung) + 3.59 % (apron) on his box — never timed
  in release; ask him once.
- **The speed lever he asked for** ("more places with a bit less snow"): groomed TRAIL segments
  (0.12 m) between the Valley pump and the pond north of it, and baked wind-SCOUR patches on the
  convex hill. **NEVER `base_m`** (0.77 m is a signed fixed input, WINTER LAW). Pumps stay OFF-ROAD.
  Needs his go; it is a content rung, not a repair.
- `world/props.cpp:141-142` tree scatter still calls `nearest()` unblended (critic #9).
- The river rows on the Onaping pond shore fall up to +5.566 m across the skirt (rung 6 found them,
  excluded from the verdict because rivers are not ribbons). He likes to drive that shore. A pond-
  shore pass is a separate rung; do not fold it into a road rung.
- Chad's owed A/B: `[snowpack] class_blend_m` 0.0 vs 1.0 (his stick; ships 0.0; never set it for him).

Not this lane's, named so nobody re-discovers them: the pump mast on bare DEM (`app/main.cpp`
`radius_at` not `drawn_radius_at`, one call site — pump/game-loop lane); pump apron + trample disks
(gait / game-loop); bank mesh ignores `deform_at` (snow lane); the four red sled legs (next sled lane);
the `SNOWDUMP` writer's `fopen("w")` CRLF bug (not ours); the `CENSUS_FINAL.md §1` paste blocks' camera
and debug envs (`SEADS_SLEDCAM`, `SEADS_SLED_DEBUG_MODE`) only work under `--smoke` because their
readers sit in the 5400–9100 band other lanes own — `SEADS_ONAPING_SMOKE` fixed only the spawn dir.

## §5 Landing SOP — what changed since 2026-09-10 (that doc's §5 still applies verbatim)

1. **Build `build-play` for him** (§1.4) and put its absolute path in the fly checklist. Every number
   he judges by feel was wrong on a Debug binary before (45× on the rider block alone).
2. **Sentinel pre-audits before the push** now: message `flight-sim2-ae` with the tip SHA + verdict
   as soon as the gate is green, fold its observations (it will flag repo weight: screenshots under
   `docs/` go in at ≤ 960×540, a gate log goes in as its verdict + red list, never the 580 KB file),
   message it again with the new tip, and again right before the push. It confirms the runway and
   takes the `seads-recon` merge.
3. **When main moves docs-only under you**, the sentinel will say so; merge it once, no re-gate is
   owed if it touched no code/config/generated — but you say that with the sentinel's verification
   quoted, not on your own reading.
4. **Full gate is ~60 min detached** (`Start-Process ctest`, log `build/.gate_ctest.log`, verdict
   VERBATIM from `python tools/gate/gate_baseline.py check`). `ctest -N` on main is now **2045**.
5. **Budget the session limit.** Two Opus builders were killed by it mid-run this pass (one at the
   API rate limit, once at the session limit). Rule given to every builder since, keep it: *if the
   limit hits, commit what compiles FIRST as "WIP <rung>" so nothing is lost, then continue.* Resume
   the SAME agent (its context is intact) rather than starting cold.

## §6 Traps found this pass (each cost real time; read before building)

- **A pixel A/B with the wrong camera is a null instrument.** Rung 7's builder shipped the apron OFF
  because before/after screenshots differed by 68 pixels *even with every apron vertex lifted 5 m*.
  The sled cam (`SEADS_SLEDCAM="2.2,35,12"`) is a narrow lens 35 m back; the frame ENDS at the bank
  strip's edge and the apron starts 18 m further out. It was never on screen in any arm. The tell
  was the +5 m probe: a positive control that does not light means the rig is blind, not that the
  effect is absent. Redone with `SEADS_SLEDCAM="80,180,30"` (dist is the only lever on how much
  ground is framed; visible half-width ≈ 0.736·dist): off/on 3,672 px, +5 m 25,126 px. **Always run
  a positive control through the same rig, and look at the picture yourself.**
- **The road census is BLIND to the ribbon's chords by construction.** It evaluates `drawn_radius_at`
  (a function) at 16 m stations and reported `SINK 0` near Valley on the very build Chad sank in. The
  real ribbon is triangles between baked rungs 53–80 m apart. When the ruler and the seat disagree,
  the ruler's *sampling* is the first suspect, not his eye. `SEADS_RIBBON_SAG` is the ruler that sees
  chords; the census SINK column stays useful only for the function-vs-drive registration.
- **A residual that scales linearly with cell halving is a slope break, not curvature.** Halve once,
  read the factor (4 = smooth, 1 = a hard step, ~1.7 = a kink). One halving named the remaining term
  and the stop rule without a third build.
- **Debug is not flyable.** "I can barely test this on account of no build-play" — see §1.4.
- **A builder's announce can name a switch that does not exist.** Rung 2's LANES announce cited a
  kill `SEADS_CENSUS_NO_FOLD` that was never written; the rung 3 builder caught it by byte-reading
  `app/main.cpp` (the NUL byte, now near line 6603, still makes `grep` lie). Audit announces against
  the tree, not the report.
- **"Skirt minus foot" and "positive = falls away" contradict** if the skirt is lower; the builder
  chose foot − skirt and documented it. State sign conventions as an inequality, not a subtraction.
- **The red `DEFEAT` plate in any `--smoke` shot is the game loop's**, not a road defect.
- **Inline python heredocs still mangle here** (and Windows cp1252 stdout chokes on ★ — add
  `sys.stdout.reconfigure(encoding="utf-8")`). Write the script to `build/.scratch/`, run the file,
  check the exit code.

## §7 Rulings Chad has made on this lane (do not re-ask)

- Pumps are OFF-ROAD by design; never connect one to the road net.
- Onaping drop-offs were the priority defect (2026-09-08) — addressed by rungs 6–8; he signed.
- Less snow = groomed trail segments + wind-scour; never `base_m`.
- `class_blend_m` is his stick A/B; ships 0.0.
- Rungs 1–5 ("flew it, all good") and 6–8 ("okay good to push it") are flown and signed.
- Guide + Opus builders is the working shape for this lane; watch tokens.

Owed by him: the §3 ruling (drive on the drawn triangle); whether roads feel slower at speed;
whether startup got slower; the `class_blend_m` A/B; the go for the speed lever.
