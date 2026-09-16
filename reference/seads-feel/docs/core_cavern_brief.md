# T10 — THE HOLLOW CORE (design brief, pre-consult)

> **STATUS: SUPERSEDED-BY-STAGING-T12 (2026-07-19).** Chad FLEW T10 → ruled out the
> concentric shell → T11 THE CORE WINDOW (arena + core-window shaft) → then FLEW T11
> and ruled the core window OUT too ("cover up the core... there are holes going to
> it and I can see through the earth... make sure the chamber is filled over the
> core"). T12 — THE SEALED CORE + MEND THE MESH — seals the arena floor (the shaft +
> emissive core render are GONE; cavern_core_m kept as the buried |position| safety
> floor), tightens the Errington entry crater (kPitRimFactor 2.0→1.3, fixing the
> half-tunnel blocker + the see-through crown), adds mouth beacon rings
> (findability), and adds a mesh-integrity verifier. The live status, final dials,
> screenshot verdicts, and round-10 fly card live in `docs/tunnel_staging.md`
> (### T12) + `docs/tunnel_fly_cards.md`. Gate 738/738. This design record (the T10
> shell sketch) is kept intact for history.

**Chad's ruling (2026-07-19, verbatim):** "I think I got to the egg but had no visual
reference its dirt so it has to be huge and I cant scrash right when I get into it so
make it a huge inner space where a bunch of planes could dogfight. Make it a huge
chamber and there is the core to the 15km sphere that lights the cavern. Its really
big, in there make it a huge underground space where you can chase someone ... a short
tunnel.. there entrance needs to be better lit and so I can see the angle of entry,
its fun to fly into a tunnel and the egg should be about 100x the size it is now we
will start with that number ... we should see the different layers of the planet asi
in core outercore mantle crust. and well lit by the core. two ways in two ways out we
already have the tunnel."

## The felt spec, decomposed
1. HUGE inner space — a bunch of planes could dogfight; you can chase someone. No
   crashing blind on entry (the old egg was unlit dirt with zero visual reference).
2. The PLANET'S CORE sits in the middle of the 15 km sphere and LIGHTS the cavern.
3. The planet's LAYERS read: crust / mantle / outer core / (inner) core.
4. SHORT tunnels; entrance better lit; the ANGLE OF ENTRY must read from outside.
5. Two ways in, two ways out — the existing Errington + Murray mouths.
6. "~100x the size it is now, we will start with that number."

## Proposed shape (for the consult to attack)
The egg stops being a buried pocket. The interior becomes CONCENTRIC:

- **Inner core (solid, glowing):** a ball at the planet center, radius `core_m ≈ 2500`.
  Solid = crash surface. This is load-bearing for MORE than looks: it floors
  `|position|` so `local_up = normalize(position)` and every `/|position|` (S-ffrad
  curvature ff) never approach the r→0 singularity. Emissive silver-white (mono law).
- **The cavern = the OUTER CORE, hollowed:** flyable shell from `core_m` out to the
  cavern ceiling `cavern_ceiling_m ≈ 10500` (~8 km of radial airspace, volume
  ≈ 4800 km³ ≈ 3500× the old egg ≥ Chad's 100× floor — "start with that number" read
  as a floor, stated honestly on the card so he can re-dial DOWN if it's too big).
- **Mantle + crust = the remaining rock shell** 10.5 → 15 km, seen two ways:
  (a) strata BANDS on the bore walls by radius as you descend (crust grey → mantle
  darker bands), (b) the cavern ceiling IS the mantle underside, lit from below by
  the core.
- **Two SHORT bores:** Errington mouth → breach; Murray mouth → breach. Each bore is
  now an independent ~5 km dive through the crust/mantle instead of today's 12.3 km
  traverse; the through-route (mouth → cavern → other mouth) still exists for the
  flythrough/sightline verifiers. Pump side-chambers re-hang off the bores near their
  breaches (game-loop hook preserved).
- **Entry read:** once a bore breaches, CORE LIGHT spills up the bore (the light at
  the end of the tunnel = the angle-of-entry cue), plus brighter mouth-pit lamps at
  the surface so the entry geometry reads before commit.

## Physics / seam audit targets (consult must verify)
- **Kernel FROZEN — zero sim/control changes.** Gravity is already `-normalize(p)·g`
  at any radius; curvature ff already divides by live `|position|` (S-ffrad); the
  instructor flies a smaller sphere automatically. Verify no hidden `R`-welded
  assumption in control/ or sim/ that breaks at |p| ≈ 3–10 km (e.g. altitude-keyed
  atmosphere, v_min/G-clamp interactions at the tighter curvature, AT harnesses).
- Level flight at r ≈ 3 km, V = 250: centripetal V²/r ≈ 20.8 m/s² > g — near the core
  "level" needs lift TOWARD the center (the instructor's ff handles the rate; does
  anything else assume n ≥ 0 cruise?). Name the felt consequence honestly.
- `contains()` becomes: (inside cavern shell: core_m < |p| < cavern_ceiling_m) OR
  (inside a bore) OR (chambers/collars). Crash-yield + full-density atmosphere ride
  the same predicate (T1 seam). Core surface + cavern ceiling = honest crash walls.
- CAVECAM (T9a) is a PRECONDITION: inside the cavern |p| < R always, the surface
  clamp must yield or the camera lives at surface radius forever.
- Far plane 60 km, near 2 m, double→float eye-rebase: verify a 21 km-diameter lit
  cavern renders sane (depth precision, horizon cull, celestial/terrain draws when
  the eye is deep inside).

## Render architecture sketch
- Cavern ceiling: inward-facing sphere mesh (or the pocket tessellator at concentric
  radii), lit by a radial "core key": brightness ∝ max(dot(N, toCore),0) with 1/d²-ish
  falloff — replaces lamp-lighting as the cavern's light; the T8 floor-key stays
  bore-only.
- Core ball: emissive mesh + additive glow billboard (street-lamp tech scaled up).
- Strata: radius-banded value ramp in the tunnel/cavern shader (mono values only).
- Mouth/entry: brighter pit lamps + (post-breach) core-light spill up the bore.

## Open questions for the consult
1. Exact radii (`core_m`, `cavern_ceiling_m`) and bore profiles (steepness vs the T8
   sightline standard — short+steep+lit-from-below may beat long+shallow).
2. Does 100×-volume-as-floor vs literal 100× need a Chad checkpoint before build, or
   does the card's honest dial note suffice? (Autonomy rule: proceed + name it.)
3. Keep the old egg/chambers as dead config or delete outright?
4. Verifier redesign: flythrough both ways + sightline through a breach into open
   cavern; what replaces the corridor-station sightline inside the cavern?
5. Strata/core in MONO (stereoscope law) — value bands only, or is a restrained warm
   tint on the core a Chad question?
