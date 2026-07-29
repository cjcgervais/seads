# Freelook and the ORIENT verb — letting go and getting put back together

What happens to your aim, your camera, and your horizon when you hold freelook, maneuver
under it, and release it. Ends with S-relorient (2026-07-28, Chad-approved): every
freelook release now fires the orient verb — instant chase-behind, every time.

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

Two deliberate exceptions, both because they're what a pilot would want:
- **Falling, not flying** (sub-stall, tail-slide): you're seated behind the *nose*, not
  the velocity — the velocity points somewhere useless, straight down or backwards.
- **Still maneuvering** (an override key held at the moment of release): nothing snaps.
  You're clearly not done; the next clean release orients.

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
- **S-relorient fire condition:** `released ∧ release_orient ∧ ¬grounded ∧ ¬any_override`
  → guarded-velocity snap + `orient_fired`, exactly once per release edge.
- **CQ2:** on release, `easeback := easeback_time`; while `easeback > 0`, mouse→aim is
  dead (`mouse_aim_live = false`); decrement after arming so the release tick itself is
  suspended. Loader wall: `0 < easeback_time ≤ 0.30 s`.
- **S7-hrz:** capture `θ_up` = aim frame's up-misalignment vs local up on the release
  edge (post-snap), then roll about forward with the D3 open-loop profile at
  `horizon_recovery_rate` toward `horizon_recovery_settle`; canceled (level, not edge) by
  re-press or any override. `rate = 0` skips all of it structurally.
- **Double-tap (S-orient):** two freelook taps within `orient_double_tap_s = 0.30 s`
  fire the same snap + cut on the second-tap *press*; `0` disables structurally.

## 4. Code — grounded in `reference/seads-feel/` (snapshot @ `89447aba5`)

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
  cut is state→camera, never camera→control).
- `config/controller.toml` `[freelook]` + `config/load_controller.cpp` — `easeback_time`,
  `orient_double_tap_s`, `horizon_recovery_rate/settle` knobs and their loader walls.

**Snapshot drift flag — S-relorient is NOT in this snapshot.** The snapshot predates it;
it shows the double-tap-era code (rules 1–3, CQ2, S7-hrz, S-orient are all present). The
mechanism itself lives on the live branch: `feel/kernel-v5` commits `274432f35`
(mechanism, gate 380/380) and `468f2b352` (red-team folds: one-shot pin + honest
banners), 2026-07-28, Chad-approved. The `release_orient` knob and its
optional-with-default-false loader read exist only there until re-snapshot (see the
watch-item in `docs/DECISIONS.md`).

## Lineage

Freelook-with-held-aim descends from EvC2026's free-look over `aimTargetDir`, but the
rules-1/2/3 contract, CQ2, S7-hrz, and both orient verbs are seads-feel C++ mechanisms
with no Luau counterpart. Ground all readings here in the seads-feel kernel.

## Open question (for Chad)

The double-tap orient still works and is now redundant with release-orient. Retire it, or
keep it as the orient-without-releasing-freelook verb? (It is the only way to orient
*while staying in* freelook.) — flagged 2026-07-28, awaiting a ruling.
