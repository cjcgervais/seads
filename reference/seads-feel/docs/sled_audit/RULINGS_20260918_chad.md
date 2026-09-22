# CHAD'S RULINGS ON THE SLED RIDE AUDIT — 2026-09-18 morning (verbatim)

Reply to `docs/SLED_RIDE_AUDIT_20260918.md` §4. His words, untouched; the mapping to the packet's
ruling numbers is the audit's reading and is marked as such.

> "86 to 91 all tapes are free riding, often rolling when hitting banks, throttle is all or nothing
> ive mitigated this by tapping and applying brake. Some of the being upside down on the ground is
> due to deliberate jumps for performing a flip off of a banks for fun but maybe only 10% of the time
> its a deliberate hit the bank hard to see what happens, other times its just going down the road
> and a ski hitting the bank on one side. a backflip sometime deliberate is not a roll over, throttle
> is cut unless im key pressing but if I key press throttle should ramp up, self righting with a
> press and I want to be able to self right by rocking bodyweight back an fourth while pressing
> stand on and off, gain pendulum momentum (not automatic re righting). I can only really turn
> sharply if I alternate gas/brake, but throttle should also be able to swing my tail around on
> account of the roost, esp with weight shifting of the sudburian.... Need to make it more dynamic
> with bodyweight (also for jump weight shift influence in air)... please continue automatically I
> have to go to work... bye and good luck. pace so as not to run out of tokens per session limit."

## Mapping to the packet's rulings (audit's reading — verify against the words above)

| # | packet question | his word | status |
|---|---|---|---|
| R1 | felt report tapes 86–91; free riding or roll tests? | ALL free riding. Rolls happen "when hitting banks": ~10 % deliberate hard bank hits "to see what happens", some deliberate flips off banks for fun, the rest = "going down the road and a ski hitting the bank on one side". | ANSWERED. The v17 numbers stand as free riding. |
| R3 | throttle cut while rolled? | "throttle is cut unless im key pressing but if I key press throttle should ramp up" | ANSWERED: a held throttle key must RAMP UP even while rolled (today `sim/sled.cpp:292-293` zeroes it). Hysteresis half unanswered. |
| R4 | is a backflip a rollover? | "a backflip sometime deliberate is not a roll over" | ANSWERED: NO. Deliberate flips must be separated from rollovers in every count. |
| R11 | self-righting with or without a press? | "self righting with a press and I want to be able to self right by rocking bodyweight back an fourth while pressing stand on and off, gain pendulum momentum (not automatic re righting)" | ANSWERED: WITH a press; the mechanic is a PENDULUM PUMP (rock the rider + stand on/off), never automatic. Rung 4 as written (C2 no-press righting) is OFF the table. |
| R6 | steer in the air, value? | "Need to make it more dynamic with bodyweight (also for jump weight shift influence in air)" | ANSWERED in direction: YES, airborne weight-shift authority wanted (K2 `k_air_shift` ON). Value = his drive. |
| NEW-A | throttle input | "throttle is all or nothing ive mitigated this by tapping and applying brake" | NEW ASK: the thumb throttle reads as binary; he modulates by tapping + brake. Candidate rung: throttle ramp/curve (`app/main.cpp` W ramp 2.5/s up, 6.0/s down) — measure tap statistics in the tapes first. |
| NEW-B | sharp turning | "I can only really turn sharply if I alternate gas/brake, but throttle should also be able to swing my tail around on account of the roost, esp with weight shifting of the sudburian" | NEW ASK: throttle-induced tail swing (power oversteer from track slip/roost) modulated by rider lean. Touches drift canon (RC-3) and Rung 2 (`plane_lat_lean_gain`). |
| NEW-C | bank hits | "often rolling when hitting banks ... a ski hitting the bank on one side" | NEW ATTRIBUTION: one-ski bank contact = the roll trigger. Rung 1 (`class_blend_m`) is the corridor-edge half; the bank face itself (bank_height 1.30, rise 3.0) is the other half — leg X1 must attribute by bank contact, not surface class alone. |
| R2 | class_blend A/B | not addressed | OPEN |
| R5, R7, R8, R9, R10, R12, R13, R14 | — | not addressed | OPEN |

## Process ruling
"please continue automatically ... pace so as not to run out of tokens per session limit." — the
follow-up round runs unattended at low concurrency; nothing lands on main; any build is a fresh lane
he drives tonight.
