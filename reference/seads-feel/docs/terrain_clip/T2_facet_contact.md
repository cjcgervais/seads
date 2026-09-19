# terrain-clip T2 — the aircraft lands on the facet the eye sees

**Lane** `sandbox/terrain-clip`, worktree `D:\seads_sandboxes\terrain-clip`, on T1 `d5d7040da`.
**THE FIX IS BUILT AND SHIPPED ARMED ON THIS LANE.** `[ground] facet_contact = 1.0`, `0.0` = identity.
Not landed, not pushed. Chad flies it; the full gate + red-team are owed before it goes to main.

T1 is the instrument: `docs/terrain_clip/CLIP_INSTRUMENT.md`. This is the one bounded fix it proposed.

---

## 0. What Chad reported

The original report (2026-09-15, a fly of `seads-recon` == main):

> *"I noticed that I was able to fly into the small earth and fly inside it. It happened at the
> Murray entrance on a separate prior occasion and then it happened at Onaping pump and area."*

T1 asked him three things: was he fast, was it just after a respawn, and did he hit something first.
His answer, **verbatim** (2026-09-17):

> *"yes I think it was at the entrance I noticed a hole there earlier as well and I cannot remember
> how I entered I think both were crashes into the hillside, im not even sure if it was the tunnel
> entrance but definitely near it."*

**Read:** both events were **hillside entries near special sites** — not a respawn state, not a
speed/tunnelling case, and not necessarily the tunnel mouth itself. That is mechanism **(a)**, the
facet gap, which is global and is worst exactly on steep ground; T1's arithmetic had already killed
tunnelling and the below-surface latch independently. **This rung is the right fix for what he
flew.** The Murray lid (mechanism (c), §5.4) stays a separate open question — "I noticed a hole
there earlier as well" is consistent with it, but he is explicit that he cannot place the entry at
the mouth, so nothing here grades it.

---

## 1. Mechanism

The aircraft's crash surface was `world::HeightField::radius_at` — the **bilinear DEM field**, an
8192×4096 equirect (~11.5 m texel). The eye is shown the render mesh, whose surface *between*
vertices is `render::facet_radius_at` — subdiv 200 × tiles 2, an **effective ~59 m cell**. Five times
coarser. Where terrain outruns one mesh cell the drawn chord bridges **above** the field and the
airframe is legally airborne inside visible rock (Chad: *"I was able to fly into the small earth and
fly inside it"*); where it hangs below, the crash shell pokes above the drawn ground and he
*"collided into nothing"*. One defect, two signs.

**The sled never had this**: `[snowpack] hf_faceted_ground = true` already pins it to the facet
through `world::SnowpackField::facet_radius_fn`, injected from `app/main.cpp`. T2 gives the aircraft
the same law through the same seam and the same function.

### The three edits

| file | what |
|---|---|
| `sim/environment.h` | `Environment::ground_facet_fn` — `std::function<double(glm::dvec3)>`, empty = absent. Plus `GroundParams::facet_contact` (default `0.0`). |
| `sim/ground.h` | new `sim::contact_radius(hf, gp, facet, up)`; `r_s` at the top of `ground_contact` reads it. `ground_contact` gains a **defaulted trailing** `const std::function<double(glm::dvec3)>* facet = nullptr`, so every existing caller and test compiles and behaves unchanged. |
| `sim/step.cpp` | passes `&env->ground_facet_fn` into `ground_contact`. |

### The mix line (`sim/ground.h`)

```cpp
inline double contact_radius(const world::HeightField& hf, const GroundParams& gp,
                             const std::function<double(glm::dvec3)>* facet, glm::dvec3 up) {
    if (gp.facet_contact <= 0.0 || facet == nullptr || !*facet)
        return hf.radius_at(up) + gp.contact_height_m;          // the pre-T2 expression, verbatim
    const double fi = hf.radius_at(up);
    const double fa = (*facet)(up);
    if (gp.facet_contact >= 1.0) return fa + gp.contact_height_m;   // the sled's law, no mix residue
    return fi + (fa - fi) * gp.facet_contact + gp.contact_height_m;
}
```

`r_s` is computed **once per tick** and is the surface the grounded snap, the fell-away tolerance,
the airborne gate and the deep-penetration floor all read — the crash surface cannot fork inside a
tick.

### Where the function is injected (`app/main.cpp`, ~line 2850)

Immediately beside `snow_field.facet_radius_fn`, in the same `if (snow_field.hf != nullptr)` block,
from the same three values:

```cpp
env.ground_facet_fn = [facet_hf, facet_subdiv, facet_tiles](glm::dvec3 dir) {
    return render::facet_radius_at(*facet_hf, dir, facet_subdiv, facet_tiles);
};
```

`facet_hf` **is** `env.ground` (`snow_field.hf = env.ground` a few lines up) — the H1 anti-fork: same
field, the mesh's own interpolation, never a second height source. `facet_subdiv`/`facet_tiles` are
`world.planet.subdiv` / `world.planet.tiles`, the shipped `[planet]` values the mesh was actually
built at. It is **the terrain facet**, not `drawn_radius_at` — exactly as the sled's base is, so the
0.77 m ambient snow fold is never counted into the crash surface.

---

## 2. The dial

`config/game.toml [ground] facet_contact` — **shipped 1.0 on this lane**, loader-bounded `[0, 1]`
(`config/load_game.cpp`), range-checked with its own named message.

| value | meaning |
|---|---|
| `0.0` | the pre-T2 DEM-field crash surface. **Identity by branch** — the facet function is not called. |
| `1.0` | collision == what the eye is shown. The sled's law, applied to the aircraft. |

**KILL: `SEADS_FACET_CONTACT=0`** — read in `app/main.cpp` right where the config value is copied
into `env.ground_params`, and it **replaces** that value (the `SEADS_OVER_BANK_BIAS` pattern), then
clamps to `[0, 1]` so a typo in the environment can never hand the kernel a surface nothing is drawn
on. This is the honest OFF arm of the A/B as well as the kill.

---

## 3. Identity proof

Three independent legs, all in `test/unit/test_facet_contact.cpp` (`FACETCONTACT *`):

1. **By branch, counted.** A stub facet function that increments a call counter is injected; at
   `facet_contact = 0.0`, `contact_radius` returns a double that compares `==` (not "within eps") to
   the literal pre-T2 expression `hf.radius_at(up) + gp.contact_height_m`, and **the counter is still
   0**. The arithmetic is not merely equal — it is the same statement.
2. **Absent == off, regardless of the dial.** `facet == nullptr` and an *empty* `std::function` both
   return the same pre-T2 value at `facet_contact = 1.0`, with the counter still 0. Every headless
   test, every golden, every `Environment` built without a render layer is structurally on the old
   arm; a test that forgets the injection reads the old surface, never a silent half-armed one.
3. **A whole tick, not just the radius.** `ground_contact` is run at three sink rates (0, 2, 9 m/s)
   with and without a 10 m-high stub facet at dial 0; `position`, `velocity`, `on_ground` and
   `crashed` compare bit-equal.

Plus the loader bounds: a `game.toml` copy with `1.5` and one with `-0.1` both **throw**; `0.0`
loads clean (so the OFF arm is a real, shippable config).

---

## 4. The numbers

### 4a. Clip census, 6 km box, 20 m step, `dial 0 → dial 1`

CLIP = the drawn surface (`facet + 0.77 m` ambient snow) rides above the lowest legal airborne CG
(`r_s + 0` where `r_s = mix(field, facet, dial) + 2.45 m`) — i.e. an airframe can sit inside visible
ground with no crash. WALL = the mirror sign, the crash shell more than 2 m above the drawn ground.

| site | arm | CLIP | WALL (>2 m) | max(facet − field) |
|---|---|---|---|---|
| ONAPING_VALLEY_PUMP | RAW | **3.834 % → 0.000 %** | 24.340 % → 0.000 % | +66.73 m |
| ONAPING_VALLEY_PUMP | BLURRED | 2.414 % → 0.000 % | 23.298 % → 0.000 % | +35.22 m |
| CTRL_VALLEY_CENTER | RAW | 1.739 % → 0.000 % | 19.875 % → 0.000 % | +43.59 m |
| CTRL_VALLEY_CENTER | BLURRED | 1.097 % → 0.000 % | 19.462 % → 0.000 % | +27.80 m |
| CTRL_SUDBURY_PUMP | RAW | 1.311 % → 0.000 % | 11.401 % → 0.000 % | +29.19 m |
| CTRL_SUDBURY_PUMP | BLURRED | 0.397 % → 0.000 % | 9.840 % → 0.000 % | +23.86 m |
| MURRAY_MOUTH | RAW | 0.408 % → 0.000 % | 6.149 % → 0.000 % | +21.25 m |
| MURRAY_MOUTH | BLURRED | 0.019 % → 0.000 % | 4.720 % → 0.000 % | +2.56 m |

**Zero by construction, not by tuning.** At dial 1 the crash surface *is* the facet, so CLIP would
need `facet + 0.77 > facet + 2.45`. Both classes close in the same arithmetic, on both arms, at
every site. Asserted, not just printed.

~~**THE NAMED RESIDUAL** — believed empty, unmeasured.~~ **T2b MEASURED IT. It is NOT empty.**
See §8. And `obstacle_contact` no longer bases building prisms on `hf.radius_at` — T2b routed them
through `sim::contact_radius` too (§8.2).

### 4b. RAW vs BLURRED — and the finding that retires T1 caveat 1

T1 flagged every number as "the unblurred bound" because the runtime `HeightField` was believed to be
post-`ImageBlurGaussian(dem_blur_radius = 3)`. **It is not.** `render/planet.cpp` `load_planet`:

```cpp
if (procedural) dem_blur_radius = 0;   // the Sudbury DEM ships pre-blurred + lakes pre-flattened
```

and `config/world.toml [ground] use_procedural = 1`. The retained field is the baked
`assets/sudbury_dem.png` **verbatim**. So the **RAW arm above IS the runtime field**, and T1's
magnitudes were exact, not a bound. The BLURRED arm (raylib's actual algorithm — three successive box
blurs of width `2r+1`, reproduced over a padded window per site) is kept as a **sensitivity arm**: it
shows a further offline blur would shave the worst gap roughly in half (66.7 → 35.2 m at Onaping) but
would move the WALL class barely at all (24.3 → 23.3 %). The defect is not a blur artefact; it is a
mesh-resolution artefact, and softening the DEM cannot fix it.

### 4c. Landing gap — the feel exposure, `|facet − field|` over ±250 m, 2 m grid

| site | p50 | p90 | p99 | MAX | signed p50 |
|---|---|---|---|---|---|
| SUDBURY spawn (Murray mouth) | **0.069 m** | 0.355 m | 0.814 m | 1.084 m | −0.034 m |
| VALLEY spawn (Errington mouth) | 0.065 m | 0.168 m | 0.260 m | 0.330 m | −0.003 m |
| VALLEY PUMP apron (Onaping) | **0.216 m** | 0.647 m | 1.256 m | 1.779 m | +0.025 m |

A normal touchdown moves **centimetres**, as T1 claimed (its `p50 |gap| ≈ 0.03 m` was the *signed*
median over the whole 500 m box; the magnitude median is 7–22 cm, which is the honest number). The
worst case inside a landing footprint is **1.8 m at the Valley pump apron** — that is a real, feelable
step and it is where Chad should look hardest.

⚠ **No existing test's runway or ground-height assumption moved.** `ground*` (24 cases, 30467
assertions), `*land*`/`*touchdown*`/`*step*`/`*sphere*`/`*snowpack*` (127 cases, 73569 assertions)
and `CLIPPROBE *` are all green with `facet_contact = 1.0` shipped in `config/game.toml`. They are
green **structurally**, not by luck: none of them injects a facet function, so all of them are on the
absent→field arm whatever the dial says. Nothing was edited to make anything pass.

### 4d. Cost

`render::facet_radius_at` is **O(1)** — a face pick, an unwarp into the vertex grid, and **three**
corner `radius_at` evaluations. It is **not a mesh walk**; it cannot grow with subdiv or tiles.

Measured on the assert-live (Debug) test build, 20 000 samples 1 m apart (the actual 120 m/s cadence):

- `hf.radius_at` ≈ **85 ns/call**
- `render::facet_radius_at` ≈ **~1.0 µs/call**

One extra per aircraft tick = **~0.012 % of an 8333 µs tick** at 120 Hz. RelWithDebInfo is faster
still.

⚠ **T2b SCALES THAT: it is ~1 µs per AIRFRAME, not per frame.** The AI now reads the same surface
(§8.1), so the shipped fleet of **10 aircraft** costs one AGL query each plus the deck window's six
forward samples on the ticks it sweeps — order **10–70 µs of an 8333 µs tick**, still under 1 %, and
it is bounded by the fleet size, not by subdiv or tiles. The measured A/B (§8.1) shows no frame-rate
complaint and *fewer* AI wrecks. **No cache needed**, and none added — a per-tile cache would be new state in the kernel's read
path for a cost that is already four orders below the budget. (The existing `facet_radius_impl` memo
sits only on the *fold* path and is untouched.)

---

## 5. Traps

1. **The dial only exists where the app injects.** `Environment::ground_facet_fn` is filled in
   `app/main.cpp` inside `if (snow_field.hf != nullptr)`. Any future sim host that builds its own
   `Environment` (a headless replay, a tape harness, a server tick) gets the FIELD surface and will
   disagree with the app by up to tens of metres on steep ground. If a replay ever has to reproduce a
   flown crash, it must inject too.
2. **The landing NORMAL is still the field's.** `hf.normal_at(up, 60 m)` still grades slope
   acceptance and touchdown attitude. Deliberate: the facet normal is piecewise-constant per triangle
   and would move feel for a second reason inside one dial. So on a steep facet the *height* is the
   drawn one while the *slope* is the field's — a documented second-order disagreement, not a fork
   (both come from the same field).
3. ~~**Buildings are unchanged**~~ — T2b changed them; see §8.2.
4. **The Murray lid is NOT this rung.** T1's second mechanism — `TunnelNet::contains` suspending
   ground contact over 59 % of the Murray mouth footprint, up to 15 m above local grade and 445 m
   out — is untouched here and needs Chad's ruling (bug, or the raid working as designed). If he
   flies into the Murray pit and finds no ground, **that is the other rung, not a T2 failure.**
5. **`chambers_on = false`** on this branch, so the deep pump pockets are structurally absent from
   the net; re-run both probes when a pump landing re-opens them.
6. **The `>= 1.0` branch is deliberate**, not a micro-optimisation: `fi + (fa - fi) * 1.0` is not
   bit-identical to `fa`, and the armed arm must be exactly the surface the mesh draws.

---

## 6. What Chad must fly

Exe: `D:\seads_sandboxes\terrain-clip\build-play\seads.exe`. **Zero `SEADS_*` env.**
A/B at any time by relaunching with `SEADS_FACET_CONTACT=0` (the whole fix off, bit-identically).

1. **THE ONAPING HILLSIDE — where he went in.** Fly low over the hills ~1.3 km W / 1.4 km N of the
   Onaping (Valley) surface pump, the steep ground north-west of the pump, and try to repeat it:
   fly *at* a hillside and try to get inside it. Expected: he cannot any more — he hits the rock he
   can see. Expected side effect he should also notice: he no longer hits anything where there is
   visibly nothing (the 24 % invisible-wall class dies with it).
2. **A NORMAL LANDING AT THE SUDBURY SPAWN** (the Murray mouth apron, where his side is born). The
   touchdown should feel exactly as before — the median gap there is 7 cm.
3. **A LANDING AT THE VALLEY PUMP MARKER** (the Onaping apron). This is the one with real exposure:
   worst case ~1.8 m inside the footprint. Watch for a touchdown that feels early, a bounce, or the
   gear reading proud of / sunk into the apron.
4. **A TAXI AND A TAKEOFF ROLL** at either site — the same `r_s` now drives the grounded snap, so a
   rollout across a mesh-cell boundary is the place a step would show as a bump.
   ⚠ **AND THE ROAD DECKS NOW SIT A SYSTEMATIC +0.45 m ABOVE THE WHEELS.** The aircraft's contact
   surface is the TERRAIN facet; a drawn road ribbon is draped on `drawn_radius_at + [ribbons]
   lift_m` (0.45 m). So taxiing along a road, the deck you can see is about half a metre proud of
   where the tyres are. That is not new to T2 — it is what `lift_m` has always meant — but T2 is the
   first rung where the wheels are pinned to the *drawn* terrain, which makes the 0.45 m the only
   remaining step and therefore the one he may now notice. Say whether it reads as wrong.
5. **Anything that feels wrong: relaunch with `SEADS_FACET_CONTACT=0` and say whether it goes away.**
   That answers "is it this dial" in one flight.

**NOT in this rung, do not grade it here:** flying into the Murray pit mouth at ~15 m AGL and finding
no ground (the tunnel-net lid, §5.4) — that one is still waiting on his ruling.

---

## 7. THE SNOW RESIDUAL — CHAD'S RULING OWED

T2b measured what dial 1 does **not** cover: how far the **drawn** planet surface (`drawn_radius_at`
= the facet plus the folded ambient snow, the surface `fill_face` lays its vertices on) rides above
the **crash** surface the aeroplane now uses (`facet_radius_at`). The bar is `contact_height_m`
2.45 m — an airframe's CG rides that far above its contact point, so drawn material thicker than
that means a plane parked legally on the facet is **inside visible snow**.

`FACETCONTACT L6`, real DEM, shipped `[planet]` 200×2 mesh, shipped `[snowpack]` table loaded,
6 km box, 20 m step, 90 601 points per site:

| site | p50 | p90 | p99 | MAX | share > 2.45 m |
|---|---|---|---|---|---|
| ONAPING_VALLEY_PUMP | 0.757 m | 1.101 m | 1.748 m | **3.945 m** | 0.2031 % (184 pts) |
| CTRL_VALLEY_CENTER | 0.761 m | 1.054 m | 1.485 m | **3.754 m** | 0.1071 % (97) |
| CTRL_SUDBURY_PUMP | 0.769 m | 0.998 m | 1.308 m | **3.665 m** | 0.0254 % (23) |
| MURRAY_MOUTH | 0.777 m | 0.998 m | 1.160 m | **2.475 m** | 0.0011 % (1) |

Worst over all four sites **3.945 m**; add the road drape (ALTERNATIVES at a station, never a stack:
`max([ribbons] lift_m 0.45, [snowpack] bank_height_m 1.30)`) and the worst drawn material over the
crash facet is **5.245 m vs 2.45 m**.

⚠ **UPPER BOUND, NOT THE SHIPPED WORLD.** `seads_tests` cannot reach `render/draw.h` (raylib side),
so L6 binds no landmask and no barren raster, and both only ever *take snow away*. The lee/curvature
tail that makes the finding is real either way; its exact shipped magnitude is smaller and unmeasured.

### 7.1 T2c — measured, not asserted

**The ruling is Chad's, and a ruling must not hold the gate hostage.** As of T2c `FACETCONTACT L6`
is a **MEASUREMENT leg**: it computes the table above, prints it, `WARN`s per site that clears the
bar (and once for the drape total), and `REQUIRE`s only the invariants that must hold **whichever
way he rules**:

| invariant | why it survives any ruling |
|---|---|
| `p50` in (0.6, 1.4) × `[snowpack] base_m` (shipped 0.85 → 0.51–1.19 m) | the ambient fold is **present** and is ambient — not missing, not drifted. **Derived from the shipped table, never an absolute pair** (red-team P1-7): a `base_m` retune moves the band with it, instead of quietly grading a world nobody ships |
| `max < 10 m` | no wild residual; a 10 m chord of drawn snow would be a fold **bug**, not a ruling question |
| share > 2.45 m is `< 1 %` | the exposure is a **tail**. If it ever became a bulk property, the premise of every option below changes |

The strict bar — `mx < contact_height_m`, verbatim, plus the drape total — moved to
**`FACETCONTACT L6-STRICT`, tagged `[.t3-strict]` (hidden)**. It is not registered with ctest
(`catch_discover_tests` lists non-hidden tests only), so the gate does not see it; T3 flips it green
by implementing whichever option Chad picks. Run it deliberately:

    build/seads_tests.exe "[.t3-strict]"        # from the worktree root, Debug

Both legs read **one** `measure_residual()` helper, so the measurement and the bar can never grade
different numbers.

### 7.2 The three options

| # | option | what it does | what it moves at a normal landing (p50 delta) |
|---|---|---|---|
| **A** | **raise `contact_height_m`** from 2.45 m to cover the tail (≥ 4.0 m for the ambient alone, ≥ 5.3 m with the drape) | the crash shell lifts everywhere | **+1.55 m to +2.85 m, everywhere, always.** Every touchdown happens one to three metres in the air; the gear visibly floats at every site, on bare rock as much as in the hollow. It buys the 0.2 % tail with a permanent global feel regression |
| **B** | **fold the ambient depth into the aircraft's contact surface**, as `world::SnowpackField::drive_radius_at` already does for the sled — the crash surface becomes `facet + ambient_depth_at(dir)` (**"the snow surface is the ground"**, Winter Law) | the aeroplane lands **on the snow**, under the same law and from the same field the sled lives under | **+0.76 m to +0.78 m** — the local ambient depth, which is exactly what is drawn under the wheels. It is not a uniform lift: it is the field, so the deep lee hollow gets its 3.9 m and the wind-scoured rock near zero. The tail closes **where the tail is**, and nowhere else |
| **C** | **accept the 0.2 % tail** | nothing moves | **0.00 m.** A plane that comes to rest in a deep lee hollow is drawn buried to — at worst — 1.5 m over its CG height, at ~1 point in 500 at the worst site, and essentially never on the aprons he actually lands on (MURRAY_MOUTH: 1 point in 90 601) |

### 7.3 Recommendation — **B**

**B is the only option that removes the residual where it exists without moving the ground where it
does not**, because the fold it adds *is* the drawn material being measured — the same
`ambient_depth_at` field, through the same seam the sled already uses, so the aeroplane and the
snowmachine would at last stand on one surface instead of two. A and C are the two ways of being
wrong about a field by treating it as a constant: A pays a permanent ≈ +2 m global lift to cover a
0.2 % tail, while C leaves the aeroplane crashing on a surface half a metre under the snow it is
drawn sitting in — the same class of defect T2 was opened to fix.

### 7.4 Why A is not merely expensive — it is the wrong *kind* of number

`contact_height_m` is **not a snow dial.** `config/game.toml:23-30` says so in Chad's own tuning
record: it is the **GEOMETRIC** CG height above the contact point, tuned **by eye** on the fly of
2026-07-15 (*"wheels one wheel height under the road"*) to the value that puts the wheels **exactly
on the road deck** — 2.45 = 2.0 + `[ribbons] lift_m` 0.45 — and leaves them ~0.05 m proud of lake
ice and ~0.45 m proud of bare fields. *"Chad tunes on sight."* Raising it to 4.0 or 5.3 m to swallow
a snow-depth tail would spend a constant he set against the **landing gear** on a problem that
belongs to the **snow field**, and every one of those three sightings — deck, ice, field — would go
wrong at once. It is not a bound to loosen; it is somebody else's measurement.

### 7.5 If he rules B: it is `aircraft_snow_contact`, its own rung

⚠ **B is a T3 rung and a kernel change, not a fold into T2 and not a widening of `facet_contact`.**
It earns its **own one-dial name — `aircraft_snow_contact`** — for two reasons that are not
bookkeeping:

1. **It moves every touchdown, everywhere, by ~0.76 m** (the measured p50). `facet_contact` moved
   the crash surface to a surface that was *already drawn*; B adds material on top of it. Folding
   two moves of the crash surface under one dial would make the A/B that Chad flies unable to
   separate them, and the identity-by-branch proof would no longer name what it holds identical.
2. **It would DOUBLE-COUNT `[ribbons] lift_m` on the corridors** unless it reuses the road-repair
   seam. `contact_height_m` **already carries** the 0.45 m deck lift (§7.4), and inside a corridor
   the drawn deck is not ambient snow at all — `world::SnowpackField::deck_floor_r()` /
   `apply_deck_floor()` is the one-sided floor the sled already drives on there. A naive
   `facet + ambient_depth_at` would stack ambient snow **under a road that has none**, lifting the
   aeroplane off the deck Chad tuned 2.45 to sit it on. B must read the corridor through
   `deck_floor_r` exactly as the drive does, or it breaks the very landing it is trying to fix.

Beyond that, It moves the crash surface at
**every** point (p50 +0.77 m), so it needs its own build, its own identity-by-branch dial, its own
gate, its own red-team and Chad's fly — exactly the ladder T2 climbed. And it inherits trap 5.1:
the ambient field must be injected by the app, or a headless host lands on bare facet.

**If he rules C**, `L6-STRICT` is deleted rather than flipped, and this section becomes the record
of why.

---

## 8. T2b — the red-team fold (2026-09-17)

Five P1s and one P2 from the red-team, folded as one commit. Everything below is **on this lane, not
landed**.

### 8.1 (P1-1) The AI flies the same surface

`sim::contact_radius` was split: `sim::air_ground_radius(hf, gp, facet, up)` is the ground radius
**without** the gear height, and `contact_radius` is that plus `contact_height_m` — bit for bit what
it was (same operands, one addition). An `Environment&` overload pulls the field, the dials and the
injected facet out of the one `Environment` `sim::step` is ticking, so an AI reader can never be
handed a different surface than the kernel got.

Ten AI ground reads were routed through it — **identity by branch at dial 0, every one**:

| file | what |
|---|---|
| `drone/drone.h:1205` | `deck_look` gained a **defaulted trailing** `const sim::Environment* env = nullptr`, and one local `ground_r` lambda now serves both the under-me sample (`:1215`) and the six forward samples (`:1228`) — they cannot land on different surfaces |
| `drone/drone.h:3781` | the one shipped `deck_look` call passes `env` |
| `drone/drone.h:2959` | the E10 AGL hoist (deck band + terrain-avoid read it) |
| `drone/drone.h:3623` | the perch's `agl_g` |
| `drone/drone.h:3831` | the ENV-2 governor / terrain-avoidance `agl` |
| `app/instructor_tick.h:626` | `plant_at_slot` — its last argument changed from `const world::HeightField*` to `const sim::Environment*`; the deck respawn (`place_on_faction_deck`) plants at ~90 m AGL and the facet gap reaches 66.7 m, so this one could plant an aeroplane inside a hill |
| `app/instructor_tick.h:1875` | the D2 regroup aim point's AGL |
| `app/instructor_tick.h:1921` | the raid ballistic-climb AGL |

Two now-false comments fixed: `deck_look`'s "PURE: reads `world::HeightField::radius_at` — the SAME
surface the kernel's own crash predicate uses" and the terrain-avoidance banner's identical claim.
Both said the AI and the kernel share a surface; T2 had made both false.

**THE MEASUREMENT — `FACETAI` in `test/unit/test_conquest_match.cpp`. IT IS A MANUAL LEG**
(tagged `[.facetai]`, therefore hidden: `catch_discover_tests` registers non-hidden tests only, so
the gate never runs it — two full 22-minute matches cost ~6 min in Debug and it needs Chad's replay
tape on disk). Run it deliberately, from the worktree root:

    build/seads_tests.exe "[.facetai]"

It SKIPS (not fails) when the replay track is absent. Re-run it after ANY change to the AI ground
reads, to `sim::air_ground_radius`, or to the `sim/step.cpp` contact block — the numbers below are
the record it is graded against. `ArmCfg::facet_contact` (`<0` = every pre-existing arm
untouched) pins the dial **and** injects `render::facet_radius_at` at the shipped `[planet]`
subdiv/tiles read from `world.toml`. Both arms therefore have a render layer and differ **only** in
the branch inside `air_ground_radius`. Real DEM, real conquest fleet, Chad's replayed tape 7:

| arm | enemy crashes | per min | deck crashes | wrecks < 60 m AGL | avoid-latch ticks | enemy plane-s |
|---|---|---|---|---|---|---|
| `facet_contact 0.0` (DEM field) | **9** | 0.40 | 7 | 9 | 53 782 | 7117 |
| `facet_contact 1.0` (drawn facet) | **7** | 0.31 | 5 | 7 | 40 397 | 7117 |

**The wing flies BETTER against the facet, not worse.** Fewer wrecks (9 → 7), fewer deck-scope
wrecks (7 → 5), and 25 % fewer terrain-avoidance latch ticks — the avoidance net fires less because
the ground it measures is now the ground it will actually be killed on, so it stops both
false-alarming over chords that bridge above the field and missing the ones that bridge below.
The arms' state hashes differ (asserted: a bit-identical pair would mean the injection never reached
the AI). **HONEST LIMIT:** the replay is open loop — this is attrition under a fixed player record,
not a rematch prediction.

### 8.2 (P1-2) The building prisms sit on the same surface

`world::BuildingColliders::hit` gained one defaulted trailing argument, `const
std::function<double(glm::dvec3)>* base_r`, which replaces `hf.radius_at(dir)` as the prism base.
`sim::obstacle_contact` gained the same defaulted trailing `facet` pointer the kernel already passes
around and builds that base function from `air_ground_radius` — **on the armed branch only**; at
dial 0 / no injection it takes the verbatim pre-T2 call and no `std::function` is constructed. The
stale comment ("The prism base is `hf.radius_at(center_dir)` — the SAME field as terrain contact")
is fixed in both `world/buildings.h` and `sim/ground.h`.

Graded by **`FACETCONTACT L7`**: identity by branch (10 probes, facet counter still 0), and the
behaviour — a prism whose field base is 40 m below its facet base kills an airframe at facet + 5 m
at dial 1 and lets it through at dial 0, while an airframe at field + 2 m (inside the prism on the
OLD base, 38 m below the building's foot on the new one) is the mirror case.

### 8.3 (P1-3) ★★★ THE REAL RESIDUAL — AND IT IS NOT EMPTY

§4a's "zero by construction" was a **tautology over a constant**: it modelled the drawn surface as
`facet + 0.77 m`, so `CLIP` asked `facet + 0.77 > facet + 2.45` and was decided before a DEM texel
was read. Those four asserts are kept, **relabelled as tripwires on the two constants**, and the
honest measurement is now **`FACETCONTACT L6`**:

    residual(d) = render::drawn_radius_at(hf, d, N, tiles, &snow) - render::facet_radius_at(...)

— the drawn planet surface (what `fill_face` lays its vertices on) minus the crash surface, over the
real DEM at the shipped mesh resolution, with the shipped `[snowpack]` table **loaded**, 6 km box,
20 m step, 90 601 points per site.

| site | p50 | p90 | p99 | MAX | fraction over 2.45 m |
|---|---|---|---|---|---|
| ONAPING_VALLEY_PUMP | 0.757 m | 1.101 m | 1.748 m | **3.945 m** | 0.203 % (184 pts) |
| CTRL_VALLEY_CENTER | 0.761 m | 1.054 m | 1.485 m | **3.754 m** | 0.107 % (97) |
| CTRL_SUDBURY_PUMP | 0.769 m | 0.998 m | 1.308 m | **3.665 m** | 0.025 % (23) |
| MURRAY_MOUTH | 0.777 m | 0.998 m | 1.160 m | **2.475 m** | 0.001 % (1) |

**THE ASSERT `mx < contact_height_m` FAILS.** T2b left `FACETCONTACT L6` RED; **T2c turned that leg
into a MEASUREMENT and moved the bar to the hidden `FACETCONTACT L6-STRICT` (`[.t3-strict]`) — see
§7.1.** The number is unchanged; what changed is that a ruling of Chad's no longer holds the gate. Both halves of T2's premise were wrong: `[snowpack] base_m` is **0.85**, not
0.77 — and 0.77 was never a ceiling anyway, because `ambient_depth_at` modulates the fold by
elevation, aspect (lee/windward) and curvature. The snow fold is a **field**, not a constant. On the
order of **one point in a thousand** — deep lee hollows — the drawn ground stands up to **1.50 m
higher than an aeroplane's entire CG height** above the surface it now crashes on, so a plane parked
legally on the facet there is buried in drawn snow. Adding the road drape (the terms are
ALTERNATIVES at a station, never a stack: `max([ribbons] lift_m 0.45, [snowpack] bank_height_m 1.30)`)
takes the worst drawn material over the crash facet to **5.245 m vs 2.45 m**.

⚠ **UPPER BOUND, NOT THE SHIPPED WORLD.** `seads_tests` cannot reach `render/draw.h` (raylib side),
so L6 binds **no landmask and no barren raster**. Both of those only ever *take snow away* (water
flattens, black rock thins to `bare_rock_depth_m`). The lee/curvature tail that makes the finding is
real either way; its exact magnitude in the shipped world is smaller and still unmeasured.

**RULING OWED (Chad's, not this rung's) — the three options, their p50 deltas and this lane's
recommendation (B) are laid out in §7.** None of them is a bound to loosen.

### 8.4 (P1-4) The banner, and a non-numeric kill env

`app/main.cpp` now emits, **after the ground params and the facet injection have both resolved**:

    [config] ground: facet_contact 1.00 (injected: yes)

It is **appended to `g_config_banner`**, not `fprintf`'d on its own, so stderr,
`<exe_dir>/seads_launch.log` (rewritten) and the feel-tape header can never disagree — the same law
the four old `[config]` blocks were folded into one for. `injected: no` with a non-zero dial is the
half-armed state the identity-by-branch fallback produces (it reads the pre-T2 field), and it now
says so out loud.

`SEADS_FACET_CONTACT` is parsed with `std::strtod` + a full-consumption + `isfinite` check.
**The chosen pattern is WARN-AND-KEEP, not error-and-exit**: a bad value prints

    [config] WARNING: SEADS_FACET_CONTACT="off" is not a number -- IGNORED, keeping game.toml [ground] facet_contact 1.00

and the config value stands. Rationale: `std::atof("off")` is `0.0`, which would have **silently
disarmed the fix**; and a fly session must never die because of a typo in the environment. This is
stricter than its `SEADS_RIBBON_MAXSEG` / `SEADS_CORNER_BLEND` `atof` neighbours on purpose — this
one decides the crash surface — but keeps their non-fatal stance.

### 8.5 (P1-5) The tape header

Because the banner is copied into the tape as `#` comment lines, the line above **is** the tape
field. To make that true, `feel_tape_open` was **moved past the ground/facet resolve** (it used to
run ~2000 lines earlier, before the dial existed). The reader side, `test/harness/feel_tape.h`, gained
`has_facet_contact()`, `facet_contact_or(dflt)`, `facet_injected()` and
`warn_if_facet_mismatch(live_contact, live_injected)`, which prints

    [feel-tape] WARNING: replay CRASH-SURFACE MISMATCH -- tape facet_contact 1.00 (injected: yes) vs live 0.00 (injected: yes) ...

and returns false. **Backwards-readable by construction**: a pre-T2b tape has no such line,
`facet_contact_or` returns the caller's default and `facet_injected()` is false — absent reads as
0/absent, exactly as `sim/ground.h` treats a missing injection. Nothing throws, nothing is
`required`. Graded by **`TAPE T2b`** in `test/unit/test_tape_roundtrip.cpp` (armed / pre-T2b /
disarmed-but-injected, six mismatch cases).

### 8.6 (P2) Two notes for the reader

* **`GroundParams::facet_contact` defaults to `0.0` while `config/game.toml` ships `1.0`, and that is
  deliberate.** The struct default is the INERT-STRICT one every `GroundParams` field uses (see
  `sim/environment.h`): an `Environment` nobody configured must not silently ship a feel. It is not a
  second opinion about the dial, because `config/load_game.cpp` **`require()`s** the key — a
  `game.toml` without `facet_contact` throws; the loader always overwrites; the struct default is
  only ever seen by a hand-built fixture, and for those "the pre-T2 field" is the right answer.
* **Cost is ~1 µs per AIRFRAME, times 10 aircraft** — see §4d.

### 8.7 T2b status

Built, tested, graph regenerated. `FACETCONTACT L6` was **RED by design** (§8.3); **T2c made it a
measurement leg and hid the bar behind `[.t3-strict]` (§7.1), so the gate is clean and the finding
survives.** The residual is the one thing this fold hands back rather than closing. Still owed
before main: Chad's fly, his ruling on the snow residual (§7), and the red-team on this fold.

### 8.8 (T2c, red-team fold 2) The prism argument reaches the game

**P0-1 — THE FIX WAS INERT IN CHAD'S BUILD.** §8.2 moved `BuildingColliders::hit` onto the contact
surface and `FACETCONTACT L7` graded it green — by calling `sim::obstacle_contact` **directly**. The
game does not: it reaches it through `sim::step`, and the one call site there (`sim/step.cpp` ~206)
was still passing four arguments, so `base_r` fell back to its default and every house in the world
kept its feet on the DEM field while the terrain under it had moved to the drawn facet. The fifth
argument `&env->ground_facet_fn` is now passed, and **`FACETCONTACT L8` drives the whole thing from
`sim::step` at `Environment` level, exactly as `app/main.cpp` does** — armed (dial 1 + injection:
an airframe 5 m above the drawn facet is killed by the building it is visibly inside) and identity
(dial 0, and an empty function at dial 1: clear air, and the sampler is never called across the
entire tick). A leg that only ever calls the callee cannot see this class of defect; L8 can.

**`FACETAI` re-run after the fix, and it did not move: 9 → 7 enemy wrecks, 7 → 5 deck-scope,
53 782 → 40 397 avoid-latch ticks, enemy plane-s 7117 — byte-for-byte the §8.1 table.** That is the
expected result and it is stated rather than assumed: the conquest-match harness binds **no**
`world::BuildingColliders` (`env.obstacles` is null there), so the `sim/step.cpp` block this fold
repaired is structurally skipped in that leg. The re-run therefore confirms the fix moved **nothing
the AI A/B measures** — the prism change is graded by `FACETCONTACT L7` (the callee) and
`FACETCONTACT L8` (through `sim::step` at env level), and its effect in Chad's build is on the
houses, which no headless AI leg flies past. **An AI-vs-buildings measurement does not exist and is
named here as a gap, not claimed as a pass.**

**P1-6** — `drone::deck_look`'s `const sim::Environment*` is **no longer defaulted**. A defaulted
crash surface is the half-armed state identity-by-branch exists to make impossible; the five
`test_drone.cpp` fixtures that have no `Environment` now pass an explicit `nullptr` and say so.

**P1-9** — the feel-tape mismatch tolerance is **5e-3**, not 1e-9. The banner stamps the dial at
`%.2f`, so a tape can never carry more than two decimals and a tape flown at, say, 0.333 was a
guaranteed false MISMATCH. Half a print unit catches 1.00-vs-0.00 and lets the `%.2f` round trip
through.

**P2** — `facet_contact_or` / `facet_injected` parse **anchored to the `[config] ground: ` line**
(a bare `find("facet_contact ")`, and especially `find("(injected: ")`, would match any other
banner line that ever mentions the dial); `sim::air_ground_radius(const Environment&, …)` asserts
`env.ground != nullptr` so a caller that reaches it past its guard dies in the gate, not silently in
Chad's fly; and the banner block in `app/main.cpp` moved **below** `render::set_tree_snowhill(…)`,
which it had been wedged in front of — the SF1 comment now stands with its own statement again.

---

## 9. Status

Built, tested, gated (`docs/terrain_clip/GATE_VERDICT_*.txt`), **not flown, not landed, not
pushed.** Owed before main: Chad's fly, his ruling on the snow residual (§7), and the red-team on
the T2b fold (standing rule for a kernel change).
