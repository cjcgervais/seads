# R4c-2 — THE REALISM PASS: PLAN AND OVERNIGHT BUILD (2026-09-01)

**For Chad, to read in the morning.** Worktree `D:\seads_sandboxes\r4a-superman`,
branch `sandbox/r4a-grip`.

---

## §0 THE SHORT VERSION

You asked for three things, a plan, and a build. Five Opus consults ran with
separate packets — procedural walk realism, fall/skid physics, the poof, the
release statistics, and an adversarial red-team of both rungs.

**Between them they found eleven defects, and eight of those turned out to be
ARITHMETIC OR WIRING, not taste.** "Walks like a robot" was three separate
arithmetic errors. "Doesn't skid" was a regression plus the wrong friction law.
"I'm buried in the deep snow" was a man drawn *hovering half a metre above the
snow*. None of them were judgement calls, and most are already fixed.

**★ AND ONE OF MY OWN DIAGNOSES WAS WRONG.** I told you last night that the
release was firing *in the air*, and that this was why you went off backward and
seated. **That is refuted.** `g_eff` is structurally ZERO in free fall — the
kernel's own comment says so and it is exact — so an airborne crossing is off by
a factor of ~50 and **cannot happen**. It is a *ground* release: the take-off
buck, or ordinary rough running. Same symptom, different cause, and it matters
because it changes the fix.

---

## §1 BUILT OVERNIGHT — what is in `build-play\seads.exe` right now

### 1.1 THE SKID — it was my regression, then it was the wrong law

Two separate faults, both fixed:

- **The regression.** When I moved the body from `render/` into `sim/`
  yesterday, the landing got rewritten to zero his velocity, and
  `ground_drag_per_s`/`rest_speed_mps` became dead parameters nothing read. The
  test that protected it did not come across either. My commit message claimed
  "the legs came with him"; for that one it was false.
- **The law.** Even restored, it was an exponential decay at 6/s — which from
  15 m/s stops him in **2.5 m**. The consult measured what that actually is:
  very nearly the right answer for **deep powder** (3.2 m), and an order of
  magnitude short for a **packed trail** (29 m). *I had applied the powder answer
  to every surface.*

**Now: Coulomb friction plus a plough term.**

| surface | µ | slide from 15 m/s |
|---|---|---|
| packed trail | **0.40 — MEASURED, whole body** | 29 m |
| deep snow | 0.55 + plough | **3.2 m, and he buries** |
| plowed road | 0.45 (inferred) | 25 m |
| lake ice | 0.15 (weak evidence) | 76 m |

The 0.40 is a real measurement on a real body: Nachbauer et al., *Kinetic
Friction of Sport Fabrics on Snow*, Lubricants 2016 — a linear tribometer at
−4.3 °C, plus that group's whole-body slide with µ back-calculated from
video-tracked CoM acceleration. **A body is 10–15× draggier than a ski**, so the
machine's own `mu_kin` was the wrong table to reuse and is not reused.

★ **The shape matters as much as the number.** Coulomb distance goes as **v²**;
an exponential's goes as **v**. Under the old law a crash at double the speed
skidded double, when it should skid nearly four times as far — and "fast crashes
don't skid proportionally" is exactly the thing an eye notices. There is a gate
leg that fails if anyone puts the wrong shape back.

★ **And the plough is the machine's own physics.** The sled already has
`0.5·rho_eff·v²·a_sub·plow_cd`. The consult derived the body's coefficient from
first principles at ~30 kg/m; the sled's own Bush constants give **29.9**. Same
number twice, nothing invented — and it is why deep snow stops him in 3 m while
the trail lets him run 29.

**A real ordering error fell out of this:** the get-up clock and the skid were
running concurrently, so on a long skid he began standing up **while still
travelling 20 m/s**. The clock now waits until he has stopped — and because
Coulomb lands on zero at a definite instant, "he has stopped" is an exact test
and not a threshold somebody picked.

### 1.2 THE POOF, AND BEING BURIED

**Your "im buried in the deep snow" was a drawing bug with an exact cause:** the
kernel pins him at `drive_r + lie_clearance_m`, and nothing ever lowered him. So
`Buried` drew as *a man lying flat, hovering 0.564 m above the snow*, for up to
four seconds, while a text label said "UP".

Built:
- **He sinks, then he is not drawn at all.** Sink depth is the snow he is in plus
  a little, so shallow snow structurally cannot swallow him.
- **A burst on the existing roost/spray path** — a fourth *trail*, not a second
  particle system (§0.3). It reuses the puff, the hash, the ballistic
  integrator, the ring cap, the sorted batch and the shader — including that
  shader's existing workaround for this project's `DrawBillboard`-renders-nothing
  trap.
- **Two bursts:** the impact (up to 120 puffs, ejecta at 0.35× impact speed,
  ~3–4 m tall, 2 s) and the emergence (a quarter of that, slow, anchored on his
  *body* — snow sliding off him, not a second explosion at his boots).

★ **The saturation speed is not a chosen number.** Work–energy on a body
punching into snow gives penetration `d = 0.00429·v²`, using the repo's own rider
mass and a frontal area derived from the rig's *measured* 1.85 m stature and
0.46 m suited breadth. That reaches **the signed 0.77 m depth law at exactly
13.4 m/s** — an ordinary trail speed. Below it he half-buries; above it the hole
cannot deepen, so the burst saturates there too. The depth law and the burst
agree without being told to.

Dial: `SEADS_POOF` (0 kills it exactly).

### 1.3 THE WALK — three arithmetic errors, not aesthetics

The consult's headline: **"three of the four biggest problems are arithmetic."**

1. **★★★ The planted foot slid backward at full walking speed.** The stance
   swept a *whole stride* over *half a cycle*, but phase advances by
   distance/stride — so half a cycle is *half a stride* of travel. The foot
   skated at 1× body speed, 3.5 m/s of backslide on hardpack, for the whole of
   stance. The header even stated the correct intent while the code did twice
   it. **Foot-skate is the one gait error people detect pre-attentively. It is
   the definition of "robot".**
2. **★★★ The cadence was 2.7× human maximum.** The legs are half a cycle apart
   *within* one cycle, so **one cycle is two steps** — my "3.9 steps/s, a jog"
   was really 7.8 steps/s = **468 steps/min**, against a human sprint maximum
   near 260. Replaced by a *measured invariant*: the walk ratio (step length ÷
   cadence ≈ 0.0060, Sekiya & Nagasaki 1998), which gives `stride = 1.20√v` and
   predicts free-walk cadence and stride to within **4 %** of normative data.
   **One measured constant replaced the three stride numbers I had picked.**
3. **★★★ The deep-snow foot lift was kinematically impossible.** It asked for
   0.85 m of foot rise against a **0.908 m** leg chain — so the IK hit its own
   clamp, and *a clamped two-bone chain is a fully-extended strut*. That is the
   most literal possible source of "robot". Deep-snow travel is **wading, not
   stepping over**: the lift now saturates at what the leg can reach.

Also fixed: the swing curve was C1-discontinuous (a hard corner at toe-off and
another at heel strike, twice a cycle). The cubic Hermite that matches the stance
slope hands back two real gait features for free — **the toe trails at toe-off**,
and **the foot retracts before heel strike**, which is the mechanism that makes a
footfall read as planted rather than stamped.

And stance fraction is no longer hard-coded at 0.5 — which gave *zero* double
support, the topology of a run. It now follows a measured row (Nilsson &
Thorstensson 1989): 0.68 at a trudge (36 % double support), 0.38 at 3.5 m/s
(24 % flight).

### 1.4 THE RELEASE — the trigger was comparing the wrong signal

Not dialled (see §2), but one threshold-free fix shipped, because it repairs a
deviation from **this repo's own standing law**, written in `sled_probe.cpp`:

> `grip_capacity_n` is compared against `grip_load_lp`, **NEVER** against
> `grip_load_n`.

The shipped kernel compared the **instantaneous** product. That makes the release
rate a function of the integrator's substep count — a numerical artefact — and it
is also wrong physically: **a grip fails under sustained overload, not on a
one-substep spike.** A rut transient carries a huge field for a few milliseconds
and your hands do not notice; a landing on an extended body lasts. The capacity
now reads the 0.1 s-filtered load. Continuous, so no threshold is added, and
`load_tau_s = 0` restores the old behaviour bit-identically.

**This alone should suppress the ground-spike releases**, which is the *character*
of what you reported, not just the rate.

### 1.5 RED-TEAM FIXES

The adversarial pass found **12 confirmed defects**. The two that mattered:

- **★★★ A tape recorded across a fall was unreplayable.** The sled tape arms a
  ground-query tap around the tick loop — its comment says "nothing else calls
  `sample_at` in between" — and R4c put the walker's own ground query inside that
  window. Every tick after a fall emitted an extra record; replay consumes them
  in order against an exact key, so the first walker record killed the tape. For
  *exactly* the drives this rung exists to record. Fixed: the tap is disarmed
  across the walker's step.
- **★★★ Remounting with the mount key stranded the man.** That block resets the
  machine (re-attaching the grip) and has no `!sled_seeded` guard, but never
  touched the walker. Result: camera stuck at the old fall site, W/S/A/D driving
  both bodies, and **every subsequent fall silently doing nothing — no
  departure, no helmet dent — forever.** Fixed.

Plus: the get-up stages did not partition the rise (16.3 s where §7.5 signs 15);
a `quat_cast` on an un-normalised matrix where its twin normalises; and two
comments that had stopped describing their code — including the one paragraph a
reader would use to decide whether the corpus is safe.

---

## §2 WHAT I DID **NOT** BUILD, AND WHY — the fall frequency

You asked for it to happen a little less. **I did not touch the capacity, and the
reason is that the consult found the number is built on a broken population.**

Running the probe over your 45 distinct drives:

| | count |
|---|---|
| drives contributing **one window at grip load 0.00** | **34** |
| drives contributing zero windows | 9 |
| drives contributing real windows | **2** |

**34 of 45 "drives" in the calibration are the spawn drop** — the machine being
placed on the ground at t=0. The sister probe already excludes exactly this; the
one that produced 78.34 does not. So the distribution behind the shipped number
is ~41 % structural zeros pooled with **two** real drives whose own p90s are
**35.2 and 123.2** — a 3.5× disagreement that this repo's own noise-floor law
says never to pool across.

Padding with zeros drags the p90 **down**, i.e. toward *more* releases. Together
with the second effect — the calibration measured a window `[take-off, landing +
0.3 s]` while the kernel is exposed to **the entire drive**, which is a strict
one-way inflation of the rate — **78.34 is plausibly two to three times too low.**

★ **And the mechanism's own arithmetic says the peak load sits on the TAKE-OFF
BUCK, which the calibration window excludes by construction.** During flight the
charge term is exactly zero, so all the touchdown extension was bought at
take-off. That, not an air release, is why you left backward and seated.

**So the next move is a measurement, not a dial** — details in §3. Raising the
capacity now would make a wrongly-characterised release rarer without making it
right, and I would be picking a number to satisfy a sentence instead of measuring
one.

⚠ **One thing only you can rule.** You said "maybe 10 % of jumps" *before* ever
driving a firing release, and "falls off pretty easy" / "a little less" *after*,
against 78.34. If the measured rate turns out to be, say, 45 % of jumps, then "a
little less" from 45 % is not 10 %. **I will measure the current rate, tell you
the number, and ask what "a little less" means against it** — rather than
silently reconciling three statements you made about three different builds.

---

## §3 THE PLAN — next, in order

### R4c-2a · MEASURE THE RELEASE (first, and it is a go/no-go)
Build a `grip_release` probe mode — ~80 % of it already exists in
`probe_grip_hold`'s event machinery. Over the corpus, at a **ladder** of
capacities, report per crossing: was it `SPAWN` / `GROUND` / `BUCK` / `AIR` /
`LAND`, and which factor carried it (field vs extension).

**The go/no-go:** if most first crossings are `LAND`, it is purely a number
problem — recompute the p90 over *real* jumps with the take-off buck inside the
span, and ship the ladder rung whose *measured* rate is nearest what you want. If
most are `GROUND` or `BUCK`, the capacity is not the defect and §1.4's filter has
to be measured first.

Then: three named rungs (5 %, 10 %, 20 % of jumps) so "a little less" is **one
rung**, executable without re-deriving anything.

### R4c-2b · THE ARMS (the loudest remaining walk tell)
The arm IK is gated on `welded`, so **while he walks nothing writes the arms at
all** — they sit in the seated bar-grip pose. *He is walking through the snow
holding an invisible handlebar.* Of everything in the packet this is most likely
what your eye actually caught. Strictly additive (it only runs when he is off the
machine, so it cannot move a signed riding pose): shoulder arc `12 + 15v` degrees
anti-phase with the same-side leg, elbow 20°→90° with speed, both damped and
abducted in deep snow — where below ~0.8 m/s the anti-phase swing breaks down
entirely and the arms come **up and out for balance**. That last bit is what
stops a trudge looking like a fast walk played slowly.

### R4c-2c · THE PELVIS (six lines that buy four channels)
Vertical bob `0.011 + 0.0233v` at **2× per cycle** (minimum at each footfall),
lateral sway `0.097 − 0.0388v` at 1× (peak over the stance foot). ★ **Note the
sign inversion: sway DECREASES with speed while bob INCREASES** — get it backwards
and a fast walk looks drunk. It also pays three dividends: the stance-phase knee
flexion wave comes free through the existing leg IK, the head bobs free through
the hierarchy, and it relieves the IK reach at heel strike.

### R4c-2d · SNOW COMING OFF HIM, and the trench
You asked for it by name. Key the shed to the **gait phase crossing at toe-off**,
not a timer — so a man standing still sheds nothing, structurally. Then the body
trench: the machine's deformation field could take a body-shaped stamp, but ⚠ it
would put the walker into the driven surface and **end the guarantee that a drive
replays whether or not a man was walking beside it**. That is a rung of its own,
and it should wait until tapes are being re-signed for another reason.

### R4c-2e · TUMBLE (R4b proper)
The empirical split from 534 analysed crash videos: **tumbling 52 %, flat landing
31 %, sliding 11 %**. A deterministic six-phase chain with the rotation count
drawn *once at departure* gives that without a ragdoll. And the real "over the
bars" term is a **rotation about the hands** — at release he carries `v = v_hands
+ ω × r`, worth 1.5–3 m/s *instantly*, which is the same order as the entire drag
difference and arrives at once instead of accumulating. That is the missing term,
and it is derived rather than a shove.

---

## §4 THE DRIVE, WHEN YOU GET UP

**`D:\seads_sandboxes\r4a-superman\build-play\seads.exe`**

1. **Come off on a packed trail.** He should now **slide a long way** — tens of
   metres, not two. And a fast crash should slide *much* further than a slow one.
2. **Come off in deep bush.** He should plough to a stop in a few metres, **go
   under with a burst of snow**, be gone, and come back out with a smaller one.
3. **Watch his feet while he walks back.** They should stay planted (no
   backslide) and the cadence should read as a man, not a wind-up toy.
4. ⚠ **His arms are still on invisible handlebars** — that is R4c-2b and it is
   known.
5. The release still fires as easily as before; that is deliberate (§2).

Dials: `SEADS_POOF`, `SEADS_WALK_SPEEDS`, `SEADS_WALK_RISE`, `SEADS_WALK_FOOTDROP`,
`SEADS_GRIP_LATGAIN`, `SEADS_GRIP_CAPACITY`.

---

## §5 STILL OPEN

- `SEADS_WALK_FOOTDROP` is still unmeasured — one `TraceLog` of the rig's rest
  foot offset turns it into a derived constant, and the consult named the two
  numbers to print.
- Step width is still the **machine's**: he walks with his boots where they sat
  straddling the sled, ~0.55 m apart. Human is 0.08–0.12 m, 0.25–0.40 m
  postholing. The rig's own hip half-span (0.095 m) is the right base.
- `lie_clearance_m` is a *lying* clearance being spent on a standing man.
- Prone is a pitch, not a pose; no real crawl.
- `mass_kg` is still 331 kg with nobody aboard.
- The deep-snow swing should become a **lift-translate-drop trapezoid**, not an
  arc — that is the shape of "large stepping".
