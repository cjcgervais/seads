# R4b — **CHAD DROVE R4 AND SAW NOTHING.** What that turned out to mean.

> Reads after `docs/snow_session_handoff_20260828_R4.md`. That file describes the depth LAW,
> which Chad ruled and which survives intact. This file is about everything that was wrong
> around it — including a P0 that R4 itself introduced.

His words, on the shipped R4 build: *"no I dont see any tracks, is it possible? Also snow
deformation ?"*

---

## 1. THE INSTRUMENT GAP THAT LET THIS SHIP ★

**Every rig that ever certified the track chain certified a PARKED machine.** `SEADS_TRACK_DEMO`
lays its whole 8-ray star in one instant at the seed, at 0 km/h, and R4 was signed off on
screenshots of exactly that. The moving case — stamps laid one per tick while the machine *and the
camera* move — had never been rendered once.

This is the same shape as the bank-strip gap in the prior handoff: **the one place he reports a
defect was the one place no instrument could look.** New rig, so it cannot recur:

| | |
|---|---|
| `SEADS_SLED_DRIVE=<0..1>` | smoke-only: pins the thumb throttle open so a headless run really drives. |
| `SEADS_TRACK_PROBE=1` | per-second line: speed, stamp count, whether a stamp was laid, the depth it was laid at, the deform under the machine, **and the patch-vs-planet-mesh and drive-vs-planet-mesh offsets** — the comparison that found §3. |

With those two, the defect reproduces on demand in ~40 s.

---

## 2. WHAT THE PROBE SAID — AND IT EXONERATED THE FIELD

```
TRACKPROBE t=0   v=0.3  km/h stamps=1   laid=1 lay_depth=0.713 deform_here=-0.5500
TRACKPROBE t=600 v=49.9 km/h stamps=155 laid=0 lay_depth=0.202 deform_here=-0.5500
```

Stamps are laid while moving, the cut is the ruled 0.55 m, and the ground behind the machine
carries it. **The field side was never broken.** Everything below is either the draw, the
viewpoint, or a bug R4 put in.

⚠ Note `lay_depth` collapsing 0.713 → 0.202. That is not the snow thinning; it is the machine
reading its own rut. It is the tell for §4 — and the confirmation that §4 is fixed is that the
same run now reads **0.713 → 0.752**, tracking the real snowpack, with the guard deleted.

---

## 3. WHY IT DID NOT READ — THE PATCH HAD BEEN DRAWING BELOW THE WORLD SINCE R1

**MEASURED:** `fold = 0.713 m`, `lift_m = 0.150 m`, so `patch_vs_mesh = -1.113 m` in the rut and
**−0.563 m even off-track**.

`render/snow_patch.cpp` says, in its own words, *"the surrounding planet mesh has NO snow in it
whatsoever"* — and that was TRUE when it was written. **R1 folded the ambient snowpack into the
planet mesh and made it false, and nobody moved the patch.** So the rider patch has been drawing on
the BARE facet, half a metre below the snow surface every one of its neighbours draws, since the
fold landed.

It is also a fork by this repo's own law, and the arithmetic says so: the DRIVEN surface is
`facet + fold + track` (probe: `drive_vs_mesh = -0.550`, the rut half a metre below the drawn
snow), while the patch drew `facet + track`. **Draw and drive disagreed by the whole fold.**

**THE FIX, AND ITS CLOSING MEASUREMENT.** The patch base is
`facet_radius_at + draw_fold_at`. Same probe, same drive, after:

| | before | after |
|---|---|---|
| `patch_vs_mesh` in the rut | −1.113 m | **−0.400 m** |
| `drive_vs_mesh` in the rut | −0.550 m | −0.550 m |
| **drawn-minus-driven** | **0.713 m** (the whole fold) | **0.150 m** (= `lift_m`, by construction) |

The drawn surface and the driven surface now agree to exactly the clearance that separates
them on purpose. That is the number to watch: if drawn-minus-driven ever drifts off `lift_m`
again, the patch has forked from the world a second time.

Patch and planet mesh now agree off-track by construction — which is precisely why the original *"there are no ruts, its depressing
the whole square area"* slab does **not** come back: that slab existed because the patch carried
snow its neighbours did not. Now they both do. ★ The fold is deliberately **not** rim-faded (only
lift and the track are): fading it would sink the patch edge ~0.7 m and cut a crater rim — the
square, in negative.

**COST, and it needed a second pass.** Evaluating `draw_fold_at` per 0.5 m node measured **1.16 →
5.10 ms/frame** for the patch (RelWithDebInfo, this box) — 4.4×, a third of a 60 Hz budget. Halving
`rows_per_frame` did not hold it. The term *cannot* vary at that scale (`curv_probe_m` is 40 m; the
ambient function IS the homogenizer, 2.6 cm per 2 m measured), so the fold is now sampled on a 9×9
lattice spanning the patch and bilinearly interpolated: **81 calls per refresh instead of 25,600**,
and `rows_per_frame` goes back to 16. The lattice is filled at `snow_patch_begin`, where the frame
is fixed; a null field leaves it empty and reads 0, i.e. bit-identical to the pre-R4b patch.

---

## 4. ⚠ P0 — R4's OWN never-fill-in GUARD WAS A SESSION-WIDE RUNNING MAX

Found by the fresh-context red team. R4 shipped, in `TrackField::add(dir, depth)`:

```cpp
if (n.dist_m < p_.half_width_m) d = std::max(d, n.depress_m);   // WRONG
```

meant to stop a re-drive healing a rut. **Consecutive stamps of a single pass are always closer than
`half_width_m`**, so the max chained stamp-to-stamp and never released: one transit of deep bush and
every later stamp — on a road, a plowed deck, bare rock — still cut 0.55 m, i.e. **metres of track
carved below the ground it was cut in**, in direct contradiction of the physics claim R4's own
commit message makes.

**The fix is at the INPUT, and the guard is gone.** `SnowpackField::track_lay_depth_at` is the
undisturbed snowpack — `depth_at` *without* the pass's own compaction subtracted — so both halves
fall out for free: a re-drive re-reads the same undisturbed depth and re-lays the same cut (it
cannot fill in), and thin ground honestly cuts thin. `depth_at` keeps subtracting compaction,
because for SINKAGE that is correct; it now calls the same `undisturbed_depth_at` body so the two
cannot drift.

**Do not reintroduce a distance-based guard.** The comment at the call site says why.

---

## 5. OTHER DEFECTS FIXED THIS SESSION

**P1 — a track cut 0.55 m into lake ice.** Over water `depth_at` returns `ice_lift_m + ice_snow_m`
(0.65) — a GEOMETRY LIFT to the mirror mesh, not a snowpack to displace. `snowpack.cpp`'s own banner
warns against exactly this consumer. `track_lay_depth_at` gates it to zero on the landmask
**fraction** (INV-8 — never a `> 0` threshold, which would band every shoreline).

**P1 — the winner-take-all depth was a cliff, and R4's comment had it backwards.** R4 took the shape
*and* the amplitude from the nearest stamp, so where two passes of different depth run 0.6–1.1 m
apart the depth flips at the Voronoi boundary with both grooves at full weight — a step of up to
0.53 m in the DRIVEN surface, the depth cliff `snowpack.h` says stops a machine dead. R4's comment
claimed *blending* would cause that; it is the other way round. Now: **shape from the nearest
distance** (the anti-sum rule is untouched — summing profiles would let berms fill their own
groove), **amplitude from a distance-weighted mean** with a `(1 − d/reach)^4` kernel — steep enough
that the nearest stamp still holds ~94% at 0.3 m, and a uniform-depth run is bit-identical because
every weight multiplies the same value.

---

## 6. TESTS — INCLUDING TWO THAT WERE BLESSING THE BUG

The red team found both R4 legs that survive a mutation of the mechanism they claim to pin, and it
was right about both:

- The "cut depth travels with the nearest stamp" leg put its two stamps **four reaches apart** and
  sampled only at their centres, so *any* cross-stamp law — winner, mean, anything — was a no-op
  there. The fixture-no-op class.
- The "second pass never fills in" leg's first fixture **was** the running-max case, so it asserted
  the P0 as correct behaviour.

Replaced by three legs, **each mutation-verified** (mutation applied, suite run, failure confirmed,
tree restored):

| leg | kills |
|---|---|
| `R4b: two passes of different depth meet WITHOUT a cliff` | restoring winner-take-all |
| `R4b: a re-drive cannot fill in its own rut, and cannot chain` | reinstating the running-max guard |
| (c) inside that leg | a lone thin stamp inheriting a neighbour's depth or the fallback dial |

★ The cliff leg is worth reading before touching the blend. Its **first draft failed honestly** by
walking the whole profile — which measures the groove WALL of a deep cut, steep by construction and
nothing to do with the seam. It now walks only the band where both grooves are saturated, so only
the amplitude can move; and it pins **continuity** (4× finer sampling ⇒ proportionally smaller step)
rather than a slope bound, because a slope bound would be calibrated to today's blend kernel and
would false-fail an honest retune of it. A Voronoi step does not shrink under refinement — that is
what makes the leg a discriminator instead of a threshold.

---

## 7. STILL OPEN — and #1 is the one that decides whether he sees anything

1. ⭐ **THE CHASE CAMERA NEVER FRAMES THE TRACK.** The default drive view looks forward over the
   rider's shoulder; the track exists only behind, the machine occludes the freshest few metres, and
   ahead is always virgin snow. **Even with everything above fixed, he has to look back to see it.**
   This is very likely the largest single term in his report and it is a design question for him:
   pull the chase camera up/back, add a glance-back, or accept it. **His call — put both to him.**
2. **ON TRAILS THE CUT IS MEASURE-INVISIBLE BY DESIGN.** `depth_at` on a packed trail is
   `trail_pack_m = 0.12`, on a road 0.02 → cuts of 0.108 / 0.018 m. His own ladder says 0.12 m is
   "NOT faint — absent". **The floor never applies where he drives fast.** This is the `min()` doing
   its job, and it means R4 closed nothing on trails. The answer is the shading scuff (R4 handoff
   §5.2), not more geometry.
3. **The signal is still weak and short.** What renders is a dim chain, and the patch spans ±39.75 m
   of the *camera*, so at 50 km/h nothing older than ~3.5 s is drawn at all. Root cause of the
   dimness is still R5 — this snow barely shades.
4. **P1 — the stamp samples depth at its CENTRE but cuts across a 1.1 m reach.** Laid 0.5 m from a
   corridor edge, a bush-depth stamp's groove reaches onto the 0.02 m deck and cuts below it. Not
   fixed. Same at rock outcrops.
5. **P2 — the ring wraps at 5 km** (~4–7 min of driving): old track self-erases, and evicting a deep
   stamp beside a shallow neighbour RAISES the surface — the fill-in the design forbids, defeated by
   capacity.
6. Unchanged from R4 §5: the draw-side serration (patch resolution, u16-capped), the thin-snow scuff
   channel, and the ski-cut signature being sub-cell at any allowed resolution.

---

## 8. ★ THE FULL GATE COMPLETED — FIRST TIME ON THIS BOX — AND FOUND A FIFTH FAILURE

The prior two handoffs both record the same debt: *"the 1562-case gate has NEVER COMPLETED on this
box"*, twice killed partway by parallel sandbox gates saturating the machine. **It completed this
session, and the trick is boring:** run `./build/seads_tests.exe` DIRECTLY, redirected to a file.
The contention was never the test suite — it was `ctest` competing with other sandboxes' `ctest`.
One process, one file, no pipe:

```
./build/seads_tests.exe > gate.txt 2>&1 ; echo "REAL_EXIT=$?" >> gate.txt
```

(The `REAL_EXIT` line is not decoration — §6.2 of the 0827 handoff records a gate that reported
`exit 0` because the code came from `tail`, not from the runner.)

**RESULT on `49b307a54`: 1565 cases, 1560 passed, 5 failed, `REAL_EXIT=42`, zero "Not Run".**

| failing case | attribution |
|---|---|
| `sled_slides_before_it_tips_on_flat_snow` | the documented pre-existing GI4 sled debt |
| `sled_grip_ceiling_stays_below_the_tip_threshold` | ″ |
| `sled_assist_reference_plane_is_load_weighted` | ″ |
| `sled_debug_sink_is_write_only` | ″ |
| ⚠ **`probe P-F: the relentless raider keeps the pump and shoots back`** | **NEW TO THE LEDGER — and not this rung's** |

⚠ **THE FIFTH ONE HAS NEVER BEEN ON ANYONE'S LIST**, because no full gate had ever finished to put
it there. Every session before this measured a *relevant subset* and honestly reported "4 failures",
which was true of the subset and blind to this.

**It is not R4/R4b's.** `test/unit/test_enemy_ai_e6.cpp:798` asserts a raider that PRESSES holds a
*tighter* radius round the pump than one that PAUSES; measured, it is the reverse (4814.85 m vs
2509.21 m) and the press arm never fires at the player at all (`rounds_at_player=0` vs 13). The file
has **zero** references to `snowpack`, `depth_at`, `TrackField` or `snow_patch`, and includes only
`combat/`, `app/instructor_tick`, and config — structurally disjoint from every line this rung
touched. It belongs to the enemy-AI thread.

**Do not fold it into the snow debt, and do not let the next session rediscover it as "new".**
The honest ledger is now: **4 sled + 1 enemy-AI pre-existing failures on a completed full gate.**
