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
sealed v6 kernel. Queued: a second golden felt flight (`.seadsrec`) flown on the sealed
v6 build as the resting-state baseline alongside Golden #1.

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
