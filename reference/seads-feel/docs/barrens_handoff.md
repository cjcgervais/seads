# Barrens layer — handoff (2026-08-10, after the winter S2 merge)

Launch line for the next session:

> Read `docs/barrens_handoff.md`; do §5 item 1.

**§5 item 1 (merge winter-S2) is DONE** — see §5.0. The next rung is now the
PNG-emission fold, which has been promoted to §5 item 1.

---

## 1. Where the work is

| | |
|---|---|
| Worktree | `D:/seads_sandboxes/barrens` |
| Branch | `sandbox/barrens-layer` @ `86cfef85c` |
| Base | `sandbox/winter-S0` @ `976b685da` — **NOT main**, see §3.1 |
| Merged in | `sandbox/winter-S2` @ `b06eb0a3e` (S2 + S2b) |
| Pushed | **No.** Backup ref `backup/barrens-pre-rebase-20260810` |
| Tree | clean |

Commits, newest first:

```
86cfef85c  barrens: fold the duplicate sudbury_cones.png read the merge created
960ed4ec0  barrens: merge winter S2 + S2b — the absorb arrives, the stub STAYS
0027c2c3c  barrens: retract the station/junction count claim
336f88687  barrens: handoff refresh — winter-S2 tip moved
49c5d605c  barrens: handoff doc — state, corrections, S2b dependency
7cd85cdf0  barrens: regen the code graph for B2
96160a557  barrens B2: the 1970 footprint drives albedo and tree density
6c78b05b7  barrens: mark the snow term PROVISIONAL - WINTER_LAW 3.2 owns exposure
52a5b6dd6  barrens: retune for GROUND-LEVEL riding
75ceac33f  barrens: B5 shatter-cone detail shader
78e27daef  barrens: shock-fabric channel + standalone equirect bake (Chad ruled C)
8da075fa1  barrens: fold the Opus red team
9b2a9584e  barrens: the 1970 industrial-barrens hero layer (B1)
```

## 2. Gate state at HEAD

| Gate | Result |
|---|---|
| `ctest` | **1064/1064** (union of barrens 1044 + winter 1063) |
| `accept_map.py` | **16/16**, 0 skipped (re-run post-merge) |
| `accept_barrens.py` | **10/10 PASS** |
| `graphify.py --check-only` | layer check OK |
| `projection.lock` | **LOCKED** carried, hash `0x0D1DCE234E86034EULL` |
| `flown_bake_id` | `20260809T200807` pinned; `bake_id` `20260810T155419` |
| Asset partition | **HELD** (see §4) |

## 3. Three things a future session must not re-learn

### 3.1 Rebase onto the winter tip, NEVER onto main

The `carry_status` fix that lets `projection.lock` hold `LOCKED` across a re-bake
lives on the winter branch. **It is not on main.** On a main base,
`build_sudbury.py` writes `status=PROVISIONAL` unconditionally, which silently
drops Chad's fly-approval while holding cryptographic proof that the projection
is unchanged. If you see PROVISIONAL after a bake, check your BASE before you
conclude you moved a projection parameter.

### 3.2 The flown bake is 4 m, not 16 m

`source_pins.toml` names `dem_blend_4m.tif` as "THE FLOWN TERRAIN, bake_id
20260809T200807". The shipped bake is **N = 24000**. `bake_barrens_assets.py`
runs at 16 m (N = 6000) — both decimate to the same 32 m work grid, so the field
agrees, but anything sized off N does not.

### 3.3 The field upsample must stay row-chunked

`sudbury_barrens.upsample_to_grid()` replaced an `np.meshgrid` form that
allocates a `(2, N, N)` coordinate array: 288 MB at 16 m, **4.6 GiB at 4 m**,
next to the 6.4 GiB albedo array on a 28.8 GiB box. Verified bit-identical to
the meshgrid form across four grid/chunk configurations. Do not "simplify" it
back.

## 4. The strongest gate — re-run it after ANY bake change

At `BARREN_GAIN = 1`, these must stay **byte-identical** to the flown manifest:

- `assets/sudbury_dem.png`
- `assets/sudbury_landmask.png`
- `assets/sudbury_normal.png`
- `assets/sudbury_buildings.bin`

and only these may move: `sudbury_color.png`, `sudbury_treedensity.png`,
`render/sudbury_gis.gen.h`.

Corroborating signals in the bake log: **remap drift 0.000 m** and **relief
taper exactly 1.000 inside 22 km**. Both held on `20260810T155419`. If either
moves, the layer has started writing into `elev` — it must only ever read it.

## 5. Next rungs, in order

### 0. DONE — `sandbox/winter-S2` is merged (2026-08-10, `960ed4ec0` + `86cfef85c`)

Gate after: **ctest 1064/1064**, `accept_map` **16/16 (0 skipped)**,
`accept_barrens` **10/10**, graphify layer check OK, `projection.lock` still
**LOCKED**, same hash `0x0D1DCE234E86034EULL`, same `bake_id` — no bake input
moved, so §4's byte-identical asset gate never came into play.

Conflicts were confined to the three **generated** graph artifacts — both sides
had regenerated them. Resolved by regeneration, never by hand.
`render/planet.cpp` auto-merged and was hand-verified to carry both terms.

**The stub stayed, and the licence to delete it is no longer a name.**

This doc predicted the stub would retire at "S2b" = the winter side's
per-vertex depth attribute. A commit titled `winter S2b` then landed
(`b06eb0a3e`) that is a *different thing entirely* — Chad's three fly findings
(trees off the trail, corduroy turned, depth readable). The delete licence
looked payable **on the title**. It was not: re-verified against the real tip,
their fragment still computes cover from sun-angle and slope with no barren
term, and the stub is the only thing preventing the hero layer from being
whitened toward `snow_albedo` 0.92 in the only season the game has.

**★ THE RETIRE CONDITION IS NOW A CODE CONDITION, NOT A RUNG NAME.** The winter
side has retired `S2b` as an identifier outright — the remaining work is
`S2-FRAG`, `S2-OVERLAY`, `S2-WIDTH`. Do not accept any commit title as
authority to delete the stub. **Grep instead:**

```sh
# The stub retires ONLY when the planet FRAGMENT consumes an interpolated depth
# attribute originating from world::SnowpackField::ambient_depth_at.
grep -rn "ambient_depth_at" render/planet.cpp
grep -n  "snowCover =" render/planet.cpp
```

Measured 2026-08-10, post-merge: **zero** hits in `render/planet.cpp`
(`ambient_depth_at` lives only at `world/snowpack.h:159`), and the fragment
still reads `snowCover = uSeasonSnow * (1.0 - water) * snowFlat`. **⇒ the stub
is load-bearing.** If the fragment still derives cover from `uSeasonSnow` and
slope, the stub stays — whatever any commit, rung, or doc calls itself.

**Also folded (`86cfef85c`):** the merge created two independent `LoadImage`
reads of `sudbury_cones.png` — ours for the B5 fabric cubemap (R), theirs for
the CPU barren raster (G). Neither branch could see it alone. Now one decode,
two consumers, with the G extraction taken *before* the GPU upload and
independent of it — folding it after `rlLoadTextureCubemap` would have made a
**sim** quantity (`depth`, feeding `drive_surface`) conditional on a cubemap
upload succeeding.

**Two corrections to things this doc asserted:**

- It said the linework station/junction counts **would move** post-merge
  because B2 moved `render/sudbury_gis.gen.h`. Measured: **50302 / 8997 —
  identical** to the winter side's pre-merge figures. They come from the 8
  baked path assets, not from the gis header, so B2 could never have moved
  them. Harmless either way; nothing asserts them. The standing rule is
  unchanged: any linework count leg must be a **floor**, never an equality.
- Winter S2 has now been **flown and signed** (depth 0.77 m), so §5.3's
  "fly S2 clean first" precondition is discharged — the barrens fly is
  unblocked and still wants to be **its own fly**.

The binding was verified **empirically, not by reading the chain** — which is
the lesson their S2 taught, when a green suite sat over an unbound input:

```
WINTER S2: barren field loaded (8192x4096, G of sudbury_cones.png)
```

### 0b. DONE — the PNG emission is folded and CERTIFIED (`ff3d49da9` + `e9c2b6560`)

Both PNGs are emitted by `sudbury_bake._emit_barren_assets` at step 5c-bis, off
the same `barren` array the albedo and tree density consume, and both are now in
`bake_manifest.txt` under a `bake_id`. Certified by a full 4 m re-bake:
**ctest 1064/1064, accept_map 16/16 (0 skipped), accept_barrens 10/10**, C++
manifest gate 40 assertions, `projection.lock` carried **LOCKED** at the same
hash. §4's byte-identical gate passed — and `color`/`treedensity` were unmoved
too, though permitted to move, which *proves* the emitted copy is invisible to
the albedo and tree consumers.

**It was hiding a real defect.** The standalone never passed `landmask`;
`build()` does. At matched 4 m resolution, before → after:

| | shipped 4 m | now |
|---|---|---|
| full-water texels carrying barren | 398,442 (max 241/255) | **0** |
| shore texels carrying barren | 99,116 | 53,080 |
| surviving depth cut (mean/p99/max) | 6.53 / 32 / 70% | 5.94 / 24.4 / 51% |

★ **The remaining shore signal is CORRECT — do not "finish the job" by zeroing
it.** A texel that is 40% lake and 60% blackened rock *should* carry barren
proportional to its land; the snowpack's ice lerp handles the water share.

★ **Do not remove the post-blur `pf[landmask] = 0.0`** in `_prefilter_resample`.
`build_barren_field` zeroes water, but the 48 m sphere blur bleeds it straight
back — measured, 135,393 texels returned to 133/255 *with* the zeroing in place.
This follows the tree density's own precedent (hard discrete constraints are
re-applied after the blur; continuous gradients are not) and cannot re-sharpen
the land-side footprint, because it only clears the water boundary in the
emitted copy.

★ **`_equirect_coords` MUST keep returning the dirs.** Step 6b needs `dx,dy,dz`
for the cos-lat solid-angle weight and the direction-keyed fBm. Dropping them
cost a 100-minute bake to a `NameError`.

★ **Order in `_emit_barren_assets` is a MEMORY constraint, not style.** Blur and
resample `barren` first, build `cone` after. Building the cone up front parks a
second 2.3 GiB array through `_sphere_metric_blur` (which holds ~6× the field,
beside `rgb`'s 6.4 GiB) and got the 4 m bake **OOM-killed**.

### 1. Fly the layer — on its own fly  ← NEXT

---

<details>
<summary>Original item 1, kept for its reasoning (now discharged)</summary>

### Merge `sandbox/winter-S2` — but DO NOT delete the shader stub yet

`sandbox/winter-S2` (@ `40c6d5976`) pays half the owed pair: the absorb
`depth *= (1 - k_barren*barren)`, `k_barren = 0.75`, in
`world/snowpack.cpp::ambient_depth_at`, fed through `world/raster.h` from the G
channel of `sudbury_cones.png`. Merge verified clean (shared base `976b685da`,
zero conflict markers).

`40c6d5976` is worth knowing about: their absorb was implemented *and had a leg
pinning it*, but nothing ever set `SnowpackField::barren` — a green suite over an
unbound input. They found it by testing against this branch's actual
`sudbury_cones.png` rather than reasoning about it, and absence is now a
supported state (no branch in `f()`), not a degraded one.

**After merging, the linework station/junction TraceLog counts will change** —
their figures (50,302 / 8,997) were read against the pre-B2 header and this
branch moved `render/sudbury_gis.gen.h`. **Expect that; it is not a regression.**

An earlier revision of this doc said an S2b acceptance leg keys off those counts.
**That was wrong** — it was this session over-reading their note, which said only
that the counts need re-measuring. Verified on their branch: nothing asserts
them. They live in a startup TraceLog line and in prose; the tests build their
own synthetic ribbons and assert `!net.junctions.empty()`, a non-emptiness check,
not a count. A truncated header is already caught by S0/D5's
`trail_coverage_floor`, which measures baked km.

The instinct behind the error is still worth holding, and the winter side has
recorded it as an S2b constraint: **any linework count leg must be a FLOOR, never
an equality** — otherwise a legitimate re-bake reds it for bookkeeping.

**★ CORRECTION TO A CLAIM IN CIRCULATION.** The winter side's note says the
PROVISIONAL stub "can be deleted the moment the branches merge — nothing needs
connecting first." **That is wrong, and deleting it on merge is a visible
regression.** Verified on their branch:

- their absorb changes **`depth`** — a sim-side quantity feeding
  `drive_surface = terrain + depth`;
- their `render/planet.cpp` has **no barren term in the albedo path at all**
  (`snowCover = uSeasonSnow * (1-water) * snowFlat` and straight into
  `mix(albedo, uSnowAlbedo, snowCover)`);
- their own `app/main.cpp:1245` notes the barren raster is not in their tree, so
  their term is off by data.

The stub at `render/planet.cpp:~221`
(`snowCover *= (1.0 - uBarrenSnowShed * barren);`) is the **only** thing keeping
the barrens from being whitened toward `snow_albedo` 0.92 in the render. Delete
it on merge and the hero layer is inverted and erased in the exact season the
game is set in — which is the failure the G channel was created to prevent.

**The stub retires at S2b, not at S2.** S2b is the winter side's per-vertex
terrain attribute through the mesh `texcoords` slot, which gives the fragment a
real depth to derive exposure from. Their reason for routing it that way is
sound and worth preserving: the planet FS has no height texture, so a GLSL twin
of `f()` would be a second implementation of the exposure field — the exact fork
§6c.1 exists to prevent.

</details>

### (detail for item 1) Fold the PNG emission into `build()`

`sudbury_barrens.png` and `sudbury_cones.png` are still written by the standalone
`bake_barrens_assets.py`. They carry **no `bake_id`** and are **not in
`bake_manifest.txt`**, so a stale one is undetectable by every gate in the tree.
Both verified byte-reproducible on 2026-08-10, so nothing is wrong today.

The cost of the gap rose at winter S2: before it, `barren` drove shading only, so
a stale raster was an art bug you could see. Now it multiplies `depth`, so a
stale `cones.png` silently **moves the ground** the sled rides — on the one asset
class no manifest covers.

Work involved: emit both at step 6 of `build()` (the resample coords already
exist), add both to `_write_bake_manifest`, and extend the manifest acceptance
leg. Memory is not a concern at 16 m and is ~2 × 2.3 GiB at 4 m — resample early
and free, do not hold them to the end.

### 2. Fly the layer — on its own fly, NOT bundled with the winter S2 fly

**Status: unblocked.** Winter S2 has been flown and signed (depth 0.77 m), so
the bundling hazard below is moot — but the "own fly" ruling stands, because
until a real per-vertex depth reaches the fragment the render still shows the
PROVISIONAL stub's exposure while the sim uses the real absorb.

The winter side offered to merge this branch before their S2 fly so black rock
and snow could be seen interacting. **Declined**, for the reason S2's
own fly existed: the one ruling they needed was whether 0.77 m typical depth and the
crest-to-valley spread feel right, and the barren shed is a second, independent
depth modifier over the same terrain (crests ~75% shed vs valleys ~22%). Bundled,
a "too much contrast" verdict cannot be attributed to `curv_gain` or to
`k_barren`. Worse, until S2b the render would show the PROVISIONAL stub's
exposure while the sim used the real absorb — two different notions of the same
field visible at once, in the exact fly meant to judge that field.

Fly S2 clean, then fly the barrens on their own.

Never seen in the air. Two specific unknowns:

- **The shatter cones have never been observed.** They live inside 420 m and the
  smoke harness spawns at 2500 m. GLSL compiles and links under a real GL
  context, and the mask/gating is unit-tested, but "it renders" is not "it reads".
- **The barrens themselves have only been checked as rasters and previews.** The
  design claim is that the read comes from *inverted polarity* (crests dark where
  `PAL_HIGHROCK` 206 makes them bright everywhere else) plus the total absence of
  trees — not from absolute darkness. That is a claim about flight, and only a
  fly settles it.

Winter helps here rather than hurting: §6c.5's `PAL_BARREN` re-derivation is
discharged, because frozen lakes are bright ice (0.78) against land snow (0.92),
so the summer dark-rock-vs-dark-water hazard inverts and dissolves. Dark barrens
now sit against a white world, so the contrast budget is larger, not smaller.
**Do not re-tune `PAL_BARREN` (42,40,37).**

## 6. Environment, and the traps that cost time

```sh
# Python — `python` on PATH is a hermes venv with NO numpy.
D:/flight_sim2/seads/offline_tool/.venv/Scripts/python.exe        # has shapely/rasterio/scipy
# run with cwd INSIDE this tree, and verify module resolution:
#   python -c "import sudbury_bake as K; print(K.__file__)"

# Build — needs Debug (SPEC 6.1 assert-live); a Release tree fails by #error.
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
      -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/seads_sandboxes/world-dem/build/_deps/raylib-src
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure

# Acceptance
python offline_tool/accept_map.py        # 16 legs, mutation-verified
python offline_tool/accept_barrens.py    # 10 legs
python tools/graph/graphify.py           # FULL regen; --stale does NOT check digests
```

★ **VALIDATE AT 16 m BEFORE EVERY 4 m BAKE.** `SEADS_SOURCE_RES=16
SEADS_SKIP_ACCEPT=1 python build_sudbury.py` runs the identical `build()` path
in ~10 minutes instead of ~100. This is not optional caution: a one-line scope
slip cost a full 100-minute 4 m bake, and two 16 m passes then caught both a
`NameError` and a water-bleed bug for a fifth of that. Restore afterwards with
`git checkout -- assets render/sudbury_gis.gen.h` — every bake output is tracked,
so any bake is revertible.

★ **An empty stderr with a truncated log is an OOM KILL, not a hang.** A Python
`MemoryError` leaves a traceback; the OS killing you leaves nothing. Check
`Get-CimInstance Win32_PageFileUsage` (peak hit 34 GB on a 29.5 GB box). The 4 m
bake peaks near the ceiling, so any new (N,N) float32 — 2.3 GiB each — must be
built late and freed immediately.

★ **`pyflakes` is installed in the venv and worth one second before any bake:**
`python -m pyflakes offline_tool/*.py`. `ast.parse` CANNOT see an undefined
name. Known false positive: `undefined name 'base'` in `sudbury_bake.py` — it is
a closure subscript store that pyflakes mis-scopes because of a later `del`.

**Long-running jobs exceed the 10-minute agent cap and get killed mid-run.** The
4 m bake takes ~100 min (not 50) and `accept_map.py` ~30 min. Launch them
detached and poll a log instead:

```powershell
Start-Process -FilePath "D:/flight_sim2/seads/offline_tool/.venv/Scripts/python.exe" `
  -ArgumentList "-u","build_sudbury.py" `
  -WorkingDirectory "D:/seads_sandboxes/barrens/offline_tool" `
  -RedirectStandardOutput bake.log -RedirectStandardError bake.err -PassThru
```

`-u` matters: without it Python buffers stdout to a redirected file and you see
nothing until it exits — including on a crash.

**Do not read liveness off the PID `Start-Process -PassThru` hands you.** The
venv `python.exe` is a **shim that re-execs** the real interpreter
(`...\Python311\python.exe`) as a *child*. The PID you get back sits at ~4 MB
and 0 CPU for the whole run while the child does the work, which reads exactly
like a stalled or dead job — it is neither. Find the worker with
`Get-CimInstance Win32_Process -Filter "Name='python.exe'"` and match on
`ParentProcessId`. Judge progress by CPU delta and working set (the real one
climbs past 12 GB), not by log growth: `accept_map`'s **leg 2**
(`dem_amplitude_ring`, a full per-pixel replay of the theatrical encoding) runs
**~25 min printing nothing**. Whole run is ~30 min, well past the doc's earlier
"exceeds 10 min".

**Write Windows paths with FORWARD slashes**, here and in every doc and log line
this tree emits. PowerShell and CMake both accept them. Backslash paths carry a
live escape hazard: `D:\flight_sim2` contains `\f`, which silently eats itself
into a **formfeed** whenever a path goes through a tool that interprets escapes
— the path then prints as `D:` + a blank gap and the reader cannot see why. The
winter side hit exactly this. `\t`, `\n`, `\b`, `\r` and `\v` are the same trap
waiting on other paths.

**Other traps.** `offline_tool/source/` is gitignored, ~2 GB, and must be copied
with `cp -a` (a plain `cp` restamps mtimes and fakes freshness past the
`source_freshness` leg). A non-ASCII character in a `TEST_CASE` name makes the
case silently never run — `gate.sh` has a tripwire; do not remove it.
`WINTER_LAW.md` / `winter_plan.json` are UTF-8; `bake_manifest.txt` is not
strictly UTF-8, so read it with `errors="replace"`.

## 7. Standing rulings — do not reopen

- **`BARREN_CONE_FRAC = 0.85`** and the shatter-cone licence are Chad's option C
  ("if its cheap and awesome put em in the barrens too"). Cones on the Copper
  Cliff barrens are deliberate. **Never file this as a bug.** `0.0` reverts to
  documented-localities-only without touching anything else.
- **`PAL_BARREN = (42, 40, 37)`** — discharged, see §5.3.
- **INV-2**: no bake-side height term, anywhere. The layer reads `elev`; it never
  writes it.
- **Roads render plowed and tunnels stay black** — separate own-shader geometry,
  locked by tests. Do not route a barren term into either.
- **Commit by explicit path.** Never `git add -A` (this tree has cross-agent
  files in it).

## 8. Numbers worth having

| | |
|---|---|
| Zone area (barren) | 19,491 ha vs City's **19,525 ha** |
| Zone area (semi-barren) | 83,576 ha vs City's **83,725 ha** |
| Field > 0.5 | 10,899 ha @ 4 m · 10,966 ha @ 16 m |
| After town suppression | 9,033 ha (200 m blur held back 1,866 ha, 17%) |
| Crest vs valley | 0.456 vs 0.349 = **1.31×** (Freedman & Hutchinson) |
| Dose monotonic | 0.480 → 0.202 → 0.010 → 0.000 |

The zone area and the field area are **different quantities** and are easy to
confuse. The field is the product `zone × dose × topo`, so valley refugia sit
near `TOPO_FLOOR` 0.30 and fall below the 0.5 mark by design. The ~56% ratio is
the "continuous field, not a decal" law working, not coverage loss.

For the winter side's ramp math: crests ~1.0 shed ~75% of depth while valley
refugia ~0.30 shed ~22%, across ~400 m of local relief. That crest-to-valley
spread is a larger depth-gradient source than the snowbank is.
