# R4a PHASE 0 — FINDINGS

**Read this one. The other two are its working.**
`docs/R4A_PHASE0_MODEL.md` is the derivation of the instrument;
`docs/R4A_PHASE0_DATA.md` is the full 27-tape distribution. This file is the
answer to the question you actually asked: *what does the grip load look like on
my own riding, and where does `grip_capacity_n` sit?*

> ★ **STATUS, 2026-08-25 — read before quoting any number in this file.**
> Phase 0 is now **committed and pushed** (`ad0bd24c8`); the §9 "nothing is
> committed" line below is superseded. The **CASE-F hole in §3.1 is FIXED**
> (`8c306e610`) and the corpus re-run that scored it is **§2.5**. And **the
> corpus is 31 tapes, not 27** — every "27" in this file, in the handoff, and in
> all four consult reports predates four later recordings; see §2.5.5. The
> percentile tables in §2 are still the 27-tape figures and have not been
> regenerated.

Branch `sandbox/r4a-phase0`, off `main` `8852cf7d7`. **Nothing is committed.
Nothing is pushed.** Section 6 lists every file that moved.

**You rule the dial. This document proposes a RANGE and shows its reasoning. It
does not pick a number, and no agent on this rung self-passed one.**

---

## 1. THE HEADLINE

### 1.1 What was measured

All **27** `.sledtape` files in `build-play/` — the 19-tape roster plus 8 tapes
recorded during and after the R3-HANDS rung — were replayed **in full and
bit-exactly** against today's kernel: `first_div tick = -1` on every one, every
`rolled` tick identical between tape and replay, **743,157 ticks = 103.2 minutes
of your own driving** at dt = 1/120. Nothing here is a sample or a subset.

The instrument is a planar sagittal **rod** rider with contact-set enumeration
(seat / boards / hands), rebuilt from the shipped `indy650.glb` on every run. Its
self-checks reproduced 27 independent times: at rest **grip = 0.00 N**, symmetric
3 g bump **grip = 0.00 N**, airborne pitching 830.13 N. *A man sitting still holds
nothing* — that is the leg the refuted point-mass model fails (it reports a
standing 228.6 N through the hands), and it is why these numbers can be trusted
to be zero when they should be zero.

### 1.2 The shape of your riding, in one paragraph

Most of the time the hands carry **200–450 N** — about a quarter to a half of the
rider's own weight (858 N), which is what a man's arms do on a bar. The 90th
percentile is **430–1,265 N** depending on the tape. Above that the distribution
does not taper, it **explodes**: p99 spans 445 N (a road cruise) to 26,872 N (a
crash), and the low-passed peak spans 461 N to 32,732 N. **Two orders of
magnitude, tape to tape, and it is real** — tape 23 is a cruise and tape 2
contains a crash. Nothing in this document is pooled across tapes and nothing
should be.

The other headline is the case mix. On the tapes with real bush riding in them
the rider spends **8–34 % of all substeps in CASE F — seat and boards carrying
nothing, hands the only contact on the machine.** On `sled_tape_6` that is 34.0 %,
more than the seated fraction. Those substeps are exactly the moments a
seat-first residual ordering reports 0 N. This is the mechanism your §7.3 failure
chain is about, and it is genuinely there in your riding, not a hypothetical.

### 1.3 The range for `grip_capacity_n`, with the reasoning

The dial is compared against the **low-passed** series (`lp`, τ = 0.1 s), not the
instantaneous one — `substeps` is a taped parameter and `dt` already differs 2×
between the game and a tape, so a threshold pinned to `inst` would silently stop
meaning what it meant the moment anyone retuned `substeps`.

**Hard floor — about 1,000 N.** Six tapes (10, 13, 18, 21, 23, 25) never leave
seated/seat-edge contact at all: their whole-run `lp` maxima are 460–642 N. Tapes
22 and 24 top out at 724 N and 796 N. **Any capacity at or below ~800 N throws him
off during a road cruise**, which is plainly wrong. 1,000 N is that ceiling plus a
modest margin. (Re-run for this document: `sled_tape_10`, `case mix: S 1.0000`,
`lp` p50 10.299 / p99 551.972 / p99.9 614.946 / **max 642.233** — 100 % seated,
never once off the seat, and the hands never see 650 N.)

**Soft ceiling — about 30,000 N.** Above that only `sled_tape_2` ever crosses, so
release becomes a once-in-27-rides event, i.e. effectively never.

Between those two the whole question is *how many of your 27 rides throw you*.
Derived from §2 below (count of tapes whose settle-excluded `lp` max crosses a
candidate — arithmetic on the measured rows, not a new run):

| candidate `grip_capacity_n` | tapes that would see at least one release |
|---:|---:|
| 1,000 N | 19 of 27 |
| 2,500 N | 17 of 27 |
| 5,000 N | 16 of 27 |
| **10,000 N** | **9 of 27** |
| 15,000 N | 4 of 27 |
| 20,000 N | 2 of 27 |
| 30,000 N | 1 of 27 |

**The band worth ruling in is 5,000–15,000 N.** That is where the curve turns:
below it a majority of rides end with him thrown, above it almost none do. Your
own gate wording — *"bucked up, not off; release rare and clearly earned"* —
lives in that knee. At 10,000 N the biggest tape in the set (`sled_tape_16`,
156,074 ticks) has its p99.9 at 8,966 N, so fewer than one substep in a thousand
is above the line on a 22-minute ride.

**Three things that bias the choice, stated so you can weigh them:**

* Every number here is a **rigid-attachment upper bound** (§4). When a compliant
  chain lands, the same riding produces *lower* loads, so a capacity dialled today
  will get *more* forgiving later. Dial low-ish now and expect to re-solve, which
  is what you already ruled for the deforming terrain.
* One measured hole (§3.1) means a small number of CASE-F substeps are
  **under**-reported by up to ~570 N, so a few peaks are slightly higher than
  printed. That pushes the same direction: a given candidate throws him slightly
  more often than this table says.
* **The table above counts TAPES, not EVENTS**, and that is a real gap — see §3.3.

---

## 2. THE PER-TAPE TABLE

Newtons, **through both hands combined**, over `ticks × 12` substeps. `ticks` is
on every row because a partial replay would be a partial distribution — on this
run every tape replayed in full, so every `ticks` equals the census figure. The
rider weighs **858.08 N**; a grip of 858 N is "both hands holding his whole body
weight". `*` = a tape outside your 19-tape roster.

`inst` = `grip_load_n` (per substep). `lp` = the τ = 0.1 s low-pass, **the series
the dial is set against**. Both columns are the **t ≥ 1 s window**, i.e. the spawn
settle excluded — the drop-onto-the-ground transient owns the whole-run `inst` max
on 6 of the 27 tapes and it is not riding. Whole-run figures are in
`docs/R4A_PHASE0_DATA.md` §2.1/§2.2 if you want them.

| tape | ticks | inst p50 | inst p99 | inst p99.9 | inst max | **lp p50** | **lp p99** | **lp p99.9** | **lp max** | case at peak | F % of run |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|:--:|---:|
| sled_tape_1 | 97,184 | 239.2 | 3,530.8 | 12,791.1 | 52,507.6 | 275.8 | 3,496.6 | 8,747.7 | **17,743.5** | S | 25.1 % |
| sled_tape_2 | 7,555 | 330.6 | 28,062.0 | 31,679.7 | 168,977.0 | 345.6 | 26,878.7 | 28,989.4 | **32,731.7** | F | 15.8 % |
| sled_tape_3 | 5,324 | 305.8 | 4,581.9 | 14,125.9 | 38,339.4 | 361.4 | 3,427.0 | 8,330.1 | **8,933.0** | F | 26.4 % |
| sled_tape_4 | 12,424 | 75.7 | 4,143.3 | 10,274.0 | 44,276.9 | 82.5 | 4,366.8 | 8,864.6 | **13,078.9** | F | 8.3 % |
| sled_tape_5 | 6,662 | 423.5 | 3,927.7 | 11,377.5 | 21,783.8 | 406.5 | 3,423.1 | 5,683.9 | **6,213.8** | S | 18.9 % |
| sled_tape_6 | 6,736 | 388.4 | 4,574.3 | 15,879.7 | 45,434.8 | 407.1 | 3,846.9 | 7,972.3 | **8,549.9** | F | **34.0 %** |
| sled_tape_7 | 5,958 | 321.0 | 1,576.0 | 2,878.4 | 3,872.5 | 325.4 | 1,554.5 | 2,310.9 | **2,335.1** | F | 2.7 % |
| sled_tape_8 | 113,662 | 273.4 | 5,703.7 | 12,868.4 | 140,124.3 | 292.9 | 4,938.0 | 10,456.5 | **22,055.2** | F | 17.4 % |
| sled_tape_9 | 15,501 | 265.9 | 4,435.9 | 6,880.7 | 40,494.1 | 282.2 | 3,745.4 | 5,127.3 | **5,213.8** | F | 16.5 % |
| sled_tape_10 | 7,843 | 10.3 | 596.9 | 931.6 | 1,004.4 | 10.3 | 552.0 | 614.9 | **642.2** | S | 0.0 % |
| sled_tape_11 | 1,288 | 416.2 | 2,893.7 | 3,705.4 | 7,218.5 | 422.7 | 2,438.2 | 2,590.6 | **2,592.3** | A | 16.8 % |
| sled_tape_12 | 5,554 | 341.9 | 2,582.9 | 10,464.5 | 20,688.4 | 362.5 | 2,530.9 | 6,019.6 | **6,294.1** | F | 10.9 % |
| sled_tape_13 | 1,312 | 302.7 | 591.6 | 772.6 | 784.8 | 316.5 | 472.9 | 509.8 | **516.1** | S | 0.0 % |
| sled_tape_14 | 129,566 | 294.3 | 4,143.9 | 9,923.1 | 85,311.2 | 319.6 | 3,652.8 | 7,929.4 | **12,208.2** | F | 14.8 % |
| sled_tape_15 † | 76,820 | 238.6 | 3,770.5 | 9,516.1 | 38,911.4 | 299.5 | 3,222.2 | 7,022.1 | **10,611.3** | S | 16.4 % |
| sled_tape_16 | 156,074 | 239.8 | 5,266.1 | 11,599.0 | 100,737.7 | 274.8 | 4,734.7 | 8,966.3 | **18,535.4** | B | 10.4 % |
| sled_tape_17 | 2,215 | 413.0 | 1,218.1 | 4,823.9 | 5,063.7 | 399.6 | 956.2 | 2,312.7 | **2,364.0** | S | 4.2 % |
| sled_tape_18 | 1,275 | 293.7 | 566.5 | 777.5 | 797.4 | 294.3 | 490.7 | 510.9 | **513.6** | S | 0.0 % |
| sled_tape_19 | 5,381 | 347.7 | 7,493.1 | 13,737.9 | 22,678.7 | 369.3 | 5,291.4 | 8,418.4 | **8,513.1** | F | 10.2 % |
| sled_tape_20 * | 6,438 | 404.7 | 6,329.8 | 19,741.6 | 27,199.9 | 412.1 | 5,953.8 | 10,010.5 | **10,983.4** | A | 26.3 % |
| sled_tape_21 * | 6,035 | 162.6 | 584.3 | 787.2 | 969.4 | 171.3 | 459.9 | 598.6 | **635.0** | S | 0.0 % |
| sled_tape_22 * | 2,396 | 129.1 | 878.3 | 1,363.1 | 1,594.2 | 142.3 | 709.8 | 723.7 | **723.7** | B | 2.3 % |
| sled_tape_23 * | 1,163 | 25.2 | 446.2 | 550.8 | 614.3 | 24.5 | 445.8 | 458.5 | **460.6** | S | 0.0 % |
| sled_tape_24 * | 4,184 | 25.8 | 798.4 | 843.3 | 1,086.6 | 88.9 | 764.5 | 792.6 | **796.4** | S | 0.2 % |
| sled_tape_25 * | 2,924 | 137.1 | 476.6 | 760.5 | 922.9 | 140.9 | 445.7 | 455.1 | **462.9** | S | 0.0 % |
| sled_tape_26 * | 51,843 | 370.7 | 6,495.7 | 14,045.7 | 80,112.1 | 383.8 | 5,931.6 | 10,238.4 | **14,622.3** | A | 19.9 % |
| sled_tape_27 * | 9,840 | 432.0 | 4,557.3 | 7,528.7 | 22,235.9 | 431.5 | 3,669.4 | 4,835.9 | **6,253.3** | S | 15.1 % |

† `sled_tape_15`'s settle exclusion is a **no-op** and the doc says so: its
`rec.tick` is the recorder's session-global counter, not an index into its own
records, so `tick × dt` never drops below 1 s. Its rows above are its whole-run
rows. This was caught by a self-check (the excluded count is exactly 1,440
substeps on the other 26 tapes and exactly 0 on this one), not by eye. It is the
same numbering artefact the census saw in its rolled tick (135,268 against 76,820
replayed ticks) and it is **not** a divergence.

**The seat/board/hand split at the peak** — the reason the rod model exists — is
`docs/R4A_PHASE0_DATA.md` §4. Short version: **the seat carries literally zero at
the peak on 12 of the 27 tapes** (CASE F on 10, CASE B on 2), and in CASE F the
wrist couple `M_h` is non-zero, which is *"he pivots up over the bars"* as an
actual quantity. A point-mass rider can carry no moment at all and would report
zero grip on exactly those substeps.

---

## 2.5 ★ THE CASE-F FIX, AND THE RE-RUN THAT SCORED IT (2026-08-25, commit `8c306e610`)

§3.1 below records the hole as **found but unfixed**, and says in as many words
that *"nobody re-ran the tapes with a fix, so it is UNKNOWN whether any published
percentile in §2 moves."* **It is no longer unknown.** The fix landed and the
corpus was re-run. This section is the diff. §3.1 is left standing as written —
it is the record of the finding, and rewriting it would erase the evidence trail.

### 2.5.1 What the fix was

`if (V <= 0.0) -> CASE F` was a proxy for "nothing below carries". Moment balance
about the rider CG (`0 = K + dz_h·H_y + z_m·N_m`) with `N_m = V − H_y` gives the
pinned-wrist family

```
N_m(z_m) = W / (z_m − dz_h),      W = −(K + dz_h·V)
```

and every support station is aft of the grips (`dz_h = −0.626` is forward-most),
so `z_m − dz_h > 0` and **`N_m ≥ 0` ⟺ `W ≥ 0`, for either sign of V.** The
existing `N_b < 0` re-drop is algebraically this same test — the code already
asked the right question on the `V > 0` side and never asked it below zero. Below
zero the admissible family is a continuum, so the reported member is the **seat**
one: minimal `|H_y|` (longest arm, 0.854 m against the boots' 0.167 m), which is
§4.3's own closure, and the unique continuous extension of the `V → 0⁺` CASE A
limit. **The `V > 0` path is byte-untouched** — deliberately, so the diff below is
sharply falsifiable.

### 2.5.2 The prediction, recorded BEFORE the re-run

Written into `8c306e610`'s message before the corpus ran, so the run could refute
it rather than confirm a story told afterwards:

| # | prediction |
|---|---|
| 1 | the six zero-F tapes (10, 13, 18, 21, 23, 25) stay **bit-identical** |
| 2 | their `lp` maxima stay in **460–642 N** |
| 3 | **tape 6 is the largest case-mix mover**: F 34.0 → ≈31.8 %, A 32.0 → ≈34.2 % |
| 4 | `lp` maxima on the violent tapes move **up by O(10 N)**, never hundreds |
| 5 | **no grip number anywhere goes DOWN** |

### 2.5.3 The result — all five held

| # | outcome |
|---|---|
| 1 | ✅ all six report `F 0.0000`; tape 10 `lp` max **642.23** against the recorded 642.233, tape 23 **460.63** against 460.634 |
| 2 | ✅ 460.63 / 462.91 / 513.55 / 516.09 / 635.03 / 642.23 |
| 3 | ✅ measured **F 0.3186, A 0.3416** — within 0.06 of a percentage point of a number written down beforehand |
| 4 | ✅ tape 2 `lp` max 32,731.7 → **32,739.30**, +7.6 N |
| 5 | ✅ no decrease anywhere |

Prediction 3 is the one that carries the weight. The fix's whole mechanism is
*"admissible CASE-F substeps are reclassified to CASE A"*, and F fell by exactly
what A rose by, on the tape predicted to move most. That is the mechanism
confirmed, not merely a green result.

**Reproducibility, by accident and then on purpose:** two independent processes
ran the corpus concurrently (a launcher script that had not died as assumed).
Every one of the 31 tapes therefore ran **twice, in separate processes**, and the
62 result rows collapse to exactly **31 unique lines** — byte-identical pairs.

### 2.5.4 ★ THE HONEST HEADLINE: the published percentiles barely move

**The §2 table is essentially unchanged, and reporting that as the result would
be a true answer to a question nobody asked** — the sixth instance of the trap
this repo has paid for five times. The measured under-reports are ≤ 572 N and
instantaneous, against `lp` maxima in the thousands to tens of thousands, so they
cannot shift a percentile.

What the fix actually buys is **integrity at the `V ≈ 0` crossings**, which the
percentiles never described. At the boundary the old rule stepped **14.04 N
between adjacent samples against a 0.35 N budget** — a jump where the physics
allows only a kink. `V ≈ 0` is every crest and every fall-away, which is where
exceedance EVENTS live, and events are what Chad's throw ruling
(`docs/R4A_THROW_RULING_20260825.md`) now rests on. A discontinuity that snaps
grip toward zero mid-excursion splits one event into two or hides one entirely.
**So the value of this fix is invisible in §2 by construction and would have been
mis-scored by a table diff alone.**

### 2.5.5 Two corrections to this document's own scope

1. ★ **The corpus is 31 tapes, not 27.** Four more were recorded after §3.5 was
   written. Every count in this file, in the handoff, and in all four consult
   reports says 27. The new tapes are **not** gentle: tape 29 is 24.4 % CASE F,
   tape 31 is 25.7 %. §3.5's *"§7.7's 'would break all 19 tapes' is now 'all 27
   tapes'"* becomes **all 31**. The pin roster is still positional and still must
   not be extended.
2. ★ **CASE F is more common across the full corpus than §1.2's "8–34 %" band
   suggested.** Seven of the 31 tapes sit above 24 %: tape 6 (31.9 %), tape 3
   (26.3 %), tape 31 (25.7 %), tape 20 (25.2 %), tape 29 (24.4 %), tape 1
   (24.1 %), tape 26 (18.8 %). Under Chad's ruling that band is the **superman
   approach** — hands-only contact is the last ATTACHED state before release, not
   a symptom of losing the bar — so roughly a quarter of the bush riding is
   already in the regime stage 3 of the ladder describes.

### 2.5.6 What the re-run did NOT do

- It did **not** recompute the full percentile tables in §2; it captured case mix
  and the `inst`/`lp`/`M_h` maxima per tape. A full table regeneration is cheap
  (~seconds per small tape) and has not been run.
- It did **not** measure exceedance EVENTS. §3.3's gap is still open, and it is
  the number the felt call needs.
- The `grip_capacity_n` band in §1.3 is **unchanged by this fix** and remains
  Chad's to rule. Nothing here self-passes a dial.

---

## 3. WHAT THE RED TEAM REFUTED AND CONFIRMED

Three independent fresh contexts re-derived this work from the artefacts. **One
of them found a real hole. It is first.**

### 3.1 ★ REAL HOLE — the CASE-F entry test is not the statement the spec makes, and it can UNDER-report

> ✅ **FIXED 2026-08-25 in commit `8c306e610`; the corpus was re-run and the diff
> is §2.5.** This section is left exactly as it was written — it is the record of
> the finding and of what was and was not known at the time, and editing it would
> erase the evidence trail. Read its "nobody re-ran the tapes, so it is UNKNOWN
> whether any published percentile moves" as **answered in §2.5.3**: they barely
> move, and §2.5.4 explains why that is the wrong place to look for the fix's
> value. The line numbers below are pre-fix and no longer resolve.

`tools/sled_probe.cpp:2549` reads `if (V <= 0.0) { case_free(); }`, matching
`docs/R4A_PHASE0_MODEL.md:412` (`STEP 1. if V <= 0 -> CASE F`). But §4.4 justifies
CASE F as *"nothing below carries"*, and `V <= 0` is **not** that statement — the
seat is unilateral (`N_s ≥ 0`), so a downward net vertical demand does not forbid a
non-negative seat normal. Where a pinned-wrist solution with `N_s ≥ 0` still
exists, the model discards it, zeroes both vertical contacts and dumps the pitch
balance into `M_h`, which `grip_load_n` does not contain.

Measured by the red-team lens, with a temporary audit added to `r4a::solve` and
then removed (`git status` is unchanged; the audit is **not** in the tree — I
checked, `grep REDTEAM tools/sled_probe.cpp` is empty):

```
tape 3  : F substeps 16,850, pinned-wrist admissible    49 (0.29%); worst under-report 572.5 N (403.4 -> 975.9)
tape 6  : F substeps 27,514, pinned-wrist admissible 1,760 (6.40%); worst  45.7 N
tape 27 : F substeps 17,837, pinned-wrist admissible   445 (2.49%); worst 506.2 N (4,131.4 -> 4,637.6)
tape 11 : F substeps  2,591, pinned-wrist admissible     8 (0.31%); worst  57.6 N
```

There is also a **jump discontinuity at `V = 0`** — a constructed sweep with
`α_x = −10 rad/s²` collapses the reported grip from 82.8 N to 0.9 N across a 1.75 N
(0.2 % of body weight) change in the vertical demand. `V ≈ 0` is every crest and
every fall-away, which is the regime this rung is about.

**Damage, stated straight:** this breaks `docs/R4A_PHASE0_MODEL.md:570`'s headline
claim that *every* number is a conservative upper bound. In these substeps it
reports **less** than a statically admissible solution under the model's own
assumptions. **What it does NOT do** — and this is the honest limit of what was
measured — **nobody re-ran the 27 tapes with the fix in, so it is unknown whether
any published `lp` percentile or peak in §2 actually moves.** The measured
under-reports (≤ ~572 N) are small against the `lp` maxima they sit near, and they
are instantaneous. **This is the first item for Phase 1: replace the `V <= 0` test
with a proper admissibility check, re-run all 27, and diff the tables.** Until
then, read §2 as the load *at least* this large in the F regime.

### 3.2 CONFIRMED — the numbers reproduce, cell for cell

A second lens re-ran **all 27 tapes** and diffed every cell of every table
programmatically. **Zero substantive mismatches.** The only 23 diffs are exact
half-way rounding ties (418.95 → "418.9"). Independently re-verified: `first_div
tick = -1` on all 27; `rolled tape == replay` on all 27; the derivation and
self-check block **byte-identical across all 27 runs** — which is what proves the
mid-run metric fix did not leave a mixed-metric closure table; the settle
exclusion is exactly 1,440 substeps on 26 tapes and exactly 0 on tape 15.

I re-ran one row myself, with the binary as it stands now, and it reproduces to
the digit: `sled_tape_23`, `first_div tick -1`, 1,163 ticks / 13,956 substeps,
`lp` (t ≥ 1 s) p50 24.480 / p99 445.761 / p99.9 458.475 / **max 460.634**, `inst`
max 614.342, `rider_cg MODEL = (+0.00033, +0.81774, −0.35734)`, at rest
`grip = 0.00 N` with `N_s +573.88 / N_b +284.20`. And `sled_tape_6`, the
CASE-F-heavy one the headline leans on: `case mix: S 0.3308 A 0.3198 B 0.0090
**F 0.3404**`, `lp` max **8,549.857 N**, `inst` (t ≥ 1 s) p50 388.435 / p99
4,574.280 / p99.9 15,879.657 / max 45,434.771. And `sled_tape_2`, the crash that
sets the top of the range: `lp` (t ≥ 1 s) p50 345.581 / p99 26,878.675 / p99.9
28,989.353 / **max 32,731.650**, `rolled tape=2611 replay=2611`. And
`sled_tape_16`, the largest in the set: 156,074 ticks, `case mix: S 0.7868
A 0.0882 B 0.0209 F 0.1041`, `lp` p50 274.817 / p99 4,734.709 / p99.9 8,966.332 /
max 18,535.385, closure over **18 of 156,074 ticks**. Every figure the same as §2.

That lens also caught the instrument **moving under the document**: the probe
source was rewritten three times during review and the doc's reproduce command was
a rebuild rather than a no-op, and two §2.3/§4.1 cells were wrong at first read
(tape 23's `lp` p99/p99.9, and a tape-23 ground speed pasted from the wrong peak
row) and were corrected live. **Both are now correct in the data doc, and the tree
is currently consistent**: I re-checked at write time — `ninja -C build-play -d
explain -n seads_sled_probe` says *"no work to do"*, so
`build-play/seads_sled_probe.exe` (04:39:42) is current against
`tools/sled_probe.cpp` (04:37:16). The lesson stands and is worth carrying: **a
provenance section that names a command instead of a commit + binary hash is
unfalsifiable the moment either moves**, and both moved inside 30 minutes.

### 3.3 THE GAP NEITHER LENS CLOSED — tapes are not events

§1.3 counts **tapes that would see at least one release**. What actually decides
whether release feels *rare and earned* is **how many separate crossings** a ride
contains — a 1.3-second total above the line could be one honest crash or forty
flickers, and the release latch is one-way. **An exceedance-EVENT count per tape
(with a hysteresis band) was not measured and is not in this data.** It is cheap —
one more accumulator in the same probe — and it is the number I would want before
ruling the dial. Phase 1.

### 3.4 CONFIRMED — the shipped path did not move

A third lens re-derived the firewall claim five independent ways and it
**survives**:

* `git diff --stat sim/` = `sled.cpp 17 +`, `sled.h 19 +`, **zero deletions, zero
  modified lines**. Both writes are strictly inside `if (dbg && !dbg->substeps.empty())`
  at `sim/sled.cpp:1656`; there are only 3 `if (dbg` sites in the file and no
  `continue`/`break` between the substep's push and the new write, so
  `substeps.back()` is always this substep's record.
* `R` is `const`, assigned once per substep and never reassigned, so
  `(a_body, omega_body, alpha_body)` really is one consistent instant.
* The shipped path calls `step_sled` with **5 arguments** at `app/main.cpp:3336` —
  `dbg` defaults null, and it is the only `step_sled` call outside `sim/`.
* Bit-exact replay holds with the sink **NULL** (`tape_360_chad_repro`, 961 ticks,
  `first_div -1`) **and with the sink NON-NULL for every substep** (the probe's own
  `griphold` run over the same golden, and over `sled_tape_23`). The stronger of
  those two nobody had asked for.
* `sled_debug_sink_is_write_only`: 14,403 assertions, **14,402 pass**. The single
  red is `REQUIRE(saw_release_under_one)` at `test/unit/test_sled.cpp:3456` — the
  **non-vacuity** check on the GI4 release band, a function of the fixture's
  trajectory only. Every bit-identity assertion passes, including
  `assist_nm 105.02708392324623787 == 105.02708392324623787`.

**The gate — re-run once more against the tree exactly as it is being left, and
this is that run, not a carried-over number:**

```
99% tests passed, 5 tests failed out of 1560
Total Test time (real) = 508.96 sec

  54 - probe P-F: the relentless raider keeps the pump and shoots back
 821 - sled_slides_before_it_tips_on_flat_snow
 822 - sled_grip_ceiling_stays_below_the_tip_threshold
 860 - sled_assist_reference_plane_is_load_weighted
 863 - sled_debug_sink_is_write_only
```

**Set equality with the five pre-existing debts named in
`docs/SESSION_HANDOFF_20260825_r3hands.md` §2.** No sixth red; no expected red
missing (which would also have been worth reporting). One ctest, one build dir.

### 3.5 CONFIRMED — the corpus, and a correction to the brief

`build-play/` holds **27** tapes, not 19. Tapes 20–27 were recorded on
`main@…-97-g4be931ce7` and `sandbox/r3-hands` during and after the R3-HANDS rung.
All 27 verify their signature and replay bit-exactly, **including the ten carrying
a `-dirty` recorder tag** — that suffix described the recorder's working tree, not
the tape's fidelity. Tape 16 (1.17 GB, 156,074 ticks) loaded whole and replayed in
2m22s with no thrash. Every tape reports the same format envelope: dt = 1/120,
substeps = 12, `hf_faceted=1`, `depth_base=0.850`, `flight-kernel-v12-2026-07-30`.

Spot-checked once more at write time, on the tree as it is being left — tapes 13,
18, 23, 11 and 22 all print `sig: VERIFIED`, replay `1312/1312`, `1275/1275`,
`1163/1163`, `1288/1288`, `2396/2396` ticks, `VERDICT: bit-exact`, and tape 11's
roll latches at tick 940 on both sides.

**This changes one line of the R4a plan:** §7.7's *"would break all 19 tapes"* is
now *"all 27 tapes"*. The pin roster is still positional and still must not be
extended.

---

## 4. THE BIAS, PLAINLY

**This instrument computes the force needed to hold the rider RIGIDLY. Every
number in §2 is a conservative upper bound** — except in the CASE-F substeps §3.1
identifies, where it can be an under-report.

Real knees, hips, elbows and shoulders comply, and that compliance is not
hypothetical — it already ships. `render::rider_absorb()`
(`render/rider_pose.cpp:109`) returns a measured, saturating absorb fraction from
the suspension's own compression and rate, and this model gives it **zero travel**.
**When a compliant chain exists, the same riding will produce visibly smaller
numbers, and any `grip_capacity_n` dialled today will become more forgiving.**
Plan on re-solving it — the same way you already ruled it must be re-solved when
the deforming terrain lands.

Three more limits, on the record:

1. **It is not a grip-strength table.** 168,977 N on tape 2 is not a claim that a
   man holds 169 kN; it is what a *rigid* rider would need. `grip_capacity_n` is a
   felt call **in this instrument's units** and must never be compared to a
   physiological figure.
2. **It is blind to roll and to any side-throw.** The planar sagittal reduction is
   your own R4a deferral. A lateral buck-off appears nowhere in this data.
3. **`I_pitch` is a slender-rod lower bound** (bounded at ≤ 14.2 % of `I_pitch`),
   and it runs the **opposite** way to the conservatisms above.

The reconstruction error bars were measured, not asserted, and they are small
where it matters: the lean bracket's worst case on the `lp` series is **34.95 N on
tape 26 against that tape's 14,622 N max — 0.24 %**. The orientation-closure error
reads ~π rad on 14 tapes and that is **not** a broken reconstruction: the worst
breadth is 23 offending ticks out of 113,662 (0.03 %), and on all 14 the first
offender is *after* that tape's roll tick — crash resets, exact on both sides. A
broken reconstruction would fail on every tick.

---

## 5. WHAT IS OPEN FOR PHASE 1+

**From this run:**

1. **★ Fix the CASE-F entry test (§3.1) and re-run all 27.** Replace `V <= 0` with
   a real admissibility check, then diff the §2 tables. Until that diff exists,
   the upper-bound claim has a measured exception.
2. **★ Exceedance EVENTS, not just percentiles (§3.3).** Per tape, with hysteresis:
   how many separate crossings of a candidate capacity, and how long each lasts.
   That is the number the felt call actually needs.
3. **Provenance by commit + binary, not by command (§3.2).** Whatever ships next
   should stamp the probe's own git describe into its header line.

**Carried forward from `docs/SESSION_HANDOFF_20260825_r3hands.md` §6 — these two
are structural and must survive into the build:**

4. **★★★ Superman is impossible in the frame the solver runs in.** `trail_chain`
   is Verlet in **sled model space** (`trail_chain.h:126`), where the machine's
   linear and angular acceleration are identically zero — only aero drag can lift
   the chain, and the flown `drag_k = 0.153/m` is a *scarf ribbon* number (≈0.0053
   for an 87.5 kg man, **29× smaller**). It needs **frame pseudo-forces** plus
   `a_body`/`alpha_body` plumbed through to render via `SledRig`. **This Phase 0
   already put both of those on the debug record** (`sim/sled.h` `a_body[3]`,
   `alpha_body[3]`) — that is the half of the plumbing that exists. **Do not** run
   the chain in world space (float precision at R = 15 km) and **do not**
   re-difference velocity in render.
5. **★★ The missing FIRST link of §7.4 — throttle → idle on release.** Settled by
   measurement: the drivetrain needs no work (idle rpm, clutch decouple below
   3.25 m/s, free-coast and engine brake all exist, and no-creep falls out by
   arithmetic) — **but the first link of the chain does not exist.**
   `sim/sled.cpp:241` gates on `rolled`, not on rider attachment, and the
   spring-return is the W key. Nothing releases the throttle when his hands do.
6. Lateral lean (`lean_l`/`lean_r`) is still one rigid pelvis translation
   (`sled_model.cpp:1504`); the lever throws are yours to rule
   (§4 of the R3-HANDS handoff — the throttle's 12° is **not** measured).

---

## 6. EXACTLY WHAT CHANGED IN THE TREE

**Branch `sandbox/r4a-phase0` @ `8852cf7d7`. Uncommitted, unpushed, left in the
tree for you.** `git diff --stat`:

| file | change | why it is safe |
|---|---|---|
| `sim/sled.h` | **+19, −0** | Adds exactly two members to `SledDebugSubstep`: `double a_body[3]`, `double alpha_body[3]`, plus their comment. No `SledParams` value, no kernel default, no other struct. |
| `sim/sled.cpp` | **+17, −0** | One block, entirely inside `if (dbg && !dbg->substeps.empty())`, immediately above `s.angular_vel += alpha_body * h;`. It copies locals that already existed one line above. **Zero deletions, zero modified lines.** The sink is null on the shipped path. |
| `CMakeLists.txt` | +12, −2 | `seads_sled_probe` only: links `seads_render_core` (raylib-free by its own layering rule) so the instrument re-derives the segment table from the shipped GLB instead of hard-coding a CG, plus an include dir and `SEADS_ASSET_DIR`. No other target touched. |
| `tools/sled_probe.cpp` | +765 | The `griphold` subcommand — the whole instrument. Tool code; links `seads_sim`, ships in no game binary. |
| `generated/graph/graph.json`, `digest/{sim,render,tools}.md` | +38, −16 | Regenerated in step, per the standing law. |
| `docs/R4A_PHASE0_MODEL.md` | untracked, new | The derivation. |
| `docs/R4A_PHASE0_DATA.md` | untracked, new | The full 27-tape distribution. |
| `docs/R4A_PHASE0_FINDINGS.md` | untracked, new | This file. |
| `docs/SESSION_HANDOFF_20260825_r3hands.md` | modified, **append only** | A new §7 pointing at this run. Nothing above it was edited. |

**NOT touched, verified by `git status`:** `render/`, `control/`, `assets/`,
`test/` (so no golden and no tape PIN roster), `indy650.blend`, the GLB, any
`SledParams` value, any kernel default, any `.sledtape`. No
`-ffast-math`/`-Ofast`/`-ffp-contract` anywhere in `CMakeLists.txt`, so the added
computation has no route to perturb the integrator even in principle — and the
27/27 bit-exact replay is the empirical proof that it did not.

**Nothing was committed. Nothing was pushed.**

Reproduce any row:

```
cmake --build build-play --target seads_sled_probe
cd build-play && ./seads_sled_probe.exe griphold sled_tape_<N>.sledtape 1e9 1e9
```

Re-verify a tape is bit-exact:

```
./build/seads_sled_probe.exe tape build-play/sled_tape_<N>.sledtape     # exit 0 = bit-exact
```

Gate (one ctest per build dir at a time):

```
cmake --build build --target seads_tests && cd build && ctest -j4
```
