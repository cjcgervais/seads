# HANDOFF — 2026-08-28. **THE TRACK IS BUILT AND READS. TWO CALLS ARE CHAD'S, AND THEY ARE FIRST.**

> **LAUNCH LINE (paste into a fresh session):**
> Read `docs/snow_session_handoff_20260828_NEXT.md`. **R4 is BUILT, RULED and VERIFIED; R4b
> fixed four defects it and R1 left behind.** Nothing is waiting on code — §4.1 and §4.2 are
> waiting on CHAD, so ASK before building either. `docs/snow_session_handoff_20260828_R4.md`
> (the depth law) and `_R4b.md` (the four defects, with the measurements) are the authority
> for THIS rung; `docs/snow_session_handoff_20260827.md` is still the authority for MECHANISM
> and measurement generally. This file supersedes `_20260827b_NEXT.md` entirely.

---

## 1. STATE

| | |
|---|---|
| **Branch** | `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow` |
| **Head** | `6a3df8865` — **NINE commits, ALL COMMITTED, NOT PUSHED.** Base was `f92939e2e`. |
| **Gate** | ★ **THE FULL GATE COMPLETES NOW** — 1565 cases, **1560 pass / 5 fail**, `REAL_EXIT=42`, zero "Not Run". See §5. |
| **Drive** | R3 look: signed. R4 track: **built and verified headless, NOT yet driven by him.** |

### This session's five commits (on top of the four R3 ones)

1. `1cc8b87b8` — **R4**: the readability floor he ruled; the cut rides per-stamp in the ring.
2. `53b0826e3` — **R4b**: he drove it and saw nothing. Four defects, one of them R4's own P0.
3. `49b307a54` — R4b probe: correct the patch-vs-mesh readout for the folded base.
4. `fd9c25e63` — docs: the closing drawn-vs-driven measurement.
5. `6a3df8865` — docs: the full gate completes, and there were always 5 failures, not 4.

---

## 2. WHAT IS SIGNED, AND MUST NOT BE REOPENED WITHOUT HIM

**From R3 (unchanged, `render/planet.h`):** `full_depth = 0.30f`, `barren_shed_keep = 1.0f`,
depth-keyed exposure ships **ARMED**. ⚠ `full_depth = 0.30` is **LOW ON PURPOSE** — the two
counter-intuitive traps are written at the dials and repeated in `_20260827b_NEXT.md` §2. Do not
"correct" it upward. The old fade-only fence expression is pinned as an **ANTI-needle** in
`test_winter_reskin.cpp`.

**From R4 (`world/tracks.h`): the FLOORED DEPTH LAW.** Chad ruled it against a measured ladder:

```
depress = min(depth * 0.90, max(0.55, 0.45 * depth))
```

He chose the floor with the ladder in front of him — 0.12 m absent, 0.30 faint, 0.60 legible,
1.50 a trench. The `min()` is the physics that survives it. **Consequence he was told and
accepted: on thin ground the cut stays sub-readable, and that gap is a SHADING problem, not a
reason to raise the floor.**

**From R4b — three invariants that each cost a defect to learn. Read before touching either file.**

1. ⚠ **NO never-fill-in GUARD IN `TrackField::add`.** R4 shipped `d = max(d, nearest.depress)` and
   it became a **session-wide running max** (consecutive stamps of one pass are always inside the
   groove), cutting 0.55 m across roads and bare rock. The fix is at the INPUT —
   `SnowpackField::track_lay_depth_at`, the UNDISTURBED snowpack — so the guard is unnecessary.
   The call site says so. **Do not reintroduce a distance-based guard.**
2. ⚠ **THE RIDER PATCH SITS ON `facet + draw_fold_at`, NOT ON THE BARE FACET.** Since R1 the
   planet mesh carries the fold; the patch did not, so it drew 0.56 m *under* the world (measured).
   **The fold is NOT rim-faded** — only lift and the track are. Fading it cuts a crater rim.
   ★ **The fork detector: drawn-minus-driven must stay at `lift_m` (0.150 m).** It was 0.713 m.
3. ⚠ **SHAPE COMES FROM THE NEAREST DISTANCE; AMPLITUDE IS BLENDED.** Two different things. The
   anti-sum rule (one profile off one distance) is what stops neighbouring berms filling their own
   groove and **stays**. Taking the DEPTH from the same winner put a half-metre Voronoi cliff in
   the DRIVEN surface. R4's comment claimed blending would cause that; it is the other way round.

---

## 3. ★★★ THE STANDING LESSONS

**(carried, still binding) An A/B whose arms are indistinguishable is not a measurement.** Any dial
you ask him to rule on must be **visible and steppable from the seat**. And **put the local fix AND
the root fix to him as options** — that is his standing instruction, and it is what produced the
two-dial R3 outcome that satisfied both of his rulings.

**★ NEW, AND IT IS THE LESSON OF THIS SESSION: A RIG THAT ONLY EXERCISES THE STILL CASE CERTIFIES
NOTHING ABOUT THE MOVING ONE.** `SEADS_TRACK_DEMO` lays its whole star in one instant at a **parked**
machine at 0 km/h. R4 was signed off on screenshots of exactly that, and the moving case — stamps
laid per tick while the machine *and the camera* move — had never been rendered once. He drove it
and saw nothing. This is the **same shape as the bank-strip gap** (§4.3 below), and it is the second
time in three sessions that the one place he reports a defect is the one place no instrument could
look. **Before signing any rung: name the state the rig cannot reach, and go build the rig that
reaches it.**

**Corollary, also paid for here:** when a fresh-context red team contradicts your own measurement,
**re-run the experiment, do not defend the reading.** Two of my conclusions were wrong this session
and its corrections were right both times.

---

## 4. THE WORK QUEUE — IN ORDER

### 1. ⭐ HE DRIVES R4. **NOTHING ELSE UNTIL HE HAS.**

```
cd D:\seads_sandboxes\sandbox_snow
cmake --build build-play --target seads -j 8
.\build-play\seads.exe
```
Land and stop the aircraft, **J** to mount, drive, **look back.** ⚠ `build-play` (RelWithDebInfo),
never `build/` — CLAUDE.md's ruling, `-O0` is 45× on the rider skinning alone.
`SEADS_TRACK_FLOOR=0` is the A/B arm (honest depth-proportional, no floor) and the kill switch.

### 2. ⭐ THE CHASE CAMERA NEVER FRAMES THE TRACK. **HIS CALL — ASK, DO NOT BUILD.**

The drive view looks forward over the rider's shoulder. The track exists only behind him, the
machine occludes the freshest few metres, and ahead is always virgin snow. **Even with everything
in §2 fixed, he has to look back to see his own work.** This is very likely the largest single term
in *"no I dont see any tracks"*. The honest options are (a) lift/pull the chase camera, (b) a
glance-back, (c) accept it — and they are different games. **Put all three to him with what each
costs.** `cam_dist/cam_high/cam_ahead` are already retunable live via `SEADS_SLED_CAM`.

### 3. ⭐ BUILD THE BANK-STRIP PROBE. **STILL UNBUILT. STILL A MEASUREMENT, NOT A FIX.**

Chad, twice: *"Sometimes my ski is going down into the road."* The road deck measures 0.0% under
across 9,321 samples, but **`SEADS_VP1_GAP` compares the drive against the MESH**, while the 0–9 m
band beside a road is drawn by the **BANK STRIPS** (`render/bank_mesh.cpp`). Nothing in this repo
measures the drive against `bank_mesh`. **Do not tune the banks against a report no instrument can
confirm** — extend the probe to compare `drive_radius_at` against the bank-strip surface in that
band, report under-percentage and worst case, then decide.

### 4. §3.2b — THE R2 SHOULDER FORK. **HIS RULING IS STILL HALF-MADE.**

*"on peoples lawns seems to sink a bit less"*. Measured drawn-above-driven by distance from a
corridor: **+0.397 m at 20–40 m**, +0.101 at 40–80, ~0 beyond 80. Lawns sit inside the 60 m mask.
**Option 3 ("accept it — probably not visible") is DEAD; he saw it.** Options 1 (extend the bank
strips to the mask width) and 2 (excavate the corridor from the mesh) are **still his call — bring
both with costs.** ⚠ Closing Chelmsford did **not** close this: he accepted the phantom corridors
as map truth, he did not rule on the shoulder float.

### 5. R5 — AND IT IS NOW THE ROOT CAUSE, NOT A POLISH RUNG

He already said yes. What changed: **R4 had to spend GEOMETRY to buy readability because this snow
barely shades.** Look at any R4 screenshot — huge terrain slopes, almost no tone change; a 0.12 m
groove is ~18° of slope and reads as nothing. R5 (micro-relief / BRDF) is why the floor was needed
at all, and it would make *untouched* snow read too. ⚠ Put the drawn-vs-driven trade to him
explicitly first. Sibling: **R3's leftover M2 softening** — the normal map is baked from bare DEM so
snow still shades with rock's micro-roughness; `vSnowDepth` is already at the FS.

### 6. THE THIN-SNOW SCUFF — the honest close for "no tracks on trails"

On a packed trail `depth_at` is `trail_pack_m = 0.12` → a 0.108 m cut; on a road 0.02 → 0.018 m.
His own ladder says 0.12 m is **absent**. **So the floor never applies where he drives fast, and R4
closed nothing on trails.** The answer is a compaction/albedo SCUFF, not more geometry:
`compaction_at` already computes the right scalar. ⚠ The obstacle is a channel — the patch writes
`texcoord.y = 0` and `draw_snow_patch` **forces `snow_depth_mix = 0` on purpose** (with R3 armed a
zero there paints a bare-ground rectangle under the rider, `render/planet.cpp:1539`). A scuff
channel must be reconciled with R3's exposure; it is not a dial.

### 7. UNFLOWN AND CHEAP

- **G1 / G2 rotor gyroscopics** — built, tested, pinned OFF, **never driven by anyone**. `k_gyro`,
  `k_gyro_react` in `sim/sled.h`. One at a time.
- **The night pass.** Armed exposure now thins night whiteness on scoured ground and his *"snow must
  STAY WHITE"* ruling predates it. He just presses `]`.

### 8. KNOWN DEBT, NOT SCHEDULED (all detailed in `_R4b.md` §7)

- **P1 — the stamp samples depth at its CENTRE but cuts across a 1.1 m reach.** Laid 0.5 m from a
  corridor edge, a bush-depth stamp reaches onto the deck and cuts below it. Same at rock outcrops.
  The clean fix is to clamp the cut by the LOCAL undisturbed depth at composition time in
  `snowpack.cpp`, where the depth is known — `TrackField` is pure and cannot ask.
- **P2 — the ring wraps at 5 km** (~4–7 min of driving): old track self-erases, and evicting a deep
  stamp beside a shallow neighbour RAISES the surface — the fill-in the design forbids, defeated by
  capacity rather than logic.
- **The draw-side serration.** Now that the depth-clipping is gone (§2.2), whatever remains is grid
  aliasing and **the lever is patch resolution**: `snow_patch_indices` returns `uint16` (raylib's
  `Mesh::indices` is `unsigned short`) so `n_side <= 255`; today 160 × 0.5 m. **224 × 0.36 m is the
  same 80 m span at 1.4× finer cells, 50,176 verts, still under the cap** — with a real cost/latency
  trade to put to him.
- **The ski-cut signature is NOT buildable geometry and should stop being specced as such.** 0.135 m
  cuts are sub-cell at 0.5 m *and* at the 0.36 m the index cap allows; drawing them needs ~0.05 m
  cells, which buys a **17 m** patch — the track would vanish 8 m behind the machine.
- **Chelmsford phantom corridors: CLOSED by his ruling** — *"I dont care about the mesh as road
  really, its kind of a coordor anyways there."* Accepted as map truth. No re-bake, INV-6 untouched.

### 9. LATER

The roost rungs (`docs/roost_consult_packet.md`; ⚠ its two P0s about `FxPool` drag and the missing
`DrawInfo` channels still stand).

---

## 5. THE GATE — READ THIS BEFORE YOU REPORT A NUMBER

★ **THE FULL GATE COMPLETES.** Two handoffs record it as never having finished on this box. The
contention was `ctest` against other sandboxes' `ctest`, not the suite. One process, one file:

```
./build/seads_tests.exe > gate.txt 2>&1 ; echo "REAL_EXIT=$?" >> gate.txt
```

**On `49b307a54`: 1565 cases, 1560 passed, 5 failed, `REAL_EXIT=42`, zero "Not Run".**

| failing case | attribution |
|---|---|
| `sled_slides_before_it_tips_on_flat_snow` | pre-existing GI4 sled debt |
| `sled_grip_ceiling_stays_below_the_tip_threshold` | ″ |
| `sled_assist_reference_plane_is_load_weighted` | ″ |
| `sled_debug_sink_is_write_only` | ″ |
| ⚠ `probe P-F: the relentless raider keeps the pump and shoots back` | **enemy-AI thread, NOT snow** |

⚠ **The fifth was never on anyone's list**, because no full gate had finished to put it there —
every prior session measured a *relevant subset*, honestly reported "4", and was blind to it.
`test/unit/test_enemy_ai_e6.cpp:798` has **zero** references to `snowpack`, `depth_at`, `TrackField`
or `snow_patch`. **Do not fold it into the snow debt and do not rediscover it as a regression.**
The honest ledger is **4 sled + 1 enemy-AI**.

---

## 6. INSTRUMENTS (`--smoke N` needs a FRAME COUNT or it just opens a window)

| command | what it measures |
|---|---|
| ★ `SEADS_SLED_DRIVE=<0..1>` | **smoke-only: pins the thumb throttle open so a headless run really DRIVES.** The rig whose absence let R4 ship broken. |
| ★ `SEADS_TRACK_PROBE=1` | per-second: speed, stamps, laid?, lay depth, deform, **and drawn-vs-driven vs the planet mesh**. Watch `patch_vs_mesh` off-track — it must stay at `lift_m`. |
| `SEADS_TRACK_FLOOR=<m>` | seeds R4's readability floor; **`0` disarms it** (the depth-proportional A/B arm, and the kill switch) |
| `SEADS_TRACK_DEMO=1` | the deterministic 8-ray star — ⚠ **PARKED machine only.** Never sign a rung on this alone. |
| `SEADS_SLED_DEBUG_MODE=1` | arms drive mode from frame 1 in smoke, no keypress |
| `SEADS_SLEDCAM="dist,az,el"` | smoke-only orbit of the machine (az 0 = ahead, + = around its left) |
| `SEADS_SLED_CAM="dist,high,ahead"` | retunes the LIVE chase camera without a rebuild — the §4.2 dial |
| `SEADS_PATCH_DEBUG=1` / `SEADS_PATCH_LIFT=<m>` | patch anchor drift + span; exaggerated lift so the patch footprint is unmistakable |
| `SEADS_NO_SNOWPATCH=1` | the patch A/B kill switch |
| `SEADS_R3_PROBE=1`, `SEADS_R3_SHEDKEEP`, `SEADS_R3_FULLDEPTH`, `SEADS_SNOWDEPTH` | the R3 exposure/fence set — see `_20260827.md` |
| `SEADS_VP1_GAP=1` | see-vs-drive gap by distance from a corridor. ⚠ **vs the MESH, not the bank strips** — §4.3 |
| `SEADS_BARREN_PROBE=1`, `SEADS_SNOWDUMP=<f>.tsv` | Chad's black-rock fence; terrain/depth/driven census |
| `seads_sled_probe gyro 1.0` | the G1 A/B |

**In-game:** PgUp/PgDn steps `full_depth` (SHIFT = fine), value on a plate top-left, hidden at ship.

---

## 7. HAZARDS THAT COST TIME

1. ⚠ **THE HEREDOC HAZARD IS REAL.** `bash <<'EOF'` **collapses backslashes** in inlined Python, so
   `"\\n"` arrives as a real newline and every anchor match against a C string literal fails.
   **Write edit scripts to a FILE and run the file.**
2. ⚠ **NEVER PIPE A GATE THROUGH `tail`** — the exit code comes from `tail`. Redirect to a file and
   check the summary line, the `Not Run` count, AND `REAL_EXIT` (§5 does).
3. ⚠ **Two ctest gates must never share `build/`**, and other sandboxes' gates will starve yours —
   which is exactly why the full gate "never completed". §5 is the way around it.
4. **Do not rebuild while `seads_tests.exe` is running** — the relink fails with
   `ld: Permission denied` and you get a stale-binary false result. The repo has lost a mutation
   round to this before.
5. **`--smoke` requires a frame count** (`--smoke 30`). Without one it silently opens a window.
6. **Comments that cite `file:line` rot the moment you insert above them.** Re-verify any line
   number you quote. Three such comments were found stale and fixed this session, and one of them
   (`snow_patch.cpp`'s "the planet mesh has NO snow in it") was **load-bearing and wrong**, which is
   how the patch spent two rungs drawing below the world.
