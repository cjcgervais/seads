# CONSULT PACKET — SEADS ENEMY & ALLIED AI vs THE GAME LOOP
### 2026-08-25. Issued to a barrage of independent consults and red teams.

You are one of several independent reviewers. **You have no shared context with
the others and no memory of how this AI was built.** That is deliberate: your
value is that you will re-derive from the artefacts rather than inherit anyone's
conclusions.

---

## ⚠⚠ HARD OPERATING RULES — READ FIRST

Several reviewers are running **in parallel against this same worktree.**

1. **DO NOT EDIT ANY SOURCE, CONFIG, OR ASSET FILE.** This is a read-and-measure
   review. Write your findings into your final report only.
2. **DO NOT BUILD, RELINK, OR RUN `cmake`/`ninja`/`ctest`.** The binary
   `build/seads_tests.exe` is already built and current. Relinking while another
   reviewer is running its tests corrupts both — and a failed build silently
   leaves the OLD binary in place, which WILL answer you with stale numbers.
3. **DO NOT COMMIT, STASH, OR CHECK OUT ANYTHING.** No `git` writes of any kind.
4. You MAY run the existing test binary read-only, e.g.
   `./build/seads_tests.exe "[.e18dive]" -s`, and you MAY run
   `python tools/ai_tape.py <tape>` and your own read-only Python over the tapes.
5. Cite evidence as `file:line` or as a number you measured. **An assertion with
   no number attached will be discarded.**

Worktree: `D:\seads_sandboxes\enemy-ai` (branch `sandbox/enemy-ai`).

---

# 1. THE GAME LOOP — the thing the AI exists to play

**A red team that does not know the loop will "fix" the AI into something that
cannot play it.** This section is the point of the packet.

**THE WORLD.** A ~15 km-radius spherical planet (Sudbury, Ontario, on a real
DEM), permanently winter. **Air is the scarce resource.** Breathable air exists
only inside two faction DOMES, plus a thin "go-anywhere" deck that hugs the
terrain everywhere. Outside both, lift and thrust lapse: an aeroplane mushes and
falls out of the sky. The map is mostly hostile vacuum.

**THE TWO SIDES.** VALLEY (blue) and SUDBURY (red), each a union of 3 atmosphere
bubbles over its towns. **The player flies VALLEY and is OUTNUMBERED 3 v 7** —
Chad's ruling: *"We can be outnumbered and that will increase the difficulty."*
The whole AI maverick fleet is SUDBURY and hostile. The player has 2 AI allies.

**THE OBJECTIVE — PUMPS.** Each faction has exactly TWO pumps that make its air:
a **SURFACE** pump in the open, and a **DEEP** pump in the "black stope", an
underground cavern (the ARENA). Killing an enemy pump **halves the victim's
dome**; killing both **removes the dome entirely** (floor 0.0 — that faction
loses its air). It also **grows the destroyer's own dome** by 25% of baseline.
A pump takes ~15 s of sustained all-guns-on-target fire (`pump_kill_seconds`).
AI raiders/strikers deal a configured fraction of player battery DPS while
"on-station" (in range AND pointed at the pump); **the damage is credited
app-side on that envelope, not by per-round hit tests.**

**THE TUNNEL.** An underground bore connects Errington (VALLEY) to Murray
(SUDBURY), ~1800 m below the surface at its deepest, passing through the ARENA —
a 4200 m-wide, 2600 m-tall open room where **both DEEP pumps sit**. The tunnel
holds FULL AIR regardless of the dome war, and **the terrain-crash predicate is
SUSPENDED inside the net volume** (flying 2 km under the terrain is the point).
Leaving the net while still underground = an instant deep-penetration wall
strike. The tunnel is the back door to the enemy deep pump, and a flanking route.

**THE AI'S STANDING MISSION** (Chad's 2026-07-26 ruling): Murray tunnel → black
stope → kill the VALLEY deep pump → Errington tunnel → the VALLEY surface pump.
**The player's job** is to stop them and kill the two SUDBURY pumps.

**CHAD'S OWN WORDS ON THE FEEL HE WANTS** (these are rulings, not suggestions):
* *"I want killers to contend with. That is a rule."*
* *"get out of easy mode and increase its ability to destroy pumps"*
* *"they should relentlessly attack it and fight me at the same time, not fly
  away to safety and then fight me"*
* *"they need to try to keep killing the pump, not shoot it and fly away"*
* on the AI taking both his pumps unaided while he watched: *"I call that a
  win!"*

He has signed the current difficulty as *"a good easy / medium baseline"* and has
signed the game loop itself.

★ **THE BAR IS NOT "the AI should win" — IT ALREADY WINS.** The bar is that it
should be **readable, fair, and look like flying.**

**WHAT COUNTS AS A REGRESSION:**
* an AI that stops fighting the player in the air (AI-vs-AI attrition and player
  pressure are BOTH content)
* an AI that presses objectives so hard the air war goes quiet
* aeroplanes that die to geometry rather than to the player
* anything that reads as a bug on screen (no tracers, sliding, teleporting)

---

# 2. ★★★ NEW, UNBUILT GAME-LOOP CANON — CHAD'S RULING TONIGHT

**This is not yet implemented. It is the direction the loop is going, and part
of your job is to tell us what it does to the AI.** Verbatim:

> "I will have a sudburian snowmobiler as well whom the player can land near a
> surface pump and spawn a snowmachine and be able to drive to it a short
> distance to fix the pump. Fair game targets for enemy ai, but I also plan for
> there to be an aa there as well that the player can operate by driving to it."

Unpacked, with the prior MILLWRIGHT ruling (2026-08-20) it extends:

* **TWO PLAYER CLASSES: pilot and millwright.** The millwright is the
  **Sudburian snowmobiler** — an embodied character on a snowmachine.
* **A REPAIR LOOP.** The player can **LAND near a surface pump, spawn a
  snowmachine, and drive a short distance to the pump to FIX it.** A damaged
  pump is therefore no longer a one-way loss — it is a decision to leave the air
  and go fix it. (Prior ruling: class choice per respawn ONLY while your pump is
  damaged; AI millwrights get dispatched on pump damage and are destroyable.)
* **THE SNOWMOBILER IS A FAIR TARGET** for the enemy AI. A man on a snowmachine
  in the open, next to the objective, is something the AI must be willing and
  able to attack.
* **A PLAYER-OPERATED AA GUN** sits at the pump. The player can **drive to it and
  operate it** — trading the air for a heavy ground gun defending the objective.

**SEADS already has a fully built, separately-signed SLED (snowmachine) kernel**
and an embodied Sudburian character — the winter program. This is the join
between the flight loop and the winter loop.

★ **CONSEQUENCE FOR THE AI, which is why you are being told:** the surface pump
stops being a static object to strafe. It becomes a **contested ground point**
with a repairing human, a snowmachine, and an AA battery on it. Attack runs on
it will be flown into defended airspace against moving ground targets.

---

# 3. THE ARTEFACTS

**Code (read-only):**

    drone/drone.h        the drone tick: foe/raid/defend/strike/leash/terrain
                         ladders. The RAID branch ~line 2140, the STRIKE DIVERT
                         ~line 1850, the leash chain ~line 2200.
    drone/maverick.h     the tunnel brain: PATROL/TRANSIT/DIVE_IN/RUN/CLIMB_OUT,
                         bore_track (the in-bore tracking law), aim_at/steer_bank
    drone/bfm.h          the dogfight state machine
    combat/raid.h        raider designation, on-station envelope, foe assignment,
                         pump defense
    combat/conquest.h    teams, pumps, scoring
    app/instructor_tick.h  where ORDERS are issued to drones each tick
    config/scenario.toml   [combat] and [maverick] -- THE SHIPPED DIALS
    config/game.toml       [conquest], [tunnel], [atmosphere]

    ★ THE SHIPPED TABLE IS config/*.toml. Struct defaults in the headers are NOT
      what ships and have burned this project repeatedly. If you quote a dial,
      quote it from the TOML.

**Tapes** — `build-play/conquest_tape_1..11.jsonl`, analyzer
`python tools/ai_tape.py <tape>`. **Tapes 7–11 are the current machine.** Tape 11
is the match the AI won. Record types: `d` (per-drone samples: pos, vel, hp,
foe, bfm, mav, raid/strike/def flags, leash, net, rpi/spi), `p` (player), `cq`
(score/pump hp), `dc`/`dk`/`da` (deaths), `tn` (tunnel transitions), `pk` (pump
kills), `pmp` (pump positions), `bub` (bubbles).

**Probes** (hidden tags; run read-only against the existing binary):

    [.e18dive]   closed-loop striker dive on the SHIPPED net + real DEM
    [.e17raid]   isolated: does a raider re-attack its pump?
    [.e17track]  replays recorded tape tracks through the shipped net
    [.e17sweep]  a whole 22-min match per arm (~45-60 s each) + a liveness arm
    [.e16air]    samples the actual air along the raid corridor

**Docs** — `docs/ENEMY_AI_E1_E2_SPEC.md` (the binding ledger),
`docs/SESSION_HANDOFF_20260824c_enemy_ai.md`,
`docs/SESSION_HANDOFF_20260825_enemy_ai_AUDIT.md`.
⚠ Some consults are told explicitly NOT to read these. Obey your own brief.

---

# 4. THE CURRENT STATE, STATED WITHOUT SPIN

| dial | value | state |
|---|---|---|
| `deck_terrain_relative` | true | **ON**, flown, signed |
| `transit_reach_s_per_km` | 11.0 | **ON**, flown |
| `run_recover_alt_m` / `run_recover_bank_cap_deg` | 300 / 25 | ON but **UNPAID FOR** — proven live, never shown to fix anything flown |
| `run_stall_s` / `run_stall_arc_m` | 8 / 150 | ON but **UNPAID FOR** |
| `raid_attack_alt_m` / `raid_reattack_m` | 0 / 0 | **OFF** — works isolated; takes AI-vs-AI attrition to zero |
| `strike_attack_alt_m` / `strike_glide_limit` | 0 / false | **OFF** — makes the deep pump die for the first time; but moves probe P-B's own control arm |

**Known findings that you may confirm, refute, or ignore:**
* A "saturated pursuit" defect was found in the **raid** and in the **strike
  divert**: both channels (bank and pitch) pinned at their caps simultaneously,
  producing an aeroplane that flies the OPPOSITE of what it is commanded. It is
  suspected in the **bore tracking law** too. *Is that one defect or three
  coincidences, and is there a single correct fix?*
* Every in-net wreck across tapes 10 AND 11 (7 of 7) dies 33–54 m below the deep
  pumps' own radius (11289.9).
* Tape 11's wrecks are **bit-identical** to tape 10's for drones the player never
  touched — same tick, same metre. The early match is deterministic. **Any real
  change must break that identity**; that makes determinism a differential tool.
* **P-H's `on-station` counter reads an identical 46.2 s across five different
  trajectories.** Suspected stuck. If it is, every raid ruling ever made on it is
  built on sand.
* Probe P-H flies a synthetic tunnel over flat terrain and is **structurally
  blind** to a defect that killed 7 aeroplanes on the real DEM.

**Gate baseline: 1540/1545.** The 5 reds are known and deliberate (4 sled debts +
`probe P-F` clause (2), red on purpose by Chad's ruling).

---

# 5. HOW TO REPORT

Your final message IS the deliverable — it is read by a synthesis step, not by a
human directly. Structure it:

1. **HEADLINE** — the single most important thing you found, in one sentence.
2. **FINDINGS**, ranked by severity. Each: the claim, the evidence
   (`file:line` or a measured number), the failure it causes in the GAME LOOP
   (§1/§2), and **what would falsify it**.
3. **WHAT I COULD NOT DETERMINE** — be explicit. An honest gap is worth more
   than a confident guess.
4. **IF I HAD ONE CHANGE** — the single highest-value fix, and its risk.

Do not soften. If the AI is good, say so and say why. If a previous conclusion in
§4 is wrong, say that plainly — **finding an error in §4 is a success, not a
conflict.**
