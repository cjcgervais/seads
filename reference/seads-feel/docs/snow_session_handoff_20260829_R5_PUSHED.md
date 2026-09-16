# HANDOFF — R5 IMMERSION IS **ON MAIN**. Eight rows signed, gate clean, pushed.

> **LAUNCH LINE (paste into a fresh session):**
> Read this file, then `docs/snow_R5_immersion_ledger.md` — the authority for R5's scope,
> fences and rulings. `docs/snow_row9_shadow_probe_results.md` holds the numbers that
> green-lit the shadows. `docs/snow_session_handoff_20260828_NEXT.md` is still the authority
> for R4/R4b, and `_20260827.md` for MECHANISM and MEASUREMENT generally.
> **This file supersedes `_20260828_R5.md`** (written pre-push, pre-merge).
> **Nothing is waiting on code.** Everything open is either Chad's call or another lane's.

---

## 1. STATE — PUSHED

| | |
|---|---|
| **main** | **`f0b25d5fc`** — pushed to `origin`. Gained 45 commits. |
| **`sandbox/snow`** | same commit, pushed, 0 ahead / 0 behind main |
| **Gate** | **1674 tests, 1669 pass, 5 fail** — the five are R4a's KNOWN baseline reds |
| **Shader** | 20 programs, zero failures (the launch check; nothing headless compiles GLSL) |
| **Drive** | Rows 1-6, 8, 9 **ALL SIGNED BY CHAD FROM THE SEAT.** Row 7 closed as absorbed. |

### The five reds are PRE-EXISTING and are NOT ours

`62` relentless raider · `911` `sled_slides_before_it_tips_on_flat_snow` ·
`912` `sled_grip_ceiling_stays_below_the_tip_threshold` ·
`950` `sled_assist_reference_plane_is_load_weighted` · `953` `sled_debug_sink_is_write_only`

R4a's pre-merge baseline was 1613/1618 with **these same five**. The suite grew by 56 cases
across both lanes and **not one went red**. 950/953 are GI4 sled debts, confirmed red on the
enemy-ai branch for weeks — **not attributable to snow's `roll_tq[]` 8 -> 9.** If any of the
five ever moves, that is real and worth chasing.

---

## 2. THE SIGNED DIALS — DO NOT REOPEN WITHOUT HIM

Every one ruled **from the seat**, 2026-08-28/29.

| dial | ships | note |
|---|---|---|
| `SEADS_TRACK_PACK` | **0.45** | he flew 0.65 and came **DOWN** |
| `SEADS_ROOST` | 1.0 | first arm, no sweep |
| `SEADS_EXHAUST` | 1.0 | first arm |
| `SEADS_BREATH` | 1.0 | first arm |
| `SEADS_SPARKLE` | 1.0 | first arm |
| `SEADS_SHADOWS` | 1.0 | flown 2026-08-29, "they look good" |
| view presets | keys 1-4, preset 1 = the shipped framing | |
| freelook fence | 83 deg up / 31.5 deg down | the ~7 deg below vertical is LOAD-BEARING |
| `day_period_s` | **300** | he was given the number that would lengthen it and DECLINED |

Carried from R3/R4: `full_depth` 0.30, `barren_shed_keep` 1.0, `min_depress_m` 0.55.

⚠ **THE RECURRING TRAP: the right answer keeps being the LOWER number.** `full_depth` 0.30
against a red team pushing up; `cam_ahead` (not `cam_dist`) for the camera; `SEADS_TRACK_PACK`
0.45 after flying 0.65. `ribbons.h:37-47` / S1_SPEC RT-1 is the standing warning — a white
track on a white world is LESS legible. **Never brighten it to make it "stand out".**

---

## 3. THE FOUR THINGS A LATER AGENT WILL GET WRONG

**1. `texcoord.y` IS DUAL-ROLE, AND A LEAK TINTS THE WHOLE PLANET.** Ambient snow DEPTH IN
METRES on the planet pass; normalized COMPACTION on the snow-patch pass. `uTrackPackMix` and
`uSparkleCompactArm` are pinned to 0 **every frame** on the planet pass and disarmed after the
patch draw — **the LAKE MIRROR MESH SHARES THE PROGRAM.** Follow that pattern for anything new.

**2. ROWS 3 AND 8 SHARE ONE SIGNAL AND HAVE TWO KILL SWITCHES — ON PURPOSE.** Welding them
(reusing `uTrackPackMix` as sparkle's arm) would make `SEADS_TRACK_PACK=0` silently
un-suppress sparkle in the rut, corrupting row 3's own A/B.

**3. THE SHADOWS ARE RECEIVER-SIDE — NEVER PROJECT THEM.** Analytic proxies tested PER
FRAGMENT against the sun ray. **If an implementation starts needing "where does the sun ray
hit the ground", it is the wrong path.** The rider patch floats `lift_m` = 0.150 m ABOVE
`facet + draw_fold_at`, so a draped decal lands 15 cm UNDER the surface the sled stands on.
That is fence 3 and it already cost this rung-family 0.56 m once.

**4. ZERO CASTERS MUST BE THE SHIPPED PIXEL, BYTE FOR BYTE.** The shipped
`lit = albedo * (uNightFill + uGroundDayGain * ndl)` survives VERBATIM; every shadow write is
inside `if (uShadowCount > 0)`. No multiply-by-1.0 anywhere on that path. Same discipline as
row 3's bit-identical fence. Pinned by test.

---

## 4. THE OPEN QUEUE — none urgent, all either Chad's or another lane's

1. ⭐ **R4a rebases onto main.** `D:\flight_sim2\seads-recon` is on `sandbox/r4a-phase0`
   (`f4aa8f1de`) and will not see any of this until they pull. My reply to them is on main at
   `docs/PACKET_TO_R4A_from_snow_R5_final.md`.
2. ⭐ **THE RIDER SHADOW TAPER.** `rider_radius` 0.25 is a single radius, so the capsule is
   **shoulder-width from neck to hips** (R4a measured: shoulder 0.2460, waist ~0.13). A taper
   needs a SECOND capsule (**budget 9 -> 10 of 16**, eating room enemy aircraft need) or a
   cone primitive. **Chad's call.** ⚠ Do NOT retype R4a's table — re-run
   `assets/character/sudburian_src/measure_body_chain.py`, which arrives with their rebase.
   Their own first read of it was misaligned by FIVE STATIONS.
3. **CASTER BUDGET: 9 of 16.** Enemy aircraft at 3 capsules each means **exactly two fit**. A
   third needs cull-to-nearest or a raised cap. **His decision, not a silent default.**
4. ~~**The sled's shadow BLINKS OFF crossing groomed trails and bank strips.**~~
   **CLOSED 2026-08-30 -- RULED NOT TAKEN (option C).** And the scoping above was
   WRONG: it is not "they have their own shaders" (a missing uniform). `ribbons`
   and `bank_mesh` have **NO LIGHTING TERM AT ALL** -- no sun, no night, and post
   applies no global exposure -- so there is nothing on them for a shadow to
   attenuate, and they hold FULL DAYLIGHT ALBEDO AT MIDNIGHT. Chad ruled C: the
   finding is recorded, the blink stays. See
   `docs/snow_R6_unlit_linework_finding.md`, including section 3.5 -- what would
   reopen it. **Do not re-pitch A or B without new evidence from the seat.**
5. **The NIGHT sparkle is not compaction-suppressed** and gates on `winterSurf` (incl. ice).
   Night-driving may make the rut glitter like undisturbed snow. One line, flagged not taken.
6. **There are NO SUDBURIAN NPCs.** Shadow caster 4 (townspeople) is unbuildable until they
   exist. Separate from the RIDER, who does exist and does cast.
7. **The bank-strip probe from R4b is STILL UNBUILT** — his "ski goes down into the road".
   Still a MEASUREMENT, not a fix.

---

## 5. ★★★ THE LESSON OF THIS RUNG, AND IT IS NOT A PLATITUDE

**THREE CONFIDENT SIGNALS WERE MEASURING NOTHING, ALL WITHIN ONE DAY:**

1. **R4a's helmet script** printed a confident number in **bind space** — the loader is
   documented "rigid nodes only" and silently returns raw vertices for a skinned node, and
   every helmet node is skinned. Wrong by 99 mm. Third occurrence on that same asset.
2. **My `helmet_top = 1.25`** carried a comment reading *"the MEASURED seated-helmet height"*.
   It was **composed** from a cowl top plus an allowance. The real crown is 1.574660 — the
   shadow's head was 0.32 m short. Meanwhile `rider_radius = 0.25`, **honestly labelled an
   approximation**, is the number R4a went and measured — *because the label invited it.*
3. **My row-9 tests had never run under the gate.** An EM-DASH in the `TEST_CASE` names;
   `catch_discover_tests` round-trips the name through ctest, the Windows codepage mangles it,
   the filter matches nothing, and the case fails as no-tests-ran — **a CLAUDE.md lesson from
   S2, verbatim.** They passed all session because they were only ever invoked with a direct
   filter, which matches the literal string.

**None of these was carelessness. Each was checked in the one mode that could not see its own
gap.** Each was found by CHANGING MODES — re-running a generator instead of reading its
output, diffing two frames instead of trusting one, running the full gate instead of a filter.

Three standing rules fall out of it:

- ⚠ **A composed number wearing a MEASURED label is worse than an honest approximation.** The
  approximation gets audited; the false label closes the question and nobody looks again —
  including its author.
- ⚠ **A green FILTERED run is not a green gate.** Any new `TEST_CASE` name must be ASCII, and
  a new test is not verified until it has run **through ctest at least once**.
- ⚠ **VERIFY ONE RUNNING `seads.exe` BEFORE HANDING HIM A DIAL ARM.** Twice a stale instance
  outlived a `taskkill` and he swept across two windows. A sweep whose arm the driver cannot
  name is the same failure as an A/B whose arms are indistinguishable.

Plus the one that shapes how to SPEC the next rung:

★ **Rows that INHERIT a number which already means something land on the first arm** (the
roost off the kernel's `roost_flux`; sparkle off the shipped night sparkle). **Rows that
INVENT one need his seat** (the tint strength, which he moved DOWN; the sled proxy shape,
which he rejected on sight). **Before building, go look for whether the quantity already
exists** — twice in one session it did and nobody had noticed.

And, paid for in the row-9 probe: ★ **PRICE THE FEAR BEFORE DESIGNING AROUND IT.** A consult
and I both treated the 5-minute day as possibly fatal. A 3 deg final from 50 m at 48.9 m/s is
**19.5 s** against a **125.8 s** window — six finals per window. "Intermittent" was a scary
adjective standing in for a number nobody had computed.

---

## 6. TO DRIVE IT

```
cd D:\seads_sandboxes\sandbox_snow
taskkill //F //IM seads.exe      # then VERIFY zero instances, per section 5
.\build-play\seads.exe
```
⚠ `build-play` (RelWithDebInfo), **never `build/`** — CLAUDE.md's ruling, `-O0` is 45x on the
rider skinning alone. `build/` is the ctest gate tree and stays that way.

**Gate:** `cmake --build build --config Debug -j 8` then
`ctest --test-dir build -C Debug` — ~28 minutes, expect **1669/1674**.

**In the air:** your own shadow is the landing cue. **CROSS-SUN** is the widest-margin
approach; sun astern gives weak separation; **sun dead ahead below 15 deg is the geometric
failure** (shadow enters at 2.1-5.4 m, too late to flare). If the sun is down, WAIT — ~126 s
of usable light per 300 s day.

**On the sled:** **J** to mount, **1-4** for the camera (2 frames the snow), **SPACE+mouse**
to look up to 83 deg. The track reads by contrast against sparkling virgin snow; the roost is
speed-keyed and throws nothing at rest; the machine casts a measured, articulating shadow whose
skis yaw with the bars.
