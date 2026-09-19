# CHAD'S DRIVE WORDS — sled first build, 2026-09-18 evening (verbatim, in order)

Exe: D:\seads_sandboxes\sled-ride-b1\build-play\seads.exe (RelWithDebInfo rebuild 19:47; the first
Debug build-play "took forever to start and ran jittery slow" and was replaced before any arm was judged).

- Run 1 baseline (Debug exe): "okay that seems the smae as main but it took forever to start and ran jittery slow"
- Run 1 baseline (RelWithDebInfo exe): "okay I flew the saeads.exe it was the same now start 2"
- Run 2 B0 SEADS_SLED_TRACTION_MU=3.0: "yea id say its a little different jump was more stable right at the start does that make sense? Lets try 3 . 2 is approved"
- Run 3 B1 SEADS_SLED_TRACTION_MU=3.0 + SEADS_SLED_ROLLED_THROTTLE=0.15: "FOR 3 i NOTICED NO DIFFERENCE AND DONT KNOW ITS PURPOSE , aLSO i DONT HAVE SOUND OR THROTTLE INDICATOR THAT I KNEW OF SO i MADE A TAPE I GUESS BUT DIDNT NOTICE ANY GOOD BEHAVIOR. PLEASE EXPLAIN THE PURPOSE OF 3"
  Tape sled_tape_3.sledtape (216 s): rolled 82.3 s in 7 episodes; W held while rolled 47.1 s -> engine_rpm mean 2476 / max 2645 (= 1700 + 0.15*6300, the dial FIRED) vs 1700 with W off; thrust_n max 0, |track_slip| max 0.00 for every rolled tick -> the downed track never touched ground in any of the 7 episodes. VERDICT: INCONCLUSIVE, unobservable (no engine sound in this exe = the open sled-audio bug; no HUD rpm). Not rejected, not approved.
- Run 3, his ruling after the tape check was shown to him: "3 IS APPROVED LETS MOVE ON TO 4"
- Run 4 B2a SEADS_SLED_RIGHT_MAXSPD=4.0 alone: "4 is approved I can land upright more often, lets move to 5"
- Run 5 B2b SEADS_SLED_RIGHT_MAXSPD=4.0 + SEADS_SLED_STAND_SHIFT=0.5: "make another rung to add on later so that we can designate ctrl when not rolled on side, but stuck in bank upside down nose down vertical or nose up on track, rider attached can use legs to roll it over backward and oin its side where it can then be weight shift mounted. 5 is approved move to 6"

## NEW RUNG REQUESTED BY CHAD (run 5, verbatim above) -- "THE LEG KICK"
Not built. Shape as he stated it: a NEW key state, CTRL, active when the machine is NOT rolled on its
side but STUCK -- in a bank upside down, nose-down vertical, or nose-up on the track -- with the rider
still attached; the rider uses his LEGS to roll the machine over BACKWARD onto its SIDE, from where the
existing weight-shift righting (STAND + pendulum, runs 4/5) takes over. It is the pitch-axis twin of
the roll righting: a press-gated, rider-powered, one-way move from a pitched-stuck attitude to the
on-side attitude, never automatic. Open questions for the rung: (1) the stuck detector (pitch band +
ground contact + speed ~0, hysteretic); (2) CTRL today = TUCK while riding, so the binding is
attitude-gated; (3) the leg-kick torque budget vs the tumble ruling (kill the tumble, never the
authority); (4) hands must stay welded (rider attached is his precondition).
- Run 6 B3 SEADS_SLED_TAILSHED=0.4 alone: "it needs more fishtail"  (no roll complaint stated) -> run 6b launched at 0.8, nothing else armed
- Run 6b B3 SEADS_SLED_TAILSHED=0.8 alone: "let give it a little more fishtail even still" (no roll complaint stated) -> run 6c launched at 1.4 (top of the derived ladder), nothing else armed
- Run 6c B3 SEADS_SLED_TAILSHED=1.4 alone: "okay good now 7"  -> B3 driven value = 1.4 (ladder 0.4 "needs more", 0.8 "a little more even still", 1.4 "good"); no roll complaint at any of the three
- Run 7 launched: TRACTION_MU=3.0 + ROLLED_THROTTLE=0.15 + RIGHT_MAXSPD=4.0 + STAND_SHIFT=0.5 + TAILSHED=1.4 (his value, not the .bat's 0.4)
- Run 7 all five at the driven values (TRACTION_MU 3.0, ROLLED_THROTTLE 0.15, RIGHT_MAXSPD 4.0, STAND_SHIFT 0.5, TAILSHED 1.4): "yes very good, now could we also get that crouch mechanic built where sudburian can pull it over and on its side and then use shift to re right it for when vertically static in snow, also when fully upside down to extend legs with shift would put the sled up first then falling over on its side is the stage that another press of the shift can right you. TO make the r autoright key fully redundant. ... Also on lake ice the ski runners are not digging in to the ice and there is no turn authority on it, at least at high speed it given limits turning I think it is less grip at lower speeds than I would like.  .. actually make a handoff for it commit and push all this first please , great work and thank you"
- Landing word (after the three recommendations): "yes tyo all 7  and all 3 of these reccomendations I concurr I want this all in a v2"
- §3.5 grip-ceiling debt (2.62 -> 2.80 under track_lat_slip_shed 1.4, leg already red on main), put to him by name: "accept 1.4"
- New gate red snowbank_inner_face_still_launches (traction_mu 3.0 sole owner; the BEFORE arm's obsolete pin REQUIRE(before.rolled) on the pre-W2 loose pile; AFTER arm healthy 2.608 s air), put to him by name (1 re-bar keep 3.0 / 2 walk back to 1.0 + re-drive): "1 re bar the test"
