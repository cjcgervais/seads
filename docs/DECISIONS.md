# Decisions

Standing decisions for the flight kernel. Each entry: date, decision, why, status.

---

## LIVE-BRANCH WATCH-ITEM — `feel/kernel-v5` moves past the seal (reconciliation is DONE)

**Resolved 2026-07-24:** the v4→v5 reconciliation merged. `main` in the game trees is
**`game-kernel-v5` (`36ee936e9`)** — the full game (tunnels, ballistics, Sudbury, Bf 109)
flies kernel v5, gate 797/797, first landing ever put down, Golden Felt Flight #1 flown on
that build. The old "feel branch diverges from a v4 main" danger no longer exists.

**Current resting state (2026-07-28):** the feel branch tip `cfe1bd7fe` is **sealed as
`flight-kernel-v6-2026-07-28`** — tag and branch backup both pushed to origin, and all
seven post-v5-seal commits grafted into the seads-recon conquest tree
(`sandbox/kernel-v5-reconcile` @ `5e27f237c`, gate 887/887, controller golden transferred
without re-record). Chad's word at the seal: "getting very near the point I don't touch
it again for a while." The watch discipline stays — a future session may move the branch
past v6 at any time:

- `reference/seads-feel/` is snapshotted at **`cfe1bd7fe` = the v6 seal (2026-07-28)** —
  current through the whole approved session, including the recorder graft. If the live
  tip has moved past that, the live tree is ground truth again until the next re-snapshot.
- **Every future session must check the branch state first** (read-only `git -C
  D:\flight_sim2\seads-feel log --oneline` / `git status` — never write there) before
  treating any dial value, snapshot, or cascade Code section as current. The branch has
  been observed to move between two commands of the same session.

---

## 2026-07-29 — FLOWN-APPROVED: the camera ANCHOR (S-keychase) — "precisely perfect"

**Chad's verdict on the stick, 2026-07-29: "now it is precisely perfect… It's the right
set up for my camera now."** Approved as the camera's finished state, and the trigger for
sealing the kernel as **v7** (advisement below).

**Chad ruled option 1**, landed as `5ea20d8c7` on `feel/kernel-v5`, gate **388/388**, zero
moved goldens, compiler-clean verified at the gate (the "new diagnostics" noise was clangd
missing include paths, not the build).

**The handback swing read fine on the stick** — consistent with the corrected ≈16°/≈0.7 s
estimate below, not the ≈96° first feared. No mitigation needed; the hard switch stands
and the "blend the handback" fallback is unused. Recorded so a future reader knows the
hard switch was flown deliberately, not by omission.

**Mechanism.** While override keys are flying and freelook is not held, the chase camera's
rest target swaps from the parked aim to the flight path, caught at a constant
`[camera] key_anchor_rate = 6.0 /s` (≈0.17 s). Mouse-aim flying is bit-identical **by
construction** — with no key held the helper returns the caller's own dials verbatim.
Selection goes through one pure helper (`render::chase_anchor`) called by both
`app/main.cpp` and the comfort instrument (`MiniCamera::advance`), so the shipped law and
the measured law cannot fork — the same route-the-live-path-through-the-tested-function
discipline as `app::tick`. Walk-back: `key_anchor_rate = 0.0` (structural off).

### ⚠ Measurement provenance — the 95.8° that motivated this cannot move

**Recorded because the mis-attribution is the reusable lesson.** The
`COMFORT turnsteady standing_oblique_deg 95.823` figure that drove this whole change is
from a scenario that holds **no override key**: `comfort_turnsteady` calls `turn_reaim`
every tick, which models a **mouse** pilot dragging the aim through the turn. There,
`keys_flying` is false, so `chase_anchor` returns the unchanged dials — **by the very
bit-identical property that makes S-keychase safe.** That 95.823° will still read 95.823°
after this fix, forever. It was never Chad's case.

Chad's case is the keyboard one, and it is measured by the scenario built for it,
`comfort_turnsteady_keys` (both arms, self-evidencing):

| leg | standing oblique |
|---|---|
| `turnsteady_keys_off` (v6 law) | **16.34°** |
| isolated law leg | 89.62° → 0.00° |
| `turnsteady` (mouse pilot — untouched, and untouchable, by this fix) | 95.82° |

So the fix targets the right case, but **the real magnitude of the reported symptom is
≈16°, not ≈96°** — roughly six times smaller than the headline number implied. The 89.62° →
0.00° leg is an *isolated law* check (the helper in isolation), not the end-to-end symptom.
Whether a standing 95.8° oblique in a sustained **mouse** turn is itself a problem is a
separate, unasked question — it is presumably the approved "camera follows the aim" feel,
but nobody has put that number to Chad.

### Watch-item: the handback swing — magnitude corrected downward

S-keychase is a **hard switch** on `keys_flying`. `cam_fwd` is carried and eased so there is
**no pop** at the switch, but the *rest target* jumps from the flight path back to the
parked aim the instant the last key comes up, and the camera then chases it under the
approved mouse-aim dials (`rate = lag_base + lag_gain·defl = 0.2 + 0.7·defl`).

**Correction to the earlier estimate in this file:** that estimate used defl ≈ 1.67 rad,
taken from the mis-attributed 95.8° mouse figure, and gave ≈96° over ≈1.2 s. Using the
*measured keys* deflection (≈16.3° ≈ 0.285 rad): rate ≈ 0.40 /s against a 0.285 rad gap —
**≈16° of swing over ≈0.7 s.** Mild, likely unremarkable on the stick. The concern was real
in kind but roughly 6× overstated in magnitude.

**It still scales with how far the aim actually parks.** In the scenario the instructor
keeps pursuing the parked aim on non-overridden axes, which is *why* it only reaches 16°.
A longer or harder key-turn than the 8-second scripted one parks the aim further off and
grows the handback proportionally, so the fly is still worth doing deliberately: hard
key-turn, mouse held still, then let go and watch the handback — key-down is the part
already known fixed. **If it reads wrong, blend the handback over a couple of tenths**; do
not raise the lag dials, which would move approved mouse-aim feel. It does not violate the
standing camera-independence constraint (state→camera throughout).

### RULED (2026-07-29) — the keys must NOT carry the aim. It is the whole point.

The option was put to Chad as a deeper fix: have the override keys **carry the aim along**,
so releasing them never hands back what was then (wrongly) called a stale target. **He ruled
against it, decisively, and the reason is a design statement about what the two input modes
ARE** — verbatim:

> "Not letting the keys carry their aim is precisely the point. Having freelook pressed is
> the mode that carries my aim and cam is free. In mouse aim, mouse is free, camera fixed —
> and it's the only way to access forward-looking oblique deflection shots in a merge while
> holding hard on a key and maintaining aim freedom at the same time. If I want to harness
> the aim into my nose then I press freelook."

**This is the mode duality, stated for the first time and now load-bearing:**

| mode | aim | camera |
|---|---|---|
| **freelook held** | **carried** (welded to the airframe) | **free** (orbit follows the mouse) |
| **mouse-aim** (no freelook) | **free** (mouse owns it) | **fixed** (anchored per `[camera] lead`) |

The keys are deliberately *outside* that duality: they move the airframe **without** taking
the aim, which is what buys **aim freedom while pulling hard** — the pilot holds a key
through the merge and keeps steering the reticle independently for a deflection shot. Making
the keys carry the aim would collapse the two modes into one and delete that capability.
Any future proposal to "fix" the parked aim under keys must be refused on this ruling.

**Chad's forward read, worth keeping as a prediction to check later:** "more can now be done
in mouse aim mode, and I suspect then my freelook will be only for situational awareness and
when I want to see / need to see obliquely — as opposed to not just *being* oblique."
I.e. v7 is expected to shift freelook from a *flying* verb to a *looking* verb. If a later
session finds freelook usage dropping and mouse-aim carrying more of the fight, that is this
prediction coming true, not a regression.

### Refinement (same day) — S-keychase is a GUNNERY mechanism, and the parked aim is a PLAN

Chad corrected the framing above, and it matters enough to restate: the value of flying on
keys without freelook is not only that the mouse stays free. Verbatim:

> "I can plan my aim for when I release the key override… but also, and most importantly,
> the fact that I am looking through the nose line of the plane from behind, or see its
> angle from behind its velocity — and it may be obliquely aligned at an enemy, but I see
> where I am shooting and plan my aim when hard-pressing maneuver without freelook."

Two corrections to how this repo had been describing it:

1. **"Stale target" was the wrong word and should not be reused.** The parked aim is
   **deliberate** — the pilot is pre-placing where he intends to be aiming when the keys come
   up. It is a *plan*, not a leftover. The defect was never that the aim was parked; it was
   that the **camera** was anchored to it, spending the eye on the plan at the moment the
   pilot needs the shot.
2. **The behind-velocity anchor is a shooting reference, not a comfort fix.** Sitting behind
   the velocity is what makes the **nose-versus-velocity angle** visible — the gun line
   against the flight path. The aircraft can be flying one way and obliquely lined up on an
   enemy another way; from behind the flight path that offset is legible and the shot can be
   read. From behind the parked aim it is not. S-keychase should therefore be classified with
   the gunnery/instrument mechanisms, not with the comfort program.

**Consequence for the handback:** the ≈16° swing on key release is the camera going *to the
place the pilot decided to look*. That is why it flew as "precisely perfect" rather than
intrusive, and it is an argument **against** ever adding the blend that was held in reserve —
softening it would blur the moment the plan arrives. Recorded so a future session does not
"improve" it.

### Consequence — the 95.82° mouse figure is very likely a FEATURE, not a defect

`COMFORT turnsteady standing_oblique_deg 95.82 / converged 0` was logged above as an
unasked question. **Chad has now effectively answered it without being asked.** That
scenario is a sustained *mouse* turn, where the camera anchors to a free aim — which is
precisely the "forward-looking oblique deflection" geometry he just described as the
capability he wants. The camera showing where the aim points rather than where the plane
goes **is the deflection view.**

⚠ **Therefore the instrument's own predicate is mode-blind and should not be read as a
verdict.** `comfort_detail::converged = oblique_deg < 10 ∧ up_debt_deg < 10` encodes an
assumption that the camera *ought* to end up behind the flight path. That is right for
keyboard flying (S-keychase now delivers it) and **wrong for mouse-aim**, where a large
standing oblique is the intended capability. `turnsteady converged 0` is the instrument
measuring the wrong goal for that mode, not a failure to converge.
**Recommendation to the harness agent (this repo cannot edit the live tree):** either
mode-qualify `converged`, or rename the mouse-mode metric so it reads as *deflection
geometry* rather than as a comfort failure. Left as-is it will keep being mistaken for a
defect — it already was once, in this very session, where it motivated a change it could
never affect.

**What the instrument says** (comfort table, shipped config, measured by the harness agent):

```
COMFORT turnsteady standing_oblique_deg 95.823   converged 0
COMFORT orient     oblique_at_fire_deg   0.375
COMFORT orient     peak_oblique_deg     51.269   (after the fire)
```

The orient verb fires correctly and collapses the camera to **0.375°** off the flight path.
Then it re-opens to ~96° and **never converges**. The D9 work fixed the release *instant*;
Chad reported the *standing* state, which returns within about a second of the cut.

**Mechanism.** The chase camera's rest target is anchored to the **aim** (`[camera] lead =
1.0` — "1.0 = the camera's rest target IS the aim"). Under mouse-aim that is correct and is
the approved feel: the aim is where you're going, and the reticle-floats-then-centres
behaviour comes from `lag_base`/`lag_gain`, not from `lead`. But when Chad flies on the
**override keys**, the keys move the aircraft while the aim stays **parked** — so the plane
flies out from under the aim and he watches it obliquely. **This is not lag against the
plane; it is the camera anchored to the parked aim.** Raising the lag cannot fix it, because
the *target*, not the catch rate, is what answers the wrong question here.
⚠ **Refined later the same day** (see the S-keychase entry above): the parked aim is
**deliberate** — the pilot's plan for the release — so it is not a "stale" target, and the
anchor swap is a **gunnery** mechanism (it makes the nose-versus-velocity gun line legible),
not a comfort one. Earlier wording in this file that called it stale is superseded.

**Chad's original sentence already said this** — "even if still turning and pressing hard
keys for control surfaces" — and the earlier framing (snap-at-release vs. a permanent
behind-lock that would delete `lag_gain`) presented a false pair and steered to the release
edge. The harness agent has said so plainly; recorded here because the framing error is the
reusable lesson, not the measurement.

**The three options put to Chad:**
1. **Anchor behind the flight path while override keys are held** (recommended by the
   harness agent). Rest target switches from the parked aim to the flight path only while a
   key is down and freelook isn't. **Mouse-aim flying is untouched** — `lead`/`lag_base`/
   `lag_gain` unchanged, so nothing Chad approved moves. Releasing the keys hands the camera
   back to the aim-anchored chase.
2. **Behind the flight path whenever freelook isn't held.** The literal reading of Chad's
   sentence, applied to mouse-aim too. **This does change approved feel:** the
   reticle-floats-off-centre-then-closes behaviour goes away, because the camera stops
   following the aim.
3. **Dial the lag faster** (`lag_base`/`lag_gain`). One line, no new mechanism, but
   **cannot fully fix it** — the target is still the parked aim, so a hard sustained
   key-turn still stands off, and it speeds up the mouse-aim float Chad liked.

**Docs-side check, so this is judged as a feel question and not a safety one: all three
options are clean against the standing camera-independence constraint** (below). Each is a
change to the camera's *rest target*, i.e. state→camera; none creates a camera-derived
quantity feeding `pitch`/`roll`/`yaw`/throttle, so the motion-sickness rubber-band cannot
form. Option 2 is nonetheless the one to fly most carefully: it removes a
visual-motion cue Chad has already approved, and approved feel is the thing this repo is
least willing to lose by accident.

**No cascade entry owns the camera anchor yet.** `lead`/`lag_base`/`lag_gain` are described
only in `controller.toml` comments and inside the freelook entry's Code section. Whichever
option is ruled, this mechanism has earned its own four-level entry — flagged as doc debt.

---

## 2026-07-28 — LANDED: retire S-relorient's D9 exception ("truly redundant") — ⚠ did NOT fix the reported symptom

Supersedes the OPEN QUESTION raised earlier the same day (S-relorient's D9 exception has no
second chance). **Chad ruled, and the change landed** — `feel/kernel-v5` commits
`35e31695f` (mechanism) + `13631ba92` (red-team folds), gate **386/386**, zero moved
goldens. **Not sealed, not tagged, not pushed**, and — the important part —
**⚠ it did not resolve the oblique-camera symptom Chad reported.** It fixed the release
*instant*; the symptom is a *standing* state. See the follow-on open question immediately
above this entry (camera anchor / standing oblique). Read the two together or this entry
reads as a success it wasn't.

**The report that forced it.** Chad, on the sealed v6: "there are cases where I press space,
use override keys for flying, then let go of space and continue with the override keys — and
the camera goes to an oblique angle." His spec, verbatim, and it is the sentence the whole
change serves:

> "Anytime my finger isn't pressing freelook, I am in chase camera directly behind and using
> mouse aim — even if still turning and pressing hard keys for control surfaces."

**The ruling.** The D9 "you're still maneuvering" exception is **retired**. The finger
leaving Space is the whole trigger. Two sub-rulings, both Chad's:
- **(a) Fire the FULL verb**, identical to a clean release — not a camera-only variant.
- **(b) Snap at release, keep the flown-in chase lag afterward** — not a permanent
  behind-lock (that would delete `[camera] lag_gain` and is a separate fly).

He is **keeping** the double-tap, but wants it *truly* redundant — he had to reach for it
precisely because the release failed. That is the test of this change: the double-tap
becomes a genuine backup rather than the only way out of a stuck state.

**Root cause — three sites in `app/instructor_tick.h`, all gated on `any_ovr`:** the
`release_orient` predicate (`orient_fired` withheld ⇒ `main.cpp` never hard-cuts `cam_fwd`
nor zeroes the orbit); the S7-hrz capture (`recov.reset()` instead of `capture()` ⇒ the
up-debt never rolls off); and the double-tap (the manual escape hatch suppressed under the
same condition). Two aggravating facts carried over from the open question and confirmed in
trace: all three are gated on `fs.released`, a **one-tick edge**, so there is **no second
chance** — letting go of the keys later re-fires nothing; and it is a **split, not a clean
no-op** — rule 3 still moves the reticle to guarded velocity while the camera doesn't cut.
The residual oblique Chad sees is `cam_fwd` on `ease_chase_forward`, which in a sustained
turn never converges. The orbit is *not* the culprit (it decays ~120 ms every non-freelook
frame regardless).

**Shape of the change** (worktree `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5`;
`app/` + `config/` only — the kernel firewall holds by construction, with one tune-data
field in `control/params.h` where `freelook_release_orient` already lives):
- **New knob, the standing structural-off-switch pattern:**
  `freelook_release_orient_with_keys`, **default `false` = today's shipped behaviour exactly**
  (every knob-off arm bit-identical), read via `optional_bool` like `release_orient`, set
  `true` in `config/controller.toml` as Chad's fly value.
- **All three sites relaxed behind that one knob**, so the two triggers can never diverge
  again. The `in.freelook_held && any_ovr` nesting branch is untouched — that is the
  while-held rule, not the release.
- **Sub-ruling with reach beyond the release edge, flagged as a real behaviour change:** with
  the knob on, an override pressed *later* no longer aborts an in-progress horizon roll.
  Required by "even if still turning and pressing hard keys"; safe because the D3 roll is
  open-loop — a gauge move about the aim forward, invisible to `control::step`, so it cannot
  fight the keys.
- **Reuse, not a third copy:** the guarded-velocity snap is currently duplicated verbatim at
  the release and double-tap sites. It gets hoisted to one file-local helper called from
  both — behaviour bit-identical, but it makes "one verb, two triggers" a property of the
  code rather than of two paragraphs of comment.
- **Code banners rewritten to the flown truth.** Three banners still state the D9 rationale
  as settled ("the next clean release orients"); they name the walk-back instead, so the next
  reader doesn't re-derive the dead rationale.

**Test discipline.** The existing pin "override held at release = legacy, no camera cut" is
**re-scoped, not deleted** — kept verbatim as the knob-OFF arm, proving legacy is exactly
reproducible. New legs (each mutation-verified, on a binary proved fresh): knob-ON full verb
fires; horizon debt retires with the key still down; **one-shot** — letting go of the keys
later fires nothing more (this guards against anyone "fixing" it with a deferred latch on
top, which was the shape floated in the open question and is now explicitly *not* the
design); double-tap fires with an override held; and knob-OFF ⇒ bit-identical across the
whole four-phase script (the strict-superset proof). Repro: hold Space → press override
mid-hold → release Space with the key still down → release the key several ticks later;
**assert on the phase after the release, not the release tick.** Harness seam respected:
`ClosedLoop` models the knob-OFF release and has no `orient_fired`, so these pins stay in
`test_relorient.cpp` (app::tick only). **No controller golden may move. If one does: STOP.**

**Trade, stated honestly.** With D9 retired, a release-while-holding-keys snaps the aim to
the flight path mid-maneuver — the same trade CARD 1 already flags for the double-tap. Chad
ruled for it; at that instant he is flying on the keys, which keep the turn. Walk-back is one
line (`release_orient_with_keys = false`) and restores today's kernel exactly.

**Status:** LANDED on `feel/kernel-v5` (`35e31695f` + `13631ba92`), gate 386/386, zero moved
goldens, **not sealed / not tagged / not pushed**, and **NOT YET FLOWN** — the fly card is
pre-filled in the live tree's `docs/flight-log.md`. 6 new test legs (both arms each); the
three pre-existing D9 pins re-scoped to pin the knob rather than deleted; three mutants
killed at 4/2/1 cases; fresh-context red-team returned no P0/P1, added two mutants of its
own (one proving the walk-back arm is genuinely pinned), and its P2s were folded.

**Doc gaps I flagged are all closed in the diff** (verified in-tree): `SPEC.md` §9.5 now
states the release fires the orient verb independent of held keys; S-relorient has a §0
supersession entry (line ~736); and the fourth site I found — the D9 clause buried inside
**S7-hrz's own §0 entry** — carries an explicit ⚠ RETIRED marker. Cascade entry:
`docs/cascade/freelook-orient-verbs.md`.

⚠ **`reference/seads-feel/` is now STALE for this mechanism.** It is a snapshot of
`cfe1bd7fe`; `app/instructor_tick.h`, `control/params.h`, `config/*` and `SPEC.md` all moved
after it. Re-snapshot only once Chad has flown and sealed — a snapshot of an unflown,
unsealed tip would enshrine a behaviour that may yet walk back.

---

## 2026-07-28 — Rudder-bias trim + S-relorient, Chad-approved ("okay we have a winner")

Two changes landed on `feel/kernel-v5` in one session, both flown and approved on Chad's
stick. Commits: `274432f35` (S-relorient), `385a43dbd` (yaw_scale), `468f2b352` (red-team
folds), `b2019cf43` (flight-log rows). Gate 380/380 at each step.

**1. Rudder trim: `yaw_scale` 2.2 → 2.0.** Chad's ask: "a little too much rudder bias in
the equation" — confirmed symptoms: nose sits crabbed / rudder always working, plus
violent snap-back at speed. No v5 commit had touched the yaw ladder; the pre-v5 tuning was
being exercised harder by v5's stronger energy model (higher V ⇒ the q-scaled yaw terms
bite more). 2.0 is the previously-flown MB-4 value, away from the AT-16 β wall.
**Chad's verdict carries a causal insight worth keeping:** "Now that the flight kernel was
given a more sufficient engine per weight ratio, the mouse aim and nose is responding
better without the need of so much rudder... it feels much better now to not have to chase
the mouse with so much rudder but now the plant is able to respond." — i.e. rung D's T/W
0.61 is *why* less rudder authority is needed: the airframe can now follow the aim with
lift instead of skidding onto it with yaw. Pre-agreed fallback rungs (NOT taken — symptom
resolved): `Cy_beta 2.5 → 1.5` if speed snap-back survived; `center_frac 0.0 → 0.3` if
crab-at-rest survived (⚠ that one walks back the Rung-M1 "nose in the MIDDLE" ruling and
was flagged as such). Walk-back: 2.2. Full ladder history:
`docs/cascade/rudder-coordination-ladder.md`.

**2. S-relorient: every freelook release fires the ORIENT verb.** Chad's ask: releasing
freelook should auto-orient (the double-tap behavior, automatic). Mechanism: on the
freelook release edge, the aim snaps to guarded velocity and the camera hard-cuts behind
the flight path — the same tested path the S-orient double-tap runs. Deliberate
exceptions: sub-stall/ballistic releases land on the nose (velocity lies there), and a
release while an override key is still held keeps legacy behavior (pilot is actively
maneuvering). Knob: `release_orient` in `[freelook]` — **optional-with-default-false in
the loader** (fixtures untouched, knob-off bit-identical legacy), `true` in the shipped
toml. Walk-back: one line, `release_orient = false`. The double-tap still works and is now
redundant; retiring it is an open question for Chad.

**Process notes worth preserving:** the plan-stage audit (this repo's session) caught a
real ordering defect — the snap must fire BEFORE the S7-hrz horizon-recovery capture so
the up-righting measures against the new forward on the same tick (release-orient
therefore rights the horizon slightly *better* than the double-tap did). The fresh-context
diff red-team came back SOUND-WITH-FIXES; its one real find (nothing pinned the fire as
one-shot — a re-fire-every-tick mutant survived the whole suite) was folded and
mutation-verified. 7 new test legs + 4 loader legs.

**Status:** LANDED and Chad-approved on `feel/kernel-v5`; in `reference/seads-feel/` as
of the 2026-07-28 re-snapshot; not yet reconciled to the game trees. Cascade entry:
`docs/cascade/freelook-orient-verbs.md`.

---

## 2026-07-28 — Auto-right quickening: `inverted_delay` 1.0 → 0.5 s ("3/3")

Same-day follow-up ask, flown and approved ("yes perfect as expected 3/3!" — the third of
three approvals that session). One TOML dial on the MB-right mechanism (Chad 2026-07-07:
"need to roll over on bank after about 2 s no gross inputs if belly up... slow roll off
ailerons"): the belly-up **rest timer** before the wings slow-roll upright halves;
`inverted_rate` stays 180°/s (the roll itself is unchanged, it just arms sooner). Landed
`e1684fdbb`, gate 380/380; verdict logged `cfe1bd7fe`.

Not a delicate change, and the reasoning is worth keeping: single dial, roll rate
untouched, and the scripted golden never dwells inverted so no goldens moved. One test
tripped **deliberately** — the "inverted plane at rest STAYS inverted" leg carries a
config-relative premise calibrated to the 1.0 s dial (`REQUIRE(window > 60)` ticks); at
0.5 s the inside-the-delay window is 48 ticks. That is the repo's designed tripwire for
exactly this kind of retune: the premise was re-derived honestly (floor 36 ticks, reasoning
in the comment) and the mutant it guards (un-gated wings-hold righting the plane) was
re-verified to die in the shorter window.

**Fly sentinel (standing):** a loop apex or slow roll where the hand rests a full half
second now auto-rights sooner. If it starts stealing inverted maneuvers, walk-back is
1.0, or 0.75 splits the difference.

---

## 2026-07-23 — The v5 rung ladder (rungs A → E), `feel/kernel-v5`

Chad flew `main` (v4-approved) and reported small, smooth mouse adjustments made the
elevator/rudder overshoot the aim circle and bounce — "at those small deflections it is
treating them like they are big deflections." Five rungs of measured, Chad-approved fixes
followed, each with its own dial and its own explicit walk-back/kill value (see
`tuning/evc2026-v5-rungE.md` §2 for the full table with pre-rung values). In order:

- **Rung A — S-truedepth** (`dc0b1d01c`): a capture event's "glance depth" is capped at its
  own momentum-earned stopping distance rather than a fixed rim target. Dial:
  `capture_depth_frac = 1.5` (walk back to `≤ 0` for the v4 fixed-rim behavior).
- **Rung A2 — sub-wall curve** (`c8e98afa3`): Chad's fly-1 verdict — "better, just needs a
  little more" on small/fine adjustments. Dial: `capture_depth_pow = 2.0` (walk back to
  `1.0` = rung A bit-identically).
- **Rung C — hold-the-line** (`f523177e9`): Chad's fly-2 ruling — "It should hold the line of
  my mouse inputs and try to get to my mouse until full stall — it might sink a bit as I
  begin the stall but the nose should stay where my mouse is asking." Dials: `K_aoa: 5.0 →
  10.0`, `pull_floor: 0.0 → 1.0` (0.0 is the structural OFF / bit-identical legacy tree).
- **Rung D — the arcade energy model** (`c0625ede1`): Chad's ruling — "give me the power — I
  had been intuiting all along that the airframe is being underserved" (this **supersedes**
  his own earlier 2026-07-08 `T_max = 9000` ruling). Dials: `k_induced: 0.05 → 0.015`,
  `T_max: 9000 → 18000` (T/W 0.31 → 0.61), `n_max: 16 → 32`. **Correction:** an earlier draft
  of this repo's tuning notes had these dial directions backwards — `0.05`/`9000`/`16` are
  the values you walk BACK TO (pre-rung-D), not rung D's shipped values.
- **Rung E — the knife edge + the red arrow** (`89447aba5`): see the entry below.

Full detail, measured before/after grids, and red-team notes for every rung:
`reference/seads-feel/docs/v5_kernel_handoff.md`.

---

## 2026-07-23 — Push/split-S commitment gate moved to 45° (rung-E)

**Decision:** The push/split-S commitment gate's `push_horizon_enter` threshold moves from
1.0° below horizon to **45.0°**, with `push_horizon_exit` at 40.0° (5° hysteresis band).
Commitment now requires BOTH lateral deflection AND genuine down-aim past 45° below horizon.

**Why:** At 1.0°, the old side-cone gate degenerated at big lateral deflections — a long
lateral drag whose aim merely grazed slightly below the horizon (an artifact of the
aim-frame's own arc, not player intent) became eligible for committed nose-down, producing
"mystery dives" the player never asked for. Chad's own words: "I think it's the knife edge
set too high on the horizon... nose-down with the bank over should need my down input too,
past like 45 degrees." Moving the gate to 45° means a shallow lateral graze can never reach
it, while a genuine big flick (hard lateral + mouse well down) still commits cleanly and can
carry a full split-S through past inverted.

**Companion decision:** the aim reticle stays raw/unclamped (it is not re-pinned to the
screen edge when off-frame); a separate red arrow is drawn at the screen edge pointing at the
true aim direction whenever it goes off-screen. This makes the otherwise-invisible
below-horizon curve of an off-screen lateral drag visible, while the 45° gate makes it
harmless even when unnoticed.

**Status:** LANDED on `feel/kernel-v5` (commit `89447aba5`, 2026-07-23) — this is real,
committed code, not a proposal. **Pushed to origin** (as of the same evening), and **not
reflected** in either the
`reference/evc2026/` snapshot (a different, prior-generation Luau kernel that never had this
mechanism) or in `main` (still v4). See `docs/cascade/push-gate-knife-edge.md` and the
reconciliation watch-item above.

---

## Standing constraint — Camera independence for motion sickness

**Decision:** The camera may lag, ease, and visually swing toward the aim direction or
heading, but it must never be an input to the flight control law — only a one-way,
downstream function of state. No camera-derived quantity (FOV, orbit angle, zoom factor,
lag-eased heading) may feed back into `pitch`/`roll`/`yaw`/throttle.

**Why:** This is the single mechanism standing between "the camera looks like it's swinging
toward centre" (the intended illusion — see `docs/cascade/mouse-aim-instructor-cascade.md`)
and actual motion sickness. If camera motion ever leaked into control, the mismatch between
commanded and actual aircraft response would be subtle, constant, and — especially on
SEADS's small non-euclidean sphere where curvature is always present — very hard for a
player to build a stable mental model around (see
`docs/cascade/spherical-earth-non-euclidean.md`).

**Evidence this is taken seriously in the existing code:** `BirdController.client.luau`'s
right-click aim-zoom is explicitly documented as "AWARENESS-ONLY... cannot affect flying" and
its steering is suspended while it's active specifically so the FOV change can't feed back
through the camera-projection aim into pitch/bank.

**Status:** standing, not a one-time decision. Applies to both EvC2026 and SEADS camera work.

---

## 2026-07-23 — Recorder graft landed: SOUND-WITH-ONE-FIX, schema gains raw_flap/raw_gear

The felt-flight recorder proposal was grafted into seads-feel (`d1e7dbe6b` on
`feel/kernel-v5`, gate 372/372) after a four-gate review run, not trusted: symbol walk,
tick-level tap at the accumulator seam (AT-9), structural read-only, and the differential
leg on the real spherical plant (same seed, recorder on/off, `LoopState` bit-identical;
480-tick round-trip replay onto stored pins; tamper signature verified). Two new permanent
tests in the gate.

**The one fix, and the lesson:** the proposal flattened `raw_in` as
pitch/yaw/roll/throttle, but `sim::Inputs` also carries `flap_cmd`/`gear_cmd`, which
raw-mode flights command inside `raw_in`. A recorded raw-mode flight with flaps would have
replayed with a clean airframe — bit-perfect divergence of exactly the silent kind this
harness exists to kill. Schema now carries `raw_flap`/`raw_gear`. Changed while the
`.seadsrec` format was still v1-unshipped, so it was free; a day later it would have been a
migration. **Standing rule: when flattening a struct into a recording schema, walk every
field of the source struct, not the fields you remember.** Proposal copies in
`harness/seads_recorder_proposal/` are synced to the grafted versions
(`test/harness/recorder.h`, `test/unit/test_recorder_firewall.cpp` at `d1e7dbe6b`), which
are now authoritative. Deliberate scope note: the in-game record toggle (main.cpp key
wiring) lands as its own small change at first real recorded flight.
