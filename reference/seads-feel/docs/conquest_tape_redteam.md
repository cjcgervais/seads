# CONQUEST TAPE — VERIFIER VERDICT (Opus, fresh context, 2026-08-06)

Scope: `docs/conquest_tape_spec.md` vs the working diff (`app/instructor_tick.h`,
`app/main.cpp`, `CMakeLists.txt`) plus the untracked `app/conquest_tape.h`,
`test/unit/test_conquest_tape.cpp`, `tools/ai_tape.py`.
No implementation file was edited. Everything below was produced from reading,
from two scratch programs under `tmp_verify/`, and from an independently
hand-built tape.

## VERDICT: **SOUND-WITH-FIXES** — but do NOT run an AI-behaviour investigation
## on this tape until P0-1 and P0-2 are fixed.

The firewall is clean, the file format is honest, the signature chain works, and
every analyzer number I could hand-compute came back exact. Two of the four
findings the recorder exists to adjudicate are instrumented **wrongly**:
F1's shot counter silently drops up to 100 % of real shots, and F4's crash
detector fires on a completely different event than the one it names.

---

# P0 — the tape lies / an event is lost

## P0-1. `ef` (enemy round fired) drops up to 100 % of real shots — MEASURED

**Where:** `app/conquest_tape.h:263-271` (the `enemy_pool` active rising-edge
scan) against `combat/kill.h:434-565` (`enemy_fire_tick`).

**Mechanism.** `enemy_fire_tick` is ADVANCE-then-SPAWN *within one call*:

* step 1 (`kill.h:443-447`) advances every active round and sets
  `p.active = false` on retire;
* step 2 (`kill.h:556-563`) spawns each new round into the **first inactive
  slot** — including a slot that step 1 just freed **this same tick**.

The recorder only samples `active` at the END of the tick (`update_snapshot`,
`conquest_tape.h:443-445`). A slot that was `active` last tick, retired in step
1, and refilled in step 2 reads `true -> true`: **no rising edge, no `ef`
line, the shot is gone.** The detector can never over-report — only
under-report — which makes this the exact failure mode that would
*falsely confirm* the "never shot at" hypothesis in
`docs/ai_primary_data_handoff.md §4`.

**Measurement** (`tmp_verify/ef_alias.cpp`, links the REAL `combat::enemy_fire_tick`;
ground-truth oracle = `p.active && p.age == 0.0`, which only a round spawned this
tick can satisfy since `weapon::advance` ages every active round):

| scenario (120 Hz, 100 s) | true spawns | `ef` recorded | missed |
|---|---|---|---|
| 4 bandits, difficulty 4, continuous fire | 3200 | 320 | **2880 (90.0 %)** |
| 3 bandits, difficulty 3, continuous fire | 1800 | 180 | **1620 (90.0 %)** |
| 1 bandit, difficulty 5, continuous fire | 1100 | 110 | **990 (90.0 %)** |
| 10 bandits, difficulty 5, continuous fire | 11000 | 1100 | **9900 (90.0 %)** |
| 4 bandits, 0.25 s bursts every 1.0 s | 800 | 80 | **720 (90.0 %)** |
| 4 bandits, 1 s burst every 3 s | 1088 | 1088 | 0 (0 %) |
| 4 bandits, 0.5 s burst every 5.5 s | 304 | 304 | 0 (0 %) |

The recorded count in every lossy row equals the pool size exactly: **once the
pool reaches its steady-state size, every subsequent spawn is invisible.** Loss
is 0 % only when the inter-burst gap exceeds the round flight time
(`kMaxFlightTime = render::kBallisticTMax = 10 s`, `weapon/ballistics.h:30`),
i.e. only when nobody is really shooting. Loss is worst exactly during sustained
fire at the player.

**Fix (concrete, cheap, exact).** Stop edge-detecting the slot; read the round's
own spawn contract that the spec itself cites (§3.3 "on the spawn tick
`prev_pos == pos`"):

```cpp
// per slot, every tick — no snapshot needed, no false positives:
if (p.active && p.age == 0.0 && p.prev_pos == p.pos) emit_ef(...);
```

`weapon::spawn` returns `age == 0.0, prev_pos == pos`; `weapon::advance` breaks
both on the very next tick. I verified this oracle recovers 100 % of spawns in
all seven rows above. Keep the active rising edge only as a redundant secondary
if you want belt-and-braces.

**Secondary (smaller) `ef` loss on the same line:** a round retired by
`combat_player_tick` on its own spawn tick (point-blank muzzle inside the
player's 9 m sphere, `kill.h:595-620`, which runs AFTER `enemy_fire_tick` in
`app/instructor_tick.h:1279`) ends the tick `active == false` and is likewise
never seen. The `age == 0.0` fix does not cure this one (the slot is inactive by
hook time); accept it, or have the tape read `cw.player_hits`/`hits` deltas as a
cross-check. Frequency: negligible compared to P0-1.

## P0-2. `dc` (the F4 crash instrument) records the WRONG EVENT, and real crashes are invisible

**Where:** `app/conquest_tape.h:319-334`; spec §3.3 ("In conquest the
inert-rise-without-kill IS the terrain/tunnel crash" — **this spec sentence is
factually wrong**, and the implementation faithfully implements it).

There are exactly **two** sites in the whole tree that set `inert = true`
(verified by exhaustive grep over all non-test `.h`/`.cpp`):

1. `combat/kill.h:415` — `combat_tick` pass 2, the player's guns. This site
   ALSO pushes to `killed_spawn_indices` (`kill.h:417`) → the recorder emits
   `dk`. Correct.
2. `app/instructor_tick.h:1311` — the **AI-vs-AI gunnery block (6c)**. A drone
   shot down by another drone goes inert and is booked through
   `combat::on_ai_kill`, **never** through `killed_spawn_indices`.

So **every `dc` line on a conquest tape means "shot down by another drone",
reported as a crash.** There is no third inert path.

Meanwhile the real terrain/tunnel crash does not go inert at all. `drone::tick`
(`drone/drone.h:1481-1487`) ends with:

```cpp
if (ground_live ? d.curr.crashed : sim::altitude(...) <= 0.0) {
    respawn_in_place(d, ap, dp);   // teleport home, hp = dp.hp, age_ticks = 0
}
```

This branch is **unconditional** — `cw.respawn_drones = false` (set in
`app/main.cpp:1435`) only governs `combat_tick`'s kill path, not this one. (The
comment at `app/main.cpp:1388-1389`, "Respawn can't undo this… respawn_in_place
never runs", is wrong for the same reason.)

The recorder's secondary arm for that case is
`jumped(>2 km) && hp_reset(d.hp > prev.hp + 1e-6)` (`conquest_tape.h:321-324`).
`respawn_in_place` sets `d.hp = dp.hp` — so **`hp_reset` is FALSE for any drone
that was at full health when it flew into the hill**, which is the overwhelmingly
common case. A healthy drone crashing into terrain therefore produces:
* no `dk`, no `dc`;
* a silent position teleport (only if a sample tick straddles it);
* possibly a spurious `tn ... "in":0` if it teleported out of the tunnel net,
  which the analyzer counts as a tunnel **exit** (§6 "Transitions: N exits").

**Net effect on the F4 finding ("a tunnel crash"):** the one death class the
event was built to catch is unrecorded, and the events that ARE printed under
`In-net deaths` with `dc` are drone-vs-drone shootdowns. Section 6 of the report
will therefore tell the investigator the opposite of the truth.

**Fix.** Three separate signals, all already available and all cheap:

1. `dk` — unchanged (`killed_spawn_indices`), = killed by the PLAYER.
2. New tag (or a `by` field on `dc`) for inert-rise-without-kill = **killed by
   another drone** (block 6c). Add `foe` of the killer if you want it; at
   minimum stop calling it a crash.
3. **True crash** = `respawn_in_place` fired. The unambiguous marker is
   `d.age_ticks` falling to 0 (`drone/drone.h:1030`) — it is monotone-increasing
   otherwise (`drone.h:~1470 ++d.age_ticks`). Snapshot `age_ticks` in `DroneSnap`
   and emit on `d.age_ticks < prev.age_ticks`. This does not depend on hp, on the
   jump distance, or on the drone being damaged. (`d.grounded` rising is an
   equivalent second marker.)

While you are there: `respawn_in_place` uses `spawn_state(ap, dp, spawn_index,
fleet_count)` — the STOCK scatter, **not** the conquest per-faction re-placement
`app/main.cpp:1390-1400` does at startup. A crashed conquest drone is therefore
reborn in the vacuum gap near the player spawn with `mav`, `raid`, `strike`,
`foe`, `leash_engaged` all reset. That is a live game-behaviour finding the
tape currently hides completely — worth its own line in the AI handoff.

---

# P1 — misleading to the attribution step / crash-safety

## P1-1. `raid` / `strike` / `def` are STANDING ORDERS, and the report calls them DUTY

**Where:** flags set at `app/instructor_tick.h:1177-1197`; reported by
`tools/ai_tape.py:462` ("3. DUTY TIME-IN-STATE … flags: raid=%, strike=%") and
`ai_tape.py:610` ("5. RAID/STRIKE TIMELINE … episodes").

`d.strike.active` is armed for **every pilot on every tick** while the enemy deep
pump lives (`instructor_tick.h:1190-1197`) — the code comment says so verbatim:
*"this is a standing order, not a beeline"*. `d.raid.active` is armed for every
designated raider whenever the target pump lives and the player-range gate
allows. The actual DUTY (the divert) happens inside `drone::tick`'s strike block
and is only visible via `mav.mode` (`DIVE_IN`/`RUN`/`CLIMB_OUT`) and — for the
DPS credit — via `combat::raider_on_station` (`instructor_tick.h:1463`), which
the tape does not record.

Confirmed on the REAL smoke tape `build/conquest_tape_1.jsonl`: at t=0.2 s
**all ten drones report `strike=100 %`** while every one of them is
`mav=PATROL, eng=0` and nobody is anywhere near the stope.

**Consequence for attribution.** Section 3 reads as "this drone spent 100 % of
its life striking"; section 5 emits one giant "strike episode" spanning the whole
tape per drone. Any conclusion drawn from those two sections about *what the AI
was doing* is unsound.

**Fix (report-side, no tape change needed for the wording):**
rename section 3 to `ORDERS STANDING (authorisation, not duty)` and section 5 to
`RAID/STRIKE ORDER WINDOWS`; print DUTY separately, derived from `mav` mode
(`DIVE_IN`/`RUN`/`CLIMB_OUT`) and — once P1-2's fields exist — from range to the
ordered pump. Say explicitly in the header of both sections: *"raid/strike/def
are standing orders sampled at read time; they do not mean the drone was
executing."*

## P1-2. Raid/strike pump attribution is derived from the wrong field; F3 is not adjudicable from this tape

**Where:** `tools/ai_tape.py:621` — `pump_idx = cq_start.get("rp")`.

`rp` is `ConquestWorld::raided_pump`, which is (a) **reset to -1 every tick**
(`app/instructor_tick.h:1421`) and (b) set **only when the pump being damaged
belongs to the PLAYER's faction** (`instructor_tick.h:1464-1469` —
`if (tgt.faction == pf)`), because it drives a HUD blink. It is *not* the
raiding drone's target.

The drone's actual ordered pump — `d.raid.pump_idx` / `d.strike.pump_idx`,
deterministic and per-drone — **is not on the tape at all** (spec §3.1 omits it),
and neither are the pump POSITIONS. So:

* the analyzer's `pump=` column names a pump the drone was probably not
  attacking (or `-1`, in which case `hp_delta` degrades to `?`);
* "nearest pump from cq rows at episode start" (spec §6.5) is **not computable**
  — there are no pump coordinates on the tape;
* the `ended=` verdict ("pump dead" vs "order dropped") is read off the wrong
  pump's hp, and the `pk` events that would settle it are parsed
  (`ai_tape.py:359`) and then **never used in any section**.

F3 ("an abandoned pump raid") therefore cannot be adjudicated with this tape.

**Fix:** add `rpi` (`d.raid.pump_idx`) and `spi` (`d.strike.pump_idx`) to the `d`
row, and one `{"t":"pmp"}` row (emitted once beside the first `bub`) carrying the
four pump positions + factions. Then the analyzer can compute the real thing:
range-to-ordered-pump over the episode, on-station time, and hp delta of the
*right* pump. Also print `pk` events in section 5/7.

## P1-3. Crash-safety window is 10 s, and the investigation target is a crash

`app/main.cpp:1764-1772` flushes only when `loop.tick_count -
conquest_tape_last_flush >= 1200`. I verified `LoopState::tick_count` is monotone
and never reset — including across player death (`app/instructor_tick.h:458`,
comment "never reset by the crash branch below") — so the cadence IS reachable
every 1200 ticks. But a `kill -9`, a GPU hang, or an unhandled fault loses the
last ≤10 s, which is precisely the window containing the event under
investigation (the tunnel crash, the final merge). The tape is ~11.6 KB/s with
10 drones (measured on `build/conquest_tape_1.jsonl`: 37 332 B / 3.2 s ≈
42 MB/h), so a **120-tick (1 s) flush costs nothing** and shrinks the loss
window 10×. Recommend changing 1200 → 120.

Verified good: a truncated tape parses cleanly — the incomplete final line is
dropped with a warning and the signature reports `ABSENT (crash tape)` rather
than `MISMATCH` (`tmp_verify/crash_tape.jsonl`). An empty/no-hdr file also
degrades gracefully. No crash-safety defect beyond the window size.

---

# P2 — polish, latent bugs, test gaps

**Analyzer**

* `tools/ai_tape.py:179` — the degenerate branch of `dist_to_bubble_edge`
  returns a **bare float** while every other path returns a 2-tuple; the caller
  at `ai_tape.py:584` does `dist, bearing_deg = ...`. Reproduced:
  `TypeError: cannot unpack non-iterable float object` for a query at the bubble
  centre (or the origin). Unreachable in practice (needs alignment to 1e-9 rad)
  but it is an unconditional report crash if it ever hits.
* `ai_tape.py:200-204` `fmt_time` prints **`01:60.00`** and `00:60.00`
  (`%05.2f` rounds 59.9952 up). Seen in my hand tape's `duration:` line and all
  over section 7. Cosmetic, but it will read as a bug to Chad.
* `ai_tape.py:502-504` — dead loop (`for r in ...: pass`).
* `pk` rows are parsed and never reported in any section (see P1-2).
* `ai_tape.py:438` calls `max-min` of at-player ef ticks "time-under-fire"; it is
  a SPAN, not an integral (the label does say "span" — keep it that way).

**Recorder**

* `conquest_tape.h:314` / `:330` — `dk`/`dc` report the PREVIOUS tick's snapshot
  position. For an inert death the drone is frozen, so `d.curr.position` at hook
  time is the exact death point and `prev.pos` is ~1 m (one tick at 120 m/s)
  stale. Only the respawn arm genuinely needs `prev.pos`.
* `emit_ef` (`conquest_tape.h:393-400`) skips inert drones. `combat_tick` runs
  AFTER `enemy_fire_tick` in the same tick (`instructor_tick.h:1269-1273`), so a
  bandit that fires and is killed by the player in the same tick has its shot
  credited to the nearest OTHER drone within 100 m, or to `-1`. Rare, silent,
  and the 100 m cap is the only guard. (Attribution is otherwise **exact**:
  `CombatSetup::bandit_gun.muzzle_body = CG(0)`, so `round.pos ==
  d.curr.position` and `best_d == 0` for the true shooter — a wingman pair
  within 100 m cannot steal it.)
* `snprintf` return values are unchecked. A divergent/NaN-huge coordinate would
  truncate a line without its `\n`, merging two JSON objects — the analyzer skips
  it as malformed AND the signature mismatches. Buffers are adequate for sane
  values (worst realistic `d` line ≈ 360 B in a 512 B buffer).
* `prev_drones_` is indexed by VECTOR index, not `spawn_index`. Safe today —
  `dw.drones` is built once at `app/main.cpp:1377-1379` and never resized or
  reordered (verified: no `erase`/`resize`/`swap` anywhere) — but it is an
  unstated invariant worth a comment.
* Stale-`dk` hazard: `killed_spawn_indices` is cleared only at the top of
  `combat_tick` (`kill.h:325`), which runs only under `cw && dw && gw`. In any
  wiring with `cw`+`dw` but no `gw`, a stale list would make the recorder emit
  the same `dk` **every tick forever**. Not reachable from `main.cpp`; reachable
  from a test/harness. Cheap guard: only trust `killed_spawn_indices` when the
  tape also saw `cw->kills` increase.
* `tag` is hard-coded `"conquest"` (`app/main.cpp:1541`); spec §4 asked for the
  game.toml-derived scenario name. Deliberate, noted in BUILD_NOTES.
* `dt` is written as `0.008333` (`%.6f` of 1/120), a 3.3e-7 s/tick truncation
  ≈ 0.24 s drift per 100 min of tape. Harmless; consider `%.9f`.
* Silent failure if the ofstream cannot open (`app/main.cpp:1547`).

**Tests — legs that pass under mutation of the thing they claim to pin**

* Leg 2, "conquest tape off arm is bit identical"
  (`test_conquest_tape.cpp:206-252`) — **NOT a fixture no-op, but much weaker
  than it reads.** It passes `gw = nullptr`, `env = nullptr`, and never engages a
  drone. With `gw` null, `combat_tick`, `conquest_tick`, the pump-DPS raid block,
  and every projectile path are structurally skipped; with `env` null there is no
  terrain and no tunnel net. It exercises the kernel step, the patrol drone tick,
  `enemy_fire_tick` with nobody firing, and the conquest order-assignment block —
  and nothing else. Recommend adding `&gw` + engaged drones + a live `env` so the
  differential covers the paths where a firewall breach could actually matter.
  (The firewall itself is structurally sound regardless: every `on_tick`
  parameter is pointer-to-const and `ConquestTape` holds no non-const handle.)
* **No leg pins the hook's POST-tick placement.** Moving
  `instructor_tick.h:1690` to before `tick()` would still pass legs 1 and 2, yet
  every event would shift one tick and every `dk`/`dc` snapshot would be two
  ticks stale. Add a leg asserting `st.curr` seen by the hook equals the
  post-tick state.
* **No leg pins the 100 m attribution cap** (`conquest_tape.h:402`) — leg 3's
  decoy drone sits 12 km away, so deleting the cap passes.
* **Leg 8 passes if `bub` is emitted EVERY tick** — it only asserts presence at
  k=0 and k=1, never absence on an unchanged tick.
* **No coverage at all for `ph` / `pd`** (spec §5 has no leg for them).
* **The binary-mode ofstream fix is untested.** I confirmed a text-mode
  regression is detectable (CRLF-ified tape → `MISMATCH (corrupt)`), but nothing
  in ctest would catch a reintroduction. Consider a byte-level assertion in the
  smoke path or a note in the handoff.

---

# INDEPENDENT VERIFY — my own hand-built tape

`tmp_verify/make_tape.py` writes `tmp_verify/hand_tape.jsonl` (750 KB, 4 400
lines) from spec §3 alone — NOT derived from `_build_selftest_tape_bytes`. Line
bytes use the same `%.3f/%.2f/%.1f` printf shapes `conquest_tape.h` emits.
Geometry chosen so the answers are analytic:

* faction 0 = a **circle** (`a == b == 3000`, centre `+X`); the cluster centroid
  sits at great-circle arc **exactly 4000 m** from the centre, on the `maj`
  bearing (φ = 0, i.e. exactly ON a sample) ⇒ true edge distance **exactly
  1000 m outside**, no sampling error at all.
* faction 1 = circle `a == b == 2000` centred on `-X` ⇒ `R·(π − 4000/R) − 2000`.
* three cluster members whose z-offsets (+400, −400, 0) cancel, so the centroid
  is exact.

| quantity | hand-computed | ai_tape.py reported | verdict |
|---|---|---|---|
| signature | fnv1a over body-after-hdr | `VERIFIED` | PASS |
| cluster count | exactly 1 | `Cluster #1` only | PASS |
| cluster members | [0, 1, 2] | `members=[0, 1, 2]` | PASS |
| cluster centroid | (14469.8197, 3952.7609, 0.0000) | `(14469.8, 3952.8, 0.0)` | PASS |
| cluster duration | 79.9968 s | `dur=80.0s` | PASS |
| both_factions | True (fs 0/1/0) | `both_factions=True` | PASS |
| **bubble 0 edge distance** | **1000.0000 m outside** | **`1000.0m outside`** | **PASS (exact)** |
| bubble 0 bearing phase | 0.0 deg | `0.0 deg` | PASS |
| **bubble 1 edge distance** | **41123.8898 m outside** | **`41123.9m outside`** | **PASS** |
| ef total / at player | 3 / 2 | `3 total shots, 2 aimed at the player` | PASS |
| time-under-fire span | 31.6654 s | `31.7s` | PASS |
| ph events / total n | 2 / 5 | `2 player-hit events (5 total …)` | PASS |
| pd events | 1 @ k=6000 (00:50.00) | `pd t=00:50.00` | PASS |
| raid episode | i=1, k 2400..4800 | `start=00:20.00 end=00:40.00` | PASS |
| raid pump hp delta | −200.0 | `hp_delta=-200.0` | PASS |
| raid range at start/end | 21497.9 m | `21497.9/21497.9` | PASS |
| tn entries / exits | 1 / 1 | `1 entries, 1 exits` | PASS |
| in-net death | dc i=3, mav RUN, net=1 | `dc i=3 … mav=RUN net=1` | PASS |
| dk i=4 with net=0 | excluded from in-net | excluded | PASS |
| duration print | 119.9952 s | `01:60.00` | **FAIL (cosmetic, P2)** |

Extra probes:

* **720-sample ellipse approximation, worst bearing** (φ offset by half a sample,
  0.25°), circle `a = 3000`: query at arc 4000 → reported 1000.1122 (err
  **+0.11 m**); arc 3100 → 100.8694 (err +0.87 m); arc 3050 → 51.69 (err
  +1.69 m); arc 3000 (exactly on the edge) → +13.00; arc 2995 (5 m inside) →
  −13.92. **Bias is always outward and bounded by ~13 m**, well inside the
  docstring's claimed "<150 m", and the SIGN is correct even 5 m inside the
  edge. The approximation is sound.
* **fnv1a agreement**: verified on my hand tape (negative coordinates, ticks up
  to 14 400) and on the REAL smoke tape `build/conquest_tape_1.jsonl` (37 332 B,
  195 lines, `signature: VERIFIED`, **0 CR bytes** — the binary-mode fix holds).
  Flipping one body byte → `MISMATCH (corrupt)`. CRLF-ifying the whole tape →
  `MISMATCH (corrupt)`.
* **Crash tapes**: 700 KB prefix of the hand tape → `ABSENT (crash tape)` +
  `Truncated final line ignored (line 3935)`, report still builds. Zero-byte file
  → graceful "No hdr line found".
* `python tools/ai_tape.py --selftest` → `SELFTEST OK`.
* `cmake --build build` → clean; `ctest -R conquest` → **29/29 passed**
  (includes the 8 new conquest-tape legs). FULL suite: **966/966 passed, 0
  failed**, confirmed on two independent runs (665 s and 722 s).

## Scratch files (delete before commit)

`tmp_verify/ef_alias.cpp`, `ef_alias.exe`, `make_tape.py`, `hand_tape.jsonl`,
`crash_tape.jsonl`, `corrupt_tape.jsonl`, `crlf_tape.jsonl`, `empty.jsonl`,
`VERDICT.md`.

---

# ROUND 2 RE-VERIFICATION (narrow, after the §9 amendment was applied)

## ROUND2 VERDICT: **CONFIRMED-FIXED** for P0-1, P0-2, P1-1 and P1-3.
## One residual **P1** on the P1-2 half: section 5 "episodes" are still standing-order windows, only better labelled.

Scope: only the four items the coordinator named; no re-audit of untouched
round-1 material. `cmake --build build` clean; the 10 conquest-tape ctest legs
**10/10 pass**, and the FULL suite is **968/968 passed, 0 failed** (577 s);
`python tools/ai_tape.py --selftest` → `SELFTEST OK`.

## 1. P0-1 (`ef` via the spawn contract) — CONFIRMED FIXED, exact, no double-count

`app/conquest_tape.h:440-449` now scans every pool slot for
`round.active && round.age == 0.0 && round.prev_pos == round.pos`, and
`prev_pool_active_` is gone. `emit_ef_spawns` is called at
`conquest_tape.h:124`, **outside** the `has_snapshot_` gate — correct: the
contract needs no baseline, so a spawn on the recorder's very first tick is
caught.

**Re-ran the oracle** (`tmp_verify/ef_alias.cpp`, extended to score the NEW
detector verbatim alongside the old one, linking the real
`combat::enemy_fire_tick`):

| scenario | true | old detector | NEW detector | delta |
|---|---|---|---|---|
| 4 bandits diff 4, continuous | 3200 | 320 (-90 %) | **3200** | **0** |
| 3 bandits diff 3, continuous | 1800 | 180 (-90 %) | **1800** | **0** |
| 1 bandit diff 5, continuous | 1100 | 110 (-90 %) | **1100** | **0** |
| 10 bandits diff 5, continuous | 11000 | 1100 (-90 %) | **11000** | **0** |
| 4 bandits, 0.25 s bursts / 1.0 s | 800 | 80 (-90 %) | **800** | **0** |
| 4 bandits, 1 s burst / 3 s | 1088 | 1088 | **1088** | **0** |
| 4 bandits, 1 s burst / 2.5 s | 3000 | 300 (-90 %) | **3000** | **0** |
| 7 bandits diff 2, 5/12 duty | 4669 | 280 (-94 %) | **4669** | **0** |

~24 000 spawns across 8 configurations: **zero missed, zero double-counted**,
including every same-tick retire-and-refill case (the rows where the old
detector lost 90 %+ are exactly the saturated-pool rows).

The oracle is **not circular**: for the four continuous rows the truth count is
independently analytic — `difficulty_params` rof x duration x bandits gives
8 Hz x 100 s x 4 = 3200, 6x100x3 = 1800, 11x100x1 = 1100, 11x100x10 = 11000,
each matching the measured "true" column exactly.

**Can `age` stay 0.0 across a tick (double-count)?** No.
`weapon::advance` (`weapon/ballistics.cpp:128-140`) unconditionally executes
`p.prev_pos = p.pos; … p.pos += p.vel*dt; p.age += dt;`, and `enemy_fire_tick`
step 1 (`combat/kill.h:443-447`) advances **every** active round before step 2
spawns any. A round therefore satisfies the contract for exactly one hook
observation, and **both** guards break simultaneously on the next tick
(`age = dt`, `prev_pos != pos`). Confirmed empirically by the delta-0 column.

Residual, unchanged from round 1, both tiny and both inherent to an
end-of-tick observer (P2): a round spawned and retired inside the same tick is
still invisible — `combat_player_tick` retiring a point-blank round
(`kill.h:595-620`), and the player-death sweep that clears the whole enemy pool
(`app/instructor_tick.h:1339-1341`) on a tick where a bandit also fired.

New latent P2: the detector assumes `enemy_fire_tick` ran this tick. The whole
combat block is gated on `cw != nullptr && dw != nullptr`
(`instructor_tick.h:1265`); in a wiring with `cw` but no `dw`, an active
`age == 0` round would be re-emitted every tick. Not reachable from `main.cpp`
(both are always passed together).

## 2. P0-2 (`dk` / `da` / `dc`) — CONFIRMED FIXED, discrimination is complete

`app/conquest_tape.h:331-369`, checked against every death/respawn path found
in round 1:

| real event | code site | tape tag | correct? |
|---|---|---|---|
| killed by the PLAYER's guns | `combat/kill.h:415` + `:417` (kill list) | `dk` | yes |
| shot down AI-vs-AI | `app/instructor_tick.h:1311` (inert, no kill list) | `da` | yes |
| terrain/tunnel crash | `drone/drone.h:1483` -> `respawn_in_place` (`age_ticks = 0`, `drone.h:1030`) | `dc` | yes |

`age_ticks` is `long long` (`drone/drone.h:458`), monotone `++` per live tick,
and `respawn_in_place` is its **only** reset — so `d.age_ticks <
prev.age_ticks` is an exact, hp-independent, jump-independent crash signal. The
hp_reset/jump arm is deleted and `DroneSnap::hp` with it.

* **Same-tick kill + respawn double-report?** No. On the pre-conquest path
  (`respawn_drones == true`) `combat_tick` calls `respawn_in_place`, so the
  drone is in the kill list AND its `age_ticks` resets — but the `continue` at
  `conquest_tape.h:350` emits `dk` only. The three branches are mutually
  exclusive by construction (`dk` -> continue, `da` -> continue, `dc` last).
* **First-tick safety?** `emit_events` runs only when `has_snapshot_`, and
  `if (!has_prev) continue` (`:337`) now guards all three branches, so none can
  read a default-constructed snapshot. Safe.
* Minor behaviour change worth knowing (P2): `has_prev` now gates `dk` too
  (round 1 emitted it from a default snapshot). Harmless today —
  `dw.drones` is built once at `app/main.cpp:1377-1379` and never resized — but
  a kill on a fleet-growth tick would be dropped.
* Ultra-rare ordering P2 (unchanged): a drone that crashes/respawns in
  `drone::tick` and is then killed by a player round later in the SAME tick
  reports `dk` only; the crash is swallowed by the `continue`.

## 3. The three new test legs — none is a fixture-no-op; 2/2 mutants caught by spot-run

I built a **mutated copy** of the recorder (`tmp_verify/mutant_tape.h`,
namespace `seads_tape_mutant`; no implementation file touched) reverting the two
P0 fixes, and replayed the legs' own fixtures against it
(`tmp_verify/mutation_check.cpp`):

* **MUT-A** — `ef` reverted to the slot-index rising edge (`prev_pool_active_`
  re-added): leg *"enemy round spawn edge fires on a same-tick slot-reuse
  fixture"* -> `ef@k=1` **absent** => its `REQUIRE_FALSE(ef.empty())` **FAILS**.
  The leg genuinely pins P0-1.
* **MUT-B** — `dc` reverted to the jump/hp_reset arm, made *more generous* than
  the original (`hp_reset` forced true, only the 2 km jump required): leg
  *"drone kill vs crash discrimination"* -> `dc@k=2` **absent** => its
  `REQUIRE_FALSE(dc.empty())` **FAILS**. The leg genuinely pins P0-2's crash
  half; a fortiori for the true old arm, which also needed an hp rise.

Result: `mutants caught: 2 / 2`.

Hand-reasoned for the third leg and for extra mutants:

* *"da vs dk discrimination"* — non-vacuous (two distinct drones, both branches
  exercised). Reverting to the pre-amendment code tags tick 2 `dc` instead of
  `da` => `REQUIRE_FALSE(find_line(out,"da",2).empty())` fails. Swapping branch
  order (inert checked before kill-list) tags tick 1 `da` =>
  `REQUIRE_FALSE(find_line(out,"dk",1).empty())` fails. Both caught.
* The slot-reuse leg's `CHECK(find_line(out,"ef",0).empty())` is a real
  anti-vacuity / anti-double-count pin: a detector firing on any active round
  would emit at tick 0 and fail.
* `dc` with `<=` instead of `<` is caught too — drone 1 (vector index 0,
  `age_ticks` static at 0) would emit `dc` at tick 2 first, and `find_line`
  returns the FIRST match, so `CHECK(dc.find("\"i\":2"))` fails.
* Leg 4 also asserts its own premise (`dw.drones[1].hp == hp_before`) — the
  right discipline: it proves the deleted arm is not what did the detecting.
* Gap (P2): neither `age == 0.0` nor `prev_pos == pos` is pinned
  *individually* — dropping either guard alone still passes the leg. Low risk
  (they diverge only for a zero-velocity round), but a two-round fixture would
  close it.

## 4. Analyzer round 2 — reads the new fields; ONE residual P1

Verified by reading + running:

* `KNOWN_TAGS` includes `pmp` and `da` (`ai_tape.py:53`); `an.pmp` is the
  one-shot row (`:366`), `an.da` its own list (`:381`).
* `_pump_target_episodes` (`:634-657`) keys on `rpi`/`spi` transitions, closes
  and reopens on a re-target with no gap, and forces `-1` while `inert`. Edge
  cases reviewed (leading `-1`, direct `A->B`, trailing run): all correct.
* `_pump_label` (`:659-670`) resolves the index against the `pmp` row's
  `pos`/`fac`, degrading to `pump=N` when `pmp` is absent.
* Section 5 `ended=` now discriminates four outcomes including *"drone shot
  down by another AI"* (`da`) vs *"drone crashed"* (`dc`), and **`pk` rows are
  now printed** in sections 5 and 7 (round 1's "parsed and never used" gap is
  closed).
* Section 6 lists `dk`/`da`/`dc` separately with an explicit legend
  (`:745-758`).
* Section 3 (P1-1) now separates *duty* (mav mode, eng, leash) from *"standing
  orders armed (NOT behavior)"* with a four-line preamble. Wording fix accepted.
* P2 `fmt_time` rollover fixed (`:214-223`, centisecond-integer split): my
  round-1 hand tape now prints `duration: 02:00.00` where it printed
  `01:60.00`.
* Flush cadence is now 120 ticks (`app/main.cpp:1766`) — P1-3 fixed.
* Backward compatibility: my round-1 hand tape (no `pmp`/`rpi`/`spi`) still
  parses, still verifies its signature, and degrades to "No raid or strike
  episodes observed" rather than crashing.
* `emit_drone`'s buffer was raised 512 -> 600 for the two new ints; `emit_pmp`
  builds a `std::string`, so neither can truncate at realistic magnitudes.

### RESIDUAL P1 — `rpi`/`spi` ARE the standing orders; section 5 asserts they are not

`d.raid.pump_idx` is set on exactly the ticks `d.raid.active` is set
(`app/instructor_tick.h:1177-1183`), and `d.strike.pump_idx` on exactly the
ticks `d.strike.active` is set (`:1190-1197` — the block whose own comment reads
*"this is a standing order, not a beeline"*). Both default to `-1`
(`drone/drone.h:375`, `:395`). So `idx != -1` is **the same predicate** as the
boolean flag, and `_pump_target_episodes` yields **exactly the same partition of
time** the old `_episodes(rows, "strike")` did.

The pump *identification* is genuinely fixed (the drone's OWN target with a real
position, not the shared HUD `rp`). The **episode segmentation is not** —
section 5 still reports authorization windows, and its new preamble
(`ai_tape.py:673-675`) actively claims the opposite:

> "Episodes below key on each drone's OWN rpi/spi target-pump index (Sec 9
> P1-2), **not the raid/strike standing-order flags** (Sec 3)…"

For the shipped fleet that means one whole-tape "strike episode" per drone
against the enemy deep pump, now decorated with a plausible pump label — a
*more* convincing wrong answer than round 1's. F3 ("an abandoned pump raid")
would still be misread.

**Fix (report-side only, and now actually computable because `pmp` exists):**
derive ON-STATION duty the way the game does. `combat::raider_on_station`
(`combat/raid.h:85-93`) is `range <= raid_range_m && dot(nose, to_pump) >=
0.35`, with `raid_range_m = 1000 m` for raids (`raid.h:40`) and `600 m` for
strikes (`strike_params`, `raid.h:75-80`). The tape carries `pos` and `vel` and
`pmp` carries the pump positions, so:

```python
lim = 1000.0 if kind == "raid" else 600.0
to_pump = vsub(pmp_pos[idx], row["pos"])
on_station = (vlen(to_pump) <= lim and
              vdot(vnorm(row["vel"]), vnorm(to_pump)) >= 0.35)
```

(`vel` direction is a sound proxy for nose in this flight model.) Print per
episode: the **order window** (what it prints today, honestly relabelled) AND
**on-station seconds + closest approach to the ordered pump** — the latter is
the number that settles "abandoned raid". Also correct the preamble sentence.

### RESIDUAL P2 (carried, still unfixed)

`dist_to_bubble_edge`'s degenerate branch (`ai_tape.py:193`) still returns a
bare float while every other path returns a 2-tuple. Re-probed this round: it
still yields `-2999.9999999999995` instead of a tuple, so the caller at `:624`
would raise `TypeError: cannot unpack non-iterable float object`. One-line fix:
`return (-unsigned if unsigned > 0 else 0.0), 0.0`.

### Round-2 scratch files (delete with the rest)

`tmp_verify/mutant_tape.h`, `mutation_check.cpp`, `mutcheck.exe`,
`ef_alias2.exe`, `round2.md`.
