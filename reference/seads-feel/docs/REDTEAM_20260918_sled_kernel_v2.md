# RED-TEAM — SLED KERNEL v2 LANDING (mechanism + regression)

**Target:** `git diff eae9829ad..HEAD`, worktree `D:/seads_sandboxes/sled-ride-b1`, lane
`feel/sled-ride-firstbuild`, tip `11e5c39da` (code tip `3ebf4604d`).
**Decision under review:** Chad, 2026-09-18 — *"yes tyo all 7  and all 3 of these reccomendations I
concurr I want this all in a v2"*, plus *"accept 1.4"* on the grip-ceiling debt and *"1 re bar the
test"* on the snowbank red.
**Ground rule for this document:** every claim below is a command I ran in this worktree and the line
it printed. Nothing is inferred from the landing doc. Main untouched; nothing pushed to main.

## VERDICT: **LAND-WITH-FIX**

The five driven values themselves survive the attack. Each one is individually pinned — I walked
every one of them back and at least one leg reds, measured (table §0.1). The goldens are byte-
unchanged, signature-verified and bit-exact under the v2 kernel. Nothing was relaxed in the loader
suite (`+71/−0`). The documented snowbank killing mutation reproduces verbatim.

What does **not** survive is three of the landing's own *claims about its evidence*. All three fixes
are in `test/` and `docs/` only — **no kernel value moves**, so Chad's signature stands and the
landing does not need re-driving.

---

## §0 — WHAT HELD UP (the attack that failed, stated first)

### 0.1 Every one of the five defaults is pinned — MEASURED, one walk-back at a time

Each row: the default was walked back to its pre-v2 identity in the real source, `seads_tests`
rebuilt, and a fixed 17-leg matrix run. `pass`/`FAIL` is the leg's own verdict.

| mutation | legs that RED |
|---|---|
| `sim/sled.h` `traction_mu 3.0 → 0.0` | `sled_kernel_v2_defaults_are_the_driven_values`, `sled_kernel_v2_defaults_equal_the_env_arm` |
| `sim/sled.h` `track_lat_slip_shed 1.4 → 0.0` | same two |
| `sim/sled.h` `rolled_throttle_frac 0.15 → 0.0` | same two **+** `sled_rolled_throttle_frac_zero_is_the_identity` |
| `config/scenario.toml` `right_assist_max_ms 4.0 → 1.3888888888888888` | `..._are_the_driven_values`, `scenario_sled_comfort_rejects_a_v2_dial_out_of_band`, `selfright: above the shipped speed gate it cannot happen`, `selfright: rocking does not trip its own speed gate` |
| `config/scenario.toml` `right_stand_shift_frac 0.5 → 1.0` | `..._are_the_driven_values`, `..._rejects_a_v2_dial_out_of_band` |
| `sim/sled.h` `right_assist_max_ms 5.0/3.6 → 4.0` ("repair the divergence") | `..._are_the_driven_values` (clause iii) |
| `sim/sled.h` `right_stand_shift_frac 1.0 → 0.5` ("repair the divergence") | `..._are_the_driven_values` (clause iii) |

**A driven value that can be walked back without a red is not a landed value — all seven walk-backs
red.** The two deliberate struct/TOML divergences are likewise ratcheted: "repairing" either one reds.

### 0.2 The goldens — argued, not just shown

```
$ git diff --stat eae9829ad..HEAD -- test/golden/      ->  (prints nothing)
$ grep -c "^# param " test/golden/sled/tape_360_chad.sledtape      -> 68
$ grep -n "traction_mu\|track_lat_slip_shed\|rolled_throttle_frac\|right_assist_max_ms\|right_stand_shift_frac" test/golden/sled/*.sledtape   -> (no hits)
```

All three goldens name 68 params and **none of the five**, so all five reconstruct from the
tape-absent block. `tape_360_chad_repro` verifies the fnv1a signature *before* any physics
(`REQUIRE(t.sig_ok)`) and then pins the replay exactly (`first_div_tick == -1`, 961/961,
`rolled_tick_tape == rolled_tick_replay == 7188`). It passes on this tip. So "the goldens are
untouched" is not only a diffstat: the bytes are unchanged, the signature still verifies, and the
replay is bit-exact **on the v2 kernel**.

### 0.3 The env path — measured on the real play exe

`cmake --build build-play --target seads` → `[11/11] Linking CXX executable seads.exe`, then
`build-play/seads.exe --smoke 2 <png>`:

```
(no env)     [config] sled first-build: traction_mu 3 rolled_throttle_frac 0.15 right_assist_max_ms 4
                      right_stand_shift_frac 0.5 track_lat_slip_shed 1.4 (env: none -- SLED KERNEL v2 shipped defaults)
(kill switch) [config] sled first-build: traction_mu 0 rolled_throttle_frac 0 right_assist_max_ms 1.388888889
                      right_stand_shift_frac 1 track_lat_slip_shed 0 (env: SEADS_SLED_TRACTION_MU=0 …)
```

Parse order is right, and I checked it rather than trusting the comment: the TOML lands at
`app/main.cpp:2478` (`sled_params.comfort = scen.sled_comfort;`) and the env block opens at
`app/main.cpp:2564`, so **env wins over the TOML**, which is what the kill switch requires.
`env_dial` rejects empty, non-numeric, trailing-garbage and non-finite values and keeps the default;
the banner's `armed` list is built from acceptance, so a rejected value never appears as armed
(`SEADS_SLED_STAND_SHIFT=99` was the only entry in the banner of the garbage arm). All confirmed
live, §P2-1 below is the one thing wrong on that path.

### 0.4 Nothing was relaxed in the loader suite

```
$ git diff --numstat eae9829ad..HEAD -- test/unit/test_load_scenario.cpp   ->  71  0
$ git show eae9829ad:test/unit/test_load_scenario.cpp | grep -n "right_assist_max_ms\|right_stand_shift_frac"   ->  (no hits)
```

`+71/−0`: `scenario_sled_comfort_matches_kernel_defaults` **never** covered the two v2 keys, so the
landing's claim that nothing had to be relaxed there is true, not convenient.

### 0.5 The snowbank killing mutation reproduces, verbatim

I re-ran it myself — `f_after.p.bank_pack_skin_m = -1.0` handed to the AFTER arm of
`snowbank_inner_face_still_launches`, rebuilt, run:

```
test/unit/test_sled.cpp:3821: FAILED:
  REQUIRE( dur_after > 1.0 )
with expansion:
  0.37083333333333335 > 1.0
test cases: 1 | 1 failed
assertions: 4 | 3 passed | 1 failed
```

Identical to §3.6.4 of the landing doc, down to the digits. The re-barred leg **can** still red.
(What it reds *on* is P1-3.)

### 0.6 The gate verdict is the runner's, and it re-checks

```
$ python tools/gate/gate_baseline.py check build/gate_59eda224a.log
gate: 6 failed of 2151
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

---

# P1 FINDINGS

## P1-1 — The (b) proof's "ARM A" is **not** the machine the game builds: two comfort dials diverge

**`test/unit/test_sled.cpp:5360-5363`** (`sled_kernel_v2_defaults_equal_the_env_arm`)

```cpp
    // ARM A -- THE MACHINE THE GAME BUILDS. Struct defaults carry the three
    // dials with no TOML key; the two that have one are applied exactly as
    // app/main.cpp applies them (`sled_params.comfort = scen.sled_comfort;`).
    sim::SledParams shipped;
    shipped.comfort.right_assist_nm = 2400.0;
    shipped.comfort.right_assist_max_ms = 4.0;
    shipped.comfort.right_stand_shift_frac = 0.5;
```

`app/main.cpp:2478` assigns the **whole** `scen.sled_comfort` struct, not three fields. I measured
every comfort double the shipped TOML actually carries against `sim::SledComfort{}` (temporary
`ZZPROBE_comfort_toml_vs_struct_divergence`, driven off `SLEDTAPE_COMFORT_D` so no field can be
missed; removed afterwards, `git status --porcelain` empty):

```
[ZZDIV] right_assist_nm              toml=2400  struct=0
[ZZDIV] right_assist_max_ms          toml=4     struct=1.3888888888888888
[ZZDIV] right_charge_push_s          toml=0.59999999999999998  struct=1
[ZZDIV] right_dir_eps                toml=0.040000000000000001 struct=0.1736
[ZZDIV] right_stand_shift_frac       toml=0.5   struct=1
[ZZDIV] total diverging comfort doubles = 5
```

**Five diverge; the leg reproduces three.** The two it misses are not peripheral — both live inside
the self-right block the fixture's phase 2 exists to exercise. `right_charge_push_s` is the pusher's
fatigue time constant, read at `sim/sled.cpp:1866` (`tau_push`), and the fixture runs it **67 %
longer** than the game does. `right_dir_eps` is the righting direction epsilon, 4.3x the shipped
value in the fixture.

**Failure scenario, measured.** Bring both arms to the true shipped table
(`right_charge_push_s = 0.6`, `right_dir_eps = 0.04`) and rebuild — the fixture moves materially:

| | leg as landed | true shipped table |
|---|---|---|
| `rolled_ticks` | 70 | **79** |
| `rpm_rolled_sum` | 185150 | **208955** |
| `assist_nm` (final) | 12.7308 | **7.15826** — a 44 % difference in the very mechanic two of the five dials control |
| final `position` | `131.28276981865076 74.433819291860573 6371000.843508929` | `134.71518045385346 68.660950818305892 6371000.8353244923` |

```
test/unit/test_sled.cpp:5397: FAILED:
  REQUIRE( a.rolled_ticks == 70 )
with expansion:
  79 == 70
```

So the leg's published non-vacuity numbers — the 70 rolled ticks and the "2645 rpm, the same number
his drive tape showed" — describe a machine that is not the shipped one. The **equality** claim
(A == B) is unharmed, because both arms are equally unfaithful; what is harmed is the sentence the
leg is named for and the one the landing leans on: *"the SHIPPED-DEFAULT exe is the SAME MACHINE as
the env-armed exe Chad actually drove"*.

**FIX (verified green here, 6 lines).** Add to **both** arms, `test/unit/test_sled.cpp:5363` and
`:5371`:

```cpp
    …comfort.right_charge_push_s = 0.6;   // config/scenario.toml [sled_comfort]
    …comfort.right_dir_eps = 0.04;        // config/scenario.toml [sled_comfort]
```

and re-pin `:5393` `rolled_ticks == 79` and `:5394` `rpm_rolled_sum == 208955.0`. I ran exactly this:

```
[v2 defaults==env] pos=134.71518045385346 68.660950818305892 6371000.8353244923 rolled_ticks=79
                   rpm_rolled_sum=208955 assist=7.15826
All tests passed (9 assertions in 1 test case)
```

All four per-dial killing mutations still red under the corrected fixture (9 assertions = the same
roster). Note **79 × 2645 = 208955 exactly** — the rolled-throttle arithmetic the leg is proud of
survives the correction intact; only the tick count moves.

⚠ Add the missing structural defence too, or this recurs on the next TOML key that diverges: clause
(iv) of `sled_kernel_v2_defaults_are_the_driven_values` pins `right_assist_nm == 2400.0` precisely
because the kernel suite cannot read config. It should pin `right_charge_push_s == 0.6` and
`right_dir_eps == 0.04` on the same line and for the same reason.

---

## P1-2 — The tape-absent identity line for `track_lat_slip_shed` is **undefended**, and the source names a guard leg that does not exist

**`test/harness/sled_tape.h:399` and `:440`**

The harness comment says, of the preset block:

> `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` **is the leg that reds on the
> delete.**

```
$ grep -rn "sled_tape_absent_dials_replay" test/
test/harness/sled_tape.h:399:    // derived under the same wrong assumption. `sled_tape_absent_dials_replay_
```

The leg does not exist. The landing doc flags this (§3.6.6 item 2) but calls the whole block
"defended by prose alone", which is too pessimistic in one direction and not pessimistic enough in
another. **I measured it, line by line** — each preset deleted alone, `seads_tests` rebuilt, the
tape legs run:

| deleted preset line | result |
|---|---|
| `t.params.traction_mu = 0.0;` (`:401`) | **`tape_360_chad_repro` RED, `tape_chad_flip_fence` RED** — defended |
| `t.params.comfort.rolled_throttle_frac = 0.0;` (`:441`) | **`tape_360_chad_repro` RED** — defended |
| `t.params.track_lat_slip_shed = 0.0;` (`:440`) | **NOTHING REDS** |

For the shed line I widened the net to every tape/golden/replay leg in the suite, not just my matrix:

```
$ ctest --test-dir build -C Debug -R "tape|golden|replay" -j4
100% tests passed, 0 tests failed out of 40
Total Test time (real) = 308.14 sec
```

**Failure scenario.** `track_lat_slip_shed` is dark in the three goldens, so deleting its identity
line changes nothing *today* — which is precisely the hazard the block's own paragraph names: *"they
would still report bit-exact, because the pins were re-derived under the same wrong assumption."* A
future agent tidying the block, or re-cutting a golden on a corridor drive where the dial is live,
gets a green gate and a corpus replaying a 1.4 tail-shed kernel that never drove it. The in-source
comment actively invites the deletion by promising a leg that would catch it.

**FIX.** Either (a) write `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` — load a
golden, assert `t.params.track_lat_slip_shed == 0.0`, `t.params.traction_mu == 0.0`,
`t.params.comfort.rolled_throttle_frac == 0.0`, `t.params.comfort.right_assist_max_ms == 5.0/3.6`,
`t.params.comfort.right_stand_shift_frac == 1.0` after `load()`, with a non-vacuity `REQUIRE` that
the shipped defaults differ; that is five lines and reds on every delete including the two that are
inert today. Or (b) at minimum correct `:399` to name which lines are covered by which existing leg.
(a) is strictly better and costs nothing.

---

## P1-3 — The re-barred snowbank leg detects the W2 **switch**, not the W2 **cap** it names

**`test/unit/test_sled.cpp:3821`**

```cpp
    REQUIRE(dur_after > 3.0 * dur_before);  // the cap DOMINATES the loose pile
```

and the comment above it (`:3806-3810`) calls the killing mutation *"disable the W2 bank-pack cap for
the shipped arm"*.

`bank_pack_skin_m` is **one switch for two mechanisms** — by design, stated at
`world/snowpack.cpp:358-365` (the reported-depth cap) and `world/snowpack.cpp:795` (the W2.2 class
branch: the bank crest classifies `TrailMain`, mu_lat 0.70, instead of `Bush`). `-1.0` disables
**both**. I swept the AFTER arm's cap value with a temporary probe (`ZZPROBE_snowbank_dominance_
sweep`, removed afterwards; `dur_before` = 0.3708 s throughout):

```
[ZZ] skin= 0.065 dur_after=2.6083 dom=  7.03x bar_air=pass bar_3x=pass   <- SHIPPED
[ZZ] skin= 1.300 dur_after=2.7375 dom=  7.38x bar_air=pass bar_3x=pass
[ZZ] skin= 10.000 dur_after=2.7375 dom=  7.38x bar_air=pass bar_3x=pass
[ZZ] skin= 1000000000.000 dur_after=2.7375 dom=  7.38x bar_air=pass bar_3x=pass
[ZZ] skin= 0.000 dur_after=2.5500 dom=  6.88x bar_air=pass bar_3x=pass
[ZZ] skin=-0.000 dur_after=0.3708 dom=  1.00x bar_air=FAIL bar_3x=FAIL
[ZZ] skin=-1.000 dur_after=0.3708 dom=  1.00x bar_air=FAIL bar_3x=FAIL
```

**Failure scenario.** Walk `world/snowpack.h:304` `bank_pack_skin_m` from `0.065` to `1e9` — the depth
cap is then completely inert, every bank reports its full ~1.3 m pile as sinkage, the W2.1 mechanic
is gone in everything but name — and the leg reports **7.38x and passes**. The bar moves only when
the value crosses zero, i.e. it is a bar on the sign sentinel and therefore on the W2.2 *class*
branch, not on the cap. `dur_after` varies by 6 % across seven orders of magnitude of the quantity
the comment says it dominates.

**Mitigation that already exists (so this is a claim defect, not a coverage hole):** the cap's
numeric behaviour *is* pinned, at `test/unit/test_snowpack.cpp:442-443`, which computes
`min(bank_full, fc.p.bank_pack_skin_m)` against the live value. The gap is that
`snowbank_inner_face_still_launches` — the leg Chad's *"1 re bar the test"* produced, and the one the
landing doc presents as re-barred — says it gates something it does not.

**FIX.** Two lines of honesty, one of teeth:
1. Rewrite `:3821`'s trailing comment and `:3806-3810` to say what is measured: *"the W2 bank
   treatment as a whole (the `bank_pack_skin_m >= 0` sentinel gates both the reported-depth cap and
   the W2.2 crest class) dominates the pre-W2 loose pile. MEASURED: the bar responds to the
   sentinel's SIGN, not to the cap value — at `skin = 1e9` the dominance is still 7.38x."*
2. If a bar on the cap itself is wanted, add a third arm at a large finite `skin` and assert the
   crest sinkage, not the airtime — airtime is provably blind to it.

---

# P2 FINDINGS

## P2-1 — `env_dial`'s rejection message is now false, on the kill-switch path

**`app/main.cpp:2568-2572`** — `"KEEPING the default %.10g (the dial is OFF, not zero-by-accident)"`.
That parenthesis was true when every one of these five defaults was zero. Measured on the play exe
built from this tip:

```
$ SEADS_SLED_TRACTION_MU=off SEADS_SLED_TAILSHED= SEADS_SLED_ROLLED_THROTTLE=nan … build-play/seads.exe --smoke 2
[config] SEADS_SLED_TRACTION_MU='off' is not a number -- KEEPING the default 3 (the dial is OFF, not zero-by-accident)
[config] SEADS_SLED_ROLLED_THROTTLE='nan' is not finite -- KEEPING the default 0.15 (the dial is OFF, not zero-by-accident)
[config] SEADS_SLED_TAILSHED='' is empty -- KEEPING the default 1.4 (the dial is OFF, not zero-by-accident)
```

**Failure scenario.** This is the exact path the landing designates as the way back to the pre-v2
machine (`app/main.cpp:2510-2515`). An operator who mistypes one of the five kill-switch values gets
a v2-armed run and a log line telling him the dial is OFF. The printed number is right and the banner
is right, so a careful reader survives; a skimming one does not, and this line exists to be skimmed.

**FIX.** `app/main.cpp:2571` → `"(the dial keeps its SHIPPED value, not zero-by-accident)"`.

## P2-2 — The new loader check `right_assist_max_ms >= 0` admits the exact failure its comment names

**`config/load_scenario.cpp:587-590`**

```cpp
    // A NEGATIVE speed gate would disarm the righting assist everywhere; …
    check(s.sled_comfort.right_assist_max_ms >= 0.0,
          "sled_comfort.right_assist_max_ms must be >= 0");
```

At `sim/sled.cpp:1847-1852` the gate is `gate_hi = right_assist_max_ms`, `gate_lo = gate_hi *
rearm_frac`; armed goes false when `right_gs_lp > gate_hi` and back true only when
`right_gs_lp < gate_lo`. At `0.0` both comparisons behave exactly as they do at `-1.0`: the assist
disarms on the first tick with any speed at all and never re-arms. **`0.0` disarms the righting
assist everywhere, and the check accepts it.**

**FIX.** `> 0.0`, and say so in the message. (`right_stand_shift_frac ∈ [0,1]` is fine as written.)

---

# P3 / OBSERVATIONS

- **P3-1 — `tools/sled_probe.cpp:3874` `g4_traction = -1.0`** means "leave `p.traction_mu` at the
  struct default", applied at `:3899` (`carve_params()`) and `:4154` (the roll-term probe). That
  default is now `3.0`, so every probe sub-command run without the explicit argument silently
  measures the v2 kernel. Not gated, not shipped — but any pre-v2 probe number quoted in the docs is
  no longer reproducible from the same command line.
- **P3-2 — the `[0,1]` justification overstates the hazard.** `config/load_scenario.cpp:588` calls an
  out-of-band shift fraction *"a rider reaching past his own board"*. `sim/sled.cpp:430-433` clamps
  `lat_target_sr` to `±p.lean_lat_stand_m`, so the reach is unreachable; measured live,
  `SEADS_SLED_STAND_SHIFT=99` is applied by the env path and is harmless. The check is fine; the
  reason given for it is not the true one, and the env path contradicts it without consequence.
- **P3-3 — `sled_kernel_v2_defaults_equal_the_env_arm` structurally cannot detect a walk-back of the
  two TOML dials**: both arms assign the same literal `4.0` / `0.5`, so mutating either the TOML or
  the struct leaves the leg green (measured: MUT-C1, MUT-C2, MUT-R1, MUT-R2 all left it `pass`). This
  is deliberate and documented, and `sled_kernel_v2_defaults_are_the_driven_values` does cover it —
  recorded so nobody later reads that leg as a five-dial ratchet. It is a three-dial ratchet.
- **P3-4 — `scenario_sled_comfort_rejects_a_v2_dial_out_of_band`** `REQUIRE`s the literal string
  `"right_assist_max_ms      = 4.0"` including its column alignment. A whitespace normalisation of
  `config/scenario.toml` reds it with a message about out-of-band rejection — a true red for a false
  reason. House style (the sibling `…rejects_an_inverted_release_band` does the same), so no change
  asked; noted because two legs now depend on the TOML's exact spacing.

---

# WHAT I DID NOT FIND

Attacked and clean:

- **An old-header replay that reads a new default.** All five dials are in `SLEDTAPE_PARAMS_D` /
  `SLEDTAPE_COMFORT_D` (`test/harness/sled_tape.h:67,73,81,83,89`), so a new tape names them and an
  old one does not; the presets sit *before* the parse loop, so a naming tape overwrites them and an
  absent dial keeps the identity. I could not construct a tape that gets the v2 value by accident.
- **A consumer of the five fields outside the sled step where the old zero was load-bearing.** The
  only readers are `sim/sled.cpp`, `app/main.cpp` (env + banner), `config/load_scenario.cpp` and
  `tools/sled_probe.cpp`. `grep -rn "thrust_n\|track_slip" audio/ render/` returns nothing;
  `rider_lat_m` reaches the render pose only, and its change is the driven feel Chad approved.
- **A relaxation hidden in the rewritten legs.** `sled_tail_shed_hoist_is_a_pure_refactor` keeps its
  17-digit absolute-trajectory golden unchanged and merely names the baseline it used to inherit —
  the only correct form. `git diff` removals across `test/unit/` are 17 + 3 lines, all accounted for
  by the two renames and the one re-bar.
- **Anything that makes the gate's 6/2151 wrong.** `gate_baseline.py check` re-run by me on the
  committed log gives the same verdict, member for member.

---

# EVIDENCE INDEX

Build used throughout: `cmake --build build --target seads_tests` (~30 s incremental; full command
per the lane contract). Every mutation was applied to the real source, built, run, and reverted with
`git checkout --`; `git status --porcelain` is **empty** before and after this document. Three
temporary probes (`ZZPROBE_snowbank_dominance_sweep`, `ZZPROBE_comfort_toml_vs_struct_divergence`,
the `ZZKILL` snowbank arm) were removed. **No full ctest was re-run and none is owed** — this
document adds no code, and the tip's gate (`build/gate_59eda224a.log`, 6/2151) still stands.

The 17-leg matrix run against every mutation:
`sled_kernel_v2_defaults_are_the_driven_values`, `sled_kernel_v2_defaults_equal_the_env_arm`,
`scenario_sled_comfort_rejects_a_v2_dial_out_of_band`, `scenario_sled_comfort_matches_kernel_defaults`,
`sled_rolled_throttle_frac_zero_is_the_identity`, `sled_tail_shed_hoist_is_a_pure_refactor`,
`sled_tail_shed_is_track_only`, `sled_tail_shed_keeps_the_slide_before_the_tip`,
`sled_tail_shed_is_dark_where_there_is_no_roost`, `snowbank_inner_face_still_launches`,
`tape_360_chad_repro`, `tape_360_chad_provocation`, `tape_360_chad_attempt1_provocation`,
`tape_chad_flip_fence`, `selfright: above the shipped speed gate it cannot happen`,
`selfright: rocking does not trip its own speed gate`, `sled_rolled_throttle_is_dark_on_flat_ground`.
Baseline: all 17 `pass`.
