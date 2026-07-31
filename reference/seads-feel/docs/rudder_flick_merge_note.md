# Merging `feel/rudder-flick` — verified-safe instructions (2026-07-10)

**Chad's ruling at session close:** only the cascade/control tuning merges; his freelook/zoom
view changes (the wheel dolly-out in mouse-aim zoom + scroll-back widening in freelook, tuned
by the map/world agent on `sandbox/world-sudbury`) must survive untouched.

## Verified overlap analysis (run 2026-07-10, base = `main` @ 0c7ae80f0)

`git diff --name-only main...feel/rudder-flick` ∩ `main...sandbox/world-sudbury` = **3 files**:

| File | Collision? | Resolution |
|---|---|---|
| `app/instructor_tick.h` | **NO** — different hunks (world: spawn alt param + celestial `tick_count`, top of file; feel: the `ci.aim_moved` line in the Input build) | git auto-merges cleanly |
| `CLAUDE.md` | textual (both appended near the same anchors) | UNION — keep BOTH sides' entries (feel added 2 seam clauses + 2 Learned lessons) |
| `SPEC.md` | textual (both appended §0 entries) | UNION — keep both (feel added S-dz-motion + S-yaw-magnet) |

**Everything else is disjoint by file.** In particular:
- `app/main.cpp`, `render/*`, `config/world.toml` — Chad's freelook/zoom view code — were
  **never touched** by the feel branch. The merge cannot clobber them.
- `config/controller.toml` — heavily tuned by feel — was **never touched** by the world
  branch, so the tuned dials land whole. (Its `[ui]`/freelook-orbit sections are unchanged
  from `main` on BOTH branches.)
- Goldens: only `test/golden/controller_golden.h` moved (feel side only; dated banners).

## What the feel branch delivers (13 flies, all Chad-verdicted or pre-fly-rejected honestly)

- Dials: `yaw_scale` 2.2 · `lean_gain` 6 · `auto_level rate` 4.5 · `deadzone` 0.05/0.08.
- **S-dz-motion** (SPEC §0): aim-motion-gated deadzone, `rest_dwell` 0.15 (A/B-flown, Chad
  chose gated). `rest_dwell = 0` = legacy bit-identical.
- **S-yaw-magnet** (SPEC §0): near-center coordination relief, `center_frac` 0.15 /
  `center_band` 1.5°. `center_frac = 1` = legacy bit-identical. Both red-teamed SOUND.
- Gate on the branch: **257/257** (247 inherited + 10 new legs). Tagged
  `golden-rudder-flick-fly13`.

## Merge procedure (whoever executes it)

1. `git merge feel/rudder-flick` into the target (`sandbox/world-sudbury` or `main`).
2. Expect conflicts ONLY in `CLAUDE.md` / `SPEC.md` → resolve by union (keep both sides).
3. Run the full gate on the merged tree. The feel tests self-arm (they do not depend on the
   shipped dial values beyond the loader pins) and the world tests are untouched by feel.
4. Fly-verify BOTH: a rudder flick near center (cascade tuning intact) AND the freelook
   wheel dolly / zoom scroll-back (view controls intact).
5. Note the world checkout's `.claude/gate_waiver` (the parked water-surface test) is a
   separate, world-thread matter — delete it only when that work lands green.
