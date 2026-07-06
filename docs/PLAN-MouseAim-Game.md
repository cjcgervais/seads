# SEADS 2026 — Mouse-Aim Game Build Plan (handoff)

> **Purpose.** This is the implementation plan for turning SEADS 2026 (the sealed deterministic
> ATM-Sphere kernel + 33 netcode layers) into a **playable mouse-aim dogfighting game**, by grafting
> the WT-style mouse-aim **instructor** (from the `D:\flight_sim2\seads` SPEC/SOLUTION docs) onto the
> authoritative kernel. Slice A (single-player flyable core) has **landed** (see below). This doc is
> the next agent's starting plan — refine it with Chad, then execute one gated slice at a time.
>
> **Governance.** Everything here is **client/net-side, downstream of the sealed kernel** — it produces
> `seads::Command`s that ride the existing INPUT-001 upstream wire (quantized ×1e6 ⇒ cross-client
> determinism preserved). It must NOT touch `src/kernel/**`, `src/det_math/**`, `config/rails/**`, the
> wire protocol, or any golden. Expect **all 15 goldens byte-identical, no seal, guardian.yml green**
> throughout. The real gate for the feel work is **Chad flying it** (the Human Delight Test).

---

## 0. The decision that framed this (adopt path = "graft")

The `flight_sim2/seads` SPEC is a feel-first ARCADE flight kernel whose premises are the *opposite* of
SEADS 2026's constitution (it BANS the `(lat,lon,heading)` state SEADS stores; uses `atan2/asin/sqrt`
freely which SEADS bans in the kernel; its gate is subjective feel, not a cross-toolchain `world_hash`).
So we do **NOT** replace SEADS's kernel. We adopt only its **control/camera/instructor layer** as a
**client-side command producer**:

```
mouse Δ → AimState (world-frame targetDir, parallel-transported) → client_frame (planesphere/local-up)
        → Instructor OUTER loop → Command{target_phi, target_g, throttle, fire}
        → INPUT-001 (×1e6) → SEALED kernel (authoritative)   → chase camera + reticle
```

The happy surprise: SEADS's `Command = {target_phi (bank), target_g (load factor), throttle, fire}`
sits at **exactly the abstraction level of the instructor's OUTER loop output**, so the instructor's
inner rate-PI + plant-inversion is **dropped** — the sealed kernel IS the inner loop (roll_rate slew
toward `target_phi`, `n_aero`/structural clamp on `target_g`). Because the point mass is coordinated
(β≡0, no independent yaw/elevator), steering *is* bank-to-turn — the instructor's natural core.

Kernel-derived mapping (from `kernel.cpp step`, do not re-derive without re-checking):
- `γ̇ = (g₀/V)(n·cos φ − cos γ)` ⇒ **trim `n_trim = cos γ / cos φ`** holds the flight path.
- `ψ̇ = (g₀/V)(n·sin φ / cos γ)` ⇒ heading turns by BANKING.
- SOLUTION's `cosΦθ` trim is for ITS plant; on THIS plant it would make every bank descend — use `n_trim`.

---

## 1. WHAT LANDED — Slice A (single-player flyable core) ✅

Files (all NEW unless noted), **uncommitted** on `main` as of this handoff:
- `src/client/client_frame.h` — planesphere/local-up adapter: reconstructs the world-Cartesian body
  frame (nose/right/body_up/local_up) from the kernel `(lat,lon,psi,phi,alt,gamma)` tuple, reusing
  `local_basis`/`geo_to_cartesian` conventions EXACTLY (one shared frame; a 2nd convention is a bug).
  Also Rodrigues `rotate_about` + `transport` (parallel transport on the sphere).
- `src/client/aim_state.h` — `AimState`: WT-style world-frame `targetDir` in a raw {fwd,up,right}
  frame; `carry()` parallel-transports every tick (every mode), `mouse(dx,dy)` rotates it zenith-free,
  `seed()`/`snap_to_nose()`. Nothing smoothed on mouse→aim.
- `src/client/instructor.h` — `instructor_step(aim, frame, phi, gamma, tuning, state) → Command`: the
  SOLUTION OUTER loop, adapted. Bank-to-turn (`target_phi = phi + bankErr`) + braking-law pull mapped
  to `target_g = n_trim + Δn`. Above-wing-line = bank+pull; below = wings-held PUSH (kernel clamps bank
  to `phi_max<90°` so there is no split-S-by-roll). `InstructorTuning` = all the feel knobs.
- `src/client/viewer_main.cpp` — MODIFIED `run_fly`: locked-cursor mouse → `AimState` → `instructor_step`
  → `Command` → the existing `predict::Predictor` (kernel driver). Floating reticle = aim projection.
  `main()` MODIFIED: `--fly` now auto-discovers a recording, else flies a BARE GLOBE (no arg needed).
- `fly.bat` — double-click launcher (runs `seads_viewer --fly` from repo root, keeps window open).

Verification done: builds+links clean into `seads_viewer`; **AT-0 sign check GREEN** (on-nose=hold,
right=+bank, left=−bank, up=+pull to n_max, down=push to n_min, up-right=banked pull — all correct
directions); headless selfcheck runs; **golden untouched by construction** (client-only). No CMake change.

Run it: double-click `fly.bat`, or `./build-client/seads_viewer.exe --fly` (bash: forward slashes!).

### Known caveats (the next agent must carry these forward)
1. **Gains are HOT.** A 15° error already commands ~max bank and ~8 g (the pull saturates the g-budget
   rate ceiling, small at 150 m/s). Signs right, magnitudes are Section-7 knobs: `k_theta`, `a_brake`,
   `n_max_adv`, `max_bank`, plus `AIM_SENS` in viewer_main. **Tune by flying — never autonomously.**
2. **Below-target = push, never roll-inverted** (bank clamp). The hysteretic push↔roll gate + a "commit
   to the loop" latch is deferred (Slice A4).
3. **Camera is still the OLD chase cam** (behind-heading + orbit freelook). The lagging/horizon-locked
   camera (Slice A1) is not done. Freelook currently just holds the aim (instructor keeps flying it).
4. **Keyboard override is the crude form** (direct manual command while held); the in-envelope nudge is
   Slice A2.
5. **Own ship carries NO weapons** in the single-process fly loop (no server to adjudicate fire). Guns
   for the flown ship arrive with the networked loop (Slice B4).

---

## 2. PHASE A — finish the single-player feel (the graft's remaining slices)

Do one at a time; each is client-only, gate stays green, judged by flying. Tag a build Chad's hands like.

- **A1 — Chase camera (lagging + horizon-locked + freelook).** New `src/client/chase_camera.{h,cpp}`.
  Port SPEC §9.2 (S7-cam/S7-cam2): a decoupled lagging chase that rests behind the **velocity** vector
  and eases toward the aim ∝ deflection (`ease_chase_forward`); camera-up **horizon-locked** (ease
  `local_up` projected ⟂ chase-forward, zenith-safe hold — `ease_level_up`); freelook = orbital offset
  on top, ease-back cosmetic. STRICTLY downstream of mouse→aim (never feeds `targetDir` — the RA9
  rubber-band ban). Replaces the current `chase_dist`/`look_az/el` block in `run_fly`.
- **A2 — Keyboard override → in-envelope nudge.** Port SPEC §9.5 S7-ovr/ovr2 adapted to the point mass:
  held W/S/A/D drive `target_g`/`target_phi` toward clamped in-envelope extremes; the MOUSE keeps the
  aim/reticle/camera (a keypress never moves the view); on release, pursuit resumes toward the unchanged
  mouse aim. (No escape-hatch needed — `n_aero` has final say.)
- **A3 — Tuning as data.** Lift `InstructorTuning` (+ `AIM_SENS`, camera lead/lag/level_rate) into a
  config file (e.g. `config/instructor.toml`, parsed once at viewer start — NOT across any kernel
  boundary, presentation-only). So the feel loop is edit-and-fly, not recompile. Add a live on-screen
  readout of the active knobs.
- **A4 — Push-vs-roll gate + curvature/deadzone polish.** Hysteretic below-target handling: push while
  the aim is ahead/near-vertical; a "commit to the loop" latch for a true split-S (pull over the top,
  since roll-inverted is bank-clamped). Add curvature-feedforward / deadzone-holds-trim refinements
  only if flying shows a limit cycle (the kernel already holds level for n=trim, so this may be a no-op).
- **A5 — Feel-tuning loop (Human Delight).** One knob per flight, hypothesis→fly→stars, tag good builds.
  Order: `AIM_SENS` → `k_theta`/`a_brake` (arrival) → `n_max_adv`/`max_bank` (authority) → camera feel.
  This is Chad-in-the-loop; the agent instruments, never gates on numbers.
- **A6 — Reticle/HUD polish.** Nose marker + aim reticle + the error gap; optional 5/8 screen clamp and
  self-centering pull-back (SPEC Item 4); a non-linear sensitivity curve (SPEC addendum §A.3 — worth a
  spike for "connectedness"). Aim-error / commanded-(phi,g) HUD readouts.

**Phase-A exit:** mouse-aim dogfight flying on the SEADS sphere feels good in the single-process viewer.

---

## 3. PHASE B — make it a networked game (the end-to-end playable loop)

This is where the graft meets the 33 netcode layers — the highest-impact direction flagged all along.
Today the flown ship drives a **local** `predict::Predictor`; wire it to an **authoritative server** so
the netcode actually drives the game.

- **B1 — Own ship upstreams to an authoritative server.** Encode each instructor `Command` as INPUT-001
  and send it UP to a running server (`broadcast_input`, or `broadcast_bound`/`broadcast_auth*` for
  seat binding). The kernel on the server is authoritative; the client no longer free-runs. Reuse the
  layer-15b/16 upstream path — the Commands are already the right shape.
- **B2 — Client-side prediction + reconcile.** Reuse `netpredict::run_predictive_client`
  (`src/net/inputclient.{h,cpp}`, layer 17): predict own ship from the same Commands, reconcile against
  the authoritative frames. Seamless canonical / bounded wire (the round-trip theorem already proven).
- **B3 — Multi-client seat binding + remotes.** BIND-001 seat (layer 18/21) so each player flies their
  own aircraft; draw remotes with the existing INTERP/PREDICT(24)/SMOOTH(25) modes already on the HUD.
  A local integration harness (two in-process clients against one `broadcast_bound` loop) before sockets.
- **B4 — Own-ship guns in the loop.** With an authoritative server, the fire `Command` goes up and the
  kernel adjudicates hits → WEAPON-001 comes back → the flown ship gets hitpoints/ammo/kills and can
  actually fight (the single-process caveat #5 dissolves).

**Phase-B exit:** two people (or one + a scripted bandit) fly against each other over the authoritative
input server, predicted/reconciled, with guns — a real dogfight.

---

## 4. PHASE C — game/product polish

- **C1 — HUD for the networked state:** assigned seat / auth state, predicted-vs-authoritative
  correction magnitude, catch-up-in-progress, an own-ship kill-feed (the event journal already exists).
- **C2 — Match flow:** spawn/respawn, crash-and-reset (SPEC §6.3 — `altitude ≤ 0` → clean rebirth),
  simple round/scoreboard flow (the WEAPON-001 scoreboard is already drawn for remotes).
- **C3 — Felt curvature + repeatability:** horizon-bulge / ground-track cues so the *sphere* is
  perceptible mid-fight (SPEC addendum §A.3 #2); a fixed human test-card (SPEC addendum §A.3 #1) so feel
  scores are comparable across tuning iterations; a target-drone / ghost for honest tracking scoring.

---

## 5. Open questions for the next agent to settle with Chad

1. **Keep the single-process `--fly` path AND add networked, or replace it?** (Recommend: keep it as the
   feel-tuning sandbox — Phase A — and add a `--net` client for Phase B, sharing the AimState/Instructor.)
2. **Tuning config format** (A3) — TOML like flight_sim2, or a simple key=value the viewer parses?
3. **How hot should the default feel be?** (A5 is human-gated; the agent proposes, Chad rules.)
4. **Server topology for B1** — reuse the existing `seads_netserver` demo binary, or a new game-server
   entry point? Which auth tier (bare `broadcast_input` vs `broadcast_auth` seat-by-identity)?

---

## 6. First moves for a cold-start agent

1. Read this doc + `SPEC.md`/`SOLUTION_instructor.md` in `D:\flight_sim2\seads` (the feel reference) +
   `CLAUDE.md` §1 rails + the three new `src/client/*.h` headers.
2. Build + fly the current core: double-click `fly.bat` (or `./build-client/seads_viewer.exe --fly`),
   fly it, and write down what the mouse *feels* like — that sets the A5 tuning agenda.
3. Pick up **Slice A1 (chase camera)** — the biggest felt gap after gains — or jump to A5 tuning if the
   gains are the dominant complaint. Confirm the golden/guardian gate is untouched (client-only).
4. Keep this plan current; leave git clean per the constitution (commit only when Chad asks).
