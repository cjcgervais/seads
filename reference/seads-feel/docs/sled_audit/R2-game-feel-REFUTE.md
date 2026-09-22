# R2-game-feel — ADVERSARIAL REFUTATION PASS

Read-only. No dial moved, no config touched, no build run, nothing pushed.
Every number below was recomputed from the tapes by an independently written awk
pass (scratch scripts, not committed), not copied from `R2-game-feel.md`.

Tapes read: `D:/flight_sim2/seads-recon/build-play/sled_tape_{1,8,16,34,86,87,88,89,90,91}.sledtape`
Source read: `D:/seads_sandboxes/sled-audit/sim/sled.h`, `sim/sled.cpp`,
`test/harness/sled_tape.h`, `config/scenario.toml`.

**VERDICT: SOLID_WITH_FIXES.** 4 of 15 findings refuted (two of them headlines).
None of the refuted items would have moved a dial in the wrong direction — the
strand's two operative recommendations (R2-C0 "re-ask Chad first", R2-C2
"frequency is the open half") survive intact and are strengthened. But two
headline findings carry a defect a reader would act on.

---

## 0 — Column contract re-derived independently

`SLEDTAPE_PIN_D` (test/harness/sled_tape.h:98-107) is 41 doubles, then
`surface` (int) and `rolled` (0/1) written explicitly by `pin_fields`
(sled_tape.h:279-283). T record = `T` + tick + 6 inputs + 3 cold + 41 + 2 = **NF 54**,
confirmed on every tape read.

From the end: `$NF`=rolled, `$(NF-1)`=surface, `$(NF-2)`=hull_engage_lp,
`$(NF-3)`=assist_nm, `$(NF-4)`=air_s, `$(NF-5)`=rolled_hold_s,
`$(NF-7)`=ground_speed_ms. R2-V1's "item 36 / 38 / 39" is correct as pin-item
ordinals.

**Killing mutation for this contract:** adding or reordering any `SLEDTAPE_PIN_D`
entry shifts every index. NF==54 is the tripwire; re-check it after any
`SledState` change.

---

## 1 — REFUTED: R2-H1 — the "on ground contact" gate DOES NOT EXIST

R2-H1 describes the shipped assist as *"gated on low-passed speed < 1.389 m/s
(hysteretic, re-arms at 0.8x), on tilt (ramp, zero below 14.3 deg, full at 20
deg), **on ground contact**, driven by the player's own `stand` input"* and
labels the whole finding **MEASURED (source text quoted verbatim)**.

I read the entire block, `sim/sled.cpp:1679-1800`. The gate set is:

    1679:  if (p.comfort.right_assist_nm > 0.0 && hands_on) {
    1703:      const double push = clamp01(static_cast<double>(in.stand));
    1704:      const double p_eff = (s.right_assist_armed ? 1.0 : 0.0) * push * w_tilt;
    1718:      if (p_eff > 0.0) { ... torque_body.z += s.right_assist_nm_now; }

`right_assist_nm > 0` / `hands_on` / `armed` (low-passed horizontal speed,
hysteretic) / `in.stand > 0` / `w_tilt` (smoothstep 0.25 to 0.35 rad).
**There is no ground-contact term anywhere in the block.** `grep -n
ground_contact sim/sled.cpp` returns exactly one hit between lines 1400 and
2000 — line 1503, in the contact loop, unrelated to this block.

Consequence the fabricated clause hides: a machine that is **airborne**, tilted
past 14.3 deg, with low-passed *horizontal* speed under 1.389 m/s and `stand`
held, **will receive the 2400 N·m righting torque mid-air.** The low-pass (tau
0.5 s, `right_speed_lp_s`) makes this narrow in practice — the speed reading has
to have been low for ~0.5 s — but it is not zero, and the honest statement is
"the speed gate is the only thing standing in for a contact gate", not "it is
gated on ground contact".

**What survives:** everything else in R2-H1 is verbatim-correct. I reproduced
each quote at its cited line: "IT CANNOT HAPPEN ABOVE 5KM/H" (sled.h:225),
"MACHINE NEEDS TO BE TIPPING OVER" (sled.h:231), the v2 quote (sled.h:234-236),
`right_assist_max_ms = 5.0 / 3.6` (sled.h:229), the ~0.93 s rock half-period
(sled.h:263), and `right_assist_nm = 2400.0` at **config/scenario.toml:2085**
(sole occurrence). The three-way "not a governor" argument in `why_it_matters`
(upright / at speed / without `stand`) does not depend on the fabricated clause
and stands.

**FIX:** strike "on ground contact" from the Shape list; replace with
"**no ground-contact gate** — the hysteretic low-passed *horizontal* speed gate
is the only proxy, so a slow-horizontal airborne attitude can arm it."

**Killing mutation for THIS refutation:** a `ground_contact` or `normal_sum`
term inside `sim/sled.cpp:1679-1800` that I missed. I read all 121 lines of the
block and grepped the identifier across 1400-2000; a rename of the contact flag
inside the block would defeat the grep but not the read.

---

## 2 — REFUTED: R2-V4 — the inverse duration/entry-speed law fires its own killing mutation

R2-V4's stated killing mutation: *"pooling all 15+ tapes and finding the
duration/entry-speed rank correlation not significant would kill it."*

I did not even need to pool further. On **the finding's own n = 22 long
episodes** (tapes 1 and 34, the exact pairs it tabulates):

    Spearman rho = -0.216   t = -0.988   df = 20   ->  p ~ 0.33

Not significant. The "inverse law" is a ranking the data does not support, and
the finding named exactly this test as fatal. Direct counterexamples inside its
own table: tape 1 EP18 entry 8.66 m/s -> 2.63 s (low entry, *short*); tape 34
EP25 entry 6.18 -> 1.68 s; tape 1 EP17 13.88 -> 4.16 s against EP20 16.20 ->
9.02 s (higher entry, *longer*).

**What survives, and it is the load-bearing half:** the *dig-in* reading is
MEASURED and I reproduce it exactly. Tape 1, 13 long episodes: mean
`ground_speed_ms` at -2.0 s = **13.68 m/s** (R2-V4 says 13.7), **zero** with a
prior-3.3 s max below 8 m/s. Tape 34, 9 long episodes: mean at -2.0 s =
**12.54** (says 12.5), **one** below 8 (EP25, 6.18). Long rolls are not
standing tip-overs. R2-C2 leans on this half only, so R2-C2 is unaffected.

The two **one-sided bounds** in the finding are also true on the data and are
the honest survivable form:

* every episode with prior-3.3 s max above 19 m/s lasted under 4.2 s
  (42.24->4.16, 22.58->3.73, 19.71->1.48, 22.51->2.04, 25.32->2.77, 19.65->2.78);
* every episode over 9 s had entry below 16.3 m/s
  (16.20->9.02, 12.53->10.22, 11.71->14.66, 12.29->15.32, 9.24->53.60).

**FIX:** delete "roll duration is inversely related to the speed carried into
the roll" and the ordered table that dresses it as a law. Keep the mean-entry
figures and replace the law with the two one-sided bounds above, labelled
DERIVED with n and the rho.

**Minor transcription errors in the same finding** (recomputed): prior-max
**42.24** not 42.4; **11.71** not 11.8. The rest of the table is digit-exact.

**Killing mutation for THIS refutation:** a different, defensible definition of
"entry speed" — e.g. the instantaneous value at onset-0.3 s (the debounce
offset) rather than the 3.3 s prior max — producing a significant rho. I used
the finding's own 3.3 s window so the comparison is like-for-like.

---

## 3 — REFUTED: R2-H2 — the "09-17 pooled" row silently drops 2 of the 6 same-day tapes

Every cell R2-H2 states is digit-exact against my independent pass, including
the one that needed an interpretation: tape 1 has **14** `O` records, the first
at tick 0 matching the first `T` at tick 0 — that is the initial-state record
the writer emits in `begin()` (sled_tape.h:141), so **13** are player
overrides, as claimed.

The defect is sample selection. `R2-game-feel.md` section 11.3's table and the
`09-17 pooled` row use tapes **87 / 89 / 90 / 91** only. Tapes **86 and 88** are
the same date, the same `right_assist_nm 2400` header, and line 1240 of the
report confirms they were read. Neither the finding nor the report says they
were excluded or why.

| set | drive_s | rolled % | episodes | one per | >=1 s | >=5 s | longest | real overrides | /min |
|---|---|---|---|---|---|---|---|---|---|
| tape 1 (OFF) | 809.9 | 16.78 | 37 | 21.9 s | 13 | 5 | 53.60 s | 13 | 0.96 |
| 87/89/90/91 (as published) | 527.0 | 3.67 | 22 | 24.0 s | 4 | 1 | 5.09 s | 2 | 0.23 |
| **all six 86-91** | **721.3** | **3.81** | **36** | **20.0 s** | **7** | **1** | **5.09 s** | **5** | **0.42** |

Tapes 86 and 88 are the two highest-onset-rate drives of the day (6 episodes in
95.2 s and 8 in 99.1 s, i.e. one per 15.9 s and 12.4 s). Including them:

* **"overrides per minute 0.96 -> 0.23 (4.2x)" becomes 0.96 -> 0.42 (2.3x).**
  This is the headline number that shrinks most, and it is the one that would
  be quoted at Chad as "the R key is nearly retired".
* **"one onset per 21.9 s -> 24.0 s, within noise" becomes 21.9 s -> 20.0 s.**
  The conclusion is *unchanged in direction and strengthened*: frequency did
  not improve; on the full same-day set it is nominally slightly worse.
* Severity: rolled share 4.6x -> **4.4x**; longest 53.60 -> 5.09 s unchanged;
  episodes >=5 s 5 -> 1 unchanged.

So R2-H2's **two conclusions both survive** — severity collapsed, frequency did
not move — but two of its stated deltas are selection-dependent and the
selection is undisclosed. A finding labelled MEASURED must publish its sample.

**FIX:** publish the all-six row as the primary and the four-tape row as a
sensitivity check, or state the exclusion criterion for 86/88 in the finding.
Restate the override delta as **0.96 -> 0.42 per minute (2.3x)**.

**Killing mutation for THIS refutation:** a documented reason 86 and 88 are not
free-riding drives (e.g. a crash tape, a deliberate rollover test, a different
`world` header). I checked the headers — same `right_assist_nm 2400`, same
`rolled_persist_s 0.3`, same dt/substeps — and found no such marker. If Chad
says 86/88 were rollover tests, the published row is correct and this refutation
falls.

---

## 4 — REFUTED: R2-21 — `k_air_shift` IS the rider-mass term, by the finding's own test

R2-21's stated killing mutation: *"Reading k_air_shift's derivation in full and
finding it is already a rider-mass term rather than a free impulse, which would
make this a no-op observation."*

I read it. `sim/sled.cpp:2077-2102`, verbatim from the source:

> // K-WS1 / K2 -- THE AIRBORNE MOMENTUM EXCHANGE, SPENT HERE.
> // I*omega + L_exch is conserved about the system CG while no running
> // surface and no hull point is touching, so the chassis takes exactly
> // the NEGATIVE of the momentum the rider's body just spent
> // Note there is no else and no decay: the delta TELESCOPES ... The net
> // effect of a full throw is an ATTITUDE change, never a rate.

`exch_l_now` is built at sim/sled.cpp:467 from the rider's body state. This is
precisely the Trials mechanism the finding recommends reaching for — "route air
authority through the rider's weight shift rather than through a free torque" —
and it is already the mechanism, shipped at 0.0.

The cited line numbers are exact (`double k_air_shift = 0.0;` **is**
sim/sled.h:859; the guard **is** sim/sled.cpp:2096-2098) and the Trials
literature half is unaffected. What is wrong is the framing: the finding sets
`k_air_shift` up as the free-impulse alternative to the honest path when it is
the honest path.

**FIX:** restate as "the honest Trials-shaped route is already built and shipped
off (`k_air_shift = 0.0`, bit-identical when off per sim/sled.h:856); the open
question is Chad's ruling on turning it on, not which mechanism to build."
Note also that the finding's quoted `lean_lat_seated_m 0.15 /
lean_lat_stand_m 0.35` are the lean *amplitudes*, not an air term — they do not
support the air-authority argument on their own.

**Killing mutation for THIS refutation:** `exch_l_now` at sim/sled.cpp:467 not
being derived from rider mass/position. I read the comment block but not the
full arithmetic at 467.

---

## 5 — RESOLVED IN THE STRAND'S FAVOUR: `ground_speed_ms` is world-frame

The strand's third-listed unverified — *"ground_speed_ms FRAME NOT READ"* —
named as a killing mutation for **both** R2-V4 and R2-H3. It resolves, and it
resolves the way the strand hoped. `sim/sled.cpp:2147-2149`:

    const glm::dvec3 up_f = glm::normalize(s.position);
    s.ground_speed_ms =
        glm::length(s.velocity - glm::dot(s.velocity, up_f) * up_f);

World-frame horizontal speed (velocity minus its component along local up).
Not body-frame, not track-derived. A machine sliding on its side reads its true
sliding speed, so the "below the gate" shares are **not** inflated by a sliding
machine reading ~0.

It is also the *same* quantity the shipped gate reads
(`v_h_r = s.velocity - dot(s.velocity, up_cg) * up_cg`, sim/sled.cpp:1689-1691),
modulo `up_cg` vs `up_f` and the 0.5 s low-pass — so R2-H3 comparing the raw
column against 1.389 m/s is apples-to-apples up to that low-pass. The remaining
honest caveat is the low-pass alone, not the frame.

**Move this out of `unverified`.** Both R2-V4's dig-in half and R2-H3 get
stronger, and R2-H3 loses its only stated killing mutation — which means R2-H3
now needs a new one. Proposed: *`up_cg` diverging from `up_f` by enough to
matter, which would require a CG offset comparable to the planet radius.* That
is a decorative mutation and should be labelled as such; R2-H3 is effectively
unkillable as stated, which is itself worth saying out loud.

---

## 6 — CONFIRMED, digit-exact

Recomputed independently; every figure below reproduces.

**R2-V1 — SOLID.** Tape 34: ticks **66134**, drive_s **551.1**, meanV **16.06**,
maxV **44.82**, >10 **66.6%**, >15 **42.2%**, >20 **24.5%**, air **6.7%**,
rolled **3564 (5.39%)**, rolled-in-air **310 (8.7%)**. Tape 1: **97184** /
**809.9** / **11.08** / **44.16** / **16304 (16.78%)** / **37** episodes /
longest **53.60 s** / total **135.9 s**. Every cell matches.

**R2-V2 — SOLID.** `sim/sled.cpp:2131-2135` reproduces verbatim, including the
CG-proximity clause. `cg_height_m` reads **0.564** in every tape header, so
1.2x = **0.677 m**, i.e. "~0.68" as claimed. The floor-not-ceiling reading is
sound.
*Fix:* the stated range "3-9%" should read **3.0-9.5%** — tape 8 measures 9.5%.

**R2-V3 — SOLID, one fix.** `sim/sled.cpp:255-263` reproduces verbatim;
`cos(1.31)` = 75.06 deg. Header grep confirms `rolled_persist_s
0.29999999999999999` and `rolled_grace_s 0.20000000000000001` on **all ten**
tapes read including tapes 1 and 8 (08-19, 08-21) — so the debounce was live
before Chad's oldest tape and the charter's "rolled fires mid-air" complaint is
indeed already fixed. The ~0.45 s decay-tail arithmetic (grace 0.2 + 0.5/2)
is correct.
*Fix:* the stated in-air share "1.3-18.1%" is the range over tapes 1/16 only.
**Tape 90 measures 37.2%** (110 of 296 rolled ticks airborne). Either qualify
the range to its tape set or restate as **1.3-37.2%** across the ten tapes read —
which also means R2-05's "81.9-98.7% of rolled time is in ground contact"
becomes **62.8-98.7%**. The mechanism explanation still holds: tape 90's
episodes are all short (longest 1.06 s, total 2.5 s), so a fixed ~0.45 s tail is
a large fraction of each.

**R2-H3 — SOLID, digit-exact.** Reproduced: tape 1, **88.3%** of all rolled time
and **92.2%** of long-episode time below 1.389 m/s. Per episode: EP37 (53.60 s)
**99.0%**; EP4 (14.66 s), EP20 (9.02 s), EP18 (2.63 s) **100.0%**; EP3 (4.16 s,
prior-3.3 s max 42.24) **8.0%**. Strengthened by section 5 above.

**R2-C0 — SOLID.** The tape deltas it cites are the ones I reproduced. The
recommendation ("re-ask Chad before re-tuning; the acceptance-bar quotes are
from 2026-08-12 and describe a machine that no longer exists") is the safest
finding in the strand and survives every refutation above — including R2-H2's,
since the severity collapse is the part that held.

**R2-C2 — SOLID as re-scoped.** Its evidence chain runs through R2-V4's *dig-in*
half (survives) and R2-H2's *frequency* conclusion (survives, and is nominally
worse on the full same-day set). It does not depend on the refuted inverse law.

**R2-17 / R2-18 / R2-19 / R2-20 / R2-22 — NOT REFUTED, sourcing caveat.** All
five are labelled LITERATURE, carry URLs, and are correctly flagged in the
strand's own `unverified` as resting on fetched summaries rather than full
corpora. I did no independent web verification in this pass (read-only source +
tape mandate), so I refute none of them.
*One fix, R2-22:* the design line "Every assist trades control for safety" and
"The Horizon games are about freedom. So forcing things isn't how they get
down" are sourced to `forzahorizonhub.com` and `ludo.guide` — third-party guide
sites, not Turn 10 or Playground Games. Attributing them to "Forza" and then to
"the industry's own vocabulary" overstates. Either re-source to a first-party
Turn 10/Playground statement or restate as "third-party Forza guides frame it
as...". The substantive claim (Forza assists are optional and named) is not in
doubt.

---

## 7 — Signed-law check

No finding in this strand contradicts the signed law. Checked explicitly:

* **depth 0.77 m fixed** — no finding touches snow depth.
* **no governor** — R2-H1's conclusion is that the shipped assist is *not* a
  governor; the fabricated ground-contact clause overstated the gating but in
  the safe direction (it made the assist sound *more* constrained than it is,
  so nobody would loosen a gate on the strength of it).
* **one surface** — untouched.
* **wheelie kept / ragdoll banned** — untouched; R2-H1 quotes the kernel's own
  refusal of a hidden nudge, which is on-law.
* **No dial moved, no config edited, no golden touched, nothing built, nothing
  run.** `sim/`, `control/`, `config/`, `test/golden/` unmodified in this pass.

---

## 8 — Still open after this pass

1. **The causal confound stands.** R2-H2's A/B is across five weeks and several
   kernel versions. The clean experiment (replay tape 1 through
   `seads_sled_probe` at `right_assist_nm` 0.0 vs 2400.0, compare episode-duration
   histograms) was not run here either — strand D-B's lane. **No dial may move on
   11.3 alone**, and the correct override delta for that argument is 2.3x, not 4.2x.
2. **Short-tape bias unresolved**, and now sharper: the two excluded 09-17 tapes
   are among the highest onset rates of the day. Ask Chad what he was doing in
   tapes 86-91 before any of them is used as a free-riding baseline.
3. **The drift is still unmeasured** (charter RC-3). Carried over unchanged. It
   remains the most important open item after the confound.
4. **R2-H3 has no live killing mutation** now that the frame question is
   resolved. It should say so rather than carry a decorative one.
