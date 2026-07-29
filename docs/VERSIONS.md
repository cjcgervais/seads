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
1.68 s), ~45 s of overrides, no raw-mode ticks, clean exit flush. Awaiting harness-agent
signing + mirror into `goldens/` — that closes the v6 books. Until then a byte-verified
**safety copy is pushed at `tools/safety/felt_flight_2.seadsrec`** (SHA-256 in
`tools/safety/README.md`; delete it once the signed authoritative copy is on a remote).
**Provenance flag for signing (do NOT edit the header in place — the fnv1a signature
covers it; the correction belongs in the signing metadata):** the file header says
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
