# Kernel Version Ledger

Every sealed kernel generation, across all repos. Each is an annotated git tag pushed to
its remote — permanent, named, and recoverable even if branches move or get rebased.
When a new version is sealed anywhere, add it here.

## Generation 1 — EvC2026 Roblox testbed (Luau)
Repo: `D:\EvC2026` → github.com/cjcgervais/EvC2026

| Tag | What it seals |
|---|---|
| `v1.0-eagle-flight` | first flyable eagle kernel |
| `v1.1-eagle-flight-feel` | feel pass |
| `v1.2-free-cursor-flight-kernel` | free-cursor mouse-aim kernel (ancestor of the instructor cascade) |

## Generation 2 — SEADS_2026 (C++, spherical physics + first mouse-aim graft)
Repo: `D:\SEADS_2026` → github.com/cjcgervais/seads

| Tag | Commit | What it seals |
|---|---|---|
| `mouse-aim-sliceA` | `83f3043` | mouse-aim instructor graft Slice A, single-player flyable core (branch `feat/mouse-aim-instructor`, first pushed 2026-07-23) |

## Generation 3 — seads-feel (C++, current authority)
Repo: `D:\flight_sim2\seads-feel` → origin (see that repo's remote)

| Tag | Commit | What it seals |
|---|---|---|
| `sealed-kernel-v3` | — | v3 seal (pre-existing) |
| `flight-kernel-v4` | `d68de7d91` | V4 APPROVED — Chad's stick verdict + kernel-defining schedule; current `main` |
| `kernel-v5-rung-a` | `dc0b1d01c` | S-truedepth: momentum-earned glance depth (depth_frac 1.5) |
| `kernel-v5-rung-a2` | `c8e98afa3` | sub-wall curve (depth_pow 2.0) — "a little more" on small adjustments |
| `kernel-v5-rung-c` | `f523177e9` | HOLD THE LINE — sag servo pull_floor 1.0, K_aoa 10; nose holds the mouse line to honest stall |
| `kernel-v5-rung-d` | `c0625ede1` | arcade energy model — k_induced 0.015, T_max 18000, n_max 32; "give me the power" |
| `kernel-v5-rung-e` | `89447aba5` | push knife-edge 45° + off-screen aim arrow; nose-down needs deflection AND real down input |
| `kernel-v5-recorder-graft` | `d1e7dbe6b` | felt-flight recorder graft, reviewed SOUND-WITH-ONE-FIX, gate 372/372 |
| **`flight-kernel-v5`** | **`149a99c40`** | **THE SEAL (Chad, 2026-07-23/24: "okay perfect... committed glued screwed and tattooed and sealed as the v5 flight kernel ready to bring on to the main game"). Rungs A/A2/C/D/E + recorder + Bf 109 rig + aim-buddy ghost (fear = split-S proximity instrument, skin system = future cosmetics economy). Gate 372/372. Tag pushed to seads_sandbox1 origin. NEXT: the kernel reconciliation into the tunnel tree.** |
| **`game-kernel-v5`** | **`36ee936e9`** | **MAIN. The full game (tunnels, ballistics, Sudbury, Bf 109) on kernel v5 — reconciliation merged to main by Chad's word 2026-07-24 after "this is the best, it's better than all the rest!" (landing + 2 kills + the rung-F sacred-middle dive). Gate 797/797. Rungs F + ghost-brace + F9 recording w/ flush-on-exit included.** |
| **`flight-kernel-v6-2026-07-28`** | **`cfe1bd7fe`** | **THE V6 SEAL (Chad, 2026-07-28: "update the kernel to v6 with a date... it's getting very near the point I don't touch it again for a while" — after a 3-for-3 approval session, "yes perfect as expected 3/3!"). Over v5: S-relorient (freelook release = instant chase-behind), yaw_scale 2.2→2.0 (rudder-bias trim), inverted_delay 1.0→0.5 s (auto-right quickening) — all three flown-approved same day. Gate 380/380, tree clean. Tag + branch backup pushed same day per convention.** |

| **`flight-kernel-v7-2026-07-29`** | **`51eb5b9e3`** | **THE V7 SEAL (Chad, 2026-07-29: "now it is precisely perfect… it's the right set up for my camera now"). The freelook-release camera, finished. Three commits over v6: `35e31695f` S-relorient ADDENDUM (retire the D9 exception — a release with override keys held now fires the full orient verb), `13631ba92` red-team folds (no P0/P1), `5ea20d8c7` S-keychase (while keys fly and freelook is not held, the chase camera's rest target becomes the flight path instead of the parked aim, caught at 6.0 /s). Gate 388/388, zero moved goldens, tree clean. Both mechanisms are optional-with-default-off knobs (`[freelook] release_orient_with_keys`, `[camera] key_anchor_rate`), so v6 is reachable one line at a time. Tag + branch pushed to origin same day.** |

| **`flight-kernel-v8-2026-07-29`** | **`ae7ae8f23`** | **THE V8 SEAL — S-keyprec: override keys have NO camera authority. Retires S-keychase (v7) **in full**, by excision rather than by knob. Chad, flying v7: "if I input some aileron to cut into their path sooner, I get a disorienting snap to the chase cam which throws off my aim" — rule: "only the precedence of the freelook push shall do that." One camera automation remains (the freelook-release snap, untouched); the camera is aim-bound at all other times. Gate 387/387 (−2 retired cases, +1 new leg), zero moved goldens, red-team clean (no P0/P1, gate and comfort numbers independently reproduced). Tag + branch + `sandbox/s-keychase-retired` pushed. ⚠ **SEALED BUT NOT FLOWN** — the first kernel seal made before Chad's stick verdict; `docs/v8_fly_cards.md` is outstanding. ⚠ **Walk-back is a REVERT, not a dial** (`sandbox/s-keychase-retired`, or the v7 tag) — deliberate, because the ruling is categorical and a knob at zero is a loaded gun.** |

**Notes for `flight-kernel-v8-2026-07-29`.** Measured v7 → v8: `comfort mouseaim_keys`
lag-behind-aim with keys down **67.781° → 10.492°**; max per-tick step at the keypress edge
3.222° → 0.378°; `comfort turnsteady_keys` standing oblique **0.000° → 16.341°** (the accepted,
Chad-ruled drift — the camera showing where he points; the cure is to move the mouse).
⚠ **Do not cite `max_step` as the headline** — it is bounded by construction (v7's swap eased at
`key_anchor_rate·dt = 6.0/120 ≈ 2.9°/tick`, so the peak saturates there however bad it feels);
the evidence is the before-vs-during divergence. The v8 residual 10.492° is ordinary chase lag
against a faster-swinging aim, not an anchor effect.
**Process worth preserving:** the new `comfort_mouseaim_keys` scenario — the first to model
mouse-aim *and* keys simultaneously — was built and shown **failing on v7 before anything
changed.** That ordering was mandatory because this is the **third** camera mechanism validated
against a case Chad does not fly (cf. S-aimclamp, S-retclamp); the generalizable rule is that a
feel mechanism's instrument must model **both hands at once**. The red-team's one finding was an
evasion no test can close (re-adding key→camera coupling as a defaulted parameter wired only from
`main.cpp` passes the whole suite, since no ctest runs `seads.exe`) — recorded as a **review bar**
at the call site and in `SPEC.md` §0 instead of as prose. Full reasoning:
`docs/DECISIONS.md` and `docs/cascade/camera-anchor-mode-duality.md`.
⚠ `reference/seads-feel/` deliberately **not** re-snapshotted at v8 — sealed but unflown, and the
walk-back is a revert, so a snapshot could enshrine code that gets reverted. It stays at the v7
seal until Chad flies the cards.

Reconciliation notes for `game-kernel-v5`: reconcile merge was `46051ca23` (gate 793/793),
fly-round fixes through `36ee936e9`. Brakes approved — "good enough for the field in front
of your house when you were five." First landing ever put down on Sudbury clay (the v1
constitution said "no landing"). **Golden Felt Flight #1** flown on this build: 3m03.6s,
22,032 ticks, signed, `goldens/golden_1_first_v5_flight.seadsrec`. Open items queued:
replay-diff as a ctest leg (golden #1 guards automatically), taxi prop-strike (suspect
brake pitch-down), escape-sky ruling, ring-size reconciliation.

Graft note for `flight-kernel-v6-2026-07-28`: all seven post-v5-seal commits
cherry-picked into the seads-recon conquest tree (`sandbox/kernel-v5-reconcile`, tip
`5e27f237c`, gate 887/887) the same day — one fixup only (recon's `app::tick` takes a
non-defaulted `sim::Environment*` seam argument; the grafted test's calls pass `nullptr`
like the sibling legs). The cherry-picked controller golden passed **without a local
re-record** — the null-Environment seam is bit-identical across the two kernel lines, so
seads-feel's golden values transferred exactly. The conquest tree flies the identical
sealed v6 kernel.

**Golden Felt Flight #2 — FLOWN 2026-07-28** on the grafted v6 build
(`D:\flight_sim2\seads-recon\build-play\felt_flight_2.seadsrec`): 2.76 min / 19,844
ticks, full-marks coverage of all three v6 mechanisms — rebalanced rudder (mouse active
31% of ticks, 144–282 m/s), 20 freelook releases all via the new auto-orient (zero
double-taps), 13 inverted episodes exercising the 0.5 s auto-right (longest dwell
1.68 s), ~45 s of overrides, no raw-mode ticks, clean exit flush.

✅ **SEALED 2026-07-29 — the v6 books are closed.** Mirrored into `goldens/` as
`golden_2_v6_seal_flight.seadsrec` with `golden_2_TELEMETRY.md`, `golden_2_VERDICT.md` and
`golden_2_telemetry.csv` (10 Hz, 1,654 rows). Signature recomputed and matched
(`13318769238472783059`); SHA-256 verified identical across all three copies. The
`tools/safety/` duplicate has been deleted per its own supersession rule, in the same commit
that added the authoritative copy. Derived numbers: 165.4 s, V min 144.2 / mean 245.0 / max
282.0 m/s, alt 114.7–2497.8 m, **peak 32.1 g sitting exactly on `n_max = 32.0`** (aerodynamic
ceiling, not a contact spike — this flight never touches the ground, unlike golden #1's
50.6 g landing jolt), 20 freelook releases with **0** double-taps.
⚠ **Inversion predicate, needed to reproduce the counts:** `dot(body_up, local_up) < -0.5`
with a ≥0.25 s dwell → 13 episodes / longest 1.68 s, matching the flight-log exactly. The
naive `dot < 0` with no dwell gives 25 / 5.74 s (12 sub-0.03 s knife-edge sign flips).
**Provenance flag, now discharged into the signing metadata** (`golden_2_VERDICT.md`) rather
than by editing the header — do NOT edit the header in place, the fnv1a signature covers it:
the file header says
`tag=v5-reconcile@46051ca23`, a hardcoded `kFeltFlightVersionTag` constant in `main.cpp`
that doesn't track the build. Truth: kernel surface = `5e27f237c` exact (sealed v6
content); app layer = tip + uncommitted conquest WIP; binary = `build-play`
RelWithDebInfo, rebuilt night of flight.

**`kFeltFlightVersionTag` — CLOSED 2026-07-29** (`400f133ec`, seads-recon). The hardcoded
constant is gone; `cmake/build_info.cmake` now generates `app/build_info.gen.h` into the
build tree from `git describe --tags --always --dirty` plus the branch, run as a script from
an ALL target on **every build** (deliberately not at configure time, which would go stale
between configures). Recordings now track the build.

⚠ **Residual provenance gap — the header still will not name the kernel seal.** Verified
2026-07-29: in the seads-recon tree `git describe` returns
**`game-kernel-v5-24-g400f133ec-dirty`**. The `flight-kernel-v7-2026-07-29` tag *exists* in
recon as a ref, but it is **not reachable from recon's HEAD** — the kernel arrives by
**cherry-pick, not merge** — so `describe` resolves to the nearest reachable tag, which is
the *game* seal `game-kernel-v5`. A future reader of a Golden #3 header would see
"game-kernel-v5" on a build that actually flies v7 kernel content, and would have to know
that "+24 commits" spans two kernel seals. This is strictly better than the old hardcoded
lie (it names a real, reachable commit) but it is **not self-describing about the kernel
generation**. Durable fix: carry an explicit kernel-seal field in the build info rather than
inferring it from `describe`; until then the kernel identity must be recorded **by hand in
this ledger** beside each golden, as the v6 entry above already does.

⚠ **Decision needed before Golden #3 is flown.** As of 2026-07-29 the recon tree carries
**20+ uncommitted files** — audio synth (`render/audio_dsp.h`, `wind_synth.h`,
`engine_synth.h` deleted), conquest, drone, scenario config, rig, draw, plus the parked
tourist-map test — i.e. several agents' in-flight work, not just the tourist map. A golden
flown now stamps `-dirty` against a tree state that **exists in no commit and can never be
reconstructed.** For an ordinary recording that is merely untidy; for a **golden** it defeats
the purpose, because a golden is a locked reference meant to be re-derivable and diffable —
if a future build ever disagrees with it, nobody could tell whether the kernel drifted or the
unrecorded WIP did. **Recommendation: commit or stash the in-flight work before flying #3.**
That work belongs to other agents, so the call is Chad's.

**Scope note for `flight-kernel-v7-2026-07-29`, carried in the tag annotation and repeated
here so it cannot be lost:** v7 fixes the **keyboard-flown** oblique. The **mouse**
sustained-turn figure (`COMFORT turnsteady standing_oblique_deg 95.82`, `converged 0`) is
**untouched and not addressable by S-keychase** — that scenario holds no override key, so
`chase_anchor` returns the unchanged dials by the same bit-identical property that makes the
fix safe. The 95.8° was mis-attributed as the motivating number during the session; the real
measured magnitude of Chad's symptom was `turnsteady_keys_off` = **16.34°**.

**Both of the tag's open items were closed by Chad the same day, after sealing** (see
`docs/DECISIONS.md`, 2026-07-29): (1) letting the keys **carry the aim** is **RULED
AGAINST** — "not letting the keys carry their aim is precisely the point"; the keys move the
airframe *without* taking the aim, which is what buys aim freedom for deflection shots while
pulling hard, and carrying it would collapse the freelook/mouse-aim mode duality. (2) The
95.82° mouse figure is therefore **very likely a feature, not a defect** — that scenario is
the forward-looking oblique deflection geometry Chad wants; the instrument's `converged`
predicate is mode-blind and should not be read as a verdict on it. The seal's scope note
stands as written; what changed is the interpretation, not the content.

**Tag-history note (provenance, `flight-kernel-v7-2026-07-29`):** this tag was created on
`0f5bab51e`, then **moved** to `51eb5b9e3` minutes later and re-pushed. Reason: a
find/replace in `docs/flight-log.md` matched 17 rows instead of 2 and falsely marked 15
unrelated *pending* rows as Chad-approved; `51eb5b9e3` is the correction. Verified
independently by row count — 5 approved rows before the bad edit, 22 at `0f5bab51e`, **7**
at the seal (the 5 pre-existing plus the 2 genuine v7 approvals). The corruption never
reached the sealed artifact. This is a deliberate, one-time exception to "never move a seal"
below, taken because the alternative was sealing falsified stick verdicts; recorded here
rather than quietly, since a future reader doing forensics would otherwise find two targets
for one tag and no explanation.

| **`flight-kernel-v8-2026-07-29`** | **`ae7ae8f23`** | **THE V8 SEAL — the first PRE-FLY seal, and PARTIALLY REJECTED on the stick.** S-keyprec: override keys are camera-inert (S-keychase retired). Chad KEPT the keys half ("the most important part") but the freelook release regressed to oblique ("we threw out the baby with the bathwater"). Never grafted, never snapshotted into `reference/`. Its keep survives inside v9. |
| **`flight-kernel-v9-2026-07-29`** | **`29787debc`** | **THE V9 SEAL — S-nosesnap, FLOWN AND APPROVED same day ("v9 is golden and has a golden flight"). The four-round camera arc CLOSED.** Freelook welds aim := nose unconditionally (the no-keys carve retired by ruling); release = ONE instant snap — behind the nose, upright to the horizon, aim/nose in view, no ease ("Snap to view upon release of freelook, no eased anything"). CQ2 0.30 s easeback KEPT by ruling with a new cockpit rationale. Gate 388/388, zero moved goldens, red-team SOUND-WITH-FIXES no P0. Measured v8→v9: nose_at_fire 18.552°→0.578°, updebt_after_release 44.904°→0.003°, no-keys drift 75.5°→1.46°. Grafted to seads-recon `6058329d3`+`4d3e848bc` (gate 901/901, comfort numbers bit-identical). Rulings ledger: `docs/DECISIONS.md`, five 2026-07-29 entries. |
| **`flight-kernel-v10-2026-07-30`** | **`f86ee7b9f`** | **THE V10 SEAL — the buttery-cascade session (docs tip `2be93007c`, gate 391/391).** Three dials: `lean_gain` 6→8 (rung 1, flown partial, kept); `side_cone` 27.5/32.5→37.5/42.5 (rung F lineage heal — the sealed table had diverged from the flown one); **`[capture] carry` 1.0→0.0 — the pool-ball capture machine RETIRED-PARKED by Chad's ruling** (verbatim + re-entry condition in DECISIONS.md; machinery kept behind 28 self-armed test legs, silent re-arm fails the loader pin loud). Flown state: Chad flew these exact values as recon's logged flip and approved the butter on the stick ("it really feels better… the pool ball feel is gone") before ruling the seal. Two supersessions ledgered (Fly-13 crabbing preference; the cue-ball founding ask) + the compensation-decay law with its first predictive use (A6M2 re-arm note). Parked: `lean_max` 40 (shelf refuted). Queued mechanism threads: 5–10° blend-boundary roll slam, horizon-gate nose-referenced arm. **Golden Felt Flight #4 PENDING** — awaits the recon graft's green word; smoothness predicate to be recovered from the sealed tape's pins (sick-state reference 6.24/3.27 reversals/s rudder/elevator); pitch-down set = the horizon-gate BEFORE-state, documented-bad not blessed. |

| **`flight-kernel-v11-2026-07-30`** | **`0602d8292`** | **THE V11 SEAL — S-ROLLMIX: blend-band roll target continuity (the 5–10° slam thread CLOSED).** Tag verified by this agent (annotated `20f14817c` → commit, pushed; ls-remote confirmed). One new dial: `[regime] roll_target_mix = 1.0` (0.0 = bit-identical v10 kill-switch AND the Golden-#4 baseline arm) — in the blend band the MANEUVER roll limb chases `blend·commit + (1−blend)·live-lean-target`, the coverage-completion of MB-lean; pitch/yaw already carried the same continuity treatment. Plus the INSTRUMENT (telem.blend/held_bank + recorder v2, the §6.5 pin #2 — all four canonical sealed goldens sig-verify under the new reader) and SPEC First Principle 5, P-helm (the mouse-helm comfort doctrine, Chad verbatim). Gate 395/395 (reconciliation on the fly card); controller golden deliberately re-recorded under the pre-stated procedure (first divergent tick 289, blend 0.9899). Flown verdict: **"it definately feels smooth… buttery… there isnt rebounding"** — the slam is GONE; the flick dip observed same fly was A/B'd to three decimals as pre-existing v10 character, then RULED A FLAW anyway (superseding the 2026-07-06 roll-first ruling — 4th compensation-decay instance) → thread **S-straightline** opened with the COMS-1 stake pinned both directions. Grafted to recon `01b28231a` (gate 905/905, recorder-v2 ported through the TickHook seam — future F9 tapes carry blend/held_bank natively); build-play re-stamped. Full pipeline on the mandalark ledger: consult `45f9737` → audit `e606070` → pre-fly card review → verdict + A/B closure → seal. |

| **`flight-kernel-v12-2026-07-30`** | **`e362df289`** | **THE V12 SEAL — S-STRAIGHTLINE: the tracer-line dip closed by axis correction. Chad: "This is now the baseline for a quality flight kernel."** (His "this v11 is the standard" = the build in hand; ledgered numbering note.) One dial: `[regime] line_hold_ff = 1.0` (0.0 = bit-identical v11) — cancels only the crab's PARASITIC vertical component (emitted-yaw keyed, four continuous gates, AoA/G-envelope bounded). The thread's story is the discipline's showcase: the approved gravity-deficit shape was killed by its own commit-1 instrument at 2–4% measured closure BEFORE any fly (attribution: the crab digs the nose at sin(bank)·yaw); the corrected all-cancel v2 was then killed by a test leg's premise sweep (−G bunt on elevated aims) before the red-team. Closures 2.23/5.05/8.31° → 0.92/1.08/4.50° (60° residual = the honest G envelope, the fly card's headline G-bite row — approved). Gate 404/404 pre-registered exact. **COMS-1 truth-check CLEARED on the stick** — "your tracers go where you pointed them" is literally true (rendering wording approval still open). Known limits ledgered, not blessed: ~1.3° endgame droop, >75° top-rudder window, push-branch kink. Grafted to recon `2116f6ea3` (914/914 = pre-registered 905+9; Environment* seam adapt documented); build-play re-stamped. ⚠ Tag is LIGHTWEIGHT (v6–v11 were annotated) — noted, re-tag at leisure. **GOLDEN #5 ORDERED** — first golden with native telem_blend/held_bank pins; candidate dip-depth predicate from nose-elevation pins. |
| `kernel-v13` | `7ed1629e0` | 2026-08-06 — the automatic-comfort round (four pilot rulings: freelook easeback RETIRED, double-tap orient RETIRED, inverted righting with NO added delay `inverted_delay` 0.5→0.0, NEW rest-edge camera horizon recovery `[horizon_recovery] rest_dwell` 0.15 s). Gate 955/955. Naming convention changed here: `kernel-vN[-name]-signed` annotated tags at the pushed `main` tip. |
| `kernel-v13g-signed` | (fly-7) | 2026-08-06 — "that flew well": `rest_dwell` 0.10→0.05, `straight_max` 6→9 °/s, `path_band` 10→15°, NEW `ease_in` 500 °/s². Gate 958/958. (`kernel-v13f-signed` = the ai-primary-data handoff, not a kernel change.) |
| `kernel-v14-leanlead-signed` | `85896a73a` | 2026-09-10/11 — S-LEANLEAD, ONE dial `[auto_level] lean_lead` 0→0.3 (bank arrives WITH the rudder on a fine turn entry; Chad: "better than main, more intuitive"). Red-team P0 folded; controller golden re-recorded. Gate 6/2015 == the baseline six by name. Landed through the sentinel protocol — the precedent for a frozen kernel landing by the book. |
| **`kernel-v15-righthand-signed`** | **`b697d24a5`** | **2026-09-13 — S-RIGHTHAND, ONE dial `[auto_level] right_hand_rest` 0→0.25 s: a hand-motion veto on MB-right, so a held loop no longer rolls out at the apex where `unfold_bank` flips ±180° at `cosΦθ = 0`. Chad: "I can do the vertical loops and immelmans without a hitch" / land word "yes land the loop fix on main". Also carried, declared: `lean_lead_lateral`. Gate on `4b59e9753` = 6 of 2060 == the baseline six by name; red-team P1 folded. Flies on `main` and on the `seads-recon` fly tree (`29e3b53de`, exe 2026-09-13 10:39). `reference/seads-feel/` re-snapshotted here 2026-09-15 (four stated purity exceptions).** |
| **`kernel-v16-yawbudget-signed`** | **`354f6df3a`** | **2026-09-15 — S-YAWBUDGET, ONE dial `[coordination] yaw_vert_budget` 1.0 (gap −5..10°, floor 1 °/s): the rudder may spend at most the elevator's spare vertical authority, so a large lateral aim no longer digs the nose at knife-edge. The unload dial built beside it was REMOVED at Chad's ruling "no deck save unload keep the yaw budget". Red-team LAND-WITH-FIX folded (`125328c05`). Gate on `b4fbea3d4` = 6 of 2101 == baseline six by name. Chad on the landing exe: "I didnt notice a difference, I can fly it fine"; earlier on the budget-only build: "I am actually pretty satisfied with the kernel now... only uncontrolled manouvering will crash you". Residual accepted and written. Landed through this repo's sentinel, second landing of the night after atmosphere AS-5. `reference/seads-feel/` re-snapshotted here.** |
| **`kernel-v17-tremor-signed`** | **`3a95a4d08`** | **2026-09-16 — S-TREMOR, ONE dial `[auto_level] hand_net_window` 0→0.20 s: a windowed NET hand-live measure (leaky integral of the aim rate over the window ÷ an identically-leaked LIVE-dt accumulator, feeding a continuous liveness `smoothstep(1, 3 °/s)` that scales the hand-rest clock's reset), so a ±1-count mouse tremor no longer blocks belly-up righting; 0 = the v16 tree bit-identically (three hash pins). Built, gated, red-teamed and folded by the OVERNIGHT autonomous run on Chad's word; both lenses found the same P0 (late veto from a rested hand — the normaliser must count LIVE time), folded `4a6248c80`. Gate on `4a6248c80` twice = 6 of 2116 == baseline six by name. Chad 22:44: "okay I flew the tremor tape, I think we are good I am satisfied, the pause at the top of a loop made it want to right it self and threw me off a bit, split s is good, and the belly up small movements kept me inverted for the most part". Named accepted exception: a sustained deliberate drift under ~1.3 °/s is not vetoed. Pure fast-forward `edfc60ee8`→`3a95a4d08` on the sentinel GO; recon `4b8c68698`, exe 22:50:12. `reference/seads-feel/` re-snapshotted here.** |
| `terrain-clip-t2-facet-signed` | `533c86409` | 2026-09-17 — TERRAIN-CLIP T2 (a LANE tag, not a kernel version, Chad: "its own lane tag is fine"): `[ground] facet_contact` 1.0 (0 = identity), the aircraft lands on the facet the eye sees; three sim files, control/ untouched. Gate on the merged tip 6 of 2137 by name. Chad: "terrain hills killed me about 4x so im not going in (at murray mine either), plane landing looks nice with wheels about 2 inches at the errington mine nearby landing strip"; Onaping apron NOT flown. Landed BY THE SENTINEL on Chad's word after he closed the lane session. |
| **`sled-kernel-v2-signed`** | **`4acac47a2`** | **2026-09-19 — SLED KERNEL v2 (not the flight kernel, which stays v17): five Chad-driven sled dials shipped as defaults, landed together at his ruling "yes tyo all 7 and all 3 of these reccomendations I concurr I want this all in a v2" — `traction_mu` 3.0, `track_lat_slip_shed` 1.4, `rolled_throttle_frac` 0.15 (sled.h), `[sled_comfort]` `right_assist_max_ms` 4.0, `right_stand_shift_frac` 0.5 (scenario.toml); env vars kept as overrides. Run 7 "yes very good"; "accept 1.4" on a known-red debt moving the wrong way; "1 re bar the test" on an obsolete pin. Landing red-team LAND-WITH-FIX folded; gate on the fold tip `3990cde63` 6 of 2152 == baseline six by name. `reference/seads-feel/` re-snapshotted here.** |

✅ **GOLDEN FELT FLIGHT #5 — FLOWN AND SEALED 2026-07-30 (late night)**, the v12-seal
flight, same night as the seal. Mirrored into `goldens/` as
`golden_5_v12_seal_flight.seadsrec` with `golden_5_TELEMETRY.md`, `golden_5_VERDICT.md`,
`golden_5_telemetry.csv` (10 Hz, 558 rows). fnv1a recomputed and matched
(`6046690173471929225`; implementation validated against sealed #4 first — this
session's raw-bytes recompute tripped the documented LF-normalization clause, the
procedure's second distinct catch). SHA-256 `2A9A01A1…67DCE9` verified source↔mirror.
55.8 s, alt 298–881 m, V 230–285: **FIRST golden with native blend/held_bank pins**
(recorder v2), and the debut of the **STRAIGHT-LINE PREDICATE** — committed near-level
U-recovered entries: 5, max parasitic dip **0.58°** (vs the v11 baseline 2.23–8.31°;
predicate bound < 1.0°, instrument stated in full). 4 inversion episodes (deepest
−0.994, deliberate aerobatics), 1 steep-down entry (recovered), blend/dwell baselines
laid (52.6/16.9/30.5% FINE/band/MANEUVER). Header `kernel=` string STALE (says v10;
build stamp `g2116f6ea3` = the v12 graft is the truth — Golden-#3 class, correction in
VERDICT). Identified from data over two candidates: `felt_flight_19` set aside as the
thin-air false start (THIRD of its class; sig-verified, SHA in VERDICT, unpromoted).
**#5 = fast/low/maneuver-dense straight-line** — #4 remains the butter golden (no
deliberate butter segment on this tape; stated, not glossed). None supersedes another.

✅ **GOLDEN FELT FLIGHT #4 — FLOWN AND SEALED 2026-07-30**, the v10-seal flight, same day
as the seal. Mirrored into `goldens/` as `golden_4_v10_seal_flight.seadsrec` with
`golden_4_TELEMETRY.md`, `golden_4_VERDICT.md`, `golden_4_telemetry.csv` (10 Hz, 1,140
rows). fnv1a recomputed and matched (`7215269534658975596` — NOTE recorder.h's fnv1a uses
VARIANT constants; a standard-FNV reimplementation mismatches every valid recording, see
the TELEMETRY note). SHA-256 `095C5692…5BC6DE` verified source↔mirror. 114.0 s: the
**buttery slow-tracking state** (smoothness predicate: body-rate full-reversal < 1.1/s
per axis in the stated slow-window instrument; measured 0.80/0.91/1.03), **both steep-down
entries HELD** (cpt ≥ +0.475 — Chad: "I couldn't produce a roll over. The knife edge
held"), and **one partial bank-over at t=55.7 s in the exact documented horizon-gate
geometry** (~22° above horizon → pitch-through → min cpt −0.554, 0.50 s) — the
before-state for that thread, captured incidentally and reconciled against Chad's verdict
(no full roll-over; a partial, matching his own Card-1 description). Identified from data
over two candidates (felt_flight_17 set aside, "no air", sig-verified, unpromoted).
**#4 = fast/clean/slow-tracking butter** — #1 slow/dirty/ground, #2 fast/clean/combat,
#3 mixed/combat; none supersedes another.

✅ **GOLDEN FELT FLIGHT #3 — FLOWN AND SEALED 2026-07-29**, the v9-seal flight, same day as
the seal and Chad's stick approval. Mirrored into `goldens/` as
`golden_3_v9_seal_flight.seadsrec` with `golden_3_TELEMETRY.md`, `golden_3_VERDICT.md` and
`golden_3_telemetry.csv` (10 Hz, 1,460 rows). fnv1a signature recomputed and matched
(`12230860415588719877`); SHA-256 `3948B0F8…C2F4D9` verified source↔mirror. 145.9 s: two
bandits downed (Chad's word — not derivable from SimState pins), a crash landing, a respawn
teleport, **20 freelook releases all instant-auto-oriented — two of them while inverted**,
8 inversion episodes, aero g-peak 27.0 (teleport tick excluded by stated predicate).
**The header's `kernel=` string says v7 and is WRONG** — stale `KERNEL_SEAL` at build time,
fixed in recon `4d3e848bc` minutes after the flight; correction carried in
`golden_3_VERDICT.md` per the never-edit-a-header rule. The v7-era warning above (dirty
recon tree) was **discharged before this flight** by the `cdf65753d` tattoo commit — the
header carries no `-dirty`, so the golden is reconstructible from recon `966654e0f` exactly.
#1 = slow/dirty/ground, #2 = fast/clean/air, **#3 = mixed/combat** — none supersedes another.

Also pushed 2026-07-23: the full pre-existing seal ladder (`world-*`, `section-*-gate`,
`golden-*`, `game-R*`, `spec-freeze`, ...) — previously local-only, now on origin.

## Canonical repo — mandalark-kernel
Repo: `D:\mandalark-kernel` → github.com/cjcgervais/mandalark-kernel

| Tag | What it seals |
|---|---|
| `canonical-scaffold-v1` | scaffold complete: seads-feel snapshots @ `d1e7dbe6b`, rung ladder A–E, replay harness, cascade docs, nightly Drive backup |

## Sealing convention
- Annotated tags only (`git tag -a`), message carries Chad's ruling where one exists.
- Tags are pushed the day they're made: `git push origin --tags`.
- A "sealed" version is: tagged, pushed, and (for kernels) green on its gate at seal time.
- Never move or delete a seal; supersede it with a new one.
