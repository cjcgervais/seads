# ⛔ SUPERSEDED — 2026-08-28. **START AT `docs/snow_session_handoff_20260828_NEXT.md`.**

> This file's §1 STATE and §4 WORK QUEUE are STALE (R4 built and ruled; Chelmsford closed;
> the gate now completes and its failure count was wrong). Its §2 (the R3 dials and the two
> counter-intuitive traps), §3 (the standing lesson) and §6 (hazards) are still good and are
> cited from the new file. Kept for that, and as the record of how R3 was signed.

# HANDOFF — 2026-08-27b. **THE SNOW LOOK IS SIGNED. THE NEXT RUNG IS THE GROUND YOU SINK INTO.**

> **LAUNCH LINE (paste into a fresh session):**
> Read `docs/snow_session_handoff_20260827b_NEXT.md`. **The R3 look rung is CLOSED and
> COMMITTED — Chad drove it and signed it.** Do §4 item 1 (build the bank-strip probe)
> before touching anything he reported. `docs/snow_session_handoff_20260827.md` is still
> the authority for MECHANISM and measurement; this file supersedes its §4/§5 work queue.

---

## 1. STATE

| | |
|---|---|
| **Branch** | `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow` |
| **Head** | `e5d89e3f9` — **three commits, ALL COMMITTED, NOT PUSHED.** Base was `f92939e2e`. |
| **Gate** | 186 relevant cases (winter/snow/SF3/tracks/R3/gyro/sled/barren): **182 pass / 4 fail** — the documented pre-existing GI4 sled debt 815/816/854/857. Zero new failures, zero "Not Run". |
| **⚠ FULL GATE** | The 1562-case gate has **NEVER COMPLETED** on this box. Two attempts died partway (test 182, then test 21) because parallel sandbox gates (`enemy-ai`, `seads-recon`) saturate the machine. **RUN IT BEFORE THIS BRANCH GOES NEAR `main`.** |
| **Drive** | Chad drove it repeatedly and signed off: *"okay ... I just drove it its good enough for now"*. |

### The three commits

1. `b8e430415` — R1 the fold, R3 armed, tracks, the rider patch, G1+G2 gyros, and every red-team fix.
2. `d97df1067` — R3 signed: `full_depth = 0.10`, and the black-rock fence split onto its own dial.
3. `e5d89e3f9` — `full_depth 0.10 -> 0.30`: the rocks were cutting straight-edged.

---

## 2. WHAT IS SIGNED, AND MUST NOT BE REOPENED WITHOUT HIM

**Shipped values, both in `render/planet.h`:** `full_depth = 0.30f`, `barren_shed_keep = 1.0f`.
R3 depth-keyed exposure ships **ARMED** (`depth_mix = 1.0f`).

His words, in order, across one evening of driving:

> *"I saw the shatter cones, the black of the barren rocks are still there, good."*
> *"0.10 for the win I see intermediate and the whiter tone is better"*
> *"i like it"*
> *"i like it but the rocks cut kinda straight edged best go to 0.3"*

**The fence, measured at the shipped values** (459,717 land samples, `SEADS_R3_PROBE=1`):

| surface | legacy (what he approved) | **SHIPPED (0.30 + keep 1.0)** |
|---|---|---|
| open ground | 0.992 | **0.985** |
| mapped rock, flat | 0.910 | **0.909** |
| **rock, SLOPED ≥12°** | **0.230** | **0.145** — *blacker than approved* |
| rock, STEEP ≥20° | 0.094 | **0.056** — *blacker* |

⚠ **THE TWO THINGS A LATER AGENT WILL GET WRONG.** Both are written at the dials in
`render/planet.h`; they are repeated here because they are counter-intuitive:

1. **`full_depth = 0.30` is LOW ON PURPOSE and must not be "corrected" upward.** A red team
   correctly found that coverage saturates below the map's median depth (census p50 0.77 m)
   and implied raising it. Chad swept it and went the *other* way. That is coherent: below
   saturation, depth-keyed exposure stops *varying* — and the varying exposure was **fighting
   the R1 fold's own relief**, washing out the hollows and crests the geometry already carries.
   Flat exposure lets the SHAPE do the work. **The intermediate he sees is GEOMETRIC, not tonal.**
2. **Why 0.10 failed and 0.30 works is the SAME mechanism.** Coverage is
   `smoothstep(0, full_depth, depth)`, so the smaller the value the narrower the band over
   which snow fades to rock. At 0.10 that band was thinner than the field's variation across
   a rock edge → near-binary transition → *"the rocks cut kinda straight edged"*. 0.30 widens
   it ~3× and the edge follows the rock again, while still saturating far below p50.

**The fence is on its own dial now.** The FS line is
`snowCover *= (1.0 - max(1.0 - uSnowDepthMix, uBarrenShedKeep) * uBarrenSnowShed * bshed * bgate)`.
`max()`, not a product or sum: `keep = 0` is **exactly** the old expression, `keep = 1` holds the
fence at any mix, and max can never shed harder than one full application. The old fade-only form
is pinned as an **ANTI-needle** in `test_winter_reskin.cpp` — restoring it silently re-welds his
tone ruling to his fence ruling, which is the regression that passes a green build.

---

## 3. ★★★ THE STANDING LESSON FROM THIS SESSION

**An A/B whose arms are indistinguishable is not a measurement.** Chad's first sweep was
worthless — *"I didnt see what the ladder was at"* — because the value lived only in a console
line he could not read while driving. Every verdict from that ride was unattributable. The fix
was to put the number **on screen** and on **PgUp/PgDn**, and only then did his rulings mean
anything. **Any dial you ask him to rule on must be visible and steppable from the seat.**

Corollary, also paid for this session: **put the local fix AND the root fix to him as options.**
When 0.10 broke his black-rock fence, the cheap move was to quietly walk the value back. Offering
him the choice got the root fix (two dials) and an outcome that satisfied *both* of his rulings —
which walking the value back could never have done. This is his standing instruction.

---

## 4. THE WORK QUEUE — IN ORDER

### 1. ⭐ BUILD THE BANK-STRIP PROBE. **DO THIS FIRST. IT IS A MEASUREMENT, NOT A FIX.**

Chad, twice now: *"Sometimes my ski is going down into the road."*

**This is OPEN and currently UNMEASURABLE, and that is the whole problem.** What we know:
- The road **deck** measures **0.0% under** across 9,321 samples.
- Open ground shows drawn-above-driven at 40% of samples but only **6–9 mm** — the known
  facet-interpolation jitter, and the likeliest thing he catches at a grazing ski angle.
- **But `SEADS_VP1_GAP` compares the drive against the MESH**, while the 0–9 m band beside a
  road is drawn by the **BANK STRIPS** (`render/bank_mesh.cpp`). The prior handoff's §3.2b flags
  this caveat about its own probe. **No instrument in this repo measures the drive against
  `bank_mesh`.** The one place he reports a defect is the one place we cannot look.

**Do not tune the banks against a report no instrument can confirm.** Extend the probe to
compare `drive_radius_at` against the bank-strip surface in the 0–9 m band, report
under-percentage and worst-case, then decide.

### 2. §3.2b — THE R2 SHOULDER FORK. **HIS RULING IS NOW HALF-MADE.**

Unprompted, he reported: *"on peoples lawns seems to sink a bit less"*. Measured the same
session (`SEADS_VP1_GAP=1`), drawn-above-driven by distance out from a corridor:

| band | float |
|---|---|
| 20–40 m out | **+0.397 m** |
| 40–80 m out | +0.101 m |
| beyond 80 m | ~0.000 m |

Lawns sit inside the 60 m corridor mask, where the drawn ground floats ~0.4 m above what he
drives on — he sinks into a surface already drawn too high, and it reads as less sink.

**§3.2b's option 3 was "Accept it — 0.42 m in a band with no nearby reference may simply not be
visible." HE SAW IT. Option 3 is DEAD.** Options 1 (extend the bank strips outward to the mask
width) and 2 (full fold, excavate the corridor from the mesh) are still open and **still his
call** — put both to him with costs before building either.

### 3. ⚠ NEW — THE CHELMSFORD PHANTOM ROADS. **UPSTREAM DATA, NOT A CODE DEFECT.**

Chad, this session: *"Some spots in chelmsford say road when it is behind a house and I dont
sink in but I dont mind because you can go fast, I believe what is really there is a field."*

**He is almost certainly right, and the mechanism is understood.** The corridor network is not
authored by us — `world/linework.h` **recovers it from the baked OSM ribbon vertices**
(`render/sudbury_gis.gen.h`, baked by `offline_tool/sudbury_ribbon.py`; see `[ribbons]` in
`config/world.toml:638`). So anything OSM tags as a way behind a house — a driveway, a service
road, a `track`, a farm lane — becomes a **real plowed corridor** with hard-pack, no sink, and a
**60 m mask footprint** suppressing the fold around it. A field with a mis-tagged lane through it
reads exactly as he describes: says road, doesn't sink, fast.

⚠ **This is the SAME root as item 2** — both are the corridor footprint written onto ground that
should be soft. Fixing the shoulder float without filtering the source ways just makes the
phantom corridors *better drawn*.

✅ **CLOSED BY CHAD, 2026-08-27b:** *"I dont care about the mesh as road really, its kind of a
coordor anyways there."* **The map's answer is (b): ACCEPT IT AS MAP TRUTH.** Do not re-bake the
ribbon kinds, do not touch INV-6 for this. The mechanism below is kept because it is the same root
as item 2 — if the shoulder fork is ever built, this is why the corridor footprint is there.

**HE HAD ALREADY DEPRIORITISED IT:** *"I dont mind because you can go fast."* When it comes up, the honest options are (a) filter ribbon
kinds at bake time (driveways/service/track excluded from the *physics* corridor while still
drawn), or (b) accept it as map truth. **(a) is a re-bake and touches INV-6** — the physics
corridor is recovered from the drawn ribbon *by construction*, so excluding a kind from physics
without excluding it from the draw is exactly the fork that invariant exists to forbid. Read
`world/linework.h:19-29` before proposing anything here.

### 4. ✅ R4 — THE READABLE TRACK. **BUILT AND RULED, 2026-08-27b. SEE §7.**

Still the right rung for *"I dont really see any track marks unless I am in the deep stuff"*:
two ski cuts (`ski_width_m 0.135`, ±0.4635 m from `stance_m 0.927`) plus a 0.38 m belt mark,
depth **proportional to local snow depth** (`deform = k * depth_at_when_laid`, k ≈ 0.45).
`world/tracks.h:44` `depress_m` is a flat constant today and stamps carry no depth and no
heading — this needs a per-stamp scalar in the ring, not a dial change.

⚠ **R4 WILL NOT MAKE THE MACHINE SETTLE DEEPER IN DEEPER SNOW, AND HE WILL ASK.** `sim/sled.h:551`:
*"Above the hump the planing operating point is also identical across 0.21..0.30 (sinkage settles
at 0.080 m)"*. The machine rides at ~8 cm at **every** depth that planes, then bogs past ~1.3 m
(`track_clearance_m = 0.255`). That is designed and **Chad-signed** ("median planes, p99 bogs").
The only lever is the planing dials and **they are his to reopen.** Say so when R4 ships.

### 5. UNFLOWN, AND CHEAP TO CLOSE

- **G1 / G2 rotor gyroscopics** — built, tested, pinned OFF-by-default, **never driven by anyone**.
  `k_gyro` and `k_gyro_react` in `sim/sled.h`. Set one to 1.0, rebuild, judge **one at a time**.
- **The night pass.** `winterSurf = max(snowCover, iceCover)` gates the S1b night glow and snow
  sparkle, so armed exposure now also thins night whiteness on scoured ground — and his
  *"snow needs to STAY WHITE"* ruling was made against the OLD mask. He just needs to press `]`.
- **R3 leftover: M2 softened under snow.** The normal map is baked from bare DEM, so snow still
  shades with rock's micro-roughness. `vSnowDepth` is available to the FS, which is exactly what
  a depth-aware softening needs.

### 6. LATER

R5 micro-relief/BRDF (**he said yes**; put the drawn-vs-driven trade to him explicitly first) —
this is what makes *untouched* snow read as snow. Then the roost rungs
(`docs/roost_consult_packet.md`; ⚠ its two P0s about `FxPool` drag and the missing `DrawInfo`
channels still stand).

---

## 5. INSTRUMENTS (all re-runnable; `--smoke N` needs a FRAME COUNT or it just opens a window)

| command | what it measures |
|---|---|
| `SEADS_R3_PROBE=1` | R3 exposure: legacy stub vs the depth field, bucketed by the fence slope. **Now models the surviving barren shed**, so the fence is measurable, not argued |
| `SEADS_R3_SHEDKEEP=<0..1>` | sweeps `barren_shed_keep` in that probe (ship 1.0) |
| `SEADS_R3_FULLDEPTH=<m>` | **seeds** the live saturation depth (PgUp/PgDn steps it in-game); `<= 0` refused |
| `SEADS_SNOWDEPTH=<0..1>` | the R3 A/B at the draw. **`=0` DISARMS; armed is the ship default** |
| `SEADS_VP1_GAP=1` | see-vs-drive gap by distance from a corridor. ⚠ **compares against the MESH, not the bank strips** — see §4.1 |
| `SEADS_BARREN_PROBE=1` | fold added over mapped black rock, by slope — Chad's fence |
| `SEADS_SNOWDUMP=<f>.tsv` | terrain/depth/driven census (`*.tsv` is gitignored now) |
| `SEADS_TRACK_DEMO=1` | lays a deterministic star of track at the sled |
| `seads_sled_probe gyro 1.0` | the G1 A/B |

**In-game:** PgUp/PgDn steps `full_depth` (SHIFT = fine, 0.02 m), with the value on a plate
top-left — hidden at the ship value so it costs a clean screenshot nothing.

---

## 6. HAZARDS THAT COST TIME THIS SESSION

1. ⚠ **THE HEREDOC HAZARD IS REAL AND I WALKED INTO IT.** `bash <<'EOF'` **collapses backslashes**
   in inlined Python, so `"\\n"` arrives as a real newline and every anchor match against a C
   string literal fails. The prior handoff warns about this and I hit it anyway. **Write edit
   scripts to a FILE and run the file.**
2. ⚠ **NEVER PIPE A GATE THROUGH `tail`.** The first full gate died at test 182 and reported
   `exit 0` because the exit code came from `tail`, not `ctest`. Redirect to a file, then check
   the summary line, the `Not Run` count, AND the real exit code.
3. ⚠ **Two ctest gates must never share `build/`** — and on this box, other sandboxes' gates will
   starve yours. Check for competing `ctest.exe` / `seads_tests.exe` before trusting a timing.
4. `--smoke` **requires a frame count** (`--smoke 30`). Without one it silently opens a normal
   window and looks like a hang.
5. **Comments that cite `file:line` rot the moment you insert above them.** Nine citations written
   in one session were already wrong by the end of it. Re-verify any line number you quote.
