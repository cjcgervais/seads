# The camera law and the two flying modes — what the eye is bound to

What the camera is anchored to, which of your aim and your camera is free in each input
mode, and why the control-surface keys reach your trajectory but never your view. Sealed as
`flight-kernel-v8-2026-07-29` — ⚠ **sealed on the gate, not yet flown** (see Status).

---

## 1. Feel — pilot voice

**The whole camera law, in Chad's own three lines** (2026-07-29 — this exact wording is also
in `SPEC.md` §9.2 so the two cannot fork):

> **freelook** — aim carried, camera free.
> **mouse-aim** — aim free, camera bound to the aim.
> **keys** — affect neither.

Read it as: whichever mode you're in, exactly one of your aim and your camera is yours to
move, and the control-surface keys are outside the pair entirely. They change where the
aircraft *goes*. They never change where you're *looking*.

**There is exactly one camera automation in the whole kernel:** releasing freelook snaps you
to chase, once. After that the camera lags your aim under mouse authority alone, forever, and
nothing else moves it on its own.

Chad's statement of why the keys must stay out of it:

> "When I am flying in mouse aim the snap back to chase is occurring with every hard key
> press. I usually fly with a combination of mouse aim with hard key inputs to maximize
> control for the fight… if I input some aileron to cut into their path sooner, I get a
> disorienting snap to the chase cam which throws off my aim and feels unnatural."
>
> "Only the precedence of the freelook push shall do that."

And the mode duality he'd stated the day before, which this simplifies rather than replaces:

> "Having freelook pressed is the mode that carries my aim and cam is free. In mouse aim,
> mouse is free, camera fixed… If I want to harness the aim into my nose then I press
> freelook."

**Why the keys must not carry the aim either.** They move the airframe *without* taking your
aim, and that is the point: in a merge you can haul on a key and still steer the reticle
independently, pulling lead across the circle for a deflection shot. If the keys carried the
aim, the two modes would collapse into one and there'd be no way to fly hard and aim free at
the same time. **Ruled against explicitly, 2026-07-29** — see `docs/DECISIONS.md`.

**The consequence you accepted:** fly on the keys without touching the mouse and the aircraft
turns away from your parked aim, so the camera ends up looking obliquely at your own plane —
about **16°** in a sustained key turn. That is the camera faithfully showing where you are
*pointing*. The cure is to move the mouse. ⚠ This is the thing fly card 3 exists to confirm,
and it is not yet confirmed.

---

## 2. Principle — instructor voice

The chase camera has a **rest target** and a **catch law**, and keeping them separate is what
makes this tractable.

- **The rest target is the aim. Always.** `[camera] lead = 1.0` — "the rest target IS the
  aim." There is no second anchor and no mode in which the target is anything else.
- **The catch law** is `rate = lag_base + lag_gain·deflection` — a deliberately loose follow
  that lets the reticle float off-centre while the mouse moves and closes the gap when it
  stops. Tuned feel Chad signed off over many rungs. **It must never be moved to solve a
  targeting problem**; a wrong target is not fixed by arriving sooner.

**Why a second, key-driven anchor was wrong.** The retired S-keychase re-anchored the rest
target to the flight path whenever a key was held. Two things followed, and the second is the
one that killed it:

1. The anchor *swap* is itself camera motion. At the keypress edge the target jumps and the
   camera chases it — that edge is the "disorienting snap."
2. It **contests the mouse.** In mouse-aim the pilot is steering the camera by steering the
   aim; a mechanism that re-points the camera on a *key* takes the view out of his hands
   mid-fight. The mode duality says the mouse owns the camera in mouse-aim, and a key-driven
   anchor is a second claimant.

So the rule is structural, not a tuning preference: **the camera path takes no key state as
input at all.** Not a knob at zero — absent. `app/main.cpp`'s `ease_chase_forward` call is
required to stay branch-free on key state, and a conditional there is a ruling violation on
sight (recorded as a review bar at the call site and in `SPEC.md` §0, because no test can
catch it — see "The evasion that no test can close").

**Freelook is the one exception, and it is a discrete player action.** Releasing freelook
fires the orient verb: aim := guarded velocity, plus a hard camera cut behind it. That is a
snap the pilot commanded by lifting his finger, not an automation reacting to his stick.

---

## 3. Math — engineer voice

One law, no selector:

- **Rest target:** with `v = v̂`, `a = âim`, `defl = ∠(v, a)`:
  `target = rotate(v, lead·defl, axis = v × a)`. At the shipped `lead = 1.0` the target **is**
  the aim. Degenerate at `defl ≈ 0` or `≈ π` (axis vanishes) → `target = v`.
- **Catch:** `rate = lag_base + lag_gain·defl`; the camera rotates toward the target by
  `min(gap, rate·dt)` — no overshoot. Shipped: `lead = 1.0`, `lag_base = 0.2 /s`,
  `lag_gain = 0.7 /(s·rad)`.
- **Key state appears nowhere in the above.** That is the mechanism.

**Measured** (`seads_harness comfort`), v7 → v8:

| leg | v7 | v8 |
|---|---|---|
| `mouseaim_keys` lag-behind-aim, keys down | 67.781° | **10.492°** |
| `mouseaim_keys` max per-tick step at the keypress edge | 3.222° | **0.378°** |
| `turnsteady_keys` standing oblique (keys only, no mouse) | 0.000° | **16.341°** — accepted, ruled |
| gate | 388/388 | 387/387 (−2 retired cases, +1 new leg) |

⚠ **Do not cite `max_step` as the headline.** It is bounded by construction: v7's swap was
*eased* at `key_anchor_rate·dt = 6.0/120 ≈ 2.9°/tick`, so the peak saturates near 2.9° however
bad the snap feels. The real evidence is the **before-vs-during divergence** (0.000° → 67.781°
on v7) and the sustained rate until the gap closes.

**The v8 residual 10.492° is not an anchor effect.** It is ordinary chase lag responding to an
aim that swings faster once the keys bite — Chad's "my lag camera will be able to predictably
catch up." Methodology note from the red-team, worth doing if a second scenario is ever
wanted: `turn_reaim` slaves the aim to the heading, so a **world-fixed-aim** variant would
isolate key→camera coupling more cleanly.

---

## 4. Code — ⚠ snapshot is STALE for this mechanism

⚠ **`reference/seads-feel/` is at `51eb5b9e3` (the v7 seal) and still contains
`chase_anchor`.** It has deliberately **not** been re-snapshotted: v8 is sealed but unflown,
and its walk-back is a revert rather than a dial, so a snapshot now could enshrine code that
gets reverted. Ground v8 readings in `D:\flight_sim2\seads-feel` (read-only) until Chad flies
the cards.

In the sealed v8 tree:

- `render/camera.h` — `ease_chase_forward` (the one camera law) and `aim_chase_camera`.
  `struct ChaseAnchor` and `chase_anchor()` are **deleted**; a dated RETIRED record sits above
  the deletion point explaining what they were and why they went.
- `app/main.cpp` — calls `ease_chase_forward` with the `[camera]` dials directly (the sealed-v6
  line). **This call site carries a review bar: it stays branch-free on key state.**
- `test/harness/instructor.h` — `MiniCamera::advance(s, aim_fwd, aim_up, cp, dt)`. The
  `keys_flying` parameter is gone; the seam **takes no override state, by ruling**, and says so
  in a banner.
- `test/harness/comfort.h` — `comfort_mouseaim_keys` (the permanent regression instrument, and
  the first scenario to model mouse *and* keys at once) and `comfort_turnsteady_keys`
  (single-arm now; its ~16° is the **expected, ruled** number, not a defect).
- `control/params.h`, `config/load_controller.cpp`, `config/controller.toml` — `cam_lead`,
  `cam_lag_base`, `cam_lag_gain`. `cam_key_anchor_rate` / `[camera] key_anchor_rate` removed.

**Standing constraint check:** the camera path is **state→camera** throughout, and now reads
strictly less state than before. Nothing camera-derived feeds `pitch`/`roll`/`yaw`/throttle.
The camera-independence constraint in `docs/DECISIONS.md` holds.

---

## Retired: S-keychase (v7) — a cautionary record, kept deliberately

**What it was:** while any override key was held and freelook was not, the camera's rest
target swapped from the aim to the flight path, caught at a constant `key_anchor_rate = 6.0 /s`.
Sealed in v7 and flown-approved as "precisely perfect." Removed entire in v8.

**Two fixes for one defect; the second was the bug.** Chad's original oblique complaint was
caused by the **D9 exception** — releasing freelook with keys held fired *no* snap at all.
Retiring D9 (the S-relorient addendum) fixed it, and that fix stands. S-keychase was then
stacked on top to also flatten the *standing* state — an over-correction, and it was the second
mechanism that fought his mouse.

**Why a flown approval didn't hold.** S-keychase was approved against
`comfort_turnsteady_keys`, which **parks the aim and flies on keys alone.** Chad flies mouse-aim
**and** keys simultaneously, and no scenario modeled that — so the defect was structurally
unmeasurable, and his approval was genuine but scoped to a case he doesn't fly. **This is the
third camera mechanism validated against a case he does not fly** (cf. S-aimclamp, S-retclamp).

> **The generalizable rule: a feel mechanism's instrument must model both hands at once.**

Two corollaries of S-keychase died with it and must not be revived out of context: its
reclassification as a **gunnery** mechanism, and the ruling that its **handback must never be
blended**. Both were true *of that mechanism*; both are moot now that there is no handback.

⚠ **Open, and honestly unresolved:** Chad's gunnery reasoning — "I am looking through the nose
line of the plane from behind, or see its angle from behind its velocity… I see where I am
shooting" — was given while describing the behind-velocity view that v8 removes. Whether the
aim-bound camera serves that same shooting need is exactly what **fly card 3** tests. It is not
yet answered, and this doc should not claim it is.

## The evasion that no test can close

The v8 red-team built a working evasion: re-adding key→camera coupling as a **defaulted
parameter** on `MiniCamera::advance`, wired only from `app/main.cpp`, reproduces the retired
anchor and **passes the entire suite untouched** — because no ctest runs `seads.exe`. There is
no unit test that can close this. The countermeasure is therefore a **review bar**, recorded at
the call site and in `SPEC.md` §0 rather than as prose in a handoff: `main.cpp`'s
`ease_chase_forward` call stays branch-free on key state, and a conditional there is a ruling
violation on sight. Anyone reviewing camera changes should read that call site first.

## Status

**Sealed** as `flight-kernel-v8-2026-07-29` (`ae7ae8f23`), gate 387/387, zero moved goldens,
branch + tag + `sandbox/s-keychase-retired` pushed. **NOT YET FLOWN** — `docs/v8_fly_cards.md`
in the live tree. Not grafted to seads-recon. **Walk-back is a revert, not a dial:**
`sandbox/s-keychase-retired`, or the `flight-kernel-v7-2026-07-29` tag.

## Lineage

No EvC2026 counterpart — the Luau testbed has a single camera mode. The freelook/mouse-aim
duality, the decoupled lagging chase-forward (S7-cam), and the carried aim-up (S7-cam3) are all
seads-feel C++ mechanisms.

## Related

- `docs/cascade/freelook-orient-verbs.md` — the freelook *release* edge: the one camera
  automation this entry keeps referring to.
- `docs/DECISIONS.md` — the 2026-07-29 rulings (keys never carry the aim; keys never move the
  camera) and the camera-independence standing constraint.
