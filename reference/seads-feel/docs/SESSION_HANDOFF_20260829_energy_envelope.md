> ⚠⚠ **SUPERSEDED 2026-08-30 by `docs/SESSION_HANDOFF_20260830_energy_envelope_LANDED.md`.**
> The perch it names as "next" is BUILT (`595cda95f`) and the whole rung is
> LANDED AND PUSHED to main at `d00157d06`. Its §5 LAWS are still correct and
> still worth reading; its STATE, gate numbers and "what is next" are stale.

# ENERGY ENVELOPE — ENV-1..ENV-4 FLOWN AND SIGNED. THE PERCH IS NEXT.
# HANDOFF 2026-08-29

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260829_energy_envelope.md; build the
PERCH (§4.1)."**

---

# ★★★ 0. CHAD FLEW IT. SIGNED. HIS WORDS, VERBATIM.

> "I just flew it very good, build the handoff, stating that the perch is up
> next. I had enemies attacking pumps both in the tunnel and myself while thet
> also sustained attacks on the surface pump."

**Three things that sentence certifies, and one of them is NEW:**

1. **The tunnel offensive survived the governor.** He saw enemies attacking
   pumps *in the tunnel*. That was the rung's biggest live hazard — the governor
   collapsed the stope offensive mid-build (P-B runs 6+→1) and the
   portal-approach corridor is what bought it back. It holds on his stick, not
   just in the fixture.
2. **They fight him and press the objective AT THE SAME TIME.** His standing
   ruling — *"they should relentlessly attack it and fight me at the same time,
   not fly away to safety and then fight me"* — read TRUE on this fly.
3. ★ **"SUSTAINED attacks on the surface pump." THIS IS NEW AND IT IS THE ONE
   TO NOTICE.** On tape 13 the *entire* enemy surface-pump offense of a 14-minute
   match was **one 4.6-second strafe** (pump 0, −3,875 HP, 02:32.59–02:37.19)
   that was never repeated — the deck runner was killed at the pump and never
   again came within 12,662 m of any pump. "Sustained" is a different machine.
   ⚠ **NOT ATTRIBUTED.** Nobody has measured *why*. Candidates, in order: the
   envelope keeping raiders alive long enough to re-attack; ENV-4's HP-delta
   trigger changing who is where; the 245 m/s scramble; or ordinary
   match-to-match variance, which on this project's own noise-floor law is large
   enough to explain a lot. **The next tape is what settles it — do not write
   the win into a doc as caused until it is measured.**

★★★ **HIS FLY LEFT A TAPE, AND IT IS THE FIRST ONE THAT CAN ANSWER ANY OF THIS:**
`build-play/conquest_tape_14.jsonl` (10,036,453 B, 2026-08-29 13:27), analyzer
`tools/ai_tape.py`. **UNANALYSED.** It is the first tape carrying ENV-1's `ed`
field — signed depth into the nearest live dome's air, per drone per sample —
so for the first time the envelope can be graded on the flight Chad actually
flew rather than on a fixture. **Job zero for whoever picks this up**, before
the perch, because it is cheap and it settles three things at once:
 * **the "sustained" surface offense** — is it real and what caused it (§0.3);
 * **did anyone still die outside the air?** Compare against tape 13's two
   watched deaths (i=0 crossing its own rim at +1,673 engaged; i=9 at +1,941 on
   a TRANSIT with the deck law active and helpless). Any thin-air death here is
   the rung's own failure and outranks the perch;
 * **is the free core actually being used?** `ed` vs altitude, deep inside — if
   nobody ever climbs, that is the perch's premise measured before it is built,
   and it is also the honest answer to "does the perch pay".

**⚠ WHAT HE DID *NOT* RULE ON, AND IT IS STILL OPEN:** §2.1 (his own parabola,
shipped OFF because it measured harmful) and §2.2 (the S4 collapse red). A
"very good" on the fly is not a ruling on either. **Put both to him again.**

**READ FIRST, IN THIS ORDER:**
1. `docs/CONSULT_PACKET_20260825_ai_audit.md` §1–2 — the game loop and the
   millwright/AA canon, IN FULL. **An agent that does not know the loop will
   "fix" the AI into something that cannot play the game. Standing rule; every
   consult you spawn gets §1–2 pasted in.**
2. `docs/RUNG_SPEC_20260828_energy_envelope.md` — the spec this built, including
   **§9, Chad's five rulings**, and §0's adjudication of 25 red-team findings.
3. `docs/TAPE13_ANALYSIS_20260828.md` — the measurements everything rests on.
4. `CLAUDE.md` standing laws.

Branch `sandbox/enemy-ai`, worktree `D:\seads_sandboxes\enemy-ai`, HEAD
`a66bb6b88`, **nothing pushed**, tree clean. Fly build
`D:\seads_sandboxes\enemy-ai\build-play\seads.exe`
(`pre-reconcile-20260821-112-ga66bb6b88`).

---

# ⚠⚠ NAMING — READ BEFORE YOU GREP

The spec numbers its rungs **E1..E4**. Those numbers **COLLIDE** with the
enemy-AI E-ladder, which already owns E1..E18 in the same files — **"RUNG E2"
there is the STRIKE DIVERT**, and `test_stope_probe.cpp`'s `pre_e2` partition
means *that* one. In code the envelope rungs are **ENV-1..ENV-4**, mapping
ENV-n == spec rung En. Not cosmetic: `pre_e2`'s copy-list decision turns on
which "E2" you mean.

---

# 1. WHAT SHIPPED

| rung | commit | what |
|---|---|---|
| ENV-1 | `d7a6c940a` | the depth instrument: both dome ellipses stamped, `air_depth_m` = max over LIVE domes, `ed` on the tape |
| ENV-2 | `8c8ab05f3` | the air-envelope governor, **ON** at Chad's loose pair |
| ENV-3 | `7d1c76c20` | the defence scramble: sprint **ON**, Chad's dive **OFF** |
| ENV-4 | `a66bb6b88` | defence triggers on **HP loss**, not proximity |

**Gate 1582/1588.** ⚠ Six reds, but **NOT the baseline set** — see §3.

**The design in one paragraph.** One continuous signed depth `d` to the nearest
LIVE dome edge. An allowance `A(d)`: the deck lane outside, 600 m at the edge, a
tan-20° ramp through the fringe, a 1,650 m plateau reaching the shrink law's own
one-tick edge jump (8,700 m), then the free core. It clamps `target_gamma` at
the final seam and does nothing else — never steers, never touches bank, never
picks a target, never breaks off a chase. **It carries NO behavioural
predicate**; `d.engaged` shadowing this exact seam is what killed i=0.

---

# 2. ★★★ THE TWO THINGS CHAD MUST RULE

## 2.1 HIS OWN PARABOLA MEASURED HARMFUL AND SHIPPED OFF

`defend_dive_gamma_deg = 0.0`. With the dive ON, the **SIGNED game-loop
certificate's clause B** (AI-vs-AI attrition, `test_maverick.cpp:1410`) went
green → **`ai_damage 0.0 > 50.0`**. The air war stopped dead — a named
regression in the audit packet's own list.

**Isolated properly, including a wrong turn worth keeping:** sweeping the sprint
speed 150/180/210/245 failed at all four, which *looked* like "any sprint kills
the air war". That was wrong — `defend_sprint_speed` is the master for **both**
legs, so the sweep was testing the pair. Disabling **only** the dive with the
245 m/s sprint still live → **certificate GREEN**. The sprint is innocent.

★ **The read, which is Chad's to rule on: this looks like a defect in the BUILD,
not in his ruling.** He asked for a **parabola** — a one-shot conversion of held
altitude into closure speed. What was built is a **sustained floor**: at least
18° down on *every* tick a defender is high and far, self-terminating only at
pump altitude, by which point the energy is spent and it is nowhere near a
merge. **A bounded one-shot with an entry and an exit has NOT been built.**
`defend_dive_gamma_deg = 18.0` restores exactly what was measured.

## 2.2 THE S4 COLLAPSE RED

`CHECK(r.deck_respawns > 0)` fails with **0** — because there were **ZERO
post-collapse deaths** (7 wrecks before the collapse, 0 in the 30 s spike, 0
later). Chad was told to **expect** that spike as the S4 ruling working; the
governor has removed it. Plausibly "survive the deck" being met — **but the
consequence is that the teleport-deletion guarantee is now UNEXERCISED, i.e.
ungraded.** Left RED rather than bent: a signed arm that can no longer reach its
own path is a finding, not a threshold to move.

---

# 3. THE GATE — ⚠ SIX REDS, CHANGED IDENTITY

**Do not report "six reds, same as baseline".** This ladder has been bitten by a
count that matched while the members moved.

* `probe P-F` clause 2 — unchanged, red on purpose.
* `S4 collapse` — **NEW**, attributed in §2.2.
* four GI4 sled debts — unchanged carry-overs.
* **E12 and E12.1 are now GREEN.** Both were red at points during this rung.
  E12.1's earlier red was re-attributed at each operating point, never
  re-baselined.

---

# 4. WHAT IS NOT BUILT — NOBODY MAY CLAIM OTHERWISE

**Nothing built is evidence that an aeroplane is saved or a pump is defended.**
It is evidence that the laws have the shape Chad ruled and that the tunnel
offensive still completes runs *with nobody shooting*.

1. **THE PERCH — ★ THIS IS THE NEXT RUNG. Chad said so on 2026-08-29. Full
   build spec in §4.1 below.**
2. **Every closed-loop arm**: the i=0 crossing replay, the i=9 1,941 m exit
   (**red at HEAD by construction** — it dies *with* the track law, so a
   bit-identical result there means the fix is fake), the deep-outside transit,
   and the **foes-present portal arm E2-A8**. Until E2-A8 exists nobody may
   claim the fringe dial pair is proven safe for the tunnel mission — only that
   it is not obviously broken with nobody shooting.
3. **R-CLOSE** — the biggest measured lever and NOT this spec: AI-vs-AI combat
   has never dealt a point of damage (enemy-on-drone-foe range median 4,137 m;
   the 60–900 m gun band open on 13 of 4,402 engaged ticks = **0.3%**).
4. The constexpr→TOML hoist of `kDefendThreatRadiusM` / `kDefendersPerFaction`.

---

# 4.1 ★★★ THE PERCH — THE NEXT RUNG, IN FULL

**Chad's ruling it completes (2026-08-28, verbatim):** *"if they are deep in
bubble they can climb and get an energy advantage to be more effective defenders
and attackers, **make sure that ability persists**."*

★ **THE GAP, STATED EXACTLY.** ENV-2 gives a drone deep inside its air
**PERMISSION** to be at 2,000 m. **Nothing makes it TAKE that altitude.** The
allowance is a ceiling, never a target — that is by design and must not change,
because a ceiling that pushes is a behaviour and this rung's whole thesis is
that the envelope is a constraint stage. So the energy advantage he ruled is
today a *possibility*, not a *posture*. The perch is what converts it.

★ **AND IT IS ALSO THE PRECONDITION FOR HIS PARABOLA (§2.1).** The dive is
shipped OFF because what was built bleeds energy it never had. A one-shot
altitude→closure conversion needs altitude worth spending. **Build the perch
first; then the dive question can be asked honestly.**

## The shape

A new **`GuardOrder`**, app-stamped beside the defend order, drone-side branch
inserted **LAST in the errand chain** — after defend / regroup / raid / strike,
before the plain-leash arm — so it can never starve a real order. It applies to
drones that are **non-engaged, order-free, and deep inside** (`d > d_knee + 500`,
using ENV-1's `d.air_depth`), flying `aim_at` toward a guard point above the
faction's standing station at target altitude
`min(guard_alt_hi_m, A(d) − 300)`.

* Dials, NEW, `require()`-loaded, **0 = OFF bit-identical**:
  `guard_alt_lo_m` / `guard_alt_hi_m` = 1,500 / 2,500.
* ⚠ The `− 300` keeps the perch strictly UNDER the envelope so the two never
  fight for the elevation channel. The governor still clamps it; the perch must
  never be the thing that gets clamped, or it will chatter.
* ⚠ **The fleet already loiters at 1,807–2,013 m** (measured, tape 13). The
  perch does not invent a new altitude — it **formalises the one they already
  fly and couples it to the edge**, which is the part that kills them today.
* ⚠ Adding dials moves `sizeof(DroneParams)`. The `pre_e2` tripwire
  (`test_stope_probe.cpp`) WILL go red at 1624 — that is it working. Decide the
  partition side (these are NOT E2 strike/arena dials → they go in the
  copy-list), then weld the new size **in the same commit**.

## The arm, and its required-red

**T-3, "the perch pays":** a scripted 270 m/s attacker strafing the pump, perch
ON vs OFF, grading **in-band-on-attacker fraction** and **time-to-first-gun-
solution**.

★ **THIS IS THE ARM THAT CAN REFUTE THE WHOLE RUNG, AND IT MUST BE ALLOWED TO.**
If those numbers do not move, the perch is spectacle — aeroplanes sitting
prettily at altitude — and it must be **reported as such**, not shipped because
it looks right. **Required-red:** with `guard_alt_hi_m = 0` the arm's trajectory
hashes must MOVE; bit-identical there means a dead branch, not success.

⚠ **Foes-present is mandatory.** Every existing deck arm grades errand flying;
this capability exists only where there is a fight.

⚠ **Watch the certificate.** ENV-3's dive took the signed AI-vs-AI attrition
clause to `ai_damage 0.0` and the perch touches the same population. Run
`ctest -R "game loop certificate"` **early**, not at the end.

---

# 5. ★★★ THE LAWS THIS RUNG PAID FOR

* ★★★ **RUN THE MUTATION. NEVER WRITE ONE DOWN.** Four arms in one night were
  blind or wrong and only running them showed it: (1) a named required-red that
  **did not fire** — the dead-dome exclusion is DOUBLY guarded, so neither guard
  is falsifiable alone and only the COMBINED mutation goes red; (2) the knee arm
  probed at d=5,000 where *both* candidate knees land on the plateau, so the
  live-knee mutation ran GREEN; (3) a depth arm asserted the MAJOR radius at a
  dome centre and went red by 4,350 m — `ellipse_r_eff` returns the MINOR bound
  there (bearing undefined, `faction_bubbles.h:152`); (4) the sprint sweep
  measured the pair, not the speed.
* ★★★ **A FAILED BUILD WITH ITS OUTPUT PIPED TO /dev/null LETS THE STALE BINARY
  REPORT A PASS.** Happened twice tonight, once on the signed certificate
  itself. Caught only by grepping `error:`. **Check the build's own exit.**
* ★★★ **A FORMULA CAN BE RIGHT ON ITS INTENDED DOMAIN AND LETHAL JUST PAST IT.**
  The first allowance permitted **527 m AGL two hundred metres OUTSIDE the air**
  — the ramp keeps producing positive ceilings on negative depth, which is the
  exact vacuum altitude that killed i=0 and i=9.
* ★★★ **WHEN A FIX FAILS, THE FAILURE IS THE EVIDENCE.** The governor collapsed
  the stope offensive (P-B runs 6+→1). The cheap fix — a 500 m TRANSIT floor,
  because the approach point sits 500 m over a mouth outside the domes — left
  runs at **1**. Entries stayed HIGH (46) while runs collapsed ⇒ `DIVE_IN` was
  still ARMING, with **perturbed geometry**. The answer was a **portal-approach
  corridor**, derived from the maverick's own dials. ⚠ NOT "exempt TRANSIT":
  that re-admits i=9, which died leaving its dome at 1,941 m *on a TRANSIT*.
* ★★ **A CAPABILITY MEASURED AS HARMFUL SHIPS OFF LOUDLY** — named key, table
  beside it, and its SHAPE still graded with the dial forced on, so a law that
  ships off is not left standing behind nothing.
* ⚠ A Catch2 name containing a **COMMA** cannot be selected by the exe's own
  filter and silently runs nothing. The signed certificate has one — reach it
  with `ctest -R "game loop certificate"`.
* Carried forward: re-measure every headline at the final head with the noise
  arm · a scope predicate is a silent shadow · a bit-identical arm means blind
  fixture OR dead branch · READ THE LOADER · a hand-mirror is not a caller · a
  fixture-wide change moves every arm that shares the fixture.

---

# 6. HOUSEKEEPING

* **Do not push.** Main advances on Chad's stick. Commit freely here.
* **Never touch another `D:\seads_sandboxes\*` or `D:\flight_sim2\seads*`
  worktree.** ⚠ R4a landed on main at `c8d88d325` and the snow lane
  (`sandbox/snow`) is rebasing — stay clear of both.
* **DO NOT TOUCH** `drone/bfm.h`, `maverick::aim_at`, `sim/`, `control/`,
  `assets/`.
* **THE GRAPH, NOT GREP**, regenerated in the SAME commit as any structural
  change, then `check`. It is current at this HEAD.
* The census scripts (`gun_census.py` / `gun_forensics.py`) are **still only in
  a session scratchpad, not committed.** Fifth handoff carrying this.
