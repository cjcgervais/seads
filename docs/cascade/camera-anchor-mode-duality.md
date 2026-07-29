# The camera anchor and the two flying modes — what the eye is fixed to

Why the camera sometimes sits behind your flight path and sometimes sits behind your aim,
which of those you get in each input mode, and why the keys deliberately never take your
aim. Sealed as part of `flight-kernel-v7-2026-07-29`.

---

## 1. Feel — pilot voice

There are two modes, and the difference is **which one of your aim and your camera is free**:

| mode | your aim | your camera |
|---|---|---|
| **freelook held** (Space) | **carried** — welded to the airframe, it goes where the nose goes | **free** — the mouse swings your head around |
| **mouse-aim** (no freelook) | **free** — the mouse owns it, independent of the plane | **fixed** — it sits on the aim |

Chad's own statement of it, 2026-07-29, and the thing everything below serves:

> "Having freelook pressed is the mode that carries my aim and cam is free. In mouse aim,
> mouse is free, camera fixed — and it's the only way to access forward-looking oblique
> deflection shots in a merge while holding hard on a key and maintaining aim freedom at the
> same time. If I want to harness the aim into my nose then I press freelook."

The **control-surface keys sit deliberately outside that duality.** They move the airframe
*without* taking your aim. That is the whole point: in a merge you can haul on a key and
still steer the reticle independently, pulling lead across the circle for a deflection shot.
If the keys carried your aim with them, you'd lose that — the two modes would collapse into
one and there'd be no way to fly hard and aim free at the same time.

**The parked aim is a plan, not a leftover.** When you hard-press a maneuver without
freelook, leaving the aim where it is *is* the point — Chad's refinement, 2026-07-29:

> "I can plan my aim for when I release the key override… but also, and most importantly,
> the fact that I am looking through the nose line of the plane from behind, or see its
> angle from behind its velocity — and it may be obliquely aligned at an enemy, but I see
> where I am shooting and plan my aim when hard-pressing maneuver without freelook."

So the key-flown camera is doing **two** jobs, and neither is comfort:

1. **It shows you your gun line.** Sitting behind the *velocity* is what lets you see the
   angle between where the aircraft is going and where its nose is pointing — and the nose
   is where the guns fire. You can be flying one way and obliquely lined up on an enemy
   another way, and from behind the flight path you can *see* that offset and read your
   shot. Behind the parked aim you cannot; you're looking at your own aircraft side-on.
2. **It leaves your aim free to be pre-placed.** The reticle stays where you put it because
   you are setting up where you want to be aiming the moment the keys come up.

**What went wrong before v7** was never that the aim was parked — that is deliberate. It was
that the *camera* was anchored to the parked aim, which spends your eye on your plan instead
of on your shot. **S-keychase separates the two:** while a key is down and you're not in
freelook, the eye goes behind the flight path (the shooting reference) while the aim stays
wherever you planned it.

**Which is why the handback is a payoff, not a cost.** When you release the keys the camera
swings to the parked aim — that is the camera going *to the place you decided to look*. It
reads as intent being delivered rather than as an artifact, which is why a ~16° swing was
judged "precisely perfect" rather than intrusive.

**The oblique view is not the bug.** A big oblique in *mouse-aim* is the deflection shot —
the camera showing you where you're pointing instead of where you're going. That is a
capability, and v7 does not touch it. The bug was only ever the oblique you get when you
aren't aiming at all.

**Chad's forward read**, worth checking against later sessions: *"more can now be done in
mouse aim mode, and I suspect then my freelook will be only for situational awareness and
when I want to see / need to see obliquely — as opposed to not just being oblique."* v7 is
expected to move freelook from a **flying** verb to a **looking** verb.

## 2. Principle — instructor voice

The chase camera has a **rest target** and a **catch law**, and they are separate concerns.
The v7 change touches only the target.

- **The rest target** is normally the aim, set by `[camera] lead = 1.0` ("the rest target IS
  the aim"). This is correct in mouse-aim, because the aim is where the pilot is going.
- **The catch law** is `rate = lag_base + lag_gain·deflection` — a deliberately loose follow
  that lets the reticle float off-centre while the mouse moves and closes the gap when it
  stops. This is tuned feel Chad signed off over many rungs and **must not be moved to solve
  targeting problems.**

Under keyboard flying that pairing breaks, because the premise fails. The keys change the
aircraft's path while the aim stays parked — **deliberately** parked, as the pilot's plan for
the release — so `lead = 1.0` spends the camera on the *plan* at exactly the moment the pilot
needs the *shot*. The camera is not lagging and the target is not stale; it is a legitimate
target that answers the wrong question while the keys are down. No amount of extra catch rate
fixes that: it only arrives at the wrong place sooner, and speeds up the mouse-aim float as
collateral.

**What the pilot needs while the keys are flying is the nose-versus-velocity angle** — the
gun line against the flight path — which is visible only from behind the velocity. That is
the information the anchor swap buys, and it is why this is a gunnery mechanism rather than
a comfort one.

**S-keychase therefore changes only *which* target the (untouched) law chases**, and only
while keys are flying and freelook is not held: the target becomes the flight path, caught
at a constant fast rate. With no key held the selector returns the pilot's own dials
verbatim, so mouse-aim flying is **bit-identical by construction, not by assertion**.

Two consequences that were flown and accepted rather than designed around:

1. **The handback is a hard switch — and that is right.** When the last key comes up the
   target jumps back to the parked aim and the camera swings to it under the normal dials.
   There is no pop (the camera direction itself is carried and eased) but there is a swing,
   sized by however far the aim was parked. Measured ≈16° over ≈0.7 s; flown and judged fine.
   A blend was held in reserve and proved unnecessary — **because the swing is the pilot's
   own pre-placed aim being delivered**, not an artifact to be smoothed away. Softening it
   would blur the moment the plan arrives.
2. **Freelook still owns the camera outright.** The keys-flying condition excludes freelook
   deliberately — in freelook the orbit is the camera, and an anchor swap underneath it
   would fight the pilot's head.

## 3. Math — engineer voice

- **Rest target:** with `v = v̂` (velocity), `a = âim`, `defl = ∠(v, a)`:
  `target = rotate(v, lead·defl, axis = v × a)`. At `lead = 1` the target is the aim; at
  `lead = 0` it is directly behind the flight path. Degenerate at `defl ≈ 0` or `≈ π`
  (axis vanishes) → `target = v`.
- **Catch:** `rate = lag_base + lag_gain·defl`; the camera rotates toward the target by
  `min(gap, rate·dt)` — no overshoot. Shipped: `lead = 1.0`, `lag_base = 0.2 /s`,
  `lag_gain = 0.7 /(s·rad)`.
- **S-keychase selector** (`render::chase_anchor`), a pure function of the mode:
  `keys_flying ∧ key_rate > 0 → (lead, lag_base, lag_gain) := (0, key_rate, 0)`, else the
  caller's dials **verbatim**. `keys_flying = any_override ∧ ¬freelook_held`.
  Shipped `key_anchor_rate = 6.0 /s` (≈0.17 s). `key_rate ≤ 0` is the structural off-switch
  and the one-line walk-back to v6.
- **Handback magnitude:** at the switch the target moves by `defl`; the swing then closes at
  `0.2 + 0.7·defl`. At the measured keyboard deflection (≈0.285 rad) that is ≈0.40 /s over a
  0.285 rad gap ≈ **0.7 s**. It scales with parked-aim distance, so a longer key-turn grows it.

**Measured** (`seads_harness comfort`, both arms self-evidencing):

| leg | standing oblique | reading |
|---|---|---|
| `turnsteady_keys_off` (v6 law) | 16.34° | the defect |
| `turnsteady_keys_on` / isolated law | → ~0° | fixed |
| `turnsteady` (mouse pilot) | 95.82°, `converged 0` | **not a defect — see below** |

⚠ **The `converged` predicate is mode-blind.** `converged = oblique_deg < 10 ∧ up_debt_deg
< 10` assumes the camera *ought* to end behind the flight path. True for keyboard flying;
**false for mouse-aim**, where a large standing oblique is the deflection capability of §1.
`turnsteady converged 0` is the instrument measuring the wrong goal for that mode. It has
already been misread once as a defect — it motivated a change it could not affect — so read
it as geometry, not as a verdict.

## 4. Code — grounded in `reference/seads-feel/` (snapshot @ `51eb5b9e3` = the v7 seal)

Snapshot note: `render/camera.h` and `test/harness/comfort.h` are taken from `7650dc6d0`
(post-seal, **comments/docs only** — every non-comment line verified byte-identical to the
seal), because the sealed banners carried the superseded comfort framing corrected in §1.

- `render/camera.h` — `struct ChaseAnchor` + `chase_anchor(keys_flying, lead, lag_base,
  lag_gain, key_rate)`: the pure selector. **PURE on purpose** — both the app and the
  comfort instrument select through this one function, so the shipped law and the measured
  law cannot fork (the same route-the-live-path-through-the-tested-function discipline as
  `app::tick`). Also here: `ease_chase_forward` (the untouched catch law, with the both-ends
  antiparallel NaN guard) and `aim_chase_camera`.
- `app/main.cpp` — the call site: builds `keys_flying` from `live.override_mask` and
  `live.freelook_held`, calls `chase_anchor`, feeds the result to `ease_chase_forward`.
  Also the freelook-orbit decay and the `orient_fired` hard cut of `cam_fwd`.
- `test/harness/instructor.h` — `MiniCamera::advance(..., keys_flying = false)`, routed
  through the same `chase_anchor`; defaulted false so every pre-existing caller is unchanged.
- `test/harness/comfort.h` — `comfort_turnsteady` (mouse pilot, via `turn_reaim`) vs
  `comfort_turnsteady_keys` (parks the aim, flies on a held roll+pitch override, runs
  **both arms**). The distinction between these two scenarios is the thing that was missed
  when 95.82° was first attributed to the keyboard case.
- `control/params.h` — `cam_lead`, `cam_lag_base`, `cam_lag_gain`, `cam_key_anchor_rate`
  (optional-with-default-0).
- `config/controller.toml` `[camera]` — the shipped dials and the walk-back note.

**Standing constraint check:** every path here is **state→camera**. `keys_flying` is input
state read *into* the camera; no camera-derived quantity (anchor choice, orbit, fov, eased
forward) feeds `pitch`/`roll`/`yaw`/throttle. The camera-independence constraint in
`docs/DECISIONS.md` holds, which is why the anchor swap was a pure feel question.

## Lineage

No EvC2026 counterpart. The Luau testbed has a single camera mode; the freelook/mouse-aim
duality, the decoupled lagging chase-forward (S7-cam), the carried aim-up (S7-cam3) and the
anchor swap are all seads-feel C++ mechanisms. Ground all readings here.

## Related

- `docs/cascade/freelook-orient-verbs.md` — what happens at the freelook *release* edge (the
  orient verb, the horizon roll). S-keychase governs the *standing* state between releases;
  the two were repeatedly confused during the session that produced both.
- `docs/DECISIONS.md` — the 2026-07-29 ruling that the keys must not carry the aim, and the
  camera-independence standing constraint.

## Open (not blocking)

- **Mode-qualify or rename `converged`** so the mouse-aim leg stops reading as a comfort
  failure. Lives in the live tree (`test/harness/comfort.h`), not editable from this repo.
- **Chad's prediction** that freelook becomes a looking verb rather than a flying verb —
  checkable against freelook-usage stats in the next Golden Felt Flight.
