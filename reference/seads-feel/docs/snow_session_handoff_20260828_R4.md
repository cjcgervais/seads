# R4 — THE READABLE TRACK. **BUILT, TESTED, AND CHAD-RULED. 2026-08-27b/28.**

> Supplements `docs/snow_session_handoff_20260827b_NEXT.md` (§4 item 4). That file's §4 item 3
> (Chelmsford phantom roads) is now **CLOSED** by Chad's ruling; its item 4 is this document.
> `docs/snow_session_handoff_20260827.md` remains the authority for MECHANISM and measurement.

---

## 1. WHAT HE ACTUALLY MEANT — AND IT INVERTS THE OLD PREMISE

His original line was read as an ask for deeper sinkage:

> *"I dont really see any track marks unless I am in the deep stuff."*

**It was not.** 2026-08-27b, unprompted:

> *"I actually want tracks I think my meaning was mistaken, I want to see tracks, upcoming."*

⚠ **THIS RETIRES THE STANDING R4 CAVEAT.** The prior handoff warned in bold that R4 "WILL NOT MAKE
THE MACHINE SETTLE DEEPER IN DEEPER SNOW, AND HE WILL ASK." He is not asking. The planing law
(`sim/sled.h:551`, bimodal by design, his own signature) is **not in question here** and does not
need reopening for this rung. The ask is a VISIBILITY ask.

---

## 2. THE MEASUREMENT THAT CAME FIRST ★

**The chain was never broken. The signature was just five times too shallow to see.**

`SEADS_TRACK_DEMO=1` lays a deterministic 8-ray star through the machine. Driven headless
(`SEADS_SLED_DEBUG_MODE=1 --smoke 300 <shot>.png 120`, `SEADS_SLEDCAM="22,40,25"`), with the groove
depth swept, the same star at the same camera and the same light:

| groove | `drive_radius` delta on-run | what the screenshot shows |
|---|---|---|
| **0.12 m** (SF3-B ship) | −0.1200 m | **NOTHING.** Not faint — absent. |
| 0.30 m | −0.3000 m | faint aliased sparkle-dashes; reads as specks, not a rut |
| 0.60 m | −0.6000 m | **a legible trail** |
| 1.50 m | −1.5000 m | a trench |

So both halves were already true and neither was enough: the physics carried the deformation
(the delta is exact at every rung), and the rider patch drew it (proved separately —
`SEADS_PATCH_LIFT=3.0` puts a 3 m rim on screen, and `draw_snow_patch` already passes
`vertex_normal_mix = 1` so the patch's own geometric normals reach the light model). **0.12 m over a
0.6 m half-width is ~18° of slope, and this snow barely shades at 18°.**

⚠ **AND THAT IS THE ROOT FINDING, STILL OPEN:** look at any of the shots — huge terrain slopes,
almost no tone change across them. The snow surface is near-flat-lit. R4 buys readability by
spending GEOMETRY on a SHADING problem. The honest fix is R5 (micro-relief / BRDF), which he has
already said yes to, and which would make *untouched* snow read too. **R4 does not close R5.**

---

## 3. THE RULING

Put to him as a three-way with the measured ladder in hand. He chose **the floored law**:

```
depress = min(depth * max_depth_frac, max(min_depress_m, depth_k * depth))
            \__ the physics that survives __/   \__ what he chose __/
```

`depth_k = 0.45`, `min_depress_m = 0.55`, `max_depth_frac = 0.90` — all in `world/tracks.h`.

**Why the floor.** The specced law alone (`0.45 x depth`, no floor) only crosses the readable line
past ~1.3 m of snow — it would have reproduced the exact complaint the rung exists to close, now by
design. The rejected third option was "fix the shading first"; he took the track now.

**Why the `min()` survives the floor.** A cut cannot be deeper than the snow it is cut into —
`SnowpackField::depth_at` already floors the reported depth at zero for the same reason. So on THIN
snow the groove is bounded by the snowpack and stays sub-readable. **That is a known, accepted gap:
thin-snow readability has to come from a SHADING mark (a scuff / polish channel), not from
geometry.** See §5. In the bush he actually drives (0.71–0.89 m) the floor binds fully and the track
reads — which is the case the rung was bought for.

---

## 4. WHAT LANDED

| | |
|---|---|
| `world/tracks.h/.cpp` | The depression is **per-stamp** now. `TrackParams::depress_for()` is the law, in one place, pure. `add(dir, snow_depth_m)` resolves it at LAY time and the scalar rides in the ring — a query can never re-derive it, because by the time you look at the rut the snow you displaced is gone. |
| | `nearest()` returns `{dist, depress}` — the depth travels with the **winner of the distance search**, never blended across neighbours. Blending would smear a deep-bush rut into the thin one beside a corridor and invent a step nobody drove. |
| | `berm_frac` (5/12) replaces the absolute `berm_m`: the shoulder rides on the stamp's own cut. `5/12 * 0.12 == 0.05` reproduces the SF3-B pair exactly. |
| | ★ **A SECOND PASS MAY DEEPEN A RUT, NEVER FILL ONE IN.** The profile runs off the nearest stamp, so a shallower re-lay would win the search and RAISE the surface under the machine driving through it — your own track healing beneath you. Guarded in `add(dir, depth)`. It is also physically right: `depth_at` has already subtracted the first pass's compaction, so a re-drive legitimately reports less snow. |
| `app/main.cpp` | The sled lays through `add(sdir, snow_field.depth_at(sdir))`. The demo star lays through the same path, so the rig certifies the SHIPPED law, not a constant-depth stand-in. |
| `SEADS_TRACK_FLOOR=<m>` | Seeds `min_depress_m` without a rebuild (the `SEADS_R3_FULLDEPTH` precedent). **`0` disarms the floor** — that is the honest depth-proportional arm of the A/B and the kill-switch for the rung. |
| `test/unit/test_tracks.cpp` | Five R4 legs: the law in all three regimes + monotonicity + never-deeper-than-the-snowpack; the depth travels with the nearest stamp (non-vacuous — the two stamps differ by >2x); the never-fill-in guard **and** that a genuinely deeper pass still cuts; the berm rides on the stamp's cut; and the depth-less overload is **bit-identical to pre-R4** (every fixture and the bench rig lay through it). Bounds are derived from the params, never from literals, so a retune cannot make a leg silently vacuous. |

**Verified on screen at the shipped values:** same demo star, same camera, no override —
`drive_radius delta on-run -0.5500 m`, and the star reads.

**Gate:** the 207-case relevant subset (snow/winter/SF3/track/R3/gyro/sled/barren) is **203 pass /
4 fail** — `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`. **These are the
documented pre-existing GI4 sled debt and I proved it rather than assuming it:** stashed the whole
R4 diff, rebuilt, and the same four fail at their old indices 815/816/854/857 (they show as
820/821/859/862 with R4 in, shifted by exactly the 5 test cases added). Zero new failures, zero
"Not Run". ⚠ **The full 1562-case gate has STILL never completed on this box** — that debt is
unchanged and still owed before this branch goes near `main`.

---

## 5. WHAT R4 DID **NOT** CLOSE — read before claiming the rung is finished

1. **THE SERRATION.** The rays read as a chain of chevrons, not a smooth rut, worst on the rays
   running diagonal to the patch grid. **It is a DRAW artifact, not a field defect** — the SF3-B
   "no scallops" leg passes (the field is smooth to 2% of the depression along a path); the rider
   patch samples at `cell_m = 0.5` and the demo's effective stamp spacing is 0.30 m, and a diagonal
   line on a 0.5 m height grid staircases. Deeper cuts make it MORE visible, so R4 made it worse.
   **The lever is patch resolution**, and it has a hard ceiling: `snow_patch_indices` returns
   `uint16` (raylib `Mesh::indices` is `unsigned short`), so `n_side <= 255`. Today 160 x 0.5 m =
   79.5 m. **224 x 0.36 m = 80.3 m of coverage at 1.4x finer cells, 50,176 verts, still under the
   u16 cap** — same span, better sampling. The cost is real and it is a TRADE for him, not a free
   win: `rows_per_frame` bounds the per-frame cost (16 rows x 160 nodes x ~1.06 us = ~2.7 ms), so
   holding that budget means `rows_per_frame ~ 11` and the refresh latency doubles (10 frames /
   0.17 s → 20 frames / 0.34 s, ~10 m of travel at 30 m/s on an 80 m patch) — i.e. a fresh rut
   appears behind the machine a little later. **Put both numbers to him; do not just spend it.**
2. **THE THIN-SNOW MARK.** §3's accepted gap. The honest shape is a compaction/albedo SCUFF, not
   geometry. `compaction_at` already computes exactly the right scalar. The obstacle is a channel:
   the patch writes `texcoord.y = 0` and `draw_snow_patch` **forces `snow_depth_mix = 0` for this
   pass on purpose** — with R3 armed a zero there reads as "no snow" and would paint a bare-ground
   rectangle under the rider, the most visible pixel in the game (`render/planet.cpp:1539`). So a
   scuff channel is a shader change that has to be reconciled with R3's exposure, not a dial.
3. **THE SKI-CUT SIGNATURE IS NOT BUILDABLE AT THIS RESOLUTION, AND THE SPEC SHOULD SAY SO.** The
   prior handoff specced two ski cuts at `ski_width_m 0.135`, ±0.4635 m off a `stance_m 0.927`, plus
   a 0.38 m belt mark. **Every one of those features is SUB-CELL at 0.5 m, and still sub-cell at the
   0.36 m the u16 cap allows.** Drawing a 0.135 m cut needs ~0.05 m cells, which at `n_side <= 255`
   buys a **17 m** patch — the track would vanish 8 m behind the machine. The single combined groove
   is not a simplification anyone should "finish" later at this resolution; it is the only honest
   geometry until either the patch stops using u16 indices (multi-mesh, or a raylib-side change) or
   the fine signature moves to SHADING, which is item 2's channel again.
4. **HEADING.** Stamps are still points, so the profile is radially symmetric. Fine for one combined
   groove — a straight run is a tube — but it means the track has rounded ends and a turn blobs
   slightly at the apex. Heading only becomes load-bearing when there is a lateral signature to
   place, i.e. item 3, i.e. not yet.

---

## 6. THE ORDER I WOULD TAKE IT NOW

1. **Chad drives R4** (`build-play`, not `build/` — CLAUDE.md's ruling; `-O0` is 45x on the rider
   skinning alone and no felt call should be made on it). `SEADS_TRACK_FLOOR=0` is the A/B arm.
2. The **bank-strip probe** — still §4 item 1 of the prior handoff, still unbuilt, still the one
   place he reports a defect (*"my ski is going down into the road"*) that no instrument can look at.
   Unchanged by this rung.
3. **§3.2b, the R2 shoulder fork** — his call is still half-made (option 3 is dead, 1 vs 2 are his).
   Note the Chelmsford closure does NOT close this: he accepted the phantom corridors as map truth,
   he did not rule on the 0.4 m of shoulder float inside the 60 m mask.
4. **R5** — and after §2 above, this is no longer just "what makes untouched snow read as snow". It
   is the root cause of why R4 needed a floor at all.
5. Still cheap and unflown: G1/G2 gyros, and the night pass (armed exposure thins night whiteness on
   scoured ground, and his *"snow needs to STAY WHITE"* ruling predates it — he just presses `]`).
