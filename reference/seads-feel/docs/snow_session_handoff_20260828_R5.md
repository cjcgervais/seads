# SUPERSEDED -- read `docs/snow_session_handoff_20260829_R5_PUSHED.md`

This file was written PRE-MERGE and PRE-PUSH. Everything in it about being unpushed,
about row 9 being unflown, and about the rider-swap warning is OUT OF DATE. R5 is on
main, all eight rows are signed, and the gate is 1669/1674.
Kept for its reasoning, not for its state.

---

# HANDOFF — the R5 IMMERSION rung. **SEVEN ROWS SIGNED. THE SHADOWS ARE UNFLOWN.**

> **LAUNCH LINE (paste into a fresh session):**
> Read `docs/snow_R5_immersion_ledger.md` FIRST — it is the authority for R5's scope, its
> fences and its rulings. `docs/snow_row9_shadow_probe_results.md` holds the numbers that
> green-lit the shadows. `docs/snow_session_handoff_20260828_NEXT.md` remains the authority
> for R4/R4b, and `_20260827.md` for MECHANISM and MEASUREMENT generally.
> **Nothing is waiting on code.** Row 7 is waiting on CHAD — and probably does not exist.

---

## 1. STATE

| | |
|---|---|
| **Branch** | `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow` |
| **Head** | **16 commits this session, ALL COMMITTED, AHEAD OF ORIGIN, NOT PUSHED.** Base `9fdca369a`. |
| **Drive** | Rows 1-6 and 8 **SIGNED BY CHAD FROM THE SEAT.** Row 9 **BUILT, NOT YET FLOWN.** |

| # | Row | Ships at | State |
|---|-----|----------|-------|
| 1 | View dial, keys 1-4 | preset 1 = the old framing | ★ SIGNED |
| 2 | Freelook fence | 83 deg up / 31.5 deg down | ★ SIGNED |
| 3 | Track tint | `SEADS_TRACK_PACK` **0.45** | ★ SIGNED (he came DOWN from 0.65) |
| 4 | Roost | `SEADS_ROOST` 1.0 | ★ SIGNED, first arm |
| 5 | Exhaust | `SEADS_EXHAUST` 1.0 | ★ SIGNED, first arm |
| 6 | Rider breath | `SEADS_BREATH` 1.0 | ★ SIGNED, first arm |
| 8 | Sun sparkle | `SEADS_SPARKLE` 1.0 | ★ SIGNED, first arm |
| 9 | Shadows on snow | `SEADS_SHADOWS` 1.0 | **BUILT. UNFLOWN.** |
| 7 | Snow shading | — | **OPEN, probably ABSORBED — ASK HIM** |

---

## 2. WHAT MUST NOT BE REOPENED WITHOUT HIM

Every dial below is Chad-signed **from the seat**, this session.

- `SEADS_TRACK_PACK` **0.45**. He flew 0.65 and moved DOWN. The instinct to raise it so the
  track is "more visible" is the S1_SPEC RT-1 regression `ribbons.h:37-47` names.
- `SEADS_ROOST` / `SEADS_EXHAUST` / `SEADS_BREATH` / `SEADS_SPARKLE` all **1.0**.
- `day_period_s` **STAYS 300**. He was handed the exact number that would buy a longer shadow
  window (429 s for 180 s of light) and declined. **Do not re-propose it.**
- Carried from R3/R4: `full_depth` 0.30, `barren_shed_keep` 1.0, `min_depress_m` 0.55.

---

## 3. THE THREE THINGS A LATER AGENT WILL GET WRONG

**1. `texcoord.y` IS DUAL-ROLE, AND A LEAK TINTS THE WHOLE PLANET.** It carries ambient snow
DEPTH IN METRES on the planet pass and normalized COMPACTION on the snow-patch pass. Two
uniforms (`uTrackPackMix`, `uSparkleCompactArm`) are pinned to 0 EVERY FRAME on the planet
pass and disarmed after the patch draw — because **the LAKE MIRROR MESH SHARES THE PROGRAM**.
Follow that pattern for anything new. Rows 3 and 8 both do.

**2. ROWS 3 AND 8 SHARE ONE SIGNAL AND HAVE TWO KILL SWITCHES — ON PURPOSE.** Sparkle's
compaction suppression reads the same channel row 3's tint reads, but through its OWN arm
uniform. Welding them (reusing `uTrackPackMix` as the arm) would make `SEADS_TRACK_PACK=0`
silently un-suppress sparkle in the rut, corrupting row 3's own A/B.

**3. THE SHADOWS ARE RECEIVER-SIDE ON PURPOSE — NEVER PROJECT THEM.** Option (c): analytic
proxies tested PER FRAGMENT against the sun ray. **If an implementation ever starts needing
"where does the sun ray hit the ground", it is the wrong path.** The ground is not one
surface: the rider patch floats `lift_m` = 0.150 m ABOVE `facet + draw_fold_at`, so a decal
draped correctly on the fold lands 15 cm UNDER the patch the sled stands on. That is fence 3,
and it already cost this rung-family 0.56 m once.

---

## 4. THE WORK QUEUE

### 1. ⭐ HE FLIES THE SHADOWS. **NOTHING ELSE UNTIL HE HAS.**

```
cd D:\seads_sandboxes\sandbox_snow
taskkill //F //IM seads.exe      # then VERIFY zero instances -- see section 5
.\build-play\seads.exe
```
`build-play` (RelWithDebInfo), never `build/` — CLAUDE.md's ruling, `-O0` is 45x on the rider
skinning alone.

**IN THE AIR: the landing cue.** CROSS-SUN is the widest-margin approach; sun astern gives
weak separation; **SUN DEAD AHEAD BELOW 15 DEG IS THE GEOMETRIC FAILURE** (shadow enters at
2.1-5.4 m, too late to flare on). If the sun is down, WAIT — ~126 s of usable light per 300 s.

**ON THE SLED:** the measured outline — two ski shadows out at the stance, the belt behind,
and the skis should **YAW WITH HIS BARS**.

`SEADS_SHADOWS=0` is the A/B arm and the kill switch.

### 2. ⭐ ROW 7 — **ASK, DO NOT BUILD.**

He said "a little better shading" BEFORE rows 3, 8 and 9 all landed in that same shader. A
general pass now is unanchored work touching four signed rows for no named defect. See the
ledger's closing section.

### 3. THE NAMED FOLLOW-UPS — none urgent, none started

- **The sled's shadow BLINKS OFF crossing groomed trails and bank strips** — `bank_mesh` and
  `ribbons` have their own shaders. Mechanical, but it is SCOPE, so it is his call.
- **CASTER BUDGET: 9 of 16 used.** Enemy aircraft at 3 capsules each means EXACTLY TWO fit. A
  third needs cull-to-nearest or a raised cap. **His decision, not a silent default.**
- **The NIGHT sparkle is not compaction-suppressed** and gates on `winterSurf` (incl. ice). If
  he night-drives, the rut may glitter like undisturbed snow. One line, flagged not taken.
- **There are NO SUDBURIAN NPCs in the tree.** Caster 4 (townspeople) is unbuildable until
  they exist. This is separate from the RIDER, who does exist and does cast.
- **The bank-strip probe from R4b is STILL UNBUILT** (his "ski goes down into the road").
- ★ **THE RIDER SHADOW'S `rider_radius` = 0.25 m IS MEASURABLY WRONG.** The R4a session
  (`sandbox/r4a-phase0`) measured the clothed body off the SAME byte-identical GLB
  (`sha256 51082acbdcb47cc6...`): shoulder half-width 0.2460, waist ~0.128-0.146. So the
  single-radius capsule is **shoulder-width from neck to hips** — within 4 mm at the top,
  ~1.8x too wide at the waist. Under Chad's "large scale, really noticeable" ruling that is a
  visible defect. ⚠ **Do NOT retype those numbers** — `body_chain.h`'s law is "re-run the
  generator, never retype", and R4a's own first read was misaligned by FIVE STATIONS. The
  generator (`assets/character/sudburian_src/measure_body_chain.py`) is not in this tree; it
  arrives with R4a's rebase. And the fix is a PROXY-COUNT change — a capsule has one radius,
  so a taper needs a second stacked capsule (**budget 9 -> 10 of 16**) or a cone primitive.
  **Chad's call, on a row he has not flown.** Full detail in the ledger's R4a section.
- **The rider shadow's PELVIS anchor is CONFIRMED CORRECT.** R4a's measured pelvis station
  centre `z = -0.5848` matches this tree's `-0.584840`, so it **survives the rider swap** —
  no re-anchor needed. Only the **seated helmet crown (1.25 m)** remains legacy-derived and
  unconfirmed; R4a has been asked to measure it with provenance.

---

## 5. ★ PROCESS RULES THIS SESSION PAID FOR

**VERIFY A SINGLE INSTANCE BEFORE HANDING HIM AN ARM.** Twice, a stale `seads.exe` outlived a
`taskkill` and he swept a dial across TWO windows — he ruled row 3's 0.45 from *"the one that
is open ... I think it is at 0.45"*. He was told and ruled anyway, so the value is his, but
the provenance is a notch weaker than R3/R4's. **A sweep whose arm the driver cannot name is
the same failure as an A/B whose arms are indistinguishable.**

**THE GLSL IS NEVER COMPILE-CHECKED HEADLESSLY.** The unit tests pin shader SOURCE FORMS only;
nothing headless compiles them. **Launch the app as part of every verify pass** and count
`Program shader loaded successfully`. Fable flagged this itself rather than claiming coverage
it did not have — that is the standard now.

**A TEST BINARY MUST BE NEWER THAN EVERY SOURCE IT COVERS.** One agent cited a green run from
a stale tree. Check the timestamps and state them.

**PRICE THE FEAR BEFORE DESIGNING AROUND IT.** A consult and I both treated the 5-minute day
as possibly fatal to row 9. A 3 deg final from 50 m at 48.9 m/s is **19.5 s** against a
**125.8 s** window — six finals per window. "Intermittent" was a scary adjective standing in
for a number nobody had computed. The probe cost an hour.

**RE-RUN THE EXPERIMENT, DO NOT DEFEND THE READING.** Carried from R4b, and it paid out twice
more: a fresh consult overturned my (b)-projection recommendation and was RIGHT (the ground is
not analytic); and I caught that same consult pricing penumbra off a 0.35 deg sun when the
shipped value is **1.40** — 4x softer, which made its "resolves from several hundred metres"
wrong. Everyone was wrong about something; the measurements settled it every time.

**THE ROWS THAT LANDED FIRST-TRY INHERITED A NUMBER THAT ALREADY MEANT SOMETHING**
(`roost_flux` from the kernel; the shipped night sparkle's amplitude). The rows that INVENTED
one needed his seat (the tint strength, which he moved DOWN; the sled proxy shape, which he
rejected on sight). Before building, go look for whether the quantity already exists — twice
this session it did and nobody had noticed.
