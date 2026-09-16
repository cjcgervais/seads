# R1c — THE ATTACK POSITION. Build spec.

Governed by `docs/SUDBURIAN_LADDER.md`. Baseline `f15a6718e` on `sandbox/gi4-ride`.
R1a is SIGNED; this is follow-on work on a signed rung, judged on its own merits.

## Chad's drive report, 2026-08-17 — verbatim, because the fix is in his words

> "the sudburian when just sliding weight fowrawrds is basically laying prone and
> going into the dash / cowl a bit too far forward head actually through the cowl
> and over the shroud, but its not that bad, the arms look a little funny with this
> mechanic, but it makes the sudburian look committed to the lean forward (could be
> just a little less) ... Actually how far fowards he leans is realistic I have
> ridden and deen someone leaning over their shroud, **its just that without the
> standing too it makes the sudburians head travel through the cowl which is the
> unnatural part** and the arms look funny in that position."

**Read that last clause twice. It is the entire specification.**

- **The lean ANGLE is right and must NOT be reduced to fix this.** Chad has ridden,
  and has watched riders lean out over the shroud. The hinge magnitude is
  *validated by the only person who can validate it*. At most trim it "just a
  little" — and only after the real fix, because the real fix may remove the
  wish entirely.
- **What is wrong is that he leans forward WITHOUT RISING.** A rider going to the
  attack position comes **up off the seat** — hips rise, knees bend, chest goes out
  over the bars. Our rider hinges about a **seated** hip, so the same forward angle
  drives his head *down* into the cowl instead of *up and over* it.
- The verifier predicted this contact exactly (helmet vs `indy650_cowl` from
  fwd 0.25, chin curtain 110 mm below the cowl top line, "visibility HIGH").
  **Chad independently found it on the first drive. The prediction and the drive
  agree, which means the model is right and only the posture is wrong.**
- **The arms "look funny" is almost certainly the same defect.** Hinging a seated
  torso drops the shoulders toward and below the bar line, so the arms rake up and
  back to reach the grips. Raising the pelvis puts the shoulders *above* the bars
  where a rider's actually are. Do not treat the arms as a separate problem until
  you have measured them after the rise.

## The job

**Give the forward lean a coupled RISE, and solve BOTH CG axes so it stays honest.**

Today `pose_and_solve_lean` solves one unknown (the hip hinge θ, plus a residual
root translation `d`) against one constraint: rider CG z-displacement must equal
`lean_fwd_m`. That is R1a's hard-won honesty and it must not be given up.

Add a second degree of freedom — **pelvis rise** (with the knees folding to absorb
it against the board-pinned boots) — and solve **two** constraints simultaneously:

```
CG_z displacement  ==  lean_fwd_m      (already enforced; keep it)
CG_y displacement  ==  lean_up_m       (NEW; today this is a ~49 mm lie, see below)
```

Two unknowns, two equations. This is a natural extension of the machinery that
already exists, not a new mechanism.

### ★ Why this is MORE honest, not less — and it retires a known lie

The independent verifier measured, on the shipped build, that the **vertical**
channel carries the same defect §D found in the fore-aft one: vertical K **0.8036**,
i.e. a **−49.1 mm CG error at `up +0.25`**, and **+21.6 mm at `up −0.10`**. Lateral
is worse still (**−64.7 mm** at lat 0.35 / up 0.25). Those survive today only
because the R1a work order said not to re-derive those channels.

So adding the CG_y constraint does **not** invent motion — it **closes a lie that is
already there.** After this rung, two of the three lean axes are CG-honest.
`sim/sled.cpp:313` sets `cg_off = (rider_mass_kg/mass)·(−lat, up, −fwd)` and
displaces the **whole** rider mass, so "visual rider CG must move by exactly the
kernel metres" is the correct honesty condition on every axis.

**Lateral is OUT OF SCOPE for this rung.** Do not touch `lean_lat_m`. Note in your
report what the lateral lie still measures so it can be scheduled.

### How the rise should be shaped

- The rise must be **driven by `lean_fwd_m`**, not invented from nothing: leaning
  forward is what makes a rider stand. Deriving it from an existing kernel scalar
  keeps §0.1 satisfied.
- It must **compose with, not fight, the existing `lean_up_m` stand input.** If Chad
  is already standing and then leans forward, the result must be sane, not doubled.
  State how you compose them.
- The knees fold against the **board-pinned boots** — that anchor already exists and
  is what makes the fold real rather than a floating offset.
- **The acceptance shape: at full forward lean the helmet clears the cowl top line
  (0.8924) with margin, rather than passing 110 mm below it.**

## Also in this rung

### ★ The HUD is covering the feet — and it is aircraft HUD in sled mode

Chad: *"there is HUD bar for flaps right bottom centre occluding my view of the
feet."* His screenshot confirms it: **`FLAPS LANDING 100%`, `BRAKE`, `GEAR DOWN`
and the aircraft instrument strip are all drawn while driving the snowmachine**,
and the flaps bar sits directly over the running boards.

This is a mode leak, not a layout problem. The ladder already records (§6/R5) that
"modes" today are loose bools in a 4,226-line `main()`. **Fix: do not draw
aircraft-only HUD elements in sled drive mode.** Find how drive mode is known
(`SEADS_SLED_DEBUG_MODE`, `info.sled_active`, whatever the real predicate is) and
gate the aircraft strip on it. Keep it minimal and surgical — this is not the mode
manager rung, it is stopping the plane's HUD from covering the sled.

Report what you gated and what you deliberately left drawn.

## Constraints — unchanged and absolute

- ZERO changes under `sim/`, `control/`, `test/harness/`, `test/golden/`. Read kernel
  state; never add, rename or re-tune a kernel value. If a fix seems to need one,
  STOP and report.
- `assets/sled/indy650.glb` byte-identical: md5 `edf1ebdcfe70b68c41c444f651b6acb1`.
- Pose stays a pure function of `(kernel state, tick)`. **No accumulator, no
  `GetFrameTime`, no smoothing, no persistent animation state.** The two-axis solve
  must stay a fixed iteration count with a `static_assert`, closed-form where
  possible — exactly the discipline the existing solve uses.
- Pure math in `seads_render_core`; only draw in the exe target.
- All six tape gates bit-exact. Suite baseline **1231 / 1227 pass / exactly the 4
  pre-existing GI4 reds (526, 527, 560, 563)**; zero new red.
  `ctest --test-dir build -C Debug --output-on-failure`. Never two gates on one
  build dir; do not test against `build-play`.
- `graph_query.py check` green, graph regenerated in-commit. **Set
  `generated_at_commit` correctly — it has named the parent commit twice now.**
- Commit explicit paths only; untracked `conquest_tape_*.jsonl` must never be
  committed. Do not push. Do not delete files you did not create.
- **Rebuild `build-play` when done** — it is what Chad drives.

## Fix these while you are in here (all previously filed, all cheap)

1. `rider_pose.h` claims **"CG honesty everywhere"**. It is one axis of three today
   and two of three after this rung. Correct the sentence to say exactly which.
2. Three constants documented from **pre-§C geometry**, never re-taken after the
   reseat moved: `k_hat` 0.821607 → **0.819082**; leg `|cross|` 0.11552 →
   **0.12360**; leg fallback dots −0.8347/+0.8490 → **−0.8158/+0.8746**.
3. The **§C guard is thin and in the wrong place**: `|reseat.y +
   rest_tip_minus_socket_m.y| < 0.001` would stay green if someone set `reseat.y` to
   the socket gap 0.041678. Put the guard where its comment says it is — assert
   directly that the posed sole lands on the deck sheet 0.252000.

## Reporting

Numbers, not adjectives:
- helmet-to-cowl clearance across the forward-lean axis, before and after, and the
  fwd value at which it now first contacts (if it ever does);
- the **zero-input pose, before and after** — Chad's positioning is a SIGNED,
  APPROVED VISUAL, so if anything moves at rest, say so loudly. It should not;
- CG error on **both** axes across the reachable set, including `steer` (the last
  verifier caught the previous sample set omitting it, and `steer` moves the grip
  sockets the hands are pinned to);
- arm and leg reach ratios and clamping vs the recorded 1.4568 / 1.0750 and
  22.5 % / 1.0 % — **the rise should IMPROVE the arms; prove whether it does**;
- what the lateral lie still measures, for scheduling.

**You do not pass your own work.** An independent verifier re-derives everything.
Do not sign anything, do not edit the ladder status line, do not push.
