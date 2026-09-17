# Kernel v17 candidate — S-tremor: a windowed NET hand-live test (launch packet)

Written 2026-09-15 late, by the v16 session, for the OVERNIGHT autonomous run.
Chad's word (verbatim, 2026-09-15): "please and establish it as a workflow that runs
automatically overnight and that is cheaper on tokens BY USING OPUS ... PLEASE KNOCK
THIS OUT OF THE PRECISION PARK." He is away. NOTHING lands on main tonight — the
run ends at BUILT + GATED + RED-TEAMED + FOLDED + HANDOFF on this lane, awaiting his
fly and his word (kernel law: one dial per version, Chad flies each).

Lane: branch `feel/tremor-netwindow`, worktree `D:\seads_sandboxes\tremor`, off
`origin/main` `edfc60ee8` (kernel == v16 landed tip `354f6df3a`, tag
`kernel-v16-yawbudget-signed`). Predecessor record: `docs/SESSION_HANDOFF_20260915_kernel_v16.md`
(§3 is the process precedent, §4 the traps, §6 the laws — read all three).

---

## §1 THE DEBT (measured, recorded, deferred by the v15 red-team as P2)

Kernel v15 (S-righthand) scales the belly-up righting authority by a hand-rest
ramp: `hand_gate = smoothstep(0, right_hand_rest, hand_rest)`, `right_hand_rest =
0.25 s` (`config/controller.toml` `[auto_level]`). The clock is in
`control/controller.cpp` (search `S-righthand: the hand-at-rest clock`):

    const bool hand_live = in.aim_moved ||
                           glm::dot(in.aim_rate_world, in.aim_rate_world) > 0.0;
    ns.hand_rest = hand_live ? 0.0 : min(hand_rest + dt, right_hand_rest);

"Hand is live" is ANY nonzero aim motion this tick. A ±1-count-per-frame mouse
tremor while belly-up resets the clock every frame, so the gate never climbs and
the airplane stays inverted. Red-team measurement (v15, same instrument family):
integrated righting **1.76 deg with a tremor vs 117.75 deg with a still hand**.
From the seat: "it will not right me." That is the 2026-08-06 resting case
("inverted righting carries NO added delay — as soon as the rest condition is
met") with a drifting hand. Nobody has felt it in flight yet; it is a trap for a
wireless mouse, a shaky hand, a desk vibration.

Why it was NOT patched with a count deadband: a deadband just moves the threshold
and eats small deliberate inputs (S-aimclamp / S-retclamp REJECTED twice — the
aim is uncapped by ruling). The recorded cure shape: a **windowed NET aim
displacement** — sum the aim motion over a short window; a tremor nets to ~0, a
real sweep does not. That is a mechanism change to the hand-live predicate, not a
dial on the existing one.

## §2 THE RULING SPACE (what the build MUST respect)

- **Gun-director law (Chad 2026-09-12):** the aim IS the guns; any auto-righting
  is vetoed while the hand MOVES. A real hand movement of any size must still
  veto. Only motion that nets to nothing over the window may be discounted.
- **The 08-06 ruling:** hands off at the apex still rights him after the short
  hand-rest; `inverted_delay` and `inverted_rate` UNTOUCHED.
- **ONE dial**, in `[auto_level]`, value `0` = structurally OFF = the v16 tree
  bit-identically (the existing hash pin `0xdbdf52980174305e` in
  `test/unit/test_loop_rollover.cpp` MUST still pass with the dial at 0 AND at the
  shipped value on the rolling-loop probe, because that probe never reaches the
  apex; add the new dial's own non-vacuity legs at the apex).
- **Frame-rate invariant** (red-team P1 of v15, AT-9 class): the v15 leg
  "the hand-rest clock is frame-rate invariant" must stay green at 240/60/30/10 fps
  and through a 0.4 s hitch. The window must be measured in SECONDS of sim time
  through `app::step_frame`, never in ticks or frames. Be careful: `aim_moved` is
  true on ONE tick per frame; `aim_rate_world` is ZOH-smeared over the frame's
  other ticks. The net-displacement accumulator must integrate the SMEARED rate
  (or the per-frame delta once) — never double-count, never miss a frame.
- **Every gate/regime/latch leg is hysteretic** (CLAUDE.md). A single threshold on
  net displacement that flips a boolean will chatter — design so that it cannot
  (a continuous measure feeding the existing smoothstep ramp is the preferred
  shape; if a threshold is unavoidable it needs hysteresis and a why-comment).
- **No derivative-on-error term, ever.** **Kernel firewall:** changes confined to
  `control/`, `config/controller.toml`, `config/load_controller.cpp`, the X-macro
  entry, telemetry/tape field if you add one (the tape column count changes —
  update `app/feel_tape_columns.h` and the v5 banner as v16 did, and pin it).
- **Loader:** range-check the new key like `right_hand_rest` (lines ~562 of
  `config/load_controller.cpp`).

## §3 DESIGN DIRECTION (a starting point, not a ruling — improve it if the
instrument says so, and WRITE DOWN why)

Dial: `[auto_level] hand_net_window = <seconds>` (0 = off). Candidate mechanism:

- Keep a leaky/boxcar integral of the aim's SIGNED angular displacement over the
  last `hand_net_window` seconds (two axes, in the aim frame's own right/up, i.e.
  the same basis `apply_mouse` rotates about). A boxcar over a ring of sim ticks is
  the honest choice for "windowed"; a first-order leak is cheaper and continuous
  — either is acceptable if the legs below pass and the fps leg holds.
- `net_disp = |windowed integral|` (deg). Define `hand_live_net = net_disp >
  tiny_floor`, OR better: replace the boolean with a continuous "liveness" that
  scales how fast `hand_rest` resets, so a tremor lets the clock climb and a sweep
  slams it to 0. Whichever you choose, the OFF arm (`hand_net_window == 0`) must
  be the exact v16 expression — guard with `if (cp.hand_net_window > 0.0)` around
  ALL new arithmetic so the golden path evaluates nothing new.
- A floor on the net displacement (like v16's 1 deg/s budget floor) is required so
  roundoff (1e-17) cannot count as motion — the v16 session's "lying instrument"
  trap (§4 of the v16 handoff).

Shipped value: pick from the instrument (expect ~0.1–0.25 s, comparable to
`right_hand_rest`). Record the sweep table in the handoff. Do NOT tune against
harness numbers past what the legs need — Chad flies it.

## §4 THE INSTRUMENT (extend `test/unit/test_loop_rollover.cpp`, S-righthand block)

Use `apex_probe` / `harness::ClosedLoop` for apex legs and `rest_at` /
`app::step_frame` for frame-rate legs. Add a tremor driver: per FRAME, `aim_dx`/
`aim_dy` alternate ±1 count (and a variant: random-walk ±1 with zero mean, seeded
by a fixed LCG — no `std::random_device`). Legs (each with a MUTATION note saying
what reds it):

1. **The tremor leg** — belly-up (theta ~170 like the ramp leg), ±1-count tremor
   for 2 s: dial OFF reproduces the debt (integrated righting < ~5 deg, gate stays
   < 0.05); dial ON restores righting (righting_frac > 0, roll_right_peak ≥
   inverted_rate − 0.5, gate reaches > 0.99). Print both numbers.
2. **The real sweep still vetoes** — the existing "loop apex — hand moving, the
   wings stay put" leg green at the shipped dial (a 40 deg/s sweep is NOT a
   tremor). Also a SMALL slow sweep (e.g. 5 deg/s sustained) must still veto:
   `hand_gate_min < 0.05`. This is the gun-director leg — the window must not eat
   a small deliberate input.
3. **Frame-rate invariance** — the tremor leg's ON numbers agree within tolerance
   at 240/60/30/10 fps and through a hitch (integrate through `app::step_frame`).
4. **Off arm bit-identical** — existing hash pin green with the dial at 0;
   additionally a NEW pin: the apex-leg roll-demand hash at dial 0 equals a
   recorded pre-change value captured by the `git show HEAD:control/controller.cpp`
   method the file documents (LEG (i) comment). Record the hash in the test with
   the method in a comment.
5. **Ramp, not step** — existing leg stays green; the tremor arm also spends
   ticks strictly inside (0.05, 0.95).
6. **Hands off unchanged** — "hands OFF still rights him (the 08-06 ruling)" green.
7. **Loader** — `right_hand_rest`-style range pin for the new key, and the loader
   refuses a negative value.

Non-ASCII TEST_CASE names silently never run (`gate_baseline.py lint`). LF endings.

## §5 BUILD / GATE MECHANICS (this box)

- Fresh worktree: configure BOTH with the staged raylib or the configure stalls
  for hours:
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src`
  `cmake -B build-play -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src`
- Lane tests fast: `cmake --build build --target <test exe>` then
  `ctest --test-dir build -R "righthand|tremor|loop_rollover" --output-on-failure`.
  `rm` the test exe before a rebuild after editing (locked-exe stale relink trap).
- **Full gate takes ~1 h** and the harness memory guard kills background ctest:
  run it DETACHED from PowerShell (`Start-Process` of a script that builds ALL
  targets then `ctest --test-dir build -C Debug > build/gate_<sha>.log`, writing a
  `.status` file with the exit code at the end), then wait on the `.status` file.
  Verdict is quoted BY NAME from `python tools/gate/gate_baseline.py check`
  (baseline `generated/gate/known_reds.txt`, six known reds; a matching COUNT
  proves nothing — SOP §6.1). Use `py -3`/the Python311 exe if `python` is the
  bare venv.
- Format: clang-format with the repo `.clang-format` on files you touched only.
- Graph: `python tools/graph/graphify.py` in the SAME commit as any structural
  change; `--stale` to confirm.
- Stage paths, never `git add -A` (349 build files once).
- Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`
  is the session default; an Opus agent signs as itself.

## §6 DEFINITION OF DONE FOR THE OVERNIGHT RUN (not a landing)

1. Dial + mechanism built, legs §4 green, OFF arm hash-pinned, lane gate green.
2. Full gate on the lane tip: red set == baseline six BY NAME.
3. Fresh-context red-team WRITTEN under `docs/REDTEAM_20260916_tremor_netwindow.md`
   (two lenses: mechanism/mutation, and law/feel). Every P0/P1 folded and
   re-measured; re-gate after folds.
4. `build-play/seads.exe` rebuilt on the final tip (report its mtime).
5. `docs/SESSION_HANDOFF_20260916_tremor_netwindow.md`: status table, the dial,
   the sweep table, the red-team verdicts, a FLY CHECKLIST with the ABSOLUTE exe
   path `D:\seads_sandboxes\tremor\build-play\seads.exe` (fly: inverted rest with
   the hand resting on the mouse; a loop with a pause at the top; an Immelmann;
   a slow 5 deg/s lateral drift while belly-up must NOT right him), and the
   walk-back order (dial → 0 = v16).
6. `LANES.toml` `[lanes.kernel]` `in_flight` one line + `as_of`; everything
   committed; **lane branch pushed** (`git push -u origin feel/tremor-netwindow`).
   **MAIN IS NOT TOUCHED. NO TAG.** The sentinel is pinged only at landing time,
   by the session Chad runs after he flies.

## §7 KNIFE-EDGE RINGING (the other half of this rung — DIAGNOSIS ONLY tonight)

Separate worktree `D:\seads_sandboxes\tremor-knife`, branch `feel/knife-edge-diag`,
off the same main. Nose-down knife-edge: `az_lat = atan2(target_body.x·cos_phi_theta
+ target_body.y·sin(phi), −target_body.z)` picks up an `eps·sin(phi)` contamination
via `lean_gain 8`; predates v14. Instrument: `test/unit/test_yawbank_balance.cpp`.
Deliverable: `docs/KNIFE_EDGE_RINGING_DIAG_20260916.md` — reproduce offline
through `app::tick`, measure amplitude/period/decay of the ring vs bank angle and
speed, identify the exact term, propose ONE dial with 0 = bit-identical, and save
the probe as `docs/knife_edge_probes.patch` (precedent `docs/e8_redteam_probes.patch`).
**No kernel change on that branch.** Commit + push the branch. Chad rules on the
dial before it is built.
