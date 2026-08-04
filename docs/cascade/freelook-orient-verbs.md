# Freelook and the ORIENT verb — letting go and getting put back together

What happens to your aim, your camera, and your horizon when you hold freelook, maneuver
under it, and release it.

> **RE-GROUNDED 2026-08-03 to the v12 seal (`e362df289`). This entry previously ended at the
> v7 seal and was wrong at the FEEL level, not merely stale at the Code level.** The **v9
> camera arc (S-nosesnap, `b4c0751`)** changed the mechanism's answer to its own central
> question. Both of the following were true at v7 and are **false at v12**:
>
> - *"the release snaps your aim to your flight path (guarded velocity)"* → **the release snap
>   lands on the NOSE, at every speed, and is a no-op on the aim by construction.**
> - *"the horizon rolls itself level"* → **the entire horizon debt is retired INSTANTLY, in one
>   tick.** The eased profile is superseded at its only consumer.
>
> Superseded text is kept below and marked, never deleted — the v7 reasoning is load-bearing
> lineage and the two rulings that overturned it are Chad's own.

---

## 1. Feel — pilot voice

Hold freelook and the world unhooks from your mouse: you're looking around while the
plane keeps flying the line you last gave it. The mouse moves your *eyes*, not the nose.

**⚠ CORRECTED AT v9 — read this before the v7 paragraph below.** Two things changed, both on
Chad's word, and together they change what letting go *feels* like:

**1. While freelook is held, your aim is welded to your nose** (the §5b weld, `b4c0751`) — keys
held or not, every tick. In his words (2026-07-29):

> *"the nose aim becomes welded to the nose directionality"*

The mouse moves your **eyes only**; it never touches the aim during freelook. So at the instant
you let go, **the aim is already on the nose.** The release snap therefore **re-aligns to the
current-tick nose and is a no-op by construction** — sub-degree, one tick of rotation's worth.
**The aim never jumps.** The old guarded-velocity target was diagnosed as *term A of the v9
defect*: an AoA-sized aim-and-camera jump at the moment the mouse took back over.

**2. The horizon rights INSTANTLY, not gradually.** Chad, verbatim, superseding the
2026-07-07 *"eased, not a snap"* ruling for this case:

> *"Snap to view upon release of freelook, no eased anything as I need to immediately view the
> back of my plane, the aim, the nose, everything — making it lag there is going to disorient."*

The whole up-debt is retired as **one gauge roll on the release tick**. The ~0.3 s of
rolled-world the eased profile left behind was *term B of the v9 defect* — measured at **44.9°
of roll still standing at the fire tick.**

**So the v12 release is: camera cuts behind the nose, aim already there, horizon upright — all
on one tick.**

---

*Superseded v7 text, kept for lineage:* ~~Letting go is the moment that matters. The ruling that
finished this mechanism (Chad, 2026-07-28) is that release should put you back together
**automatically**: release mid-turn and you're instantly seated behind your own flight path —
camera cut behind the velocity, aim on it, horizon rolling itself level.~~ The fly-card language
survives intact: "release space mid-turn → instant chase-behind every time." Before this, that
composed "put me back together" move was the double-tap orient (comfort mode, Chad 2026-07-17);
now the release itself is the verb, and the double-tap is redundant.

~~One deliberate exception, because it's what a pilot would want:~~
- ~~**Falling, not flying** (sub-stall, tail-slide): you're seated behind the *nose*, not
  the velocity — the velocity points somewhere useless, straight down or backwards.~~

**⚠ NO LONGER AN EXCEPTION — it is now the universal rule.** v9 made **the nose primary at
every speed**, so the sub-stall/tail-slide case stopped being a carve-out and became the only
behaviour. The instinct behind the old exception was right and it simply generalised: the
velocity can point somewhere useless, and the nose is where the pilot is looking.

There used to be a second exception — **still maneuvering**, an override key held at the
moment of release, where nothing snapped. **Chad retired it on 2026-07-28.** The rule is
now his sentence, and there is no key-held carve-out in it:

> "Anytime my finger isn't pressing freelook, I am in chase camera directly behind and
> using mouse aim — even if still turning and pressing hard keys for control surfaces."

Why it went: the exception had no second chance. Releasing Space with a key down spent the
one-tick release edge, and letting go of the keys afterward oriented nothing — you had to
press and release Space *again*. And it wasn't even a clean no-op: your **reticle moved**
to the flight path while the **camera didn't cut** and the **horizon debt never rolled
off**, leaving you rolled with the aim somewhere the eye wasn't. Chad's words for what he
saw: "the camera goes to an oblique angle." He was reaching for the double-tap to escape
it — which is exactly what proved the release was broken.

**Sealed** as `flight-kernel-v7-2026-07-29` — but retiring D9 **was not, by itself, enough**,
and the reason is worth keeping.

Measured after it landed: the verb fires and collapses the camera to 0.375° off the flight
path — then it re-opens, because fixing the release *instant* never addressed the *standing*
state. The oblique Chad was describing came from the chase camera's rest target being
anchored to his **parked aim** (`[camera] lead = 1.0`) while the override keys flew the
plane out from under it. Not lag against the plane — and not a *stale* target either: the
parked aim is deliberate, the pilot's pre-placed plan for when the keys come up. The defect
was spending the **eye** on that plan at the moment he needed to see his **shot**. A second
mechanism, **S-keychase**, was added for it — the rest target became the flight path while
keys flew — and Chad flew the pair as "precisely perfect."

⚠ **S-keychase was then retired in full in v8** (`flight-kernel-v8-2026-07-29`). It turned out
to be an over-correction: **retiring D9 alone was the fix**, and the second mechanism fought
the mouse during mouse-aim-plus-keys flying, which no scenario had modeled. **Everything in
this entry — the D9 retirement and the release-edge orient verb — stands unchanged and is what
v8 keeps.** The standing-state half is gone; see
`docs/cascade/camera-anchor-mode-duality.md`.

**This entry governs the release *edge*; the *standing* state between releases belongs to
`docs/cascade/camera-anchor-mode-duality.md`** — including why the control-surface keys must
never carry the aim. The two were repeatedly confused during the session that produced both,
so read them together.

## 2. Principle — instructor voice

**⚠ AMENDED AT v9. A sixth mechanism now fires FIRST and changes what the others do.**

0. **The §5b WELD (`b4c0751`) — the mechanism this entry was written without.** On *every*
   freelook-held tick, keys or not, `aim := nose`. The aim does not "freeze" during freelook —
   **it rides the nose.** Entry does the welding: the pre-freelook mouse command stops driving
   the plane at the spacebar press, the nose holds its heading, and QWEASD is the only control
   while you look. **Everything below inherits from this**: rule 2's "snap to nose once" and
   rule 3's release snap are both near-no-ops under the weld, because the aim is already there.

Five mechanisms compose, in the order they fire on a release tick:

1. **Freelook rules 1–3** (the aim-hold contract): while held, the aim ~~is frozen~~ **rides the
   nose (mechanism 0)** and the mouse feeds the orbit camera (rule 1). If an override key fires
   during the hold, the aim snaps to the nose once (rule 2) — the stick took over, the stale aim
   is a lie. On release after an override was used, the aim snaps to ~~*guarded velocity*~~
   **the NOSE** (rule 3). **⚠ v9: the guarded-velocity target is RETIRED — the nose is primary
   at every speed.** The v7 rationale (*"the maneuver took you somewhere; resume from where
   you're actually going"*) was overturned: resuming from the velocity moved the aim off the
   nose at the handover, which is the jump Chad was feeling.
2. **S-relorient** (2026-07-28): a mouse-only release — which under rule 3 used to move
   nothing — now fires the same snap (**v9: to the NOSE**, not guarded velocity), plus the
   camera cut. Behind a knob
   (`release_orient`): default-false in the loader (knob-off is bit-identical legacy;
   test fixtures untouched), true in the shipped config.
   **ADDENDUM, same day — the D9 exception retired.** A second knob,
   `release_orient_with_keys` (default false = sealed-v6 legacy exactly, `true` shipped),
   relaxes the `¬any_override` term at *all three* sites at once — the release predicate,
   the S7-hrz capture, and the double-tap — so the release and the double-tap are now
   literally one verb through one shared `orient_snap_dir` call. **Sub-ruling that reaches
   past the release edge:** an override pressed mid-roll no longer cancels an in-progress
   horizon righting (a freelook re-press still does). Required by "even if still turning";
   safe because the D3 roll is an open-loop gauge move `control::step` cannot see.
3. ~~**The guarded-velocity snap**~~ **The NOSE snap** (shared by rule 3, the double-tap, and
   S-relorient), `orient_snap_dir`. **⚠ v9: this helper is now unconditional** — it returns the
   nose and reads no speed and no velocity at all. ~~aim goes to the velocity direction *only
   if* the aircraft is genuinely flying forward — above ballistic speed and with velocity ahead
   of the nose — else to the nose.~~ The guard, the ballistic floor and its hysteresis are all
   dead code paths at v12; the snapshot notes the now-unread `cp` parameter as a recorded future
   cleanup that deliberately **did not** ride along with v9.
4. **S7-hrz horizon recovery — ⚠ SUPERSEDED AT v9 AT ITS ONLY CONSUMER.** On the release edge
   the **entire** up-misalignment is now retired as **ONE gauge roll in the same tick as the
   forward snap** — not captured and rolled away over ~0.3 s. Still a gauge move, invisible to
   `control::step`; still §9.1-legal as a discrete player-commanded event rather than a
   continuous easing. **`st.recov` is permanently inert** at v12 (its `reset()`s remain,
   harmless). **Ordering is still load-bearing and survives unchanged:** the snap fires *before*
   the righting, so the angle is measured about the released forward on the same tick — the
   v6 ordering, re-confirmed. ~~(…the roll retires a stale debt about the wrong axis…)~~ The
   old note that release-orient *"rights the horizon slightly better than the double-tap"*
   still holds, and for the same reason.
5. **CQ2 ease-back**: mouse→aim input stays suspended for a beat after release (the
   release tick included), so the freshly snapped aim can't be smeared by residual mouse
   motion.

The camera cut (`orient_fired`) is the one thing S-relorient adds where rule 3 already
snapped: the caller hard-seats camera-forward to the aim. Fire-once discipline is pinned
by test — the red-team's one real find was a re-fire-every-tick mutant surviving the
suite; a one-shot pin now kills it.

## 3. Math — engineer voice

- **⚠ RETIRED AT v9 — `orient_snap_dir` is now unconditional:**
  ```
  dir = orientation · (0,0,−1)          // the nose. No speed test, no velocity read.
  ```
  ~~**Guarded velocity:** with `v = velocity`, `spd = |v|`, `nose = orientation · (0,0,−1)`:
  `dir = v/spd` if `spd > v_ballistic ∧ (v/spd)·nose > 0`, else `dir = nose`. Ballistic
  floor is hysteretic: `v_ballistic = 30 m/s` entry, `v_ballistic_exit = 40 m/s`.~~
  **`v_ballistic` and `v_ballistic_exit` are no longer read by this mechanism.** Anyone
  reasoning about a release from these two constants is reasoning about v7.
- **The weld (§5b, v9):** on every tick with `freelook_held`, `aim.forward := nose`,
  unconditionally — evaluated *before* the release branch, so the two are mutually exclusive
  on a given tick.
- **Release edge:** `released = freelook_prev ∧ ¬freelook` (one tick, edge-triggered).
- **S-relorient fire condition:** `released ∧ release_orient ∧ ¬grounded ∧ (¬any_override ∨
  release_orient_with_keys)` → **nose snap** (v9; was guarded-velocity) + `orient_fired`, exactly once per
  release edge. (Pre-addendum the last term was a bare `¬any_override`.) The one-shot
  property is pinned by test: releasing the keys *later* fires nothing more — a deferred
  latch is explicitly **not** the design.
- **CQ2:** on release, `easeback := easeback_time`; while `easeback > 0`, mouse→aim is
  dead (`mouse_aim_live = false`); decrement after arming so the release tick itself is
  suspended. Loader wall: `0 < easeback_time ≤ 0.30 s`.
- **S7-hrz — ⚠ v9 INSTANT form (supersedes the profile at this, its only, consumer):**
  ```
  if horizon_recovery_rate > 0 ∧ ¬grounded ∧ released
                             ∧ ¬(any_override ∧ ¬release_orient_with_keys):
      mis = aim.up_misalignment(up)
      if |mis| > kFinishEps:  aim.roll_about_forward(mis)   // the WHOLE debt, one tick
  ```
  ~~then roll about forward with the D3 open-loop profile at `horizon_recovery_rate` toward
  `horizon_recovery_settle`~~ — **retired.** `horizon_recovery_rate` is now a **pure enable**:
  `rate = 0` still skips all of it structurally (the knob-off strict-superset proof), but a
  positive value no longer sets a speed. `horizon_recovery_settle` is not read on this path.
  `kFinishEps` makes an already-level release a **structural no-op** — bit-identical, no roll
  at all (it inherits the old `capture()`'s deadband). The with-keys clause preserves the
  knob-off legacy arm exactly (`release_orient_with_keys = false` = sealed-v6 D9: a keys-held
  release rights nothing).
  ~~Cancel arm (level, not edge) is now `freelook_held ∨ (any_override ∧
  ¬release_orient_with_keys)`~~ — **there is no cancel arm any more.** A roll that completes
  within the tick that starts it cannot be cancelled, so the whole in-progress-cancellation
  question the addendum's sub-ruling settled is now **moot by construction** rather than by
  rule.
- **Double-tap (S-orient):** two freelook taps within `orient_double_tap_s = 0.30 s`
  fire the same snap + cut on the second-tap *press*; `0` disables structurally.

## 4. Code — grounded in `reference/seads-feel/` (snapshot @ `e362df289` = the **v12** seal, 2026-07-30)

**Snapshot status: current at v12, verified symbol-by-symbol 2026-08-03.** This section
previously cited `51eb5b9e3` (the **v7** seal) and declared itself *"current"* — it was not, and
the earlier finding that the Feel/Principle levels were *"likely still sound"* **was wrong**:
verification showed the mechanism's central answer had changed, so §§1–3 were corrected too.

**The v9 symbols, read at source in `app/instructor_tick.h`:**

- **`orient_snap_dir(s, cp)`** — now `return s.orientation * dvec3{0,0,-1};`. **The whole
  guarded-velocity body is gone.** The `cp` parameter is retained but unread; the snapshot's own
  comment records dropping it as a *future* candidate and states that **no simplification rode
  along with v9** — a deliberate scope discipline worth copying.
- **The weld** — `if (in.freelook_held) st.aim.snap_forward_to_nose(st.curr.orientation);`,
  the `if` arm ahead of the release branch, so weld and release are mutually exclusive per tick.
- **The release branch** — `else if (fs.snap_to_nose || release_orient)` →
  `snap_forward_to_dir(orient_snap_dir(...))`, setting `res.orient_fired` when
  `release_orient`. Fire condition unchanged from v7:
  `cp.freelook_release_orient ∧ fs.released ∧ ¬grounded ∧ (¬any_ovr ∨
  cp.freelook_release_orient_with_keys)`.
- **The instant righting** — the `horizon_recovery_rate > 0 ∧ … ∧ fs.released` block,
  `roll_about_forward(mis)` on the whole `up_misalignment`, guarded by
  `HorizonRecovery::kFinishEps`.
- **`input::HorizonRecovery` / `st.recov`** — **permanently inert at v12.** Still constructed
  and still `reset()` — harmless, and removing the struct is another recorded future candidate.
  **Do not read its presence as evidence the profile still runs.**

**Unchanged from the v7 grounding and re-verified:** `input/aim_state.h`'s `struct Freelook`
(`override_used`, `freelook_prev`, `easeback`; `Freelook::step → Step{snap_to_nose,
mouse_aim_live, released}`), the CQ2 ease-back gate (`fs.mouse_aim_live && !st.grounded`), and
the `[freelook]` knobs with their loader walls.

**Still true, and still the constraint:** `render/camera` consumes `orient_fired` as the hard
camera-forward cut — **state→camera, never camera→control.**

*The v7-era section below is retained for lineage. Where it disagrees with the symbols above,
the symbols win.* It included `app/main.cpp` (the per-frame camera glue that earlier readings of
this mechanism had to fetch from the live tree).

- `input/aim_state.h` — `struct Freelook`: latches `override_used`, `freelook_prev`,
  `easeback`; `Freelook::step` returns `Step{snap_to_nose, mouse_aim_live, released}` —
  rules 1/2/3 and CQ2 live here, and **S-relorient does not touch this file** (it is
  app-level; the `test_freelook.cpp` pins on `Freelook::step` still pin the legacy arm).
- `app/instructor_tick.h` — the composition point: the rule-3 release handling (which
  already performs the guarded-velocity snap via `snap_forward_to_dir`), the S7-hrz block
  (`st.recov`, `input::HorizonRecovery`, `recov.capture` on `fs.released`), and the
  S-orient verb (`in.orient_cmd` → snap + `res.orient_fired`, suppressed grounded or
  under `any_ovr`).
- `render/camera` — consumes `orient_fired` as the hard camera-forward cut (downstream
  only; the standing camera-independence constraint in `docs/DECISIONS.md` holds — the
  cut is state→camera, never camera→control). **This is the cut only.** The camera's
  *resting* behaviour between cuts is `ease_chase_forward` against a rest target anchored
  by `[camera] lead = 1.0` — which is the unresolved standing-oblique question, not part of
  this mechanism.
- `config/controller.toml` `[freelook]` + `config/load_controller.cpp` — `easeback_time`,
  `orient_double_tap_s`, `horizon_recovery_rate/settle` knobs and their loader walls.

**Symbols added by the ADDENDUM** (in the snapshot as of the v7 seal): `orient_snap_dir` —
the hoisted guarded-velocity helper in `app/instructor_tick.h`, called from *both* the
release and double-tap sites so "one verb, two triggers" is a property of the code rather
than of two paragraphs of comment; `freelook_release_orient_with_keys` in
`control/params.h`, read via `optional_bool` in `config/load_controller.cpp`, `= true` in
`[freelook]`.

**Status: the mechanism described by §§1–3 is SEALED as `flight-kernel-v12-2026-07-30`
(`e362df289`).** Its shape was last changed by **v9 `flight-kernel-v9-2026-07-29`
(`29787debc`, S-nosesnap `b4c0751`), which is flown-approved** — the fourth attempt at the
camera arc and the one that closed it. v10/v11/v12 did not touch this path.

*v7-era status line, retained:* **SEALED as `flight-kernel-v7-2026-07-29` (`51eb5b9e3`),
Chad-approved on the stick** — "now it is precisely perfect… it's the right set up for my
camera now." The
addendum (`35e31695f` + `13631ba92`) and S-keychase (`5ea20d8c7`) were flown together as
one bundle; gate 388/388, tag + branch pushed. The prior S-relorient landing (`274432f35` +
`468f2b352`, gate 380/380) was separately approved and is sealed in v6.

## Lineage

Freelook-with-held-aim descends from EvC2026's free-look over `aimTargetDir`, but the
rules-1/2/3 contract, CQ2, S7-hrz, and both orient verbs are seads-feel C++ mechanisms
with no Luau counterpart. Ground all readings here in the seads-feel kernel.

## Rulings (both open questions closed 2026-07-28)

1. **The D9 exception is retired** — the finger leaving Space is the whole trigger. A
   release with override keys held fires the **full** verb, identical to a clean release
   (not a camera-only variant), and it **snaps at release while keeping the flown-in chase
   lag afterward** — not a permanent behind-lock, which would delete `[camera] lag_gain`
   and is a separate fly. Behind `freelook_release_orient_with_keys` (default `false` =
   today's kernel exactly; `true` shipped). Sub-ruling with reach past the release edge: an
   override pressed *later* no longer aborts an in-progress horizon roll — required by
   "even if still turning", and safe because the D3 roll is open-loop and invisible to
   `control::step`. **Explicitly not** the deferred-fire latch floated when this was an open
   question; a one-shot test leg now forbids that shape.
2. **The double-tap stays** — as the orient-without-releasing-freelook verb, the only way to
   orient *while staying in* freelook. Chad's condition is that it be **truly** redundant:
   a genuine backup rather than the only escape from a stuck state. It gets the same
   relaxation as the release site, so the two triggers cannot diverge again, and the shared
   guarded-velocity snap is hoisted to one helper so "one verb, two triggers" is a property
   of the code rather than of two paragraphs of comment.

**Trade, on the record:** a release-while-holding-keys now snaps the aim to the flight path
mid-maneuver — the same trade CARD 1 already flags for the double-tap. Chad ruled for it; at
that instant he is flying on the keys, which keep the turn. Walk-back is one line.

## Known doc gap — live tree, not closable from here

`SPEC.md` §9.5 in `D:\flight_sim2\seads-feel` still states the **pre**-relorient release rule
(`aim := nose` only if an override was used), and S-relorient has **no SPEC §0 supersession
entry at all**. That file is read-only to this repo, so it must be corrected in the worktree
by the agent holding the diff. Flagged 2026-07-28.
