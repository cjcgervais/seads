# CONTRIBUTION SOP — working a lane against the SEADS monorepo

Written for an agent starting a fresh session on any lane. Every rule below was
bought with a real, dated failure — the citation is the reason the rule exists.
If a rule and the tree disagree, measure, then fix whichever is wrong.

**The map of who owns what is `LANES.toml` (source paths, branch, status,
in-flight) and `tools/gate/lane_map.toml` (test files). Read both before
touching anything you did not write.**

---

## 1. The nightly main gate — consume it, don't re-measure it

`tools/gate/nightly_main_gate.sh` gates `origin/main` in a dedicated worktree
(`D:\seads_sandboxes\_nightly_main`) and publishes a machine-written
`D:\seads_sandboxes\_nightly_main\STATUS`: main tip SHA, UTC timestamp,
tests passed/total, the red set with lane attribution, duration.

How a lane consumes it:
- **Before merging main**: read STATUS. Any red in your gate that is already in
  STATUS's set is main's ruled debt, not your regression. Any red NOT in it is
  yours to attribute.
- **Never trust a STATUS whose `result:` is not `OK`** — `GATE-DID-NOT-RUN`
  means exactly that. A gate that could not run never says "no reds"; treating
  an empty instrument as green is how the row-9 shadow red hid
  (PACKET_TO_ENEMY_AI_from_snow.md §2: GREEN on a stale tree, RED on a fresh
  checkout, same commit).
- **Check the `main:` SHA against `git rev-parse origin/main`** — a STATUS for
  an older tip is history, not a baseline.

*Why:* on 2026-08-30 every lane paid its own ~57-minute gate at merge time to
learn the identical fact, two lanes hand-typed the answer differently, and
reconciling them cost two more full gates. Run the fact once, publish it
machine-written.

## 2. Never hold uncommitted work as the only copy

End EVERY session committed and pushed to your sandbox branch, even mid-rung.
A half-done rung committed with an honest "WIP, does not build" message is
recoverable; an uncommitted working tree is invisible to every other lane and
mortal to a crash.

*Why:* on 2026-08-30 one lane's session held live edits in its working tree as
the only copy — which froze the file for everyone (the `test_snow_shadows.cpp`
fence exists because of exactly this), AND a second agent independently rebuilt
a fix that was already sitting uncommitted elsewhere. Duplicated work and a
blocked lane, both from the same uncommitted state. (As of 2026-08-30 the
enemy-ai worktree holds four uncommitted modified files — see LANES.toml —
which is this failure mode armed and waiting.)

## 3. Integrate daily, not at rung end

Merge `origin/main` into your sandbox branch (or land your finished slices to
review) at least daily. Small merges keep attribution mechanical.

*Why:* the 2026-08-30 merge was **47 commits deep** (`22e8508d9`). It was
conflict-free and provably inert — and it STILL cost a day, because at that
depth "whose red is whose" is archaeology, not a diff.

## 4. Escalation: report an OBSERVATION, never a DIAGNOSIS

When another lane's tests look implicated, publish ONE line: **test name,
commit, reproduction command.** Full stop. Do not name a cause in their code.
The owning lane runs the measurement; if you already ran one, attach the
measurement output, not your reading of it.

*Why:* on 2026-08-30 a lane published a theory naming another lane's kernel
edit (`roll_tq[]` 8→9) as the suspect BEFORE running the measurement that
exonerated it, and had to publish a retraction louder than the accusation
(PACKET_TO_ENEMY_AI_from_snow.md §0, commit `24c5ee823`). "The suspicion went
out at another lane's kernel before the measurement that would have killed it."
An observation cannot be wrong; a diagnosis can, and a wrong one costs the
other lane a bisect rung.

## 5. Shared files: announce, keep it small, land it fast

The `[shared]` list in LANES.toml (`gate.sh`, `.gitattributes`, `CLAUDE.md`,
`CMakeLists.txt`, `SPEC.md`, the gate tooling) has no single owner. To edit
one: **announce first** (a packet doc, or a line in your handoff naming the
file and the one-line intent), make the change **minimal**, and **commit+push
it same-session** — a shared file sitting in someone's uncommitted tree blocks
every lane at once (rule 2, squared).

*Why:* `.gitattributes` is the live example — one line (`* text=auto eol=lf`,
`01d9ce70e`) ended a test being green on one machine and red on another at the
same commit. Small, announced, landed fast: that is the template. The
counter-example is the gate itself pre-`a2f718da1`: a shared file that drifted
from reality (failing on ANY red, forever) until every lane learned to ignore
it — a disarmed tripwire nobody owned.

## 6. Definition of done — end of every session, in this order

1. **Red set matches the baseline**: `.claude/hooks/gate.sh` passes — i.e.
   `gate_baseline.py check` says the red set is EXACTLY
   `generated/gate/known_reds.txt`, member for member. A matching COUNT proves
   nothing: on 2026-08-30 two lanes both reported "six reds" for different six.
2. **Your own lane's tests are green** (your globs in `lane_map.toml`) unless a
   red of yours has been RULED a debt and recorded in the baseline. Never
   record a fresh red just to get green.
3. **Graph regenerated in the SAME commit** as any structural change:
   `python tools/graph/graphify.py`, then confirm with `--stale`. It was 45
   commits behind at winter S0 — a stale graph answers questions about a tree
   that no longer exists.
4. **Nothing uncommitted, everything pushed** (rule 2). `git status` clean is
   part of done, not a nicety.
5. **One line of status**: update your lane's `status` / `in_flight` /
   `as_of` in LANES.toml. One line, machine-findable — not prose in a handoff
   that the next agent has to excavate (hand-typed prose bookkeeping is what
   `gate_baseline.py`'s header is a memorial to).

---

New test names are ASCII (`gate_baseline.py lint`; a non-ASCII TEST_CASE name
silently never runs under ctest here). Files land with LF endings
(`.gitattributes` enforces it — do not fight it). `sim/` and `control/` are the
frozen kernel: no lane touches them, ever (CLAUDE.md).
