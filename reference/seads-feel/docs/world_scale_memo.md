# World-Scale Design Memo — planes, horizon, bubbles, the 100 m floor, trees

*Design note, not code. Answers Chad's scale questions with derivations from the live config
(`config/aircraft.toml`, `config/controller.toml`) and the canon plan
(`D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md`). No files besides this one were touched.*

## Feel

A 15 km sphere with a 100 m outside-bubble floor flies like this: in the open, the world is
close enough that your own turn circle is a scratch on the globe, but the *horizon* is what
you actually feel — on the deck it's barely more than a football field's worth of ridgeline
ahead of you, so terrain and bandits alike rise up at you rather than being seen coming.
Force a straggler down to that floor and the sim stops being about vertical energy and
becomes a white-knuckle hedge-hop with a very short sightline — which is either exactly the
"come home or die trying" pressure Chad wants, or an invisible-tree-branch simulator,
depending on how the assist and collision rulings below land.

---

## 1. How big are the planes on a 15 km map?

**Scale ratio.** `config/aircraft.toml` does not model a wingspan directly (mass = 3000 kg,
wing area S = 16 m² — `config/aircraft.toml:33-34`); a WWII single-seat fighter with that wing
area implies a span in the Bf 109 / Spitfire family, ~10-11 m (Bf 109 = 9.9 m, Spitfire =
11.2 m — UNVERIFIED against this repo, no span constant exists; used as the reference class
the MASTER_PLAN itself cites for turn-radius sizing, `MASTER_PLAN.md:161`). Call it **span ≈
10 m**. Against R = 15,000 m (`config/aircraft.toml:7`) that is a **1:1,500 ratio** — a plane
is to the planet roughly as a ~1.8 m person is to a **2.7 km hill** (1.8 m × 1,500).

**Angular size at engagement range**, θ ≈ span / distance (small-angle):

| Range | θ (deg) |
|---|---|
| 200 m | 2.9° |
| 500 m | 1.1° |
| 1 km | 0.57° |
| 3 km | 0.19° |

A bandit at 3 km is a fifth of a degree — a speck; gun/visual engagement in practice happens
inside ~1 km where the target subtends ≥0.5°, consistent with the existing gunsight cone
(`config/scenario.toml:47` `track_cone_deg = 45`, `config/scenario.toml:42` `hit_cone_deg = 2`).

**Circumference in flight time.** 2πR = 94,248 m ≈ 94.2 km. At combat speeds:

| Speed | Lap time |
|---|---|
| 140 m/s (`v_full`, the compression knee — control effectiveness begins tapering here, not the turn corner; per `config/aircraft.toml` comments. The actual corner speed is 255.7 m/s, per `vessel_ontology.md` R2, and it sits past `v_redline` — unreachable) | 673 s ≈ 11.2 min |
| 250 m/s (near redline, MASTER_PLAN §3.B cites 245 m/s redline) | 377 s ≈ 6.3 min |

Matches the SPEC.md framing ("a lap takes ~9 minutes") — a fight that drifts a full
circumnavigation is a 6-11 minute affair, i.e. curvature is a *cross-map* fact, not something
you'd stumble into inside one merge.

**Turn circle vs. the sphere.** r = V² / (g·√(n²−1)) (g = 9.81, `config/aircraft.toml:8`; this
is the same formula MASTER_PLAN uses for the tunnel chamber, `MASTER_PLAN.md:161,305`):

| V | n | r (m) | diameter (m) | diameter as fraction of R |
|---|---|---|---|---|
| 140 m/s | 4 g | 516 | 1,032 | 6.9% |
| 140 m/s | 6 g | 338 | 676 | 4.5% |
| 140 m/s | 8 g | 252 | 503 | 3.4% |
| 250 m/s | 4 g | 1,645 | 3,290 | 21.9% |
| 250 m/s | 6 g | 1,077 | 2,154 | 14.4% |
| 250 m/s | 8 g | 803 | 1,606 | 10.7% |

The subtended arc of even the *largest* of these (3.29 km at 250 m/s / 4 g) is
3,290/15,000 ≈ 0.22 rad ≈ 12.6° of the great circle — small, but not negligible; a sustained
4 g turn at redline eats a noticeable slice of the world. At fighting speeds (140-200 m/s) and
typical sustained-turn g the circle is 500 m-1 km, i.e. under 1% of R.

**Conclusion — does it read "small planet" from the cockpit?** Not from turn geometry alone —
a single turn circle is locally flat and won't register as curvature by itself (this matches
`docs/TEACHING.md`'s own framing: curvature is felt through *holonomy over a lap*, camera-up
rotation after a banked circuit, and the horizon bulge — not through the turn radius itself,
`docs/TEACHING.md:229-238`). Where the world DOES read small: (a) the horizon distance itself
(§2 below) is dramatically closer than real-world experience at any altitude above ~500 m,
and (b) full-lap flights (6-11 min) are short enough that circumnavigation is a realistic
in-mission event, not an abstraction. The 1:1,500 plane:planet ratio is closer to a large
offshore island than a real Earth-scale flight sim — intentional, per SPEC's framing (R is
"the feel knob," `config/aircraft.toml:7`).

---

## 2. Horizon on the deck

d ≈ √(2·R·h), R = 15,000 m. Note this is the small-h APPROXIMATION of the exact spherical
horizon distance `R·acos(R/(R+h))` — it's within a fraction of a percent at the altitudes this
table cares about, but drifts as h grows (+4.9% at h=3,000 m, the last row below), so treat the
3,000 m row as directional, not exact:

| h (m) | horizon distance d | time to close at 140 m/s | time to close at 250 m/s |
|---|---|---|---|
| 50 | 1,225 m | 8.75 s | 4.9 s |
| 100 | 1,732 m | 12.4 s | 6.9 s |
| 500 | 3,873 m | 27.7 s | 15.5 s |
| 1,000 | 5,477 m | 39.1 s | 21.9 s |
| 3,000 | 9,487 m | 67.8 s | 37.9 s |

At h = 100 m the horizon sits at **1.7 km** — startlingly close, exactly as the brief flags.
For reference, MASTER_PLAN's already-planned "Global Deck" sits at 120 m AGL
(`MASTER_PLAN.md:68`), whose horizon is 1,897 m — the same order of magnitude.

**What this does to on-the-deck fights and spotting.** A bandit converging head-on at combat
speed closes the horizon-to-merge distance in single-digit seconds at 100 m AGL (6.9-12.4 s
depending on speed) — noticeably less reaction time than the "several-second visual pick-up"
a real-world low-level intercept affords, because on Earth the horizon at 100 m is ~35.7 km
(no curvature-imposed cap at fighter ranges; terrain masking is the only real limiter). On
SEADS's 15 km sphere, curvature itself becomes the limiter at low altitude: bandits genuinely
"rise over the horizon like ships" rather than being spotted as distant dots and tracked in.
This is a *feature* for the tactical read Chad is after (deck fights are surprise-merge fights
by construction) but it also means AI/HUD spotting aids matter more on the deck than they do
at altitude — a player relying on eyeball-only spotting at 100 m has under 10 seconds from
"first visible" to "in gun range."

---

## 3. Bubble sizing (contain a corner-speed turn fight)

Using the §1 turn-circle table, a fight needs room for (a) the primary turning circle itself,
(b) merge/extend geometry (planes disengage and re-enter, not just circle), and (c) more than
one engaged pair. MASTER_PLAN's own underground-chamber sizing used the identical formula and
landed on "~600-800 m across" for a single Bf 109-class turn at 90-130 m/s / 3 g
(`MASTER_PLAN.md:159-165`) — a *minimum* single-plane-pair envelope, explicitly "gameplay-sized,"
not authentic.

At SEADS's actual combat speed band (140-250 m/s) and a representative sustained-fight g
(4-6 g — 8 g is a brief defensive spike, not a sustained turn-fight g per the n_max=32 note
that lift, not the G-clamp, is the real ceiling at these speeds, `config/controller.toml:117-124`):
single-turn diameters run **676 m - 2,154 m** at 140-250 m/s / 6 g, up to **3,290 m** at
250 m/s / 4 g.

**Recommendation:** size the fight-bubble diameter at roughly **4× the typical sustained-6g
turn diameter** (676 m at 140 m/s to 2,154 m at 250 m/s) to give a multi-plane furball room to
extend, re-merge, and not bump the bubble edge mid-turn — that puts the honest range at
**~3 km (tight, low-speed knife fight) to ~8 km (open, high-speed slashing fight)**. A single
fixed number is a judgment call for Chad; the derivation above is the honest floor. Note this is strictly a fight-bubble
(air-combat containment) number, separate from the town/economy bubble radius MASTER_PLAN
already treats as *runtime state*, not a fixed param (`MASTER_PLAN.md:391-396`).

---

## 4. The 100 m atmosphere FLOOR idea

**Framing check (important):** the CURRENT `[atmosphere]` mechanism in `config/aircraft.toml`
(lines 12-30) is a **ceiling** — full performance (f = 1.0) below `taper_alt = 4000 m`,
Gaussian falloff above. Chad's ask *inverts* this: outside a fight-bubble, the good-performance
band should **collapse toward the ground** (down to ~100 m) instead of collapsing at altitude
— so a straggler caught in no-man's-land loses power/lift the moment they climb, not the moment
they go too high, and the ONLY way to keep flying well is to hug the 100 m deck (where the
horizon is 1.7 km, per §2) and run for the bubble.

**(a) Mechanism shape.** This is not a new formula — it's the *existing* Gaussian taper
(`atm_frac(h) = exp(-max(0, h-taper_alt)²/(2·taper_sigma²))`, `config/aircraft.toml:13-16`)
fed a **position-dependent `taper_alt`** instead of the current constant 4000 m:

```
taper_alt(pos) = lerp(100 m, 4000 m, bubble_membership(pos))
atm_frac(h, pos) = exp(-max(0, h - taper_alt(pos))^2 / (2*taper_sigma^2))   # UNCHANGED math
```

`bubble_membership(pos)` is exactly the smooth-max-of-domes shape MASTER_PLAN already scoped
for `AtmosphereField` (§3.C: deck + N spherical-cap bubble domes, edge softness ~50 m,
`MASTER_PLAN.md:391-396`) — inside/near a bubble, `taper_alt` reads the full 4000 m combat
ceiling; far outside every bubble, it collapses to 100 m. Keeping the *math* identical and
only swapping the *input* (a scalar field replacing a scalar constant) is the least invasive
version of this ask — it reuses the "one number, everywhere" plant/controller call sites
without inventing a second falloff shape.

**(b) What 100 m demands, performance-wise.** Terrain-following at 140-250 m/s over Sudbury's
DEM: the world-build docs cite **350 m of theatrical relief** for the current bake
(`MASTER_PLAN.md:38`; CLAUDE.md's earlier brief said "100-300 m" — treat 350 m as the more
authoritative live number, UNVERIFIED beyond that citation since `render/` in this worktree
has no tree/terrain assets to inspect directly). A hill rising 150 m over a 1 km horizontal
run, crossed at 200 m/s (5 s), needs a sustained ~30 m/s climb rate to clear at 100 m AGL —
**the "~20+ m/s at combat speed" citation this used to lean on is the SUPERSEDED T_max=9000
comment block** (`config/aircraft.toml:50-59` is annotated as stale by its own v5 RUNG D note);
the LIVE table's excess-power climb rate, `(T−D)·V/W` at V=140 with the current T_max=18,000 N,
works out to **~66.6 m/s** — trivially inside the 30 m/s requirement, more margin than the old
citation implied. The SAME 150 m rise over a 300 m run (1.5 s at 200 m/s) needs ~100 m/s
vertical rate — still **out of reach** (66.6 < 100), but much closer to achievable than the
stale ~20+ m/s number suggested (66.6/100 ≈ 67% of the way there, vs ~20%). Combined with the
§2 horizon (1.7 km at 100 m ≈ 8.5 s of warning at 200 m/s, and real terrain masking gives *less*
warning than the curvature horizon, since a nearer ridge can block line of sight to a farther
one): **a straggler blind-flying the 100 m floor over Sudbury's ridge country will still crater
into the steepest terrain at a meaningfully high rate**, independent of enemy action, even
though the live climb margin is better than previously documented.

**(c) What it does to the fight.** Energy fighting (zoom climbs, altitude banking, BnZ)
becomes structurally impossible outside a bubble — thrust/lift starve the moment you climb
above ~100-200 m out there, per the existing bubble-edge design language ("mushy — engine
dies — nose drops," `MASTER_PLAN.md:207`). Outside a bubble a straggler is reduced to a flat,
terrain-hugging scramble, itself capped by the terrain-avoidance math in (b) — which **is**
squarely Chad's stated intent ("really force stragglers into the bubble"). The named risk:
if the failure mode is mostly "flew into a ridge" rather than "got shot down," the mechanic
reads as a terrain-collision tax, not a tactical squeeze — worth flagging before it's built,
not after a play session's worth of confused crash respawns.

**(d) The honest cost.** SPEC §6's sphere-invariant purity ("every up/level/bank recomputed
from `normalize(position)`... never cached... never a global axis") is about *reference
frames*, not this specifically — the real cost is architectural: `rho_at(altitude)` today is
a single-source function with (per MASTER_PLAN's own census) **~8 consumers**: the plant
(`sim/step.cpp`), controller inversion (`control/controller.h` / `controller.cpp`),
`ang_accel_max_derived` + `load_factor` (`sim/aero.h`), the harness instructor trim, the drone
stall guard, wind audio, and the AT-18a/b config-side reconstructions
(`MASTER_PLAN.md:378-383`). A position-dependent atmosphere means **every one of those 8 call
sites must migrate together**, or it's exactly the H1-class double-counting fork SPEC bans
(dynamic pressure applied more than once, in disagreeing places). This is not a config
tweak — it IS the kernel-v4 `sim::Environment`/`AtmosphereField` change MASTER_PLAN already
scoped as its single riskiest, most-adversarially-reviewed kernel touch
(`MASTER_PLAN.md:360-397`), gated behind an all-null-bit-identical regression and its own
review round. **The 100 m-floor idea is a ruling on top of that already-planned mechanism,
not a standalone one-line change** — it should land in that phase, not earlier.

---

## 5. Collidable trees

This worktree (`seads-feel`) has no `render/` tree code to inspect directly (world/art lives
on `sandbox/world-sudbury` / `sandbox/planet-art`); per `docs/world_art_direction.md`, trees
are the primary **orientation instrument** ("known-size 3D features... standing radially up,"
`docs/world_art_direction.md:54-57`) and the current ruling is explicit: **"Visual-first: no
collision in v1... Collision/consequence is a later, deliberate, logged ruling"**
(`docs/world_art_direction.md:69-71`). MASTER_PLAN's tunnel-lamp section references "the same
instancing pattern as the tree props" (`MASTER_PLAN.md:174,287-288`), confirming trees are
GPU-instanced billboards/meshes, not individual physics bodies — UNVERIFIED exact count in
this worktree, but for scale: the building bake alone is 87,584 instances across the whole
map with spatial tiling + horizon/distance cull (per the memory ledger on world-build); a
forest layer at typical density would plausibly be 1-2 orders of magnitude denser than that.

**Broadphase cost, if collision were added.** The plant already has a planned single-source
terrain query, `h_terrain(dir)` (MASTER_PLAN §3.A, `MASTER_PLAN.md:259-268`), evaluated once
per tick against the DEM. Reusing the SAME spatial-tile bucketing already built for building
culling would make a tree broadphase O(1) amortized (test only the current terrain cell's
tree bucket) — the *engineering* cost is modest and follows an existing pattern; it is not the
binding constraint.

**The binding constraint is gameplay feel.** Hard collision on every background tree at
140-250 m/s in an arcade dogfighter is unforgiving: a single clipped branch = instant crash,
across a 94 km circumference of forest. Real-world arcade-mode WWII sims typically do NOT
collide small/background trees at all, or make them soft past a size threshold — full
hard-collision forestry reads as "invisible foliage kills," not skill expression.

**Recommendation — tiered:**
- **Background/ambient trees:** stay NO-COLLIDE (keep the current, explicit v1 ruling).
- **Hero trees** (large, individually authored, placed near bubble edges / landmarks — the
  same distinct authored class as the Copper Cliff Superstack / church heroes, reusing the
  `blender-hero-forge` pattern) get real collision, using the identical swept-crash-predicate
  pattern already planned for terrain and tunnel walls (`MASTER_PLAN.md:261-262,291-297`).

This makes tree hazard rare, legible, and attributable ("that WAS a big tree") instead of an
omnipresent invisible tax — and costs nothing architecturally beyond what MASTER_PLAN's ground
query already plans to build.

---

## Rulings needed

1. Is "stragglers crash into terrain, not just lose the fight" an ACCEPTED cost of the 100 m
   floor, or does it need a terrain-avoid assist before it ships? (yes/no)
2. Does the 100 m-floor idea wait for the kernel-v4 `Environment`/`AtmosphereField` phase
   (§4d), or does Chad want an earlier, cheaper approximation first? (wait / earlier approx)
3. Approve the fight-bubble diameter range **~3-8 km** (§3), or specify a single canonical
   number now? (approve range / give a number)
4. Approve the tiered tree-collision plan — background soft, hero trees solid (§5)? (yes/no)
5. Should the 100 m floor apply GLOBALLY outside every bubble, or only in a designed
   "no-man's-land" band between specific bubble pairs (leaving distant open ocean/wilderness
   untouched)? (global / band-only)
