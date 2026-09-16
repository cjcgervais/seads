# ENEMY-AI — RUNG E17. HANDOFF 2026-08-24 (late)

---

# ★★★ FLOWN 2026-08-25 — TAPE 11. HIS VERDICT, VERBATIM

> "It looks like one of them may have crashed the tunnel, I just loitered in my
> zone and watched, enemies took care of both pumps, I call that a win!"

**THE ENEMY WON THE MATCH UNAIDED, IN 7 MIN 36 S, WHILE HE WATCHED.**

    06:38.06   pump 2 (his DEEP pump) destroyed
    07:00.86   pump 0 (his SURFACE pump) destroyed
    07:36.58   final  score [0, 200]  outcome_int 0

★ **AND THE RAID ATTACK PATTERN WAS OFF FOR THIS.** The offense that won is
E12/E13/E16 plus the terrain deck — nothing from E17's raid half. That is a
direct input to the open ruling below: **the enemy can already take both pumps
without it**, so the attack pattern is no longer needed to make the AI win. It
is now purely a question of whether he wants raiders that *look* like they are
pressing, at the price of the AI-vs-AI air war.

## ★★★ AND HIS "one of them may have crashed the tunnel" IS RIGHT — AND MY
## ATTRIBUTION WAS WRONG. THE SPIRAL FIX DID NOT TOUCH IT.

Tape 11 has **FOUR** in-net wrecks (tape 10 had three), and two of them are
**BIT-IDENTICAL to tape 10 — same tick, same metre**:

    tape 10  dc i=7  t=04:27.57  pos=[-739.5, -6033.796, 9469.571]
    tape 11  dc i=7  t=04:27.57  pos=[-739.5, -6033.796, 9469.571]

The recovery never engaged. `dw.dparams = scen.drone` is a wholesale copy so the
dial did reach the game — it simply was not in the loop that matters.

★★★ **EVERY IN-NET WRECK, BOTH TAPES, DIES 33-38 m BELOW THE DEEP PUMPS' OWN
RADIUS.** Both deep pumps sit at |pos| = 11289.9:

    tape drone   |pos| at death   nearest deep pump   dist    radius gap
      10     7          11252.8              pump3   3613 m       -37.1
      10     2          11254.3              pump2   2389 m       -35.6
      10     9          11252.0              pump3   3063 m       -37.9
      11     7          11252.8              pump3   3613 m       -37.1
      11     2          11254.3              pump2   2389 m       -35.6
      11     4          11256.7              pump2   1694 m       -33.2
      11     9          11253.2              pump3   3883 m       -36.8

That is not the bore's centreline. **They are diving at the DEEP PUMP and
hitting the arena floor ~35 m under it**, every time, from 1.7 to 3.9 km out —
inside `StrikeOrder::engage_m` (6500 m).

**SO IT IS THE STRIKE DIVERT, NOT `bore_track`.** While the divert owns the run
the steering is `aim_at(deep_pump)` — mode stays RUN, which is why the tape reads
`mav=RUN`, and why I misread it. `bore_track` (and therefore `run_recover_alt_m`)
is BYPASSED, and `ro.hold_exit` is set, which is exactly what my corrected stall
bail declines to interrupt. Both halves of the tunnel fix were aimed at a law
that was not flying the aeroplane.

★ **AND IT IS THE SAME DEFECT AS THE RAID, UNDERGROUND.** A saturated
pure-pursuit at a POINT TARGET WITH A FLOOR RIGHT UNDER IT. The medicine already
built and measured for the raid — **aim ABOVE the target so the elevation
channel stops saturating** — is the fix here, applied to the strike divert
(`strike_k_el` / `strike_gamma_cap`, `drone/drone.h`'s strike block). That is the
next rung, and it is small.

⚠ **THE SPIRAL FIX AND THE RUN BAIL ARE THEREFORE UNPAID FOR.** They are green,
0-OFF, and harmless, but nothing has yet shown either fixes anything he has
flown. Do not describe them as having fixed the tunnel deaths. The P-H sweep
said so first (bit-identical no-op) and tape 11 confirmed it in his own game.

---

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260824c_enemy_ai.md; take the next
enemy-AI rung."** Supersedes `docs/SESSION_HANDOFF_20260824b_enemy_ai.md`,
whose OPEN item 1 (THE TUNNEL-NET COLLISIONS) is **diagnosed and fixed**, and
whose OPEN item 2 (the fire chain) is now entangled with this rung's raid half
— see THE ONE RULING OPEN below.

⚠ **E16 AND E15 WERE NEVER COMMITTED.** `sim/aero.h`, `config/game.toml`,
`app/main.cpp`, `config/load_game.*`, `config/scenario.toml` and the b-handoff
are all still working-tree only. What Chad flew as tape 10 exists as
uncommitted edits, and this rung is stacked on top of them.

---

## HIS TWO RULINGS, VERBATIM

> 1. "They need to try to keep killing the pump, not shoot it and fly away."
> 2. (on fixing the arena crossing rather than only bailing out) "yes on that"

## ★★★ THE TWO DEFECTS TURNED OUT TO BE ONE SHAPE

**Both pursuit laws pin BOTH channels at their caps at once, and an aeroplane
with a saturated bank and a saturated pitch demand flies the opposite of what
it is commanded.** Neither was a missing behaviour; both were attack geometry.

### The raid (his ruling 1)

`test/unit/test_raid_persist_probe.cpp`, tag `[.e17raid]`. Isolated: one
raider, a live RaidOrder, nothing else armed.

    cmd_bank   pinned 64.7 deg (of 64.7) for 120 s out of 120
    cmd_gamma  pinned -29.8 deg (of -29.8) -- a dive at a GROUND target
    AGL        900 -> 430 m in fifteen seconds, then 310-450 m

`k_az 3.5` saturates the bank past 18.5 deg of bearing error, so the command is
bang-bang and never modulates. The raider arrives low, fast and banked, and
from then on the pull that would turn it is spent not hitting the ground.
Measured: 993 m from its pump, **6517 m away eighty seconds later**, still
commanding a hard turn. On his tape: one 335 m pass at t=220 s, ten kilometres
of departure, next pass three minutes later — **15.8 s of on-station bought
with 652.8 s of standing orders**, with `raid=1 foe=-1 eng=0 leash=0 def=0`
throughout. Nothing was competing for the stick.

### The tunnel (his ruling 2)

`test/unit/test_tunnel_track_probe.cpp`, tag `[.e17track]` — replays tape 10's
recorded tracks through the SHIPPED net (params off `cfg::load_game_toml`, real
Sudbury DEM).

    3 of his 4 AI runs wrecked, all at the SAME radius (11240/11224/11239 m)
      at lateral points kilometres apart -- a SHELL, not terrain relief
    the bore bottoms at R=13305 (1800 m below terrain) at arc s=5984
    the SURVIVOR bottomed at 13302, sd in a tight -65..-290 m band,
      arc rising monotonically 0 -> 1.000
    Chad himself flew to R=10802 -- 440 m DEEPER than any of them -- and lived

The three that died opened identically, then at arc 0.46-0.55 — where the bore
crosses the **arena**, the 4200 m-wide open room — the walls fell away
(sd -1000, -2000, -2400 m).

★★★ **AND THE VERTICAL CHANNEL WAS COMMANDING A FULL +44.1 deg CLIMB, PINNED AT
THE CAP, THE ENTIRE WAY DOWN.** Track 7A: alt_err +755 → +1719 m *below* the
centreline, cmd_gamma +44.1 throughout. Track 2A: +1018 → +2063, same. They
were told to climb, at maximum, and sank two kilometres. That is a **spiral**:
1-2 km off the centreline saturates the bank too, and a pinned bank plus a
pinned climb demand does not climb — it tightens the spiral into the room's
floor. They die crossing back into rock, where `sim/step.cpp`'s tunnel yield
stops suspending the crash predicate and grades it a deep-penetration strike.

---

## WHAT SHIPPED

    run_recover_alt_m        300.0   scenario.toml [maverick]  [was: absent]
    run_recover_bank_cap_deg  25.0
    run_stall_s                8.0
    run_stall_arc_m          150.0
    raid_attack_alt_m          0.0   scenario.toml [combat]  ★ OFF, see below
    raid_reattack_m            0.0                           ★ OFF, see below

**THE SPIRAL FIX** (`run_recover_alt_m`): past 300 m of vertical error, hand the
lift back to the vertical by shrinking the bank cap toward 25 deg, blended over
[300, 600]. Cross-track can wait — the room is 4200 m wide with nothing to hit
laterally; altitude is the axis that kills. 300 m is far outside the ±80 m band
a healthy run holds and inside the 755 m the first wrecked sample was already
at, so it can only engage on a run already on its way to the wreck.

**THE RUN BAIL** (`run_stall_s`): RUN was the only tunnel mode with no timeout —
TRANSIT has a budget, DIVE_IN 25 s, CLIMB_OUT 40 s — and both its exits are
functions of `m.s_est` alone, so a lost centreline made RUN permanent. Now a
run that has not made 150 m of arc in 8 s breaks off UP.

---

## ★★★ THE ONE RULING OPEN: THE RAID SHIPS **OFF**

Built, measured, and shipped off on the E15 precedent, because the price is
yours to set and not mine:

**THE RAID BRANCH MUTES GUNS EXCEPT AT THE PLAYER** (E6.5's red-teamed
PLAYER-FOE-ONLY fold). Making raiders PERSIST on the pump keeps them in the
errand branch instead of dropping into pursuit/BFM — and the AI-vs-AI air war
goes quiet. On the signed certificate clause B ("real AI-vs-AI fighting with
real attrition", floor `ai_damage > 50`):

    OFF (0 / 0)     GREEN          400 / 1800   ai_damage  0.0   RED
    0 / 1800        GREEN          400 / 0      ai_damage 16.4   RED
    400 / 4000      GREEN

⚠ The greens and reds disagree in a way a 6-minute whole-match sim near a
threshold is entitled to (**the probe noise-floor law**). It is NOT tuned to
green — it is OFF, and the trade goes to you: **pump pressure bought with
AI-vs-AI attrition.**

WHAT IT BUYS, isolated and unambiguous (`[.e17raid]`, closest approach in 120 s):

    start 400 m past    OFF 993 m                 ON 749 m
    start 1500 m past   OFF never returned (1813) ON 410 m
    start 4000 m past   OFF never returned (3980) ON 393 m

ON, the raider settles into a **~60 s re-attack cycle** — passes at 449 m and
417 m, steady 65 deg bank at a steady 390 m AGL. OFF it never comes back at all.

**To turn it on: `raid_attack_alt_m = 400.0`, `raid_reattack_m = 1800.0`.**

---

## ★★ WHAT IS **NOT** VERIFIED — READ THIS BEFORE TRUSTING THE SPIRAL FIX

`[.e17sweep]` (new, one 22-minute match per arm) shows the spiral fix at its
shipped value is a **bit-identical no-op in P-H** — every number to the digit.
A liveness arm at `run_recover_alt_m = 1 m` DOES move (crashes 20→16), so the
branch is live and correctly wired; it simply never fires, because **P-H's
tunnel runs hold the bore within ±300 m and cannot see the defect**. P-H flies
`test_tp()` over `uniform_field(300)`; tape 10's spiral came from the shipped
net over the REAL DEM.

So the spiral fix is: mechanism measured from Chad's own tape against the
shipped geometry, code proven live, **and unverified against the defect it
targets.** The next session's first job is a closed-loop tunnel probe on the
shipped net + real DEM. Do not report it as measured until that exists.

P-H sweep, for the record (baseline = the shipped world Chad flew):

    E0 PRE-E17                        crashes 20 (0.89/min)  tunnel 11/2723
    L  LIVENESS recover_alt 1 m       crashes 16             tunnel 12/2820
    T1 spiral fix only                crashes 20  <- BIT-IDENTICAL to E0
    T2 spiral + bail                  crashes 16 (0.71/min)  tunnel 11/2716
    R1 attack pattern only            crashes 12 (0.54/min)  tunnel  7/3635
    S  SHIPPED-at-the-time            crashes 12 (0.54/min)  tunnel  9/3039

★ Note R1: the attack pattern cut crashes hard AND pushed the player's first
pump loss out 6.0 → 7.3 min — i.e. in P-H it traded aggression for survival,
which is the OPPOSITE of the ask. But P-H's `on-station` reads an identical
46.2 s across all five arms, which is not credible for five different
trajectories — **that counter is suspect and should be audited before anyone
rules on the raid using it.**

---

## STATE

- Worktree `D:\seads_sandboxes\enemy-ai`, branch `sandbox/enemy-ai`.
- **NOTHING COMMITTED** — this rung AND E15/E16 beneath it are working-tree only.
- Gate: **1540/1545, and the 5 reds are EXACTLY the documented baseline** — the
  4 GI4 sled debts + `probe P-F` clause (2), red on purpose by his ruling.
  **Do not bend P-F a third time.**
- FLY BUILD relinked (`build-play/seads.exe`). Graph regenerated, `check` OK.
- Zero `assets/` touched.

## ★ TWO THINGS THE GATE CAUGHT IN MY OWN WORK

* ★★★ **A NaN I SHIPPED INTO THE PLANT.** `glm::normalize(d.raid.target_pos)` —
  and RaidOrder's default target IS the origin. Three tests died on
  `sim/invariants.h` with a hard fail-fast. The repo's own raid.h precedent
  ("coincident => no NaN") was one file away. Guarded now with a length.
* ★★★ **MY BAIL PULLED STRIKERS OUT OF THE CHAMBER MID-STRAFE.** A striker
  diverted onto the deep pump is orbiting it, off the bore, making no arc
  progress BY DESIGN — my stall detector called that a stall. Probe P-C went to
  ZERO rounds at the player. The existing exit already deferred to
  `ro.hold_exit`; my new one did not. **When you add an exit to a state
  machine, it inherits every deferral the old exits carry.**
* ★★ A hung `ctest` from an earlier session had been sitting on one unit test
  ("orbit inertia: back-to-back grabs blend by the EMA factor") for 80 minutes,
  holding `seads_tests.exe` and blocking every relink. Killed it. **That test
  can hang; nobody has investigated why.**

## OPEN, RANKED

1. ★★★ **CHAD'S RULING ON THE RAID TRADE** (above). One line either way.
2. ★★ **A CLOSED-LOOP TUNNEL PROBE ON THE SHIPPED NET + REAL DEM.** Without it
   the spiral fix is unverified and P-H is structurally blind to the whole
   class. This is the instrument this rung needed and did not build.
3. ★★ **P-H's `on-station` COUNTER IS SUSPECT** — 46.2 s identical across five
   different trajectories. If it is stuck, every raid ruling ever made on it is
   built on sand.
4. ★★ **THE FIRE CHAIN — "not aggressive"** (carried, and now entangled: the
   raid branch's gun mute is exactly why the attack pattern costs attrition).
5. ★ **E14 is still unflown** (the defender slot).
6. ★ The render mirror is not terrain-relative (carried from E16).
7. `probe P-F` clause (2) — his ruling to re-make. Do not bend it.
