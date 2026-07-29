# Freelook and the ORIENT verb — letting go and getting put back together

What happens to your aim, your camera, and your horizon when you hold freelook, maneuver
under it, and release it. Ends at the **v7 seal** (2026-07-29, Chad-approved: "now it is
precisely perfect"): every freelook release fires the orient verb — instant chase-behind,
every time, **including with control-surface keys still held** — and while you fly on those
keys the camera sits behind your flight path rather than behind a parked aim.

---

## 1. Feel — pilot voice

Hold freelook and the world unhooks from your mouse: you're looking around while the
plane keeps flying the line you last gave it. The mouse moves your *eyes*, not the nose.

Letting go is the moment that matters. The ruling that finished this mechanism (Chad,
2026-07-28) is that release should put you back together **automatically**: release
mid-turn and you're instantly seated behind your own flight path — camera cut behind the
velocity, aim on it, horizon rolling itself level. The fly-card language: "release space
mid-turn → instant chase-behind every time." Before this, that composed "put me back
together" move was the double-tap orient (comfort mode, Chad 2026-07-17); now the release
itself is the verb, and the double-tap is redundant.

One deliberate exception, because it's what a pilot would want:
- **Falling, not flying** (sub-stall, tail-slide): you're seated behind the *nose*, not
  the velocity — the velocity points somewhere useless, straight down or backwards.

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
was spending the **eye** on that plan at the moment he needed to see his **shot**. That took
a second mechanism, **S-keychase**: while keys fly and freelook is not held, the rest target
becomes the flight path, where the nose-versus-velocity angle — the gun line — is legible.
The two together are what he flew as "precisely perfect."

**This entry governs the release *edge*; the *standing* state between releases belongs to
`docs/cascade/camera-anchor-mode-duality.md`** — including why the control-surface keys must
never carry the aim. The two were repeatedly confused during the session that produced both,
so read them together.

## 2. Principle — instructor voice

Five mechanisms compose, in the order they fire on a release tick:

1. **Freelook rules 1–3** (the aim-hold contract): while held, the aim is frozen and the
   mouse feeds the orbit camera (rule 1). If an override key fires during the hold, the
   aim snaps to the nose once (rule 2) — the stick took over, the stale aim is a lie. On
   release after an override was used, the aim snaps to *guarded velocity* (rule 3) — the
   maneuver took you somewhere; resume from where you're actually going.
2. **S-relorient** (2026-07-28): a mouse-only release — which under rule 3 used to move
   nothing — now fires the same guarded-velocity snap, plus the camera cut. Behind a knob
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
3. **The guarded-velocity snap** (shared by rule 3, the double-tap, and S-relorient): aim
   goes to the velocity direction *only if* the aircraft is genuinely flying forward —
   above ballistic speed and with velocity ahead of the nose — else to the nose.
4. **S7-hrz horizon recovery**: on the release edge, the aim frame's accumulated
   up-misalignment (holonomy debt from the maneuver) is captured once and rolled away
   open-loop — a gauge move, invisible to the control law. **Ordering is load-bearing:**
   the snap fires *before* the capture, so the righting is measured about the new forward
   on the same tick. (The plan-stage audit caught this; done the other way, the roll
   retires a stale debt about the wrong axis. Release-orient therefore rights the horizon
   slightly better than the double-tap did, whose debt retired one release later.)
5. **CQ2 ease-back**: mouse→aim input stays suspended for a beat after release (the
   release tick included), so the freshly snapped aim can't be smeared by residual mouse
   motion.

The camera cut (`orient_fired`) is the one thing S-relorient adds where rule 3 already
snapped: the caller hard-seats camera-forward to the aim. Fire-once discipline is pinned
by test — the red-team's one real find was a re-fire-every-tick mutant surviving the
suite; a one-shot pin now kills it.

## 3. Math — engineer voice

- **Guarded velocity:** with `v = velocity`, `spd = |v|`, `nose = orientation · (0,0,−1)`:
  `dir = v/spd` if `spd > v_ballistic ∧ (v/spd)·nose > 0`, else `dir = nose`. Ballistic
  floor is hysteretic: `v_ballistic = 30 m/s` entry, `v_ballistic_exit = 40 m/s`.
- **Release edge:** `released = freelook_prev ∧ ¬freelook` (one tick, edge-triggered).
- **S-relorient fire condition:** `released ∧ release_orient ∧ ¬grounded ∧ (¬any_override ∨
  release_orient_with_keys)` → guarded-velocity snap + `orient_fired`, exactly once per
  release edge. (Pre-addendum the last term was a bare `¬any_override`.) The one-shot
  property is pinned by test: releasing the keys *later* fires nothing more — a deferred
  latch is explicitly **not** the design.
- **CQ2:** on release, `easeback := easeback_time`; while `easeback > 0`, mouse→aim is
  dead (`mouse_aim_live = false`); decrement after arming so the release tick itself is
  suspended. Loader wall: `0 < easeback_time ≤ 0.30 s`.
- **S7-hrz:** capture `θ_up` = aim frame's up-misalignment vs local up on the release
  edge (post-snap), then roll about forward with the D3 open-loop profile at
  `horizon_recovery_rate` toward `horizon_recovery_settle`. Cancel arm (level, not edge) is
  now `freelook_held ∨ (any_override ∧ ¬release_orient_with_keys)` — a freelook re-press
  always cancels; an override key only cancels with the addendum knob off. `rate = 0` skips
  all of it structurally.
- **Double-tap (S-orient):** two freelook taps within `orient_double_tap_s = 0.30 s`
  fire the same snap + cut on the second-tap *press*; `0` disables structurally.

## 4. Code — grounded in `reference/seads-feel/` (snapshot @ `51eb5b9e3` = the v7 seal, 2026-07-29)

Snapshot status: **current**, and it now includes `app/main.cpp` (the per-frame camera glue
that earlier readings of this mechanism had to fetch from the live tree).

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

**Status: SEALED as `flight-kernel-v7-2026-07-29` (`51eb5b9e3`), Chad-approved on the
stick** — "now it is precisely perfect… it's the right set up for my camera now." The
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
