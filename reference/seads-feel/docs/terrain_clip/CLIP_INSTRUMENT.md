# terrain-clip T1 — "I can fly into the small earth and fly inside it", MEASURED

**Lane** `sandbox/terrain-clip`, worktree `D:\seads_sandboxes\terrain-clip`, off main `3a95a4d08`.
**INSTRUMENT ONLY. No fix is built. No dial is moved.**

Chad, 2026-09-15 (a fly of `seads-recon`, = main): *"I noticed that I was able to fly into the small
earth and fly inside it. It happened at the Murray entrance on a separate prior occasion and then it
happened at Onaping pump and area."*

Probe: `test/unit/test_terrain_clip_probe.cpp` (ctest names `CLIPPROBE *`).
Numbers: `docs/terrain_clip/probe.tsv`.

---

## 1. The mechanism as found

**The aircraft's crash surface is the FIELD. The eye sees the FACET. They are not the same surface.**

| what | file:line | reads |
|---|---|---|
| aircraft terrain contact | `sim/ground.h:62` | `r_s = hf.radius_at(up) + gp.contact_height_m` — `world::HeightField::radius_at`, the bilinear DEM field |
| the gate above it | `sim/ground.h:160` | `if (r > r_s) return;` — a plain radius compare, per tick |
| the caller + its yield | `sim/step.cpp:186-217` | `ground_contact` runs only when `!env->tunnels->contains(next.position)` |
| the drawn planet | `render/sphere_param.h:196-204`, `render/planet.cpp` `fill_face` | mesh vertices sampled from the same field at `subdiv 200 × tiles 2` — an **effective ~59 m cell** (`config/world.toml [planet]`) |
| the surface the eye actually sees between vertices | `render/sphere_param.h:204` `facet_radius_at` | the linear interpolation of that triangle |
| the sled, for contrast | `world/snowpack.h:415` `facet_radius_fn`, injected in `app/main.cpp`; `[snowpack] hf_faceted_ground = true` | **the FACET** — which is precisely why the sled does not fall through |

Cadence: `sim_dt = 1/120 s` (`config/aircraft.toml:10`), one **point** sample of `next.position` per
tick, **no swept segment**. `[ground] deep_penetration_m = 50.0`.

The DEM is 8192×4096 equirect (~11.5 m texel) — **five times finer than the render mesh**. Anything
the mesh cannot resolve (a bench, a scarp, a gorge wall) survives in the crash field and is *chorded
over* in the drawn mesh. Where the chord rides **above** the field, the airframe is legally airborne
inside visible rock.

---

## 2. The suspects, written before the numbers

- **(a) the collision surface lies BELOW the drawn mesh** — drawn/driven fold reversed, a drape
  above the crash radius, LOD/analytic mismatch.
- **(b) tunnelling** — the point sample skips the surface at speed.
- **(c) a deliberate hole** (the Murray bowl / the tunnel net) whose collision yield reaches further
  than the drawn opening.
- **(d) a state with collision disabled** (spawn grace, respawn, mode switch) that leaves the
  airframe under the surface.
- **(e) a sign convention** that reads a below-surface state as airborne and never ejects.

---

## 3. The numbers

### 3a. Drawn-vs-crash gap, 2 m grid, ±500 m of each site (`gap = facet − field`, + = drawn above crash)

| site | p50 | p99 | p99.9 | MAX | MIN | CLIP* | net SUSPENDED† |
|---|---|---|---|---|---|---|---|
| MURRAY_MOUTH | −0.05 | 0.45 | 0.63 | **+1.27** | −1.93 | 0.000 % | **59.17 %** |
| ERRINGTON_MOUTH | −0.02 | 0.19 | 0.25 | +0.33 | −0.45 | 0.000 % | 16.03 % |
| ONAPING_VALLEY_PUMP | −0.03 | 1.31 | 1.69 | +1.85 | −4.39 | 0.112 % | 0 % |
| CTRL_SUDBURY_PUMP | −0.02 | 14.56 | 24.70 | **+34.48** | −38.13 | 6.561 % | 0 % |
| CTRL_VALLEY_CENTER | +0.01 | 8.69 | 27.46 | **+40.95** | −57.58 | 3.032 % | 0 % |

\* CLIP = `facet + 0.77 (ambient snow) > field + 2.45 (contact_height_m)` — the airframe can sit
inside the drawn ground with **no crash**.
† SUSPENDED = `TunnelNet::contains(dir·(field + contact_height))` — ground contact is **not run at
all** there.

### 3b. The same gap swept COARSE (20 m) over a **6 km box** — Chad's *"and area"*

| site | CLIP | invisible wall (>2 m)‡ | max drawn ABOVE crash | at |
|---|---|---|---|---|
| ONAPING_VALLEY_PUMP | **3.834 %** | 24.34 % | **+66.73 m** | 1.30 km W, 1.36 km N of the pump |
| CTRL_VALLEY_CENTER | 1.739 % | 19.88 % | +43.59 m | (−400, 1000) |
| CTRL_SUDBURY_PUMP | 1.311 % | 11.40 % | +29.19 m | (0, −160) |
| MURRAY_MOUTH | 0.408 % | 6.15 % | +21.25 m | (1980, 2560) |
| ERRINGTON_MOUTH | 0.050 % | 0.86 % | +6.17 m | (−2620, 1100) |

‡ the mirror-image defect: the crash shell pokes **above** the drawn ground — Chad's earlier
"I collided into nothing" (T18). Same root cause, opposite sign.

**Worked example (CTRL_VALLEY_CENTER, `probe.tsv`):** a ~60 m scarp (232 m → 295 m) crossed in ~40 m
horizontal — inside one 59 m mesh cell. Along one column the gap runs
`+4.3 → +18.3 → −6.8` and the next `+27.5 → +33.6 → −13.3`. The drawn chord bridges the scarp foot
by +33 m and hangs 47 m below its crest.

### 3c. Tunnel-net suspension footprint (5 m grid, ±700 m, altitude scan 0…300 m)

| mouth | crash surface suspended up to | reach from the mouth axis |
|---|---|---|
| MURRAY | **15 m ABOVE local grade** | **445 m** |
| ERRINGTON | 15 m above local grade | 498 m |

That is `Bowl::lip_cap_m = 6.0` plus the T27-REV grade-following relief term
(`world/tunnel_net.h:222-268`), over the full `bowl_radius_m = 450` opening.

### 3d. Cadence and the below-surface case (`CLIPPROBE cadence`, `CLIPPROBE below-surface`)

- 120 m/s → **1.000 m/tick**; 200 m/s → **1.667 m/tick**. Against `deep_penetration_m = 50`, and a
  point test every tick, the surface **cannot be skipped**.
- 1 m under the shell at 100 m/s level: `crashed=0, on_ground=1`, position **ejected UP to exactly
  `r_s`** (the re-attach grace + flat branch, `sim/ground.h:186-196`). Never left inside, never
  pushed down.
- 60 m under at 100 m/s: `crashed=1` (deep-penetration wall strike).
- 1 mm under at 30 m/s sink: `crashed=1`. The airborne gate is a radius compare, not a signed
  altitude — there is no wrong-radius `r − R` path.

---

## 4. Verdict

**Suspect (a) WINS, with (c) as a second, separate mechanism at Murray only.**

- **(a) CONFIRMED — the winner.** The crash surface is `HeightField::radius_at`; the drawn surface is
  the ~59 m render facet. Wherever terrain is steeper than that cell, the drawn chord rides above the
  crash field: **+66.7 m measured inside Chad's own named site (Onaping pump area), 3.8 % of the
  ground within 3 km of it**. That is a two-storey-thick slab of visible rock with no collision in
  it. It is **global, not site-specific** (1.3–1.7 % at both controls) — Chad named the two places he
  flies low, not the only two places it happens. **The sled does not suffer this because
  `[snowpack] hf_faceted_ground` already pins it to the FACET; only the aircraft still reads the
  field.**
- **(c) CONFIRMED at Murray, and it alone explains that site.** Within 500 m of the Murray mouth the
  facet gap is negligible (max +1.27 m, 0 % clip) — but **59 % of that area has ground contact
  suspended entirely**, up to 15 m above local grade, out to 445 m. Level flight at 15 m AGL across
  the pit mouth drops the crash surface, and the open bowl is underneath. Errington is milder
  (16 %). The tunnel net does not reach Onaping: the Valley pump is 14.2 km from the Errington mouth
  and 20.2 km from the arena centre (arena horizontal semi-axis 4.2 km).
- **(b) KILLED by arithmetic.** 1.0 m/tick at 120 m/s, point test every tick, 50 m threshold.
- **(d) KILLED.** An airframe 1 m under the shell is ejected **up** to `r_s` and latched grounded —
  the code has no path that leaves it inside or pushes it down.
- **(e) KILLED.** `sim/ground.h:160` compares radii directly; 1 mm under is contact.

---

## 5. The ONE bounded fix proposed (NOT built)

**Pin the aircraft's crash surface to the surface the eye is actually shown — the same injection the
sled already uses.**

- `sim::Environment` gains one optional `std::function<double(glm::dvec3)> ground_facet_fn`,
  injected once in `app/main.cpp` from `render::facet_radius_at(hf, d, planet.subdiv, planet.tiles)` —
  the identical seam and the identical function `SnowpackField::facet_radius_fn` already carries for
  the sled (`world/snowpack.h:415`). No new height source, no H1 fork: same field, the mesh's own
  interpolation.
- `sim/ground.h` line 62 becomes a lerp, never a second surface:
  `r_s = mix(hf.radius_at(up), facet(up), gp.facet_contact) + gp.contact_height_m`.
- **THE IDENTITY DIAL: `[ground] facet_contact = 0.0`** — 0 is bit-identical to today's field-only
  crash surface (the function is not even called). `1.0` is the sled's law, collision == what is
  drawn, and it kills **both** signs at once: the clip class *and* the 24 %-of-Onaping
  invisible-wall class.
- Feel exposure is small and measurable before he flies it: `p50 |gap| ≈ 0.03 m`, so a normal
  landing on open ground moves centimetres; the change is metres only where the mesh was already
  lying to him.
- One dial, one fly, per the frozen-kernel precedent (`[auto_level] lean_lead`, `[coordination]
  yaw_vert_budget`).

**Murray is a SECOND, separate rung and needs Chad's ruling, not a fix:** the 15 m × 445 m
suspension lid over the pit mouth is *deliberate* (`Bowl::lip_cap_m` exists so a climb-out does not
"land" on the invisible grade shell). Whether flying in over the rim at 15 m AGL and finding no
ground is a bug or the raid working as designed is **his call**, not ours.

---

## 6. Caveats, stated plainly

1. The probe reads the **raw baked DEM**. The runtime `HeightField` is post-`ImageBlurGaussian`
   (`dem_blur_radius = 3`, ~34 m kernel) and post-lake-flatten (`render/planet.cpp:1010`). Both the
   field and the facet are drawn from the same buffer either way, so the **mechanism and the sign of
   every number are exact**; the magnitudes above are the unblurred bound and the blurred world will
   read somewhat smaller. Re-running this probe against the blurred field is the first thing T2
   should do.
2. The drawn surface here is `facet + 0.77 m` ambient snow. It does **not** include the road-repair
   drapes (ribbons, banks, decks, the `apron_m` pad) or the tunnel bench/sleeve meshes, all of which
   sit on `drawn_radius_at` and can only add to the gap. The numbers are a **floor**, not a ceiling.
3. `chambers_on = false` on this branch, so the two deep pump pockets are structurally absent from
   the net; a future pump landing re-opens them and this probe must be re-run.
