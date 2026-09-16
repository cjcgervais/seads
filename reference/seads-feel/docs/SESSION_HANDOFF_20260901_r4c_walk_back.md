# R4c — HE GETS UP AND WALKS BACK (built 2026-09-01)

**Worktree `D:\seads_sandboxes\r4a-superman`, branch `sandbox/r4a-grip`.**
**AWAITING CHAD'S DRIVE.**

> ⚠⚠ **DO NOT WORK IN `D:\flight_sim2\seads-recon`.** That is his FLY TREE.

---

## §1 OPEN THIS

    D:\seads_sandboxes\r4a-superman\build-play\seads.exe

Ride. Come off. **Watch him get up and walk back to the machine.** Press `R`
to get on again.

---

## §2 WHAT HIS TWO RULINGS BOUGHT

**"Its quality / speed is determined by snow depth"** — and **depth is the one
independent variable**, driving both. Not a surface class: the corridor override
already zeroes depth on plowed trails and roads (§2.3), so the depth field
already carries the class distinction **continuously**. There is no threshold in
`sim/walker.*` for that reason.

**His three anchors, and they are measurements, not endpoints I drew a line
between:**

| depth | his words | shipped |
|---|---|---|
| ~0, hardpack | *"quick gait"* | 3.5 m/s, 0.90 m stride → **3.9 steps/s** |
| **"about a foot or two"** = 0.30–0.61 m → **0.45 m** | *"midly handicapped ... but slowed"* | 1.6 m/s, 1.00 m stride |
| **0.77 m**, the SIGNED depth law | *"large stepping"*, and he goes UNDER first | 0.6 m/s, 1.10 m stride → **0.55 steps/s** |

★★★ **STRIDE IS A SEPARATE ROW FROM SPEED, and that is what makes his two
phrases two MOTIONS rather than one motion at two rates.** Stride GROWS with the
snow while speed FALLS, so cadence — authored nowhere, only ever speed/stride —
collapses from a trot to a heave. Deep snow is a man lifting one knee clear at a
time. Hardpack is a jog. **And the swing foot's clearance IS the depth he is
standing in**, so "large stepping with snow coming off" is the foot getting out
of what it is in, not an animation that says so.

★★★ **THE MIDDLE ANCHOR IS THE WHOLE POINT.** A straight line hardpack→deep
would make him fast in a foot of snow, which is exactly what *"midly handicapped
... but slowed"* refuses. There is a gate leg that goes red if anyone
straightens it, and it was run as a mutation.

**The sequence, on ONE clock:** poof + **buried** → poof + **prone** →
**crawl** → **afoot**. Every stage is a *fraction* of the get-up time the depth
already bought (§7.5's own "fifteen seconds in the bush and two on the trail"),
so the whole thing stretches with the snow and nothing has a second clock to
drift on. **In hardpack the buried stage is structurally zero seconds long** —
not merely short.

**The helmet.** Each fall advances the dent one stage and **stops at the worst
one**. The hook was already there and already said so (`sled_helmet_dent_set` —
"cosmetic crash-damage helmet ... crash logic may set it directly"), and it
already clamped to [0,4] — which is what makes this an accumulation rather than
`U`'s cycle, and `U` cycles back to pristine because a preview should. A helmet
does not heal. Cosmetic only: nothing but the draw reads it.

---

## §3 THE DRIVE CHECKLIST

1. **Come off somewhere packed.** He should be up in about **two seconds** and
   trotting.
2. **Come off in deep bush.** He should go *into* it, be gone, come back out,
   crawl, and then heave through it — and it should cost you something like
   **fifteen seconds**. That difference is the beat §7.5 exists for.
3. **Walk him back**: `W` forward, `S` back, `A`/`D` turn. The same keys the
   machine uses, so your hands do not change when your body does.
4. **The camera is on HIM** once he is off, and stays on him while he is buried
   — the frame he climbs out of is the frame he went into.
5. **Watch his feet.** They should stay planted while they are on the ground
   (no skating) and lift clear of the snow when they swing.
6. **`R` puts him back on the machine.**
7. **Take a few falls in a row** and check the helmet is quietly getting worse.

**Dials, one at a time:**

| env | default | what |
|---|---|---|
| `SEADS_WALK_SPEEDS="hard,mid,deep"` | `3.5,1.6,0.6` | ★ the three paces |
| `SEADS_WALK_RISE="hard,deep"` | `2,15` | seconds to get up |
| `SEADS_WALK_FOOTDROP` | `0` | ⚠ **if his boots ride high**, see §5 |
| `SEADS_GRIP_LATGAIN` | `2.0` | which way the release throws him |
| `SEADS_GRIP_CAPACITY` | `78.34` | how easily he comes off (see §4) |

---

## §4 WHAT I DID *NOT* CHANGE, AND WHY — THE RELEASE STILL FIRES TOO EASILY

You said it was *"pretty easy ... but this way its good for testing"*, so it is
untouched. But I owe you the diagnosis, because **your "backward, sitting" was
one cause, not two**:

- §3 of the lateral measurement says the corpus-wide departure sign is **aft**
  (drag streams him back in flight) and that it **flips at the landing**. So a
  release that fires in the AIR throws him backward, and one that fires at the
  TOUCHDOWN throws him forward over the bars. He went backward — it is firing
  early. And "sitting" follows: he never got to superman, so there was no
  stretched pose to leave in.
- **And I can see the reason in my own work.** 78.34 is the p90 of a statistic
  taken **once per airborne window** (84 samples, read at touchdown). The kernel
  compares `load` to it on **every substep of the whole drive**. A level a
  per-window statistic clears 10 % of the time is cleared by the running signal
  far more often. That is this thread's own recorded disease for the third time:
  *a threshold read off a subsample is right only by luck.*

**The honest fix is a measurement I have not run**: the percentile of the
RUNNING load over your real corpus. It is the next small rung whenever you want
it.

---

## §5 THE GAPS, NAMED RATHER THAN DISCOVERED

- **`SEADS_WALK_FOOTDROP` is the one number in this rung I could not measure
  without the game.** The rest pose has his boots on a running board, not on the
  ground, and how far that sits above the snow is a fact about the `.blend`. It
  ships at 0 with a dial rather than a constant somebody chose — if his boots
  read high or sunk, that is the knob, and the value you land on becomes the
  measurement.
- **Prone is a pitch, not a pose.** He lies face-down by rotating about his own
  shoulders; his limbs keep the pose the rig gives them. A real prone/supine
  and a real crawl are hand-built poses and belong with R4b's tumble.
- **The poof is an EDGE, not yet a particle.** The kernel raises it on exactly
  the step he goes in and the step he comes out; §0.3 says reuse the roost/spray
  path rather than fork a second system, and doing that properly is its own
  small rung. Today it prints a note. **The edge is right; the emitter is the
  debt.**
- **No tumble** (R4b), **no snow shedding off him** as he walks, and **no
  automatic remount** — `R` is still the scaffold R4d/R4e retire.
- **`mass_kg` is still 331 kg with nobody on the machine** (stage 2's gap,
  unchanged): §7.7 names `cg_off` and `patch_geometry` and nothing else, and
  dropping 87.5 kg without re-deriving the inertia it was measured at is the
  trap `sim/sled.h` warns about in its own words. R4b.
- **He is not in the sled tape**, deliberately: the tape is a MACHINE tape, and
  nothing in `sim/sled.cpp` reads one line of the walker, so a drive replays
  bit-exactly whether or not a man was walking beside it. **The day he can push
  the machine (§7.8's righting, R4d) that stops being true** and the tape has to
  grow.

---

## §6 THE ARCHITECTURAL MOVE THIS RUNG MADE

**The man's body left `render/` for `sim/`.** Stage 2 built the departure in
`render/rider_flight.*` on §7.7's split ("render-side: chain solve, blend
weights, IK, tumble pose"). That was right about his POSE and wrong about his
BODY: a man who takes input, samples the snowpack, decides where he is on a
sphere and hands control back to the machine is a second rigid body, and the
moment R4c gave him a walk there would have been **two homes for one man**.

So it moved — the same move stage 1 made when the arming memory went from
`render/body_drive.*` into `sim/rider_grip.*`, for the same reason. Render keeps
the man it DRAWS. `render/rider_flight.*` is gone and its legs came with it.

**And the attitude debt stage 2 wrote down came due here.** A translation-only
departure cannot walk: a man whose feet step along the machine's forward while
he travels along his own is broken on sight. He now has his own frame — HIS up
(the sphere's, under his feet, not the machine's) and HIS forward (the heading
the kernel walks him along) — so his feet and his travel cannot disagree by
construction.

---

## §7 MUTATION-VERIFIED, RUN NOT WRITTEN DOWN

| mutation | red leg |
|---|---|
| gait advanced on a CLOCK instead of distance | `walker_the_gait_advances_on_distance_not_on_time` — off by 0.34 of a cycle, the exact ramp deficit predicted |
| the middle anchor thrown away (straight lerp) | `walker_depth_sets_the_pace_across_all_three_of_his_anchors` |

Plus stage 2's four, re-run: ungating `rider_frac`, the throttle, the bars, the
lean each red a named leg.

⚠ **And one leg failed for a reason that was not the law**, which is worth
recording: the first draft seeded him at 0.564 m — the machine's CG height —
which is **exactly where he comes to rest**, so he landed on the first step and
never fell. The helper now puts him in the air, and the property it exposed is
real and stated: *a man released at rest height is already down.*

---

## §8 NEXT

1. **His drive**, §3.
2. Whichever of these he wants: the release-timing measurement (§4), the poof
   emitter, R4b's tumble + real prone/crawl poses, or R4d/R4e (right the
   machine, remount) which retire `R`.
3. **The red-team pass has not been run** on stage 2 or R4c. Stage 1's found
   nine real defects in work that was already gated and mutation-verified.
