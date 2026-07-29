# CLAUDE.md — repo guide

## What this repo is

`mandalark-kernel` is the **future canonical home** of Chad's flight kernel, plus its
knowledge base (the "instructor cascade" docs). It is not yet the live kernel — the live
kernels still live in their own repositories (see "Extraction plan" below). Today this repo
is: point-in-time reference snapshots of those live kernels, a structured knowledge library
explaining the flight-feel mechanisms at four levels, decision/tuning/feel logs, **golden
reference outputs under `goldens/` (this agent's, as of 2026-07-29)**, and (owned by a
separate agent, do not touch) a test harness under `harness/` and run captures under
`captures/`.

Chad is a non-coder game designer with strong flight-feel intuition who steers AI-driven
builds by flying and reporting what he feels, not by reading code. Everything in
`docs/cascade/` exists to translate between his feel-language and the actual mechanism, in
both directions.

## START HERE IF YOU ARE A FRESH SESSION

Read **`docs/SESSION_HANDOFF.md`** first. It carries the current state of play (repo tips,
kernel seals, what is in flight, what is blocked on whom), Chad's load-bearing statement of the
camera model, and the standing decisions that are easy to get wrong. `docs/DECISIONS.md` is the
authoritative reasoning record behind it.

## LIVE-BRANCH WATCH-ITEM — check branch state before trusting anything

The v4→v5 reconciliation is DONE: `main` in the game trees is `game-kernel-v5`
(`36ee936e9`, merged by Chad's word 2026-07-24, gate 797/797) — the whole game flies
kernel v5. The live risk is now the other direction: the feel branch **moves past the
seal**. `D:\flight_sim2\seads-feel`, branch **`feel/kernel-v5`**, is where new feel work
lands first. As of 2026-07-28 its tip `cfe1bd7fe` is sealed as
**`flight-kernel-v6-2026-07-28`** (tag + backup pushed, grafted into the seads-recon
conquest tree, re-snapshotted into `reference/seads-feel/` the same day) and Chad expects
a quiet period — but the discipline stands. Every session that touches this
repo's docs, tuning captures, or reference snapshots **must check that branch's state
first** (read-only `git log`/`git status` against `D:\flight_sim2\seads-feel` — never
write there) before treating any snapshot, dial value, or rung as current. A live tuning
session may move the branch at any time — mid-command, even. Dated files here are
snapshots of one moment, not a live feed. See `docs/DECISIONS.md`'s watch-item for the
full detail.

## The four-level cascade doc convention (`docs/cascade/`)

Every entry in `docs/cascade/` explains ONE flight-feel concept at four linked levels, in
this order:

1. **Feel** — pilot voice. What it feels like in the cockpit, ideally in Chad's own words
   where he's given them. No code, no math — this is the level Chad reads and writes.
2. **Principle** — instructor voice. The mechanism in plain engineering English: what rule
   governs the behavior and why it exists. Bridges Feel and Math.
3. **Math** — engineer voice. The actual formulas, thresholds, and control-law structure,
   named precisely enough that someone could reimplement it from this section alone.
4. **Code** — file/function/constant names in the reference snapshots (never line numbers —
   line numbers drift; cite symbols). Says explicitly which snapshot directory it's grounded
   in, and flags anything not yet reflected in that snapshot rather than inventing it.

When adding a new cascade entry, follow this shape. When a mechanism spans multiple kernels
(e.g. the mouse-aim cascade exists in both the EvC2026 Luau testbed and the seads-feel C++
kernel), ground Math/Code in the **current-authority** kernel and note the lineage/testbed
relationship explicitly — see `docs/cascade/mouse-aim-instructor-cascade.md`'s "Lineage"
section for the pattern.

## Sacred: tuning values and `goldens/`

**Never silently change a tuning constant or a golden reference value.** Every number in
`tuning/`, and every file under `goldens/`, represents either a live-kernel snapshot or a
locked reference output. If a value looks wrong, that's a question to raise (with Chad, or
in `docs/DECISIONS.md` as an open question) — not an invitation to "fix" it. Tuning docs in
this repo are captures, not control surfaces: editing `tuning/evc2026-v5-rungE.md` does not
change what Chad is flying.

**Sacred applies to this agent too, and hardest here** — `goldens/` is now this agent's
(below), which removes the "someone else owns it" backstop. An existing golden is
**append-only in practice**: never re-derive, re-record, re-sign, or "tidy" one. Add new
goldens; supersede, never overwrite.

## `goldens/` — THIS AGENT'S, as of 2026-07-29 (Chad's ruling)

Ownership moved here when this agent sealed Golden Felt Flight #2. Sealing a golden means
**all** of the following, in this order — the checklist is the job, not paperwork:

1. **Verify the recording's own fnv1a body signature** by recomputing it (scheme in
   `reference/seads-feel/test/harness/recorder.h`, `read_records`: hash every line except
   the `# sig fnv1a=` line, each with a trailing `\n`). A mismatch means STOP — do not seal.
2. **Verify SHA-256** against every other existing copy before moving or deleting any of them.
3. **Derive telemetry from the recording's own per-tick SimState pins only** — never by
   re-simulating. **State the derivation constants** in the TELEMETRY doc (`sim_dt`, `R`,
   `alt = |p| - R`, `climb = v·r̂`) so every number is reproducible.
4. **Reconcile derived counts against the flight-log**, and when they disagree, *recover the
   predicate rather than restate the number*. Golden #2's inversion count only reproduced at
   `dot(body_up, local_up) < -0.5` with a ≥0.25 s dwell; the naive `dot < 0` gave 25 instead
   of 13. **Write the predicate down** — an unstated definition reads as a regression later.
5. **Never edit a recording's header**, even when it is wrong. The fnv1a signature covers it.
   Provenance corrections belong in the `_VERDICT.md` signing metadata.
6. **Force-add the `.seadsrec`** (`git add -f`) — `*.seadsrec` is ignored by default so
   flights are promoted deliberately, never by accident.

Each golden is four files, following `golden_1_*`: the `.seadsrec`, `_telemetry.csv`
(10 Hz decimation), `_TELEMETRY.md` (numbers + conclusions + known limits), and `_VERDICT.md`
(Chad's verdict, purpose, provenance/signing record). Record the seal in `docs/VERSIONS.md`.

**Goldens pin different things and none supersedes another** — #1 is the slow/dirty/ground
case (landing, taxi, a tunnel run), #2 the fast/clean/air-combat case. Never retire one as
"covered by" a newer flight.

## `harness/`, `captures/`

Owned by a separate, parallel agent building the test harness. **Do not create, write, or
modify anything under these two directories** — including `harness/replay_diff.py`, which
this repo's goldens are the calibration input for. See `harness/README.md` for what they do.

## Extraction plan

The kernel does not live here yet. Live kernels, in generation order:

1. `D:\EvC2026` — Roblox/Luau mouse-aim "instructor cascade," a **prior-generation testbed**.
   Read-only to this repo. Actively tuned by a separate live session as "v5 / rung E" —
   though as of this repo's 2026-07-23 snapshot, the actual rung-E work is on the seads-feel
   branch below, not in EvC2026.
2. `D:\SEADS_2026` — C++ spherical-earth navigation/combat kernel (pure physics: great-circle
   nav, ballistics, hit detection). Read-only to this repo. No camera code; supplies the
   non-euclidean geometry half of the "spherical earth" cascade entry.
3. `D:\flight_sim2\seads-feel`, branch `feel/kernel-v5` — the **current-authority** C++
   flight-feel kernel: the instructor/controller, the capture-arrival servo, the push-gate,
   the camera, and the harness that measures all of it. Read-only to this repo. This is
   where Chad is actually flying and where new rungs land.

This repo becomes authoritative **gradually**: as mechanisms stabilize on `feel/kernel-v5`
and merge to its `main`, the plan is to extract them here as the single canonical kernel,
with `docs/cascade/` as the permanent knowledge layer above whatever the code becomes. Until
that extraction happens, treat every file under `reference/` as a snapshot, and treat the
live repos as ground truth.
