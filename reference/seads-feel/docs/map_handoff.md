# S-mapread / S-maparrow / S-mapteam — the M-key tactical chart

**Branch:** `sandbox/bubble-atmosphere`. **Status:** ROUND 3 (S-pumpglyph / S-pumpcube) BUILT +
GATED **1024/1024, zero goldens moved** → **AWAITING CHAD'S FLY.** (1019 was the round-2 baseline;
the +5 are the new `test/unit/test_pump_frame.cpp` cases. No golden was re-recorded.)

**Round-1 verdict (Chad, 2026-08-09): "IT LOOKS GOOD."** The declutter, the greyscale plate, the
arrow and the shape language all survived; round 2 is the colour system + two look items.

**Evidence:**
- round 1 — `docs/map_shots/map_before.png` vs `map_after.png` (same cell, both viewed);
  `map_after_zoom.png` is the player-arrow close-up.
- round 2 — `map_after.png` (the round-1 chart) is the BEFORE; `map_r2_chart.png` is the AFTER,
  with `map_r2_pump_zoom.png` (green pump inside an ENEMY orange ring),
  `map_r2_pump_ally_zoom.png` (green pump inside an ALLY blue ring),
  `map_r2_player_zoom.png` (the blue player arrow + two deep pumps) and
  `map_r2_ingame_tag.png` (Chad's callsign tag, in game, in ally blue). All viewed, not assumed.

Chad's ask, verbatim (2026-08-09):

> "correct the arrow on the map so the point of it faces my actual direction? It gets spun around
> sometimes. Also The map is too cluttered. Need better contrast for objective markers and player
> markers, proper color needed for the markers. Let make allies green and change my color to green
> as well. Make the enemies red (and their markers red) Make sure that there are not conflicting or
> camouflaging colors on the map. Use arial font, newspaper greyscale for the look but objectives
> and playermarkers properly color coded."

---

## 1. THE ARROW — attribution, then fix

The arrow was driven by the **GROUND TRACK** — the tangential component of **velocity**. That source
has two independent failure modes, and both read on screen as "spun around":

1. **SIGN FLIP (the 180°).** Velocity is not facing. In a stall / tail-slide / hammerhead the plant's
   own α runs to ~180° (SPEC §9's BALLISTIC note, *"tail-slide lies"*): the aircraft FACES one way and
   MOVES the other, so a velocity-driven arrow snaps a full half-turn while the nose never moved.
2. **DEGENERACY (the free spin).** The tangential component vanishes on any near-vertical trajectory
   — steep climb/dive, the apex of a loop, a low-speed park. The old guard only bailed below
   **1e-6 m/s**, so in (say) a 200 m/s vertical dive the surviving ~1 m/s of tangential drift — pure
   lateral noise — set the entire bearing and the arrow spun freely.

**There was NO constant offset.** The screen-space rotation sign and the map's north/east basis were
already correct, and that is now pinned in the code comment *and* re-verified visually (see §5).

**The fix** (`render/bubble_map.h`, `render::map_facing_heading_rad`):
- drive the bearing from the **NOSE** (`state.orientation * (0,0,-1)`) — what "my actual direction"
  means to a pilot, and something that can never disagree with where he is pointed;
- **HOLD the last valid bearing** (the `sim/aero.h` v̂-guard idiom) while the nose is within
  `asin(arrow_hold_sin)` of straight up/down. Default `0.15` ⇒ ~8.6°. `0.0` is the kill-switch arm:
  bit-for-bit the legacy recompute-always behaviour.

Pinned by three legs in `test/unit/test_bubble_map.cpp`, including a tail-slide leg that computes the
OLD ground-track bearing as an oracle and asserts it is exactly π away from what the arrow now reads
(so the legs are not vacuous), and a 64-sample rotating-noise leg through the vertical that requires
the held bearing to come back **bit-identical**.

## 2. Declutter — what went, what stayed

| Removed / thinned | Why | Dial to bring it back |
|---|---|---|
| 176 dashed snowmobile **trails** | the single biggest source of noise on the before-shot | `[map] trails_visible = 1` |
| **River** casings (3.4 px cased → 1.2 px hairline) | the blue web dominated the whole sheet | `rivers_visible = 0` to drop entirely |
| **Minor roads** (3 px cased → 1.1 px hairline) | town grid now reads as texture, not as content | `minor_roads_visible = 0` |
| **Lake labels** below 8 km span (was 3 km) | the label pile-up he called clutter | `lake_label_span_m` |
| **Pump-zone place text** ("Onaping / Dowling", "Coniston") | redundant with the objective markers drawn on top of them | `zone_labels_visible = 1` |
| **Overlapping labels** | anti-overlap: a label is dropped within 54 px of an already-placed one, in priority order (towns → airstrips → lakes) | code (`kLabelMinSepPx`) |
| **OSM-id junk labels** ("highway w246254652") | a data defect, not a place name — anything with 5+ consecutive digits is dropped | code (`label_is_junk`) |

**Deliberately KEPT:** every lake polygon, every road, the rivers, the airstrips, the named towns,
the tunnel route hint, the compass, the scale bar. Nothing was deleted from the data — the pass makes
the tactical layer dominant, and `basemap_ink` (0.92) screens the whole plate back in one knob.

## 3. Colour scheme

The **basemap carries no chroma at all** — the loader *rejects* a tinted `paper`/`ink`/`water`, so
"newspaper greyscale" is an executable ruling, not a comment. Every coloured pixel on the map is
therefore tactical, and nothing on the plate can camouflage a marker.

| Class | Colour | Shape |
|---|---|---|
| **Chad (player)** | green `[0.22,0.95,0.40]` | **arrow**, biggest marker on the map, the only one wearing a ring |
| **Allies** | green `[0.16,0.78,0.34]` | **circle** |
| **Enemies** | red `[0.86,0.16,0.14]` | **square** |
| **Objectives (pumps)** | amber `[0.99,0.72,0.10]` | **diamond**; owner shown by a green/red RING around it, never by the fill |
| Unfactioned contact | grey | small circle |
| Tunnel mouths / route | near-black | line + ringed dot (infrastructure, deliberately neither team) |

Team colour is **player-relative**: Chad's side is green whichever faction he flies. The territory
ellipses follow the same mapping and are **outline-only** by default (`territory_fill = 0.0`) — a
green wash under green markers is exactly the camouflage he called out; the dial is kept if the
outline alone reads too thin.

**Colour-blind safety.** Green/red is the most common confusion pair, so hue is never the only cue:
each class owns its own SHAPE, and the three tactical hues are separated in **lightness** as well
(ally luma ~0.55, enemy ~0.30, objective ~0.75), so they stay distinct under deuteranopia. Every
marker also sits on a near-black halo (`outline_px`), which is what gives the contrast against the
light plate.

## 4. Font — and its trade

Arial, loaded at runtime from `C:/Windows/Fonts/arial.ttf` (`render/map_font.h`), with a fallback
chain → `arialbd.ttf` → raylib's built-in default. This is the *exact* pattern already shipped for
the nose-art tag font in `draw.cpp`.

**⚠ TRADE FOR CHAD TO RULE:** Arial ships with Windows but is **not redistributable**, so it is not
vendored into `assets/`. On this box it is correct; on any machine without it the map silently falls
back to the old face. The clean fix if SEADS is ever distributed is **Liberation Sans** (metric-
compatible with Arial, freely redistributable) dropped into `assets/` — a one-line change in
`render/tourist_map.cpp`'s `map_font()`.

## 5. What was verified, and what was NOT

**Verified visually** (screenshots read, not assumed):
- before/after of the same cell — clutter, greyscale, marker colours, shapes, contrast;
- **arrow direction**: two shots 810 frames apart showed the player marker travelling east-and-
  slightly-south on the map, and the arrow in both points east-and-slightly-south. Sign and frame
  confirmed against real motion, not just against the code comment.

**NOT verified visually — honest gaps:**
- the **tail-slide 180° flip** and the **near-vertical hold** cannot be produced in a hands-off
  `--smoke` run (the plane flies level). Both are covered by unit legs only. **This is the main thing
  for Chad to fly.**
- the **under-attack pump alert blink** (amber pulse ring) — needs a live raid.
- the map at a **non-1920×1080 window size** and with a **live enemy/ally mix in the same area**.

---

# ROUND 2 - S-mapteam (Chad's second ask, 2026-08-09)

> "rEMOVE ALL THE PLACE NAMES FROM THE MAP PLEASE, IT LOOKS GOOD. Please Make the pump symbols look
> like pump symbols and make the pump symbols green the same green used for oxyggen bottles in an
> ambulance ... Make the enemies the slag orange color we use across this codebade and make the
> allies switch back to an opposite blue functionally and with precision the opposite of blue on the
> color wheel of the orange slag color and verify this is right. Then go an make sure all allies
> planes and their name tag are colored blue in map and in game. And the real planes the same slag
> orange code for the in game planes, map icons and their enemy name tags"

("the real planes" = the RIVAL/enemy planes.)

## R2.1 - ONE palette, no second table

`config/world.toml` **`[teams]`** is now the single faction palette. `render::set_team_colors()`
pushes it once at startup (`app/main.cpp`) and **every** faction-coloured pixel reads
`render::team_colors()` (`render/team_color.h`):

| Surface | Site |
|---|---|
| map contact markers (ally circle / enemy square) | `render/draw.cpp` map tactical layer |
| map airspace tags ALLIED AIR / ENEMY AIR | same, `draw_region` |
| map OURS / THEIRS score stack | same |
| map player arrow + its ring | same (Chad = ally blue) |
| map pump OWNER rings | same |
| **in-game aircraft livery** | `FrameInfo::rig_player_color` / `rig_bandit_color`, fed from `team_colors()` |
| **in-game ALLY / BANDIT tags** | `render/draw.cpp` drone label pass |
| **in-game Chad callsign tag** (MaNdALaRK) | `render/draw.cpp` floating player tag (was gold) |
| **3D pump bodies + beacons** | `render/draw.cpp` conquest pump markers |
| **the molten slag pour** | `render/slag.cpp` - `teams.enemy` IS the `heatColor()` hot stop |

Two tables were DELETED so they cannot fork: `[map] ally_color/enemy_color/objective_color/
player_color` and `[fleet_rig] player_color/bandit_color`.

## R2.2 - the colours, and how each was derived

| Class | Value | Where it comes from |
|---|---|---|
| **Enemy** | `[1.00, 0.55, 0.12]` | **the slag orange** - the `hot` stop of `heatColor()`. It is no longer typed into the lava GLSL: `render/slag.cpp` injects `#define SLAG_HOT vec3(...)` from `team_colors().enemy` when it builds the shader, so the pour and the faction are the same number by construction. |
| **Ally** | `[0.12, 0.57, 1.00]` | the **exact 180 deg HSV complement** of the enemy at the same S and V. Derived, not picked (below). |
| **Objective (pumps)** | `[0.000, 0.518, 0.239]` | **medical-oxygen green.** North American medical gas containers and their pipe/label markers follow **CGA Pamphlet C-9** (adopted by NFPA 99C), which assigns **green to oxygen** - the green on an ambulance O2 cylinder. C-9 names the colour but publishes no Pantone; the de-facto ink used by medical-gas marker printers is the ANSI Z535.1 safety green, **PANTONE 348 C = #00843D = (0,132,61)/255**. |

**The complement derivation, and the trap.**

```
enemy (1.00, 0.55, 0.12)  ->  HSV (29.3181818 deg, S 0.88, V 1.00)
        + 180 deg         ->  HSV (209.3181818 deg, S 0.88, V 1.00)
C = V*S = 0.88 ; X = C*(1 - |209.318/60 mod 2 - 1|) = 0.88*0.5113636... = 0.45 ; m = V - C = 0.12
sector 3 ([180,240)) => (0, X, C) + m  ->  RGB (0.12, 0.57, 1.00)   EXACTLY
```

WARNING: **Do NOT "simplify" the ally to `(0.12, 0.55, 1.00)`.** That channel-reversal looks
identical on screen and is **210.68 deg**, i.e. **181.36 deg away, not 180**.

**"Verify this is right" is EXECUTABLE, in two places:**
- `test/unit/test_team_color.cpp` - converts both to HSV and requires hue separation exactly 180 deg
  (1e-9), equal S, equal V; reconstructs the complement forward from the orange (so the leg is not
  merely self-consistent); pins the trap value's 178.64 deg shortest-arc reading; guards the HSV
  converter itself against six reference hues and the 360 deg wrap seam; and pins the three-way luma
  ordering. **Mutation-verified**: ally := the channel-reversal => the loader throws; ally value
  nudged V 1.00->0.98 => the test fails.
- `config/load_world.cpp` - runs the SAME predicate (`render::is_exact_complement`, 1e-6) over the
  loaded `[teams]`, so a hand-edited config fails **loud at startup**. Two rejection legs in
  `test_load_world.cpp` (the channel-reversal and a same-hue desaturated blue), plus a
  `CHECK_NOTHROW` on the shipped table so the leg is not vacuously throwing.

**Colour-blind note.** Orange/blue replaces green/red - the safest common pairing. The shape
language is kept anyway, and the three tactical hues are still separated in **lightness**:
objective green luma ~0.39 < ally blue ~0.51 < enemy orange ~0.61 (pinned by a test leg).

## R2.3 - the pump glyph

The amber diamond is gone. Pumps now draw the standard **P&ID / ISA-5.1 centrifugal-pump symbol**:
a **circular casing**, a **discharge wedge** notched out of it in the halo ink, and a **base
plinth** wider than the casing. Three cues, none of which the ally circle or the enemy square has.
`objective_px` 7.5 -> **9.0** so the wedge and the foot survive at chart scale.

**Ownership was re-solved.** The fill is now a fixed neutral green, so the OWNER RING is the only
ownership cue: it is thicker than before and sits on its own dark casing so a blue ring survives
over a grey lake and an orange ring survives over dark ink. Both directions were viewed
(`map_r2_pump_ally_zoom.png`, `map_r2_pump_zoom.png`) - the green reads clearly inside each.

The same split runs in the world: the **3D pump body is the oxygen green**, and the tall beacon
column + lamp carry the **owner's team colour** (the old VALLEY-cool / SUDBURY-warm tint was
faction-ABSOLUTE, so an allied pump read "warm" for one side and "cool" for the other).

WARNING - **a raylib trap worth remembering:** `DrawTriangle` culls anything not wound
counter-clockwise in screen space. The wedge shipped **invisible** twice - once from the wrong
winding, once because the "fix" ROTATED the vertex list instead of REVERSING it (a rotation leaves
winding unchanged). Only reading the screenshot caught it; the gate is structurally blind here (no
ctest runs `seads.exe`).

## R2.4 - the place-name purge

`[map] place_labels_visible = 0` removes **every place NAME** on the chart in one dial: the named
towns (CHELMSFORD / COPPER CLIFF / ERRINGTON / MURRAY), the pump-zone text, the **airstrip names**
and the **lake names**. Nothing is deleted from the data - set it to `1` and the whole layer returns
exactly as before (the anti-overlap, junk-filter and `lake_label_span_m` logic is untouched
underneath).

**Deliberately KEPT** (chart FURNITURE, not place names): the airstrip **dots**, the compass rose,
the scale bar, the GREATER SUDBURY / tactical chart title block, and the map's own TACTICAL tags
ALLIED AIR / ENEMY AIR (which are team-coloured).

## R2.5 - what was verified, and what was NOT

**Verified visually** (screenshots read, not assumed): every place name gone; the pump glyph at real
chart scale; green-in-blue-ring and green-in-orange-ring both legible; ally blue circles, enemy
orange squares, the blue player arrow with its ring, blue/orange airspace tags and OURS/THEIRS;
and **Chad's own callsign tag in ally blue in game**, on his blue-rimmed plane.

**NOT verified visually - honest gaps:**
- **an allied AI plane and an enemy AI plane, with their ALLY / BANDIT tags, in one in-game
  frame.** A hands-off `--smoke` run flies level and the conquest squadrons spawn ~7 km away inside
  their own bubbles; several attempts (aim-offset smoke args, and a temporary near-player respawn
  patch that was reverted) never framed one. The tag and livery colours are correct **by code
  inspection** - both read the same `team_colors()` the map reads, keyed on the same
  `drones_friendly` flag the tag already used - but **nobody has SEEN them**. This is the first
  thing for Chad to check.
- the pump **under-attack blink** and a **dead** pump glyph (needs a live raid / a kill).
- the map at a non-1920x1080 window size.

---

## FLY CARD — S-mapread / S-maparrow

Open the map with **M** while flying.

1. **THE ARROW, level flight.** Fly straight, open the map. Does the arrow point where the nose
   points? Turn 90° and re-check.
2. **THE ARROW, straight up.** Pull vertical, hold, open the map. It should **freeze** at your last
   heading, not spin. Push over the top and re-check that it picks up the new heading cleanly.
3. **THE ARROW, tail-slide.** Go vertical, let it fall back on the tail (the α≈180 regime), open the
   map. **This is the specific bug** — it must keep pointing where the nose points and must NOT flip
   180°.
4. **FIND YOURSELF.** With allied greens on screen, how fast do you find your own marker? It is the
   biggest one and the only one with a ring around it.
5. **READ THE FIGHT.** Green circles = friends, red squares = enemies, amber diamonds = pumps (the
   ring around a diamond is green if it's ours, red if theirs). Can you tell at a glance?
6. **CLUTTER.** Is the plate quiet enough now, or too quiet — did I take away something you use?

### ROUND 2 (S-mapteam) - fly this

7. **NAMES.** Open M. Every place name should be gone; the compass, the scale bar and the
   GREATER SUDBURY title block should remain. Do you MISS any of the names? (`place_labels_visible
   = 1` brings them all back.)
8. **THE PUMPS.** Do they read as PUMPS - round body, a notch, a foot - and is the green the
   ambulance-oxygen green you meant? Can you tell whose a pump is at a glance (blue ring = ours,
   orange ring = theirs)?
9. **THE TEAM COLOURS ON THE MAP.** Blue circles = friends, orange squares = enemies, blue arrow
   = you. Anything camouflaged now that the plate is grey and the pumps are green?
10. **THE ONE THING NOBODY HAS SEEN.** Get an **allied** maverick and an **enemy** maverick in
    view in game. The allied plane and its **ALLY** tag must be BLUE; the enemy plane and its
    **BANDIT** tag must be the SLAG ORANGE. Your own tag (MaNdALaRK) is now ally blue too - say
    if you want your own tag back in gold.
11. **THE SLAG.** Fly past the Copper Cliff pour. It should look exactly as it always has - the
    enemy orange IS the molten hot stop now, so if the pour changed, say so.

### Dials if you disagree (all `config/world.toml` `[map]`)

| If… | Turn |
|---|---|
| arrow still spins near vertical | `arrow_hold_sin` **up** (0.15 → 0.25 ≈ 14°) |
| arrow feels sticky / lags a real turn | `arrow_hold_sin` **down**; `0.0` = kill-switch, old behaviour |
| chart too faint / too busy | `basemap_ink` (0.92) |
| want the trails back | `trails_visible = 1` |
| want the town grid gone | `minor_roads_visible = 0` |
| want the rivers gone | `rivers_visible = 0` |
| more / fewer lake names | `lake_label_span_m` (8000) |
| territory hard to see | `territory_fill` 0.0 → 0.06-ish, or `territory_outline_px` up |
| markers too small / too big | `marker_px`, `objective_px`, `player_px` |
| halo too heavy | `outline_px` (2.2) |
| **want the place names back** | `[map] place_labels_visible = 1` |
| **pump glyph too small / too big** | `[map] objective_px` (9.0) |
| **a team colour is wrong** | `[teams] enemy` (moves the slag pour too, by design); `ally` is then FORCED to its 180 deg complement by the loader, so change both together or it will not load |
| **the pump green is wrong** | `[teams] objective` (no complement constraint on this one) |

---

# ROUND 3 - S-pumpglyph + S-pumpcube (Chad's third ask, 2026-08-09)

**Round-2 verdict (Chad): "Very good."** Two follow-ups, verbatim:

> "I need the pump symbol improved and the green pumps inside the black stope need a wire cube that
> is neon slag and opposing blue of the same team color codes to frame in that pump otherwise they
> dont apper claimed by either side in the black stope"

## R3.0 - WHICH PUMPS ARE WHERE (found, not assumed)

`combat::make_pumps` (`combat/conquest.h`) builds exactly four, and `Pump::surface` is the split:

| idx | faction | `surface` | where it actually is |
|---|---|---|---|
| 0 | VALLEY (0) | **true** | `world::kPumpValleySurface`, on the ground, 450 m team beacon column |
| 1 | SUDBURY (1) | **true** | `world::kPumpSudburySurface`, same |
| 2 | VALLEY (0) | **false** | `combat::place_deep_pump` in the tunnel-net ARENA - **the black stope** |
| 3 | SUDBURY (1) | **false** | same, the other end of the arena - **the black stope** |

So "the pumps inside the black stope" = **indices 2 and 3, one per faction**. Flying VALLEY (the
default `player_faction`), pump 2 is the ALLY-blue one and pump 3 the ENEMY-orange one - which is why
those are the two shots below.

**There is no neutral/capturable pump today**: `faction` is fixed at construction and only ever 0 or
1. The neutral arm below is therefore defensive, and deliberately so.

## R3.1 - THE MAP GLYPH, judged at TRUE CHART SCALE

The round-2 glyph was convincing at 480 px and a **green blob** at 1:1, and the reason is a
legibility rule rather than a drawing bug: at r ~ 9 px an INTERNAL detail cut in near-black out of a
dark-green disc carries ~0.35 luma over a couple of pixels and averages away, while the
**silhouette** survives any downsample. Three changes, all silhouette-first:

| Round 2 | Round 3 | Why |
|---|---|---|
| discharge wedge NOTCHED into the casing, in halo ink | **volute cone standing PROUD of the casing**, in the pump green with a constant-width dark outline | changes the outline, so it survives at icon size. Also the truer ISA-5.1 centrifugal symbol (circle + discharge trapezoid off the top) |
| plinth in the SAME green as the disc | **black baseplate** (halo ink) | separated from the casing by ~0.35 luma instead of by an edge; the green-on-green foot was half the blob |
| no interior cue | **paper-light shaft dot** at the impeller centre | the one interior cue, and it is LIGHT-on-dark |
| every ring sized off `r` | **rings derived from the glyph's real extent** (`gext = 1.8 r`) | **this was the actual bug in the first round-3 cut**: an `r`-derived owner ring painted straight over the new cone, so the whole silhouette improvement was invisible on the chart and showed only in the zoom. Now `objective_px` scales the entire assembly, ring included, with nothing to re-tune by hand |

`[map] objective_px` 9.0 -> **7.5** (it is now the CASING radius; the drawn glyph is ~1.8x it).

The halo pass takes a **pixel `pad`** instead of an inflated `r`: a uniform scale grew the cone's
height by `pad*1.72` and capped the green nozzle with a disproportionate black slab. Padding each
offset keeps the outline a constant-width stroke all the way round.

**Judged on `docs/map_shots/map_r3_pump_scale.png`** - a 1:1 crop of the real chart with three pumps,
an enemy square and the road web in frame. It reads as a green machine with a funnel and a black
foot; it cannot be confused with the ally circle or the enemy square. `map_r3_pump_zoom.png` is the
construction check only.

## R3.2 - THE NEON TEAM WIRE CUBE (the real ask)

`render/pump_frame.h` (POD + pure helpers, header-only, same shape as `team_color.h`) and a
`[pump_frame]` block in `config/world.toml`, pushed once by `render::set_pump_frame_style()`.
**No new colour literal exists anywhere** - the frame reads `render::team_colors()`.

**Geometry.** 12 edges of a cube centred on the pump, `half_extent_m` 40 m (the deep-pump body is a
26 m shell, so the frame clears it by 14 m and reads as a frame, not a skin). The basis is built per
frame from `normalize(pu.pos)` and the underground RUN (the two mouth directions), projected off
local-up, with a spanned fallback - **never a cached or fixed world axis**, so it obeys the sphere
rule and aligns with the chamber the pilot actually flies through.

**Emissive approach.** `BLEND_ADDITIVE`, depth-TEST **on** (stope rock still occludes it), depth-
WRITE **off**, drawn in the additive pass AFTER every opaque depth-writer (the street-lamp P1-1
ordering rule). It rides raylib's default UNLIT path - there is no sun and no lamp on a deep pump, so
a shaded wireframe is invisible down there, which *is* the defect Chad reported. Additive over black
can only brighten.

**Thickness is `layers` nested shells `layer_step_m` apart, NOT a line width.** GL core profile
clamps `glLineWidth` to 1, so a width-based "neon" would silently ship as a hairline on exactly this
hardware. The shells bloom additively up close and collapse to one bright line at range. Shipped
3 shells at 0.4 m (0.8 m read as three distinct parallel wires at 140 m).

**Z-fighting.** Impossible by construction and the loader enforces the premise: `half_extent_m` AND
the innermost shell `half_extent_m - (layers-1)*layer_step_m` must both exceed
`render::kDeepPumpShellM`. That constant is **single-sourced** - `draw.cpp` now draws the beacon
sphere from it too, so the bound can never drift from the geometry it guards. Depth-write off means
the shells cannot fight each other or the body.

**Scope + the neutral ruling (both decided here, flagged for Chad):**
- `deep_only = 1` (default) honours the literal ask - **stope pumps only; the surface pump look is
  bit-for-bit untouched**. **`deep_only = 0` frames EVERY pump**, surface ones included. That is the
  dial if he wants it everywhere.
- **A DEAD pump, and any out-of-range faction, draws the frame in `neutral_color` (dim grey), never
  in a team hue.** A frame is an ownership CLAIM; a destroyed pump has no owner, and inheriting the
  last owner's colour would be the stale-ownership lie this mechanism exists to remove. Say the word
  and a dead pump can draw no cube at all instead.

**Perf (judged STRUCTURALLY - a DEM bake was running on the box, so no timing line taken during this
session is trustworthy and none was used).** 12 edges x 3 shells x 2 deep pumps = **72 `DrawLine3D`
calls per frame**, every one of them inside a single `BeginBlendMode` block with no texture or shader
change, so rlgl folds them into **one batched line flush**. **No shader is built here, ever** - no
`LoadShader`, no per-pump material, nothing to recompile per frame. That is the T-thread lamp-storm
lesson applied up front, not after a fly.

## R3.3 - THE STOPE CAMERA (new smoke hook)

`SEADS_TUNCAM_PUMP="index[,dist_m][,el_deg]"` (`app/main.cpp`, smoke-only, planet frame), the
sibling of `SEADS_TUNCAM_CAVERN`. It puts the eye at `dist_m` from a named conquest pump looking
along the underground run. It exists because the gate is structurally blind to everything drawn and
"is the ownership frame legible in the dark" can ONLY be answered from inside the chamber. It prints
the pump's index, SURFACE/DEEP and faction so a shot can never be mislabelled.

```
SEADS_TUNCAM_PUMP="2,140,12" ./build-play/seads.exe --smoke 60 shot.png   # ally, in the stope
SEADS_TUNCAM_PUMP="3,140,12" ./build-play/seads.exe --smoke 60 shot.png   # enemy, in the stope
```

## R3.4 - what was verified, and what was NOT

**Verified visually - every one of these was read, not assumed:**
- `docs/map_shots/pump_cube_ally_stope.png` - **pump 2 (ALLY) at 140 m inside the black stope**: the
  blue cube frames the green pump, unmistakably claimed, in a chamber with no sun and no lamp on it.
- `docs/map_shots/pump_cube_enemy_stope.png` - **pump 3 (ENEMY) at 140 m**, the same in slag orange.
  Side by side there is no ambiguity about which side owns which.
- `docs/map_shots/pump_cube_enemy_far.png` - the same enemy cube at **700 m across the chamber**:
  still clearly a coloured frame around a green pump, so it reads on approach, not just up close.
- `docs/map_shots/map_r3_pump_scale.png` - the improved glyph at **true 1:1 chart scale**, and
  `map_r3_after.png` (whole chart) vs `map_r3_before.png` (the round-2 chart).

**NOT verified visually - honest gaps:**
- **A view from INSIDE the cube.** Attempted at 34 m and 22 m and both are useless: the pump body is
  a 26 m sphere, so at any eye position inside the 40 m frame you are within ~8 m of its surface and
  green fills the whole FOV before a cube edge enters it. The geometry says the edges are behind you;
  no screenshot proves it. **This is the one thing only Chad's fly can settle** - fly through a stope
  pump and say whether the frame reads on the way in and out.
- **A DEAD pump's grey frame** and a **neutral-faction** frame: no live kill was staged; both arms
  are pinned by unit legs only.
- **The under-attack alert ring at its new radius** (needs a live raid).
- **Any performance number.** A DEM bake was running throughout; the `[SMOKE_TIMING]` line is
  contended and was deliberately not used. The perf claim above is structural (draw-call count +
  shader lifetime), not measured.

## R3.5 - FLY CARD additions

12. **THE STOPE PUMPS.** Fly into the arena. Can you tell **at a glance** whose each deep pump is?
    Blue frame = ours, orange = theirs. Check it on the approach (from across the chamber) AND close
    in, and say whether it still reads while you are **inside** the frame.
13. **SIZE.** Does the cube FRAME the pump or crowd it? `[pump_frame] half_extent_m` (40 m) is the
    dial; the pump body is 26 m.
14. **BRIGHTNESS.** Too neon / not neon enough? `brightness` (1.0), then `layers` (3) and
    `layer_step_m` (0.4) for how thick the tube reads.
15. **EVERYWHERE?** Want the surface pumps framed too? `[pump_frame] deep_only = 0`.
16. **THE GLYPH.** On the map (M): does the pump symbol read as a PUMP now at normal chart scale -
    round body, funnel on top, black foot - without zooming? `[map] objective_px` (7.5) scales the
    whole assembly, owner ring included.

### Round-3 dials

| If... | Turn |
|---|---|
| cube too big / too small | `[pump_frame] half_extent_m` (40.0; must stay > 26) |
| cube too bright / too dim | `[pump_frame] brightness` (1.0) |
| cube wire too thin / too thick | `[pump_frame] layers` (3), `layer_step_m` (0.4) |
| **want it on the surface pumps too** | `[pump_frame] deep_only = 0` |
| want the cube gone entirely | `[pump_frame] enabled = 0` |
| dead/unowned frame colour | `[pump_frame] neutral_color` |
| pump glyph too big / too small on the chart | `[map] objective_px` (7.5) |
