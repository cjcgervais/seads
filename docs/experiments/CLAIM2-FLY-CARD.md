# FLY CARD — "does a straight-up deflection have roll in it?"

**For Chad. One sortie, two blocks, ~4 minutes of actual flying.**
**Registration:** `CLAIM2-ROLL-IN-VERTICAL.toml`, locked `2026-08-04T05:22:15Z`,
sha `79c3714cf1e0e660…`. **The registration was locked before this card was written and before
any data exists.** Written 2026-08-03 by the kernel-docs agent.

> ⛔ **NOT ISSUABLE YET.** Two SOP-01 gates are red and they are `cascade-recorder`'s, not
> yours — see §5. **Do not fly this until they are green.** The card is finished and waiting.

---

## 1. Warm up first. No instruments, no task.

**Fly however you like for as long as you like.** Loops, tunnel, shooting, nothing.
Per your 2026-07-31 amendment: **the warm-up comes before the instruments, always.** Nothing in
the warm-up is recorded as data. Say when you're ready.

## 2. What you'll do — the same thing twice

**Two blocks. About 60–90 seconds of flying each.** Between them someone changes one thing.

> **You will not be told which block is which, and you should not try to work it out.**
> That is deliberate (clause C5). Your job is to fly and say what you feel.

**The task, both blocks, identical:**

> **Point straight up and pull. Again. And again.**
>
> From level, deflect the aim straight up and take the nose vertical. Let it settle, come
> back down, do it again. **Ten or so pulls per block.** Keep them clean and vertical —
> resist the urge to turn, roll, or make it interesting.

**Fly nothing else during the blocks.** This is the one strange instruction on the card and
it is load-bearing: the analysis tool has **no way to filter rows**, so it measures the whole
tape. If half the block is level cruising, the level cruising drowns the thing we are trying
to see. **The sortie is the filter.**

## 3. What to feel for, and what to say

**The one question:** *when you ask for straight up, does the aeroplane put roll in it?*

Say it in your own words, per block. Useful shapes:

- "it went up clean, no roll"
- "it rolled left going up, every time"
- "it rolled on the way in but straightened out"
- "block 2 was cleaner than block 1" ← **this is the most valuable thing you can say**

**Your words are the specification.** Don't translate them into mechanism. If it felt like
something and you can't name it, say the something.

## 4. What is instrumented, and what the numbers will decide

| | |
|---|---|
| **measured** | `bankRaw` — the bank angle the aircraft actually held, 60 times a second |
| **statistic** | mean of \|bank\| across the whole block |
| **threshold** | **2.0°** difference between the blocks |
| **why that number** | on the flown tape of 2026-08-03, mean \|bank\| at near-vertical aim is **8.43°**. 2.0° is about a quarter of that — big enough to be felt, far above the noise of the column |
| **floor** | 1,800 rows (30 s) per block, or the tool refuses to conclude anything |

**It deliberately does NOT measure the dwell channel itself.** It measures **the symptom you
reported.**

### ⚠ What the second block actually changes — corrected 2026-08-04, before the flight

**One block has the dwell servo's *levelling push* turned off. It does NOT have the whole dwell
channel turned off** — the servo's own *damper* is still running, in **both** blocks, unchanged.

I had this wrong in the registration and said "dwell off." The build agent found it while
building the block and was right: turning that dial to zero removes the push but leaves the
damper, which is about a third of what that channel does. **The comparison is still clean —
the damper is identical in both blocks, so anything that differs is still the one thing I
changed.** It just means the answer is narrower than I first wrote, and the wording below is
the corrected version.

**One thing to notice, not a problem:** the second block is a configuration this aeroplane has
arguably never flown — damper with no push. It can only ever *resist* roll, so it should feel
calmer if anything, never looser. **If it feels loose or divergent, that is worth saying.**

### What the outcomes mean — written now, so nothing can be rationalised later

- **Blocks differ by ≥ 2.0°** → the dwell servo's **levelling push** is a real contributor to
  the roll you feel. A named cause, and something to fix.
- **Blocks differ by < 2.0°** → **one of two things, and we check which before saying which.**
  Either the levelling push is not the cause, **or** it is a cause and its own damper quietly
  cancelled it — the damper pushes the opposite way, at about half the strength of the thing we
  turned off. **We can tell these apart from the tapes you are about to fly**, because both blocks
  record the damper every frame. **Until that check is run the answer is reported as
  "indeterminate," not "no."** ⚠ It also does not clear the dwell channel outright, because the
  damper flew in both blocks.

  > **Why this is written here and not decided afterwards:** a red-team agent found that a
  > "no difference" outcome had two incompatible explanations and no way to choose between them.
  > That was found **before** you flew, so the rule for reading it is written **before** you fly.
  > Deciding what a result means after seeing it is how you get an answer you can't trust.
- **Your feel disagrees with the numbers** → **the instrument is wrong and gets rebuilt.**
  Not you. (`THE LAW` clause 3.)

## 5. ⛔ Kill flags — stop and say so

1. **The recorder isn't writing.** If the log isn't growing, stop. A flight nobody recorded is
   the 2026-08-02 failure and we do not repeat it.
2. **The two blocks feel identical in every way** — including no difference at all in the thing
   you're looking for. Worth saying out loud; it is data.
3. **Anything else feels wrong with the aeroplane.** Wrong speed, wrong power, wrong handling.
   One variable moved; if something else moved too, the comparison is void.
4. **You get bored or it stops being ten clean pulls.** Say so and stop — a sloppy block is
   worse than a short one, because the floor check can't see sloppiness.

---

## 6. SOP-01 GATE — the evidence block. Commands run and their real output.

**Per SOP-01, a flight request without this block is VOID.** Filled by the kernel-docs agent,
2026-08-03. **Two rows are RED and they gate the flight.**

| | gate | state | evidence |
|---|---|---|---|
| **G1** | sink exists and writable | 🟢 | `captures/EvCTAPE-v3_20260803T2049_d5e0717-dirty.log`, 1,966,697 bytes on disk |
| **G2** | end-to-end smoke test | 🟡 | **GREEN for the control config, RED for treatment.** The 2026-08-03 sortie proves the running build emits rows and an agent read them back off disk. **The treatment build has never been built or smoke-tested.** |
| **G3** | every instrument armed | 🟢 | `bankRaw` (ord 13), `dwellB` (22), `rollAimApp` (35), `rollBstApp` (36), `keyMask` (34) all present and non-constant in the flown tape; indices from the tape's own `cols=` |
| **G4** | proven non-blind | 🟢 | `python test_reconcile_rollout.py` → *"GREEN: negative control passed and all 9 injected faults were caught"* — run this session |
| **G5** | coverage sufficient | 🟢 | the question is "how much bank during a vertical pull"; `bankRaw`, degrees, every frame |
| **G6** | rate adequate | 🟢 | **60.0 Hz per-frame**, measured: 5,173 rows over 86.19 s, ordinals 2→5174 contiguous, **zero drops** |
| **G7** | analysis runs BEFORE the flight | 🟢 | **the whole chain was run on the real tape this session.** `parse_tape.py --tape1 --arm control` → 5,173 rows; `compare_arms.py --prereg …` → **`REFUSED: DATA PREDATES PREREGISTRATION`**. That refusal is the correct behaviour and proves the C3 cutoff is enforced mechanically, not on trust |
| **G8** | reference numbers cited | 🟢 | mean \|bankRaw\| = **8.43°** at `aimElev > 45°` (n=633), **19.44°** at `> 60°` (n=60), tape sha `b85c43bb…` |
| **G9** | volume known in advance | 🟢 | ~1.9 MB / 5,173 rows per 86 s ⇒ **~3,600–5,400 rows and ~1.3–2.0 MB per block** |
| **G10** | build identity recorded | 🟡 | **GREEN for control** — header carries `build=d5e0717-dirty 2026-08-03 14:41 tree=… branch=cascade/rebuild`. **RED for treatment** — that build does not exist yet |

### What is owed before this card may be issued — `cascade-recorder`

1. **Build the treatment arm** — `dwellLevelRateMult = 0.000`, **and nothing else changed.**
   `compare_arms` diffs the two headers and refuses if a second key moved.
2. **Smoke-test it** (G2) and **confirm its build stamp lands in the header** (G10).
3. Confirm `dwellLevelRateMult` is reachable as config, not a code edit — if flipping it
   requires a source change, the two builds differ by more than one key and **this design
   needs revisiting before, not after, the flight.**

**Nothing else is outstanding.** The recorder, the attribution arithmetic, the bridge, the
comparison tool and the C3 enforcement are all verified working on real data as of
2026-08-03.
