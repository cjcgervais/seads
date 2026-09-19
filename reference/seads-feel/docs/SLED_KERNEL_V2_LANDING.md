# SLED KERNEL v2 — THE FIVE DRIVEN VALUES ARE THE SHIPPED DEFAULTS

**Chad, 2026-09-18, verbatim:** *"yes tyo all 7  and all 3 of these reccomendations I concurr I want
this all in a v2"*

Lane `feel/sled-ride-firstbuild`, worktree `D:/seads_sandboxes/sled-ride-b1`. **Main is not touched.**
The drive record is `docs/SESSION_HANDOFF_20260918_sled_firstbuild_DRIVEN.md` (his word per run) and
`docs/SESSION_HANDOFF_20260918_sled_firstbuild.md` (the build, the red-team fold, the gate).

---

## §1 — WHAT CHANGED, BY FILE AND LINE

### 1.1 The five shipped values

| dial | identity (pre-v2) | SHIPPED v2 | where it ships from | his word |
|---|---|---|---|---|
| `params.traction_mu` | `0.0` | **`3.0`** | `sim/sled.h:1088` | run 2: *"a little different jump was more stable right at the start ... 2 is approved"* |
| `params.track_lat_slip_shed` | `0.0` | **`1.4`** | `sim/sled.h:1334` | run 6a/6b/6c: *"it needs more fishtail"* / *"a little more fishtail even still"* / **"okay good"** |
| `comfort.rolled_throttle_frac` | `0.0` | **`0.15`** | `sim/sled.h:257` | run 3: *"NOTICED NO DIFFERENCE"* → shown the tape → **"3 IS APPROVED"** |
| `comfort.right_assist_max_ms` | `1.3888888888888888` | **`4.0`** | `config/scenario.toml:2097` | run 4: *"4 is approved I can land upright more often"* |
| `comfort.right_stand_shift_frac` | `1.0` | **`0.5`** | `config/scenario.toml:2131` | run 5: *"5 is approved"* |

Combination, run 7: **"yes very good"**. Each line carries the one-line provenance comment
`SLED KERNEL v2, Chad-driven 2026-09-18, identity = <old>`.

### 1.2 ⚠ THE SPLIT, AND WHY IT IS NOT AN INCONSISTENCY

**Three dials ship from `sim/sled.h`; two ship from `config/scenario.toml`.** That is not a choice of
taste, it is where the loaded value actually comes from:

- `SledParams` has **no TOML bridge at all** (`app/main.cpp:2478` bridges only
  `sled_params.comfort = scen.sled_comfort;`), so for `traction_mu` and `track_lat_slip_shed` the
  struct default **is** the shipped table.
- `rolled_throttle_frac` is a `SledComfort` field with **no TOML key**. It survives the comfort bridge
  untouched, so its struct default is likewise the shipped value.
- `right_assist_max_ms` and `right_stand_shift_frac` **are** `require`d TOML keys, so the TOML line
  wins and editing the struct would change nothing the game reads.

**The struct defaults for those last two are deliberately LEFT at the identity** — and it turns out to
be load-bearing twice over: they double as the TAPE-ABSENT reconstruction (§2), and
`scenario_sled_comfort_matches_kernel_defaults` (the bit-neutral-load leg) never covered them, so
nothing had to be relaxed there. The divergence is stated in four places so nobody "repairs" it:
`sim/sled.h:271` and `:382`, `config/scenario.toml` beside each key, `config/load_scenario.cpp:480`,
and the leg `sled_kernel_v2_defaults_are_the_driven_values` (iii).

### 1.3 The env vars are kept, and are now OVERRIDES

Every `SEADS_SLED_*` still works and now overrides a live default (`app/main.cpp`). The whole pre-v2
machine is one launch line:

```
SEADS_SLED_TRACTION_MU=0 SEADS_SLED_TAILSHED=0 SEADS_SLED_ROLLED_THROTTLE=0 \
SEADS_SLED_RIGHT_MAXSPD=1.3888888888888888 SEADS_SLED_STAND_SHIFT=1
```

MEASURED on `build-play/seads.exe`:

```
[config] sled first-build: traction_mu 0 rolled_throttle_frac 0 right_assist_max_ms 1.388888889
  right_stand_shift_frac 1 track_lat_slip_shed 0 (env: SEADS_SLED_TRACTION_MU=0
  SEADS_SLED_ROLLED_THROTTLE=0 SEADS_SLED_RIGHT_MAXSPD=1.3888888888888888
  SEADS_SLED_STAND_SHIFT=1 SEADS_SLED_TAILSHED=0)
```

The banner's no-env text changed from `none -- identity, bit-identical to main` (now false) to
`none -- SLED KERNEL v2 shipped defaults`. With no env set, on the play exe:

```
[config] sled first-build: traction_mu 3 rolled_throttle_frac 0.15 right_assist_max_ms 4
  right_stand_shift_frac 0.5 track_lat_slip_shed 1.4 (env: none -- SLED KERNEL v2 shipped defaults)
```

### 1.4 Loader

`config/load_scenario.cpp` gained the range check the six-touch landing path owes each landed dial:
`right_assist_max_ms >= 0` and `right_stand_shift_frac` in `[0, 1]`, mutation-proven on the real table
by the new leg `scenario_sled_comfort_rejects_a_v2_dial_out_of_band`.

---

## §2 — THE TAPE-ABSENT PROOF

### 2.1 What the harness did BEFORE the fix, read from the source

`test/harness/sled_tape.h` `load()` starts from a **default-constructed `SledParams`** and overwrites
only what the tape names (the `param` and `cparam` branches). So **a dial absent from an old header
silently takes TODAY'S STRUCT DEFAULT** — the harness's own comment (`:322-338`) has said so since
2026-08-26, and names the hazard exactly: *"The first dial that ships a NON-INERT struct default
silently rewrites the history of every older tape — and they would still report bit-exact, because
the pins were re-derived under the same wrong assumption."*

v2 is that first dial. Three of them.

**THE FIX IS AT THE READ**, in the existing OFF-by-absence block, as literal constants
(`test/harness/sled_tape.h:440-448`):

```
t.params.track_lat_slip_shed = 0.0;                // v2 default 1.4
t.params.comfort.rolled_throttle_frac = 0.0;       // v2 default 0.15
t.params.comfort.right_assist_max_ms = 5.0 / 3.6;  // shipped 4.0 (toml)
t.params.comfort.right_stand_shift_frac = 1.0;     // shipped 0.5 (toml)
```

`t.params.traction_mu = 0.0;` was **already there** (`:401`, GI4 §9.7) — but until today it agreed
with the struct default and nothing could tell them apart. Its comment now says it is load-bearing.
The last two change nothing today (their struct defaults *are* the identity); they are written so the
law does not rest on that coincidence a second time.

### 2.2 The run — nine tapes, before and after, byte-for-byte

`build/seads_sled_probe.exe tape <abs path>`, stdout+stderr captured. The "before" build is the lane
tip `13025fc28` (code `eae9829ad`) with nothing edited; the "after" build is this commit. His six
09-17 tapes in `D:/flight_sim2/seads-recon/build-play` were opened **read-only; nothing was written
to his fly tree.**

```
diff -r <pre>/ <post>/     ->  prints nothing
FINAL TREE: ALL NINE REPLAYS BYTE-IDENTICAL TO THE PRE-v2 BASELINE
```

| tape | replay | verdict | vs the table in `..._firstbuild.md` §2.1 |
|---|---|---|---|
| `test/golden/sled/tape_360_chad.sledtape` | 961/961, rolled tape=7188 replay=7188, dial gap 34 | bit-exact on a kernel this tape never saw | — |
| `test/golden/sled/tape_360_chad_attempt1.sledtape` | 901/901, rolled 1368/1368, dial gap 34 | bit-exact (same qualifier) | — |
| `test/golden/sled/tape_chad_flip.sledtape` | 661/661, dial gap 34 | bit-exact (same qualifier) | — |
| `sled_tape_86` | 11430/11430, dial gap 2 | bit-exact | **matches** |
| `sled_tape_87` | 14272/14272, dial gap 2 | bit-exact | **matches** |
| `sled_tape_88` | 1837/11896, GROUND KEY MISMATCH sample 186528, FIRST DIVERGENCE **tick 46589 `position.x`** | NOT bit-exact | **matches** |
| `sled_tape_89` | 6355/15256, FIRST DIVERGENCE **tick 68315 `velocity.x`** | NOT bit-exact | **matches** |
| `sled_tape_90` | 12990/12990, dial gap 2 | bit-exact | **matches** |
| `sled_tape_91` | 14833/20734, FIRST DIVERGENCE **tick 14832 `velocity.x`** | NOT bit-exact | **matches** |

The three that were already divergent on the lane base diverge at the **same tick and the same field**
— the differential form, which is stronger than pass/fail. 88/89/91 are pre-existing: their headers
were cut on `kernel-v17-…-33-g4b8c68698` / `-53-g7b377c6f0`, neither of which is this lane's base.
`test/golden/sled/` is **untouched** (`git status --short test/golden/` prints nothing).

### 2.3 THE NEGATIVE CONTROL — the fix is load-bearing, measured

The preset lines were deleted (including the pre-existing `traction_mu` one), the probe rebuilt, and
the same tapes replayed. **Every one broke:**

| tape | with the presets | with them DELETED |
|---|---|---|
| `tape_360_chad` | 961/961 bit-exact | **114/961**, GROUND KEY MISMATCH at 5435, FIRST DIVERGENCE tick 6773 `position.x` |
| `sled_tape_86` | 11430/11430 bit-exact | **1465/11430**, FIRST DIVERGENCE tick 1464 `belt_speed_ms` |
| `sled_tape_90` | 12990/12990 bit-exact | **8487/12990**, FIRST DIVERGENCE tick 87761 `belt_speed_ms` |

The harness was restored and re-proven identical before any of §3 was written.

---

## §3 — THE LEGS, AND (b) PER DIAL

### 3.1 Legs that pinned "the old value IS the shipped default" — now explicit identity constants

Five legs read their own baseline out of `sim::SledParams{}` or the shipped TOML and reddened the
moment the defaults moved. **That class was the entire red list** — no other leg in the subset moved.

| leg | what it asserted | what it asserts now |
|---|---|---|
| `sled_rolled_throttle_frac_zero_is_the_shipped_zero` → **renamed** `sled_rolled_throttle_frac_zero_is_the_identity` | `p.comfort.rolled_throttle_frac == 0.0` read off the default (`0.15 == 0.0` FAILED) | constructs `p.comfort.rolled_throttle_frac = 0.0` and **also** pins `SledComfort{}.rolled_throttle_frac == 0.15`, so it reds if the v2 default is walked back |
| `sled_tail_shed_hoist_is_a_pure_refactor` | `const SledParams p;` — a **17-digit absolute-trajectory golden** measured with shed `0.0` AND `traction_mu` `0.0` | constructs **both** identities explicitly. **The pinned numbers are unchanged** — nothing was relaxed; the baseline is stated instead of inherited |
| `sled_tail_shed_is_track_only` | `off.track_lat_slip_shed == 0.0` (`1.4 == 0.0` FAILED); the positive control's OFF arm also inherited | both OFF arms constructed at `0.0` |
| `selfright: above 5 km/h it cannot happen` → **renamed** `selfright: above the shipped speed gate it cannot happen` | drove a **hard-coded 4.0 m/s** against a 1.3889 gate; at the v2 gate of 4.0 the fixture sits exactly ON the threshold and never disarms | drives `right_assist_max_ms * 2.0`, **factor measured** (never disarms at 1.5x; disarms at 2x / 3x / 4x / 6x), and pins the shipped gate `== 4.0` |
| `selfright: rocking does not trip its own speed gate` | `gs_peak > shipped_gate * 0.5` → `1.9224 > 2.0` FAILED | the clause is measured against the **pre-v2 raw gate as an explicit constant** (`5.0/3.6`), which is the gate the hazard was measured against, plus a new upper bound naming the headroom v2 bought |

### 3.2 `sled_kernel_v2_defaults_are_the_driven_values` (NEW, `test/unit/test_load_scenario.cpp`)

Pins all five by name in the one place that can see both halves — three from the struct, two from the
loaded TOML — plus the two struct defaults that must **stay** at the identity, plus
`right_assist_nm == 2400.0` (the precondition §3.3's fixture hard-codes).
**Killing mutation:** any of the five moving.

### 3.3 `sled_kernel_v2_defaults_equal_the_env_arm` (NEW, `test/unit/test_sled.cpp`) — **THIS IS (b)**

⚠ **RE-CUT 2026-09-19 BY THE P1-1 FOLD — the numbers below are NOT the ones this section shipped
with.** As landed, arm A patched three comfort fields onto a default `SledComfort` and called that
"the machine the game builds"; five diverge, and the fixture it published (70 rolled ticks, 185150)
described a machine nobody builds. Arm A now reads `config/scenario.toml` through the real loader.
Full account, construction and mutation evidence in **§7.1**.

Arm A is the machine the game builds, **two lines for `app/main.cpp:2476-2478`'s two lines**:
`sim::SledParams shipped;` then `shipped.comfort = shipped_comfort();`, where `shipped_comfort()`
returns `cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", ap).sled_comfort` — the **whole**
loaded `[sled_comfort]` table, all 35 rows, exactly as `sled_params.comfort = scen.sled_comfort;`
assigns it. Arm B starts from the **same** two lines (he drove the shipped toml too), then puts each
of the five back to its pre-v2 identity by hand and arms it to its driven value as a **literal** —
the five `SEADS_SLED_*` assignments, in `app/main.cpp`'s own order. 600 ticks: 300 of a leaned,
steered, WOT carve from 40 m/s, then she is put over at 110°, then 300 with **W held and STAND
pressed**.

```
[v2 defaults==env] pos=134.71518045385346 68.660950818305892 6371000.8353244923
                   rolled_ticks=79 rpm_rolled_sum=208955 assist=7.15826
All tests passed (9 assertions in 1 test case)
```

Equality is over the **whole** `SledState` via `SLEDTAPE_PIN_D` (the tape's own roster, so new fields
join by construction), **plus** the rolled-window rpm sum — and that second observable is not
decoration: 79 rolled ticks × (1700 + 0.15 × 6300) = **208955 exactly**, the same **2645 rpm** the
drive tape showed when Chad held the throttle rolled over. The rolled-throttle arithmetic survived
the correction intact; only the tick count moved.

**(b) PER DIAL — the killing mutation, with its MEASURED sensitivity:**

| dial | mutation | result |
|---|---|---|
| `traction_mu` | `+1e-9` | **reds.** MEASURED first: this dial is *dark* at v0 = 5 / 10 / 20 m/s (separation exactly 0.0 between 0.0 and 3.0), 0.918 mm at 30, **8.87 m at 40** — which is why the fixture drives at 40 m/s. A 10 m/s fixture would have passed while blind |
| `track_lat_slip_shed` | `+1e-9` | **reds** |
| `rolled_throttle_frac` | `+1e-9` | **reds — via the rpm sum, not the final state.** On flat ground a downed track has no contact, so this dial reaches `engine_rpm` / `belt_speed_ms` and nothing else, and both are recomputed per tick and integrate into nothing. A final-state-only leg would have been vacuous here |
| `right_stand_shift_frac` | `+1e-9` | **reds** |
| `right_assist_max_ms` | `+1e-9` | **DARK, and reported rather than dressed up.** It is a THRESHOLD on a low-passed speed, not a gain; a nanometre-per-second move in the gate changes not one bit. **Not asserted.** Its real killing mutation — walking the key back to the pre-v2 `5.0/3.6` — the fixture sees loudly. ★ **AND SINCE THE P1-1 FOLD IT IS RUN ON THE REAL FILE:** editing `config/scenario.toml` to `1.3888888888888888` reds this leg at `rolled_ticks 106 == 79`, and `right_stand_shift_frac = 1.0` reds it at `76 == 79` — arm A follows the toml, arm B holds the driven literal. §7.1 |

`right_assist_nm` arrives at the shipped `2400.0` in both arms **because both arms load the toml**;
that matters because at the struct default of `0.0` the entire self-right block is a no-op and
**both** comfort dials are dark in any fixture. Since the fold there is no hand-copied value left in
either arm to keep honest — but §3.2 still pins `right_assist_nm`, `right_charge_push_s` and
`right_dir_eps`, because those three toml rows are now read identically by **both** arms and so a
walk-back of any of them cannot red *here* (see §7.1).

### 3.4 The subset gate

```
ctest --test-dir build -C Debug -R "sled|rider|tape|selfright|load_scenario" -j4
98% tests passed, 4 tests failed out of 214      (368.04 s)
```

**The four are exactly the four `snow`-lane known sled debts, by name** —
`sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`. The verdict is the
runner's: `python tools/gate/gate_baseline.py check build/subset_v2_final.log`, exit 0, **"No new
reds."** (It also calls the two `ai` baseline reds "now PASSING"; they are not — this is a **subset**
log and `-R` never selected them. The baseline was **NOT** re-recorded.)

### 3.5 ⚠ A REPORT CHAD SHOULD SEE — v2 MOVED TWO OF THE FOUR KNOWN DEBTS

They are still red, still the same four by name, still pre-existing — but two of their numbers moved,
and the movement is attributed by measurement (defaults flipped back one at a time, rebuilt, re-run):

| known-red leg | pre-v2 | SHIPPED v2 | direction | attributed to |
|---|---|---|---|---|
| `sled_slides_before_it_tips_on_flat_snow` (`max_roll < 0.35`) | 0.42257279222223798 | **0.42012672394096495** | **better** (0.0024 rad closer) | `track_lat_slip_shed 1.4` |
| `sled_grip_ceiling_stays_below_the_tip_threshold` (`peak/onset <= 0.90`) | 2.62256439833490473 | **2.79578277788376406** | **worse** (+0.17) | `track_lat_slip_shed 1.4` |
| `sled_assist_reference_plane_is_load_weighted` | 341.37576434061895725 | 341.37576434061895725 | unchanged | — |
| `sled_debug_sink_is_write_only` | `saw_release_under_one == false` | same | unchanged | — |

Attribution run: with **both** `traction_mu` and `track_lat_slip_shed` put back to `0.0` the two legs
report the pre-v2 numbers exactly; with `traction_mu` alone put back they report the v2 numbers. So
`traction_mu 3.0` is inert in both (consistent with its measured ~40 m/s onset) and the fishtail dial
Chad climbed to 1.4 owns the whole movement. **`sled_grip_ceiling_…` getting worse is a debt moving
in the wrong direction under a value he signed** — not a blocker for this commit (the leg was already
red on main, and he drove the value and said "okay good"), but it belongs in front of him before the
full gate and the tag.

---

### 3.6 ★ THE ONE NEW RED, ATTRIBUTED AND RE-BARRED — Chad: *"1 re bar the test"*

The full gate on the landing tip `c02ffe8fc` (§5 below) carried **one** new red:
`snowbank_inner_face_still_launches` (`test/unit/test_sled.cpp:3747`). Put to Chad by name with two
options — **(1)** keep `traction_mu 3.0` and re-bar the leg, **(2)** walk the dial back to 1.0 and
re-drive — his ruling, 2026-09-18, verbatim: **"1 re bar the test"**. `traction_mu` stays at 3.0.

Commit: `3ebf4604d`.

#### 3.6.1 ATTRIBUTION — measured, one dial at a time

A probe ran the leg's own two arms across the v2 dials, each put back to its identity alone
(temporary `ZZPROBE_snowbank_attribution` in `test/unit/test_sled.cpp`, built into
`build/seads_tests.exe`, removed afterwards — `git status --porcelain` shows no `test/` entry).
`before` = cap off (`bank_pack_skin_m = -1.0`), `after` = the shipped cap; `dur` in seconds; `dom` =
`dur_after / dur_before`.

| arm | `traction_mu` | `track_lat_slip_shed` | `rolled_throttle_frac` | `before.rolled` | `dur_before` | `dur_after` | `dom` |
|---|---|---|---|---|---|---|---|
| pre-v2 identity | 0.0 | 0.0 | 0.0 | **1** | 0.358 | 2.633 | 7.35x |
| **v2 shipped** | **3.0** | **1.4** | **0.15** | **0** | 0.371 | 2.608 | 7.03x |
| `traction_mu` back ONLY | **0.0** | 1.4 | 0.15 | **1** | 0.358 | 2.633 | 7.35x |
| `track_lat_slip_shed` back ONLY | 3.0 | **0.0** | 0.15 | 0 | 0.371 | 2.608 | 7.03x |
| `rolled_throttle_frac` back ONLY | 3.0 | 1.4 | **0.0** | 0 | 0.371 | 2.608 | 7.03x |

**`traction_mu` is the SOLE OWNER.** Putting it back alone restores the old bar's fact *and every
number* to the pre-v2 row exactly; putting either of the other two back alone changes **not one
digit**. `track_lat_slip_shed 1.4` and `rolled_throttle_frac 0.15` are **inert in this leg** — note
this is the mirror image of §3.5, where the shed dial owned the whole movement and `traction_mu` was
the inert one.

**The boundary** (`traction_mu` swept alone, other two at identity):

| `traction_mu` | 0.0 | 0.8 | **1.0** | **1.2** | 1.5 | 2.0 | 3.0 |
|---|---|---|---|---|---|---|---|
| `before.rolled` (the OLD bar) | 1 | 1 | **1** | **0** | 0 | 0 | 0 |
| old bar verdict | green | green | **green** | **RED** | red | red | red |
| `dur_before` | 0.358 | 0.212 | 0.388 | 0.379 | 0.371 | 0.371 | 0.371 |
| `dur_after` | 2.633 | 2.567 | 2.583 | 2.592 | 2.600 | 2.604 | 2.608 |
| `dom` | 7.35x | 12.08x | 6.67x | 6.84x | 7.01x | 7.02x | 7.03x |

**Green at `traction_mu <= 1.0`, red from 1.2.** The shipped 3.0 is well past it.

#### 3.6.2 WHY THE OLD BAR WAS STALE, NOT BROKEN

The failing assertion was **only** `REQUIRE(before.rolled)`. The `before` arm takes its dials from
`sim::SledParams()` — the **shipped struct default** — and pinned a pre-W2 incident: the ~1.3 m loose
reported bank pile *buries and trips* the machine on the way up the rise. With v2's contact ceiling
the buried machine **plows through** the pile instead of tripping on it (`rolled` 1 → 0, air
0.358 → 0.371 s). Same class as §2's tape-absent bug and §3.1's five legs, one layer further out: a
leg that pins "the pre-v2 value IS the shipped default" silently re-ran on the v2 kernel.

**The AFTER arm was never unhealthy**: launch 0.946 s, air 2.608 s against the 1.0 s bar, `rolled` 0.
The leg's *name* was true the whole time; only its stale premise reddened.

#### 3.6.3 WHAT IS BARRED NOW

**Every AFTER bar is untouched — nothing was relaxed:**

```cpp
REQUIRE_FALSE(after.rolled);
REQUIRE(after.air_start >= 0);
REQUIRE(after.air_end > after.air_start);
REQUIRE(dur_after > 1.0);          // a real jump, not a stumble
```

`before.rolled` becomes a **printed fact, not an assertion**, and in its place the leg asserts the
positive statement it is named for — the shipped cap's airtime **DOMINATES** the cap-off pile's:

```cpp
REQUIRE(dur_before > 0.0);              // the denominator is a real launch too
REQUIRE(dur_after > 3.0 * dur_before);  // the cap DOMINATES the loose pile
```

```
[GI W2] inner-face launch: FACT (reported, not barred) before.rolled=0 | dominance dur_after/dur_before=7.03x
```

**Why 3x.** Measured 2.608 / 0.371 = **7.03x** at the shipped defaults, and the ratio never falls
below **6.67x** anywhere on the `traction_mu` sweep above (range 6.67x – 12.08x). A 3x bar therefore
keeps better than **2.2x of headroom** while still being far inside the mutant's 1.00x.

The leg's CHOICE-AT-AMBIGUITY comment was rewritten in place: it now states that the pre-W2 baseline
premise **is stale since SLED KERNEL v2's contact ceiling**, quotes Chad's *"1 re bar the test"*, names
`traction_mu` as the sole owner with the 1.0/1.2 boundary, and states the killing mutation.

#### 3.6.4 THE KILLING MUTATION — STATED IN THE LEG AND **RUN**

The sentinel's caveat on a re-bar is that the re-barred leg must still be able to red. It can.

**Mutation:** hand the AFTER arm the cap-off field as well — `f_after.p.bank_pack_skin_m = -1.0`, the
W2.1 one-switch sentinel, i.e. **disable the W2 bank-pack cap for the shipped arm**. Rebuilt
`seads_tests`, run:

```
[GI W2] inner-face launch: BEFORE(cap off) air=[1.483,1.854] dur=0.371 rolled=0
[GI W2] inner-face launch: AFTER(shipped cap) air=[1.483,1.854] dur=0.371 rolled=0
[GI W2] inner-face launch: FACT (reported, not barred) before.rolled=0 | dominance dur_after/dur_before=1.00x

D:/seads_sandboxes/sled-ride-b1/test/unit/test_sled.cpp:3821: FAILED:
  REQUIRE( dur_after > 1.0 )
with expansion:
  0.37083333333333335 > 1.0

test cases: 1 | 1 failed
assertions: 4 | 3 passed | 1 failed
```

`dur_after` collapses **2.608 → 0.371 s** and the dominance **7.03x → 1.00x** — both the airtime bar
and the 3x dominance bar are violated (Catch2 aborts the case at the first `REQUIRE`, so the 1.00x is
shown by the fact line rather than by a second FAILED block).

**Restored** (`bank_pack_skin_m` mutation removed), rebuilt, re-run:

```
[GI W2] inner-face launch: BEFORE(cap off) air=[1.483,1.854] dur=0.371 rolled=0
[GI W2] inner-face launch: AFTER(shipped cap) air=[0.946,3.554] dur=2.608 rolled=0
[GI W2] inner-face launch: FACT (reported, not barred) before.rolled=0 | dominance dur_after/dur_before=7.03x
All tests passed (6 assertions in 1 test case)
```

#### 3.6.5 THE SUBSET

```
ctest --test-dir build -C Debug -R "snowbank|sled_kernel_v2|sled_tail|sled_rolled|selfright" -j4
100% tests passed, 0 tests failed out of 29        (10.59 s)
```

#### 3.6.6 ⚠ STILL OPEN, AND NOT FIXED HERE

1. The AFTER arm also takes `sim::SledParams()`, so what the leg calls "the shipped cap" is now the
   **v2** kernel, not W2's. It passes and its dominance claim is true of the shipped machine — which
   is arguably the more useful reading — but the drift is named, not hidden. Same for the `before`
   arm: it is now "the v2 kernel with the cap off", not "pre-W2".
2. §5's second observation stands: `test/harness/sled_tape.h:399` names a guard leg
   `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` that **does not exist**. The
   tape-absent identity block is still defended by prose alone.
3. **The full gate has not been re-run on this tip.** §5's verdict (7 failed of 2151, one new red) is
   `c02ffe8fc`'s. A fresh detached full ctest on the re-barred tip, with the verdict written by
   `python tools/gate/gate_baseline.py check`, is still owed before `sled-kernel-v2-signed`.

---

## §4 — WHAT IS NOT DONE HERE

This commit is the **defaults builder**. Still owed before anything reaches main, per §7 of the build
handoff: the landing red-team in front of the decision, the **full** ctest from clean and detached on
the landing tip with the verdict by name, the flight-log row, the `LANES.toml` status, the annotated
tag `sled-kernel-v2-signed`, **the one-line announcement to the sentinel BEFORE any push to main**,
and the merge of `origin/main`. Nothing here was pushed to main and nothing was announced.

## §5 — GATE (the owed full ctest, detached, on the landing tip)

*(The gate-runner task called this "§4 GATE"; §4 was already taken by WHAT IS NOT DONE HERE, so it
lands as §5. Nothing else in this file was touched and this append is NOT committed.)*

**VERDICT: NOT CLEAN. One new red. `sled-kernel-v2-signed` must not be cut on this tip.**

### Provenance

| | |
|---|---|
| tip gated | `c02ffe8fc7eda7ac78c0444bab239de1361e958b` (`c02ffe8fc`) |
| tree at gate | clean (`git status --porcelain` printed nothing) |
| build | `cmake --build build` → `[10/10] Linking CXX executable seads.exe`, 0 errors |
| command | `ctest --test-dir build -C Debug -j4 > build/gate_c02ffe8fc.log 2>&1` (detached) |
| ctest exit | `8` (→ `build/gate_c02ffe8fc.status`) |
| commit time | `2026-09-18T22:36:38-07:00` |
| log mtime | `2026-09-18 22:58:16 -0700` — **21 m 38 s AFTER the commit**, so the log is this tip's |

### Result

```
99% tests passed, 7 tests failed out of 2151
```

`python tools/gate/gate_baseline.py check build/gate_c02ffe8fc.log`:

```
gate: 7 failed of 2151
baseline: 6 known reds

*** 1 NEW RED(S) -- these are regressions ***
  [snow] snowbank_inner_face_still_launches
      test/unit/test_sled.cpp
```

The baseline six are present and unchanged, member for member: `probe P-F: the relentless raider
keeps the pump and shoots back` (79), `E12.1: the raider backfill keeps a faction's pump offense
alive` (119), `sled_slides_before_it_tips_on_flat_snow` (1291),
`sled_grip_ceiling_stays_below_the_tip_threshold` (1292),
`sled_assist_reference_plane_is_load_weighted` (1330), `sled_debug_sink_is_write_only` (1333) — the
same six the pre-v2 log `build/gate_eae9829ad.log` carried, where `gate_baseline.py` printed
*"OK -- the red set is EXACTLY the baseline, member for member."*

### THE NEW RED — and it is this landing's own law, missed once

Test #1339, `test/unit/test_sled.cpp:3747`, fails at `:3786`:

```
D:/seads_sandboxes/sled-ride-b1/test/unit/test_sled.cpp:3786: FAILED:
  REQUIRE( before.rolled )
with expansion:
  false

[GI W2] inner-face launch: BEFORE(cap off) air=[1.483,1.854] dur=0.371 rolled=0
```

The leg's `before` arm is the **pre-W2 baseline**, and it builds that baseline from
`sim::SledParams()` — the struct default — at `test/unit/test_sled.cpp:3761`:

```cpp
const BankCrossRun before = run_bank_crossing(sim::SledParams(), f_before, 15.0);
```

Its own comment pins the fact it is asserting: pre-W2, the loose bank pile *"BURIES and TRIPS the
machine on the way up the rise (rolled=true)"*. v2 moved `traction_mu` 0.0 → 3.0 and
`track_lat_slip_shed` 0.0 → 1.4 **in the struct default**, so the machine now grips the rise and no
longer trips — and a leg that was pinning a pre-v2 fact silently re-ran on the v2 kernel.

This is exactly the class the landing law named: *"Legs that pin 'zero is the shipped zero' must now
construct the pre-v2 value EXPLICITLY (identity constant), not rely on the default."*
`sled_rolled_throttle_frac_zero_is_the_shipped_zero` → `..._is_the_identity` was converted. **This
one was not found.** Same bug as the tape-absent bug in §2, one layer out: the harness read was
fixed, the *test corpus* read was not swept.

**PROVEN, not inferred.** Temporarily giving the `before` arm the three identity constants:

```cpp
sim::SledParams PROBE_IDENTITY;
PROBE_IDENTITY.traction_mu = 0.0;
PROBE_IDENTITY.track_lat_slip_shed = 0.0;
PROBE_IDENTITY.comfort.rolled_throttle_frac = 0.0;
const BankCrossRun before = run_bank_crossing(PROBE_IDENTITY, f_before, 15.0);
```

```
[GI W2] inner-face launch: BEFORE(cap off) air=[1.442,1.800] dur=0.358 rolled=1
All tests passed (5 assertions in 1 test case)
```

The probe was reverted (`git checkout -- test/unit/test_sled.cpp`; `git status --porcelain` empty)
and `seads_tests` rebuilt. **The fix is not applied — it is a source change on a landed tip and
belongs to the builder, not the gate runner.**

Note also, unfixed and unmeasured: the leg's `after` arm *also* takes `sim::SledParams()`, so what it
now calls "the shipped cap" is the v2 kernel, not W2's. It passes, but its meaning drifted.

### ctest -N DELTA: 2148 → 2151, +3, every one accounted for

`ctest --test-dir build -C Debug -N` → `Total Tests: 2151`. From `git show c02ffe8fc -- test/unit/`:

**Added (+5)**
- `sled_kernel_v2_defaults_are_the_driven_values` — `test/unit/test_load_scenario.cpp:112`
- `scenario_sled_comfort_rejects_a_v2_dial_out_of_band` — `test/unit/test_load_scenario.cpp:153`
- `sled_kernel_v2_defaults_equal_the_env_arm` — `test/unit/test_sled.cpp:5308`
- `sled_rolled_throttle_frac_zero_is_the_identity` — `test/unit/test_sled.cpp:4522`
- `selfright: above the shipped speed gate it cannot happen` — `test/unit/test_sled_selfright.cpp:468`

**Removed (−2)** — both RENAMES of the two immediately above, not deletions
- `sled_rolled_throttle_frac_zero_is_the_shipped_zero`
- `selfright: above 5 km/h it cannot happen`

Net **+3** genuinely new legs. No test disappeared.

### Two further observations

1. `test/golden/` is untouched by the landing commit: `git diff --stat c02ffe8fc^ c02ffe8fc --
   test/golden/` prints nothing.
2. `test/harness/sled_tape.h:399` names a guard leg
   `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` as *"the leg that reds on the
   delete"*. `grep -rn "sled_tape_absent_dials_replay" test/` returns **only that comment**. The leg
   does not exist. The tape-absent identity block is currently defended by prose alone.
   ★ **CLOSED 2026-09-19 by §7.2** (landing red-team P1-2): the leg is written, and the "prose alone"
   reading was wrong in both directions — two of the five presets were already defended by
   `tape_360_chad_repro` / `tape_chad_flip_fence`, and `track_lat_slip_shed` was defended by nothing
   at all.

---

## §6 — GATE ON THE RE-BARRED TIP (the §4b owed by the re-bar run)

### Provenance

| | |
|---|---|
| tip gated | `59eda224a` (code tip `3ebf4604d` + this doc) |
| tree at gate launch | `git status --porcelain` → empty |
| build | `cmake --build build` from clean, `[11/11] Linking CXX executable seads.exe` |
| command | `ctest --test-dir build -C Debug -j4 > build/gate_59eda224a.log 2>&1` (DETACHED, `.status` on exit, polled every 60 s) |
| ctest exit status | `build/gate_59eda224a.status` → `8` (= the six baseline reds) |
| log mtime | `2026-09-18 23:48:08 -0700` |
| commit time | `2026-09-18T23:29:15-07:00` — log is **19 min AFTER** the commit, so it gated this tip |
| wall time | `Total Test time (real) = 1003.56 sec` |

### Result — **6 failed of 2151, the baseline six BY NAME**

```
$ python tools/gate/gate_baseline.py check build/gate_59eda224a.log
gate: 6 failed of 2151
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

```
99% tests passed, 6 tests failed out of 2151
	  79 - probe P-F: the relentless raider keeps the pump and shoots back (Failed)
	 119 - E12.1: the raider backfill keeps a faction's pump offense alive (Failed)
	1291 - sled_slides_before_it_tips_on_flat_snow (Failed)
	1292 - sled_grip_ceiling_stays_below_the_tip_threshold (Failed)
	1330 - sled_assist_reference_plane_is_load_weighted (Failed)
	1333 - sled_debug_sink_is_write_only (Failed)
```

**No new reds.** The verdict is the runner's, not the author's.

### The one new red of §3.6 is CLOSED

```
30/2151 Test #1339: snowbank_inner_face_still_launches ... Passed    1.04 sec
```

Against the previous tip's log for contrast:

```
$ python tools/gate/gate_baseline.py check build/gate_c02ffe8fc.log
gate: 7 failed of 2151
baseline: 6 known reds
*** 1 NEW RED(S) -- these are regressions ***
  [snow] snowbank_inner_face_still_launches
```

7/2151 → 6/2151. `traction_mu` **stays 3.0** — Chad: *"1 re bar the test"*. Nothing in the kernel
moved to earn this green; only the stale pre-W2 bar did.

### ctest -N

`ctest --test-dir build -C Debug -N` → `Total Tests: 2151`. Delta vs the 2148 named baseline = **+3**,
the same +3 accounted for line-by-line in §5 (five added, two of them renames of removed legs). The
re-bar commit `3ebf4604d` added and removed no test — it changed bars inside one existing leg.

---

## §7 — FOLD LOG: THE LANDING RED-TEAM'S P1s, AND P2-2

*(The fold task called this "§5 FOLD LOG"; §5 and §6 were already taken by the two gate stages, so
it lands as §7 and the landing gate lands as §8. Nothing earlier in this file was renumbered.)*

Red-team: `docs/REDTEAM_20260918_sled_kernel_v2.md`, commit `5516349f8`, **VERDICT LAND-WITH-FIX**,
3 P1 / 2 P2 / 4 P3. Folded here: **all three P1s**, plus **P2-2** by the sentinel's binding
requirement (a loader check that admits the failure its own comment names). **P2-1 and all four P3s
are CARRIED** — listed in §7.6 with what carrying each one costs.

**NO KERNEL VALUE MOVES IN ANY FOLD.** `sim/sled.h`, `sim/sled.cpp`, `world/snowpack.*` and
`config/scenario.toml` are untouched by every commit in this section. Chad's signature stands and the
landing does not need re-driving. The files that move are one loader check, four test files and the
generated graph.

### 7.1 P1-1 — ARM A IS NOW THE REAL MACHINE (the GO blocker)

**THE DEFECT.** `app/main.cpp:2476-2478` builds the sled the game drives in two lines:

```cpp
sim::SledParams sled_params;                 // struct defaults
sled_params.comfort = scen.sled_comfort;     // the WHOLE loaded [sled_comfort] table
```

The (b) proof's arm A reproduced that bridge by hand and reproduced **three** comfort fields. Five
diverge toml-vs-struct. The two it missed — `right_charge_push_s` (the pusher's fatigue time
constant, `sim/sled.cpp` `tau_push`, running 67 % long) and `right_dir_eps` (4.3x the shipped value)
— both live **inside the self-right block the fixture's phase 2 exists to exercise**. The equality
claim was never harmed, because both arms were equally unfaithful; what was harmed was the sentence
the leg is named for, and the "70 rolled ticks / 2645 rpm, the same as his drive tape" the landing
published.

**THE FIX IS NOT THE TWO FIELDS.** A hand-patched list closes today's miss and leaves tomorrow's. The
hand copy is **gone**: `test/unit/test_sled.cpp` now reads the same `config/scenario.toml` the game
reads, through the same loader, following the precedent already standing in the sibling sled suite
(`test/unit/test_sled_selfright.cpp` `shipped()`, whose banner records what the alternative cost:
*"v1's suite was GREEN while the drive failed completely … every outcome leg proved the mechanism at
4000 N m while config/scenario.toml SHIPPED 900. The tests certified a kernel nobody ran."*).

**THE EXACT CONSTRUCTION OF BOTH ARMS, as they now stand.**

```cpp
// file-local, test/unit/test_sled.cpp
const sim::SledComfort& shipped_comfort() {
    static const sim::SledComfort c = [] {
        const sim::AircraftParams ap =
            cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
        return cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", ap).sled_comfort;
    }();
    return c;
}

// ARM A -- two lines for app/main.cpp's two lines.
sim::SledParams shipped;
shipped.comfort = shipped_comfort();      // ALL 35 toml rows, incl.
                                          //   right_assist_max_ms    = 4.0
                                          //   right_stand_shift_frac = 0.5
                                          //   right_assist_nm        = 2400.0
                                          //   right_charge_push_s    = 0.6
                                          //   right_dir_eps          = 0.04
                                          // traction_mu 3.0 / track_lat_slip_shed 1.4 come from
                                          // SledParams{} (no toml bridge); rolled_throttle_frac
                                          // 0.15 SURVIVES the assignment (no toml key), as in game

// ARM B -- the same two lines (he drove the shipped toml too), then each of the
// five put BACK to its pre-v2 identity and armed to its driven value as a LITERAL,
// in app/main.cpp's env-block order.
sim::SledParams env;
env.comfort = shipped_comfort();
env.traction_mu = 0.0;  env.track_lat_slip_shed = 0.0;
env.comfort.rolled_throttle_frac = 0.0;
env.comfort.right_assist_max_ms = 5.0 / 3.6;   env.comfort.right_stand_shift_frac = 1.0;
env.traction_mu = 3.0;                        // SEADS_SLED_TRACTION_MU
env.track_lat_slip_shed = 1.4;                // SEADS_SLED_TAILSHED
env.comfort.rolled_throttle_frac = 0.15;      // SEADS_SLED_ROLLED_THROTTLE
env.comfort.right_assist_max_ms = 4.0;        // SEADS_SLED_RIGHT_MAXSPD
env.comfort.right_stand_shift_frac = 0.5;     // SEADS_SLED_STAND_SHIFT
```

**RE-RUN AGAINST THAT MACHINE, AT THE DRIVEN VALUES, BIT-EXACT:**

```
$ ./build/seads_tests.exe "sled_kernel_v2_defaults_equal_the_env_arm"
[v2 defaults==env] pos=134.71518045385346 68.660950818305892 6371000.8353244923 rolled_ticks=79 rpm_rolled_sum=208955 assist=7.15826
All tests passed (9 assertions in 1 test case)
```

Same roster of 9 assertions as before the fold (equality over the whole `SledState` via
`SLEDTAPE_PIN_D` plus the rolled-window rpm sum, and the four per-dial killing mutations), and the
numbers are the red-team's corrected numbers to the digit. **79 × 2645 = 208955 exactly** — the
rolled-throttle arithmetic the leg is proud of survives; only the tick count moved.

**THE FOLD IS MUTATION-PROVEN, AND IT BOUGHT TEETH THE LEG DID NOT HAVE.** Arm A follows the real
file now, so a walk-back of a **toml** row reds the (b) proof, which it could not do before:

| mutation | where | result |
|---|---|---|
| `right_assist_max_ms 4.0 → 1.3888888888888888` | `config/scenario.toml` | **RED** — `REQUIRE( a.rolled_ticks == 79 )`, `106 == 79` |
| `right_stand_shift_frac 0.5 → 1.0` | `config/scenario.toml` | **RED** — `76 == 79` |
| `right_charge_push_s 0.6 → 1.0` | `config/scenario.toml` | **RED** in `sled_kernel_v2_defaults_are_the_driven_values`: `CHECK( s.sled_comfort.right_charge_push_s == 0.6 )`, `1.0 == 0.59999999999999998` |
| the three struct dials, `+1e-9` each | `test/unit/test_sled.cpp` fixture | **RED**, unchanged from the landing (§3.3) |

⚠ **THE ONE THING THE FOLD TAKES AWAY, STATED.** Both arms now read the *non-driven* toml rows from
the same file, so a walk-back of one of those moves both arms equally and **cannot** red in the (b)
proof. Three of them are load-bearing for this fixture — `right_assist_nm` (at the struct `0.0` the
whole self-right block is a no-op and both v2 comfort dials go dark), `right_charge_push_s` and
`right_dir_eps`. All three are pinned by name in clause (iv) of
`sled_kernel_v2_defaults_are_the_driven_values` (`test/unit/test_load_scenario.cpp`), which is where
the mutation above reds. That is the trade, written down: the (b) proof gained the two driven toml
rows and gave up the three undriven ones, which now have a dedicated pin instead.

### 7.2 P1-2 — the tape-absent guard leg EXISTS now

`test/harness/sled_tape.h:399` had been naming
`sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` as *"the leg that reds on the
delete"* while `grep -rn "sled_tape_absent_dials_replay" test/` returned **only that comment**. A
comment that advertises a guard is worse than no comment: it invites the tidy-up it claims to catch.

**The leg is written** (`test/unit/test_sled_tape.cpp`, 17 assertions). It loads
`tape_360_chad.sledtape`, `REQUIRE`s that all five dials really are in the tape's `dial_gap` (half
one of the non-vacuity: they are genuinely ABSENT, so the presets are what is being read),
`REQUIRE`s that today's struct defaults **differ** for the three that ship from `sim/sled.h` (half
two: "reconstructed at the identity" and "took today's default" are distinguishable outcomes), and
then pins all five reconstructed values at their identities.

**MEASURED KILLING MUTATION** — the shed preset line deleted from `test/harness/sled_tape.h`,
`seads_tests` rebuilt:

```
$ ./build/seads_tests.exe "sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default"
test/unit/test_sled_tape.cpp:711: FAILED:
  REQUIRE( t.params.track_lat_slip_shed == 0.0 )
with expansion:  1.39999999999999991 == 0.0

$ ./build/seads_tests.exe "[tape]"
test cases:  12 |  11 passed | 1 failed
```

**11 of 12 `[tape]` legs still pass with that line gone** — i.e. the new leg is the *only* thing in
the tape suite standing over it, which is exactly the red-team's measurement (`ctest -R
"tape|golden|replay"` → 40/40 passed before the leg existed). The harness comment at `:399` now says
which preset is defended by which leg, measured, instead of promising one leg for all five.

### 7.3 P1-3 — the snowbank leg no longer claims a guard it does not carry, and now carries one

Two halves, as the red-team specified.

**(1) THE CLAIM IS CORRECTED.** `bank_pack_skin_m` is one switch for two mechanisms by design — the
reported-depth cap (`world/snowpack.cpp:358-365`) **and** the W2.2 crest class branch (`:795`) — and
the airtime bar responds to the sentinel's **sign**, not the cap's value. The trailing comment "the
cap DOMINATES the loose pile" named something the bar cannot see. It now reads *"the W2 bank
treatment as a whole"*, and carries the red-team's sweep verbatim in-source (`0.065 → 7.03x`,
`1.30 → 7.38x`, `10 → 7.38x`, `1e9 → 7.38x` **all pass**; only crossing the sign sentinel collapses
it to `1.00x`).

**(2) THE CAP VALUE GETS ITS OWN ARM, on the quantity the cap computes.** A third arm at a large
**finite** skin keeps the sentinel positive — so the W2.2 class branch does not move and this is a
clean one-variable test of the cap alone — and asks the crest how deep the machine sits:

```
[GI W2] inner-face launch: CAP VALUE arm -- crest sink track capped(0.065 m)=0.0313  uncapped(1e9)=0.0805  surf TRAIL MAIN / TRAIL MAIN
All tests passed (9 assertions in 1 test case)
```

`REQUIRE(crest_uncapped.surface == crest_capped.surface)` is the one-variable check; the bars are
`crest_capped.sink_m[kTrack] < 0.05` and `crest_uncapped > 2.0 × crest_capped` (measured 2.58x). The
0.05 is the cap-plus-feather ceiling **for this fixture**, whose 0.30 m ambient the W2 cap never
claimed to touch — `snowbank_crest_sinkage_bounded` runs the thin 0.02 m ambient precisely so the
cap's own number is readable there.

**MEASURED KILLING MUTATION** — `world/snowpack.h:304` `bank_pack_skin_m = 0.065 → 1.0e9`, the exact
walk-up the airtime bar sleeps through:

```
test/unit/test_sled.cpp:3891: FAILED:
  REQUIRE( crest_capped.sink_m[kTrack] < 0.05 )
with expansion:  0.08054823792596977 < 0.05
assertions: 8 | 7 passed | 1 failed
```

Seven of eight assertions pass under that mutation — including every airtime bar, at 7.38x. The new
arm is the one that reds. `world/snowpack.h` was restored and the leg re-run green before anything
was committed.

### 7.4 P2-2 — the loader check no longer admits `0.0`

`config/load_scenario.cpp` shipped `check(right_assist_max_ms >= 0.0, …)` directly under a comment
saying *"A NEGATIVE speed gate would disarm the righting assist everywhere"*. At
`sim/sled.cpp:1847-1852`, `gate_hi = right_assist_max_ms` and `gate_lo = gate_hi * rearm_frac`; armed
goes false once `right_gs_lp > gate_hi` and back true only once `right_gs_lp < gate_lo`. **At `0.0`
both comparisons behave exactly as they do at `-1.0`** — the assist disarms on the first tick with
any speed at all and never re-arms. The check accepted the failure its own comment named.

Now `> 0.0`, and the message says why: `"sled_comfort.right_assist_max_ms must be > 0 (0 disarms the
righting assist everywhere, exactly as a negative does)"`. Mutation-proven on the **real** table by a
new arm in `scenario_sled_comfort_rejects_a_v2_dial_out_of_band` (`right_assist_max_ms      = 4.0` →
`0.0`, `CHECK_THROWS`). Relaxing the loader back:

```
test/unit/test_load_scenario.cpp:262: FAILED:
  CHECK_THROWS( cfg::load_scenario_toml(write_temp(bad, "v2maxspd0"), kAp) )
because no exception was thrown where one was expected
```

The shipped table is `4.0`, so **the loaded game is unchanged** — this closes a hole, it does not
move a value.

### 7.5 What the fold touched

| file | change |
|---|---|
| `config/load_scenario.cpp` | P2-2: `>= 0.0` → `> 0.0` on `right_assist_max_ms`, message rewritten |
| `test/unit/test_sled.cpp` | P1-1: `shipped_comfort()` reads the real toml, both arms rebuilt on it, pins re-cut 79 / 208955. P1-3: comment corrected + the cap-value arm |
| `test/unit/test_load_scenario.cpp` | P1-1: clause (iv) pins `right_charge_push_s` / `right_dir_eps` beside `right_assist_nm`. P2-2: the `0.0` mutation arm |
| `test/unit/test_sled_tape.cpp` | P1-2: the guard leg `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` |
| `test/harness/sled_tape.h` | P1-2: `:399` now states which preset each existing leg defends, measured |
| `generated/graph/*` | `python tools/graph/graphify.py` in the same commit (412 files, 1438 include edges, 1704 symbols) |

**Not touched by any of it:** `sim/`, `world/`, `app/`, `config/scenario.toml`, `test/golden/`.

### 7.6 CARRIED, with the cost of carrying stated

- **P2-1 — `app/main.cpp:2571` still prints `"(the dial is OFF, not zero-by-accident)"`.** True when
  all five defaults were zero; false now. **COST:** on the kill-switch path — the designated way back
  to the pre-v2 machine — a mistyped value gives a v2-armed run and a log line saying the dial is
  off. The printed number and the banner are both right, so a careful reader survives. Carried only
  because the fold scope is P0/P1 + P2-2; it is a one-string change and should ride the next commit
  that touches `app/`.
- **P3-1** `tools/sled_probe.cpp:3874` `g4_traction = -1.0` means "leave `p.traction_mu` alone" —
  which no longer means zero.
- **P3-2** the `[0,1]` justification for `right_stand_shift_frac` overstates the hazard.
- **P3-3** `sled_kernel_v2_defaults_equal_the_env_arm` structurally cannot detect a walk-back of the
  *tape-absent* struct defaults — that is §7.2's leg's job now, which is new since the red-team read
  the tree.
- **P3-4** `scenario_sled_comfort_rejects_a_v2_dial_out_of_band` `REQUIRE`s literal toml text.

### 7.7 Re-run before the landing gate (non-doc files moved, so all of it was owed)

**The nine replays, `build/seads_sled_probe.exe tape <abs path>`** — his six 09-17 tapes opened
**read-only** in `D:/flight_sim2/seads-recon/build-play`; `git status --porcelain` in that tree
printed nothing before and after. Every one reproduces §2.2 **to the tick and the field**:

| tape | replay | vs §2.2 |
|---|---|---|
| `test/golden/sled/tape_360_chad` | 961/961, rolled 7188/7188, dial gap 34 | **matches** |
| `test/golden/sled/tape_360_chad_attempt1` | 901/901, rolled 1368/1368, dial gap 34 | **matches** |
| `test/golden/sled/tape_chad_flip` | 661/661, dial gap 34 | **matches** |
| `sled_tape_86` | 11430/11430, dial gap 2 | **matches** |
| `sled_tape_87` | 14272/14272, dial gap 2 | **matches** |
| `sled_tape_88` | 1837/11896, GROUND KEY MISMATCH 186528, FIRST DIVERGENCE tick 46589 `position.x` | **matches** |
| `sled_tape_89` | 6355/15256, FIRST DIVERGENCE tick 68315 `velocity.x` | **matches** |
| `sled_tape_90` | 12990/12990, dial gap 2 | **matches** |
| `sled_tape_91` | 14833/20734, FIRST DIVERGENCE tick 14832 `velocity.x` | **matches** |

**The subset**, `ctest --test-dir build -C Debug -R "sled|rider|tape|selfright|load_scenario" -j4`
→ `build/subset_p1fold.log`:

```
98% tests passed, 4 tests failed out of 215        (363.78 s)
	1292 - sled_slides_before_it_tips_on_flat_snow
	1293 - sled_grip_ceiling_stays_below_the_tip_threshold
	1331 - sled_assist_reference_plane_is_load_weighted
	1334 - sled_debug_sink_is_write_only
```

The verdict is the runner's: `python tools/gate/gate_baseline.py check build/subset_p1fold.log`,
exit 0, **"No new reds."** (It again calls the two `ai` baseline reds "now PASSING"; they are not —
this is a **subset** log and `-R` never selected them, exactly as in §3.4. The baseline was **NOT**
re-recorded.) 214 → 215 tests: **+1**, the P1-2 guard leg. The four reds are the four `snow`-lane
known sled debts, by name, unchanged.

**THE RUN-7 DRIVE TAPE**, `sled_tape_9.sledtape` (the last tape in this lane; its header carries all
five driven values — `traction_mu 3`, `track_lat_slip_shed 1.3999999999999999`,
`rolled_throttle_frac 0.14999999999999999`, `right_assist_max_ms 4`, `right_stand_shift_frac 0.5` —
so it IS the env-armed run 7 Chad signed "yes very good"):

```
== SLED TAPE REPLAY D:/seads_sandboxes/sled-ride-b1/sled_tape_9.sledtape ==
  dt=0.00833333333 substeps=12  ticks=20890  overrides=2  ground_samples=1501390
  sig: VERIFIED
  replayed 3983/20890 ticks
  GROUND KEY MISMATCH at sample index 338486 -- the replayed kernel asked for different ground than the taped drive (first-divergence attribution).
  FIRST DIVERGENCE tick 3982 field position.x
  events: rolled tape=3210 replay=3210
  dial gap: none -- this tape names every comfort dial this build declares.
  VERDICT: NOT bit-exact (see above).
```

**READ THAT LINE HONESTLY, BOTH HALVES.** `dial gap: none` is the pin that matters here: the tape
names **every** comfort dial this build declares, so the replay ran at the five DRIVEN values out of
the header rather than reconstructing anything — the env-armed drive and the shipped-default build
describe the same dials. `rolled tape=3210 replay=3210` is the same event, same count. The `NOT
bit-exact` is a **GROUND KEY** divergence on a 20890-tick live drive over faceted terrain, the same
class `sled_tape_88/89/91` carry on the lane base and carried before v2 (§2.2) — a streaming-ground
property of long recorded drives, not a kernel disagreement. No fold in this section can move it:
`seads_sled_probe` links `seads_sim` + `seads_render_core` and **does not link `seads_config` or any
test target**, so not one byte of §7 is reachable from that binary.

---

## §8 — THE LANDING GATE (the §4b owed by the fold; `config/load_scenario.cpp` moved, so it was required)

*(The fold task called this "§4b"; §4 and §4b were already spoken for, so the landing gate lands as
§8. It supersedes §6 as the gate of record: §6's verdict is `59eda224a`'s, and that tip does not
contain the loader change.)*

### Provenance

| | |
|---|---|
| tip gated | `3990cde63f81d2da9d255fb46187934b24f1a722` (`3990cde63`) — **the landing tip** |
| tree at gate launch | `git status --porcelain` → empty (and empty again after) |
| build | `cmake --build build` → `[17/17] Linking CXX executable seads.exe`, 0 errors |
| command | `ctest --test-dir build -C Debug -j4 > build/gate_3990cde63.log 2>&1` (**DETACHED**, `.status` written on exit, polled every 60 s; one ctest only, against `build/`) |
| ctest exit status | `build/gate_3990cde63.status` → `8` (= the six baseline reds) |
| commit time | `2026-09-19T01:45:13-07:00` |
| log mtime | `2026-09-19 02:02:42 -0700` — **17 min 29 s AFTER the commit**, so the log is this tip's |
| wall time | `Total Test time (real) = 1040.94 sec` |

### Result — **6 failed of 2152, the baseline six BY NAME**

The verdict is the runner's, not the author's:

```
$ python tools/gate/gate_baseline.py check build/gate_3990cde63.log
gate: 6 failed of 2152
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

```
99% tests passed, 6 tests failed out of 2152
	  79 - probe P-F: the relentless raider keeps the pump and shoots back (Failed)
	 119 - E12.1: the raider backfill keeps a faction's pump offense alive (Failed)
	1292 - sled_slides_before_it_tips_on_flat_snow (Failed)
	1293 - sled_grip_ceiling_stays_below_the_tip_threshold (Failed)
	1331 - sled_assist_reference_plane_is_load_weighted (Failed)
	1334 - sled_debug_sink_is_write_only (Failed)
```

**No new reds.** The `snowbank_inner_face_still_launches` red of §3.6 stays closed, now with the
cap-value arm of §7.3 riding along inside it.

### ctest -N DELTA: 2151 → 2152, +1, accounted for

`ctest --test-dir build -C Debug -N` → `Total Tests: 2152`. The one addition is the P1-2 guard leg:

```
  Test #1277: sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default
```

No test was removed and none renamed. (`#1363 sled_kernel_v2_defaults_equal_the_env_arm` and
`#1567 sled_kernel_v2_defaults_are_the_driven_values` are the same two legs as before the fold —
their bodies changed, their names did not.)

### The play exe on the landing tip

```
cmake -B build-play -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src
cmake --build build-play --target seads          ->  [26/26] Linking CXX executable seads.exe
```

| | |
|---|---|
| exe | `D:/seads_sandboxes/sled-ride-b1/build-play/seads.exe` |
| mtime | `2026-09-19 02:05:34 -0700` (after the landing tip's commit at 01:45:13) |

**Smoke, 5 s, with every `SEADS_SLED_*` explicitly unset:**

```
$ ./build-play/seads.exe --smoke 5
[config] sled first-build: traction_mu 3 rolled_throttle_frac 0.15 right_assist_max_ms 4
  right_stand_shift_frac 0.5 track_lat_slip_shed 1.4 (env: none -- SLED KERNEL v2 shipped defaults)
exit=0
```

All five driven values on the banner, **env: none**. The shipped-default exe is the machine.

### What is still owed before `sled-kernel-v2-signed`

Unchanged from §4 except that the red-team and its fold are now done: the flight-log row, the
`LANES.toml` status, the annotated tag, **the one-line announcement to the sentinel BEFORE any push
to main**, and the merge of `origin/main`. Nothing here was pushed to main and nothing was announced.
`D:/flight_sim2/seads-recon` was opened read-only for the tape replays and never written.
---

## §9 — THE KERNEL PROOF (post-merge firewall), BY COMMAND

Section numbers: the landing plan called this "§6" and the human steps "§7" before the re-bar gate,
the fold and the landing gate each claimed a section of their own. Nothing was renumbered; these are
appended as **§9** and **§10**. `§4` still names what this lane does not do.

`origin/main` was fetched at the top of this stage and is **`533c8640966ef82b311d138959da6b8187d7e4e3`**
— byte-identical to this lane's base `533c86409`, i.e. **main has not moved**, 0 commits behind, so no
merge was made and none was needed.

```
$ git fetch origin main
From https://github.com/cjcgervais/seads_sandbox1
 * branch                main       -> FETCH_HEAD
$ git rev-parse origin/main
533c8640966ef82b311d138959da6b8187d7e4e3
```

### 9.1 Everything this lane touches — `git diff --name-only 533c86409..HEAD`, in full

```
$ git diff --name-only 533c86409..HEAD
LANES.toml
app/main.cpp
config/load_scenario.cpp
config/scenario.toml
docs/DRIVE_WORDS_20260918_sled_firstbuild.md
docs/REDTEAM_20260918_sled_firstbuild_law-feel.md
docs/REDTEAM_20260918_sled_firstbuild_mechanism.md
docs/REDTEAM_20260918_sled_kernel_v2.md
docs/SESSION_HANDOFF_20260918_sled_firstbuild.md
docs/SESSION_HANDOFF_20260918_sled_firstbuild_DRIVEN.md
docs/SLED_KERNEL_V2_LANDING.md
drive_sled_firstbuild_1_baseline.bat
drive_sled_firstbuild_2_B0_traction_mu.bat
drive_sled_firstbuild_3_B1_rolled_throttle.bat
drive_sled_firstbuild_4_B2a_right_maxspd.bat
drive_sled_firstbuild_5_B2b_stand_shift.bat
drive_sled_firstbuild_6_B3_tailshed.bat
drive_sled_firstbuild_7_all_together.bat
generated/graph/digest/app.md
generated/graph/digest/config.md
generated/graph/digest/sim.md
generated/graph/digest/test.md
generated/graph/graph.json
sim/sled.cpp
sim/sled.h
test/harness/sled_tape.h
test/unit/test_load_scenario.cpp
test/unit/test_sled.cpp
test/unit/test_sled_selfright.cpp
test/unit/test_sled_tape.cpp
$ git diff --name-only 533c86409..HEAD | wc -l
30
```

That is the whole blast radius, 30 paths, and it is what `LANES.toml [lanes.sled-ride-firstbuild]
announced` now lists path for path (plus `docs/flight-log.md` and `CLAUDE.md`, which this
landing-docs commit adds).

Read against the landing path promised in the DRIVEN handoff §4:

| path | why it is here |
|---|---|
| `sim/sled.h`, `sim/sled.cpp` | the kernel itself: three shipped values + their identity constants, the throttle split, the hoist, the shed |
| `config/scenario.toml`, `config/load_scenario.cpp` | the `[sled_comfort]` landing path for the two dials the loader owns (`right_assist_max_ms`, `right_stand_shift_frac`), and the P2-2 fix that stops the check admitting `0.0` |
| `app/main.cpp` | the env **overrides** and the one `[config]` banner line |
| `test/harness/sled_tape.h`, `test/unit/test_sled*.cpp`, `test/unit/test_load_scenario.cpp` | the tape roster and the legs: the defaults==env arm, the tape-absent guard, the re-barred snowbank leg, the loader leg |
| `generated/graph/*` | regenerated in the same commit as the structural change, per `CLAUDE.md` |
| `drive_sled_firstbuild_*.bat` (7) | KEPT files, the seven drive launchers (`drive_body_*.bat` precedent) |
| `docs/*`, `LANES.toml` | this record, his words, the two red-teams, the lane section |

### 9.2 THE FORBIDDEN SET IS EMPTY — shown by the command, not asserted

The aeroplane kernel, its table, the ground contact law, the step loop and every golden:

```
$ git diff --name-only 533c86409..HEAD -- control/ config/controller.toml sim/ground.h sim/step.cpp test/golden/
$ git diff --name-only 533c86409..HEAD -- control/ config/controller.toml sim/ground.h sim/step.cpp test/golden/ | wc -l
0
```

**Nothing.** Zero lines of output, zero paths. Kernel v17 and the T2 facet-contact rung are untouched
by this landing; no golden was moved or re-recorded; `sim/ground.h` — the file the terrain-clip lane
owns and the one a merge must never take "ours" on — is not in the diff at all.

---

## §10 — THE HUMAN LANDING STEPS (nothing below this line was done by this lane)

This lane pushed **only** `origin feel/sled-ride-firstbuild`. Main is untouched, no announce was made,
no tag was placed. Each step below is a human's, in this order, and step 2 gates everything after it.

**1 — ANNOUNCE TO THE SENTINEL, BEFORE ANY PUSH TO MAIN.** One line, the announce law
(`announce-every-main-push`); docs included, no exception. It must name: the lane
`feel/sled-ride-firstbuild`; the tip being pushed; the code tip `3990cde63`; the gate verdict by name
(`6 failed of 2152`, the baseline six, `OK -- the red set is EXACTLY the baseline, member for
member`); the shared files from §9.1; and that the forbidden set is empty.

**2 — WAIT FOR GO.** Do not push on silence. If the sentinel asks for a re-gate because `origin/main`
moved between the announce and the GO, re-gate — §9 records main at `533c86409` *as of this commit*,
not as of the push.

**3 — PUSH, FAST-FORWARD ONLY.** From this worktree, with the branch tip SHA spelled out:

```
git rev-parse feel/sled-ride-firstbuild          # the SHA to push; confirm it by eye
git fetch origin main && git merge-base --is-ancestor origin/main feel/sled-ride-firstbuild && echo FF-OK
git push origin <sha>:refs/heads/main            # no --force, no lease, no branch-name shorthand
```

`merge-base --is-ancestor` printing `FF-OK` is the fast-forward proof. If it does not print, **stop**:
main moved, and the merge + re-gate is owed before anything is pushed.

**4 — THE ANNOTATED TAG, with his three words in the body.** At the pushed tip, not at a re-made
commit:

```
git tag -a sled-kernel-v2-signed <sha> -F -   # body: the three words, verbatim, one per line
git push origin sled-kernel-v2-signed
```

The body carries, verbatim and attributed:

- landing — *"yes tyo all 7  and all 3 of these reccomendations I concurr I want this all in a v2"*
- the grip-ceiling debt under `track_lat_slip_shed` 1.4 — *"accept 1.4"*
- the new gate red `snowbank_inner_face_still_launches` — *"1 re bar the test"*

**5 — RESYNC HIS FLY TREE, `D:/flight_sim2/seads-recon`.** That tree is his, and this lane opened it
read-only (raylib source for the build) and never wrote to it. The merge is done **there**, by a human
or the sentinel, after main carries the tag:

```
cd D:/flight_sim2/seads-recon
git status --porcelain      # must be EMPTY before anything else
git fetch origin && git merge --ff-only origin/main
```

**6 — TASKLIST CHECK BEFORE THE REBUILD.** A running `seads.exe` holds the link target and the
rebuild fails, or worse, silently relinks nothing and he flies a stale exe (the locked-exe trap):

```
tasklist | findstr /I seads.exe      # must print NOTHING
```

**7 — REBUILD HIS PLAY EXE, RelWithDebInfo.** `build-play` is RelWithDebInfo always; `build/` (gate)
is Debug. His run-1 word on a Debug play exe was *"took forever to start and ran jittery slow"*.

```
cmake -B build-play -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src
cmake --build build-play --target seads
```

**8 — RECORD THE EXE MTIME AND PROVE IT IS THE NEW ONE.** The mtime must be *after* the merge commit,
and the banner must show all five values with `env: none`:

```
ls -l --time-style=full-iso build-play/seads.exe
./build-play/seads.exe --smoke 5
# expect: [config] sled first-build: traction_mu 3 rolled_throttle_frac 0.15 right_assist_max_ms 4
#         right_stand_shift_frac 0.5 track_lat_slip_shed 1.4 (env: none -- SLED KERNEL v2 shipped defaults)
```

Give him the **absolute** path of that exe (`D:/flight_sim2/seads-recon/build-play/seads.exe`) and its
mtime, per the fly-checklist law. On this lane the same smoke was already green on
`D:/seads_sandboxes/sled-ride-b1/build-play/seads.exe`, mtime `2026-09-19 02:05:34 -0700` (§8).
