# R4a PHASE 0 - THE DATA

**What this is.** The per-tape distribution of the rider grip load produced by
`probe_grip_hold`, the instrument specified in `docs/R4A_PHASE0_MODEL.md`. This is
the number `grip_capacity_n` gets dialled against. **Chad rules that dial by eye.**
Nothing here proposes a value.

**Provenance.** Every number below came out of a run done for this document - none
is estimated, carried over, or inferred. Branch `sandbox/r4a-phase0`, probe built
into `build-play/` (RelWithDebInfo), asset `assets/sled/indy650.glb`.
Reproduce any row with:

```
cmake --build build-play --target seads_sled_probe
cd build-play && ./seads_sled_probe.exe griphold sled_tape_<N>.sledtape 1e9 1e9
```

(the two `1e9` arguments are the per-tick trace window; setting it past the end of
the tape suppresses the trace and leaves the summary + distribution blocks.)

---

## 0. THE THREE READING RULES

1. **PER TAPE. NEVER POOLED.** There is no ensemble row in this document and one
   must not be added. This repo's probe-noise-floor law was paid for once already:
   a metric spread 4-39 across arms that could not differ, and a famous baseline
   turned out to be a lucky ensemble. The spread across these 27 tapes is over two
   orders of magnitude and it is *real* - tape 10 is a cruise and tape 2 contains a
   crash. A mean of the two describes neither.
2. **`ticks` IS PRINTED ON EVERY ROW.** A replay halts at the first divergence, so a
   partial replay is a partial distribution and a percentile over 1,163 ticks is not
   the same claim as one over 156,074. * On this run **all 27 tapes replayed in
   full**: `first_div tick` is `-1` on every one, and every `ticks` below equals the
   census figure exactly. No distribution here is truncated.
3. **THE DIAL IS COMPARED AGAINST `lp`, NOT `inst`.** `docs/R4A_PHASE0_MODEL.md` S5.2:
   `inst` is sampled once per substep, and `substeps` is a **taped** parameter while
   `dt` already differs 2x between the game (1/120) and a tape (1/60). A threshold
   pinned to `inst` would silently stop meaning what it meant the moment anyone
   retuned `substeps`, and nothing would go red. `inst` is published here for
   attribution only.

---

## 1. THE INSTRUMENT'S OWN CHECKS, THIS RUN

Printed identically at the head of every one of the 27 runs - the model is rebuilt
from the GLB each time, so these are 27 independent reproductions, not one cached:

```
rider_cg MODEL = (+0.00033, +0.81774, -0.35734)   <- reproduces sim/sled.h K2 to the digit
I_pitch = 7.09063 kg m^2   k = 0.2847 m           (I_own 14.2% / m*d^2 85.8%)
support interval [dz_b, dz_s] = [-0.45939, +0.22750]  span 0.68688 m

(i)   AT REST            case S  grip     0.00 N   N_s +573.88  N_b +284.20
(ii)  PURE 3g BUMP       case S  grip     0.00 N   N_s +2295.53 N_b +1136.80
(iii) AIRBORNE PITCHING  case F  grip   830.13 N   M_h +18.30 N m
```

Spec S7.4 asks for grip 0 / N_s 573.9 / N_b 284.2 at rest, grip 0 on the symmetric
bump, and grip 830.1 N with M_h +18.32 N.m airborne. All three hit. **The at-rest
leg matters most: a man sitting still holds nothing.** That is the leg that fails
the refuted seat-first model, which reports a standing 228.6 N through the hands.

---

## 2. THE DISTRIBUTIONS

Units are **newtons**, through **both hands combined**. Percentiles are exact
nearest-rank order statistics over the **sorted per-substep sample** - no binning,
no interpolation. `substeps` = ticks x 12 on every tape (`p.substeps = 12`, taped).

For scale: the rider weighs **858.08 N** (87.5 kg x 9.80665). A `grip` of 858 N is
"both hands are holding his entire body weight."

### 2.1 INSTANTANEOUS peak load - `grip_load_n`

| tape | ticks | substeps | p50 | p90 | p99 | p99.9 | max |
|---|---:|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 97,184 | 1,166,208 | 239.0 | 910.1 | 3,528.6 | 12,783.3 | 52,507.6 |
| **sled_tape_2** | 7,555 | 90,660 | 328.1 | 830.3 | 28,017.1 | 31,678.9 | 168,977.0 |
| **sled_tape_3** | 5,324 | 63,888 | 287.3 | 1,034.5 | 4,520.1 | 14,044.3 | 38,339.4 |
| **sled_tape_4** | 12,424 | 149,088 | 75.1 | 592.5 | 4,138.9 | 10,220.9 | 44,276.9 |
| **sled_tape_5** | 6,662 | 79,944 | 418.9 | 1,093.5 | 3,917.3 | 11,263.4 | 21,783.8 |
| **sled_tape_6** | 6,736 | 80,832 | 380.5 | 1,009.5 | 4,521.6 | 15,863.7 | 45,434.8 |
| **sled_tape_7** | 5,958 | 71,496 | 316.3 | 615.2 | 1,569.0 | 2,861.9 | 3,872.5 |
| **sled_tape_8** | 113,662 | 1,363,944 | 273.2 | 849.9 | 5,700.6 | 12,864.4 | 140,124.3 |
| **sled_tape_9** | 15,501 | 186,012 | 264.7 | 938.8 | 4,426.1 | 6,862.8 | 40,494.1 |
| **sled_tape_10** | 7,843 | 94,116 | 10.3 | 449.7 | 598.3 | 937.9 | 1,126.1 |
| **sled_tape_11** | 1,288 | 15,456 | 373.8 | 506.8 | 2,859.6 | 3,679.3 | 7,218.5 |
| **sled_tape_12** | 5,554 | 66,648 | 340.6 | 659.0 | 2,573.7 | 10,461.9 | 20,688.4 |
| **sled_tape_13** | 1,312 | 15,744 | 300.4 | 457.2 | 591.0 | 778.0 | 1,015.2 |
| **sled_tape_14** | 129,566 | 1,554,792 | 294.1 | 833.5 | 4,141.8 | 9,922.8 | 85,311.2 |
| **sled_tape_15** | 76,820 | 921,840 | 238.6 | 795.3 | 3,770.5 | 9,516.1 | 38,911.4 |
| **sled_tape_16** | 156,074 | 1,872,888 | 239.7 | 683.6 | 5,263.2 | 11,597.9 | 100,737.7 |
| **sled_tape_17** | 2,215 | 26,580 | 394.2 | 598.7 | 1,159.7 | 4,818.7 | 5,063.7 |
| **sled_tape_18** | 1,275 | 15,300 | 292.9 | 439.0 | 568.9 | 892.7 | 1,440.6 |
| **sled_tape_19** | 5,381 | 64,572 | 330.1 | 824.7 | 7,315.5 | 13,726.3 | 22,678.7 |
| **sled_tape_20 *** | 6,438 | 77,256 | 395.6 | 1,235.1 | 6,297.9 | 19,444.3 | 27,199.9 |
| **sled_tape_21 *** | 6,035 | 72,420 | 161.4 | 443.5 | 589.8 | 792.2 | 1,382.1 |
| **sled_tape_22 *** | 2,396 | 28,752 | 129.0 | 448.3 | 872.1 | 1,359.1 | 1,594.2 |
| **sled_tape_23 *** | 1,163 | 13,956 | 25.2 | 431.9 | 465.6 | 639.6 | 1,085.2 |
| **sled_tape_24 *** | 4,184 | 50,208 | 124.5 | 436.7 | 798.9 | 843.9 | 2,792.4 |
| **sled_tape_25 *** | 2,924 | 35,088 | 137.0 | 443.2 | 477.4 | 763.0 | 922.9 |
| **sled_tape_26 *** | 51,843 | 622,116 | 369.9 | 1,013.7 | 6,488.4 | 14,025.0 | 80,112.1 |
| **sled_tape_27 *** | 9,840 | 118,080 | 432.1 | 775.1 | 4,536.1 | 7,523.2 | 22,235.9 |

`*` = an EXTRA tape, outside the 19-tape roster.

### 2.2 LOW-PASSED load - `grip_load_lp` (tau = 0.1 s, the house `slew_toward` shape)

**This is the series the dial is set against.**

| tape | ticks | substeps | p50 | p90 | p99 | p99.9 | max |
|---|---:|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 97,184 | 1,166,208 | 275.4 | 873.6 | 3,494.2 | 8,746.5 | 17,743.5 |
| **sled_tape_2** | 7,555 | 90,660 | 338.8 | 1,022.4 | 26,872.0 | 28,988.3 | 32,731.7 |
| **sled_tape_3** | 5,324 | 63,888 | 357.6 | 1,182.9 | 3,381.5 | 8,306.8 | 8,933.0 |
| **sled_tape_4** | 12,424 | 149,088 | 82.5 | 553.7 | 4,357.2 | 8,774.8 | 13,078.9 |
| **sled_tape_5** | 6,662 | 79,944 | 403.8 | 1,245.4 | 3,416.4 | 5,666.5 | 6,213.8 |
| **sled_tape_6** | 6,736 | 80,832 | 400.3 | 1,003.0 | 3,802.9 | 7,951.8 | 8,549.9 |
| **sled_tape_7** | 5,958 | 71,496 | 321.1 | 603.8 | 1,541.8 | 2,309.4 | 2,335.1 |
| **sled_tape_8** | 113,662 | 1,363,944 | 292.6 | 913.0 | 4,935.2 | 10,456.0 | 22,055.2 |
| **sled_tape_9** | 15,501 | 186,012 | 281.1 | 964.2 | 3,734.3 | 5,126.6 | 5,213.8 |
| **sled_tape_10** | 7,843 | 94,116 | 10.3 | 441.4 | 551.9 | 614.1 | 642.2 |
| **sled_tape_11** | 1,288 | 15,456 | 402.9 | 554.0 | 2,436.0 | 2,590.6 | 2,592.3 |
| **sled_tape_12** | 5,554 | 66,648 | 358.1 | 592.8 | 2,518.9 | 6,008.1 | 6,294.1 |
| **sled_tape_13** | 1,312 | 15,744 | 302.6 | 448.5 | 470.7 | 509.8 | 516.1 |
| **sled_tape_14** | 129,566 | 1,554,792 | 319.4 | 866.0 | 3,651.1 | 7,928.9 | 12,208.2 |
| **sled_tape_15** | 76,820 | 921,840 | 299.5 | 788.1 | 3,222.2 | 7,022.1 | 10,611.3 |
| **sled_tape_16** | 156,074 | 1,872,888 | 274.7 | 680.0 | 4,733.7 | 8,964.0 | 18,535.4 |
| **sled_tape_17** | 2,215 | 26,580 | 394.1 | 581.9 | 955.0 | 2,304.5 | 2,364.0 |
| **sled_tape_18** | 1,275 | 15,300 | 292.6 | 431.1 | 488.1 | 510.4 | 513.6 |
| **sled_tape_19** | 5,381 | 64,572 | 360.8 | 830.9 | 5,214.5 | 8,416.6 | 8,513.1 |
| **sled_tape_20 *** | 6,438 | 77,256 | 404.9 | 1,264.7 | 5,936.2 | 9,957.1 | 10,983.4 |
| **sled_tape_21 *** | 6,035 | 72,420 | 169.8 | 434.7 | 457.2 | 597.5 | 635.0 |
| **sled_tape_22 *** | 2,396 | 28,752 | 141.7 | 436.9 | 707.3 | 723.7 | 723.7 |
| **sled_tape_23 *** | 1,163 | 13,956 | 24.5 | 368.8 | 445.7 | 458.3 | 460.6 |
| **sled_tape_24 *** | 4,184 | 50,208 | 99.6 | 461.3 | 776.4 | 1,549.9 | 1,766.9 |
| **sled_tape_25 *** | 2,924 | 35,088 | 139.9 | 436.1 | 445.4 | 454.1 | 462.9 |
| **sled_tape_26 *** | 51,843 | 622,116 | 383.1 | 1,121.6 | 5,928.0 | 10,232.8 | 14,622.3 |
| **sled_tape_27 *** | 9,840 | 118,080 | 430.0 | 818.2 | 3,660.8 | 4,835.2 | 6,253.3 |

### 2.3 The same two, with the SPAWN SETTLE excluded (t >= 1.0 s)

Every tape opens with the machine dropped onto the ground, and that settle is a real
kernel transient but it is **not riding**. On `sled_tape_13` it owns the entire-run
`inst` max at tick 12 - t = 0.10 s, ground speed **0.07 m/s** - which would have put
a 1,015 N spike into a dial that is supposed to describe a man on a moving sled.
Both windows are published so nobody has to take my word for which is which.

| tape | ticks | substeps >=1 s | inst p50 | inst p99 | inst p99.9 | inst max | lp p50 | lp p99 | lp p99.9 | lp max |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 97,184 | 1,164,768 | 239.2 | 3,530.8 | 12,791.1 | 52,507.6 | 275.8 | 3,496.6 | 8,747.7 | 17,743.5 |
| **sled_tape_2** | 7,555 | 89,220 | 330.6 | 28,062.0 | 31,679.7 | 168,977.0 | 345.6 | 26,878.7 | 28,989.4 | 32,731.7 |
| **sled_tape_3** | 5,324 | 62,448 | 305.8 | 4,581.9 | 14,125.9 | 38,339.4 | 361.4 | 3,427.0 | 8,330.1 | 8,933.0 |
| **sled_tape_4** | 12,424 | 147,648 | 75.7 | 4,143.3 | 10,274.0 | 44,276.9 | 82.5 | 4,366.8 | 8,864.6 | 13,078.9 |
| **sled_tape_5** | 6,662 | 78,504 | 423.5 | 3,927.7 | 11,377.5 | 21,783.8 | 406.5 | 3,423.1 | 5,683.9 | 6,213.8 |
| **sled_tape_6** | 6,736 | 79,392 | 388.4 | 4,574.3 | 15,879.7 | 45,434.8 | 407.1 | 3,846.9 | 7,972.3 | 8,549.9 |
| **sled_tape_7** | 5,958 | 70,056 | 321.0 | 1,576.0 | 2,878.4 | 3,872.5 | 325.4 | 1,554.5 | 2,310.9 | 2,335.1 |
| **sled_tape_8** | 113,662 | 1,362,504 | 273.4 | 5,703.7 | 12,868.4 | 140,124.3 | 292.9 | 4,938.0 | 10,456.5 | 22,055.2 |
| **sled_tape_9** | 15,501 | 184,572 | 265.9 | 4,435.9 | 6,880.7 | 40,494.1 | 282.2 | 3,745.4 | 5,127.3 | 5,213.8 |
| **sled_tape_10** | 7,843 | 92,676 | 10.3 | 596.9 | 931.6 | 1,004.4 | 10.3 | 552.0 | 614.9 | 642.2 |
| **sled_tape_11** | 1,288 | 14,016 | 416.2 | 2,893.7 | 3,705.4 | 7,218.5 | 422.7 | 2,438.2 | 2,590.6 | 2,592.3 |
| **sled_tape_12** | 5,554 | 65,208 | 341.9 | 2,582.9 | 10,464.5 | 20,688.4 | 362.5 | 2,530.9 | 6,019.6 | 6,294.1 |
| **sled_tape_13** | 1,312 | 14,304 | 302.7 | 591.6 | 772.6 | 784.8 | 316.5 | 472.9 | 509.8 | 516.1 |
| **sled_tape_14** | 129,566 | 1,553,352 | 294.3 | 4,143.9 | 9,923.1 | 85,311.2 | 319.6 | 3,652.8 | 7,929.4 | 12,208.2 |
| **sled_tape_15** | 76,820 | 921,840 | 238.6 | 3,770.5 | 9,516.1 | 38,911.4 | 299.5 | 3,222.2 | 7,022.1 | 10,611.3 |
| **sled_tape_16** | 156,074 | 1,871,448 | 239.8 | 5,266.1 | 11,599.0 | 100,737.7 | 274.8 | 4,734.7 | 8,966.3 | 18,535.4 |
| **sled_tape_17** | 2,215 | 25,140 | 413.0 | 1,218.1 | 4,823.9 | 5,063.7 | 399.6 | 956.2 | 2,312.7 | 2,364.0 |
| **sled_tape_18** | 1,275 | 13,860 | 293.7 | 566.5 | 777.5 | 797.4 | 294.3 | 490.7 | 510.9 | 513.6 |
| **sled_tape_19** | 5,381 | 63,132 | 347.7 | 7,493.1 | 13,737.9 | 22,678.7 | 369.3 | 5,291.4 | 8,418.4 | 8,513.1 |
| **sled_tape_20 *** | 6,438 | 75,816 | 404.7 | 6,329.8 | 19,741.6 | 27,199.9 | 412.1 | 5,953.8 | 10,010.5 | 10,983.4 |
| **sled_tape_21 *** | 6,035 | 70,980 | 162.6 | 584.3 | 787.2 | 969.4 | 171.3 | 459.9 | 598.6 | 635.0 |
| **sled_tape_22 *** | 2,396 | 27,312 | 129.1 | 878.3 | 1,363.1 | 1,594.2 | 142.3 | 709.8 | 723.7 | 723.7 |
| **sled_tape_23 *** | 1,163 | 12,516 | 25.2 | 446.2 | 550.8 | 614.3 | 24.5 | 445.8 | 458.5 | 460.6 |
| **sled_tape_24 *** | 4,184 | 48,768 | 25.8 | 798.4 | 843.3 | 1,086.6 | 88.9 | 764.5 | 792.6 | 796.4 |
| **sled_tape_25 *** | 2,924 | 33,648 | 137.1 | 476.6 | 760.5 | 922.9 | 140.9 | 445.7 | 455.1 | 462.9 |
| **sled_tape_26 *** | 51,843 | 620,676 | 370.7 | 6,495.7 | 14,045.7 | 80,112.1 | 383.8 | 5,931.6 | 10,238.4 | 14,622.3 |
| **sled_tape_27 *** | 9,840 | 116,640 | 432.0 | 4,557.3 | 7,528.7 | 22,235.9 | 431.5 | 3,669.4 | 4,835.9 | 6,253.3 |

The settle owns the whole-run `inst` max on 6 of the 27 tapes: tape 24 (2,792.4 -> 1,086.6 N), tape 18 (1,440.6 -> 797.4 N), tape 23 (1,085.2 -> 614.3 N), tape 21 (1,382.1 -> 969.4 N), tape 13 (1,015.2 -> 784.8 N), tape 10 (1,126.1 -> 1,004.4 N).
On the other 21 the spawn drop is not the biggest thing that happens, which is
itself worth knowing: on a tape with a real crash in it, being dropped onto the
ground at spawn is not remotely the worst moment.

> WARNING - ONE TAPE'S EXCLUSION IS A NO-OP, and it was caught by a self-check, not
> by inspection. The window is applied as `tick * dt >= 1.0 s`, and on
> **sled_tape_15** `rec.tick` is a SESSION-GLOBAL counter rather than an index into
> the records (the census flagged the same thing: its rolled tick reads 135,268
> against 76,820 replayed ticks, which is not a divergence). Its ticks therefore
> begin far above 1.0 s, no substep is excluded, and its `>=1 s` rows are byte-
> identical to its whole-run rows. Its `t [s]` column in S3 is session time, not
> time into the tape. Nothing else is affected: on the other 26 tapes the excluded
> count is exactly `12 / dt` substeps, which is the check that found this.

---

## 3. WHERE THE PEAK HAPPENED, AND WHAT THE MACHINE WAS DOING

Peak of the **instantaneous** series (t >= 1 s window), with the state at that exact
substep. `case` is the S4.4 enumeration: **S** seated, **A** seat edge, **B** board
edge, **F** free - hands are the only contact. `surf` is the per-patch surface class
at that substep; `alpha_x` is rad/s^2; `n_sum` is the total ground normal load in N
(**`n_sum` = 0 is airborne**).

| tape | max N | tick | t [s] | case | gs [m/s] | surf | n_sum | alpha_x | depth [m] |
|---|---:|---:|---:|:--:|---:|---|---:|---:|---:|
| **sled_tape_1** | 52,507.6 | 34,935 | 291.12 | S | 4.34 | Bush | 170,768 | +120.6 | 0.845 |
| **sled_tape_2** | 168,977.0 | 5,679 | 47.33 | F | 25.99 | Bush | 233,723 | -1963.5 | 0.845 |
| **sled_tape_3** | 38,339.4 | 3,961 | 33.01 | F | 9.30 | Bush | 159,255 | -1117.7 | 0.709 |
| **sled_tape_4** | 44,276.9 | 6,015 | 50.12 | F | 6.37 | Bush | 0 | +372.4 | 0.928 |
| **sled_tape_5** | 21,783.8 | 2,794 | 23.28 | S | 10.29 | TrailMain | 102,883 | +74.8 | 0.821 |
| **sled_tape_6** | 45,434.8 | 3,062 | 25.52 | F | 7.62 | Bush | 87,248 | +215.0 | 0.631 |
| **sled_tape_7** | 3,872.5 | 3,552 | 29.60 | F | 26.93 | Bush | 0 | -4.0 | 0.818 |
| **sled_tape_8** | 140,124.3 | 71,812 | 598.43 | F | 8.32 | Bush | 202,936 | +1264.9 | 0.771 |
| **sled_tape_9** | 40,494.1 | 14,119 | 117.66 | F | 34.42 | TrailMain | 100,170 | +86.9 | 0.787 |
| **sled_tape_10** | 1,004.4 | 5,276 | 43.97 | S | 12.26 | Road | 4,282 | -3.4 | 0.020 |
| **sled_tape_11** | 7,218.5 | 973 | 8.11 | A | 12.75 | Bush | 39,178 | -164.9 | 0.560 |
| **sled_tape_12** | 20,688.4 | 5,474 | 45.62 | F | 9.36 | Road | 0 | +97.7 | 0.020 |
| **sled_tape_13** | 784.8 | 1,275 | 10.62 | S | 3.14 | Road | 3,661 | -1.3 | 0.020 |
| **sled_tape_14** | 85,311.2 | 3,224 | 26.87 | F | 7.16 | Bush | 167,244 | +920.4 | 0.935 |
| **sled_tape_15** | 38,911.4 | 163,512 | 1362.60 | S | 4.62 | Road | 116,980 | +226.8 | 0.020 |
| **sled_tape_16** | 100,737.7 | 114,235 | 951.96 | B | 22.00 | Bush | 229,847 | +305.5 | 1.052 |
| **sled_tape_17** | 5,063.7 | 1,625 | 13.54 | S | 5.74 | TrailMain | 23,278 | -46.8 | 0.195 |
| **sled_tape_18** | 797.4 | 648 | 5.40 | S | 13.79 | Road | 3,931 | -0.9 | 0.020 |
| **sled_tape_19** | 22,678.7 | 3,065 | 25.54 | F | 1.25 | Road | 36,395 | +124.0 | 0.020 |
| **sled_tape_20** | 27,199.9 | 5,087 | 42.39 | A | 7.65 | Bush | 96,293 | +78.3 | 0.817 |
| **sled_tape_21** | 969.4 | 2,365 | 19.71 | S | 3.14 | Road | 3,608 | -2.3 | 0.020 |
| **sled_tape_22** | 1,594.2 | 280 | 2.33 | B | 6.96 | TrailMain | 8,628 | +45.3 | 0.350 |
| **sled_tape_23** | 614.3 | 165 | 1.38 | S | 3.19 | Road | 4,052 | -6.5 | 0.020 |
| **sled_tape_24** | 1,086.6 | 1,009 | 8.41 | S | 10.48 | Road | 5,934 | +4.6 | 0.020 |
| **sled_tape_25** | 922.9 | 1,776 | 14.80 | S | 3.16 | Road | 3,668 | -4.0 | 0.020 |
| **sled_tape_26** | 80,112.1 | 27,666 | 230.55 | A | 21.90 | Bush | 194,156 | +195.0 | 0.979 |
| **sled_tape_27** | 22,235.9 | 4,474 | 37.28 | S | 6.04 | Road | 42,688 | -9.8 | 0.020 |

And the peak of the **low-passed** series - the one the dial is set against:

| tape | max N | tick | t [s] | case | gs [m/s] | surf | n_sum | alpha_x | depth [m] |
|---|---:|---:|---:|:--:|---:|---|---:|---:|---:|
| **sled_tape_1** | 17,743.5 | 32,139 | 267.82 | F | 15.39 | Bush | 0 | +48.9 | 0.764 |
| **sled_tape_2** | 32,731.7 | 5,680 | 47.33 | F | 25.18 | Bush | 31,608 | -168.6 | 0.845 |
| **sled_tape_3** | 8,933.0 | 3,974 | 33.12 | F | 4.37 | Bush | 0 | +53.9 | 0.710 |
| **sled_tape_4** | 13,078.9 | 6,019 | 50.16 | F | 3.13 | Bush | 2,524 | +101.1 | 0.928 |
| **sled_tape_5** | 6,213.8 | 4,741 | 39.51 | F | 4.30 | Road | 0 | -8.8 | 0.020 |
| **sled_tape_6** | 8,549.9 | 3,069 | 25.57 | F | 5.03 | Bush | 0 | -40.0 | 0.631 |
| **sled_tape_7** | 2,335.1 | 3,562 | 29.68 | F | 25.99 | Bush | 0 | +3.1 | 0.818 |
| **sled_tape_8** | 22,055.2 | 71,813 | 598.44 | F | 6.58 | Bush | 33,470 | +196.5 | 0.771 |
| **sled_tape_9** | 5,213.8 | 3,206 | 26.72 | F | 32.23 | TrailMain | 0 | -31.3 | 0.584 |
| **sled_tape_10** | 642.2 | 4,302 | 35.85 | S | 3.07 | Road | 3,446 | +4.2 | 0.020 |
| **sled_tape_11** | 2,592.3 | 949 | 7.91 | F | 14.27 | Bush | 0 | +6.2 | 0.569 |
| **sled_tape_12** | 6,294.1 | 5,480 | 45.67 | F | 6.73 | Road | 0 | +16.2 | 0.020 |
| **sled_tape_13** | 516.1 | 1,136 | 9.47 | S | 5.45 | Road | 2,292 | -2.3 | 0.020 |
| **sled_tape_14** | 12,208.2 | 20,616 | 171.80 | F | 17.95 | TrailMain | 56,947 | +38.4 | 0.289 |
| **sled_tape_15** | 10,611.3 | 197,678 | 1647.32 | F | 14.81 | Bush | 35,118 | +59.0 | 1.413 |
| **sled_tape_16** | 18,535.4 | 119,143 | 992.86 | F | 4.28 | Bush | 0 | +74.7 | 0.858 |
| **sled_tape_17** | 2,364.0 | 1,629 | 13.57 | S | 5.57 | TrailMain | 10,667 | -23.6 | 0.230 |
| **sled_tape_18** | 513.6 | 648 | 5.40 | S | 13.79 | Road | 3,931 | -0.9 | 0.020 |
| **sled_tape_19** | 8,513.1 | 2,869 | 23.91 | F | 4.40 | Bush/TrailMain | 0 | -10.0 | 0.558 |
| **sled_tape_20** | 10,983.4 | 5,094 | 42.45 | A | 3.08 | Bush | 0 | -54.9 | 0.817 |
| **sled_tape_21** | 635.0 | 2,366 | 19.72 | S | 3.09 | Road | 3,436 | +4.1 | 0.020 |
| **sled_tape_22** | 723.7 | 193 | 1.61 | S | 4.45 | TrailMain | 2,398 | -1.8 | 0.672 |
| **sled_tape_23** | 460.6 | 166 | 1.38 | S | 3.15 | Road | 4,054 | -1.6 | 0.020 |
| **sled_tape_24** | 796.4 | 174 | 1.45 | S | 3.23 | TrailMain | 2,752 | -1.8 | 0.254 |
| **sled_tape_25** | 462.9 | 1,360 | 11.33 | S | 3.16 | Road | 3,784 | +3.4 | 0.020 |
| **sled_tape_26** | 14,622.3 | 27,369 | 228.07 | F | 27.57 | TrailMain | 0 | -52.3 | 0.899 |
| **sled_tape_27** | 6,253.3 | 4,476 | 37.30 | S | 5.16 | Road | 10,291 | +5.7 | 0.020 |

---

## 4. *** THE SEAT / BOARD / HAND SPLIT AT THE PEAK

**This is the section the whole rod model exists for.** The refuted point-mass +
seat-first-ordering model reports **zero** grip load on exactly the bump that throws
him, because it has no mechanism that lets the seat drop out. The table below is the
demonstration that this model does not do that.

Each fraction is that contact's load divided by the rider's own weight (858.08 N).
`hand` is `hypot(H_aft, H_up)`. They do **not** sum to 1 and are not meant to: the
hand force has a fore-aft component the vertical contacts cannot carry at all, and
under load all three exceed 1 g.

| tape | case at peak | seat N (x g) | board N (x g) | **hand N (x g)** | H_aft N | H_up N | M_h N.m |
|---|:--:|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | S | 12,326.4 (14.37) | 12,355.5 (14.40) | **52,507.6 (61.19)** | +52,507.6 | +0.0 | +0.0 |
| **sled_tape_2** | F | 0.0 (0.00) | 0.0 (0.00) | **168,977.0 (196.92)** | -87,169.9 | -144,757.2 | +24,947.7 |
| **sled_tape_3** | F | 0.0 (0.00) | 0.0 (0.00) | **38,339.4 (44.68)** | -245.8 | -38,338.6 | +3,285.0 |
| **sled_tape_4** | F | 0.0 (0.00) | 0.0 (0.00) | **44,276.9 (51.60)** | -1,820.6 | -44,239.5 | +36,077.6 |
| **sled_tape_5** | S | 5,520.0 (6.43) | 325.3 (0.38) | **21,783.8 (25.39)** | -21,783.8 | +0.0 | +0.0 |
| **sled_tape_6** | F | 0.0 (0.00) | 0.0 (0.00) | **45,434.8 (52.95)** | -41,654.2 | -18,145.1 | +15,884.7 |
| **sled_tape_7** | F | 0.0 (0.00) | 0.0 (0.00) | **3,872.5 (4.51)** | +929.3 | -3,759.3 | +3,183.3 |
| **sled_tape_8** | F | 0.0 (0.00) | 0.0 (0.00) | **140,124.3 (163.30)** | -19,288.3 | -138,790.5 | +125,112.1 |
| **sled_tape_9** | F | 0.0 (0.00) | 0.0 (0.00) | **40,494.1 (47.19)** | +33,983.2 | +22,020.7 | +1,723.3 |
| **sled_tape_10** | S | 817.0 (0.95) | 436.4 (0.51) | **1,004.4 (1.17)** | +1,004.4 | +0.0 | +0.0 |
| **sled_tape_11** | A | 1,922.6 (2.24) | 0.0 (0.00) | **7,218.5 (8.41)** | +7,175.0 | -791.6 | +0.0 |
| **sled_tape_12** | F | 0.0 (0.00) | 0.0 (0.00) | **20,688.4 (24.11)** | +11,867.4 | -16,946.2 | +14,080.2 |
| **sled_tape_13** | S | 1,034.2 (1.21) | 3.1 (0.00) | **784.8 (0.91)** | +784.8 | +0.0 | +0.0 |
| **sled_tape_14** | F | 0.0 (0.00) | 0.0 (0.00) | **85,311.2 (99.42)** | -33,987.9 | -78,248.5 | +71,560.5 |
| **sled_tape_15** | S | 7,652.7 (8.92) | 4,374.5 (5.10) | **38,911.4 (45.35)** | +38,911.4 | +0.0 | +0.0 |
| **sled_tape_16** | B | 0.0 (0.00) | 36,978.9 (43.09) | **100,737.7 (117.40)** | +100,085.6 | +11,444.1 | +0.0 |
| **sled_tape_17** | S | 5,291.5 (6.17) | 1,524.6 (1.78) | **5,063.7 (5.90)** | -5,063.7 | +0.0 | +0.0 |
| **sled_tape_18** | S | 926.2 (1.08) | 163.5 (0.19) | **797.4 (0.93)** | +797.4 | +0.0 | +0.0 |
| **sled_tape_19** | F | 0.0 (0.00) | 0.0 (0.00) | **22,678.7 (26.43)** | -16,004.7 | -16,067.9 | +13,953.1 |
| **sled_tape_20** | A | 35,917.6 (41.86) | 0.0 (0.00) | **27,199.9 (31.70)** | -26,585.7 | -5,747.7 | +0.0 |
| **sled_tape_21** | S | 664.9 (0.77) | 358.8 (0.42) | **969.4 (1.13)** | +969.4 | +0.0 | +0.0 |
| **sled_tape_22** | B | 0.0 (0.00) | 385.6 (0.45) | **1,594.2 (1.86)** | +1,567.7 | +289.6 | +0.0 |
| **sled_tape_23** | S | 894.7 (1.04) | 406.8 (0.47) | **614.3 (0.72)** | +614.3 | +0.0 | +0.0 |
| **sled_tape_24** | S | 560.8 (0.65) | 1,021.5 (1.19) | **1,086.6 (1.27)** | +1,086.6 | +0.0 | +0.0 |
| **sled_tape_25** | S | 741.6 (0.86) | 367.9 (0.43) | **922.9 (1.08)** | +922.9 | +0.0 | +0.0 |
| **sled_tape_26** | A | 57,380.2 (66.87) | 0.0 (0.00) | **80,112.1 (93.36)** | -78,540.0 | -15,793.1 | +0.0 |
| **sled_tape_27** | S | 5,457.8 (6.36) | 3,445.9 (4.02) | **22,235.9 (25.91)** | -22,235.9 | +0.0 | +0.0 |

**What to read off it:**

* **The seat carries literally nothing at the peak on 12 of the 27 tapes** -
  CASE F on 10 (tapes 2, 3, 4, 6, 7, 8, 9, 12, 14, 19) and CASE B on 2
  (tapes 16, 22). In CASE F **both** the seat and the boards
  are zero and the hands are the only contact on the machine. A residual-ordering
  model reports 0 N there. This one reports the whole load.
* CASE S at the peak on 12 tapes, CASE A on 3.
* The case mix over the *whole* run (S6) is the breadth version of the same point:
  on `sled_tape_6` the rider is in CASE F for **34.0 %** of all substeps and CASE S
  for only 33.1 %. A third of that tape is a man whose seat is carrying nothing.
* The at-rest leg (S1) proves this is not "non-zero by construction": sitting still,
  the model reports **grip = 0.00 N** and puts all 858 N through seat and boards.

### 4.1 The wrist couple - "he pivots up over the bars"

`M_h` is non-zero **only in CASE F**, and it is the quantity a point mass could not
represent at all. Peak |M_h| per tape, over t >= 1 s:

| tape | max &#124;M_h&#124; N.m | tick | t [s] | gs [m/s] | alpha_x | hand N (x g) |
|---|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 27,574.3 | 63,894 | 532.45 | 7.62 | +266.1 | 34,747.2 (40.49) |
| **sled_tape_2** | 24,947.7 | 5,679 | 47.33 | 25.99 | -1963.5 | 168,977.0 (196.92) |
| **sled_tape_3** | 5,455.9 | 3,972 | 33.10 | 5.00 | +101.3 | 16,103.4 (18.77) |
| **sled_tape_4** | 36,077.6 | 6,015 | 50.12 | 6.37 | +372.4 | 44,276.9 (51.60) |
| **sled_tape_5** | 8,163.0 | 4,735 | 39.46 | 6.66 | -4.4 | 14,309.6 (16.68) |
| **sled_tape_6** | 16,423.4 | 3,062 | 25.52 | 7.62 | +322.6 | 33,519.0 (39.06) |
| **sled_tape_7** | 3,187.1 | 3,479 | 28.99 | 32.02 | +76.5 | 3,464.4 (4.04) |
| **sled_tape_8** | 125,112.1 | 71,812 | 598.43 | 8.32 | +1264.9 | 140,124.3 (163.30) |
| **sled_tape_9** | 6,386.5 | 5,684 | 47.37 | 8.60 | +47.3 | 10,615.0 (12.37) |
| **sled_tape_10** | 0.0 | 120 | 1.00 | 0.00 | +0.0 | 7.5 (0.01) |
| **sled_tape_11** | 2,557.8 | 913 | 7.61 | 17.19 | +32.1 | 3,866.3 (4.51) |
| **sled_tape_12** | 14,080.2 | 5,474 | 45.62 | 9.36 | +97.7 | 20,688.4 (24.11) |
| **sled_tape_13** | 0.0 | 120 | 1.00 | 0.98 | -0.5 | 459.1 (0.54) |
| **sled_tape_14** | 71,560.5 | 3,224 | 26.87 | 7.16 | +920.4 | 85,311.2 (99.42) |
| **sled_tape_15** | 9,753.7 | 166,030 | 1383.58 | 4.79 | +159.3 | 20,828.1 (24.27) |
| **sled_tape_16** | 38,056.7 | 119,140 | 992.83 | 6.62 | +235.8 | 45,686.3 (53.24) |
| **sled_tape_17** | 118.5 | 1,608 | 13.40 | 6.25 | +9.2 | 92.0 (0.11) |
| **sled_tape_18** | 0.0 | 120 | 1.00 | 0.01 | +0.0 | 29.5 (0.03) |
| **sled_tape_19** | 13,953.1 | 3,065 | 25.54 | 1.25 | +124.0 | 22,678.7 (26.43) |
| **sled_tape_20** | 7,362.6 | 2,140 | 17.83 | 5.06 | +101.7 | 19,280.1 (22.47) |
| **sled_tape_21** | 0.0 | 120 | 1.00 | 0.00 | +0.0 | 19.6 (0.02) |
| **sled_tape_22** | 16.3 | 308 | 2.57 | 7.42 | -0.1 | 26.2 (0.03) |
| **sled_tape_23** | 0.0 | 120 | 1.00 | 1.79 | +0.1 | 445.9 (0.52) |
| **sled_tape_24** | 0.0 | 120 | 1.00 | 0.30 | +3.1 | 628.6 (0.73) |
| **sled_tape_25** | 0.0 | 120 | 1.00 | 1.05 | -0.4 | 453.0 (0.53) |
| **sled_tape_26** | 32,936.0 | 11,295 | 94.12 | 28.15 | +278.5 | 35,430.9 (41.29) |
| **sled_tape_27** | 5,996.6 | 9,666 | 80.55 | 24.55 | +40.5 | 7,062.6 (8.23) |

Tapes 10, 13, 18, 21, 23, 24, 25 never enter CASE F after the settle, so their |M_h| is
exactly zero - the rider is never off the machine on them. That is a property of
those rides, not a gap in the instrument.

---

## 5. THE ERROR BARS - MEASURED, NOT ASSERTED

Two quantities in this instrument are reconstructed rather than observed, and the
probe measures its own error on both.

**(a) The intra-tick orientation.** Only the tick-END quaternion is observable, so
each substep's orientation is walked BACKWARD through the kernel's own integrator.
The check is the reconstructed start of tick *n* against the recorded end of tick
*n-1*.

**(b) The intra-tick lean.** `d` slews every substep but only its tick-end value is
observable, so it is interpolated across the tick and the residual is BRACKETED by
re-solving every substep with the tick-end `d` - the column below is the largest
disagreement between the two arms, not an estimate of it.

| tape | closure max [rad] | ticks over 1e-9 | first at tick | lean max &#124;dd&#124; [m] | bracket max &#124;d inst&#124; N | bracket max &#124;d lp&#124; N |
|---|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 3.138e+00 | 13 | 8898 | 0.019455 | 199.76 | 14.63 |
| **sled_tape_2** | 2.507e-15 | 0 | - | 0.016499 | 125.83 | 10.70 |
| **sled_tape_3** | 3.140e+00 | 1 | 2254 | 0.012797 | 15.22 | 0.84 |
| **sled_tape_4** | 3.130e+00 | 3 | 5252 | 0.012378 | 87.09 | 3.78 |
| **sled_tape_5** | 1.780e+00 | 1 | 3382 | 0.011791 | 41.89 | 10.30 |
| **sled_tape_6** | 3.132e+00 | 1 | 4038 | 0.015094 | 256.52 | 12.77 |
| **sled_tape_7** | 2.394e-15 | 0 | - | 0.011514 | 14.29 | 0.95 |
| **sled_tape_8** | 3.140e+00 | 23 | 1508 | 0.020002 | 1,863.40 | 30.51 |
| **sled_tape_9** | 1.950e+00 | 1 | 5939 | 0.013275 | 34.85 | 2.40 |
| **sled_tape_10** | 2.849e-15 | 0 | - | 0.004524 | 0.47 | 0.04 |
| **sled_tape_11** | 1.806e-15 | 0 | - | 0.011314 | 1.02 | 0.10 |
| **sled_tape_12** | 2.052e-15 | 0 | - | 0.016244 | 56.25 | 5.08 |
| **sled_tape_13** | 2.077e-15 | 0 | - | 0.016208 | 3.53 | 0.39 |
| **sled_tape_14** | 3.140e+00 | 23 | 3513 | 0.016224 | 146.93 | 19.47 |
| **sled_tape_15** | 3.089e+00 | 12 | 150178 | 0.016298 | 341.17 | 14.73 |
| **sled_tape_16** | 3.116e+00 | 18 | 1911 | 0.016830 | 130.02 | 24.77 |
| **sled_tape_17** | 2.377e-15 | 0 | - | 0.014270 | 5.95 | 0.61 |
| **sled_tape_18** | 2.060e-15 | 0 | - | 0.011311 | 3.00 | 0.41 |
| **sled_tape_19** | 2.894e+00 | 1 | 3230 | 0.011313 | 10.66 | 0.61 |
| **sled_tape_20** | 3.117e+00 | 2 | 2250 | 0.014497 | 42.13 | 7.74 |
| **sled_tape_21** | 3.126e-15 | 0 | - | 0.000000 | 0.00 | 0.00 |
| **sled_tape_22** | 2.629e-15 | 0 | - | 0.001267 | 0.63 | 0.09 |
| **sled_tape_23** | 2.337e-15 | 0 | - | 0.002454 | 0.02 | 0.00 |
| **sled_tape_24** | 2.277e-15 | 0 | - | 0.005767 | 0.23 | 0.09 |
| **sled_tape_25** | 2.304e-15 | 0 | - | 0.000000 | 0.00 | 0.00 |
| **sled_tape_26** | 3.116e+00 | 12 | 5081 | 0.017541 | 210.41 | 34.95 |
| **sled_tape_27** | 1.841e+00 | 3 | 3392 | 0.012569 | 100.72 | 9.51 |

**Reading (a) honestly - and this needed measuring, not assuming.** On 13 tapes the
closure is at machine epsilon (~1e-15 rad): the reconstruction is exact. On the
other 14 the *max* is large - up to pi rad - and my first reaction was that the
reconstruction was broken, which would have made every number above worthless. It
is not. **The count column is the discriminator, and it is 1 on 5 of those
14 tapes and more than 1 on the other 9.** The worst offender in the whole set is
tape 20 at 2 ticks of 6,438 - 0.031 %. A handful of ticks out of
thousands is a DISCONTINUITY, not a drift: the orientation genuinely teleports
there (a crash reset), and the reconstruction is exact on both sides of it. A broken
reconstruction would fail on *every* tick. The affected ticks are a handful of
substeps out of millions. Measured, not assumed: of the 14 tapes with a non-zero count, 14
have their first offending tick AFTER that tape's roll tick, which is the signature
of a post-roll crash reset.

> WARNING: the metric itself was fixed during this run: it was `2*asin(|v|)`, which
> folds above 90 deg and cannot tell a genuine 180 deg jump from a near-2pi one. It
> is now `2*atan2(|v|, |w|)`, which is monotone on [0, pi] and identical for `q` and
> `-q`. The breadth counter is new in the same edit. Both changes are in
> `tools/sled_probe.cpp` and touch no measured value.

**Reading (b).** The lean bracket is under 1 N on the quiet tapes and reaches
1,863.40 N on `sled_tape_8` - but only on `inst`, where that tape's max is 140,124.3 N (1.33 %).
On `lp`, the series the dial actually uses, the worst bracket in the whole set is
**34.95 N on tape 26** against that tape's max of 14,622.3 N - **0.24 %**. The
reconstruction does not move the number Chad dials against.

---

## 6. CASE MIX OVER THE WHOLE RUN

The fraction of all substeps spent in each contact case. This is the shape of the
ride, and it is what separates a cruise tape from a crash tape.

| tape | ticks | rolled @ | S seated | A seat edge | B board edge | **F free** |
|---|---:|---:|---:|---:|---:|---:|
| **sled_tape_1** | 97,184 | 6323 | 50.7% | 21.0% | 3.2% | **25.1%** |
| **sled_tape_2** | 7,555 | 2611 | 78.4% | 5.2% | 0.6% | **15.8%** |
| **sled_tape_3** | 5,324 | 1376 | 71.6% | 1.5% | 0.5% | **26.4%** |
| **sled_tape_4** | 12,424 | 4559 | 85.2% | 6.4% | 0.1% | **8.3%** |
| **sled_tape_5** | 6,662 | 2830 | 72.6% | 8.4% | 0.1% | **18.9%** |
| **sled_tape_6** | 6,736 | 3096 | 33.1% | 32.0% | 0.9% | **34.0%** |
| **sled_tape_7** | 5,958 | 3549 | 66.5% | 30.7% | 0.0% | **2.7%** |
| **sled_tape_8** | 113,662 | 1248 | 65.5% | 15.5% | 1.6% | **17.4%** |
| **sled_tape_9** | 15,501 | 3622 | 58.8% | 24.4% | 0.3% | **16.5%** |
| **sled_tape_10** | 7,843 | - | 100.0% | 0.0% | 0.0% | **0.0%** |
| **sled_tape_11** | 1,288 | 940 | 79.5% | 3.6% | 0.1% | **16.8%** |
| **sled_tape_12** | 5,554 | 1743 | 63.5% | 25.0% | 0.6% | **10.9%** |
| **sled_tape_13** | 1,312 | - | 80.1% | 19.9% | 0.0% | **0.0%** |
| **sled_tape_14** | 129,566 | 3258 | 71.5% | 10.5% | 3.3% | **14.8%** |
| **sled_tape_15** | 76,820 | 135268 | 66.2% | 16.3% | 1.1% | **16.4%** |
| **sled_tape_16** | 156,074 | 1735 | 78.7% | 8.8% | 2.1% | **10.4%** |
| **sled_tape_17** | 2,215 | - | 88.7% | 6.7% | 0.4% | **4.2%** |
| **sled_tape_18** | 1,275 | - | 93.4% | 6.6% | 0.0% | **0.0%** |
| **sled_tape_19** | 5,381 | 3094 | 74.8% | 15.0% | 0.0% | **10.2%** |
| **sled_tape_20** | 6,438 | 2172 | 64.4% | 6.3% | 2.9% | **26.3%** |
| **sled_tape_21** | 6,035 | - | 100.0% | 0.0% | 0.0% | **0.0%** |
| **sled_tape_22** | 2,396 | - | 97.2% | 0.0% | 0.5% | **2.3%** |
| **sled_tape_23** | 1,163 | - | 100.0% | 0.0% | 0.0% | **0.0%** |
| **sled_tape_24** | 4,184 | - | 99.8% | 0.0% | 0.1% | **0.2%** |
| **sled_tape_25** | 2,924 | - | 100.0% | 0.0% | 0.0% | **0.0%** |
| **sled_tape_26** | 51,843 | 4096 | 65.1% | 13.6% | 1.5% | **19.9%** |
| **sled_tape_27** | 9,840 | 3281 | 66.5% | 18.1% | 0.2% | **15.1%** |

* Every `rolled` tick above matched between tape and replay on every tape, and
`first_div` was `-1` everywhere - the replays are bit-exact, so these distributions
are over the ride Chad actually rode.

---

## 7. WHAT THIS DATA DOES *NOT* SAY

Repeating `docs/R4A_PHASE0_MODEL.md` S6 because these numbers will outlive the
document that qualifies them:

1. **This is the load to hold the rider RIGIDLY.** Real knees, hips, elbows and
   shoulders comply, and `render::rider_absorb()` already ships a measured absorb
   fraction this model gives zero travel to. **Every number above is a conservative
   upper bound.**
2. **It is not a grip-strength table.** A peak of 168,977.0 N on tape 2 is not a claim
   that a man holds 169 kN. It is the force that would be required of a rigid rider,
   and the whole point of the scale is that `grip_capacity_n` is a felt call against
   *this instrument's* units. It must never be compared to a physiological figure.
3. **It is blind to roll and to any side-throw** - the planar sagittal reduction is
   Chad's own R4a deferral. A lateral buck-off does not appear anywhere in this data.
4. **`I_pitch` is a slender-rod lower bound**, bounded at <= 14.2 % of `I_pitch`, and
   it runs the *opposite* way to the three conservatisms above.

---

## 8. THE NEXT STEP

`grip_capacity_n` is Chad's dial and this document does not propose a value. What it
does say is where to look: **S2.2 and S2.3 (`lp`), per tape, not pooled** - and that
the honest span of a candidate is bracketed by tape 23 (a cruise: `lp` max 460.6 N)
and tape 2 (a crash: `lp` max 32,731.7 N). Anything in between decides *which of these
27 rides throws him*, which is a felt question, not a computed one.
