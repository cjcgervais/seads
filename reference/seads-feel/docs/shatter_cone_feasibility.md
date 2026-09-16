# Shatter cones on the black rock — is it feasible?

Chad's ask: "if it is at all possible to apply rendered shattercone look on the
sides of the black rocks in some places."

**Short answer: yes, and it is cheap — but the geology and the rendering have
different answers, and you should rule on them separately.**

---

## 1. The rendering: feasible, ~a day, gated to near-camera steep faces

It cannot live in this raster, and that is not a limitation of the raster — it is
arithmetic. A shatter cone is 1 cm to ~3 m at Sudbury. Our budget:

| thing | scale on the ground |
|---|---|
| terrain mesh vertex spacing | ~59 m |
| equirect albedo / normal texel | ~6.8 m at the hero point |
| a shatter cone | 0.01 - 3 m |

The cone is **two to three orders of magnitude below the finest thing the map can
store**. No bake at any sane resolution reaches it. So it has to be a **runtime
detail pass**, which is the standard solution and is well-trodden:

- **Triplanar detail normal**, gated by `slope x barren_mask x distance_fade`.
  The gate is the whole trick: flat ground, non-barren ground, and anything past
  ~300 m pays only a few ALU ops and never takes the branch. Slope and the
  barren mask are spatially coherent (whole cliff faces agree), so GPU warps
  agree on the branch and it stays cheap. A triplanar pass costs ~3 texture
  samples where it *does* run — a small fraction of the frame on a 15 km sphere.
- **The pattern.** Either (a) one tileable hand-authored cone-striation detail
  normal, or (b) fully procedural: a jittered-grid (Worley) apex field, conical
  falloff `h = -d * tan(halfAngle)`, angular striation `triangleWave(theta * N)`
  with `N` rising with distance from the apex to fake the horsetail branching,
  normals taken analytically from the height derivatives. (a) is cheaper and
  more art-directable; (b) never repeats. I'd start with (a).
- **Tiling breakup**: per-cell random rotation is the cheap fix; Heitz & Neyret
  histogram-preserving blending (3 samples) is the quality fix if it reads
  repetitive.
- **Blending onto the base normal**: use **Reoriented Normal Mapping**, not
  additive — our base normal comes from a coarse mesh and can be steep, which is
  exactly the case where additive/whiteout blending goes soft:

      t = n_base * (2,2,2) + (-1,-1,0)
      u = n_detail * (-2,-2,2) + (1,1,-1)
      r = normalize(t * dot(t,u) - u * t.z)

  Applied per-plane *before* the triplanar weight combine.

**The mask is already free.** `sudbury_barrens.png` is being baked anyway for the
albedo and the trees; the shader reads the same channel. There is no extra asset,
no extra bake, no extra projection work.

**Cone orientation, if we want the detail to be honest**: shatter-cone apices
point up-range, toward the impact point. Sudbury's cones sit in the *footwall*
and point toward the basin. A shader can honour that essentially for free by
orienting the cone axis from the surface point toward the basin centre rather
than randomly — a nice, real, cheap touch.

Real apex geometry to build to: apical angle **75-90 deg at Sudbury** (60-120 deg
across all impact structures), so half-angle ~40-45 deg with +/-15 deg jitter.
Striations radiate from the apex, **narrow at the apex and fanning/forking wider
away from it** — the "horsetail". Sub-cones nest on larger cones (self-similar),
which is why a two-octave pattern reads better than one.

---

## 2. The geology: this part is artistic licence, and you should know it

Shatter cones at Sudbury are absolutely real — it is one of the world's type
localities. But three things cut against putting them on *the black rock*:

1. **Wrong rock, mostly.** Cones occur in the **footwall** — the target rock
   beneath and outside the Sudbury Igneous Complex — not in the melt sheet. The
   documented field localities are **Kelly Lake** and the **Windy Lake / Highway
   144** area. The industrial barrens are concentrated around Copper Cliff,
   Coniston and Falconbridge. **No source places documented shatter cones on the
   blackened Copper Cliff barrens.** They are two unrelated phenomena separated
   by 1.85 billion years, and I could not find them co-located in the literature.
2. **Wrong surface condition.** Cones read best on **freshly broken, quarried or
   roadcut faces** — field geologists hammer rock open specifically to reveal
   them. Lichen crust, deep weathering rind, and surface oxidation mask the fine
   striae. A soot/patina film is, by the same argument, likely to obscure them.
   The counter-case: where exposure is excellent — glacially scoured, bald,
   vegetation-free rock — cones *are* visible on natural outcrop in >90% of
   outcrops at some structures. Sudbury's barrens are exactly that kind of bald
   scoured rock, which is the strongest argument in favour.
3. **Scale.** Typical cones are cm-scale. Metre-scale ones exist (up to ~3 m at
   Sudbury) but are the exception. At the size that would actually read from a
   cockpit, you are drawing the rare case everywhere.

### So: three honest options

**A — Put them where they're real (my recommendation).** Treat shatter cones as
a *located feature*: a hero outcrop treatment at **Kelly Lake** (which is already
a named landable hero lake in `HERO_LAKES`, at rho ~34 km) and along the **Hwy
144 / Windy Lake** corridor. Use a generic dark fractured/scoured-rock detail on
the barrens themselves. This buys the impact structure as a real, findable place
in the world instead of wallpaper — and it is *more* interesting: the crater is
the reason the ore is there, which is the reason the smelters are there, which is
the reason the rock is black. The map can tell that whole chain.

**B — Put them on the barrens anyway.** Cheap (same mask, same shader), looks
great, geologically a stretch but *defensible* on the "bald glacially-scoured
exposure" argument. If you want the black rock to feel alien and shattered from
the air and on strafing runs, this delivers it. It is your call and it is a
legitimate one — just make it knowingly.

**C — Both, with different intensity.** Full cone detail at the real localities,
a faint version on the most exposed barren crests. Slightly more shader work,
best of both.

Either way the mask, the gate, and the shader are the same code. The only thing
that changes is *where the intensity comes from* — a second baked channel for
option A/C (a "footwall shock fabric" mask keyed off the SIC footwall geometry),
or the barrens channel itself for option B. Both are cheap.

---

## 3. What I'd need from you

Only one ruling: **A, B, or C.** Everything else is decided. The layer bake does
not block on it — `sudbury_barrens.png` is the mask for B and C regardless, and
A only adds a second small channel.
