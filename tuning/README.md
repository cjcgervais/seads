# tuning/

This directory is the future home of the flight kernel's feel constants as commented data
files (the eventual successor to scattered `GameConfig.luau` tables / `*.toml` files spread
across three different repos). It doesn't do that yet — for now it holds **capture
documents**: point-in-time records of what constants exist, what they're set to, and what
they mean, pulled from the live/snapshotted sources so future tuning work has a paper trail
instead of having to re-derive it from git blame across three repositories.

- `evc2026-v5-rungE.md` — tuning constants from the EvC2026 Roblox/Luau kernel
  (`GameConfig.luau`, `BirdController.client.luau`), PLUS the real "v5 rung ladder" dial
  history from the current-authority C++ kernel (`D:\flight_sim2\seads-feel`,
  branch `feel/kernel-v5`) — despite the filename, the rung ladder itself lives in the C++
  kernel, not in EvC2026's `GameConfig.luau`. See the file's own header for the split.

When this repo's extraction plan (see root `CLAUDE.md`) actually moves the kernel here, this
directory becomes the live tuning data — until then, treat every value in here as **a
snapshot**, not a control surface. Changing a number in a file here does not change the
live kernel Chad is flying.

**Tuning values are sacred** (see `CLAUDE.md`): never silently "fix," round, or
"clean up" a constant recorded here. If a value looks wrong, that's a question for Chad or
for whoever owns the live branch — not an invitation to edit it.
