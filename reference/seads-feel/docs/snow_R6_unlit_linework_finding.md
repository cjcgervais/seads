# R6 SCOPE NOTE -- THE LINEWORK SURFACES ARE UNLIT. That is the real bug
# behind R5 queue item 4.

> ## RULED 2026-08-30: **OPTION C. NOT TAKEN.**
> Chad ruled C off this note, without the night drive. The finding STANDS and is
> recorded; the blink STAYS; nothing is built. **This is a ruling, not a defect
> left lying around** -- see section 3.5 for what C actually costs and what would
> reopen it. Do NOT re-derive this and pitch A or B again without new evidence.

> Written 2026-08-29 from a CODE READ, not from the seat. Section 4 is the
> one-minute drive check that confirms or kills it. Nothing here is built.
> Supersedes nothing; it RE-SCOPES item 4 of
> `docs/snow_session_handoff_20260829_R5_PUSHED.md` section 4.

---

## 1. WHAT THE R5 HANDOFF SAID, AND WHY IT UNDERSTATED IT

> "The sled's shadow BLINKS OFF crossing groomed trails and bank strips --
> `bank_mesh` and `ribbons` have their own shaders. Mechanical, but it is SCOPE."

"They have their own shaders" reads as a MISSING UNIFORM: wire `uShadowA/B/Count`
into two more programs and the shadow crosses. It is not that.

**Those two programs have NO LIGHTING TERM AT ALL.** There is nothing on them for
a shadow to attenuate.

| program | file | sun input | night input | what it outputs |
|---|---|---|---|---|
| ribbons (roads + groomed trails) | `render/ribbons.cpp:44` `kRibbonFS` | **none** | **none** | flat albedo |
| bank strips (the oreo) | `render/ribbons.cpp:399` `kBankFS` | **none** | **none** | flat albedo |
| planet / snow / lake mirror | `render/planet.cpp` | `ndl` | `uNightFill` | lit, shadow-receiving |
| buildings (S4) | `render/buildings.cpp:107` | `uSunDir` | `uAmbient` | lit |

`draw_ribbon_surfaces(r, eye, winter)` and `draw_bank_surfaces(b, eye)` take no
sun direction. Neither signature has anywhere to put one. Grep for `uSun`,
`ndl`, `uNightFill`, `uGroundDayGain` across `render/ribbons.cpp` returns
nothing.

**And the post pass does not cover for them.** `render/post_glsl.cpp` is a
display transform -- contrast, lift, grain, split-tone, halation, vignette,
FXAA, DoF. There is no global exposure or night term in it. Checked because if
post dimmed the frame the surfaces would darken at dusk for free; it does not.

## 2. SO THE SHADOW IS THE SYMPTOM, NOT THE DEFECT

Two consequences, and the second is the bigger one:

1. **The row-9 shadow cannot cross a road, a groomed trail, or a snowbank.**
   Correctly, given the architecture: the shadow is receiver-side and multiplies
   a DIRECT-SUN term. These receivers have no direct-sun term.
2. **Roads, trails and banks hold FULL DAYLIGHT ALBEDO AT MIDNIGHT.** Nothing
   about them responds to the sun, so the 300 s day sweeps past and they do not
   change. Every other surface in frame does.

Number 2 has been true since S3 and SF2 and nobody has named it. It is an AGE
artifact, not a policy: buildings arrived later (S4) and were lit from the
start. The linework rungs predate the lighting model they now sit inside.

## 3. THE FORK -- THIS IS CHAD'S, AND IT IS NOT A CLOSE CALL EITHER WAY

**Option A -- LOCAL. Shadow-only, on the flat color.**
Pass `fragRel` and `toSun` into both programs, lift `shadow_sun_occ` out of
`planet.cpp` into a shared GLSL snippet, and multiply the flat albedo by it,
all inside `if (uShadowCount > 0)` so the zero-caster pixel stays byte-identical
(row 9's fence 4 discipline, unchanged).
- The shadow crosses. Item 4 closes.
- **It buys an inconsistency:** the trail darkens under the sled but never
  darkens at dusk. A surface that responds to an occluder and not to the light
  that casts the occlusion is a stranger read than the blink, and it is the kind
  of thing that gets noticed once and cannot be unseen.
- Small: two shaders, two uniform blocks, one shared snippet, one A/B env fence.

**Option B -- ROOT. Give both surfaces the planet's lighting term.**
`uNightFill + uGroundDayGain * ndl`, single-sourced from the planet pass, then
the shadow falls out for free because the term it multiplies now exists.
- Fixes both consequences with one change, and puts the linework on the same
  lighting model as everything else in frame.
- **It is a LOOK CHANGE ON EVERY FRAME, day and night.** The trail colors
  (`trail_winter_color`, `road_bed`, the oreo `uBase`) were all tuned BY EYE
  against an unlit surface. Lighting them will move every one of those, and at
  least `trail_winter_color` is a Chad-driven value. This is a dial sweep from
  the seat, not a code change.
- The recurring trap applies: the answer will probably be a LOWER gain than
  looks right in a screenshot. Three for three so far.

**Option C -- NOT NOW.** Record the finding, leave the blink. Defensible: the
sled is on the ground, the shadow is a LANDING cue for the aircraft, and no
approach path crosses a groomed trail at flare height. The blink costs nothing
in the mode the shadow was built for.

**No recommendation is offered on A vs B without section 4 first.** A is cheap
and buys a defect. B is right and costs a sweep. C is free and honest. Which one
is worth it depends on how loud the night read actually is, and nobody has
looked.

## 3.5 WHAT C COSTS, AND WHAT WOULD REOPEN IT

Ruled 2026-08-30. C is CHEAPEST NOW and it is not free later. Both of these are
now KNOWN and ACCEPTED, not unknown:

1. **The machine shadow blinks off crossing any road, groomed trail or
   snowbank.** Accepted because the shadow is a LANDING cue for the aircraft and
   no approach path crosses a groomed trail at flare height. The sled is on the
   ground when it meets one.
2. **Roads, trails and banks hold full daylight albedo at midnight.** This is
   the one that is easy to forget was ruled rather than missed. If a night rung
   ever gets driven -- night flying, dusk approaches, the night sparkle line
   (R5 queue 5) -- this is already on the board and does not need rediscovering.

**What reopens it, and only these:**
- A night or dusk drive where the linework glow actually reads badly from the
  seat. Section 4 is that drive, still valid, still one minute, just not owed.
- Any rung that gives ribbons or banks a lighting term for its OWN reasons --
  at which point the shadow is free and A/B collapses into that rung's scope.
- Sudburian NPCs or any second ground caster whose shadow is meant to be READ on
  a trail rather than merely present.

**What does NOT reopen it:** a screenshot, a code read, or a later agent
rediscovering section 1 and mistaking a ruling for an oversight. The lighting
gap is now DOCUMENTED, which is exactly what C bought.

## 4. THE DRIVE CHECK -- ONE MINUTE, AND IT DECIDES THE FORK

This is a MEASUREMENT and it comes before the design. Per section 5 of the R5
handoff: price the fear before designing around it.

```
cd D:\seads_sandboxes\sandbox_snow
taskkill //F //IM seads.exe
    # then VERIFY zero instances remain -- a stale instance has outlived a
    # taskkill twice on this branch and both times the sweep was worthless
.\build-play\seads.exe
```
- **J** to mount the sled, drive onto a GROOMED TRAIL or alongside a snowbank.
- **WAIT for the sun to go down.** The day is 300 s, so this is at most a
  couple of minutes on the clock. Usable light is ~126 s of it.
- **The question: at full dark, do the trail and the bank still GLOW?**
  Everything around them will have fallen to `uNightFill`. If the linework
  stays bright, it will read as lit strips on a dark field.
- Second look, in daylight: drive the sled ACROSS a groomed trail and watch the
  machine shadow. It should vanish at the trail edge and reappear on the far
  side.

**If the night glow is loud, that is B and the shadow comes along for free.**
**If it reads as fine or even as a plausible "packed snow catches moonlight",
that is C, and A is not worth its inconsistency.**

⚠ Do not pre-tune anything against a screenshot. The lesson that keeps getting
paid for on this rung-family is that a value checked in one mode does not
survive contact with the mode that consumes it.

## 5. WHAT IS NOT IN SCOPE HERE

- The rider shadow taper (R5 queue 2) -- still Chad's, still costs caster slot
  9 -> 10 of 16, still needs R4a's `measure_body_chain.py` RE-RUN, never retyped.
- The caster budget (R5 queue 3) -- still 9 of 16, still his.
- Night sparkle compaction suppression (R5 queue 5) -- one line, still flagged
  not taken. It is ADJACENT to option B (both are night reads) and if B is
  chosen it is worth doing in the same sweep, so he judges one night picture
  instead of two.
- The bank-strip probe from R4b (R5 queue 7) -- "the ski goes down into the
  road". Still unbuilt, still a MEASUREMENT. It touches the SAME two surfaces as
  this note, so if the fork lands on B, build that probe first and get both
  answers from one drive.

## 6. STATE

**Ruled C on 2026-08-30 -- recorded, not built.** Nothing built beyond this file. `sandbox/snow` and `main` were
level at `db319e328` when this was written; gate 1669/1674 with R4a's five known
baseline reds.
