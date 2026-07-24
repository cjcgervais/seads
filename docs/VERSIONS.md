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

| *(pending seal)* | `46051ca23` | **THE KERNEL RECONCILIATION** (2026-07-24): `flight-kernel-v5` merged into the tunnel game — tunnels, ballistics, Sudbury world, Bf 109 fleet, bandits all on v5. Gate 793/793, branch `sandbox/kernel-v5-reconcile` pushed. Merge findings: wheel brakes re-derived upward for the fighter engine; bandits inherit v5 energy retention (turn wide, not slow); escape-sky gravity numbers flagged for retune (level cruise can now exceed old dive-only escape speeds). Fly build: `D:\flight_sim2\seads-recon\build\seads.exe`, stamp `KERNEL v5 [reconcile]`. Seals into `main` on Chad's fly verdict. |

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
