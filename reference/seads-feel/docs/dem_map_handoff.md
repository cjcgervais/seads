# world-dem — where it stands, and what to do next

Branch `sandbox/world-dem` in `D:\seads_sandboxes\world-dem`. Nothing pushed.
Fly it: `D:\seads_sandboxes\world-dem\build\seads.exe`

**State:** gate 972/972, acceptance 11/11 legs green against the shipped bake
(`bake_id=20260809T200807`), all 11 killing mutations confirmed to kill their leg.
AWAITING CHAD'S CERTIFICATION FLY — see the list further down.

## ★ RULING 2026-08-10 — main does NOT advance until this is flown

Chad's call, and the reasoning is worth keeping because it is a rule about what a
ref MEANS, not about this branch:

> `main` is the one ref in this project that can mean "flown and signed".
> Advancing it to an unflown bake spends that meaning for no gain.

`sandbox/world-dem` is pushed to origin — **that** is the step that removes risk;
once the branch is on the remote nothing can be lost. The main fast-forward is then
purely organizational: zero urgency, one command, whenever. And it stays available
indefinitely, because main is a strict ancestor and is not checked out in any
worktree, so it remains a clean fast-forward no matter how long it waits.

`projection.lock` is `status=PROVISIONAL` for exactly the same reason — that field
records CHAD'S FLY, not the gate. The two move together.

**THE ORDER, and nothing jumps the queue:**

1. backup lands (`sandbox/world-dem` -> origin)  ✅
2. Chad flies the bake
3. his rulings fold
4. `projection.lock` -> `LOCKED`
5. `main` fast-forwards to that commit — current and correct in one move
6. trail cache refresh + re-measure
7. graphify regen
8. `winter_plan_query.py` opens S0

**DO NOT refresh the trail cache before the fly.** It is a ~6x free win and it is
tempting to slip in while someone is already in here, but it CHANGES BAKED ASSETS —
which invalidates the pinned `bake_id` and the SHA-256 manifest under the very
artifact Chad is about to fly, and forces a re-fly for nothing. It does not affect
the projection or the terrain under judgement, so it is safe immediately AFTER:
either its own small bake iteration, or folded with whatever the fly rulings
require. Queued at step 6 above.

**Do not slice the 106 commits down to "just the world-dem ones" either.** That is
not a fast-forward: those commits sit ON TOP of the lineage, so extracting them
means cherry-pick or rebase — new hashes, and near-certain breakage, since the DEM
work depends on what is under it (kernel v5, the fields work). It would manufacture
exactly the divergence that was just proven not to exist. The 106 are all
previously-signed, already-pushed work; taking them wholesale is the clean option.

**Downstream dependency:** the winter thread's BLOCK-1 now requires
`projection.lock = LOCKED`, not merely gate-green. A sled on the deck at 20-30 m/s
feels terrain far more than a plane at altitude, so S0's seam checks and ride-feel
measurements have to run against ground Chad has already accepted — otherwise a fly
ruling moves the floor under S0's own results.

**Verify ancestry, do not trust a memory note.** The divergence warning in the
ballistics memory has no scope marker and was over-read once in this session:
```
git rev-list --left-right --count main...HEAD          # left=main-only, right=here
git rev-list --left-right --count origin/main...HEAD
git worktree list | grep -w main                       # must be empty to move it
```
Measured 2026-08-10: `0 106` and `0 86` — main is a strict ancestor of this branch.

## The story so far

Chad's original complaint was that Lake Wanapitei's shore looked cut straight and
the wilderness looked "folded". Two rounds chased data defects that were real but
were not the cause. The cause was the PROJECTION: an aeqd disk squeezes
tangentially with rho, so a round crater lake 33 km out was drawn as a 2.84 lens.
Chad's Option B — a hybrid conformal radial law, 1:1 inside 22 km, conformal past
32 km — took Wanapitei from 2.84 to 1.23 against a real 1.19, and it was never a
shore bug at all.

A fresh-context red team then attacked the result (`docs/dem_map_red_team.md`).
Its headline was a REVERSAL: the "~2x DEM residual" the branch had recorded as an
open problem did not exist, and acting on the commit message that recorded it
would have broken a correct DEM. The instruments had lied because they shared one
defect — patches sized in SPHERE metres, which at the rim span 9.9 km of ground
and a 2.6:1 range of the very scale being measured.

## What this session did

1. **Chad's straight edges (2026-08-09 fly).** "Some small bodies of water in the
   bubble area had unnatural looking straight edges; straight edges are rare in
   Sudbury." They were: the mirror-mesh outlines went through Douglas-Peucker at a
   FLAT 40 m. DP error is an absolute sagitta while shoreline curvature scales
   with the lake, so a 2 km lake kept 5–9% of its vertices and gently curving
   shore became SINGLE CHORDS up to 43% of the lake's own span — Nepahwin, 14 km
   out, went 541 → 49 vertices with a 719 m straight run. The heroes were never
   the complaint (40 m on 17 km of Wanapitei is 0.09 of span, at the source
   outline's own level). The tolerance is now span-relative
   (`C.water_simplify_m`), which holds the flown chord fraction constant at every
   lake size: fleet mean chord/span 0.217 → 0.084 against the source outlines'
   own 0.045, for +25% water vertices in a static mesh.

2. **The shoreline STAIRCASE, which was the other half of the same complaint.**
   The landmask's `>0.5` re-binarise had already been removed, but order-1
   `map_coordinates` is bilinear POINT sampling: on a 0/1 field it returns a
   fraction only within one 4 m source texel of the shore, so 99.7% of the shipped
   mask was still hard 0/1 and every shoreline still sat on a full-texel
   staircase — 11.5 m of ground in the core, up to 96 m at the rim, elongated
   radially. The mask is now area-averaged at half a destination texel of SPHERE
   arc before the resample, so the alpha `sphere_param.h` documents as a
   mip-filterable coverage channel finally is one.

3. **The remap-range pin.** `emin/emax` were the 0.5/99.5 percentiles of the WHOLE
   DISK, measured live every bake — so the gain that defines the CORE's look was a
   function of the FAR FIELD. Every wilderness change moved the ground under
   Copper Cliff, both bubble centres and both pump anchors by a few percent, with
   every test green. The range is now pinned in `assets/remap_range.lock`, written
   by the bake that measured it; the bake still measures the live percentiles and
   STOPS if they drift past `REMAP_PIN_TOL_M`. Re-pin deliberately with
   `SEADS_REMAP_REPIN=1`, and re-fly the core when you do.

   **Pinned at `207.000 … 447.592 m -> 0…350 m`, gain 1.4547.** Worth noting that
   the red team measured this gain as **1.47** two bakes earlier: it had already
   drifted ~1.4% between bakes, which is the drift the pin exists to stop, arriving
   on schedule. Chad's approved core look is now nailed to this number.

4. **The acceptance instrument that can say no.** Eleven legs, each with the
   mutation that kills it, in `offline_tool/accept_map.py` +
   `test/unit/test_bake_manifest.cpp`. See `docs/dem_map_acceptance.md`. The bake
   runs them and reports RED; `--mutants` re-runs every killing mutation so the
   evidence is reproducible instead of a claim in a commit message.

5. **The bake manifest.** One `bake_id` per run, stamped into the lock and the
   header, plus a SHA-256 of every artifact. A mixed asset set is now a red C++
   gate on every build. This tree had already shipped one.

   Incidental but load-bearing evidence: across two independent bake runs,
   `sudbury_normal.png`, `sudbury_treedensity.png` and `sudbury_buildings.bin` came
   out BYTE-IDENTICAL, while the DEM, colour and landmask changed exactly as the
   mask-coverage change predicts (the mask feeds `keep_lake` and the wilderness
   histogram match). **The bake is deterministic**, so a hash mismatch means
   something really changed — which is the assumption the whole manifest rests on,
   and it is now observed rather than hoped for.

## What Chad still has to look at

The legs cannot see any of this; it needs the stick.

- **The small lakes in the bubble** — Nepahwin, Simon, Richard, Fly, McFarlane,
  Kelly, Whitson, Garson, Laurentian, Ramsey. Are the shores curves now?
- **Wanapitei's hero-facing shore** — water standing on land, or land above it.
- **The wilderness at 25–35 km** — is the combing gone from the SHADING, not just
  from the geometry.
- **The ground under Copper Cliff and the pump anchors** — anything visibly moved.
  Both are inside 22 km, where the guarantee says nothing may have.
- **The Murray entrance** — the ~3 m offset may or may not clear the cut disk.
- **North-west of Wanapitei** — the ribbon fade replacing the old guillotine ring.

## The one scoped follow-up

**Acceptance costs the bake a second pass of its most expensive step.** Two legs
(`dem_amplitude_ring`, `normal_map_amplitude`) replay the bake's conditioning from
the 24000² source — the mesh-DEM blur and the normal map's half-texel prefilter.
That is deliberate: comparing the shipped asset against a RE-DERIVATION rather than
against the pipeline is exactly how this branch talked itself into a residual that
did not exist. But it is a real tax on every future bake.

Two ways to remove it, neither done blind:

1. **Reuse the arrays the bake already holds.** `build()` has `elev_blur`, and
   `emit_normal_map` has the prefiltered tapered elev, at the moment acceptance
   needs them. Dump both to gitignored scratch keyed by `bake_id`; `Ctx.source()`
   loads them when the id matches and recomputes otherwise, so a standalone run
   stays correct.
2. **Blur only windows around the six acceptance patches.** `_sphere_metric_blur`
   is local, so a crop padded past 4σ of the largest band is identical inside the
   patch — EXCEPT that the function derives its log-sigma band range from the
   array's own min/max, which a crop changes. Pass the GLOBAL range in, or the
   optimization silently alters the thing being measured.

Either is maybe an hour. Both were left undone because neither can be tested
without another full bake, and an untested speedup in the bake path is worse than
a slow honest gate. `SEADS_SKIP_ACCEPT=1` skips it meanwhile — loudly, and still
exits non-zero, because an uncertified bake must not exit 0.

## For the agent taking terrain over for traversable snowmachine ground

Read `Game_loop_idea/WINTER_LAW.md` first — that is the canon, this is only what
the MAP layer now guarantees you and what will bite you.

**Nothing here fights ONE SURFACE, and the two sides turn out not to touch at
all.** Every change in this branch is bake-side. The winter side adds NO bake-side
term anywhere, inside or outside 22 km (Chad, 2026-08-09) — sled micro-relief is a
runtime deterministic term inside `world::HeightField::radius_at`, as ruled. So the
bit-identity invariant holds outright and **you do not need to reserve a no-op hook
in the bake for us**; an earlier draft of this section told you to, and that was
planning for a coupling that does not exist.

Five things that are load-bearing for you:

1. **The landmask alpha is now FRACTIONAL, not 0/1.** It is a genuine coverage
   channel, area-averaged at half a texel, with shorelines feathered across
   roughly one texel. Any code that asks "is this water" must threshold (the bake's
   own booleans use `> 0.5`) — reading it as a bitmask will now give you a band of
   half-water at every shore. This matters directly for lake ice: the
   punch-through and drifted-over-creek work keys off exactly this channel, and
   the feather is the shore transition you want, not noise to remove.

2. **Lakes have a dedicated flat MIRROR MESH**, decoupled from the terrain mesh, at
   `elev + [water] surface_lift_m` (~0.4 m). That plane — not the terrain under
   it — is the drivable ice surface, and its outline is now sub-texel accurate to
   the shaded shore (that was this session's fix). The shore transition a sled
   crosses is therefore mesh-to-mesh; check it early.

3. **Inside 22 km, the height field is BIT-IDENTICAL, and with no bake-side winter
   term it simply stays that way.** Tunnel mouths, cut disks, both faction bubbles
   and both pump anchors are built against it; `relief_taper` and `core_safe_scale`
   are EXACTLY 1.0 there by construction. Nothing is asked of you here — it is
   listed so you know what you are standing on, and so that IF the winter side ever
   does grow a bake-side term, whoever adds it knows it must be a literal no-op
   inside `RHO_CONFORMAL_START_M` the same way these are.

4. **The theatrical remap is PINNED and the bake will STOP on drift.** If you change
   anything far-field, expect `REMAP PIN BROKEN` — that is the gate working. Re-pin
   with `SEADS_REMAP_REPIN=1` only deliberately, and understand that re-pinning
   moves Chad's approved core relief, so it costs a re-fly.

5. **Every bake now ends in acceptance** (`accept_map.py`, eleven legs) and every
   BUILD checks the bake manifest. If you touch the bake, budget for the legs; if
   you change the radial law or the outline law, they will catch it. `--mutants`
   shows you what each leg can actually see before you trust one.

**The trail-width coupling point, since S2 owns it.** Ribbon widths live in ONE
place — `C.RIBBON_KINDS`, name → (kind code, HALF-width m). Today there is a single
`"trail"` class at **2.6 m half (5.2 m full)**. The winter law rules main trail
~8–9 m and tributary ~3–3.5 m, so today's roster is both too narrow for a main and
one class short: you will need to split `trail` into at least main/tributary, and
the law says ONE width value per class shared by the render ribbon and the physics
hard-pack corridor — so whatever you add here must be the source the corridor reads,
not a parallel constant. Geometry is `offline_tool/sudbury_ribbon.py` (miter offsets
take a PER-VERTEX half-width now, which is what you want for a corridor that
narrows in the bush); batching and the kind bins are `_build_ribbon_batches` in
`sudbury_header.py`.

Related, and already true: ribbon width now tapers smoothly to zero across the
wilderness annulus rather than being cut on a constant-rho arc — so a trail that
runs outward thins away instead of ending at a visible ring.

## Known and deliberate

- `lake_flatness_and_freeboard`'s half-asymmetry number is reported, not asserted:
  one shore of a lake genuinely can be a hill. The hard assert is the defect
  class — no land BELOW its own water in the collar.
- Vermilion (2.99 → 4.72) and Long Lake (9.49 → 5.91) remain the largest shape
  errors on the map. Both sit at ~21 km, in the blend zone, and both are residual
  ANISOTROPY: no scalar corrects an anisotropic amplification, so the zone is left
  ~0.82x too flat radially and ~1.21x too steep tangentially. That is an accepted
  cost of Option B, not a solved problem, and PCA aspect is near-blind to the
  related near/far WEDGE (Wanapitei's measured near-half/far-half width ratio is
  0.99 against a real 0.70).
