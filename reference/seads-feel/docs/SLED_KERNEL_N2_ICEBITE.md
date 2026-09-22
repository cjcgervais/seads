# SLED KERNEL N2 — LAKE-ICE LOW-SPEED SKI BITE (`ice_bite_mu`)

Lane `feel/sled-legwork-n1n2` in `D:\seads_sandboxes\sled-legwork`, off main `4acac47a2` (sled kernel v2),
audit docs merged at `cf35d4019`. Built 2026-09-19. **Not flown.** Build spec: the recon packet in the
launch message (§1 N2); handoff words `docs/SESSION_HANDOFF_20260918_sled_firstbuild_DRIVEN.md` §5 N2.

Chad, drive run 7, 2026-09-18:

> "on lake ice the ski runners are not digging in to the ice and there is no turn authority on it, at
> least at high speed it given limits turning I think it is less grip at lower speeds than I would like."

Read as two sentences: **keep** the high-speed limit (real), **raise** the low-speed ski bite, ice only.

---

## §1 — THE ONE DIAL

`sim::SledComfort::ice_bite_mu` — an **additive lateral mu on the steered patches (the skis) only**,
**LakeIce only** (SK-1a blend-weighted from each patch's own ground sample, so a shoreline is continuous),
full at rest and fading to **exactly `+0.0`** by `kIceBiteVrefMs = 8.0 m/s` (a constant in `sim/sled.cpp`,
not a second dial) on that patch's own tangential speed:

```
mu_lat_eff(ski) = d.mu_lat + ice_bite_mu * w_ice * (1 - u*u*(3 - 2u)),   u = clamp01(v_patch / 8.0)
```

Site: `sim/sled.cpp` lateral bite, directly after `double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;`,
before the B3 shed branch. **A branch on `> 0.0`**: at the identity the term does not exist. Above 8 m/s the
sum is `mu_l + 0.0`, bit-identical — the high-speed limit is kept by arithmetic, not by a clamp.

| | value | where |
|---|---|---|
| identity (struct default, tape-absent reconstruction) | **0.0** | `sim/sled.h` |
| shipped | **0.25** | `config/scenario.toml [sled_comfort] ice_bite_mu` |
| env override / kill | `SEADS_SLED_ICE_BITE` band [0, 0.45] | `app/main.cpp` banner prints `ice_bite_mu` |
| loader | `require` + `check` [0, 1] | `config/load_scenario.cpp` |
| tape header | `X(ice_bite_mu)` appended to `SLEDTAPE_COMFORT_D`; identity preset `0.0` | `test/harness/sled_tape.h` |

The env band top 0.45 keeps `0.22 + 0.45 = 0.67 < 0.70` (TrailMain): a ski on ice never out-bites a ski on a
groomed trail. What the dial deliberately does **not** touch: `SledParams::track_lat_mu` 0.70 is surface-blind
(ice ski:track 0.22:0.70; at 0.25 it becomes 0.47:0.70). A per-surface track mu is its own rung, ruling owed.

The v2 split is kept on purpose: the struct default stays at the identity forever, because it doubles as the
tape-absent reconstruction; the driven/shipped value lives in the toml (v2 landing doc §1.2).

### ⚠ The shipped value was picked by measurement, not by his seat

0.25 is the middle of the ladder 0.15 / 0.25 / 0.35 and the value the recon recommended. The table in §2 is
what it was picked from. His drive is the next thing this dial owes: `drive_sled_n2_icebite.bat`, and
`SEADS_SLED_ICE_BITE=0` is the way back to the 09-18 machine in one line.

---

## §2 — THE MEASUREMENT (`build/seads_sled_probe.exe icebite 0 0.15 0.25 0.35 0.45`)

All-water field (every sample LakeIce, `surface = 4`), full lock `steer = +1`, 8 s at 60 Hz, means over ticks
240–480. Per row the throttle is the one that holds `v0` closest at the identity (searched in 0.01 steps),
then held across the ladder. `v_min` is the lowest ground speed seen in the run (the settle dips it).

```
   v0   thr   dial | yaw[d/s] radius[m]  roll_pk  alat_pk  v_end  v_min rolled
  3.0  0.08  0.000 |   27.158     7.762    0.685   0.1778   3.68   2.74     no
  3.0  0.08  0.150 |   35.285     5.976    0.889   0.2311   3.68   2.74     no  [differs from dial 0 at tick 0, v 2.738]
  3.0  0.08  0.250 |   40.075     5.264    1.009   0.2626   3.68   2.74     no  [differs from dial 0 at tick 0, v 2.738]
  3.0  0.08  0.350 |   44.442     4.748    1.119   0.2913   3.68   2.74     no  [differs from dial 0 at tick 0, v 2.738]
  3.0  0.08  0.450 |   48.456     4.356    1.219   0.3177   3.68   2.74     no  [differs from dial 0 at tick 0, v 2.738]
  6.0  0.13  0.000 |   18.872    18.182    0.841   0.2012   5.99   4.00     no
  6.0  0.13  0.150 |   20.749    16.544    0.922   0.2213   5.99   4.00     no  [differs from dial 0 at tick 0, v 3.996]
  6.0  0.13  0.250 |   21.966    15.632    0.975   0.2344   5.99   4.00     no  [differs from dial 0 at tick 0, v 3.996]
  6.0  0.13  0.350 |   23.154    14.835    1.026   0.2471   6.00   4.00     no  [differs from dial 0 at tick 0, v 3.996]
  6.0  0.13  0.450 |   24.311    14.134    1.075   0.2596   6.00   4.00     no  [differs from dial 0 at tick 0, v 3.996]
 10.0  0.22  0.000 |   11.474    50.661    0.911   0.2099  10.14   7.90     no
 10.0  0.22  0.150 |   11.474    50.661    0.911   0.2099  10.14   7.90     no  [differs from dial 0 at tick 0, v 7.895]
 10.0  0.22  0.250 |   11.474    50.661    0.911   0.2099  10.14   7.90     no  [differs from dial 0 at tick 0, v 7.895]
 10.0  0.22  0.350 |   11.474    50.661    0.911   0.2099  10.14   7.90     no  [differs from dial 0 at tick 0, v 7.895]
 10.0  0.22  0.450 |   11.474    50.661    0.911   0.2099  10.14   7.90     no  [differs from dial 0 at tick 0, v 7.895]
 20.0  0.43  0.000 |    5.883   193.789    0.894   0.2393  19.82  17.40     no
 20.0  0.43  0.150 |    5.883   193.789    0.894   0.2393  19.82  17.40     no  [whole state == dial 0]
 20.0  0.43  0.250 |    5.883   193.789    0.894   0.2393  19.82  17.40     no  [whole state == dial 0]
 20.0  0.43  0.350 |    5.883   193.789    0.894   0.2393  19.82  17.40     no  [whole state == dial 0]
 20.0  0.43  0.450 |    5.883   193.789    0.894   0.2393  19.82  17.40     no  [whole state == dial 0]
```

Read:
- **3 m/s (v ≈ 3.7):** yaw 27.2 → **40.1 deg/s at 0.25** (+48 %), radius 7.76 → 5.26 m. Monotone up the ladder.
- **6 m/s:** 18.9 → 22.0 deg/s (+16 %), radius 18.2 → 15.6 m.
- **10 m/s:** yaw/radius identical to the printed digits; the whole state differs only because the settle dips
  `v_min` to 7.90 m/s (under the 8.0 fade) for a few ticks at the start — the term is live there by arithmetic.
- **20 m/s:** **whole `SledState` bit-identical** (`SLEDTAPE_PIN_D` roster) at every value up to 0.45.
- **Nothing rolled** at any speed at any value. Peak |a_lat| 0.32 g at 0.45 (bar 0.70 g). Peak roll 1.2°.

**Why 0.25:** it is the first value on the ladder that turns "no turn authority" into a number a rider will
feel at walking pace (radius 7.8 → 5.3 m, +48 % yaw) while staying well inside the tip envelope, and it leaves
the 6 m/s row a modest +16 % so the fade toward his real high-speed limit is not a cliff. 0.35 would give
+64 % at 3 m/s; 0.15 only +30 %. He decides from the seat; both neighbours are one env var away.

### Parked creep (v0 0, steer +1, throttle 0, 63.4 s = the shape of his tape-90 parked run)

```
  dial  0.000: CG drift 1.026866 m, peak ground speed 0.077013 m/s
  dial  0.150: CG drift 1.014471 m, peak ground speed 0.080873 m/s
  dial  0.250: CG drift 1.005167 m, peak ground speed 0.082146 m/s
  dial  0.350: CG drift 0.997741 m, peak ground speed 0.082893 m/s
  dial  0.450: CG drift 0.991884 m, peak ground speed 0.083446 m/s
```

The bar was "< 0.01 m rise"; the drift **falls** by 0.02 m at 0.25 (more ski hold = less creep), so no shape
fix (the `v > 0.05` gate the spec held in reserve) is needed. ⚠ **A PRE-EXISTING FACT, not this rung's:** today's
kernel creeps a parked machine **1.03 m in 63 s** on ice with the bars turned and the thumb shut (`slip_ang =
delta` at rest applies `-0.88 · mu · N` sideways per ski). Reported, not fixed here.

### The killing mutation — run, not just named

`g.steered` → `!g.steered` at the term (the bite lands on the TRACK instead of the skis), probe rebuilt:

```
   3.0  0.08  0.000 |   27.158     7.762   ...
   3.0  0.08  0.250 |   26.684     7.899   ...   <- yaw FALLS with the dial
  20.0  0.43  0.250 |    5.883   193.789   ...   [whole state == dial 0]
```

More rear hold against the same front turns the nose **less**; `sled_ice_bite_raises_low_speed_yaw`'s
`y0 < y1` reds on the mutant. (The recon's guess that the 20 m/s row would move under this mutant was wrong —
the fade still reaches zero on the track's own speed — so the ordering leg, not the high-speed leg, is the
mutant's net. Stated so nobody claims the other.)

---

## §3 — THE GOLDEN REPLAY (bytes)

`build/seads_sled_probe.exe tape <abs path>`, his six 09-17 tapes in `D:/flight_sim2/seads-recon/build-play`
opened **read-only** (nothing written to his fly tree), plus the run-7 tape
`D:/seads_sandboxes/sled-ride-b1/sled_tape_9.sledtape`. `replay_pre/` on the lane tip **before** the kernel
edit (probe built from `cf35d4019` sources), `replay_post/` after.

### 3.1 At the identity — the diff prints nothing

Filter (the three lines that change by construction because `SLEDTAPE_COMFORT_D` grew by one):
`grep -v -E "DIAL GAP|dial\(s\)|dial gap|^    [a-z_]"`, then `diff` per tape:

```
tape_86: filtered diff prints nothing     11430/11430, rolled 1464/1464, dial gap 2 -> 3
tape_87: filtered diff prints nothing     14272/14272, rolled 31604/31604, dial gap 2 -> 3
tape_88: filtered diff prints nothing     1837/11896, GROUND KEY MISMATCH 186528, FIRST DIVERGENCE tick 46589 position.x (pre-existing)
tape_89: filtered diff prints nothing     6355/15256, FIRST DIVERGENCE tick 68315 velocity.x (pre-existing)
tape_90: filtered diff prints nothing     12990/12990, rolled 87761/87761, dial gap 2 -> 3
tape_91: filtered diff prints nothing     14833/20734, FIRST DIVERGENCE tick 14832 velocity.x (pre-existing)
tape_9:  filtered diff prints nothing     3983/20890, GROUND KEY MISMATCH 338486, FIRST DIVERGENCE tick 3982 position.x (pre-existing on this lane base; tape_9 was cut on the sled-ride-b1 lane's kernel)
```

The only raw differences are `DIAL GAP: 2 → 3`, the dial list gaining `ice_bite_mu`, and the VERDICT count —
exactly the three lines the spec predicted (§0.6). The three pre-existing divergers diverge at the **same tick
and field** as on v2's landing (`docs/SLED_KERNEL_V2_LANDING.md` §2.2). Exit codes identical.

The hermetic surrogate: `sled_ice_bite_zero_is_the_identity` — a 600-tick full-lock ice drive at walking pace,
final state pinned to 17 digits **recorded on the pre-edit build**, green on the post-edit build.

### 3.2 At the shipped 0.25 — which tapes differ and why (tier-1, `SEADS_SLED_ICE_BITE=0.25`)

| tape | replay at 0.25 | why |
|---|---|---|
| 86 | 13/11430, FIRST DIVERGENCE **tick 12** `position.x` | the drive **starts on the lake** at rest: the term is live from the first tick |
| 87 | 13371/14272, FIRST DIVERGENCE tick 42381 `position.x` | bit-exact for 111 s, then the first slow ice ticks |
| 88 | 13/11896, FIRST DIVERGENCE tick 44765 `position.x` | starts on the lake (its pre-existing 46589 divergence is later) |
| 89 | 6355/15256, tick 68315 `velocity.x` | **unchanged** — pre-existing divergence before any ice |
| 90 | 13/12990, FIRST DIVERGENCE **tick 79287** `position.x` | the parked-on-ice run with the bars turned (12 ticks in) |
| 91 | 14833/20734, tick 14832 `velocity.x` | **unchanged** — pre-existing divergence before any ice |
| tape_9 | 3983/20890, tick 3982 `position.x` | **unchanged** — pre-existing |

This is the honest reason the struct default ships 0.0 forever: at 0.25 those tapes replay a different
machine, and every tape that never named this dial reconstructs at the identity (v2 §1.2).

### 3.3 Tape 90's rollover count at 0.25 — sequential-serve open loop (`seads_sled_probe tape2`)

Tier-1 stops at the first divergence by construction, so it cannot count rollovers over a whole tape at a
driven value. Tier-2 (`replay_open_loop_liveish`, nearest-dir serve) was tried first and **measured not to
reproduce a 100 s tape even at the tape's own params** (tape 88: 10,411 rolled ticks against the tape's 296;
tape 90: 0 against 296) — it is the short-window instrument `sled_tape.h` says it is. So the probe's `tape2`
mode serves the taped ground stream **in tape order with the key check ignored**: honest exactly where the
ground is uniform under a small spatial divergence (a flat lake), stale at the shore. The TAPE arm reproducing
the tape's own count is the self-check.

```
sled_tape_90 (12990 ticks; tape's own record: rolled ticks 296)
  arm TAPE  ice_bite_mu=0   : ice_ticks 10753  rolled_ticks 296  rolled_on_ice 0  first_rolled 87761  max_tilt 157.16  max_v 38.35
  arm CAND  ice_bite_mu=0.25: ice_ticks 10616  rolled_ticks 266  rolled_on_ice 0  first_rolled 87761  max_tilt 157.15  max_v 38.35
```

TAPE arm == the tape (296 rolled, 10,753 ice ticks — the recon's own count). At 0.25 the rollover count
**does not rise** (266 ≤ 296), **zero on ice** in both arms, same first rolled tick 87761 (on Bush, where all
296 were). The 137-tick ice-count change is the shore crossing landing on a slightly different served sample
(the stale zone).

The other three tapes that touch ice, same instrument, same caveat (open loop over sequentially served ground
after a divergence that begins on the first ice tick; `rolled_ticks` counts ticks the latch was UP, i.e. how
long a downed machine lay there, not how many times it went over):

```
sled_tape_86 (11430; tape's own 434)  TAPE: ice 2107 rolled 434 on_ice 107 first 1464  |  0.25: ice 2043 rolled 677 on_ice 82  first 1464
sled_tape_88 (11896; tape's own 550)  TAPE: ice 2587 rolled 550 on_ice 347 first 45908 |  0.25: ice 2583 rolled 423 on_ice 339 first 45908
sled_tape_87 (14272; tape's own 266)  TAPE: ice 3312 rolled 266 on_ice 0   first 31604 |  0.25: ice 3312 rolled 266 on_ice 0   first 31604
```

Read honestly: **on ice** the rolled-tick count falls or holds on every tape (107→82, 347→339, 0→0, 0→0) and
the first rolled tick never moves. Tape 86's **total** rises 434→677, all of it **off ice** after the
trajectory has diverged onto stale served ground (`ice_cg_drift` 495→423 m says the two machines are ~70 m
apart by the shore) — a downed machine lying longer on ground that was served for a different position. That
is the instrument's stated limit, not a measurement of the dial; the synthetic table (no rollover at any speed
at any value to 0.45, peak |a_lat| 0.32 g) and tape 90's own ice segment (0 rolled, both arms) are the
rollover evidence this rung stands on. Tape 87 is bit-identical over its 3,312 ice ticks because all of them
are above 8 m/s.

---

## §4 — THE LEGS (`test/unit/test_sled.cpp`, tag `[icebite]`; loader/tape legs beside the v2 ones)

| leg | claim | killed by |
|---|---|---|
| `sled_ice_bite_zero_is_the_identity` | 600-tick ice drive at dial 0.0 == 17-digit golden from the pre-edit build | the term leaking into the identity |
| `sled_ice_bite_keeps_the_high_speed_limit` | 20 m/s full lock: whole state == between 0.0 / 0.45 and between shipped toml / shipped-with-0.0 | the fade not reaching exactly 0 |
| `sled_ice_bite_is_ice_only` | Bush, TrailMain, Road at 3 m/s: whole state == between 0.45 and 0.0 | the LakeIce gate dropped or read off the track's class |
| `sled_ice_bite_raises_low_speed_yaw` | 3 m/s: yaw(0) < yaw(0.15) < yaw(0.25) < yaw(0.35), none rolled; shipped value is on the ladder | the `!g.steered` mutant (measured above) |
| `sled_kernel_v2_defaults_are_the_driven_values` (+CHECK) | toml 0.25, struct 0.0 | a walk-back of either |
| `scenario_sled_comfort_rejects_a_v2_dial_out_of_band` (+case) | `-0.1` throws | dropping the `>= 0` half of the check |
| `sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` (+2) | absent from the golden tapes; reconstructs at 0.0 | deleting the identity preset line |

`./build/seads_tests.exe "[icebite]"` → **All tests passed (39 assertions in 4 test cases)**.

**Not built, stated:** the spec's `sled_ice_bite_never_reaches_the_track` wanted a per-patch `kRollBite` bucket
from the debug sink; `SledDebugSubstep::roll_tq[kRollBite]` is a per-substep **sum over patches**, so the track
cannot be isolated there without extending the sink. The claim is carried by the mutant run in §2 instead.

---

## §5 — R, THE TRIPWIRE, AND N1

`git diff 4acac47a2 -- app/main.cpp app/player_mode.h | grep -c KEY_R` → `0`. N1 (the leg work) is **not in
this commit**: it is the next rung on this lane, its spec is the recon's §2, and its precondition test
(`legwork_rest_pose_holds_before_any_press`) has not been run.

---

## §6 — GATE

Full gate on the N2 tip `d1c6ace3e`, launched DETACHED (`Start-Process ctest --test-dir build -C Debug -j4
--output-on-failure`, stdout to `build/gate_n2_icebite.log`), lane `build/` = Ninja Debug, WinLibs LLVM.

```
99% tests passed, 6 tests failed out of 2156        Total Test time (real) = 1133.84 sec
    #79   probe P-F: the relentless raider keeps the pump and shoots back
   #119   E12.1: the raider backfill keeps a faction's pump offense alive
  #1292   sled_slides_before_it_tips_on_flat_snow
  #1293   sled_grip_ceiling_stays_below_the_tip_threshold
  #1331   sled_assist_reference_plane_is_load_weighted
  #1334   sled_debug_sink_is_write_only
```

Verdict is the runner's, not hand-written: `python tools/gate/gate_baseline.py check build/gate_n2_icebite.log`
→ `gate: 6 failed of 2156 / baseline: 6 known reds / OK -- the red set is EXACTLY the baseline, member for
member.`, **exit 0**. ctest -N delta: 2152 (v2 landing tip) → 2156, +4 = the four `[icebite]` legs.
