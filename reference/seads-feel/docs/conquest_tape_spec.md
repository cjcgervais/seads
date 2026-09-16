# CONQUEST TAPE — Phase 0 spec (Fable, 2026-08-06)

The binding spec for the AI primary-data recorder + analyzer
(docs/ai_primary_data_handoff.md Phase 0/0b). Implementers follow this
EXACTLY; anything ambiguous escalates back to Fable, never gets invented.

HARD RULES (the seal-pass binding):
- NEVER touch `sim/`, `control/`, any golden, or `test/harness/recorder.h`
  (the felt tape format is pinned).
- The recorder is a READ-ONLY observer: const refs/pointers in, bytes out.
  It must be structurally incapable of changing a trajectory.
- Null/off arm must be BIT-IDENTICAL (the DroneWorld/GunWorld firewall
  pattern; a differential test proves it).
- All TEST_CASE/SECTION names pure ASCII (the 4x-recurred ctest trap).
- No wall clocks, no rng anywhere near the tick path. Tick index is the only
  time. File I/O happens in the app layer only (main.cpp owns flush timing —
  flushing may use frame boundaries; SAMPLING is tick-driven).

---

## 1. THE TAP — a second per-tick hook on `app::step_frame`

`app/instructor_tick.h` already threads a per-tick `TickHook` (felt recorder,
F9). Add a SECOND, independent hook alongside it — do not alter the existing
one:

```cpp
// app/instructor_tick.h (near TickHook)
using ConquestTapeHook = void (*)(const TickInput& in, const LoopState& st,
                                  const DroneWorld* dw,
                                  const combat::CombatWorld* cw,
                                  const ConquestWorld* cq,
                                  const sim::Environment* env, void* ctx);
```

`step_frame` gains two trailing defaulted params `ConquestTapeHook cq_hook =
nullptr, void* cq_hook_ctx = nullptr` (AFTER the existing `cq` param, so every
existing call site compiles unchanged). Inside the tick loop, call it at the
END of each tick iteration (after `tick()` returns and after the existing
felt hook call), passing the SAME `dw/cw/cq/env` pointers step_frame received.
Null hook => zero behavior change.

## 2. THE RECORDER — `app/conquest_tape.h` (new header, namespace `seads_tape`)

Header-only, includes: std, glm, `app/instructor_tick.h`, `combat/conquest.h`,
`combat/kill.h`, `drone/drone.h`, `world/tunnel_net.h`, `world/faction_bubbles.h`.
NO raylib. Model on `test/harness/recorder.h` (Recorder class + free
serializers), but the format is JSONL (§3), not columns.

```cpp
class ConquestTape {
 public:
  // Called once per sim tick by the hook. tick = st.tick_count.
  void on_tick(const app::TickInput& in, const app::LoopState& st,
               const app::DroneWorld* dw, const combat::CombatWorld* cw,
               const app::ConquestWorld* cq, const sim::Environment* env);
  // Drain accumulated lines into out (append), clear the buffer. The APP
  // decides when (periodic + exit). Returns false if nothing pending.
  bool drain(std::string& out);
  // The footer signature line over EVERYTHING emitted since construction
  // (running fnv1a over every drained byte). Called once at clean exit.
  std::string footer() const;
  // Header line (version, tag) — emitted by the ctor into the buffer.
  explicit ConquestTape(const std::string& tag);
};
```

Internally keep a running `fnv1a` (copy the constant/prime from
`test/harness/recorder.h` seads_replay::fnv1a — reimplement locally, do NOT
include that header) updated over every byte handed to `drain`, plus the
previous-tick snapshot needed for edge detection (§3.3).

### 2.1 Cadence

- SAMPLE rows: every tick where `st.tick_count % 24 == 0` (5 Hz at 120 Hz).
- EVENT rows: derived EVERY tick (edges must not be missed).
- BUBBLES row: at first on_tick and again on any tick where either faction's
  `cq->state.radius_scale[f]` changed vs the snapshot.

## 3. FILE FORMAT — `cqtape-v1` JSONL

One JSON object per line, hand-emitted with `snprintf`/ostream (no JSON lib).
All floats `%.3f` unless noted. Keys short but unambiguous. Line types:

### 3.0 header / footer

```
{"t":"hdr","v":"cqtape-v1","tag":"<tag>","dt":0.008333,"sample_every":24}
{"t":"sig","fnv1a":"<decimal uint64>"}          // clean-exit only
```

The sig hashes every body byte AFTER the hdr line and BEFORE the sig line
(hdr excluded so the app can write it immediately; document this in the
header comment). A tape without a sig line = crash tape, still valid.

### 3.1 sample rows (every 24th tick)

Player (`k` = tick index everywhere):
```
{"t":"p","k":N,"pos":[x,y,z],"vel":[x,y,z],"hp":H,"alive":0|1,"cr":N}
```
- pos/vel from `st.curr` (`%.2f` pos, `%.2f` vel). hp = `cw->player_hp`
  (`%.1f`); alive = `cw ? combat::is_dead(cw->damage)?0:1 : 1`.
- **`cr` (RUNG S3-GUNS)** = `cw->cosmetic_rounds`, the CUMULATIVE count of
  cosmetic rounds spawned (`%lld`, 0 with no `cw`). Cosmetic rounds are the
  tracers that give the abstracted AI-vs-AI DPS and the pump-raid credit a
  visible source; they carry `damage == 0.0` and are handed to no damage sweep
  (`combat/kill.h`), so **`cr` can never explain a point of lost HP — it
  explains the LOOK of one.** Differences between consecutive `p` rows give the
  live rounds/s. Monotone non-decreasing within a session.

Per drone (one line each, every sample tick, dead/inert included):
```
{"t":"d","k":N,"i":spawn_index,"pos":[..],"vel":[..],"hp":H,"inert":0|1,
 "eng":0|1,"foe":F,"wf":0|1,"bfm":B,"mav":M,"raid":0|1,"strike":0|1,
 "def":0|1,"leash":0|1,"fs":0|1,"net":0|1,
 "ta":0|1,"gc":G,"bc":B,"af":A,"ds":0|1,"vt":0|1}
```
- foe: the raw int (`kFoeNone -1`, `kFoePlayer -2`, else drone index).
- bfm: `static_cast<int>(d.bfm.mode)`; mav: `static_cast<int>(d.mav.mode)`.
  Document the enum name lists verbatim in the header comment of
  conquest_tape.h AND in the hdr-adjacent comment of ai_tape.py:
  bfm 0=Intercept 1=Offensive 2=Yoyo 3=Extend 4=Perch 5=Slash 6=Defensive (4-6 = RUNG E7; 0-3 unchanged);
  mav 0=PATROL 1=TRANSIT 2=DIVE_IN 3=RUN 4=CLIMB_OUT.
- raid/strike/def: `d.raid.active`, `d.strike.active`, `d.defend.active`.
- leash: `d.leash_engaged`. fs: `d.friendly_side`.
- net: 1 if `env && env->tunnels && env->tunnels->contains(d.curr.position)`.
- RUNG B2 (2026-08-26), additive — a pre-B2 tape simply lacks these four keys
  and the analyzer must read them with `.get()` and print "n/a (pre-B2 tape)".
  The wire version stays `cqtape-v1`; the added keys are ~44 B/row (+17-19% on
  a full tape).
  - `ta`: `d.terrain_avoid_engaged` (0|1) — the hard-deck/terrain-avoid latch.
  - `gc`: `d.cmd_gamma`, `%.4f`, **RADIANS** — the FINAL commanded gamma handed
    to `autopilot()` this tick, i.e. AFTER the terrain-avoid override, the
    arena gamma clamp, the arena-shell guard and the E1.4 bank slew. Under the
    latch `gc == avoid_gamma` exactly (that is the latch's signature).
  - `bc`: `d.cmd_bank`, `%.4f`, **RADIANS** — likewise for the commanded bank.
  - `af`: `d.air_frac`, `%.3f`, 0..1 — `sim::atm_frac_at` sampled at the
    POST-TICK position, i.e. the `pos` on this same row (a respawn tick reports
    the air at the respawn slot, not at the wreck).
  - `ds`: `d.deck_scope` (0|1) — RUNG S1-DECK, additive on the same terms (a
    pre-S1-DECK tape lacks it). The DECK-SCOPE latch: 1 means the air at the
    shipped `avoid_agl_release_m` (400 m) over this drone's own ground point
    is below `avoid_air_frac_full - deck_scope_hyst_frac`, i.e. it is flying
    the 60/110 m deck band and not the in-bubble 250/400 one. With `ta` it
    says WHICH band a latch tick belonged to.
  - `ed`: `d.air_depth`, `%.1f`, METRES — ENV-1, additive on the same terms
    (a pre-E1 tape lacks it). Signed horizontal depth into the nearest LIVE
    dome's air: positive inside, negative outside, 0.0 exactly on the edge,
    taken as the MAX over live domes (air is air, whoever made it). ⚠ `-1e9`
    is the "no live dome anywhere" sentinel and is not a distance. ⚠ It is not
    `d.leash`'s geometry: the leash SWAPS to the enemy ellipse when a faction's
    own dome dies, which is why the app stamps both domes explicitly.
  - `vt`: `d.raid.via_tunnel` (0|1) — RUNG S2-TUNNEL, additive on the same
    terms (a pre-S2 tape lacks it). 1 means the app's raid ROUTER has ruled
    this raid must go by the bore: the direct great circle from this drone's
    own position to the target surface pump crosses more thin air (outside BOTH
    faction ellipses) than the tunnel route does, and this pilot did not win
    one of the `raid_deck_run_slots`. Re-decided every tick, so it flips back
    to 0 at the far mouth — that flip is the run-scheduler freeze returning and
    the raid branch taking the approach. Read with `raid` and `mav`: a raid
    row with `vt`=1 and `mav`>0 IS a raid flown through the tunnel.
  - gc/bc are the command that PRODUCED the step into the recorded `pos`.
    ★ All four are READ-ONLY report fields: no game decision branches on them.

Conquest (one line per sample tick):
```
{"t":"cq","k":N,"pump_hp":[h0,h1,h2,h3],"rs":[r0,r1],"score":[s0,s1],
 "out":O,"pua":0|1,"rp":R,"cd_f":F,"cd_s":S,"planes":P}
```
- from `cq->state`: pumps[i].hp, radius_scale, score, `int(outcome)`,
  countdown_faction/countdown_s, planes_left; `pua`/`rp` from the
  ConquestWorld fields pump_under_attack/raided_pump.

### 3.2 bubbles row

For each faction f, compute `world::faction_ellipse(f, grow, cdir, maj, a, b)`
with grow from `app::faction_growth_from_state(cq->state, grow)`:
```
{"t":"bub","k":N,"f":[{"c":[x,y,z],"maj":[x,y,z],"a":A,"b":B},{...}]}
```
c/maj unit vectors `%.6f`; a/b metres `%.1f`. This is what lets the analyzer
compute distance-to-bubble-edge in Python without re-deriving world code.

### 3.3 event rows (checked every tick, emitted on occurrence)

Edge detection uses a snapshot of the previous tick held inside ConquestTape
(per drone: inert, hp, net flag, mav mode, pos; per pump: hp; per enemy-pool
slot: active; player: cw->deaths, cw->player_hits, cw->player_hp; conquest:
radius_scale). First on_tick initializes the snapshot, emits no events.

- ENEMY ROUND SPAWNED (F1's instrument):
  a `cw->enemy_pool` slot with active rising edge (false/absent -> true).
  On the spawn tick `prev_pos == pos` (kill.h spawn contract), the round sits
  at the shooter's muzzle. Attribute shooter = the non-inert drone minimizing
  `|d.curr.position - round.pos|` (record that distance as `att_m`; it should
  be < ~30 m — if > 100 m emit shooter -1, never guess silently).
  ```
  {"t":"ef","k":N,"shooter":I,"att_m":D,"rng_p":Rp,"tgt_foe":F}
  ```
  rng_p = |shooter pos - player pos| (or -1 if shooter unknown);
  tgt_foe = the shooter's `d.foe` this tick.
- PLAYER HIT: `cw->player_hits` increased.
  `{"t":"ph","k":N,"n":delta,"hp":player_hp_after}`
- PLAYER DEATH: `cw->deaths` increased. `{"t":"pd","k":N}`
  ★ AMENDED BY RUNG E6.1 (2026-08-20) — see §10 below. The edge is now
  `cw->death_events` (BOTH death branches) and the row carries the CAUSE.
- DRONE KILLED BY FIRE: spawn_index in `cw->killed_spawn_indices` (non-empty
  only on the kill tick; snapshot not needed).
  `{"t":"dk","k":N,"i":I,"pos":[..],"foe":F,"rng_p":Rp,"mav":M,"net":0|1}`
  pos/foe/mav/net = the PREVIOUS-tick snapshot of that drone (its state when
  it died, before any wreck handling); rng_p = distance to player.
- DRONE CRASHED (the F4 instrument): a drone whose `inert` rose this tick
  WITHOUT appearing in killed_spawn_indices, OR whose respawn fired
  (pre-conquest arm: position jump > 2 km in one tick while hp reset). In
  conquest the inert-rise-without-kill IS the terrain/tunnel crash.
  `{"t":"dc","k":N,"i":I,"pos":[..],"mav":M,"net":0|1,"vel":[..]}`
  (all from the previous-tick snapshot — the state that flew into the wall).
- REINFORCEMENT (rung E5, 2026-08-20 amendment): a slot going inert -> alive
  is a WAVE REVIVE, its own event, emitted BEFORE the age_ticks respawn
  branch (a wave also resets age_ticks — without this ordering every wave
  reads as a phantom `dc` terrain crash and poisons attribution).
  `{"t":"dr","k":N,"i":I,"pos":[..]}`  (pos = the fresh launch position).
- TUNNEL TRANSITION: per drone and player, `net` flag edge either way.
  `{"t":"tn","k":N,"who":I,"in":0|1,"pos":[..]}`  (who: -2 player, else
  spawn_index — same convention as foe).
- PUMP DEATH: `pumps[i].hp` fell to <= 0 this tick (alive edge).
  `{"t":"pk","k":N,"pump":I,"fac":F}`
- PUMP REPAIRED (L2, 2026-09-01): the OPPOSITE alive edge -- a millwright
  finished a fix and `alive` went false -> true. ADDITIVE: no existing row
  changes, so a pre-L2 tape decodes unchanged and a session with no repair in
  it emits none of these. `rs` is the LIVE radius_scale pair after the revive.
  `{"t":"pr","k":N,"pump":I,"fac":F,"rs":[R0,R1]}`
- BUBBLE CHANGE triggers a fresh `bub` row (§3.2), not an event row.

Budget note: 10 drones x 120 Hz edge checks are integer/bool compares +
one contains() per drone per tick; the pool scan is over `enemy_pool.size()`
slots (bounded, reused). No allocation per tick except when events fire and
when sample rows serialize (reserve the string buffer).

## 4. APP WIRING — `app/main.cpp`

- AUTO-ON: exactly when `conquest_on` is true (the same flag that passes
  `&cq` into step_frame). NO keybind, NO config knob. Construct
  `seads_tape::ConquestTape tape(tag)` before the frame loop; tag = the
  game.toml-derived scenario name or "conquest".
- File: `conquest_tape_<n>.jsonl` beside the exe — reuse the same
  next-free-index scan pattern the felt recorder uses (main.cpp ~line 310).
  Open the ofstream ONCE at startup (append mode not needed), write the
  drained hdr immediately.
- Hook: a free function in main.cpp's anon namespace wrapping
  `ConquestTape::on_tick` via ctx (mirror felt_recorder_hook exactly).
  Passed to step_frame only when conquest_on.
- FLUSH: after step_frame each frame, if `st.tick_count - last_flush_tick >=
  1200` (10 s sim), `tape.drain(buf); file << buf; file.flush();` — a crash
  loses at most 10 s. On clean exit (after the frame loop): drain, write,
  write `tape.footer()`, flush, close.
- The smoke path (`--smoke`) must not break: recorder simply runs if conquest
  is on; no interaction with screenshots.

## 5. TESTS — `test/unit/test_conquest_tape.cpp` (new TU, wired like siblings)

ASCII names. Use existing test fixtures/patterns (see test_conquest.cpp,
test_instructor_tick.cpp for how to build LoopState/DroneWorld/CombatWorld).

1. "conquest tape hook fires once per tick with the frame's own pointers" —
   step_frame with a counting hook: calls == res.ticks; the dw/cw/cq/env
   pointers received == the ones passed (pointer equality).
2. "conquest tape off arm is bit identical" — same seed/inputs, two runs
   (hook null vs hook recording), LoopState.curr AND every drone curr
   bit-identical over ~200 ticks (the S8-drone differential firewall leg).
3. "enemy round spawn edge attributes the shooter" — hand-built CombatWorld:
   pool slot flips active with pos == drone 3's position; expect one ef event,
   shooter == drone 3's spawn_index, rng_p == the hand-set player range.
4. "drone kill vs crash discrimination" — tick A: drone in
   killed_spawn_indices + inert rise => dk (with the PRE-death snapshot pos);
   tick B: different drone inert rise alone => dc, no dk.
5. "tunnel transition edges both ways for player and drone" — a minimal
   TunnelNet (borrow the smallest builder used by test_maverick/test_tunnel
   tests); move a drone across contains() boundary in and out => two tn events
   with correct `in` values; same for the player (`who` == -2).
6. "sample cadence is every 24th tick and only that" — run 49 hand-driven
   on_tick calls with tick_count 0..48; drain; count "t":"p" lines == 3
   (ticks 0, 24, 48).
7. "footer signature matches a recomputed fnv1a over the drained body" —
   drain everything, recompute the hash over bytes-after-hdr, compare with
   footer(); then corrupt one byte and REQUIRE mismatch.
8. "pump death and bubble rows fire on the radius change tick" — drop a pump
   hp to 0 + bump radius_scale in the state between ticks => pk event + a bub
   row on that tick.

Every test parses its own drained output with plain string find/substring
checks (no JSON library) — but each asserted line must be located by its
`"t":"xx"` tag + tick, never by absolute line index.

## 6. ANALYZER — `tools/ai_tape.py` (Phase 0b)

Python 3 stdlib only (json, math, argparse, collections). Optional
matplotlib import guarded (`--png` flag; text report is the product).

`python tools/ai_tape.py <tape.jsonl> [--png out.png] [--selftest]`

Report sections (plain text, in this order):
1. TAPE: version/tag, tick span, duration (ticks * dt), sig VERIFIED /
   ABSENT(crash tape) / MISMATCH(corrupt).
2. THREAT TIMELINE (F1): every ef event (time, shooter, range, tgt_foe),
   every ph/pd; summary: total shots AT any target, shots while tgt_foe==-2
   (at player), hits on player, time-under-fire. If zero ef events: say so
   loudly — plus per-drone foe==player total dwell time and closest approach
   to the player with tick, from sample rows (the "were foes even assigned"
   question).
3. DUTY TIME-IN-STATE: per drone, % of sampled time in each mav mode, bfm
   mode, and each of raid/strike/def/leash/eng flags; foe-assignment churn
   (transitions per minute).
4. DWELL CLUSTERS (F2): sliding detection over sample rows — any >= 60 s
   window where >= 3 drones sit within a 1.5 km radius centroid; report
   centroid (and its distance + bearing to EACH faction bubble edge using the
   latest bub row: distance along the great circle to the ellipse boundary —
   approximate by sampling the ellipse boundary at 720 points, min arc
   distance; document the approximation), member drones with their duty
   flags/foe at cluster time, both-factions flag.
5. RAID/STRIKE TIMELINE (F3): per drone, contiguous raid-active (and
   strike-active) episodes: start/end time, pump index (nearest pump from cq
   rows at episode start), pump hp delta over the episode, player range at
   start/end, whether the episode ended with pump dead / drone dead / order
   dropped.
6. TUNNEL (F4): every tn/dc/dk with net==1: entries, exits, in-net deaths
   with position and mav mode.
7. OUTCOME: score/rs/planes over time (coarse table), final outcome.

`--selftest`: generate a synthetic tape in-memory exercising every line type
with KNOWN truths (e.g. 3 drones parked 500 m apart for 90 s => exactly one
cluster; one ef at a known tick; one in-net dc), run the full pipeline,
assert the report contains each expected finding, print SELFTEST OK. The
selftest is the ctest-side contract too (see §7).

## 7. BUILD WIRING

- Add test_conquest_tape.cpp to the unit test target exactly like its
  sibling TUs (CMakeLists pattern — copy the adjacent line).
- Add a ctest leg running `python tools/ai_tape.py --selftest` IF AND ONLY IF
  a python-invoking ctest precedent exists in this tree (check; the graph
  layer check in gate.sh is bash — if no clean precedent, SKIP the ctest leg
  and note it in the handoff; the selftest still runs by hand and in the
  Opus verification).
- `python tools/graph/graphify.py` must be rerun in the same commit
  (new file + new TU = structural change). The supervising session does this.

## 9. OPUS ROUND-1 FOLDS (2026-08-06 — binding amendments; supersede §3 where they conflict)

P0-1 — ef DETECTION IS THE ROUND'S OWN SPAWN CONTRACT, NOT A SLOT EDGE.
enemy_fire_tick retires and refills a slot in the same call (true->true, no
edge): the slot-edge detector MEASURED 90% missed shots at steady pool size.
Detect instead: a round with `p.active && p.age == 0.0 && p.prev_pos == p.pos`
(the kill.h spawn contract) IS a spawn this tick — no snapshot needed for ef.
Drop prev_pool_active_ entirely.

P0-2 — dc/dk/da REDEFINITION. Exactly two sites set inert: combat_tick pass 2
(fills killed_spawn_indices -> stays `dk`) and instructor_tick.h ~1311
(AI-vs-AI gunnery, NOT in the kill list). A terrain crash NEVER rises inert in
conquest — drone::tick calls respawn_in_place unconditionally; age_ticks
resets to 0 on respawn. Therefore:
- `dk` unchanged (kill-list membership).
- NEW `da` = inert rise WITHOUT kill-list membership (AI-vs-AI shootdown):
  same fields as dk.
- `dc` (true crash/respawn) = `d.age_ticks < prev age_ticks snapshot` (the
  respawn reset), fields unchanged (prev-tick snapshot state); DELETE the
  hp_reset/jump arm. Snapshot gains age_ticks.

P1-2 — RAID ATTRIBUTION NEEDS THE PUMPS AND THE DRONE'S OWN TARGET:
- d rows gain `"rpi":d.raid.pump_idx,"spi":d.strike.pump_idx` (ints, -1 none).
- New static line right after hdr (and never again): `{"t":"pmp","k":K,
  "pos":[[x,y,z],[..],[..],[..]],"fac":[f0,f1,f2,f3],"surf":[s0,s1,s2,s3]}`
  from cq->state.pumps (%.1f positions).
- Analyzer: raid/strike episodes keyed on rpi/spi transitions; pump named by
  index with its pmp position; report pk rows in section 5/7.

P1-1 — STANDING ORDERS, NOT DUTY: raid/strike/def flags are armed as standing
orders for every pilot every tick. Analyzer section 3 must label them
"standing orders" and derive DUTY from mav mode + eng + leash; section 5
episodes key on rpi/spi (above), not the active flags.

P1-3 — flush every 120 ticks (1 s), not 1200.

P2 — fmt_time minute rollover (01:60.00); fix.

TESTS to add (C++): ef fires on a SAME-TICK slot-reuse fixture (retire+refill
one slot, expect the ef event — the P0-1 regression pin); dc via an age_ticks
reset fixture with hp unchanged; da vs dk discrimination (inert rise without
kill-list membership => da, never dc). Keep every existing green leg.

## 8. WHAT SONNET DOES NOT DO

No commits. No touching sim/, control/, test/golden/, test/harness/,
config/*.toml values, or any existing test. No new dependencies. No scope
beyond this file. Findings/blockers go in a BUILD_NOTES.md at the worktree
root for Fable's review.

## 10. RUNG E6.1 AMENDMENT — PLAYER DEATH CAUSE (2026-08-20, binding)

CHAD'S TAPE-2 REPORT: he died twice "for no reason", and the tape carried ZERO
`pd` events.

MEASURED ROOT CAUSE (not a guess — read off the code at the seam): `pd` edge-
detected `combat::CombatWorld::deaths`, and that counter is incremented by
exactly ONE of the app's TWO death-reset branches — the component-death branch
in `app::tick`. The other branch, the TERRAIN-CRASH reset (`st.curr.crashed` /
`altitude <= 0`), resets the airframe and sets `res.respawned` but books
nothing. A crash death was therefore STRUCTURALLY invisible to the recorder,
which is precisely the class of death "I died for no reason" describes.

AMENDMENT:

- `combat::CombatWorld` gains a WRITE-ONLY OBSERVATION LEDGER: `death_events`
  (cumulative, BOTH branches), `death_cause`, `death_pos`, `death_air_frac`,
  `death_in_net`, `death_damage`, and `last_damage_src`. Nothing in `combat/`,
  `drone/`, `sim/` or `control/` reads any of them — no trajectory can move.
  `deaths` is UNTOUCHED and keeps its HUD/score meaning.
- Both app death seams call `app::book_player_death(...)` BEFORE the reset
  overwrites the state the record is made of. The cause comes from state the
  app already holds at the reset (which branch fired, plus the
  `last_damage_src` latch the two existing damage sites now set) — nothing new
  is plumbed through `sim/`.
- The `pd` row becomes:
  `{"t":"pd","k":N,"cause":"crash|gun|ground|component|giveup|none",
    "pos":[x,y,z],"air":F,"net":0|1,"comp":[eng,pilot,wl,wr,str]}`
  where `air` = `sim::atm_frac_at` at the death point and `comp` = the
  component picture at the reset.
- CAUSE TABLE (one table, mirrored in `app/conquest_tape.h::cause_tag` and
  `tools/ai_tape.py::PD_CAUSE_TEXT`):
  `crash` the ground-contact verdict fired; `gun` component death whose last
  damage was an enemy round; `ground` component death whose last damage was a
  terrain scrape; `component` component death with no attributed source; `giveup` the player
  held X on foot / on the machine and gave the life up (no aircraft damage);
  `none` a pre-E6.1 tape.

★ THERE IS DELIBERATELY NO `o2` CAUSE. This build has NO hypoxia/O2 damage path
of any kind — no code anywhere reduces a component for lack of air. A vacuum
death can therefore only present as a `crash` (thrust and lift lapse with
`atm_frac`, the machine mushes into the ground). The recorded `air` fraction is
the honest instrument: the analyzer flags any `crash` with `air < 0.25` as a
vacuum mush. Minting an `o2` cause with no mechanism behind it would be a
guess, and the spec forbids guessing the attribution.

ANALYZER: `tools/ai_tape.py` renders every `pd` with its cause, air fraction,
in-net flag, component picture and a cause tally, on BOTH section-2 branches
(a tape with zero enemy rounds can still hold deaths — that is tape 2). The
`--selftest` emits one `crash` and one `gun` death and asserts both, the
thin-air flag and the tally, per the every-line-type contract.
