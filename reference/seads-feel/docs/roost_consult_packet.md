# CONSULT PACKET — S5 ROOST, and whether the snow meets the palpable precision standard

Branch `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow`, base `f92939e2e` + uncommitted
R1/R2 work. C++17, raylib 5.5 + OpenGL 3.3. Planet radius **R = 15 000 m**, the whole surface real
Greater Sudbury. The rideable machine is a snowmobile ("the sled").

**You are being consulted context-free. Everything you need is here. Verify the file:line citations
rather than trusting them, and say plainly if any claim below is wrong or overstated.**

---

## 1. THE ASK, IN THE OWNER'S WORDS

Chad (the owner; the only authority on feel), 2026-08-27, verbatim:

> "yes when you say float do you mean the felt displacement at any given height within the
> snowcolumn that I am riding over. The conditions should be palpable and observable in the sled
> performance, also I think this is the time to make sure we make a note for the amount of roost
> (rearward thrown snow about 30 to 50 ft), volumetrically variable per snow depth and throttle
> application and duration. So I want to make sure our snow is meeting the palpable precision
> standard."

Earlier the same thread, on why a purely visual fix was rejected:

> "thats not the only point I should see skis going into snow, deeper snow should bury me a bit,
> when I give throttle and plane out I should be planing out over this snow and not just making
> tracks but depressing snow... Snow that is properly shaded and felt, not some cheap drawn in
> illusion"

**So the standard is: snow depth must be palpable and observable in machine performance, and roost
must be thrown ~30–50 ft (9–15 m) rearward, volumetrically variable with depth, throttle, and
duration.**

---

## 2. WHAT EXISTS

### 2.1 The one number
`sim/sled.h:1076-1078`, verbatim:

> `// ★★ THE ONE NUMBER, TWO CONSUMERS (§3.5a). |slip| x available_snow. S5's roost scales off THIS
> field and the escape thrust is computed FROM it -- fork them and the most legible feedback signal
> in the game starts lying.`
> `double roost_flux = 0.0;`

`docs/winter_s3_spec.md:139` adds the kill mechanism:

> "`roost_flux` is stored in `SledState`. **S5's roost must scale off this field and nothing else**
> ... `bury_frac` is `clamp(sink_track / track_clearance_m, 0, 1)`: a buried track has no free cleat
> to throw snow with, which is simultaneously why the roost dies and why the escape fails."

### 2.2 The felt half is built and deeply tuned
`sim/sled.h` carries Bekker pressure-sinkage, `bury`, `plane_frac`, planing lift, roost flux,
`track_clearance_m` (0.255), `roost_ref_depth_m` (0.550), `roost_gain` (0.550), and a cold-stiffened
pack chain. Machine dimensions, measured (`sim/sled.h:398-405`): `stance_m 0.927`,
`ski_width_m 0.135`, `ski_len_m 0.95`, `track_width_m 0.38`, `track_len_m 1.14`.

### 2.3 The drawn half does not exist
`render/draw.h:451` — `double sled_roost_flux` is *"the ONE product S5's roost **will** scale off"*.
`render/draw.cpp:4240-4241` — *"the S5 roost rung replaces this with the real roost-coupled
system."* Both future tense. **Today `roost_flux` drives a HUD bar (`render/draw.cpp:2509`) and
nothing else. No snow is thrown.**

Existing particle machinery that a roost could plausibly reuse: `render/precip.{h,cpp}` +
`render/precip_draw.*` (snowfall), `render/smoke.{h,cpp}`, and `combat/kill.h`'s `FxPool` /
`Fx` (`FxKind { HitSpark, Explosion, Touchdown }`) — a deterministic pooled particle system with
`pos`/`vel`/`age`/`seed` and free-list reuse. There is **no** sled spray/powder FX of any kind.

### 2.4 The rest of the world state (context, not the subject)
An in-progress rung ("R1") just folded the ambient snow depth into the RENDERED planet mesh, closing
a ~0.77 m see-vs-drive gap. A track-deformation field (`world/tracks.*`) composes into
`drive_radius_at` so the machine feels its own ruts. Neither has changed any physics: the driven
surface is provably bit-identical (173,118-sample census, byte-for-byte).

---

## 3. THE MEASUREMENT (taken this session, `tools/sled_probe.cpp roost`)

Flat analytic bush, machine settled before the throttle opens, terminal state reported.
Columns per throttle: `flux / speed m/s / plane_frac / sink_track m`.

**Throttle held 6.0 s:**

| depth m | thr 0.25 | thr 0.50 | thr 0.75 | thr 1.00 |
|---|---|---|---|---|
| 0.05 | 0.002 / 11.1 / 0.14 / 0.04 | 0.020 / 17.1 / 0.28 / 0.03 | 0.032 / 20.8 / 0.36 / 0.03 | 0.037 / 24.9 / 0.45 / 0.03 |
| 0.15 | 0.013 / 10.8 / 0.23 / 0.06 | 0.072 / 15.3 / 0.37 / 0.05 | 0.090 / 20.9 / 0.53 / 0.04 | 0.121 / 21.4 / 0.55 / 0.04 |
| 0.30 | 0.040 / 10.2 / 0.27 / 0.09 | 0.139 / 14.9 / 0.46 / 0.07 | 0.188 / 19.0 / 0.60 / 0.06 | 0.246 / 19.0 / 0.60 / 0.06 |
| 0.45 | 0.078 / 9.3 / 0.23 / 0.13 | 0.195 / 14.8 / 0.51 / 0.08 | 0.294 / 17.1 / 0.62 / 0.07 | 0.366 / 17.1 / 0.62 / 0.07 |
| **0.60** | 0.097 / 8.7 / 0.19 / 0.15 | 0.226 / 14.6 / 0.54 / 0.10 | 0.348 / 15.6 / 0.60 / 0.09 | **0.419** / 15.6 / 0.60 / 0.09 |
| **0.77** (bush) | 0.083 / 8.0 / 0.16 / 0.19 | 0.216 / 13.7 / 0.54 / 0.12 | 0.318 / 14.1 / 0.56 / 0.12 | **0.373** / 14.1 / 0.56 / 0.12 |
| 1.00 | 0.042 / 7.3 / 0.13 / 0.23 | 0.145 / 10.8 / 0.37 / 0.19 | 0.216 / 11.4 / 0.42 / 0.17 | 0.247 / 11.5 / 0.43 / 0.17 |
| **1.50** | **0.000** / 6.2 / 0.09 / 0.30 | **0.000** / 6.2 / 0.09 / 0.30 | **0.000** / 6.2 / 0.09 / 0.30 | **0.000** / 6.2 / 0.09 / 0.30 |

**Duration matters strongly.** Peak flux across the whole grid: 0.5 s → 0.106; 2.0 s → 0.232;
6.0 s → 0.419.

**Two findings worth your attention:**

1. **The good news.** Over 0.05–1.00 m, flux is richly variable in all three of the owner's axes —
   depth (a clear hump peaking near `roost_ref_depth_m` 0.550), throttle (0.083 → 0.373 at bush
   depth, a 4.5× span), and duration (0.106 → 0.419). His requested variability already exists as a
   number. An earlier in-repo note (`sim/sled.h:512`) recording "roost_flux 0.000" appears to be one
   pinned bog scenario, not the general case — worth your check.
2. **The cliff.** At 1.50 m depth flux is **exactly 0.000 at every throttle and every duration**, and
   speed / plane_frac / sink are **bit-identical across all four throttles** (6.2 / 0.09 / 0.30).
   Once bogged, throttle input changes *nothing measurable*. `sink_track` 0.30 exceeds
   `track_clearance_m` 0.255, so `bury_frac` saturates and the term dies — which
   `docs/winter_s3_spec.md:139` says is intended. The same regime appears at 1.00 m for short
   applications (0.5 s and 2.0 s both 0.000).

---

## 4. QUESTIONS

**Q1 — Is `roost_flux` sufficient as the sole driver of a drawn roost?** The contract says S5 "must
scale off this field and nothing else." But the owner's spec is *volumetric* — a mass of snow thrown
30–50 ft. Flux is `|slip| × available_snow`, dimensionless-ish. What is the honest mapping from this
one scalar to (thrown mass, exit velocity, cone angle, throw distance) such that 9–15 m of throw
falls out at high flux? What ADDITIONAL state (track speed, `thrust_n`, `depth_under_m`) may a
renderer legitimately read without violating "nothing else", and where is the line between reading
state and re-deriving physics?

**Q2 — The bog cliff: correct physics or a palpability defect?** At ≥1.0–1.5 m, throttle does
nothing to any reported quantity. Physically a bogged sled does stop roosting. But the owner's
standard is that "conditions should be palpable and observable in the sled performance", and a
regime where the throttle is disconnected from every observable is the opposite. Is the correct
answer (a) leave the kernel alone and make the bog legible some other way (audio, visual spray
collapse, HUD), (b) a kernel change, or (c) is the 1.5 m case simply outside real bush depth
(measured ambient bush is 0.71–0.77 m) and therefore not worth a ruling?
**Constraint: `sim/` and `control/` are a FROZEN kernel. The world thread may not touch them. Any
kernel change is a separate owner-ruled feel session, one dial at a time, with him on the stick.**

**Q3 — Volumetric conservation.** The owner wants "not just making tracks but depressing snow". A
track-deformation field already cuts a trench into the DRIVEN surface. Should thrown roost mass be
conserved against that trench (snow thrown = snow removed), and if so how, given the trench is a
persistent field and the roost is transient particles? Is conservation worth the coupling, or is it
a trap that will read worse than two independent effects?

**Q4 — The rendering vehicle.** Given `FxPool`/`Fx` (deterministic pooled particles with
pos/vel/age/seed) and the precip system already exist: particles, a mesh plume, a shader-driven
billboard cloud, or something else? At 9–15 m throw with a wide cone, roughly what particle budget
does a convincing roost need, and does the existing pool shape support it? Note this project's
history: an earlier effect was rejected by the owner as "a cheap drawn in illusion", so a
particle spray that does not track the physics will be rejected.

**Q5 — Is the FELT half actually complete?** The owner's first sentence asks whether "float" means
felt displacement in the snow column. It did not — it was a visual registration number. So: given
the table in §3, is depth already palpable through the machine (speed at bush 0.77 m spans
8.0 → 14.1 m/s across throttle; `plane_frac` spans 0.16 → 0.56), or is something missing that would
make depth *felt* rather than merely *true*? What instrument would settle that, given no automated
test may judge feel and only the owner's drive can rule it?

**Q6 — The rung order**, such that each step is independently drivable and judgeable by the owner.

### Constraints binding any answer
- `sim/` and `control/` are FROZEN. No kernel edits from this thread.
- No automated test runs the app binary; a green gate proves nothing about anything the owner sees
  or feels. Every visual/feel claim needs a screenshot or his drive.
- House law: **no guessing.** Every number describing a real thing comes from a measurement or the
  owner's words.
- "One number, two consumers" (§3.5a) is a standing law: escape thrust and the drawn roost must read
  the same product, computed once.
