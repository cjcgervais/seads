# LADDER — PLAYER-JOURNEY ANGLE
## What the first ten minutes and the battle need: failure cost, recovery, legibility, feedback

Designer strand, angle **player-journey**. Worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. **READ-ONLY against main.** Nothing in `sim/`, `control/`, `config/`,
`test/golden/` was opened for write. No dial moved, no golden touched, no `ctest` run, no
`seads.exe` launched, no probe built, nothing pushed. `seads-recon` was opened for reading
only — four `.sledtape` files streamed byte-wise out of `build-play`, nothing written there.
The only file this strand wrote is this one.

**Judge weighting, as given:** EQUAL. Chad gave no ranking; comfort, fun and feedback weigh
the same. This ladder is ordered by *player-journey value per unit of risk*, not by which
third of the charter a rung serves.

**Confidence vocabulary.** **MEASURED** (I computed it this session from his tapes or read it
verbatim out of the shipped source) · **DERIVED** (arithmetic here on stated inputs) ·
**LITERATURE** (someone published it — evidence that it was *said*) · **GUESS** (labelled,
every time). Where I inherit another strand's measurement I say whose and whether I
re-derived it.

**Every numeric claim carries its killing mutation.** A claim without one is decoration.

**No rung is justified by a harness number alone.** Each cites a tape event AND one of Chad's
words, verbatim, with document and section.

---

## 0. WHAT I MEASURED MYSELF THIS SESSION

So that the ladder does not rest on relay. Five independent reads, all read-only:

| # | What | Result | Instrument |
|---|---|---|---|
| A | v17 pooled (tapes 86–91) | 12.02 min · **52 past-90 events (4.32/min)** · 41.4 s past 90° (**5.74 %**) · **5 R presses** (0.416/min) · 58 landings, **17 upright (29.3 %)**, 26 over 10 g · 15 recovered / **37 never (71.2 %)** | `tape_summary.json`, re-pooled by me |
| B | whole corpus (84 parsed tapes) | 206.08 min · 1032 events (5.01/min) · 1053.5 s past 90° (8.52 %) · 146 R + 84 `episode_start` + **9 `mount_or_other`** · 984 landings, **306 upright (31.1 %)**, **519 over 10 g** · 716 never recovered (69.4 %) | same |
| C | rollovers by surface, pooled | **TrailMain 334 / 19.79 min = 16.88/min** · Bush 516 / 124.89 = 4.13 · Road 171 / 50.45 = 3.39 · RockOutcrop 1.71 · LakeIce **0.85** · TrailTributary 0.65 | same |
| D | **every autoright `O` record and the `T` records around it** | six rightings in the current-map tapes; **throttle command = 1.00 in 6 of 6**; `steer_actual` **1.0000 in 4 of 6**, none at centre; bars still ≥0.1 at +0.50 s in 4 of 6; tape 91 @15130 holds **exactly 1.0 for the full 1.5 s** after the righting | raw byte stream of `sled_tape_{85,86,88,91}.sledtape` |
| E | who consumes the kernel's readouts | `assist_nm`/`right_assist_nm_now` → **only `app/spawn_policy.h`** · `belt_speed_ms` → **nothing** · `track_slip` → **nothing** · `engine_rpm` → **nothing** · `ground_speed_ms` → `render/sled_plumes.h` + `app/main.cpp` | `grep -rln` over `render/` and `app/` |

D is the one that changed my ranking. D-E-06 reported the steer half of it; nobody reported
that **the throttle command is pinned at WOT in all six**, so what R hands back is not just a
turned machine — it is a turned machine at wide-open throttle, because `sim/sled.cpp:293-294`
held the throttle at zero *because she was rolled* and lets go of it the instant R clears the
flag.

*Killing mutation for the whole table:* the column contract. `T` = 54 tokens, `O` = 45,
`ground_speed_ms` at pin 35, `steer_actual` at pin 28. I checked `ntok == 45` on every `O`
record I read and cross-checked `steer_actual` in the `O` against the `T` immediately before
it — they agree to nine digits in all six cases. Reorder `SLEDTAPE_PIN_D`
(`test/harness/sled_tape.h:98`) and every index shifts.

---

## 1. THE PLAYER-JOURNEY READ, IN FIVE SENTENCES

1. **The crash is not one event, it is a chain.** 52 past-90 crossings on v17 collapse into
   16 merged crash episodes — **3.25 tumbles per crash** — and only 15 of the 52 end in a
   resumed ride, a count that is identical under all four recovery predicates R3 tried
   (MEASURED by R3, re-derived exactly by its refuter). A player cannot name what he did
   inside a chain; the failure literature says that is precisely when he stops feeling
   responsible.
2. **The onset is a dig-in, not a wobble.** On the long episodes the machine carries a mean
   **13.68 m/s two seconds before the roll flag** and sheds nearly all of it inside 1–2 s
   (R2 §10.4, MEASURED and independently reproduced by R2's refuter: zero of 13 episodes had
   a prior-3.3 s max below 8 m/s). There is no cue for it — none of `track_slip`,
   `belt_speed_ms` or `engine_rpm` reaches a single output channel (my read E).
3. **Recovery hands him back a machine pointed the wrong way with the throttle open** (my
   read D).
4. **The battle is literally gated on the roll flag.** `app/main.cpp:7907-7910`:
   `sting_stance_ok` is true for `Sled && !sled.rolled`. 5.74 % of his v17 ride time is time
   in which neither the throttle nor the drone answers.
5. **And the thing holding the machine up 86 % of the time is invisible.** The comfort assist
   applies a non-zero body-z torque on **79.8–93.7 %** of ticks on every one of the six v17
   drives, at p95 up to **765 N·m** — half the machine's whole tipping budget — and reaches no
   eye, ear or panel.

The ladder below buys, in order: a recovery that returns him to the fight, a boundary that
stops snatching at him, and three channels that let him name what just happened.

---

## 2. THE LADDER

Eight rungs. Each is ONE named dial with an identity value that is bit-identical to today's
build, a measurable invariant with its killing mutation, one line for his drive, and a
law check against the frozen law (depth 0.77 · no governor · wheelie kept · STAND rights /
LEAN throws · ragdoll banned · one surface).

Six of the eight touch `app/` or `render/` only. **Exactly two touch `sim/` — PJ-7 and PJ-8 —
and they are ranked seventh and eighth for that reason.**

---

### PJ-1 — R hands the machine back at wide-open throttle with the bars hard over

**FELT PROBLEM, his words.** `app/main.cpp:8279-8286` (the `KEY_R` block, carrying the
DRIVE-2 ask):

> "I need a key for now that lets me autoright until we get the guy running back to the
> snowmachine"

and `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0**:

> "Let me slide around a bit arcade but **allow me to land on my skis more often after a
> roll** (even though R works). But not every time — allow it to happen."

**TAPE EVIDENCE. MEASURED BY ME** (read D above), streaming the `O` records and the `T`
record on each side of them out of `sled_tape_{85,86,88,91}.sledtape`. Six autorights exist
in the current-map corpus:

| tape | tick | `steer_actual` at the override | throttle cmd | `|steer_actual|` < 0.1 by |
|---|---|---|---|---|
| 86 | 5804 | **1.0000** | 1.00 | +0.467 s (then back to 1.0 by +1.25 s) |
| 88 | 46589 | **1.0000** | 1.00 | +1.233 s |
| 88 | 48179 | 0.4833 | 1.00 | +0.542 s |
| 91 | 6372 | **1.0000** | 1.00 | +0.733 s |
| 91 | 15130 | **1.0000** | 1.00 | **never within 1.5 s** |
| 85 | 60106 | 0.6618 | 1.00 | +0.317 s |

**Zero at centre. Four of six at full lock. Six of six at WOT.** R3's refuter adds the other
half of the picture: all five v17 presses land on the *last past-90 exit tick* of their crash
episode — the press is what ends the episode. So this is not a rare edge; it is the shape of
every recovery he takes.

**MECHANISM.** `app/main.cpp:8287` `if (IsKeyPressed(KEY_R) && app::autoright_legal(player,
sled.rolled))`. The block zeroes `sled_lean_lat` (`:8356`) and `sled_lean_fwd` (`:8357`) and
**never touches `sled_steer_cmd`** — while the C key, three hundred lines earlier, zeroes all
three: `app/main.cpp:8263-8266`, `sled_steer_cmd = 0.0;  // SK-1c: C recentres the bars too`.
The kernel's own `steer_actual` is not in the override's reset list either. And
`sim/sled.cpp:293-294` is
`const double throttle = (s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle);` — the throttle
is held at zero **by the roll** and released the instant R clears it.

**THE ONE DIAL.** `[sled_input] autoright_recentre` — **identity 0.0 = today's build, bit
identical** (the line is simply absent). At 1.0 the R block zeroes `sled_steer_cmd` beside the
two lean commands, exactly as C already does.

**INVARIANT.** On his next drive: **no `O` record is followed by `|steer_actual| > 0.1`
sustained past 0.5 s.** Today that is 4 of 6. Report it as "autorights returned straight /
autorights".

**KILLING MUTATION.** Set the dial and re-measure the same six-shaped events. If `steer_actual`
is still pinned past 0.5 s after an `O`, the app-side command was not the cause (the kernel
field is not in the override reset list either) and the rung is dead. Second: `autoright_legal`
(`app/player_mode.h`) gates the key, so a legality change silently changes what the metric
counts. Third, and it is the honest one — **the tape cannot tell a held A/D key from a stale
accumulator.** Tape 91 @15130 (full lock for the whole 1.5 s, throttle pinned, machine at
0.1 m/s) is consistent with him simply *holding* a steer key through the press, in which case
zeroing the command buys one frame and the rung is decoration. **Only his drive settles that**,
which is why the checklist line below tells him to take his hands off.

**DRIVE-CHECKLIST LINE.** "Roll her on the trail, hit R, and take your hands right off A and
D — does she drive away straight, or does she turn the moment you touch the throttle?"

**LAW CHECK.** Depth 0.77: untouched. **No governor:** the player's own key zeroing the
player's own command — nothing reads or moves the rider's mass. Wheelie: untouched. **STAND
rights / LEAN throws:** untouched — R is the scaffolding key on a separate path; the `stand`
self-right block (`sim/sled.cpp:1679-1800`) is not modified. Ragdoll: not involved. One
surface: no surface term touched. `sim/`, `config/`, `test/golden/` all unchanged — this is an
`app/` input-layer dial.

---

### PJ-2 — The groomed trail is where he rolls, and the one measured cure is a dial nobody has put under his thumb

**FELT PROBLEM, his words.** §0:

> "**I rule that it should be roll resistant.** … **Make it possible to roll but not the
> rule.**"

and Chad 2026-08-24, quoted verbatim in `sim/sled.h` at `traction_mu` (~lines 961-1005; ledger
entry D-C §1):

> "at high speeds, getting pulled in and flipping out like 15x is not desirable so yes take
> care of that"

**TAPE EVIDENCE. MEASURED BY ME** (read C above), pooling `roll_by_surface` over all 84 parsed
tapes: **TrailMain 334 past-90 events in 19.79 min = 16.88/min**, against Bush 4.13/min, Road
3.39/min and LakeIce **0.85/min**. The groomed trail — where a snowmobile should be most
planted, and where a new player rides — rolls **4.1× Bush and 20× lake ice.** D-B-6's per-tape
sign test: of the 12 tapes carrying more than 20 s of both TrailMain and Bush, **TrailMain's
rate exceeds Bush's in 12 of 12** (p ≈ 2⁻¹² ≈ 0.00024), spanning four kernel buckets and four
weeks. Tonight's tape 91: 7 rolls in 27 s of trail against 11 in 106 s of bush. Tape 90 is the
control in the other direction: **90 s on lake ice, zero rollovers**, while all 7 of its
rollovers happened in its 19 s of bush.

**MECHANISM.** `sim/sled.cpp:630-636` blends the per-patch `SurfaceDials` across a class
transition, gated on `gs.surf_mix > 0.0`. `surf_mix` is written in exactly one place —
`world/snowpack.cpp:722-746`, inside `if (p.class_blend_m > 0.0 && …)` — and
`config/world.toml:645` is `class_blend_m = 0.0`. **So `blend_dials` (`sim/sled.cpp:200-213`)
is never called on main.** The comment directly above that line, read verbatim this session:

> "the CLASS BLEND width … The step is what made one ski grazing a road edge throw the machine
> into 1.34 rotations at 24 m/s with no steer input (rho_eff 0 -> 260 under ONE ski = a pure
> yaw+roll couple). 0.0 = OFF, bit-identical. Measured on `seads_sled_probe bankgraze`:
> blend 0.0 -> yaw -482.7 deg, roll 166.6 deg, ROLLED; blend 1.0 -> yaw -11.6 deg,
> roll 52.7 deg, no rollover … **AWAITING CHAD'S DRIVE -- not ruled.**"

**THE ONE DIAL.** `[snowpack] class_blend_m` — **identity 0.0 = shipped, bit-identical by
construction** (`surf_mix` is identically zero and the blend branch is structurally dead). It
is in one of only two TOML blocks that reach the kernel, so this is a live A/B with no
recompile — which is exactly what RC-8 asks of every mechanism.

**INVARIANT.** TrailMain past-90 events per minute **on TrailMain** from his own next tape,
against today's 16.88/min — and Bush's 4.13/min must **not** fall by more than the trail's
does. If both fall together it is a stability change, not a boundary fix, and it has bought
"roll resistant" by a route his ruling did not authorise.

**KILLING MUTATION.** **Attribute each rollover to the surface at the episode's EXIT rather
than its entry.** If TrailMain's rate collapses under re-attribution, the machine was merely
*crossing* the trail, not rolling on it, and the class-step hypothesis loses its tape support.
D-B-6 names this mutation and **did not run it** — running it is this rung's first leg, it is
read-only, and it costs one pass over the corpus. Second mutation: the bankgraze numbers come
from `seads_sled_probe`, not from his tapes; if the shipped trail's class step is smaller than
the road's, the probe overstates the prize.

**DRIVE-CHECKLIST LINE.** "Same trail out of Chelmsford twice, `class_blend_m` 0.0 then 1.0 —
did she stop snatching at the trail edge, and did the drift still come back when you got on
the throttle?"

**LAW CHECK.** **Depth 0.77 UNTOUCHED** — `base_m` is not the dial and must not be; it stays
0.85 (`config/world.toml:554`), which is the parameter that yields his signed p50 of 0.77 m.
**No governor** — a terrain-sampling continuity fix; no torque added, nothing reads or moves
the rider's mass. Wheelie / STAND / LEAN / ragdoll: untouched. **One surface: read carefully —
this does not add a surface, it removes a discontinuity between two that already exist.**
⚠ **THE STICK RULING ALREADY COVERS IT**, verbatim from `LANES.toml:939`: *"[snowpack]
class_blend_m STAYS 0.0 — it is live-tunable and it is CHAD'S A/B, not the lane's call. The
lane measured 1.0 as the value it would pick; it ships 0.0 and he decides in the seat."*
**So PJ-2 is not a build. It is an ask, and it was reserved for him a week ago.**

---

### PJ-3 — Something has been holding the machine up 86 % of the time and he has never been shown it

**FELT PROBLEM, his words.** §0:

> "**Just allow the balance of body mechanism to ENHANCE ability, i.e. tighten a turn** instead
> of having to be the necessary condition of not rolling over."

and §0b:

> "**Allow for bad driving too but keep the benefit there for good riding.**"

Both sentences are unanswerable from the seat today: he cannot learn an enhancement he has
never been shown, and he cannot tell good riding from bad if the machine's contribution is
silent.

**TAPE EVIDENCE. MEASURED BY ME** from `tape_summary.json`, the six v17 drives:
`assist_active_frac` = **0.798 / 0.860 / 0.866 / 0.876 / 0.884 / 0.937** — the comfort assist
applies a non-zero body-z torque on **80–94 % of ticks on every single drive** — with
magnitude p50 **5–75 N·m** and p95 **215–765 N·m**. Against the machine's own tipping scale
(`mass_kg` 331 × 9.80665 × the 0.4635 m half-stance = **1505 N·m**), p95 reaches **51 % of the
entire roll budget**. And **read E**: `grep -rln "assist_nm" render/ app/` returns exactly one
file, `app/spawn_policy.h`, and that is two resets. Same for `right_assist_nm_now`.

**MECHANISM.** The term is written at `sim/sled.cpp:1881` (`torque_body.z += tq`, the A + B2
comfort assist) and stored in `sim::SledState::assist_nm`. `render/draw.h` (read this session,
lines 537-633) carries `sled_air_s`, `sled_hull_engage`, `sled_susp_x[3]`, `sled_grip_attached`,
`sled_weight_x/y`, `sled_hud_xray` — **and no assist field.** The instrument he already chose is
right there: the x-ray grid on the sweater, `render/draw.cpp:5250-5330`, `sled_hud_xray`
default true, `H` toggles, and his SK-1d words (commit `c2b7bfeeb`, 2026-08-25) are *"make it
pasted to his white/grey sweater … make the grid blue and the ball slag orange, glowing as the
themed color code."*

**THE ONE DIAL.** `render::kAssistTell` (+ `SEADS_SLED_ASSIST_TELL`) — **identity 0.0 = the
x-ray panel is pixel-identical.** Above 0 it grades the slag-orange ball's glow with a
**band-gated** `|assist_nm|`. One field carried into `render::DrawInfo`, beside
`sled_hull_engage`, which is already there for exactly this class of readout.

**INVARIANT.** At 0.0, a frame capture of the HUD region is byte-identical. Above 0, acceptance
is **his word** — "did you notice when she was helping you?" — never a number. The one number
worth reporting beside it is the **fraction of ticks the tell is lit** under the chosen band,
which must sit far below the 86 % duty or the tell is wallpaper.

**KILLING MUTATION.** The assist is non-zero 86 % of the time but its **median is 5–75 N·m —
0.3 to 5 % of the roll budget.** A tell gated on `assist_nm != 0` is on almost always and
carries no information, so **the band, not the duty, is the whole rung**; if no band can be
found that is both informative and infrequent, the rung is dead. Second mutation: which mass
the tipping scale uses. `sim/sled.h:543` says `mass_kg = 331.0 // machine + rider, TOTAL` and
`:784-785` says `rider_mass_kg = 87.5 // split OUT of mass_kg, NEVER added`, while
`docs/gi4_ride_handoff.md` §2 writes "(331 + 87.5) × 9.81 = 4106 N". **The bytes are on 331:**
`grep -n "p.mass_kg" sim/sled.cpp` returns exactly one line (224). Any figure quoted to him
must name the mass it used.

**DRIVE-CHECKLIST LINE.** "Ride the bush with `SEADS_SLED_ASSIST_TELL=0.6` — does the orange
ball light up at moments you agree the machine was saving you, or is it just on all the time?"

**LAW CHECK.** Depth: untouched. **No governor** — this *shows* a torque that already ships;
it adds no term and moves no mass. It is the opposite of the thing the kernel's own comment
forbids ("a hidden nudge is the RNG-shaped sin this kernel forbids", `sim/sled.h` at the
self-right block). Wheelie / STAND / LEAN / ragdoll / one surface: untouched. `render/` and
`app/` only; `sim/`, `config/`, `test/golden/` unchanged.

---

### PJ-4 — The dig-in that starts the crash makes no sound, and the track it comes from is silent and still

**FELT PROBLEM, his words.** `docs/roost_consult_packet.md` **§1** (Chad 2026-08-27):

> "**The conditions should be palpable and observable in the sled performance** … So I want to
> make sure our snow is meeting the **palpable precision standard**."

> "**Snow that is properly shaded and felt, not some cheap drawn in illusion**"

and §0, on the move he loves, which *is* slip:

> "I've seen lots of snowmobiles **drift around corners and then with throttle, straighten out.
> The feel is really good. Don't lose the feel**"

**TAPE EVIDENCE.** R2 §10.4, **MEASURED and independently reproduced by R2's own refuter**: on
tape 1's 13 long roll episodes the mean ground speed **2.0 s before the roll flag is
13.68 m/s**, and **zero of the 13** had a prior-3.3 s max below 8 m/s (tape 34 agrees:
12.54 m/s, one below 8). The machine carries 10–16 m/s, sheds nearly all of it inside 1–2 s,
and *then* lies over — a dig-in, not a balance failure. D-D measured the slip doing it: mean
`|track_slip|` **0.522** in contact and moving; `|slip| > 0.3` on **53.03 % of all ticks**
(RockOutcrop 70.2 %, Bush 65.4 %, TrailMain 53.1 %); belt running a mean **20.50 m/s faster
than the ground**; and **33.77 % of ground-moving ticks have `|slip| > 0.3` with
`roost_flux < 0.02`** — the track is spinning and throwing nothing, so nothing tells him.

**MECHANISM.** Three kernel readouts exist for exactly this and have **zero consumers** —
my read E: `grep -rln` over `render/` and `app/` for `track_slip`, `belt_speed_ms` and
`engine_rpm` returns **nothing at all**. `sim/sled.h:1307-1310` says otherwise in its own
words: *"Track SURFACE speed (the slip cue) — the rig reads THIS, never ground speed, because
a decoupled coast at 0 belt speed while still moving IS the 100 % slip an observer should
see."* Nothing reads it. `render/sled_model.cpp` (7 687 lines) has no track/belt/cleat/scroll
animation and `render::SledRig` (`render/sled_model.h:38-60`) has no belt channel. The engine
voice is driven by `sled_thumb`, the raw input scalar (`app/main.cpp:11820` →
`render/sled_synth.h::set_drivers`), not by `engine_rpm`.

**THE ONE DIAL.** `render::kSledSlipVoice` (+ `SEADS_SLED_SLIPVOICE`) — **identity 0.0,
bit-identical**: the layer's gain multiplies to zero before the soft-clip and
`test_sled_audio.cpp` passes untouched. Above 0, a fourth `SledSynth` layer — filtered noise
whose level rides `|track_slip|` × contact and whose brightness rides `belt_speed_ms`.

**INVARIANT.** At 0.0 the rendered audio buffer is sample-identical. Above 0 the honest
measurement is **duty**: with `|slip| > 0.3` on 53 % of ticks, the layer must sit under
`render/mix_levels.h`'s `kSledRunGain` or it becomes the loudest thing in the mix. Acceptance
is his word: "could you hear her let go before she went?"

**KILLING MUTATION.** If `track_slip` were near-constant the layer is a constant hiss and
worthless. It is not — p50 0.395, p90 0.955 (D-D) — so it spans the range. The real risk runs
the other way and is this rung's own test: **at 53 % duty, a slip voice that is not band-shaped
is ambient noise, and ambient noise is not a cue.** Second: the dig-in reading assumes
`ground_speed_ms` is world-frame; if it is body-frame, a machine sliding on its side reads ~0
while genuinely moving and the "sheds it in 1–2 s" claim softens. R2 UNVERIFIED-13 names this
and did not read the writer — **read it before quoting 13.68 m/s to him.**

**DRIVE-CHECKLIST LINE.** "Ride the trail hard with `SEADS_SLED_SLIPVOICE=0.5` — can you hear
the track break loose *before* she puts you down, or is it just noise?"

**LAW CHECK.** Depth: untouched. **No governor** — audio only; no term, no mass. Wheelie /
STAND / LEAN / ragdoll: untouched. **One surface**: it reads the kernel's one product per patch
and adds no second surface source (the anti-fork rule the roost plume already obeys).
`render/` only.

---

### PJ-5 — The roll he is grading never reaches his eye

**FELT PROBLEM, his words.** §0:

> "Yep I can do backflips, but **she's too unsteady. I rule that it should be roll resistant.**"

and §0b:

> "Get a fable consult and **the measure of it shall be if my intent is heard.**"

Roll is the quantity he is grading. He is grading it off a silhouette inside a level box.

**TAPE EVIDENCE.** D-D, pooled over 86 578 v17 ticks: tilt past 20° on **23.42 %** of ticks
and past 45° on **8.92 %**, p90 **37.25°**, p95 100.75°. Mine, from `tape_summary.json`
(tape 1, the longest single drive): `tilt_deg` p50 3.25°, **p90 128.75°**. And my read A: 52
past-90 crossings in 12.02 min of v17 over 16 merged crash episodes — **3.25 crossings per
crash** (R3-M1, re-derived to the digit by R3's refuter). None of it reaches the camera.

**MECHANISM.** `app/main.cpp:9827` is `pose.up = sup;` **unconditionally**, where
`sup = normalize(anchor_pos)` — the planet radial. (Verified this session: the only other
`pose.up = sup` is `:9871`, inside the `SEADS_MANCAM` debug rig; the live sled path is 9827.)
`app/rest_horizon.h` — the v13 rest-edge horizon-roll law, **shared by the aeroplane AND the
Sting** — is not included by the sled path at all.

**THE ONE DIAL.** `render::kSledCamRoll` (+ `SEADS_SLED_CAM_ROLL`) — **identity 0.0:
`pose.up == sup` exactly and every framing number is unchanged.** Above 0,
`pose.up = slerp(sup, body_up_tangent, kSledCamRoll)` with a hard cap.

**INVARIANT.** At 0.0 the camera basis is bit-identical (pin it beside `test_interp.cpp`'s
fixed point, or diff a frame dump). Above 0, acceptance is his word only — and a second,
purely defensive number: the blend must never approach the degeneracy the existing
`kSledLookElMax = 1.45` note already warns about.

**KILLING MUTATION.** At v17 p95 tilt of **100.75°**, a full blend inverts the view — so only
a partial blend (≈0.25–0.35) with a cap is worth a drive at all, and the rung dies on the
first A/B if he reads the roll fine off the silhouette. ⚠ **AND A PARAPHRASE, NOT HIS WORDS:**
session memory records that an R1 motion-blur / tunnel-vision speed FX in this same family was
reverted at his word for the reason "looked bad" (`sting-rpas-plan`, 2026-09-12, scrap tag
`scrapped/speedfx-r1-20260912`); **the raw quote was not found in this worktree** and D-D flags
it UNVERIFIED-2. It is carried here as a **risk flag only** and must never be shown to him as
his own sentence.

**DRIVE-CHECKLIST LINE.** "Same bush run twice, `SEADS_SLED_CAM_ROLL=0` then `=0.3` — which
one lets you feel her going over *before* she's gone, and does the second one make you sick?"

**LAW CHECK.** Depth / no governor / wheelie / STAND / LEAN / ragdoll / one surface: **all
untouched.** Camera only, `app/` + `render/`.

---

### PJ-6 — For a third of a second after every corner, the bars lie to him

**FELT PROBLEM, his words.** Chad 2026-08-25, `seads-recon/docs/snowform_measurements.md`
§M11 header:

> "**I am not enough able to hold a steady turn** as I am using wsad and it wont carve so well"

and Chad 2026-08-25, quoted verbatim in `app/main.cpp:8236-8241` (the SK-1d comment):

> "**steering is too slow**"

**TAPE EVIDENCE.** D-E-02, MEASURED over the nine current-map tapes: `cmd_minus_bar_abs_max` =
**0.3500** in tapes 86, 87, 88, 89 and 79 — *exactly* the DERIVED ceiling — and
`bar_behind_during_decay_p90` = **0.250–0.328** across all nine, over 570–671 decaying ticks
per tape. And D-E-01: **0 of 301** steer excursions on this map held a partial angle; the
median excursion's plateau is **0.0167 s — one frame at 60 fps.**

**MECHANISM.** `app/main.cpp:8253` `const double back = 3.0 * frame_dt;` — the app's
self-centre (verified this session). `sim/sled.cpp:303` slews `steer_actual` at
`steer_rate_per_s = 2.0` (`sim/sled.h:881`, and 2.0 in the `# param` header of all 84 tapes).
From full lock the **command** reaches 0 in 0.333 s while the **bars** need 0.500 s, so the
command runs 50 % faster than the machine can act. SK-1d's own comment, three lines above the
line that does it, states the law it then broke on the release: *"a command that outruns the
slew just queues up and lies to the HUD."* It fixed that on the push and reintroduced it on the
release.

**THE ONE DIAL.** `[sled_input] steer_centre_per_s` — **identity 3.0 = shipped, bit-identical.**
Rule it to **2.0**, matching the kernel's own bar slew.

**INVARIANT.** `cmd_minus_bar_abs_max` must fall from 0.350 to ≈ one tick of quantization
(≈0.017) on his next tapes, and `bar_behind_during_decay_p90` to ≈0. Both are already computed
by `docs/sled_audit/de_input.py`, so the instrument exists and costs nothing.

**KILLING MUTATION.** Tape 91's outlier of 0.983 is a `hands_on == false` episode, **not** this
mechanism (the kernel forces `steer_target = 0` while the app command persists), so the metric
must exclude grip-off ticks or it reports a number with nothing to do with the dial. Second: if
`frame_gap_ticks_p50` is ever not 2.0 — it is 2.0 in **all nine** tapes, i.e. he drives at
60 fps — the integrator advances at a different rate and the derived 0.333 s moves.

**⚠ THE HONEST CAVEAT, ON THE RECORD.** D-E's companion rung — `steer_hold_frac`, making the
bars *hold* — carries an explicit do-not-ship-alone warning, because M11.2/M12 separately
measured that **there is no carveable steer angle on Road at any speed tested**: every steer
from 0.02 to 0.26 rolls past 140° under a 12 s speed-held sweep. **A steer that holds, on its
own, buys a more repeatable rollover, not a carve.** PJ-6 is deliberately only the *release*
half — the half that removes a lie without adding a hold — and the hold half belongs behind a
plant-side rung this ladder does not propose.

**DRIVE-CHECKLIST LINE.** "Corner hard on the road and let go of A/D — do the bars come back at
the same rate the machine does, or does the HUD beat the skis home?"

**LAW CHECK.** Depth: untouched. **No governor** — it *slows* an app-side auto-centre to match
the machine; nothing moves the rider. Wheelie / STAND / LEAN / ragdoll / one surface: untouched.
`app/` input layer only — **`sim/steer_rate_per_s` is NOT touched**, which is what keeps RC-5
("THE FEEL IS SIGNED": steering gains stay put) intact.

---

### PJ-7 — The battle is gated on a flag with no hysteresis, and the hull rests inside its threshold

**FELT PROBLEM, his words.** §0b:

> "**DOnt make it impossibly hard, there is a batttle going on as well.**"

and, on the flag's other consumer, Chad 2026-08-26, quoted verbatim into
`config/scenario.toml [sled_comfort]`:

> "IF ON THE SEAT AFTER A ROLLOVER PRESSING THE STAND BUTTON … **MACHINE NEEDS TO BE TIPPING
> OVER** … NOT ABOVE 5KM/H … IT CAN FAIL TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD"

**TAPE EVIDENCE. MEASURED BY ME** (reads A and B): v17 spends **41.4 s of 721.5 s = 5.74 %
past 90°**, 71.2 % of past-90 events never return to steady driving, and `sled_tape_91` **ends
at 131.4° tilt, 0.41 m/s, throttle 0, `# sig` present** — a clean shutdown with the machine on
its roof, the last tape of the night. While `rolled` is set, the throttle is zero
(`sim/sled.cpp:293-294`) **and the drone cannot launch** (`app/main.cpp:7907-7910`,
`sting_stance_ok` = `(Afoot && man_upright) || (Sled && !sled.rolled)`). Between five and six
percent of his ride time is time in which neither the machine nor the weapon answers.

**MECHANISM.** `sim/sled.cpp:254`:
`const bool attitude_now = glm::dot(body_up, up_cg) < std::cos(1.31);` — **75.06°, one hard
edge, no hysteresis** (verified this session; D-A's 39-gate audit found this is the rule, not
the exception — the self-right's speed gate is the *only* two-threshold gate in the kernel,
while `sim/sled.h:229-231` claims "Hysteretic like every gate in this kernel"). The dwell
integrator at `:255-262` debounces the **rise** at `rolled_persist_s` 0.3 s, but
`s.rolled = attitude_now && s.rolled_hold_s >= rolled_persist_s` — so **once the hold has
saturated, `rolled` follows the 75.06° test substep by substep, at 1440 Hz, with no de-latch
delay.** And `config/scenario.toml:2124` states the C1 hull makes a downed machine *"rest
~65-75 deg instead of flopping inverted"*. **75.06° sits inside that resting band.**

**THE ONE DIAL.** `[sled_comfort] rolled_release_frac` — **identity 1.0 = release threshold
equals latch threshold = today's single edge, bit-identical by construction.** Below 1.0 the
flag latches at 75.06° and releases at a larger angle (0.93 ⇒ ≈70°), using the same
`right_assist_rearm_frac` shape (`sim/sled.cpp:1695`) the kernel already owns — the one gate in
it that is already hysteretic.

**INVARIANT.** **`rolled` edges per second while the machine is down.** No instrument counts
this today: `tape_summary.json` reports `kernel_rolled_flag_s` as a total, never an edge count.
**So this rung's first leg is read-only and free** — add a `rolled` edge counter to
`tools/sled_tape_audit.py` and run it over the 84 tapes. If no tape shows a machine resting in
the 65–75° band with a chattering flag, the rung is dead before a line of kernel is written.

**KILLING MUTATION.** This is **DERIVED, not observed** — D-A §3.1a says so and lists it as its
UNVERIFIED-3. And the tape cannot clear it either way: the tape pins once per tick at 120 Hz
while the kernel runs the test at 1440 Hz, so a chatter faster than 8.33 ms is invisible to the
corpus. **A null result from the tape does not clear the mechanism — it only fails to find it**,
and settling it properly needs `seads_sled_probe`, not a tape. Second: the C1 resting band
"~65-75 deg" is a shipped comment, not a measurement I re-derived.

**DRIVE-CHECKLIST LINE.** "Put her on her side on the trail and leave her there with W held —
does the engine note and the P prompt sit steady, or do they stutter?"

**LAW CHECK.** Depth: untouched. **No governor** — a readout's release threshold; no torque, no
rider mass. Wheelie: untouched. **STAND rights / LEAN throws: untouched, and deliberately so —
this rung does not go near `sim/sled.cpp:1679-1800`.** Ragdoll: untouched. One surface:
untouched. ⚠ It **is** a `sim/` change and a new `[sled_comfort]` key, so it is one of only two
rungs here that touch the frozen kernel; it lands under the `kernel-v14-leanlead` precedent —
ONE dial, identity = the old tree, gate == the baseline six by name.

---

### PJ-8 — The punch he asked for and the outlier that ends the run are the same linear term

**FELT PROBLEM, his words.** §0b — and note that both halves of this sentence are about the
same spike:

> "It should be arcadey to a degree so that it is fun., **SLiding banging, punchy, jumps. Just
> make it more stable.**"

and `docs/gi4_ride_handoff.md` §1 item 2:

> "**should be able to launch in the air**"

**TAPE EVIDENCE. MEASURED BY ME** (read B), pooled over all 84 tapes: **984 landings in
206.1 min = 4.78/min, 306 upright at +0.5 s = 31.1 %**, and **519 of 984 land above 10 g** peak
`|g_eff|`. R4-03's hang-bucketed peak-g p50 rises monotonically — **6.6 / 13.2 / 18.7 / 24.4 /
34.0 g** across the 0.15–0.5 s, 0.5–1, 1–1.5, 1.5–2 and 2 s+ buckets — with a worst single
event of **261.6 g** (tape 16, 2.48 s hang), re-derived exactly by R4's refuter.

**MECHANISM.** `sim/sled.cpp:705`, inside the per-patch loop:
`normal = p.susp_k * x + p.susp_c * xdot;` with `std::max(normal, 0.0)` as the only bound. At
touchdown `x ≈ 0`, so the first contact substep is **the damper alone**: `3 × 3600 × vz`
newtons, **linear in sink rate, with no blow-off.** At the correct machine weight —
**3246.0 N** (`mass_kg` 331 × 9.80665; `sim/sled.h:784-785`: `rider_mass_kg = 87.5 // split OUT
of mass_kg, NEVER added`, and `grep -n "p.mass_kg" sim/sled.cpp` returns exactly one line, 224)
— that is **3.33 g per m/s of sink**: ≈16 g at a 1 s symmetric hang, ≈42 g at 16 m/s. Every
real race damper is digressive above a knee for exactly this reason; this one is linear all the
way up.

**THE ONE DIAL.** `[sled_comfort] susp_damp_knee_ms` — **identity 0.0 = no knee = today's pure
linear damper, bit-identical by branch.** Above 0, the damper's velocity exponent falls above
that shaft speed (the shim-stack blow-off), leaving low-speed damping — the trail chop he has
been riding for five weeks — untouched.

**INVARIANT. Two-sided, and the second side is the real gate.** (a) Landing peak-g p90 in the
1.5 s+ hang buckets falls; (b) **a slow trail traverse replays byte-identical.** Measured on
`seads_sled_probe` over the same recorded sends, knee off vs on — R4's probe leg D-3.

**KILLING MUTATION.** If the shipped `susp_c = 3600` was fitted to landings rather than to trail
ride, the knee is the wrong tool and 3600 is already the answer. **There is no fit record for
`susp_c` anywhere in `docs/` that R4 or I could find**, and that absence is this rung's first
job, not its assumption. Second, and it cuts against this rung's own size: the tape differences
velocity across consecutive **tick** records (dt 0.008333 s) while the kernel runs 12 substeps
inside each tick, so a one-substep spike is smeared over twelve and the measured peaks read
**low** — the p50 column is trustworthy, the p99/max column may be partly sampling. Third, a
framing mutation that must be carried into any conversation with him: **"31 % of landings" is
not "31 % of jumps."** R3's refuter established that **93 % of the 58 counted v17 air events are
sub-second hops** (hang p50 0.39 s; only 4 of 58 over 1.0 s). This rung is about the tail, not
about the typical event.

**DRIVE-CHECKLIST LINE.** "Send the biggest bank you can find, knee off then knee on — did she
still land *punchy*, or did the knee make her mushy?"

**LAW CHECK.** Depth: untouched. **No governor** — a valve law on a force that already exists;
nothing reads or moves the rider's mass. **WHEELIE KEPT** — `track_pitch_half_m` is not touched,
and D-A §6.4 measures that dial as the wrong place to buy wheelie back anyway (0.20 → 0 red,
0.18 and 0.22 → 5 and 7 red). STAND / LEAN / ragdoll / one surface: untouched. ⚠ `sim/` plus a
new `[sled_comfort]` key — the second of the two frozen-kernel rungs, same precedent and same
gate bar as PJ-7. **And it is ranked last on purpose: it is the only rung on this ladder that
could take away something he asked for by name.**

---

## 3. RULINGS CHAD MUST GIVE FIRST

Six, shortest first. Three of them block a rung outright.

**RULING 1 — The six drives of 2026-09-17 have no words. One line each on tapes 86–91.**
What you were doing and how it felt. Tape 91 ended with the machine on its roof at 131.4°;
tape 89 had **zero** rollovers in 127 s on almost the same terrain. Until those two lines
exist, every "how it feels today" number in this whole audit is behaviour without an
attribution, and rule 6 of the charter — no feel recommendation on a harness number alone —
means **no dial on this ladder may move.** (D-C O14; R3 UNVERIFIED-1; the audit plan §6 item 2,
still unanswered.) *This is the cheapest and highest-value thing available.*

**RULING 2 — `[snowpack] class_blend_m`, 0.0 vs 1.0. It was reserved for you a week ago.**
`LANES.toml:939`, verbatim: *"it is live-tunable and it is CHAD'S A/B, not the lane's call. The
lane measured 1.0 as the value it would pick; it ships 0.0 and he decides in the seat."*
**PJ-2 is entirely this ruling** — there is nothing to build, only an A/B to drive.

**RULING 3 — Should the machine still cut the throttle the whole time she is on her side?**
`sim/sled.cpp:293-294` zeroes the throttle whenever `rolled` is set — 5.74 % of your v17 ride
time — and the same flag gates the drone launch. PJ-7 asks whether the flag's *release* should
be hysteretic. The prior question, which only you can answer, is whether that flag should own
the throttle at all when there is a battle on.

**RULING 4 — Do you want to be able to lean and aim at the same time?** ⚠ **NO RUNG WAS
WRITTEN FOR THIS, BECAUSE THE LAW QUESTION COMES FIRST.** The Sting launches from the seat —
`app/main.cpp:7907-7910`, carrying your 2026-09-04 ruling *"press P on the snowmachine …
deployment from the seat to the Sudburian's hands to take aim"* — and while the launcher is
shouldered the mouse drives `sting_aim_az/el` and **the lean freezes wherever it was**
(`app/main.cpp:6336 / 6363 / 6414`: the chain is `if (Drone) … else if (sting_shouldered) …
else if (!freelook_held) { lean }`, and `lean_return_tau_s` is **0.0 in all 84 tape headers**,
so there is no return). You hold `|lean| > 0.98` for **15–44 %** of your ride time (D-E-04), so
the frozen body is usually a leaned-over one. Three answers are possible: (a) leave it frozen;
(b) let the lean fall to centre while shouldered — **which is the app moving the rider without
you, i.e. the §0b governor line, and needs your explicit word**; (c) re-bind the aim off the
mouse. *(Found by R3's refuter re-deriving R3-M8 against the source and overturning it; the
mouse has three claimants, not two.)*

**RULING 5 — `right_assist_nm = 2400` has never been ruled, and its own comment argues 1500.**
`config/scenario.toml:2082-2085`: the prose reads *"1500 gives a one-press ceiling of 18.6 +
asin(1500/1932) = 70 deg … **Chad rules the final value on his drive**"* and the next line
ships **2400.0**, 1.6× it. The kernel default is **0.0** (`sim/sled.h:224`). You drove v3 at
2400 and said *"it works now, but a multiple press from full inversion should not be able to
right it and it looks terrible"*; v4 changed the mechanism under it, your v4 drive FAILED for
the lean-coupling reason, `31e0dd96a` fixed that — and **no drive of the fixed form is on
record.** (D-C C2 and §6 item 3.)

**RULING 6 — Is GI3 signed by your silence, or unjudged?** Every rollover complaint on file
predates the GI3 rollfix; the drive an hour after it committed ran a stale binary (19 `cparam`
lines against a 25-line roster); and every drive since 2026-08-21 has produced words about the
rider, the walk, the grip and the camera — **never about rolling.** (D-C C8.) That silence is
either a pass or an unasked question, and only you can say which. It changes whether this
ladder is fixing a live complaint or re-tuning against a five-week-old one — which is R2's own
C-0 and the largest single risk in the audit.

---

## 4. WHAT THIS LADDER DELIBERATELY DOES NOT PROPOSE

Named, so nobody rediscovers them as omissions.

- **No value for any dial.** Every rung names the dial and its identity and stops. RC-5: "THE
  FEEL IS SIGNED." Picking a number is his seat, not this document.
- **No roll-stiffness change, no damping change, no threshold raise.** R2-C2 is right that the
  open half of his ruling is **frequency**, and that the onset is a deceleration event, not a
  balance failure — so a roll term would treat a symptom. `roll_damp_nms` (D-A §6.1) is a
  ruling about the arc-vs-flick ladder, not a rung, and it is his.
- **No steer *hold*.** D-E's `steer_hold_frac` is the obvious companion to PJ-6 and is
  deliberately left off: M11.2/M12 measured that no carveable steer angle exists on Road at any
  speed, so a hold alone buys a more repeatable rollover. It belongs behind a plant-side rung.
- **Nothing aimed at the air.** `k_gyro_react` would be the industry-precedented in-air control
  (Rainbow's "Reflex Gyro", R4-15), but `sim/sled.cpp:1354`'s `rep_belt` cannot fall below body
  forward speed, so arming it today buys **nose-up only — the half that makes a landing worse**
  (R4-14, confirmed exactly by R4's refuter). The belt expression is the blocker, not the dial,
  and that is a mechanism rung, not a player-journey one.
- **Nothing about latency.** D-E-07 measures ≈33–50 ms motor-to-photon, below the first step at
  which MacKenzie & Ware found any measurable cost. "Add smoothing or prediction" is not
  supported by anything in this audit.
- **No lean-scale change.** D-E's `lean_px_full` is a real candidate (band *crossings* correlate
  with rollovers at r = +0.815) but the correlation is n = 9 and **does not reach p < 0.05**
  even before controlling for ride intensity, and the lean is the one mechanism he has praised
  by name. Not on this ladder.
- **No session-length or tape-count claim.** These are a developer's own fly-tests; a short tape
  is usually a rebuild, not a quit. Using them as engagement evidence would be exactly the
  harness-number-alone the charter forbids (R3-M9/UNVERIFIED-2).

---

## 5. UNVERIFIED — what this strand could not establish

1. **No felt report exists for tapes 86–91.** Every v17 number here says what the machine and
   his hands did, never how it felt. RULING 1.
2. **The 2.0 s crash-episode merge is a choice.** At 1.0 s tonight reads 18 episodes, at 3.0 s
   and 5.0 s it reads 16 — stable above 2 s, moves at 1 s (R3, re-derived by its refuter).
3. **`rolled` edge-rate (PJ-7) is DERIVED and the tape cannot settle it.** 120 Hz pins against a
   1440 Hz test.
4. **The class-step / trail-edge link (PJ-2) rests on entry-surface attribution.** The exit
   attribution mutation is named by D-B-6 and **has not been run.**
5. **R2 §11.3's "the assist fixed recovery" is confounded** — different drives, different
   terrain, five weeks and several kernels apart. It is DERIVED, not MEASURED, and the clean
   experiment (replay tape 1 through the probe at `right_assist_nm` 0.0 vs 2400.0) has not been
   run. Nothing here leans on it.
6. **The motion-blur revert (PJ-5's risk flag) is a paraphrase from memory**, not a quote.
7. **`ground_speed_ms`'s frame was not read.** PJ-4's dig-in number and PJ-7's speed gates both
   assume world-frame.
8. **No probe was built and no test was run.** Every number here is either read from shipped
   source or read from a tape he recorded.

---

*Designer strand, angle player-journey. 2026-09-18, against `ed0a43ce8` (== `origin/main`'s
kernel text). Read-only. No dial changed, no config edited, no golden moved, no test run,
nothing pushed. Nothing written in `seads-recon`.*
