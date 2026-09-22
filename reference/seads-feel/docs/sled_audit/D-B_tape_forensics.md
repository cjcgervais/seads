# STRAND D-B — TAPE FORENSICS

**Scope.** Every `.sledtape` in `D:/flight_sim2/seads-recon/build-play`, parsed
by `tools/sled_tape_audit.py` (stdlib-only, streaming, read-only). 91 files,
11.79 GB, **84 parsed**, 7 skipped (zero-byte). **1,483,776 ticks = 206.1
minutes = 3 h 26 min of Chad's recorded driving** across five kernel tags.
Machine-readable output: `docs/sled_audit/tape_summary.json`.

**Read-only.** Tapes were opened `'rb'`. Nothing in `seads-recon` was written.
No ctest, no `seads.exe`, no probe build was needed for this strand.

---

## 1. METHOD — what was verified, not assumed

### 1.1 Column order taken from the source, then checked against raw bytes

`test/harness/sled_tape.h` writers (`on_tick` :192, `on_override` :210,
`on_ground` :218, `pin_fields` :277) and the three X-macro rosters
(`SLEDTAPE_PARAMS_D` :52, `SLEDTAPE_COMFORT_D` :79, `SLEDTAPE_PIN_D` :98) give:

```
T tick | throttle brake steer lean_lat lean_fwd stand | air_temp_c snow_hardness
        cold_t_ref_c | pin41 | surface rolled            -> 54 tokens
O tick | pin41 | surface rolled                          -> 45 tokens
G tick | dir.x dir.y dir.z drive_r depth_m surface       ->  8 tokens
```

Hand-check on raw bytes of `sled_tape_91` (tick 0, 1, 4999) — all four
independent cross-checks passed:

| check | expected if the slice is right | measured |
|---|---|---|
| token count | 54 | 54 |
| `pin[6..9]` is a unit quaternion | \|q\| = 1 | `1.000000000` |
| `pin[0..2]` is a **globe** position | \|pos\| ≈ 15 035 m | 15 092.414 m |
| `pin[35] ground_speed` vs \|velocity\| | ≤, close | 33.7137 vs 33.7756 |

A wrong 4-token offset would not produce a unit quaternion. **Confidence:
MEASURED.**

### 1.2 Independent recompute

Every headline metric was recomputed by a **second, differently-written**
implementation (quaternion-sandwich rotation instead of the tool's mat3
expansion), in `scratchpad/verify2.py`. Exact agreement on tapes 86 and 91 for:
`frac_past90` (0.12684479598726728 to all digits), rollover event count,
`rolled`-flag seconds, top speed, steer/lean saturation duty, band fraction,
catwalk duty. The one divergence was **explained, not waved off**: air events
30 (raw) vs 21 (tool) — the tool drops hops shorter than 0.15 s
(`sled_tape_audit.py` air block) and scores landing attitude 0.5 s *after*
touchdown rather than at it. Both choices are documented in the tool.

### 1.3 Killing mutations actually run

| metric | mutation | result | verdict |
|---|---|---|---|
| `frac_past90`, tape 86 | replace orientation with random unit quats | 0.0542 → **0.4935** | orientation-driven ✓ |
| `frac_past90`, tape 86 | read `up` as flat-world +Y instead of `normalize(position)` | 0.0542 → **0.9577** | **globe geometry is load-bearing** ✓ |
| `frac_kernel_rolled` | both of the above | **3.62 s, unchanged** | correctly reads the kernel's own column, so it is an independent cross-check on the geometric one ✓ |
| yaw-vs-steer ladder | shuffle the steer column (same marginal, pairing destroyed) | every cell collapses to a flat ≈0.26 (low speed) / ≈0.13 (30+) | structure is pairing, not binning ✓ |
| lean-band dwell | drop the EOF flush | tape 86 max 12.77 s → 4.77 s | (this was a real bug — see §8) |

The flat-world mutation is the important one. **A flat-world reading of these
tapes reports the machine 96 % rolled over.** Any sled number quoted from a
tape without `up = normalize(position)` is wrong by that margin.

### 1.4 Confidence vocabulary

**MEASURED** = counted off the tape. **DERIVED** = arithmetic or inference on
measured quantities, confound named. **LITERATURE** = outside reference.
**GUESS** = labelled as such inline. No GUESS appears in §5.

---

## 2. CORPUS INVENTORY

| bucket (describe base tag) | tapes | drive | GB | describe-n range |
|---|---|---|---|---|
| `pre-reconcile-20260821` | 53 (14–40, 48–73) | 132.9 min | 7.64 | 55 … 551 |
| `kernel-v13g-signed` | 13 (1–13) | 39.9 min | 2.48 | 173 … 217 |
| `kernel-v14-leanlead-signed` | 3 (74–76) | 4.6 min | 0.19 | 66 |
| `kernel-v15-righthand-signed` | 9 (77–85) | 16.7 min | 0.83 | 30 |
| `kernel-v17-tremor-signed` | 6 (86–91) | 12.0 min | 0.65 | 33 … 53 |

**⚠ The base tag is not a kernel.** `pre-reconcile-20260821` spans describe-n 55
(2026-08-21) to 551 (2026-09-10) — three weeks of kernel work under one base.
Treat it as a *time window*, never as a kernel arm.

### Parse failures — all 7, with cause

`sled_tape_41, 42, 43, 44, 45, 46, 47` — **`SKIP_ZERO_BYTE`**, 0 bytes on disk
(mtimes 2026-08-30 18:52 and 2026-09-03 00:21–00:44). The writer opened the
file and nothing was ever drained: a start with no tick, not a corrupt tape.
**0 malformed lines across the other 84 tapes** (`n_bad_lines` = 0 everywhere),
so no data was silently dropped.

---

## 3. PER-BUCKET TABLE

Time-weighted where a fraction; histograms pooled from raw bins (never by
averaging percentiles).

| | pre-reconcile | v13g | v14 ⚠ | v15 | **v17 (tonight)** |
|---|---|---|---|---|---|
| tapes / drive min | 53 / 132.9 | 13 / 39.9 | 3 / 4.6 | 9 / 16.7 | **6 / 12.0** |
| **rollovers past 90° per min** | 5.18 | 6.30 | 0.87 | 2.16 | **4.32** |
| frac of time past 90° | 0.0821 | 0.1325 | 0.0047 | 0.0387 | **0.0574** |
| frac with kernel `rolled` set | 0.0563 | 0.1000 | 0.0000 | 0.0260 | **0.0381** |
| **never recovered to steady driving** | 69 % | 70 % | — | 67 % | **71 %** |
| recovery p50 (s) | 3.42 | 3.48 | — | 5.55 | **3.81** |
| autoright **R** presses / min | 0.72 | 1.08 | 0.00 | 0.12 | **0.42** |
| air events / min | 5.02 | 5.44 | 0.44 | 2.40 | **4.82** |
| **landed upright** | 31.9 % | 27.6 % | 100 % | 35.0 % | **29.3 %** |
| hang p50 (s) | 0.46 | 0.57 | 2.33 | 0.54 | **0.39** |
| landings over 10 g | 352/667 | 118/217 | 2/2 | 21/40 | **26/58** |
| steer saturated (>0.98) | 24.1 % | 26.0 % | 3.0 % | 11.5 % | **19.0 %** |
| lean saturated (>0.98) | 25.7 % | 24.5 % | 8.9 % | 18.3 % | **26.5 %** |
| lean in 0.3–0.7 band | 24.8 % | 26.0 % | 12.6 % | 19.8 % | **20.3 %** |
| **steer-band dwell p99 (s)** | 0.365 | *n/a* | 0.305 | 0.335 | **0.315** |
| steer sign-flips / min | 12.4 | 13.6 | 3.1 | 5.5 | **8.4** |
| throttle WOT / zero / modulated | 58/34/8 % | 68/23/9 % | 3/93/3 % | 49/45/5 % | **52/40/7 %** |
| brake duty | 3.8 % | 3.8 % | 3.5 % | 6.9 % | **8.7 %** |
| stand duty | 35.3 % | 44.7 % | 1.5 % | 9.8 % | **25.6 %** |
| assist non-zero / \|assist\| p95 (N·m) | 84 % / 645 | 79 % / 635 | 99 % / 445 | 88 % / 505 | **86 % / 575** |
| catwalk duty / pitch p95 / p99 | 12.8 % / 14.3° / 51.3° | 13.1 % / 11.8° / 53.3° | 0.2 % / 6.3° | 1.5 % / 14.8° / 30.8° | **11.9 % / 14.3° / 66.8°** |
| speed p50 / p90 (m/s), top (km/h) | 7.9 / 26.9, 163.8 | 10.1 / 30.1, 164.8 | 0.1 / 2.9, 49.7 | 5.4 / 19.9, 122.0 | **6.4 / 27.1, 164.8** |
| \|a_body\| p50 / p99 / max (m/s²) | 3.25 / 28.3 / 2561 | 3.75 / 31.8 / 851 | 0.25 / 9.8 / 168 | 0.75 / 20.3 / 935 | **3.75 / 25.3 / 1766** |

**⚠ The v14 column is not a driving arm.** Speed p50 = 0.12 m/s, throttle at
zero 93 % of the time, snow depth under the machine p50 = 0.025 m, 4.6 min
total. Tapes 74–76 are a session in which the sled sat. Its 0.87 rollovers/min
and 100 % upright landings (n = 2) **must not** be read as "v14 fixed the
roll." Listed for completeness only.

### Rollovers by surface — pooled across the whole corpus

| surface | events | time on surface | **events / min on that surface** |
|---|---|---|---|
| **TrailMain** | 334 | 19.8 min | **16.88** |
| Bush | 516 | 124.9 min | 4.13 |
| Road | 171 | 50.5 min | 3.39 |
| RockOutcrop | 4 | 2.3 min | 1.71 |
| LakeIce | 6 | 7.1 min | 0.85 |
| TrailTributary | 1 | 1.5 min | 0.65 |

(1032 events total, 206.1 min. Attribution is the surface under the machine at
the tick the past-90° episode *begins*.)

---

## 4. TONIGHT (tapes 86–91, kernel v17)

**⚠ Tonight is two builds, not one.** Tapes 86–90 are
`kernel-v17-tremor-signed-33-g4b8c686`; **tape 91 is
`…-53-g7b377c6f0`** — 20 commits later, the build carrying the road-repair
merge. Tape 91 is also the only tape of the six with Road/TrailMain terrain,
and it is by a wide margin the worst. Do not pool it with 86–90 without saying so.

| tape | build | dur | surface mix | rolls (% time past 90°) | recovered / never | **R** | top m/s | air (upright) | stand | catwalk (p95) | lean sat / steer sat / band | WOT | brake | \|assist\| p50/p95 | end tilt | \|a_body\| p99 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 86 | -33 | 95 s | Bush 61 / Rock 20 / Ice 18 | **8** (5.4 %) | 4 / 4 | 1 | 38.1 | 10 (30 %) | 22 % | 14.7 % (66.2°) | 28/16/35 % | 63 % | 8.4 % | 75 / 715 | 2° | 27.8 |
| 87 | -33 | 119 s | Bush 54 / Ice 23 / Rock 23 | 4 (2.5 %) | 2 / 2 | 0 | **45.8** | 4 (25 %) | 25 % | 15.6 % (3.8°) | 28/20/16 % | 48 % | 11.6 % | 45 / 495 | 1° | 20.8 |
| 88 | -33 | 99 s | Bush 55 / Rock 23 / Ice 22 | **10** (7.6 %) | 2 / **8** | 2 | 44.9 | 10 (30 %) | 19 % | 9.1 % (2.8°) | 20/22/28 % | 56 % | 12.7 % | 55 / 495 | 2° | 29.8 |
| **89** | -33 | 127 s | Bush 37 / Ice 34 / Rock 29 | **0** (0.0 %) | — | 0 | 41.9 | 4 (**100 %**) | **39 %** | 22.7 % (3.2°) | **15/11/23 %** | 44 % | 4.9 % | 45 / 455 | 2° | **14.2** |
| 90 | -33 | 108 s | **Ice 83** / Bush 17 | 7 (3.5 %) | 1 / 6 | 0 | 38.4 | 9 (**0 %**) | 7 % | 2.9 % (51.2°) | 15/5/6 % | 20 % | 11.9 % | 5 / 215 | 1° | 22.8 |
| **91** | **-53** | 173 s | Bush 61 / **Road 23 / Trail 16** | **23** (**12.7 %**) | 6 / **17** | 2 | 42.8 | 21 (29 %) | 33 % | 7.2 % (50.8°) | **44/33/18 %** | **72 %** | 5.2 % | 75 / 765 | **131°** | **39.8** |

Tonight's rollovers by the surface under the machine at entry:

| tape | Bush | LakeIce | RockOutcrop | Road | TrailMain |
|---|---|---|---|---|---|
| 86 | 5 in 59 s | 2 in 18 s | 1 in 19 s | — | — |
| 87 | 4 in 65 s | 0 in 28 s | 0 in 27 s | — | — |
| 88 | 7 in 54 s | 3 in 22 s | 0 in 23 s | — | — |
| 89 | 0 in 47 s | 0 in 43 s | 0 in 37 s | — | — |
| 90 | **7 in 19 s** | **0 in 90 s** | — | — | — |
| 91 | 11 in 106 s | — | — | 5 in 40 s | **7 in 27 s** |

**Tape 91 ends upside down** — last tick tilt 131.4°, speed 0.41 m/s, throttle
0, `# sig` present (clean exit). He shut the session down while the machine was
on its roof. That is the single most legible frustration signal in the corpus,
and it is the last tape of the night.

---

## 5. FINDINGS

Each finding: claim → evidence → confidence → **killing mutation** (the change
that would make it fail). Feel claims additionally cite Chad's verbatim words.

### D-B-1 — Rolling is the rule, not the exception. **MEASURED.**

Tonight: **4.32 past-90° rollovers per minute** — one every 13.9 s — and
**5.74 %** of drive time spent inverted. Across the whole corpus the rate has
never been below 2.16/min in any real driving bucket (206 min, 5 tags).

> Chad, `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0 (drive 4,
> 2026-08-12, verbatim): *"**I rule that it should be roll resistant.**"* …
> *"**Make it possible to roll but not the rule.**"*

Tape event: `sled_tape_91`, 23 rollovers in 173 s, ending at 131° tilt.
**Killing mutation:** shuffle the orientation column → count must change (it
does: 8 → 2 events and frac 0.054 → 0.494 on tape 86). Independently: the
kernel's own `rolled` column agrees in sign and rank on every tape.

### D-B-2 — Post-roll recovery is worse than a coin flip, and has not improved. **MEASURED.**

**71 % of tonight's rollovers never returned to steady driving** (tilt < 20°
and speed > 5 m/s held 0.5 s): 37 of 52. Corpus: 69 % (pre-reconcile), 70 %
(v13g), 67 % (v15), 71 % (v17). **Landed-upright fraction is 29.3 % tonight vs
27.6 % at v13g** — four weeks and three signed kernels, no measurable movement
on the metric Chad named.

> Chad, §0: *"Let me slide around a bit arcade but **allow me to land on my
> skis more often after a roll** (even though R works). But not every time —
> allow it to happen."*
> Chad, §0b (2026-08-12 night): *"Leaning shall enhance the ride an just make
> it more stable and slef righting by chance more."*

Tape event: `sled_tape_90`, **0 of 9 landings upright**, 1 recovery out of 7
rollovers. **Killing mutation:** widen the recovery predicate to tilt < 45° or
drop the 0.5 s hold → the unrecovered fraction must fall. It is a strict
predicate by choice; the *trend across buckets* is what carries the claim, and
all four buckets used the same predicate.

### D-B-3 — He is not pressing R. **MEASURED.**

**5 autoright presses against 52 rollovers tonight** (0.42/min vs 4.32/min).
v15: 0.12/min. The O-record frustration proxy is *low* precisely where
rollovers are *high*, so the backstop is not what is absorbing them — he is
either riding them out or the run ends. Corroborated: tape 91's 2 presses
against 23 rollovers, and the tape ending inverted without a third press.
**Killing mutation:** mis-classify `episode_start` O-records as autoright →
the rate triples to 1.4/min. The tool separates them by the pre-override tilt
(129.1° and 106.7° before the two real presses on tape 91, vs 0.0° at tick 0).

### D-B-4 — There is no intermediate steering position. **MEASURED. Corpus-wide.**

Across **206 minutes, 84 tapes, 1,483,776 ticks and 1,574 separate steer
excursions into the 0.3–0.7 band, the longest one lasted 0.500 s.** Band dwell
p99 is 0.305–0.365 s in every bucket. The lean axis, on the same tapes and the
same band definition, holds up to **36.1 s**.

The steer axis is a keyboard ramp: `|steer|` p50 = 0.005 and p90 = 0.995 — the
median steer input is *zero* and the 90th percentile is *full lock*, with a
near-uniform ramp in between whose transit time (≈0.185 s across a 0.4-wide
band ⇒ ≈2.2/s) is the input ramp rate, not a held position.

**Era note (MEASURED), and it is a clean step:** of the 40 parsed tapes numbered
≤ 47 (to 2026-08-29), **0 contain a single steer-band tick** — steer was
strictly binary, 0 or ±1. Of the 44 tapes numbered ≥ 48 (from 2026-09-03),
**43 do**; the one exception, tape 52, is 0.12 s long. `steer_rate_per_s` = 2.0
in the header of *every* tape in the corpus, so the kernel's steer actuator
never changed — **the ramp was added in the input layer between tape 40 and
tape 48.** Any cross-era comparison of steer duty therefore compares two input
encodings, not two kernels.

**Killing mutation:** a single steer sample held in [0.3, 0.7) for more than
0.5 s anywhere in the corpus falsifies the headline. Also: sorting each tape's
`|steer|` samples ascending leaves the *tick fraction* identical while the
dwell p50 explodes — which is exactly why the 20 % band-occupancy figure, taken
alone, is not evidence that an intermediate band is used.

### D-B-5 — Above ~10 m/s, full lock turns *less* than three-quarter steer. **MEASURED (the numbers) / DERIVED (the cause).**

Mean |yaw rate| about local up, ground contact only, speed > 2 m/s, pooled
exactly. Tonight (tapes 86–91):

| speed m/s | steer 0.1–0.3 | 0.3–0.6 | 0.6–0.9 | **0.9–1.0** |
|---|---|---|---|---|
| 0–5 | 0.205 | 0.275 | 0.385 | **0.624** ✓ |
| 5–10 | 0.353 | 0.363 | 0.466 | **0.590** ✓ |
| 10–15 | 0.373 | 0.551 | **0.648** | 0.452 ✗ |
| 15–20 | 0.222 | 0.291 | **0.370** | 0.281 ✗ |
| 25–30 | 0.190 | 0.102 | 0.130 | 0.131 |
| 30+ | **0.245** | 0.252 | 0.197 | **0.108** ✗✗ |

At 30+ m/s full lock yields **less than half** the yaw rate of 0.6–0.9 steer,
and less than the *lightest* steer band. The same inversion appears in the v15
bucket (15–20: 0.707 at 0.6–0.9 vs 0.600 at full; 20–25: 0.566 vs 0.241) and
corpus-wide from the 10–15 band up. Below 10 m/s the ladder is clean and
monotone — a 3.0× spread from light to full lock.

> Chad, §0: *"Just allow the balance of body mechanism to **ENHANCE ability,
> i.e. tighten a turn** instead of having to be the necessary condition of not
> rolling over."*

Tape event: `sled_tape_91`, `steer_sat` 33 % — the highest of the night — with
23 rollovers; `sled_tape_89`, `steer_sat` 11 %, zero rollovers.

**Killing mutation (RUN):** shuffle the steer column across ticks. Every cell
collapses to a flat ≈0.26 (low speed) / ≈0.13 (30+) — the ladder *and* the
inversion are both pairing-driven, not binning artefacts.

**⚠ CONFOUND, stated plainly.** The 0.9–1.0 cells are 4–8× oversampled
(n = 2052 vs 266 at 30+) because the keyboard puts him there by default, and
because he reaches for full lock *when the machine is already not turning*.
**The tapes establish the correlation; only the probe (`seads_sled_probe`, a
commanded steer sweep at fixed speed) can establish the direction of
causation.** That probe leg is owed and is NOT claimed here.

### D-B-6 — The groomed trail is the rollover surface. **MEASURED.**

**16.88 rollovers per minute on TrailMain vs 4.13 on Bush and 0.85 on LakeIce**,
pooled over the whole corpus. Per-tape sign test: of the **12** tapes carrying
more than 20 s of both TrailMain and Bush, **TrailMain's rate exceeds Bush's in
12 of 12** (p ≈ 2⁻¹² ≈ 0.00024 under equal rates), spanning four kernel buckets
and four weeks — tapes 1, 8, 14, 15, 16, 26, 29, 34, 37, 61, 91 plus 4.
Tonight's tape 91: 7 rollovers in 27 s of trail (15.6/min) against 11 in 106 s
of bush (6.2/min). Counterexample worth chasing: tape 67, 131 s of TrailMain
with **1** rollover.

Tape 90 is the clean control in the other direction: **90 s on lake ice with
zero rollovers**, while all 7 of its rollovers happened in its 19 s of bush
(22/min).

**This is the strongest surface-localised signal in the corpus and it points
away from the kernel** — a groomed trail is where a snowmobile should be most
planted. It is consistent with (but does not prove) a geometric lip at the
trail boundary; the road-repair lane's open F2 junction-cut item and Chad's
*"lines still flash at intersections"* sit in the same neighbourhood.

**Killing mutation:** attribute each rollover to the surface at *exit* rather
than entry → if the rate on TrailMain collapses, the machine was merely
crossing the trail, not rolling on it. **NOT RUN — owed.** Until it is run,
treat "on the trail" as "at the trail" (see §7).

### D-B-7 — The catwalk is bimodal: flat or over the back. **MEASURED.**

Under `stand > 0.5 AND lean_fwd < −0.30 AND throttle > 0.90 AND air_s == 0`
(ground contact — an airborne backflip is not a catwalk), tonight's pitch is
p50 **0.75°**, p90 9.25°, p95 14.25°, **p99 66.75°**, max 85.4°. The condition
holds 11.9 % of drive time. There is no populated band between "nose down on
the snow" and "past 60° and going over": the distribution has a spike at zero
and a tail straight to the flip.

> Chad, §0b: *"It should be arcadey to a degree so that it is fun., **SLiding
> banging, punchy, jumps.** Just make it more stable."*

Tape event: `sled_tape_86` catwalk p95 = 66.2° with 14.7 % duty; `sled_tape_87`
p95 = 3.8° at nearly the same duty (15.6 %) — the same commanded pose produced
either nothing or a flip on two tapes four minutes apart.
**Killing mutation:** drop the `air_s == 0` clause → p95 jumps as airborne
backflips contaminate the sample; that clause is what makes this a catwalk
measurement at all.

### D-B-8 — Throttle and brake are near-binary; braking is hard. **MEASURED / LITERATURE.**

Tonight: WOT 52 %, closed 40 %, **modulated (0.05–0.95) only 7.3 %**. Thumb
ramp rate is bimodal at ≈5/s and ≈12/s (two discrete ramps; 12/s = 0→1 in
83 ms). Brake duty 8.7 % — the highest of any bucket, up from 3.8 % at v13g.
Braking deceleration p50 4.05, p90 7.85 m/s².

7.85 m/s² is **0.80 g of retardation on snow**. A real snowmobile on packed
snow is in the 0.4–0.5 g neighbourhood (**LITERATURE**, not measured here; the
dynamics strand owns this number). Flagged for R1, not ruled on.

**Killing mutation:** compute decel from `belt_speed_ms` instead of
`ground_speed_ms` → track slip enters the number and the p90 must move.

### D-B-9 — Landings are violent and mostly fail. **MEASURED.**

58 air events tonight (4.82/min), hang p50 0.39 s, max 2.43 s, **26 of 58
landings above 10 g** peak |g_eff|, max 600.9 m/s² (61 g, tape 91). Only 29.3 %
land upright. Corpus |a_body| exceeds 100 m/s² on 2,422 ticks (0.163 %); per-tape
maxima reach 2561 m/s² (tape 16), 1766 (tape 88, tonight).

**Limitation, stated:** the `a_body` histogram ceiling is 200 m/s², so any count
above that bin is invalid — only the exact per-tape `max` is trustworthy up
there. Counts quoted here stop at 100 m/s².
**Killing mutation:** score landing attitude at touchdown instead of +0.5 s →
upright fraction rises (tape 91: 29 % → 33 %). The +0.5 s form is the one that
answers "did it *stay* on its skis."

### D-B-10 — The arcade roll-stability moment is live almost always, and small. **MEASURED.**

`assist_nm` (the `roll_stiff`/`roll_damp` torque, `sim/sled.cpp:1819–1885`) is
non-zero **86 % of drive time** tonight, but its magnitude is p50 **45 N·m**,
p95 575 N·m, max 9157 N·m. The "86 %" is an artefact of the term being non-zero
whenever φ ≠ 0 with ground contact; the *size* is the honest readout, and the
median is small against a 331 kg machine. This is the §0b arcade-lawful term,
not a rider governor — it adds a body-z torque and never moves the rider's mass,
so §0b's *"no I dont like thge idea in the handoff at all! It will ruin the feel
to have a governor"* is **not** violated by it.

**Killing mutation:** report the active fraction alone (as the first pass did)
→ "86 % assisted" reads as a heavy hand; the magnitude histogram shows it is
not. This finding exists because the fraction alone was misleading.

### D-B-11 — Bucket-to-bucket rollover rates are NOT a kernel ranking. **DERIVED.**

v13g 6.30/min → v15 2.16/min → v17 4.32/min looks like a regression tonight. It
is confounded at least four ways, all measured: surface mix (v15 was 74 % bush
and 0.2 % trail; tonight was 4 % trail + 6 % road, and trail rolls at 4.1× bush
— D-B-6); stand duty (9.8 % v15 vs 25.6 % tonight); speed (p90 19.9 vs 27.1 m/s);
and the input-encoding change at tape 48 (D-B-4). **No kernel verdict may be
read off this table.** The surface-controlled comparison — same surface, same
speed band, across tags — is owed and is the right instrument.

---

## 6. WHAT THE TAPES CANNOT SETTLE

- **RC-1** (*"A plain full-lock turn at trail speed with hands-off weight must
  NOT roll"*) — Chad never drives hands-off, so the corpus contains no such
  trial. Probe-only.
- **Causation in D-B-5** — see the confound there. Probe-only.
- **Grip release / superman events** — `sim/sled.cpp:2046` forbids reading a
  capacity off a tick-level subsample; the kernel runs the grip law at 1440 Hz
  and the tape pins at 120 Hz. The tool reports `grip_lower_bound`
  (extension p95 0.405 m, load p95 4.5 tonight) and **refuses to convert it to a
  release count**. Anyone who needs release counts must instrument the kernel.
- **Whether TrailMain rolls are *on* the trail or *at its edge*** — needs the
  exit-surface mutation of D-B-6.
- **Tremor / knife-edge ringing** — `sim/sled.cpp` runs 12 substeps at 120 Hz;
  the tape pins once per tick, so anything above 60 Hz is aliased. No spectral
  claim is made from `a_body` beyond the amplitude percentiles quoted.

---

## 7. THE ACCEPTANCE BAR VS THE TAPE

`ROLL_COMFORT_HANDOFF.md` §0/§0b, verbatim, scored against tonight:

| Chad's word (verbatim, §) | tonight | met? |
|---|---|---|
| *"I rule that it should be roll resistant"* (§0) | 4.32 rollovers/min, 5.7 % of time inverted | **no** |
| *"Make it possible to roll but not the rule"* (§0) | one every 13.9 s | **no** |
| *"allow me to land on my skis more often after a roll"* (§0) | 29.3 % upright, vs 27.6 % at v13g | **no movement** |
| *"self righting by chance more"* (§0b) | 71 % never recover | **no** |
| *"balance of body mechanism to ENHANCE ability, i.e. tighten a turn"* (§0) | lean holds (to 36 s) and does enhance below 10 m/s; steer has no intermediate position at all and inverts above 10 m/s | **half** |
| *"SLiding banging, punchy, jumps"* (§0b) | 4.82 air events/min, 26/58 landings over 10 g, top 164.8 km/h | **yes** |
| *"Don't lose the feel"* (§0) | speed envelope, jump rate and catwalk duty are all at or above the v13g-era values | **yes** |
| *"DOnt make it impossibly hard, there is a batttle going on as well"* (§0b) | a machine inverted 5.7 % of the time, un-recovered 71 % of the time, cannot be fought from | **no** |

---

## 8. TOOL NOTES — bugs found and fixed in this strand

`tools/sled_tape_audit.py` gained four readouts in pass 2 (`band_dwell`,
`input_events`, `assist_nm_abs`, `roll_by_surface`). Two bugs were found in
**my own new code** by independent recompute before anything was published:

1. **Band runs still open at EOF were never flushed.** Tape 86's longest held
   lean — 12.77 s, the tape's maximum — was dropped, and the metric reported
   4.77 s: a 2.7× understatement of exactly the quantity the metric exists to
   measure. Fixed by flushing both dwell accumulators after the read loop.
2. **`band_dwell` shipped `out(bins=False)`.** The cross-tape aggregator pools
   raw bins; with the bin array absent, the pooled percentile silently returned
   the last bin edge (59.995 s) instead of failing. Fixed by shipping bins.

Both are the same failure class: **a metric that is wrong but plausible-looking
is worse than one that crashes.** The independent-recompute step in §1.2 is
what caught them, not review.

Runtime: 84 tapes, 11.79 GB, **12.6 s wall** on 6 workers (~241 core-seconds).
`--only` and `--max-ticks` restrict a run; `--jobs 1` is the debuggable path.

---

*Strand D-B, sled ride audit, 2026-09-18. Read-only against main. No dial
changed, no config edited, no goldens moved.*
