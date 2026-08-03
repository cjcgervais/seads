# legacy-replay/ — pre-SOP-01 replay tooling

Everything in this directory predates SOP-01 (2026-08-02) and the M1
`[EvCTAPE]` per-frame recorder. Nothing here is deleted; it is grouped here
so a fresh agent does not mistake it for the current data path. Status per
file:

| File | Status | Why |
|---|---|---|
| `capture_server.ps1` | **DEAD** — do not run | Has no producer. Paired with `FlightRecorder`'s HTTP-POST path, which review struck down. See its own DEAD banner at the top of the file and `SOP-01-PRIMARY-DATA.md`'s incident report. |
| `INSTALL.md` | **DEAD** — do not follow | Instructions for wiring the same dead sink `capture_server.ps1` serves. See its own DEAD banner. |
| `REPLAY_ROADMAP.md` | **MIXED** — SEADS section live, EvC section superseded | Its SEADS/C++ roadmap (`seads_recorder_proposal/`) is unaffected by SOP-01 and still describes real, usable work. Its Roblox/EvC section describes extracting `BirdController`'s cascade into a pure module for headless replay — a real idea, but superseded as the *primary-data* answer by M1's `[EvCTAPE]` tape (PREFLIGHT-GATE.md), which needed one session instead of an extraction project. Read as a possible *future* replay layer, not as today's data path. |
| `replay_diff.py` | **LIVE** for the SEADS C++ kernel workflow (`README.md` Workflow A). Its Roblox/EvC workflow (Workflow B, `.jsonl` from `FlightRecorder`) is dead for the same reason as `capture_server.ps1` — there is nothing to diff because nothing was ever recorded. |
| `seads_recorder_proposal/` | **LIVE**, unaffected by SOP-01 — it records the SEADS C++ kernel, an entirely different codebase from the EvC2026/Roblox side this SOP governs. |

**Why this matters:** the 2026-08-02 incident that produced SOP-01 happened
in part because an agent named `capture_server.ps1` as a sink for a flight
without checking whether anything fed it. Grouping the dead material here,
physically apart from the live SOP-01 program at the harness root, is a
structural answer to that failure — not just a label.

Moving these files out of the harness root changes their path. Any
document elsewhere in the repo that still references
`D:/mandalark-kernel/harness/capture_server.ps1` (etc.) by the old path is
now stale; fixing those documents is outside this session's write scope
(`D:/mandalark-kernel/harness/` and `D:/mandalark-cascade-research/tools/`
only) and is called out explicitly in `../HARNESS-STATE.md` rather than
silently left wrong.
