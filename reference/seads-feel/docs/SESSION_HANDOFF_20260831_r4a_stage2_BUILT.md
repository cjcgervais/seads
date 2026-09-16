# R4a STAGE 2 — HE LETS GO, AND HE COMES OFF (built 2026-08-31)

**Worktree `D:\seads_sandboxes\r4a-superman`, branch `sandbox/r4a-grip`.**
Base `b8dce66eb`. **AWAITING CHAD'S DRIVE — nobody self-passes this one (§7.9).**

> ⚠⚠ **DO NOT WORK IN `D:\flight_sim2\seads-recon`.** That is his FLY TREE.

---

## §1 WHAT HE ASKED FOR, AND WHAT HE NOW HAS

His words at the top of this rung: *"he dosent let go of the bars YET."*
He does now.

The kernel decides it, the machine feels it, and the man leaves — in that order,
because that is the order §7.3's failure chain is written in and the order is the
feel.

| | | where |
|---|---|---|
| 1 | `grip.capacity` 1e30 → the measured **78.34** | `sim/rider_grip.h` |
| 2 | the 87.5 kg leaves: `cg_off`, `patch_geometry`, the airborne exchange | `sim/sled.cpp` |
| 3 | no hand input reaches the machine: throttle, brake, bars, weight-shift | `sim/sled.cpp` |
| 4 | the free body: gravity, drag, the snow under him, sliding to rest | `render/rider_flight.{h,cpp}` **(new)** |
| 5 | the two IK welds that were HOLDING him release | `render/sled_model.cpp` |

**§7.4's chain then runs itself and nothing was added to make it.** At throttle 0
the clutch engagement (`clamp((throttle - 0.02) / 0.08)`) falls to zero, the belt
commands nothing, the track's own drag is the only longitudinal term left, and
the machine coasts to rest **and does not creep** — Chad's "no creep" is not a
rule imposed anywhere, it is what an unpowered track does, and the kernel already
said so.

---

## §2 THE DIRECTION — ONE HALF IS A RULING, THE OTHER HALF IS PHYSICS

**Q1, the side (Chad, 2026-08-30): "whichever way he's already leaning out."**
`rider_flight_launch` adds `lat_gain_per_s × the lateral he already had`, sign
and magnitude both his lean's. That is the whole of the side.

**★★★ FORWARD IS NOT A TERM, AND MUST NOT BECOME ONE.** He departs with the
machine's own world velocity and *nothing added*. The measured 8-of-10
over-the-bars departure is then **emergent**: the landing is the machine shedding
tens of m/s² into the snowpack while he is a free body with only gravity and air
on him. §3 of the launch doc reads it exactly this way — *"the machine stops and
he does not."* A forward gain would be paying twice for one fact, and
`flight_he_leaves_with_the_machines_velocity_and_nothing_added` is the leg that
goes red if anyone adds one.

---

## §3 THE DIALS — ONE AT A TIME, AND ONLY ONE OF THEM IS HIS

| env | default | what it does |
|---|---|---|
| **`SEADS_GRIP_LATGAIN`** | **2.0 /s** | **★ HIS DIAL.** How hard the release throws him to the side. 0 = straight over the nose. |
| `SEADS_GRIP_CAPACITY` | 78.34 | how easily the grip breaks. **Bigger = rarer.** `1e30` restores stage 1 exactly (he can never come off) — the kill switch and the A/B. |
| `SEADS_FLIGHT_LIE` | = `cg_height_m` (0.564) | how low he ends up lying in the snow. |
| `SEADS_BUCK_GAIN` / `_DECAY` | 0.02 / 3.0 | **SIGNED, FROZEN, AS A PAIR.** Do not move these; they are what he signed on 2026-08-30. |

**⚠ `lat_gain 2.0` IS DERIVED, NOT MEASURED, AND IT IS LABELLED AS SUCH** in
`render/rider_flight.h`: clear a 0.4 m half-width machine inside the ~1 s a
landing-height departure gives him, at a typical 0.2 m of lean. §7.7a already
ruled that only his eye can set it — the side is only ~0.22 of his horizontal
departure, so this gain does most of the work.

---

## §4 THE DRIVE CHECKLIST

**Open `D:\seads_sandboxes\r4a-superman\build-play\seads.exe`.**

1. Drive normally for a few minutes. **The first question is how often it
   happens** — Chad's own target is *"maybe 10 % of jumps given my normal
   driving"*, at the *further* end, *"a little bit rare"*. Too often → raise
   `SEADS_GRIP_CAPACITY`. Never → lower it.
2. **Take a big jump and land it hard.** Watch for: bucked UP but held (the
   common case, and it must stay the common case); then, rarely, the hands go
   and he leaves.
3. **When he goes — which way?** He should go forward over the bars and out to
   whichever side he was already leaning. If the side is invisible, raise
   `SEADS_GRIP_LATGAIN`. If he is flung sideways like he was kicked, lower it.
4. **Where does he end up?** He should land in the snow and slide to a stop, not
   float above it and not sink through it (`SEADS_FLIGHT_LIE`).
5. **The machine, after.** It should shut its own throttle, straighten its bars,
   coast, stop — **and sit there without creeping.**
6. **Press `R` to get back on** (the autoright is the interim remount until R4c–e
   build the real one). He should be back on the machine, upright, no snap.

A console line prints the instant it fires:
`R4a STAGE 4: HE LET GO -- v ... m/s, lean_lat ..., lat gain ..., lie ...`

---

## §5 THE GAPS, NAMED HERE RATHER THAN DISCOVERED LATER

- **`mass_kg` is still 331 kg with nobody on it.** §7.7 names `cg_off` and
  `patch_geometry` and names nothing else, and dropping 87.5 kg without
  re-deriving the inertia tensor it was measured at is the trap `sim/sled.h`
  warns about in its own words. A riderless Indy is 243.5 kg and would coast
  differently. **R4b**, with the inertia re-derived.
- **He keeps the machine's ATTITUDE while he flies.** The departure is a
  translation on the root; a machine that rolls hard after he leaves rolls him
  with it. R4b's tumble is where a world-locked attitude belongs.
- **No tumble, no poof, no getting up.** R4b by §7.9's own staging.
- **`seat_load_frac` / `board_load_frac` still do not exist** and `rider_up_m`
  still has no physical driver. Neither was needed for the release; both are
  still unbuilt spec (they were flagged in the launch doc and the decision is
  recorded here rather than left silent).
- **The camera still follows the MACHINE, not him.** Unruled. It is probably
  right for R4a — the machine coasts to rest near where he lands — but it is
  Chad's call and it is not a thing to change without one.
- **The arms snap out of the bar pose the frame the welds release.** They then
  follow the body chain. "Hands are visibly last to go" (§7.9's video gate) is
  delivered by the STAGE ORDER, not by an arm animation.

---

## §6 WHY NO GOLDEN MOVED, AND IT IS STRUCTURAL

Every tape in the corpus was cut before the grip dials entered
`SLEDTAPE_PARAMS_D`, so the loader forces `grip.unseat_gain = 0` on all of them.
At gain 0 the extension is identically 0, the load with it, and **no capacity of
any value can be exceeded**. The three goldens in `test/golden/sled` were checked
for a `grip.` line: none has one.

**The stage-1 tripwire was re-aimed, not deleted.**
`sled_the_grip_law_moves_no_golden` said in its own comment that it "would go red
the moment somebody wires the release into the machine". That is today. Deleting
it would have thrown away what it was really protecting — the corpus — so it is
now `sled_the_tape_preset_still_moves_no_golden`, driving the tape preset against
a capacity of **zero**, the most hostile value there is.

**RE-TAPE (§4 step 4 of the launch doc) is deliberately NOT done yet.** New
goldens that exercise the release should be cut from **his** drive, once he has
signed the dials. A golden cut from a synthetic drive nobody has flown would pin
a feel nobody approved.

---

## §7 THE INSTRUMENT DID NOT FOLLOW THE CODE, ON PURPOSE

`tools/sled_probe.cpp superman` now **pins `grip.capacity` to 1e30 itself**.
Without that it would have inherited the new 78.34, the release would have fired
mid-replay, the machine would have changed under the measurement, and the
distribution that produced 78.34 would silently have become a distribution of a
different machine. `SEADS_GRIP_CAPACITY` overrides it for a deliberate stage-2
measurement (*how often does the release actually fire over the corpus*) — and
then the replay is honestly a different one.

---

## §8 WHAT IS MUTATION-VERIFIED, RUN NOT WRITTEN DOWN

Four gates, four mutations, four reds — each one run:

| mutation | red leg |
|---|---|
| `rider_frac` ungated (the mass never leaves) | `sled_the_87_kilos_leave_and_that_is_a_separate_fact` |
| throttle ungated | `sled_the_release_takes_the_rider_off_the_machine` (the coast-down) |
| bars ungated | ...the `steer_actual` line |
| lean ungated | ...the `rider_lat_m` line |

**★ The mass leg needed isolating and that is the point.** In an ordinary drive
the mass leaving is INVISIBLE behind its neighbours — the lean commands are
zeroed too, so his displacement decays on its own and a whole-drive A/B would go
red even with `rider_frac` left ungated. So both arms get the SAME already-leaned
man and the same zero inputs and differ in ONE bit; the lean then decays
identically in both, and what cannot be the same is whether that displacement
still moves the contact patches. **Plus the control arm** — the same two arms
with nothing leaned must agree BIT FOR BIT, which is what stops the assert above
passing for some other reason.

---

## §9 NEXT

1. **His drive.** One dial at a time, §4 above. Nothing lands on main before it.
2. **Then re-tape** — cut goldens off the drive he signed.
3. **Then R4b**: the tumble (and with it the world-locked attitude), the poof,
   surface-dependent getting up — and the honest mass/inertia gap in §5.
4. The red-team pass on this rung has **not** been run. Stage 1's found nine real
   defects in work that was already gated and mutation-verified; §5 step 3 of the
   launch doc asks for it before landing.
