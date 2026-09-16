# Barrens — START HERE (2026-08-11, after three fly findings)

> **UPDATE (later 2026-08-11):** sparkle LANDED (`2be110a75`), and after two
> more flies the shed is now **SLOPE-GATED at the MACRO scale**
> (`2c979901d`): flat barrens rewinter (floor 0.10), the black lives on
> macro faces ≥ ~12°, gate keyed on the MESH normal (never the 4 m normal
> map — its per-texel slopes are 17–19° micro-roughness even on flat
> landform, the root cause of fly-2's "flats still dark, faces not black").
> Dials: `[snowpack] barren_slope_lo_deg 8 / hi_deg 12 / flat_shed_frac
> 0.10`. RockOutcrop + ski sparks now live on sloped rock. Winter side
> notified in their packet's reply addendum.
>
> **Fly 3 (`156492aa9`, gate 1075/1075):** the "triangulated" transition was
> fragNormal interpolating across ~59 m triangles — the gate's macro normal
> is now a mip-smoothed `textureLod(normalCube, …, ~90 m)` sample (string-pin
> guards the revert); ramp tightened to 8–12°; and a render-only FACE-DARK
> term (`[ground] barren_face_dark 0.65 / _hi_deg 20`) chains off the shed's
> 12° so faces ≥20° render ~0.05 albedo void-black. Depth law untouched.
>
> **Fly 4 (`c2f3a1baa` + certify `352382632`, bake `20260811T175353`):**
> face_dark → **0.85** (~0.02 true black); bake tree-kill now the saturating
> shed curve (barrens canopy-free, 18×/54× measured — treedensity moved,
> winter side notified, their collision follows); rock-value MOTTLE baked
> into the cores (32–128 m, band-pass +52%); `cone_value()` carves the
> shatter-cone striae into face-dark within 420 m (normal-only detail was
> invisible on black — that's why the cones were never seen). accept_barrens
> now **12 legs** (tree-kill + mottle, both mutation-tested). Lock carried
> LOCKED, accept_map 16/16, ctest 1075/1075.
>
> **Fly 5 (`b61f2d9ef`, runtime-only — Chad ruled NO new bake):** face_dark
> 0.95, and new `[ground] barren_face_mottle 0.6` — the baked mottle's bright
> patches RESIST face-dark (a flat multiply crushes the mottle's absolute
> range to 1–3 DN, which is why steep faces read smooth shiny black). The
> steepest rock is now the most mottled: charcoal patches over ~0.008
> void-black. Shader literals 0.095/0.126 mirror C.BARREN_MOTTLE_LO/HI ×
> PAL_BARREN — re-derive if a future bake cranks the mottle
> (BARREN_MOTTLE_AMP is the far-field counterpart).

Launch line:

> Read `docs/barrens_next_session.md`; do §3 item 1.

**Read `docs/barrens_handoff.md` too** — it holds the state, the gate commands,
the environment traps, and the standing rulings. This file is the *current
thread*: what Chad flew, what was ruled, and what is owed.

---

## 1. Where things are

| | |
|---|---|
| Worktree | `D:/seads_sandboxes/barrens` |
| Branch | `sandbox/barrens-layer` @ `021103d3e` |
| Base | `sandbox/winter-S0` @ `976b685da` — **NOT main** (`barrens_handoff.md` §3.1) |
| Merged in | `sandbox/winter-S2` @ `b06eb0a3e` (S2 + S2b) |
| Pushed | **No.** Backup ref `backup/barrens-pre-rebase-20260810` |
| Tree | clean |
| Gate | ctest **1064/1064** · accept_map **16/16** · accept_barrens **10/10** · layer check OK · `projection.lock` **LOCKED** `0x0D1DCE234E86034EULL` |

Session commits, newest first:

```
021103d3e  two fly findings — barrens black by day, snow white at night
14c8bd3d6  handoff — fold certified + four rulings to inherit
e9c2b6560  CERTIFY the fold — 4 m re-bake, 16/16, water contamination gone
ff3d49da9  fold the PNG emission into build() — and it was hiding a shore bug
17687735a  make the stub's delete licence a GREP, not a rung name
86cfef85c  fold the duplicate sudbury_cones.png read the merge created
960ed4ec0  merge winter S2 + S2b — the absorb arrives, the stub STAYS
```

## 2. What Chad flew, and what was ruled

He flew the certified layer and reported **three** things. A Fable 5 consult
ruled on all three — packet at `docs/consult_barrens_too_light.md`, and its
verdicts are quoted in the commit messages rather than only here.

### 2.1 "Reads too light" — FIXED, awaiting his fly (`021103d3e`)

`barren` is a COVERAGE product, median 0.212 on land, but both consumers
multiplied it linearly — so the shed removed ~16% of snow typically and albedo
bottomed at 0.318 even at `b = 1.0`. The layer could not produce its own hero
colour.

Fix: `depth *= (1 - k_barren * smoothstep(0.10, 0.60, b))`, `k_barren` 0.75 → 1.0,
applied identically in `world/snowpack.cpp` and the planet FS, single-sourced
through `render::set_barren_shed_law` (defaulted from `world::SnowParams`).

★ **The proof it was broken: `RockOutcrop` was dead code.** The class needs
`barren > 0.5 AND depth < 0.10`, but the old shed floor was `depth × 0.25` =
0.19 m — unreachable. The winter side's "crests shed ~75%" assumed crest barren
≈ 1.0; it is 0.212 typical.

### 2.2 "Snow is mid grey at night" — FIXED, awaiting his fly (`021103d3e`)

`night_glow` [0.70,0.75,0.84] → **[0.95,1.02,1.14]** (×1.357, cool ratio exact).
Config-only.

★ **Why the old value certified as white:** the A/B table in `world.toml` was
measured on a frame *with moon fill* (~+0.21 lum), and `[moon] fill_lo = 0.45`
zeroes the fill for about half the lunar cycle. **Re-run that A/B at moon phase
< `fill_lo`, never above it.** Note `winter_snow_cover` 0.85 enters the glow
twice (inside `albLum` and again as `winterSurf`), capping it at 72% of its dial.

### 2.3 Highway rock-cuts — ★ BUILT (RC1) THEN KILLED BY CHAD'S FLY 2026-08-12

RC1 was built to spec (commit `286b64d33`: DEM-probed kind-6 walls, 10.0 km,
gate all green) and REVERTED the same night on Chad's fly verdict:

> "they are in the wrong place and are doubled up, there is no hillside to
> meet the cuts and they look man made — lets forget the cuts."

**Do not rebuild this from the same recipe.** The failures a future attempt
must solve first, diagnosed from his three observations:

1. **Wrong place / doubled**: the probe emitted LEFT and RIGHT walls
   independently at a fixed 13 m offset — a single-side cut with a high far
   ridge produced walls BOTH sides (the doubling), and the fixed offset does
   not sit where the real face is (8–26 m spread was measured, then thrown
   away by the constant).
2. **No hillside to meet the cuts**: the wall reconstructs relief the mesh
   blur erased, so it rises out of terrain that stays FLAT behind it — a
   free-standing fin, not a cut through a hill. The honest fix is terrain-
   side (sharpen the mesh near cuts), which collides with INV-2 and the
   whole conditioning law — a much bigger rung than a ribbon sibling.
3. **Man-made look**: 20 m stations + smoothed constant-offset top = an
   extruded fence. Real cuts are ragged in plan AND profile.

The full recipe/results are recoverable at `286b64d33` (spec was
`docs/rockcut_spec.md` in that commit). The revert is `d7d81eb2b` and keeps
everything else: shed law, sparkle, night glow, the certified bake.

## 3. Next rungs, in order

### 1. ★ FLY the two fixes — Chad, before anything else is built

Nothing else should land until he rules on these, because everything below
changes the same read. **Judge day and night separately**; they were fixed for
different reasons and Fable confirmed they do not fight (both increase
separation).

**Fly checklist:**
- **Day, barren core** (Copper Cliff / Coniston / Falconbridge): should now read
  near-black rock, not grey. The *graded halo* around each lobe must survive —
  if the edge looks like a decal, `barren_shed_lo` is too high.
- **Day, off-barren ground**: must be *unchanged*. The signed 0.77 m ambient is
  untouched outside the barrens by construction; if snow depth feels different
  anywhere else, something is wrong beyond this change.
- **Night, moonless or thin moon** (this is the frame that matters — a full moon
  hides the bug): open snowfield from a low hover should read **white**.
- **Night, same field from 1–2 km**: if it greys with altitude, that indicts
  `sky_aerial`'s night floor, **not** the glow — report it that way.
- **Night, barren core**: should be truly black against the white, halo grading
  smoothly between.
- **Night, lake crossing**: ice must still read a step darker than shore snow.
- **Riding onto a barren core**: ambient depth now goes to ~0 there (bare rock).
  That is intended per §6c.2 — confirm it feels right rather than abrupt.

### 2. Snow sparkle — ★ LANDED `2be110a75` (2026-08-11), awaiting the same fly

Built exactly to the spec below (Sonnet to the Fable spec, supervised; gate
1067/1067). Two `[ground]` dials: `snow_sparkle = 0.4` (0 = off),
`snow_sparkle_sharp = 120`. Add to the fly: night snowfield in motion should
glitter in moonlight AND on a moonless starlit night (the kStar term); sparkle
must die out by ~600 m and never shimmer at range; bare barren cores must NOT
sparkle. Original spec kept for reference:

He said snow "appears to sparkle" in moon *and starlight*. Fable specified it
concretely; the spec is in the consult result and worth re-reading in full
before implementing. Essentials:

- planet-local cell lattice `floor(normalize(fragDir) * ~3.5e4)` (≈0.4 m cells),
  sparse hosts at `p_hash13 > 0.94`;
- per-cell jittered facet normal, `pow(dot(reflect(...), -viewDir), ~120)` — eye
  motion animates it, **no clock**, same determinism rule as `water_stars`;
- gate `nightAmt × winterSurf × (kStar + uMoonFill × ndlMoon)` with
  `kStar ≈ 0.15` — **the additive star term is load-bearing**: he explicitly
  wants starlight sparkle, so it must not be moon-gated;
- ★ anti-moire, the three rules that got `water_stars` through review: soft
  smoothstep falloff (never a hard point); compute the dot and its `fwidth` in
  **uniform control flow** before any branch (the GLSL-UB lesson at
  `planet.cpp:350–355`); distance-fade to zero by ~400–600 m. This is what the
  deleted road mottle lacked.
- Apply additive, clamped, peak ~0.4, in the land path scaled by `(1-water)`.

Runtime-only. Two new `[winter]` dials.

### 3. Highway rock-cuts (finding 2.3) — ★ KILLED by the fly, see §2.3 (design below kept for the record only)

★ **OSM is a dead end, measured:** `roads_raw.json` has 6,500 ways / 2,703 km
and **zero** `cutting` or `embankment` tags. Do not build a tag path.

★ **The DEM finds them, measured** on the cached 4 m source: probing 562 km of
major highway at 20 m stations with side probes at 8–26 m, criterion
`side − grade > 3 m` → **48.7 km flagged (9.3%)**, face heights p50 4.1 m, p90
6.3 m, max 12.0 m, **14.0 km above 5 m**. Probe script:
`…/scratchpad/rockcut_probe.py`.

Design (Fable):
- a new `LineKind::RockCut` sibling in the ribbon system, **its own shader**,
  `is_corridor() == false` (the `Waterway` precedent) so linework physics and
  the snowbank logic are untouched;
- **extruded faces, not albedo** — Chad's complaint is explicitly topological, so
  a dark stripe on blurred terrain is a second "too light" waiting to happen.
  Base at `facet_radius_at` (how ribbons drape today), **top at the true crest
  from the unblurred 4 m DEM**, carried as a per-station attribute: the wall
  reconstructs the relief the 70 m mesh blur erased;
- ★ **clamp face height to (true crest − rendered facet)**, never the full
  grade-to-crest measurement, or walls float above the terrain they sit on;
- **its own channel, NOT the `barren` field.** Blast-face and smelter-kill are
  different physical classes; folding them would fire `RockOutcrop` along
  highway shoulders and strip trees in bands. `shed = 1` unconditionally in its
  own shader, with snow ledges on the horizontal benches — that is what makes it
  read as topology. No night glow on the faces (§2.5 dark verticals).
- ★ **Integrate as a bake stage — do NOT write a standalone emitter.** That is
  exactly the manifest-less drift `ff3d49da9` just cleaned up.
- Start with the **≥5 m core (14 km)**, not all 48.7 km — knee-high berms dilute
  the read. Widen only after a fly.
- Known hazard to disclose: the road is not notched into the rock (INV-2 forbids
  writing `elev`), so deep cuts read as walls flanking a road on a hump from
  some ground angles. Correct from the air. Second hazard: 48.7 km of wall the
  sled can drive through unless collision is wired.

### 4. Small, while nearby

- **Twilight dead band** (Fable F1): `nightAmt` reaches 1.0 only ~5.7° below the
  horizon while the sun's diffuse dies at 0°, so snow dips to ~0.27 in between —
  a guaranteed grey-snow window every 1.5 h cycle. Tighten the lower edge to
  ~−0.03 so the glow arrives as the sun leaves.
- `[moon] fill_lo = 0.45`: leave it. If Chad reports "some nights are greyer
  than others" *after* the glow raise, this knee is the answer, not the glow.

## 4. Corrections carried forward — do not re-derive

- ★ **The stub's delete licence is a GREP, not a rung name.** `S2b` is retired as
  an identifier (the winter side's remaining work is `S2-FRAG`, `S2-OVERLAY`,
  `S2-WIDTH`). The PROVISIONAL stub in `render/planet.cpp` retires **only** when
  the planet fragment consumes an interpolated depth attribute originating from
  `world::SnowpackField::ambient_depth_at`. Verify with
  `grep -rn "ambient_depth_at" render/planet.cpp` — zero hits today ⇒ **the stub
  is load-bearing**. No commit title is authority to delete it.
- ★ **The shore-feather residual is NOT "correct"** — I called it that and Fable
  showed it is a *double discount*: the ice lerp already weights the land branch
  by `(1-wfrac)`, and the emitted `b` is area-weighted, so the land branch is
  discounted twice. Small and conservative (extra snow at shorelines; 66,815
  texels, mean 6.4%). Proper fix if ever wanted: mask-normalized blur
  (`blur(field·land)/blur(land)`) then the re-clear. **Do not describe the
  current semantics as correct.**
- ★ Four bake rulings — the shore residual, the post-blur `pf[landmask] = 0.0`,
  `_equirect_coords` returning the dirs, and the emission ordering — are in
  `barrens_handoff.md` §5.0b. Each is a plausible "cleanup" that breaks
  something measured.
- ★ **`PAL_BARREN` (42,40,37) stays closed**, now for a *measured* reason: at
  typical snow cover only ~30–45% of the base colour shows, so darkening it does
  almost nothing. The fix was never in the palette.

## 5. Process rules earned the hard way this session

- ★ **Validate at 16 m before every 4 m bake.** `SEADS_SOURCE_RES=16
  SEADS_SKIP_ACCEPT=1 python build_sudbury.py` — ~10 min vs ~100. A one-line
  scope slip cost a full 100-minute bake; two 16 m passes then caught both that
  and a water-bleed bug. Restore with
  `git checkout -- assets render/sudbury_gis.gen.h`.
- ★ **An empty stderr with a truncated log is an OOM kill, not a hang.** A Python
  `MemoryError` leaves a traceback; the OS leaves nothing. The 4 m bake peaks
  near the ceiling (pagefile peak 34 GB on a 29.5 GB box).
- ★ **`pyflakes` is installed** — one second, and `ast.parse` structurally cannot
  see an undefined name. Known false positive: `undefined name 'base'` in
  `sudbury_bake.py` (closure subscript store mis-scoped by a later `del`).
- ★ **The venv `python.exe` is a shim** that re-execs the real interpreter as a
  child, so the PID `Start-Process` returns sits at 4 MB / 0 CPU and looks dead.
  Match on `ParentProcessId`; judge by CPU delta, not log growth.
- ★ **Measure, don't reason.** Three times this session reasoning lost to a cheap
  measurement: the linework counts (twice), the water contamination's real
  severity, and the "too light" root cause. Every one was minutes to measure.
