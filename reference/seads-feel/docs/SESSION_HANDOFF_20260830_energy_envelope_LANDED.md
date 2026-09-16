> ⚠⚠ **SUPERSEDED 2026-08-31 — DO NOT LAUNCH FROM THIS FILE.**
> **Launch from `docs/SESSION_HANDOFF_20260831_enemy_ai.md`.**
>
> Its STATE and its "what's next" are stale: main has moved to `bbaf2a2d9`, a
> CONTRIBUTION SOP + `LANES.toml` now govern how a lane lands work, the gate
> checks the RED SET not the count, and the seven-reds picture in here was
> partly a **CRLF working-tree artifact** (see the new handoff §3). The
> parabola is no longer shipped OFF — Chad ruled it ON as a ONE-SHOT.
>
> **Its LAWS section is still correct and is why this file is kept.**

# ENERGY ENVELOPE — LANDED ON MAIN. HANDOFF 2026-08-30

LAUNCH LINE: ⚠ SUPERSEDED — see the banner above.

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule; every
   consult you spawn gets §1–2 pasted in.**
2. `docs/RUNG_SPEC_20260828_energy_envelope.md` — the spec that was built,
   including **§9 (Chad's five rulings)** and §0 (25 adjudicated red-team
   findings).
3. `docs/TAPE13_ANALYSIS_20260828.md` — the measurements it all rests on.
4. `docs/SESSION_HANDOFF_20260829_energy_envelope.md` — the previous handoff.
   **Superseded on state, still correct on the LAWS in its §5.**
5. `CLAUDE.md` standing laws.

---

# 0. STATE — ★ THIS ONE IS PUSHED

**`origin/main` = `d00157d06`.** Chad ruled *"land it as-is"* (2026-08-29) and it
was pushed `db319e328..d00157d06`, fast-forward, no force. Branch
`sandbox/enemy-ai` sits at the same commit, worktree
`D:\seads_sandboxes\enemy-ai`, tree clean.

⚠ **THE STANDING "DO NOT PUSH" LAW IS NOT REPEALED.** Chad authorised *this*
landing, not a change of habit. Main still advances on his stick.

**Fly build:** `D:\seads_sandboxes\enemy-ai\build-play\seads.exe`
(`pre-reconcile-20260821-248-gd00157d06`, relinked 2026-08-30 at the landed
HEAD, so it now contains the perch and the merge).

## ⚠⚠ `seads-recon` IS STILL NOT UPDATED — THE ONE UNFINISHED ASK

Chad asked for main **and** `D:\flight_sim2\seads-recon` (his fly tree). Main is
done; seads-recon is **NOT**, and it is **not ours to take**: the peer session
`sudburian-grip-law-r4a` is live in it on `sandbox/r4a-phase0` with
**uncommitted work in `test/unit/test_snow_shadows.cpp` and
`tools/sled_probe.cpp` that exists nowhere else.** It said it would message when
it releases the tree.

★ **DO NOT switch that worktree while it is held.** Standing law, and this
session lost its own uncommitted arms tonight to a careless `git checkout` —
do not do it to someone else. When released: check out main there, relink
`build-play`, hand Chad the absolute exe path.

---

# 1. WHAT LANDED

| rung | commit | what |
|---|---|---|
| ENV-1 | `d7a6c940a` | depth instrument: both dome ellipses stamped, `air_depth_m` = max over LIVE domes, `ed` on the tape |
| ENV-2 | `8c8ab05f3` | the air-envelope governor, **ON** at Chad's loose pair, constant knee |
| ENV-3 | `7d1c76c20` | defence scramble: 245 m/s sprint **ON**, Chad's dive **OFF** |
| ENV-4 | `a66bb6b88` | defence triggers on **HP loss**, not proximity |
| ENV-3.4 | `595cda95f` | **the perch** — permission becomes posture, free core only |
| merge | `d00157d06` | origin/main reconciled in, graph regenerated over the merged tree |

⚠⚠ **NAMING: ENV-n, NOT E-n.** The spec numbers rungs E1..E4; the enemy-AI
ladder already owns E1..E18 and **its "RUNG E2" is the STRIKE DIVERT**, which is
what `test_stope_probe.cpp`'s `pre_e2` partition means. Not cosmetic — that
partition's copy-list decision turns on which "E2" you mean.

**The design in one paragraph.** One continuous signed depth `d` to the nearest
LIVE dome edge. An allowance `A(d)`: deck lane outside, 600 m at the edge, a
tan-20° ramp through the fringe, a 1,650 m plateau reaching the shrink law's own
one-tick edge jump (8,700 m), then the free core. It clamps `target_gamma` at
the final seam and does nothing else — never steers, never touches bank, never
picks a target, never breaks off a chase. **No behavioural predicate**;
`d.engaged` shadowing this seam is what killed i=0. Four exemptions only:
underground+arena (structural), ballistic CLIMB/DIVE, the terrain latch (which
still wins), and the **portal-approach corridor**. On top: a defence sprint, an
HP-loss trigger, and the perch — the one law here that deliberately PUSHES,
which is safe only because the governor clamps it on the same tick.

---

# 2. THE GATE — ⚠ 1725/1732, SEVEN REDS, AND ONE IS NOT OURS

**Never report a red count without its members. This ladder has been bitten by a
count that matched while the identities moved — it happened three times in the
single session that built this.**

| red | status |
|---|---|
| `probe P-F` clause 2 | carry-over, red on purpose |
| **`E12.1`** | ⚠ **RED ON A DIFFERENT CLAUSE than its previous red** — the raid-duty ratio, `0.83809` against a required `0.84199` (14.5% uplift where the arm demands 15%). Attributed to the perch changing what order-free drones do, which churns `raider_rank`. **NOT BENT, NOT RE-BASELINED.** ⚠ Half a percent is also NOT licence to call it noise: it needs the noise arm before anyone rules. |
| 4× GI4 sled debts | carry-overs |
| **`R5 row 9 ... uShadowCount`** | ★ **NOT OURS. INHERITED FROM MAIN.** Proven, not assumed: the file did not exist on the pre-merge branch, it arrived with `origin/main`, and on disk it is **474 CRLF of 474 line endings** while the test matches a pattern SPANNING a line break. `sudburian-grip-law-r4a` already has the fix, uncommitted, in seads-recon. **Do not touch that file.** |

★ **`S4 collapse` went GREEN with the perch.** It was red the rung before
because there were ZERO post-collapse deaths, so `deck_respawns > 0` could not
reach its own path. Deaths returned; the teleport-deletion guarantee is
exercised again.

---

# 3. ★ WHAT TAPE 14 PROVED — and what it did not

`build-play/conquest_tape_14.jsonl` (10 MB), Chad's signing fly, analysed.

* ★★★ **ZERO THIN-AIR DEATHS.** All 11 deaths at `af = 1.00` — full air, terrain
  or guns. On tape 13, i=0 and i=9 died mushing in vacuum. **This is the rung's
  central claim, confirmed on the flight he actually flew rather than a fixture.**
* **The envelope is visibly shaping them.** Median altitude by depth band:
  outside **347 m**, fringe **904**, plateau **1,547**, free core **1,783**
  (p90 2,004, max 2,236). Exactly the ruled ordering.
* **The perch's premise, measured before it was built:** in the core they
  stopped a few hundred metres UNDER what they were allowed. That headroom is
  what the perch takes.

⚠ **WHAT IT DID NOT PROVE — his "SUSTAINED attacks on the surface pump".** That
is NEW (tape 13's entire surface offense was one 4.6 s strafe, never repeated)
and it is **STILL NOT ATTRIBUTED**. Candidates: the envelope keeping raiders
alive to re-attack, ENV-4's trigger, the sprint, or ordinary variance — which on
this project's own noise-floor law is large. **Do not write it up as caused.**

---

# 4. THE NEXT RUNG — ranked, with the honest case for each

## 4.1 ★ T-3: THE ARM THAT CAN REFUTE THE PERCH. Cheapest, and it is a DEBT.
The perch shipped with only pure-law arms — they grade where it fires and where
it refuses, **not whether a perched drone is a better defender or attacker.**
T-3 is: perch ON vs OFF against a scripted 270 m/s attacker, grading
**in-band-on-attacker fraction** and **time-to-first-gun-solution**, foes
present. **If those do not move, the perch is spectacle and must be REPORTED as
such, not kept because it looks right.** Required-red: `guard_alt_hi_m = 0` must
MOVE the trajectory hashes; bit-identical there means a dead branch.

## 4.2 ★★★ R-CLOSE — THE BIGGEST MEASURED LEVER IN THE WHOLE PROGRAM.
**AI-vs-AI combat has never dealt a point of damage.** Tape 13: allies ended 14
minutes at 100.0 hp, `da` = 0, over 4,402 engaged-on-drone-foe samples;
enemy-on-foe range median **4,137 m**; the 60–900 m gun band open on **13 ticks
= 0.3%**. Foes ARE assigned and the DPS seam IS armed (`kAiVsAiDpsFrac` 0.8).
What fails is **CLOSURE**. This is the lever behind BOTH of Chad's asks
("defense awareness", "attack enemyplane awareness"), and it is why the
post-collapse furball he photographed can never resolve.
⚠ Work the **AI-vs-AI** regime first: P-A's refutation
(`bfm_intercept_chase_unfaded` ON made the player-facing case WORSE — rounds
54→23, hits 16→2) does not apply there. **Do not re-attempt the player-facing
speed fix.**

## 4.3 The closed-loop replays, still unbuilt.
i=0's crossing; **i=9's 1,941 m exit — RED AT HEAD BY CONSTRUCTION**, since it
dies *with* the deck law, so a bit-identical result there means the fix is fake;
the deep-outside transit; and **E2-A8, the foes-present portal arm**. Until
E2-A8 exists nobody may claim the fringe dial pair is proven safe for the tunnel
mission — only that it is not obviously broken with nobody shooting.

## 4.4 C2 — the divert standoff law. Still the largest pre-existing debt.
`aim_station()` BESIDE `maverick::aim_at`, switching only the three objective
call sites, leaving the BFM callers alone. ⚠ Its probe MUST carry a
foes-present arm: 5 of 7 in-net crashes on tape 13 were mid-fight, and every
existing deck arm grades errand flying.

---

# 5. RULINGS OPEN FOR CHAD

1. ★ **His own PARABOLA is shipped OFF** (`defend_dive_gamma_deg = 0`). With it
   ON, the SIGNED certificate clause B went `ai_damage 0.0 > 50.0` — the air war
   stopped dead. **The read to put to him: this looks like a defect in the
   BUILD, not in his ruling.** He asked for a one-shot parabola; what was built
   is a SUSTAINED FLOOR (≥18° down every tick while high and far), which bleeds
   the energy before the merge. **A bounded one-shot with an entry and an exit
   has NOT been built.** Build the one-shot, or leave it off?
   ⚠ Build it AFTER the perch has proven it holds altitude worth spending.
2. **E12.1's half-percent miss** — noise arm first, then his call.
3. Carried, unchanged: the ballistic dive re-arm; stage 0's collision with
   unlimited pursuit; the lower attack floor over his pump; reinforcement waves
   for a dead-dome faction; deep-pump defence (he ruled it OUT for the envelope
   rung — a deep defend order is unflyable as the code stands); the raid attack
   pattern and E18, both still OFF.

---

# 6. ★★★ THE LAWS — the ones this rung paid for, in blood

* ★★★ **RUN THE MUTATION. NEVER WRITE ONE DOWN.** Four arms in one session were
  blind or wrong and only RUNNING them showed it: a named required-red that did
  not fire (a doubly-guarded property is not falsifiable one guard at a time —
  only the COMBINED mutation goes red); a knee arm probing where both candidate
  designs agree, so the mutation ran GREEN; a depth arm asserting the MAJOR
  radius at a dome centre where `ellipse_r_eff` returns the MINOR bound
  (`faction_bubbles.h:152`); and a sprint sweep that measured the PAIR because
  one dial was master for both legs.
* ★★★ **A FAILED BUILD WITH ITS OUTPUT PIPED TO `/dev/null` LETS THE STALE
  BINARY REPORT A PASS.** Twice in one session, once on the signed certificate
  itself. **Check the build's own exit, always.**
* ★★★ **A FORMULA CAN BE RIGHT ON ITS INTENDED DOMAIN AND LETHAL JUST PAST IT.**
  The first allowance permitted **527 m AGL two hundred metres OUTSIDE the air**
  — the exact vacuum altitude that killed i=0 and i=9.
* ★★★ **WHEN A FIX FAILS, THE FAILURE IS THE EVIDENCE.** The governor collapsed
  the stope offensive; the cheap fix (a 500 m TRANSIT floor) left runs at 1.
  Entries stayed HIGH while runs collapsed ⇒ DIVE_IN was still ARMING, with
  perturbed GEOMETRY. The answer was the portal-approach corridor.
* ★★★ **A CAPABILITY MEASURED AS HARMFUL SHIPS OFF LOUDLY** — named key, table
  beside it, and its SHAPE still graded with the dial forced ON, so a law that
  ships off is not left standing behind nothing.
* ★★ **CRLF:** `core.autocrlf=true`. The defect's ONLY shape is *read a file in
  BINARY, then match a pattern SPANNING a line break* — single-line patterns are
  immune. `sizeof` tripwires are immune (compile-time), and behaviour hashes
  that hash SIMULATION STATE are immune. ⚠ **If the `sizeof` tripwire moves
  after a merge, BELIEVE IT — it is a real field landing.** ⚠ MSYS
  `grep`/`cat -A`/`sed` strip CRs and will call a CRLF file clean; only a
  byte-level read proves it (`open(p,'rb').count(b'\r\n')`).
* ★★ **NEVER `git checkout` A FILE WITH UNCOMMITTED WORK IN IT.** This session
  destroyed its own perch arms that way and had to rebuild them. Copy aside
  first — the same law as "never overwrite Chad's files", applied to yourself.
* ★★ **CHECK FOR LIVE PEER SESSIONS BEFORE ANY WORKTREE OPERATION** (`ListAgents`).
  seads-recon was held with unique uncommitted work; asking first is what
  prevented destroying it.
* ⚠ A Catch2 name containing a **COMMA** cannot be selected by the exe's own
  filter and silently runs NOTHING. The signed certificate has one — reach it
  with `ctest -R "game loop certificate"`.
* Carried forward: re-measure every headline at the final head with the noise
  arm · a scope predicate is a silent shadow · a bit-identical arm means blind
  fixture OR dead branch · READ THE LOADER · a hand-mirror is not a caller · a
  fixture-wide change moves every arm that shares the fixture · when you add an
  exit to a state machine it inherits every deferral the old exits carry.

---

# 7. HOUSEKEEPING

* **Main is at `d00157d06`.** Do not push again without Chad's word.
* **Never touch another `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*`
  worktree.** ⚠ seads-recon is HELD (§0).
* **DO NOT TOUCH** `drone/bfm.h`, `maverick::aim_at`, `sim/`, `control/`,
  `assets/`. ⚠ For C2: `bfm.h` has **six** `aim_at` callers at HEAD, not the
  four the hand-maintained note claims.
* **THE GRAPH, NOT GREP**, regenerated in the SAME commit as any structural
  change, then `check`. Current at this HEAD (334 files, 1310 symbols).
* Relink the fly build before finishing and **quote the stamp you actually
  built**. ⚠ Never relink while a sweep runs.
* The census scripts (`gun_census.py` / `gun_forensics.py`) are **still only in
  a session scratchpad, not committed.** Sixth handoff carrying this.
