--!strict
--[[
	GameConfig — single source of truth for all tunable constants.

	DESIGN THESIS (do not violate): flight physics and asymmetric balance are ONE
	system. Every flight number below is also a balance number. Tune them together.
	See docs/RESEARCH.md and docs/HANDOFF.md.

	The flight model is built on Energy-Maneuverability theory: each bird's behaviour
	is governed by its ability to convert energy (altitude <-> speed) and to turn.
	- EAGLE  = energy fighter (boom-and-zoom): high wing loading -> fast, WIDE turns,
	           strong climb/dive, high endurance. Wins by attrition.
	- CROW   = angles fighter: low wing loading -> tight turns, fast roll, agile in a
	           swarm of 4. Trades/sacrifices collisions.

	Units: studs, seconds. Workspace.Gravity is 196.2 studs/s^2 (Roblox default).
]]

local GameConfig = {}

--------------------------------------------------------------------------------
-- WORLD SCALE / GRAVITY — the master recalibration knob (session 8, "real-scale raptor
-- flight"). See docs/RESEARCH.md §v3. The whole v2 lift/drag model was implicitly pinned to
-- Roblox's DEFAULT gravity 196.2 studs/s², which at the canonical 0.28 m/stud is ≈5.6× real g
-- — so dives accelerated too punchily and there was no altitude to develop a real stoop.
--
-- THE FIX (proven invariant): drop gravity toward real AND drop AIR_DENSITY by the SAME factor.
-- Then v_stall = √(g·wingLoading / (clMax·ρ)) has g and ρ CANCEL, so stall/cruise/dive/terminal
-- speeds — and the `stall < spawn < cruise` invariant — are preserved EXACTLY at any gravity.
-- Only the ACCELERATIONS get gentler (real-scale "majesty") and turns/dives get proportionately
-- bigger (hence the larger world + 2 km ceiling). The other gravity-coupled forces (flapThrust,
-- flapClimbForce, Thermals.strength) are multiplied by GRAV_SCALE below to hold their ratio to
-- weight. Everything scale-free (all speeds, control-authority speeds, every rad/s rate,
-- TERMINAL_VELOCITY, CRASH_SPEED) is UNCHANGED — so the player-approved v2 feel character
-- (grip, powered climb, momentum, eagle-wide/crow-tight) survives untouched.
--
-- GRAVITY_G is the ONE knob to sweep in playtest:
--   1.0 = true real g at 0.28 m/stud (gravity ≈ 35; most majestic / longest stoop)
--   2.0 = "arcade-real" (gravity ≈ 70, ≈2.8× gentler than the old 196.2) — current default
--   3.0+ = punchier / arcadier.  The envelope & invariant cannot break across the whole range.
--------------------------------------------------------------------------------
local METERS_PER_STUD = 0.28           -- Roblox canonical scale (2019 World panel)
local REAL_G          = 9.81           -- m/s²
local GRAVITY_G       = 1.3            -- fraction of real gravity — THE sweep knob (2.0->1.5->1.3: more loft + less climb-speed bleed, since the along-climb gravity decel scales with this)
local GRAVITY         = REAL_G * GRAVITY_G / METERS_PER_STUD  -- studs/s² (≈70.1 at GRAVITY_G=2)
local LEGACY_GRAVITY  = 196.2          -- the gravity the v2 feel was originally calibrated at
local GRAV_SCALE      = GRAVITY / LEGACY_GRAVITY              -- multiplies every gravity-coupled force
local function meters(m) return m / METERS_PER_STUD end      -- real metres → studs (world sizing)

GameConfig.MetersPerStud = METERS_PER_STUD -- exposed for any UI/debug that wants real-unit readouts

--------------------------------------------------------------------------------
-- GLOBAL FLIGHT ENVIRONMENT
--------------------------------------------------------------------------------
GameConfig.Flight = {
	GRAVITY = GRAVITY,          -- studs/s^2 (DERIVED from scale above). Set Workspace.Gravity to match (GameServer).
	GRAVITY_G = GRAVITY_G,      -- echoed for tooling/HUD; edit the local above to retune
	GRAV_SCALE = GRAV_SCALE,    -- echoed: factor applied to all gravity-coupled forces
	-- AIR_DENSITY is the abstract scalar that folds 0.5*rho*wingArea into the lift/drag terms.
	-- CALIBRATED for the v2 lift curve: with cl0~0.3 and these profiles, level cruise at each
	-- bird's glideSpeed trims at a few degrees AoA (lift ~= gravity), and stall speed sits well
	-- below spawn (0.9*glide) and cruise. See FlightPhysics.computeCL and the calibration note
	-- in project-open-questions. Raising it raises lift everywhere (lower stall speeds).
	-- v3: scaled by GRAV_SCALE in lockstep with gravity so stall/cruise/terminal speeds stay put
	-- (the g and ρ in v_stall cancel — see the WORLD SCALE block). The BASE (the 0.0xx literal) is the
	-- real loft lever: it sets lift-vs-weight independent of gravity, so raising it lowers stall speeds
	-- and gives more loft / readier climb. Raised 0.033 -> 0.044 (session: "need more loft / go up more
	-- readily"). Sweep GRAVITY_G for accel "punch"; edit THIS base for outright lift/loft.
	AIR_DENSITY = 0.044 * GRAV_SCALE,
	WIND_BASE = Vector3.new(0, 0, 0), -- ambient wind; per-map override allowed
	-- Control authority scales with airspeed: below MIN you can barely steer
	-- (this is what makes a stalled/slow bird helpless and gives speed meaning).
	CONTROL_AUTHORITY_MIN_SPEED = 20,  -- studs/s where control ~0
	CONTROL_AUTHORITY_FULL_SPEED = 90, -- studs/s where control is full
	-- Aerobatics (loop / barrel roll) unlock above this airspeed. NOTE (session: holistic redesign):
	-- with STABILITY_RATE = 0 the weathervane this gate suppresses is already inert, so this is now a
	-- near-cosmetic safety hook — the GRIP assist (not a weathervane) governs the nose at all speeds.
	-- Lowered 110->60 so it can NEVER fight a slow loop top: loop COMMIT is owned by the controller's
	-- ride-the-edge AoA protection (Controls.aimStallBandDeg) + powered-lift floor, not by this number.
	AEROBATIC_MIN_SPEED = 60,
	TERMINAL_VELOCITY = 420,  -- RAISED 400→420 (Chad 2026-07-02): this is the FLAP-achievable top speed. In
	                          -- FlightPhysics the non-diving hardCap = TERMINAL, and the glide-bleed-to-maxLevelSpeed
	                          -- is SKIPPED while flapping — so flapping out of a dive holds/boosts speed up to TERMINAL
	                          -- ("once the attitude is reached, the flap boosts"). 420 = the true top speed via flapping;
	                          -- a pure dive tops lower at Eagle.diveSpeedCap=390. (Anti-cheat ceiling scales with this,
	                          -- so the legit 420 stays inside the envelope. Crow can't approach it — flapThrust far lower.)
	-- STATIC AERODYNAMIC STABILITY (rad/s base): how fast the airframe weathervanes its
	-- ORIENTATION toward the airflow. This self-limits angle-of-attack (anti-stall) WITHOUT
	-- destroying lift — it rotates the nose toward the path, not the path toward the nose, so
	-- the v2 kernel keeps lift while the player's pitch input still dominates. Per-profile
	-- `stabilityRate` multiplies this; `recoverNoseDownRate` adds extra pull past the stall.
	-- (Replaces v1's PATH_TRACK_RATE, which rotated VELOCITY toward the nose and thus drove
	-- AoA->0->no-lift — the root cause of the old constant-stall spiral.)
	-- Auto-leveling weathervane: ELIMINATED (1.5 -> 0.7 -> 0.15 -> 0). Chad: "get rid of any
	-- auto-levelling... have the AoA not change unless input is given by the user." The grip
	-- assist below (velocity follows the nose) is what now keeps AoA low / prevents stall, so the
	-- nose is NEVER dragged back toward the airflow on its own — it stays exactly where the player
	-- points it. The per-profile recoverNoseDownRate (past-stall auto-recovery) is also zeroed so
	-- nothing rotates the nose without input. PLAYTEST-PROVISIONAL (re-add a whisper if low-speed
	-- handling needs a safety net).
	STABILITY_RATE = 0,
	-- GRIP / ALIGNMENT ASSIST (rad/s base, airspeed-scaled). Research §v2: great arcade-sim
	-- flight = "force-based core + a grip/alignment assist". This rotates VELOCITY toward the
	-- nose in EVERY attitude so the bird flies where it points — point up, you climb (with lift),
	-- not stall. THE fix for the recurring "not enough lift / too much stall". Per-profile
	-- sustainedTurnRate adds the eagle-wide/crow-tight turn tightness ON TOP in banked turns.
	-- Higher = more "on rails" (toward noclip); lower = more loose/aerodynamic. PLAYTEST-PROVISIONAL.
	GRIP_RATE = 2.2,
	-- PHUGOID DAMPING (vertical-motion stabilizer, studs/s of vertical accel per stud/s of
	-- climb rate). A high-L/D wing (the eagle glides at L/D ~34) has an almost UNDAMPED
	-- phugoid: lift responds to speed, so the bird slowly trades altitude<->speed in a ~3s
	-- oscillation -> visible porpoising/bobbing, worst while flapping (overspeed balloons it).
	-- Real airframes shed this via pitch damping; here it's an aerodynamic force opposing
	-- vertical velocity, scaled by dynamic pressure so it fades at low speed and never makes
	-- free lift at rest. RELEASED by active pitch input and disabled while diving, so
	-- intentional climbs and the dive/stoop keep full authority. Higher = stiffer, more
	-- "on-rails" glide; 0 = pure (porpoising) aero. PLAYTEST-PROVISIONAL.
	PHUGOID_DAMPING = 2.1,
	-- VERTICAL FLIGHT ENVELOPE (powered climb + service ceiling). Design rule (Chad, session 6):
	-- "as long as you can flap you should be able to climb" — a sustained climb should end from
	-- FATIGUE (running out of stamina) or the ceiling, NOT merely from bleeding airspeed. While
	-- actively flapping, control authority gets a floor (so you can still command the climb at low
	-- speed) and the beating wings keep making lift past the nominal stall; when stamina hits 0 you
	-- can't flap, the floors vanish, and the airspeed stall returns (the fatigue stall).
	-- SERVICE_CEILING (× per-profile climbCeilingBonus — eagle climbs higher than crow) is the top
	-- of the envelope: flap power and powered-climb lift fade to 0 over CEILING_BAND studs below it,
	-- so the bird levels off near the top instead of climbing forever. Combat happens below
	-- Thermals.maxHeight (480); the eagle ceiling (520) sits just above so the wall is only felt at
	-- the very top. PLAYTEST-PROVISIONAL.
	-- v3 REAL-SCALE: golden eagles soar/migrate at 1350–2000 m AGL (PMC9179650); 2 km ≈ 7150 studs
	-- at 0.28 m/stud. The eagle owns this tall sky (vertical/energy game); crows top out lower
	-- (climbCeilingBonus 0.7 → ~5000). The apex is reached via the powered climb + thermals over
	-- several energy cycles, not in one flap — that grind IS the energy game.
	SERVICE_CEILING = meters(2000), -- ≈7143 studs; envelope top for climbCeilingBonus = 1.0 (the eagle)
	CEILING_BAND = meters(170),     -- ≈607 studs below the ceiling over which flap power fades to 0
	FLAP_AUTHORITY_FLOOR = 0.6, -- min control authority while flapping (lets you hold a slow climb without tumbling)
	FLAP_LIFT_FLOOR = 0.55,     -- fraction of clMax lift retained past stall WHILE FLAPPING (anti-mush)
	-- FLAPPING IN A DIVE: flap thrust stays a useful accelerator through low/mid dive speed but
	-- naturally fades at the top of a stoop (you can't beat your wings fast enough at speed) —
	-- gravity still carries the dive. Full thrust until (1 - FADE_BAND) of diveSpeedCap, then
	-- ramps down to FLAP_DIVE_FLOOR by the cap. Level/climb flapping is untouched (no regression).
	FLAP_DIVE_FADE_BAND = 0.4, -- top fraction of diveSpeedCap over which dive flap thrust fades out
	FLAP_DIVE_FLOOR     = 0.1, -- min dive flap thrust fraction retained at the very top end
	-- AEROBATIC FREEDOM: above AEROBATIC_MIN_SPEED, an aggressive pitch/roll command suppresses the
	-- weathervane so the nose can swing away from the airflow — this is what lets a fast eagle pull a
	-- full LOOP or barrel roll instead of being auto-leveled out of it. Ramps in over
	-- AEROBATIC_SPEED_BAND studs/s above the min; hands-off/slow flight keeps full stability (so it
	-- can never reintroduce the level-flight porpoising). PLAYTEST-PROVISIONAL.
	AEROBATIC_SPEED_BAND = 40,
	-- OBSTACLE COLLISION (BirdCollision, queue #4). Birds are CFrame-driven so they have NO
	-- native Roblox collision — the controllers sweep a Spherecast along each step instead.
	-- CRASH_SPEED is the CLOSING speed (studs/s, into a surface normal) at or above which a
	-- map impact is lethal; below it the bird slides along the surface (graze a spire, brush
	-- the ground) instead of dying. This is a flight==balance lever: low value = terrain is a
	-- deadly hazard the agile crow can thread but the fast wide-turning eagle clips in a turn
	-- fight. PLAYTEST-PROVISIONAL.
	CRASH_SPEED = 45,
}

--------------------------------------------------------------------------------
-- PER-CLASS FLIGHT + COMBAT PROFILES
-- FlightPhysics.new(profile, position) consumes a Profiles entry verbatim.
-- Keep these two profiles structurally identical (same keys) so the engine is
-- class-agnostic. ALL asymmetry lives in these numbers.
--------------------------------------------------------------------------------
GameConfig.Profiles = {
	--========================================================================
	Eagle = {
		displayName = "Eagle",

		-- AERODYNAMICS -------------------------------------------------------
		mass = 16,              -- raised session 7: speed accel/decel felt too twitchy; mass = momentum (thrust/mass & drag/mass both gentler -> energy fighter retains energy). Lift is mass-independent so trim/stall unchanged.
		wingLoading = 1.0,      -- HIGH: fast, wide turns, strong glide/dive (reference = 1.0)
		-- v2 LIFT CURVE: CL(a) = cl0 + liftSlopePerDeg*a, capped at clMax at the stall angle,
		-- then a forgiving roll-off to postStallCl over stallPadDeg. cl0 > 0 means the eagle
		-- makes lift at ZERO AoA, so level cruise (~130) is the hands-off trim instead of the
		-- old knife-edge stall. Calibrated so stall speed (~65) sits below spawn (0.9*glide=117)
		-- and cruise (130). See FlightPhysics.computeCL. PLAYTEST-PROVISIONAL.
		cl0 = 0.30,             -- baseline lift at zero AoA (THE fix for "not enough lift")
		liftSlopePerDeg = 0.11, -- lift-curve slope (~= 2*pi per radian, thin-airfoil theory)
		clMax = 1.95,           -- peak lift (raised 1.6->1.95: "need more lift authority" — climbs/turns harder before stall)
		postStallCl = 0.70,     -- lift retained past the stall (a recoverable mush, not a cliff)
		stallPadDeg = 6,        -- degrees beyond stallAngle over which lift rolls off
		parasiticDrag = 0.020,  -- low parasitic drag -> great glide/dive
		inducedDragK = 0.58,    -- induced-drag coefficient. CD_i = k·CL² so a hard pull bleeds energy ∝ load-factor²
		                        -- (RESEARCH §v4 drag polar — the principled EM cost). 0.85->0.65->0.58: the eagle is the
		                        -- ENERGY fighter and should RETAIN energy through a pull, but the n² cost is deliberately
		                        -- NOT removed — pitching still costs energy, just no longer "way too much".
		stallAngleDeg = 27,     -- critical AoA before lift rolls off (24->27: pull harder before it lets go)
		stabilityRate = 1.0,    -- weathervane strength multiplier (moot while STABILITY_RATE = 0)
		recoverNoseDownRate = 0, -- past-stall auto-recovery DISABLED (no auto-level; nose stays where pointed)

		-- THRUST / SPEED -----------------------------------------------------
		-- flapThrust must overcome cruise drag so the eagle can hold level flight instead of
		-- decaying into a stall. At 130 cruise, drag decel ≈ 41 studs/s²; old 380/mass≈32 was
		-- short, so level flight bled speed until it stalled. 520/12≈43 lets it sustain/slowly
		-- gain at cruise. It still trades altitude for top speed (energy fighter) — the dive,
		-- not the flap, is its speed source. PLAYTEST-PROVISIONAL.
		-- P2 (player ask 2026-06-30 "more power for flaps"): flapThrust 600->800, flapClimbForce 400->500.
		-- Flap power is the eagle's ENERGY ENGINE — more of it = faster re-acceleration after a turn/zoom =
		-- stronger boom-and-zoom (harder for crows to catch). Punch comes from THRUST, not lightness — mass is
		-- deliberately kept at 16 (momentum / energy-retention = the eagle identity). The 1-v-4 counter-levers
		-- live with the CROWS (numbers, possession, formation, collision trades) — Crow flap is NOT auto-scaled.
		-- (Option B — a wingbeat-phase "felt surge" per downstroke — is the juicy follow-up to prototype live;
		-- kept as continuous force here so it's safe to ship UNPLAYTESTED. Watch for porpoising if you add it.)
		flapThrust = 900 * GRAV_SCALE,       -- forward force from a flap (800->900: "flapping should make you go a lot faster" — 2026-06-30)
		flapClimbForce = 500 * GRAV_SCALE,   -- raised 400->500 (P2: "more power for flaps" -> stronger climb). Base was
		                        -- ABOVE gravity (196); ×GRAV_SCALE keeps it above the new (lower) gravity by the same
		                        -- ratio, so a flapping eagle keeps climbing even after a
		                        -- steep climb has bled its airspeed (lift ~0) — a real eagle muscles up
		                        -- on wingbeats, not airspeed. "Flap = climb" until stamina/ceiling. THE
		                        -- climb-rate knob; mass-independent so the momentum tuning doesn't sap it.
		-- FLAP ENERGY RETENTION (player ask 2026-06-30, "BF-109 energy retention"): fraction of the
		-- drag bleed cancelled while actively flapping in level/climb flight (see FlightPhysics drag
		-- block). 0.6 = the eagle keeps ~60% more of its energy under power, so it HOLDS/builds speed
		-- through a soft (~14°) climb instead of washing out — the energy-fighter identity, dialled up.
		-- Higher than the crow's on purpose (flight==balance). Stamina still drains, so climbs end from
		-- fatigue, not become free. PLAYTEST-PROVISIONAL.
		flapDragRetention = 0.6,
		-- ENERGY RETENTION (player ask 2026-06-30, "War Thunder ARCADE energy retention" — the TOP ask). The
		-- dominant speed-bleed in a CLIMB is gravity's component ALONG the flight path (GRAVITY·sinθ), NOT drag.
		-- This is the persistent, always-on (NOT flap-gated) arcade term that adds back this fraction of that
		-- along-path gravity decel WHILE CLIMBING (see FlightPhysics energy block) — so after a dive you can BOOM
		-- then ZOOM back near your start altitude if you don't hard-turn, a 30° climb bleeds speed slowly, and an
		-- ascending defensive spiral over a lower-energy target becomes flyable. 0.82 = the eagle keeps ~82% of the
		-- climb energy gravity would take (RAISED 0.7→0.82, Chad 2026-07-02: "I lose energy so fast if I zoom up —
		-- need more retention"). HIGHER than the crow (0.4) on purpose — the eagle IS the energy fighter; this
		-- asymmetry is the 1-v-4 lever. Capped <0.95 with a residual cost (never noclip); dives are untouched (only
		-- cancels while climbing). Stall/stamina/ceiling still bound it. PLAYTEST-PROVISIONAL.
		energyRetention = 0.82,
		-- GLIDE COAST (Chad 2026-07-02: "going to glide mode, keep my speed a LOT longer if I'm not turning or
		-- climbing, just staying aerodynamic"). Per-sec exponential decay of speed ABOVE cruise (maxLevelSpeed)
		-- while gliding hands-off — see FlightPhysics speed-cap block. This (NOT drag) was the dominant bleed:
		-- at the old hard-coded 0.5, a 420 eagle shed ~125 studs/s² and dropped to 250 in ~2 s. 0.15 = the excess
		-- decays ~3.3× slower, so an aerodynamic eagle COASTS (420→250 now ~7-8 s) — the boom-and-zoom holds its
		-- energy. LOWER = holds speed even longer (0 = never bleeds below terminal). Higher than crow's on purpose
		-- (energy-fighter identity, flight==balance). Stall/turn/climb still cost energy normally. PLAYTEST-PROVISIONAL.
		glideBleed = 0.15,
		glideSpeed = 130,       -- comfortable cruise (high)
		maxLevelSpeed = 170,
		diveThrustBonus = 1.6,  -- thrust multiplier while diving (gravity also assists)
		diveSpeedCap = 390,     -- RAISED 320→390 (Chad 2026-07-02): a pure dive now tops ~390 (was 320). Flapping OUT
		                        -- of the dive can push past it to TERMINAL_VELOCITY=420 (the true top). Crows (diveSpeedCap
		                        -- 220) still can't match it — the eagle's energy/dive dominance IS the 1-v-4 identity.

		-- TURN / CONTROL (TWO turn params — see RESEARCH caveat) -------------
		rollRate = 3.2,         -- rad/s instantaneous roll authority (banking)
		pitchRate = 2.0,        -- rad/s pitch (lowered session 7: nose-down felt too fast; mouse adds analog fine control)
		yawRate = 1.1,          -- rad/s head-led yaw (slow; turns are made by banking)
		sustainedTurnRate = 1.0,-- rad/s sustained level turn ceiling (LOW: wide turns)
		rollInertia = 0.55,     -- 0..1 sluggishness; higher = heavier-feeling controls

		-- ENERGY / STAMINA (flapping & hard manoeuvres cost stamina) ---------
		-- P3 (player ask 2026-06-30 "more stamina for flapping" — a deliberate shift: longer LEASH, not just
		-- punch): budget widened again. 320 cap / 5 cost ≈ 64s of sustained full-throttle flapping (was 240/6
		-- ≈ 40s), regen 16. Stamina STAYS the limiter (boom-and-zoom identity survives): with P2's stronger
		-- flap this must end from fatigue or the ceiling, not become "free" power. Eagle-only — Crow stamina is
		-- a 1-v-4 lever left for the matchup pass. Suits the eagle's energy-fighter / endurance identity.
		maxStamina = 320,       -- raised 240->320 (P3 "more stamina"); flap throttle scales cost by effort so partial throttle lasts even longer
		staminaFlapCost = 5,    -- per second at FULL up-throttle (6->5: longer leash); scaled by |throttle| in FlightPhysics
		staminaDiveCost = 4,    -- per second of powered dive / down-throttle
		staminaRegen = 16,      -- per second when gliding (14->16: recovers a touch faster between climbs)
		climbCeilingBonus = 1.0,-- relative climb ceiling (eagle climbs higher)

		-- COMBAT / ENDURANCE -------------------------------------------------
		health = 99,            -- 3 clean "hits": the discrete combat model (GameConfig.Combat.eagleHits)
		                        -- makes one UNPARRIED crow ram cost health/eagleHits = 33, so exactly
		                        -- 3 rams down the eagle. HP bar retained (reads as 3 chunks).
		beakDamage = 26,        -- close peck. RAISED 22→26 (S17): crow health is 25, so the old 22 could NOT
		                        -- kill a crow in one clean beak jab (left it at 3 HP) — that was the core of Chad's
		                        -- "I'm hitting them with the beak but they don't die." A committed head-on beak
		                        -- (narrow 35° cone, short 2s window, long 4s cd) now one-shots a crow like the talon
		                        -- does; the 1-v-4 balance stays in the saturation (one zone at a time), not in the
		                        -- beak being sub-lethal. flight==balance: still < talon; dial back toward 25 if too strong.
		talonDamage = 34,       -- grab/strike, the bread-and-butter eagle kill (one-shots a 25 HP crow)
		diveAttackMultiplier = 3.46, -- RESEARCH: stoop ~3.46x level-flight lethality
		-- Collision: eagle is the heavy body; it survives trades better than a crow.
		collisionDamageDealt = 60,   -- damage IT inflicts on whatever bird it rams (full-speed)
		-- Vulnerability multiplier applied to the OTHER bird's incoming ram
		-- (see GameServer.processCollisions). Eagle is the heavy body: a full-speed crow
		-- ram (crow dealt 30) does 30*0.85 ≈ 26 to it, so ~4 rams kill the 100-HP eagle —
		-- the documented trade. PLAYTEST-PROVISIONAL (collision tuning = queue item #3).
		collisionDamageTaken = 0.85,
		bodyRadius = 4.5,           -- collision/attack reach (studs)
	},

	--========================================================================
	Crow = {
		displayName = "Crow",

		-- AERODYNAMICS -------------------------------------------------------
		mass = 3.0,
		wingLoading = 0.55,     -- LOW: more lift accel -> lower stall speed + tighter turns
		-- v2 LIFT CURVE (see Eagle for the model). Crow has higher clMax + later stall so it can
		-- point its nose harder and out-turn the eagle; low wingLoading gives it a very low stall
		-- speed (~47 < spawn 0.9*glide=85 < cruise 95). PLAYTEST-PROVISIONAL.
		cl0 = 0.30,             -- baseline lift at zero AoA
		liftSlopePerDeg = 0.11, -- lift-curve slope
		clMax = 1.6,            -- higher peak lift -> harder instantaneous turn (angles fighter)
		postStallCl = 0.90,     -- very forgiving post-stall (twitchy but recoverable)
		stallPadDeg = 8,        -- gentle, wide stall roll-off
		parasiticDrag = 0.045,  -- higher drag -> bleeds energy faster, worse glide/dive
		inducedDragK = 0.55,    -- cheaper to turn (pays less induced drag per unit lift)
		stallAngleDeg = 24,     -- stalls later -> can point nose harder without dropping
		stabilityRate = 0.7,    -- weathervane multiplier (moot while STABILITY_RATE = 0)
		recoverNoseDownRate = 0, -- past-stall auto-recovery DISABLED (no auto-level)

		-- THRUST / SPEED -----------------------------------------------------
		flapThrust = 185 * GRAV_SCALE,       -- 150->185 (player ask 2026-06-30: crows should "maintain speed when climbing some" too — modest; level cap 140 < eagle 170 still protects the matchup)
		flapClimbForce = 200 * GRAV_SCALE,   -- lower than the eagle: the crow is less of a climber (lower ceiling via climbCeilingBonus)
		-- FLAP ENERGY RETENTION (see Eagle). LOWER (0.4 vs 0.6): the crow can hold speed through a gentle
		-- climb "some" (player ask) but the eagle stays the superior energy fighter — the crow's game is
		-- the tight turn + the ram, not out-running/out-climbing the eagle. flight==balance. PLAYTEST-PROVISIONAL.
		flapDragRetention = 0.4,
		-- ENERGY RETENTION (see Eagle). LOWER (0.4 vs 0.7): the crow holds SOME climb energy so a squad can
		-- reposition vertically, but the eagle stays the superior energy fighter — the crow can't out-zoom or
		-- sustain the ascending spiral, so its game remains the tight turn + the ram. flight==balance. PROVISIONAL.
		energyRetention = 0.4,
		-- GLIDE COAST (see Eagle). Kept at the OLD default 0.5 (vs the eagle's 0.15): the crow's cruise stays
		-- tight — it does NOT coast far above its 140 level cap, so it can't hold a boom-and-zoom speed like the
		-- eagle. Structural symmetry + the eagle's coast advantage is a flight==balance lever. PLAYTEST-PROVISIONAL.
		glideBleed = 0.5,
		glideSpeed = 95,
		maxLevelSpeed = 140,    -- slightly slower top end than eagle in level flight
		diveThrustBonus = 1.25,
		diveSpeedCap = 220,     -- much lower dive cap: cannot win a vertical energy fight

		-- TURN / CONTROL -----------------------------------------------------
		rollRate = 5.0,         -- very fast roll (swarm agility)
		pitchRate = 3.6,
		yawRate = 1.8,
		sustainedTurnRate = 2.1,-- HIGH sustained turn: wins the prolonged turn fight
		rollInertia = 0.20,     -- light, twitchy controls

		-- ENERGY / STAMINA ---------------------------------------------------
		maxStamina = 70,
		staminaFlapCost = 6,
		staminaDiveCost = 5,
		staminaRegen = 9,       -- regenerates fast
		climbCeilingBonus = 0.7,-- lower climb ceiling than eagle

		-- COMBAT / ENDURANCE -------------------------------------------------
		health = 25,            -- 4 crows ~= 1 eagle of HP
		beakDamage = 9,
		talonDamage = 12,
		diveAttackMultiplier = 2.0,  -- crows dive too, but it is not their identity
		-- The SACRIFICE: a crow ramming at full speed deals 30 to whatever it hits, and dies.
		-- It absorbs the eagle's heavy body (eagle dealt 60) * 1.4 = 84 >> 25 HP, so a ram is
		-- a true trade — ~4 crow rams down an eagle, and each ramming crow dies in the process.
		-- The eagle avoids the math by out-positioning and picking off stragglers.
		collisionDamageDealt = 30,
		collisionDamageTaken = 1.4,  -- >1: fragile body; a full-speed hit is fatal (it trades)
		bodyRadius = 2.2,
	},
}

--------------------------------------------------------------------------------
-- COMBAT — the asymmetric talon/beak ↔ suicide-ram model (HANDOFF session 11).
--------------------------------------------------------------------------------
-- The contract (see docs/HANDOFF.md + .loop/talon-beak-combat/state.md):
--  • Eagle survives `eagleHits` UNPARRIED crow rams; one ram = health/eagleHits damage.
--  • A Crow's only attack is the suicide ram: a committed eagle↔crow contact ALWAYS
--    kills the crow, and costs the eagle one hit UNLESS the eagle was covering that
--    contact zone with the right extended weapon (talons/beak). [zone/parry = iter 3]
--  • Eagle wins: all crows dead & eagle alive.  Crows win: eagle takes its 3rd hit
--    (≥1 crow survives).  Tie: the eagle's last hit and the last crow's death coincide.
GameConfig.Combat = {
	eagleHits = 3,   -- unparried crow rams the eagle survives (1 ram = health/eagleHits)

	-- STAGE 7 STRIKE TELEMETRY (dev instrument; DEFAULT false = byte-identical to today). Arm in a SOLO
	-- Studio bot-fight to attribute the low contact-rate ("hard to hit them / they don't hit me") to a
	-- MECHANISM before any combat number moves. When true it batch-logs (~1 Hz) every eagle-strike frame
	-- (hit AND clean miss: dist/reach/axisDot/cone/window) + every eagle↔crow closest-approach ("MERGE":
	-- minSep, relAtMin, gatePass, CONTACT/MISS). Read the rows against the decision table in HANDOFF.
	-- Do NOT arm in a live round (the near-miss scan adds frame work). Fully inert — no scan, no
	-- allocation — when false. Server-side only: touches no control law, no camera, no damage number.
	strikeTelemetry = false,  -- (S30 capture DONE — attribution logged. Re-arm to true on sandbox to validate a fix.)

	-- DIRECTIONAL STRIKE (eagle only) — REWORKED 2026-06-30 (Chad): drop the left/right talon dichotomy. There
	-- are now just TWO zones, and the GAME auto-picks which strike a LEFT-CLICK throws by which zone the nearest
	-- enemy is in (the proximal reticle makes the strike — GameServer.enemyZone), each with its own
	-- startup→active duration→cooldown and a coverage cone:
	--   • TALON — a wide BELLY zone (cone around the bird's DOWN/-Up axis, so it SWINGS as you roll/bank — the
	--             talon arc reticle shows it). LONG active window, SHORT reset: the bread-and-butter grab when a
	--             crow is below / below-to-the-side. (Chad: ~4 s effective duration, ~2 s cooldown.)
	--   • BEAK  — a narrow FORWARD zone (cone around the nose; the beak/boresight reticle). SHORT active window,
	--             LONG reset: a committed head-on jab. (Chad: ~2 s duration, ~4 s cooldown.)
	-- During a strike's ACTIVE window (past startup) the zone's cone BOTH parries an incoming crow ram (crow
	-- dies, eagle unharmed — tryStrikeCatch) AND deals offensive damage to a crow inside the cone within reach
	-- (the eagle can HUNT). ONE catch-or-hit per window. The client mirrors the picked zone live
	-- (Body attr StrikeArmedDir "beak"/"talon"/"none") to LIGHT the matching reticle.
	--
	-- WHY THIS IS BALANCED (1-v-4 — preserve this reasoning): the eagle presents ONE zone at a time (belly OR
	-- nose) → it covers/strikes ONE angle per commitment → four crows from spread angles still win by
	-- SATURATION (the whole asymmetry). ⚠️ WATCH (flight==balance): the talon's LONG active window (4 s active /
	-- 2 s cd ≈ 67% uptime) flirts with the research >50% single-angle "always-on" guardrail. If 1-v-4 turns
	-- eagle-favored, cut the talon duration / widen its cooldown / drop the offensive damage (or zero the
	-- offensive pass → defense-only) FIRST. Server owns every timer + the zone pick (anti-cheat): the client
	-- sends only a fire EDGE, never a direction. ALL PLAYTEST-PROVISIONAL (durations/cooldowns are Chad's asks).
	bankStrikeThresholdDeg = 18,  -- LEGACY (no longer used; the strike zone is picked by enemy proximity, not bank)
	strikeSelectRange = 200,      -- studs; an enemy within this range drives the zone pick + reticle lighting
	-- TALON-PICK BIAS (S31, Chad "the crows are too good at flying, I can't get 'em"): the auto-pick
	-- (GameServer.enemyZone) chose the narrow 35° BEAK 100% of the time (Stage-7) and the wide 70° belly
	-- TALON workhorse was NEVER selected, so strikes whiffed on crows arcing across the front-low quadrant.
	-- This bias is ADDED to the belly(-Up) side of the "more-below-vs-more-ahead" comparison, so front/side-
	-- low crows now arm the 70° talon (which also SWINGS with bank onto a crossing target) instead of the beak.
	-- A genuinely head-on crow (LookVector dot dominant) still picks the beak jab. IDENTITY = 0.0 (byte-for-byte
	-- today); shipped at 0.35 for Chad to fly. ⚠️ flight==balance (red-team): s.stype is locked for the whole
	-- 4s talon window, so this also arms the wide belly cone for the DEFENSIVE PARRY more often — it pushes on
	-- the talon's >50%-always-on guardrail. JUDGE BY: landed hits on maneuvering crows (NOT pick-share) AND
	-- whether 4 crows can still saturate/land front-low rams. Too eagle-favored → drop toward 0.2 (or cut talon
	-- duration FIRST per the guardrail note below). Barely helps → raise toward 0.5. Revert: set to 0.0.
	-- AUTO-STRIKE (S31 accessibility, Chad "getting a hit is near impossible… a kid won't time clicks"):
	-- when true, the eagle's strike auto-fires the instant a crow is genuinely inside the auto-picked
	-- zone's cone AND within reach (GameServer.updateEagleStrikes) — a new player lands hits without
	-- frame-perfect timing; the manual LMB click still works. Cooldown + one-hit-per-window unchanged, so
	-- it's not a death-ray. This was the REAL wall (both design consults): reach/pick couldn't matter while
	-- landing a hit needed a perfectly-timed click on a briefly-crossing crow. IDENTITY = false (today).
	-- ⚠️ this is the assist that most reduces the skill floor — likely a "kid tier" default; keep it OFF for
	-- a ranked/expert tier when the difficulty-tier config lands. Revert: set to false.
	autoStrike = true,
	talonPickBias = 0.35,
	-- STRIKE REACH BUFFER (S31, Chad "still too hard to get them, they fly in circles and get out of the
	-- way, I need a bigger buffer to strike with"): extra studs added to EVERY strike's offensive reach
	-- (GameServer.updateEagleStrikes). This is the "buffer" — a bigger sphere so a crow circling just past
	-- your nose still gets clipped. It affects ONLY the eagle's offensive hunt: the parry (tryStrikeCatch,
	-- cone-only at body contact) and the ram are UNTOUCHED, so this does NOT change crow lethality. IDENTITY
	-- = 0 (byte-for-byte today: talon reach 24 / beak 28). Shipped at 12 (→ talon 36 / beak 40) for Chad to
	-- fly. ⚠️ flight==balance: reach is THE documented "death-ray" lever — a bigger reach means the eagle
	-- kills circling crows from farther, so fewer survive to mob. JUDGE IN 1-v-4: can you catch them now AND
	-- can 4 crows still gang up? Too easy / crows evaporate → drop toward 6-8. Still can't reach → raise toward
	-- 18. If it turns eagle-favored, trim this BEFORE touching cones/damage. Revert: set to 0.
	strikeReachBonus = 12,
	-- ⚠️ STRIKE REACH (S17): the offensive hunt reach was `(rEagle+rMax+ATTACK_BUFFER)*reachFraction` ≈
	-- (4.5+4.5+3)*0.85 ≈ 10.2 studs — barely past the 6.7-stud TOUCHING distance, i.e. you had to nearly
	-- collide to land a strike. Meanwhile the ARMED reticle lights at strikeSelectRange (200 studs), so a
	-- crow read as "on target" from 20× outside actual hit range → Chad: "pretty sure I'm hitting them but
	-- they don't die." Each strike now carries an explicit `reachStuds` (a believable swoop-and-strike range);
	-- if set it OVERRIDES the point-blank formula (GameServer.updateEagleStrikes). Cones/timers UNCHANGED, so
	-- the 1-v-4 saturation balance holds (one zone at a time, one hit per window). flight==balance: this is the
	-- eagle's EARNED hunt reach; if it turns eagle-favored, drop reachStuds / beakDamage / talon uptime FIRST.
	strikes = {
		-- TALON: wide belly cone (swings with bank), long active window + short reset — the workhorse grab.
		-- reachStuds kept CONSERVATIVE (24): the talon already one-shots + has a 70° cone at ~67% uptime, so a
		-- big reach here would be a death-ray (red-team #2). 24 is still a huge lift from the old ~10.2 point-blank.
		talon = { startup = 0.10, duration = 4.0, cooldown = 2.0, axis = "belly",   coverHalfAngleDeg = 70, damage = "talon", reachFraction = 0.85, reachStuds = 24 },
		-- BEAK: narrow forward cone, short active window + long reset — the committed head-on jab. Slightly LONGER
		-- reach (28) than the talon: the narrow 35° cone + short 2s window make it hard to land, so a committed
		-- head-on jab needs the range to actually connect — this is the strike Chad struggled with most.
		beak  = { startup = 0.10, duration = 2.0, cooldown = 4.0, axis = "forward", coverHalfAngleDeg = 35, damage = "beak",  reachFraction = 0.85, reachStuds = 28 },
	},

	-- ▶ CONTACT JUICE (S31, phase 1) — additive CLIENT FX on server-detected contact. The server only
	-- FIRES a CombatFX event (GameServer.applyDamage); ALL particles/pose run client-side and touch NO
	-- damage number, NO flight kernel, NO camera composition law (CS-1..CS-9 safe). Feathers + the strike
	-- wing-pose ship live (Increment 1); talon leg-extension / camera kick / crow tumble are queued INERT
	-- for Increment 2. Cheap in a swarm: pooled emitters, distance-gated, zero per-hit Instance creation.
	fx = {
		enabled       = true,   -- master switch for client contact FX (false = byte-identical to pre-S31)
		range         = 500,    -- studs; a client skips FX beyond this from its camera (swarm cost guard)
		-- Feather burst sizes (ParticleEmitter:Emit count) by event — Chad "there should be LOTS of feathers":
		featherCrowDeath = 42,  -- a crow KILLED (the payoff burst — big cloud you can't miss)
		featherStrike = 22,     -- a strike lands but doesn't kill (glancing puff)
		featherRam    = 40,     -- a crow ram lands on the eagle (big burst = "you got hit")
		-- S41: the fx envelope is now the BRIEF IMPACT ACCENT (the "smack" scuff at contact instant).
		-- The feathers are now discrete PARTS — GameConfig.Rescue.featherParts + FeatherBurst.
		featherSize   = 1.6,    -- small scuff puff — the accent, not the read (feathers are parts now)
		featherLifeMin = 0.4, featherLifeMax = 0.6,  -- s; a BLINK — the smack, gone before the feather ring peaks
		featherSpeed  = 18,     -- studs/s; sharp fast opening — reads as impact, not a hang
		featherGravity = 2,     -- irrelevant over 0.5s; near-zero
		-- STRIKE ATTACK POSE (client, attribute-driven off Body StrikeActive/StrikeArmedDir; wing pose
		-- only — never the physics Body root). Wings cup up/forward into a flare while a strike is armed.
		strikePose        = true,
		strikePoseWingDeg = 34,  -- deg of extra wing dihedral raised during an armed strike; 0 = no pose
		-- CROW SPIRAL-DEATH (S31, Chad "the bird spirals down dead"): a killed crow tumbles/spirals down
		-- for tumbleDuration s before despawning, instead of vanishing — so a kill READS. Server-driven
		-- (the model is anchored + CFrame-animated). true = on.
		crowTumble        = true,
		tumbleDuration    = 1.4,   -- s the dead crow spirals down before it despawns
		tumbleSpin        = 9,     -- rad/s tumble spin rate
		-- INCREMENT 2 (still queued, INERT):
		talonExtendDeg    = 0,   -- deg the legs swing tucked -> extended on a TALON strike (needs leg Motor6Ds)
		cameraKickDeg     = 0,   -- additive camera kick on taking a ram (0 = off; camera-adjacent, Chad-flies)
	},
}

--------------------------------------------------------------------------------
-- SQUAD / FORMATION (Crow only) — see RESEARCH: tightness is the core risk knob.
--------------------------------------------------------------------------------
GameConfig.Squad = {
	size = 4,
	-- Boids weights (Reynolds: separation / alignment / cohesion) for AI crows.
	boids = {
		separationWeight = 1.5,
		alignmentWeight = 1.0,
		cohesionWeight = 1.0,
		-- GOAL (eagle-chase) weight — the MOBBING lever. Previously the goal term silently
		-- reused cohesionWeight (1.0), so at close range separation (1.5×separationMul, up to
		-- 2.1 in loose) OVERPOWERED the eagle-chase and the crows scattered instead of swarming
		-- (playtest: "they get scared and fly away"). Set ABOVE separation so the 3 AI crows
		-- close in and pressure the eagle from multiple angles; separation still spreads them
		-- enough to surround rather than pile onto one point. Tune live: raise to mob tighter,
		-- lower toward 1.0 if they clump/collide. flight==balance: this is a 1-v-4 pressure knob.
		-- 2.0 with the orbit ON (Squad.mob.enabled). Must beat separation (up to 2.1 in loose) so the orbit
		-- RING binds the crows to the eagle instead of them drifting off; SAFE to be strong because the goal
		-- is the shell, not the eagle centre (strong = follow-the-ring, not ram). Raise toward 2.5 if they
		-- still "flock off"; lower toward 1.5 if they clump on the ring.
		-- (Prior note:) Now SAFE to raise (2.0) because the aggressive-squad goal is no longer the eagle's CENTRE — it's
		-- an ORBIT-SHELL station point (see Squad.mob + GameServer mobOrbitTarget). So goal-seek being
		-- strong makes crows FOLLOW THE SHELL / circle, not ram-and-crash. (Cranking it while the goal was
		-- the eagle centre is what made 3 crash — 4 birds on one vector. Research: mobbing≠flocking, crows
		-- are independent attackers that orbit+dive-bomb, cohesion DROPS. docs/RESEARCH-crow-mobbing.md.)
		goalWeight = 2.0,
		neighborRadius = 60,     -- studs; local perception
		neighborFovDeg = 270,    -- field-of-view angle of the perception cone
		maxSteerForce = 220,
	},
	-- Formation presets: spacing is the tight<->loose knob.
	-- TIGHT: protected + concentrated offense, but catastrophic dive/collision exposure.
	-- LOOSE: coverage + resilience, but exposes stragglers the eagle hunts.
	formations = {
		tight = { spacing = 8,  cohesionMul = 1.6, separationMul = 0.7, collisionRisk = 1.5 },
		loose = { spacing = 26, cohesionMul = 0.8, separationMul = 1.4, collisionRisk = 0.7 },
	},
	defaultFormation = "loose",
	-- How a non-possessed crow supports: it mirrors the squad heading and converges
	-- on the possessed crow's target when within engageRadius.
	engageRadius = 45,
	swapCooldown = 0.35,         -- seconds between possession swaps (anti-spam)

	-- OBSTACLE AVOIDANCE (AI crows only — queue #8). A forward Spherecast look-ahead steers the
	-- bird around spires/ground so it stops attriting itself on terrain (which let bot waves
	-- crash before reaching the eagle). Applied ONLY in updateAICrows, so it never affects the
	-- player-controlled crow (Chad's rule: the crow you fly into the eagle gets NO avoidance).
	-- GENTLE avoidance (re-tuned 2026-06-30 after the strong version made bots FLEE the map — player: "the AI
	-- crows fly away from me… 18-20 km away; they should be mobbing me"). The real anti-suicide guarantee is
	-- the NON-LETHAL AI-crow terrain slide in updateAICrows (a bot can't die on a building), so avoidance only
	-- needs to be a light nudge that peels a crow off a wall it's about to graze — NOT a hard override that
	-- fights the eagle-chase and sends it veering off into open sky. Weight back to 3.0, shorter lookahead, and
	-- NO urgency floor (far-away terrain ⇒ ~0 avoidance), so the chase goal dominates and the swarm closes in.
	-- (The PLAYER-flown crow still gets no avoidance and real terrain crashes — Chad's rule.)
	avoidance = {
		lookaheadTime = 1.5,  -- s; look-ahead distance = speed * this
		minDistance   = 60,   -- studs floor on the probe distance
		maxDistance   = 350,  -- studs ceiling
		spherePad     = 10,   -- extra studs of body radius for a wider berth
		weight        = 3.0,  -- light nudge off imminent terrain — does NOT dominate the eagle-chase
	},

	-- AI FLIGHT GOVERNOR (all AI crows) — INCREMENT 0: make them FLY CONTROLLED before any mob geometry.
	-- Playtest diagnosis (2026-07-01): crows fly "too fast — blow past, can't turn tight", then "fine, then
	-- suddenly crash" into "terrain/spires AND the ground". Root cause: the boids input DIVES at the target
	-- (dir.Y < -0.4 → dive → accelerates to the 220 dive-cap), so turn radius (v/ω) balloons and they overshoot
	-- into terrain / sink into the ground on the pull-out. Fix = keep them near a controllable combat speed and
	-- OUT of dives (diving is the eagle's game, not the crow's mob), + a soft altitude floor so a hard bank
	-- doesn't sink them into the deck. Applied to every AI crow in updateAICrows AFTER steeringToInput.
	aiFlight = {
		enabled       = true,
		combatSpeedMul = 1.35, -- orbit/cruise speed cap = Crow.glideSpeed × this (95×1.35≈128). Above it, stop
		                       -- flapping to bleed (parasitic drag) → tighter turns, no overshoot.
		                       -- RAISED 1.05→1.35 (S19): the S18 cut to ~100 was the WRONG direction — the eagle cruises
		                       -- 130 (max 170), so crows at 100 could never stay with it → "mice and cat, I chase them."
		                       -- ~128 (just under the eagle's cruise + the crow's own 140 maxLevelSpeed cap) lets them
		                       -- press and stay on the eagle. Holdable on the 150 ring now that the bank cap is 72°:
		                       -- required bank atan(128²/(150·45.6))≈67° < 72° cap (≈5° margin). The crow's real edge is
		                       -- the TURN (2.1 rad/s) + dive, not level speed (it can't out-run the eagle) — so this keeps
		                       -- it close enough to convert to a turning/dive fight. ⚠️ if they still can't keep up when
		                       -- you run, the lever is a faster dive-bomb pass (raise Crow.maxLevelSpeed), not more cruise.
		suppressDive  = true,  -- crows never DIVE in the mob (that's what builds the runaway overshoot speed).
		floorHeight   = 250,   -- studs; below this the crow is forced to climb (flap + nose up) so it can't sink
		                       -- into flat ground while banking. (Spires are handled by horizontal avoidance.)
		floorClimbPitch = 0.5, -- nose-up pitch applied under the floor (+ = climb, per steeringToInput sign).

		-- AI AUTOPILOT — bank limit + turn coordination (fixes "crash when chased, losing control").
		-- The flight engine has NO auto-level (the eagle's locked feel; crows inherit it), so when the
		-- player chases, a crow rolls hard to reverse, reaches knife-edge, loses vertical lift and falls.
		-- A human corrects this; the bot can't. This AI-ONLY assist caps the bank and adds up-elevator in
		-- the turn so it stays controllable. Does NOT touch the player's no-auto-level (AI loop only).
		autopilot     = true,
		maxBankCos    = 0.40,  -- cos(bank limit). 0.40 ≈ 66° max bank; below this uprightness, leveling takes over.
		                       -- RAISED 0.5→0.40 (60°→66°) S19: the 60° cap was THROTTLING the crow's designed turn
		                       -- advantage (Crow.sustainedTurnRate 2.1 rad/s vs Eagle 1.0) → on perturbation they hit the
		                       -- cap and understeered OUTWARD ("flock off / won't mob"). 66° lets them carve the turn WITH
		                       -- the eagle. Paired with orbitRadius 180 so the ring's required bank (~63° at v≈128) sits
		                       -- UNDER 66° with the S14 knife-edge safety margin mostly intact (66°→90° = 24° vs 72° = 18°).
		                       -- ⚠️ this is the exact cap that guards the S14 falling-crash (roll-past-vertical when chased);
		                       -- if that recurs, DON'T just lower the cap (66° < the 63° the ring needs → they spiral out) —
		                       -- WIDEN orbitRadius instead so a lower bank holds. Chad OK'd "a couple crashed" if they mob.
		levelAssist   = 3.0,   -- leveling roll gain (higher = snaps back to wings-level harder).
		turnPitchUp   = 0.5,   -- up-elevator added ∝ bank so a banked turn holds altitude (coordinated turn).
	},

	-- MOBBING (aggressive bot crows only) — the biologically-grounded 1-v-4 model, INCREMENT 1.
	-- Research (docs/RESEARCH-crow-mobbing.md): real corvids don't ram a raptor — they CIRCLE it
	-- ("flying about the intruder") at a standoff distance and make staggered dive-bomb passes. So
	-- instead of goal-seeking the eagle's CENTRE (4 birds → one vector → the pile-up/crash Chad saw),
	-- each crow seeks a point on an ORBIT SHELL around the eagle and circles it (GameServer.mobOrbitTarget
	-- feeds this as the boids goalPoint). Increment 2 will add the staggered dive-bomb + peel-off passes.
	mob = {
		enabled       = false, -- UPDRAFT Phase 0: crows CALMED (no dive-bomb mobbing) for the line-riding
		                       -- flight test — combat is shelved. Flip back to true only to revisit the mob.
		                       -- (was: Increment 1 orbit. ON now that AI flight control is fixed (governor +
		                       -- autopilot → crows fly controlled, dogfight well). This binds each crow to a
		                       -- standoff ring around the eagle so they MOB instead of "flocking off and going
		                       -- far away" (playtest 2026-07-01). Needs goalWeight raised (below) to beat
		                       -- separation. See docs/HANDOFF SESSION 14.
		orbitRadius   = 180,  -- studs; standoff shell radius the crows circle at (BETWEEN dive-bomb passes). 220 (orig)
		                      -- stood off too far ("flying away"); 150 (S18) was un-holdable — the required bank exceeded
		                      -- the cap and they spiraled out. 180 is the red-team sweet spot: at the keep-up speed v≈128
		                      -- the required bank is atan(128²/(180·45.6))≈63° < the 66° cap (maxBankCos 0.40) WITH the S14
		                      -- knife-edge margin mostly restored. ⚠️ COUPLED: orbitRadius, combatSpeedMul (128) and
		                      -- maxBankCos (66°) move together — holdable iff v ≤ √(tan(bankCap)·g·R). To bring the ring
		                      -- CLOSER, first widen the bank cap or slow them, or they understeer OUT again. NOTE: the
		                      -- "in your face" pressure is meant to come from the frequent dive-bomb PASSES, not a tight ring.
		orbitHeightBias = 40, -- studs; sit slightly ABOVE the eagle (attack-from-above prep for the dive-bomb in increment 2)
		orbitLead     = 60,   -- studs; tangential lead point along the ring → how fast they circle (0 = just hold station).
		                      -- LOWERED to 60 (S19): with the ring close (150) and the eagle moving, a big lead demanded
		                      -- excess bank on top of just following him; 60 = they hold close station + gently circle
		                      -- (surround/harass) rather than race around the ring and sling outward.
		-- MOB STEERING PROFILE (S24 — the scatter fix, hypothesis pending playtest confirmation).
		-- Research (docs/RESEARCH-crow-mobbing.md): mobbing ≠ flocking — real corvids switch to
		-- METRIC/independent-attacker mode; cohesion DROPS. That reframe was wired into the goal GEOMETRY
		-- (orbit shell) but NOT the steering blend: mobbing crows still ran full alignment(1.0)+cohesion(0.8).
		-- The bot wave spawns 12 studs apart and each stations at its OWN bearing, so the squad rings one
		-- narrow sector (separation spreads them only to ~the 60-stud perception edge ≈ 19° of a 180 ring) —
		-- "surround from multiple angles" never happened. And whenever the dive-bomb scheduler sends a crow
		-- OUT (peel target beyond the ring) or the eagle runs, alignment/cohesion (up to ~1.8 combined) drag
		-- the others along vs goalWeight 2.0 — the leading suspect for the 5-playtest "they scatter and fly
		-- away" (code-verified mechanism; that it's THE cause is confirmed only by Chad's next Play). This
		-- table is passed as the boids formationCfg while mobbing (updateAICrows): alignment/cohesion OFF
		-- (independent attackers), separation kept high (research-verified). Bearings should diffuse as passes
		-- and peels re-ring crows at new bearings with nothing re-syncing them — but the TIMESCALE is unproven;
		-- if playtest still shows one-sector attacks, the pre-registered fallback is explicit per-crow bearing
		-- offsets in mobOrbitTarget (the surgical version of what the old flag promised). AI-only; player
		-- squads keep their tight/loose formation blend. (Replaces `clumpSpread`, which NO code ever read.)
		-- ⚠️ S25 SUPERSEDES this as the PRIMARY scatter cause: steeringToInput's horizontal channels were
		-- SIGN-INVERTED (Boids.luau S25 note) — crows stably flew AWAY from every goal, so they never sat on
		-- the ring long enough for this flock-drift mechanism to be the driver. Keep this profile anyway: it
		-- is the research-correct mob blend (independent attackers), just demoted from "root cause" to
		-- "secondary/contributing." Set `steering = nil` to A/B the old flocking blend in isolation.
		steering = { spacing = 30, cohesionMul = 0, alignmentMul = 0, separationMul = 1.4 },

		-- INCREMENT 2 — STAGGERED DIVE-BOMB + PEEL-OFF (the mob's first TEETH).
		-- Research (docs/RESEARCH-crow-mobbing.md): real corvids don't sit on the ring — they take
		-- TURNS committing a fast pass THROUGH the raptor, throw the pass, then PEEL OFF to recover,
		-- never all-at-once (all-4-on-one-vector = the pile-up). Structurally: ONE crow attacks at a
		-- time (squad-level round-robin in GameServer.updateAICrows), the rest hold the standoff ring.
		-- On a committed pass the attacker aims at the eagle CENTRE and gets a raised speed cap
		-- (attackSpeedMul) so it actually reaches ram range; contact = the crow's sacrifice ram (eagle
		-- takes health/eagleHits), a miss = the eagle read/dodged it. Then it PEELS out+up to recover.
		-- AI-only; the player-possessed crow is never scheduled or overridden.
		-- NOTE (do NOT overclaim): this is the mob's TEETH + a CONTROLLED close-then-break replacing the
		-- constant passive drift-into-contact. It is NOT by itself an "attrition fix" — AI-crow waves
		-- respawn only when ALL are dead, and a landed pass is a deliberate sacrifice, so crows still
		-- trade themselves. Whether "attrites to ~1" is a bug or the intended sacrifice dynamic is a
		-- Chad design call (open question in the s16 playtest). flight==balance: this is timed pressure
		-- the eagle can read and beat, not a swarm pile-up. Flip `enabled=false` to fall back to pure orbit.
		-- ⚠️ CADENCE (REVISED S19 — the "mice/cat" strategy pivot): the old rule was "one attacker at a
		--    time" (attackDuration+peelDuration ≤ attackPeriod), which made a lone pass every 3.4s while 3
		--    crows circled — so the DOMINANT behavior was orbit-circling, which reads as EVASION ("they flock
		--    off, I chase them like mice"), not mobbing. Now attackPeriod is SHORT so a new attacker launches
		--    before the last finishes → ~1-2 crows are ALWAYS diving at the eagle, from DIFFERENT ring bearings
		--    (round-robin + the mob steering profile's pass-through bearing diffusion) → constant harassment =
		--    the mob FEEL. This is NOT the old pile-up
		--    (that was all-4 on ONE vector at uncontrolled speed); it's ≤2 controlled attackers from spread
		--    angles, separation keeping them apart. Concurrent-attacker count ≈ attackDuration/attackPeriod.
		--    ⚠️ don't push attackPeriod so low that 3-4 pile on at once — keep it ≥ ~1.0. flight==balance: more
		--    passes = more ram pressure on the eagle (each landed ram = health/eagleHits); raise attackPeriod
		--    if the 1-v-4 turns crow-favored, lower it toward mob harder.
		diveBomb = {
			enabled       = true,
			attackPeriod  = 1.4,  -- s between attack LAUNCHES. SHORT (was 3.4) so ~1 crow is always mid-dive + often a
			                      -- 2nd = the eagle is under near-continuous harassment from spread angles. See cadence note.
			attackDuration = 1.6, -- s the attacker commits toward the eagle (closes the standoff for the pass)
			leadTime      = 0.35, -- CROW LEAD (S31, Chad "I can't kill them, they get away so fast"): the dive-bomb
			                      -- aims at a LED intercept (eaglePos + eagleVel·leadTime) instead of the eagle's
			                      -- CURRENT position. The old zero-lead pursuit-curve arced BEHIND a turning eagle
			                      -- and slid past at 10-25 studs (Stage-7, 7/7 miss) → crows never entered strike
			                      -- range, which read as "they get away." A led pass flies a true intercept and
			                      -- arrives AT the eagle on a readable line → your buffed reach/talon can clip it on
			                      -- the merge, AND it finally activates the parry (tryStrikeCatch, never-fired). ⚠️
			                      -- flight==balance: this RAISES crow lethality (passes now connect ≈33 dmg each) —
			                      -- that's the intent (restarts the mob's attrition clock). If 1-v-4 flips crow-favored,
			                      -- raise attackPeriod (1.4→~2.2) FIRST, do NOT lower leadTime (keep passes lethal, just
			                      -- space them). Over-leads past a turning eagle → 0.25; still slides past → 0.45.
			                      -- IDENTITY = 0 (memberGoal = eaglePos, byte-for-byte today). Revert: set to 0.
			peelDuration  = 1.4,  -- s the attacker breaks away+climbs to recover before rejoining the ring
			attackSpeedMul = 1.8, -- LIFTS the AI governor's cruise cap during the pass so the attacker flies as fast
			                      -- as the KERNEL allows and reaches ram range (vs the ~100 orbit cruise). ⚠️ It does NOT
			                      -- reach 95×1.8≈171: the attacker keeps suppressDive ON, so FlightPhysics clamps a
			                      -- non-diving crow at Crow.maxLevelSpeed=140. So the real pass tops ~140 studs/s — still
			                      -- easily crosses the tightened 150 orbit to contact. Anything above 1.47 (140/95) is a
			                      -- no-op here; to make the pass genuinely faster you'd raise Crow.maxLevelSpeed (a balance
			                      -- change — don't do it silently).
			peelRadiusMul = 1.15, -- peel-off aims back OUT to orbitRadius × this (beyond the ring) so it clears the eagle.
			                      -- LOWERED 1.6→1.15 (S18): 1.6 (×220 ≈ 352 studs) flung the attacker FAR out+up after
			                      -- every pass → read as "fleeing." 1.15 (×150 ≈ 172) just clears the ring so it re-engages.
			peelHeightBias = 50,  -- studs; climb this much above the eagle on the peel (regain attack-from-above E). LOWERED
			                      -- 120→50 (S18) so the peel is a tight re-engage, not a long flight up-and-away.
			-- suppressDive stays ON even during the pass for the first playtest (a controlled fast closure, not a
			-- stoop) so we don't reintroduce the dive-into-terrain overshoot; revisit once cadence feels right.
		},
	},
}

--------------------------------------------------------------------------------
-- THERMALS — restore energy (altitude currency). Placed by GameServer.
--------------------------------------------------------------------------------
-- v3 REAL-SCALE: thermals are the energy elevator into the high soaring band (up near the 2 km
-- ceiling). Wider cores so they're findable in the much bigger sky; maxHeight raised to carry the
-- eagle into the stoop-launch altitude. strength ×GRAV_SCALE keeps its ~0.7g ratio (a thermal
-- reduces sink / gives a gentle climb when combined with gliding lift — it doesn't levitate you).
-- WATCH-ITEM (RESEARCH §v3): real thermal climb is slow; if reaching the apex is tedious in
-- playtest, raise strength above the scaled value as a gameplay concession.
--------------------------------------------------------------------------------
-- UPDRAFT (S31 pivot) — proximity-flight SCORE loop. Phase 0 test: "is line-riding FUN?"
-- Fly CLOSE to terrain → combo grows (closer + faster + longer) → BANK it by pulling away clean, or
-- LOSE it by crashing (feather-burst, keep flying — non-lethal). Client-computed for feel (see
-- BirdController + docs/UPDRAFT-strategic-plan.md). All PLAYTEST-PROVISIONAL — tune to taste.
--------------------------------------------------------------------------------
GameConfig.Updraft = {
	enabled        = false,  -- SUPERSEDED by GameConfig.Rescue (S32): the line-riding proximity scorer
	                         -- is repurposed as the rescue STYLE meter (reuses computeProximity). Off here
	                         -- so the two HUDs don't both draw. Flip on only to revisit standalone line-riding.
	proximityDist  = 45,     -- studs; terrain within this counts as a ridable "line". Bigger = more forgiving.
	sweetSpot      = 8,      -- studs; at/under this, closeness maxes at 1.0 — the knife-edge thrill.
	minSpeed       = 55,     -- studs/s; below this no combo grows (must actually be FLYING a line, not hovering).
	refSpeed       = 160,    -- studs/s; speed at which the speed bonus = 1.0 (eagle cruises ~130-170).
	comboRate      = 120,    -- combo points/sec at max closeness + ref speed (the earn rate).
	multPerSec     = 0.6,    -- multiplier climb per second while riding a line (risk sustained = reward).
	multMax        = 8,      -- multiplier cap.
	bankDelay      = 0.8,    -- s of clean pull-away (not close, no crash) before the combo auto-BANKS to score.
	crashLoss      = 1.0,    -- fraction of the unbanked combo lost on a crash (1 = lose it all).
}

--------------------------------------------------------------------------------
-- EAGLES TO THE RESCUE (S32 Phase 0) — the sacred slow-mo auto-catch loop.
-- Fly a fast line past a stranded squirrel → it LEAPS for your talons in slow-motion →
-- guaranteed catch → carry on your back → deliver up the waterfall updraft. Non-lethal, kid-first.
-- All numbers from the Fable Packet-01 consult (docs/rescue-consults/PACKET-01-the-catch.md);
-- PLAYTEST-PROVISIONAL — ground truth is Chad's Play. Consumed by RescueServer + BirdController.
--------------------------------------------------------------------------------
GameConfig.Rescue = {
	enabled          = true,   -- master switch for the whole rescue loop + HUD
	-- P3 "danger without cruelty": terrain NEVER kills in rescue mode — the eagle slides/bounces off
	-- (Phase 1 adds the cartoon bounce + dazed recovery). Implemented as a gated effective CRASH_SPEED
	-- at the two collision call sites (server processCrashes + client onFlightStep); the stored
	-- Flight.CRASH_SPEED (45) is UNCHANGED so combat is byte-identical if ever un-shelved. The deep
	-- Y < GROUND_FAILSAFE_Y (-200) kill still applies (nothing legitimately flies that low in the valley).
	-- B4.1 SOLID TREES (Fable S43 consult). Ghost trees today; flip and the eagle's swept-sphere hits
	-- the canopy so THREADING the grove at speed becomes the skill flex — and, for free, the existing
	-- style meter (which only sees CanQuery=true parts) starts charging near trees. Note that is any
	-- queryable part within styleProxDist along the ray fan, so plain grove OVERFLIGHT charges too,
	-- not only deliberate skims — intended (the grove becomes live scoring space). NOT lethal:
	-- nonLethalTerrain already forces every impact to a graze-slide. "off" = the byte-identical ghost
	-- forest · "trunks" = wood solid, leaves soft (kid-kind fallback) · "full" = both (Fable's flight
	-- hypothesis — canopy is a roof you skim).
	-- 🔒 CHAD'S CALL, S45c — STAYS "off". Offered the flip, he chose the ghost forest: *"trees I dont
	-- collide with which is a positive. Makes it more simple."* That is a DESIGN DECISION about feel,
	-- not an oversight, and it outranks the scoring argument below. Do not flip it back without asking.
	-- ⚠️ THE COST, so nobody rediscovers it as a bug: this ONE switch is both things at once. Trees are
	-- non-queryable, and `computeProximity` (the style meter) only sees CanQuery=true parts — so the
	-- style meter CANNOT CHARGE NEAR TREES. The advertised canopy-threading skill, the game's headline
	-- skill-ceiling verb, currently pays nothing. Chad's simplicity and that scoring are in direct
	-- conflict while they share a flag.
	-- ▶ ✅ DONE (S46, B4.2) — the two ARE decoupled now, see `treeStyleScoring` below. The scorer got
	-- its own source (canopy POSITIONS via SpatialHash, not raycast hits), so canopy-threading pays
	-- WITHOUT a single tree becoming a collision surface. The ghost forest above is untouched and
	-- `treeCollision` still does exactly one job: collision.
	treeCollision    = "off",

	-- B4.2 — TREE STYLE SCORING (the decoupling). The style meter scores the canopy SURFACE from tree
	-- positions, independent of CanQuery/collision. false ⇒ provably byte-identical to pre-S46 (the
	-- index is never even built — no scan, no per-frame lookup).
	-- ⚠️ Its own prox/sweet knobs ON PURPOSE: styleProxDist=40 was tuned for a 10-ray world-axis fan
	-- against BIG terrain surfaces (ground, mesa) and systematically under-detects. The tree term is an
	-- exact analytic distance to ~170 overlapping ~40-stud balls; reusing 40 there would drown the
	-- ground line-riding feel Chad already signed off. Tune trees here, never in styleProxDist.
	-- Canopy radius runs 13.6–22.4 studs, so proxDist 16 means the deep interior of a crown scores ~0
	-- and the skin scores 1.0 — that is the threading mechanic, see RescueRules.shellCloseness.
	-- S46 CYLINDER AXIS FIX. A Roblox Cylinder's axis is LOCAL X, but Trunk/Mesa/SafePad/RimPool passed
	-- the intended height in Y and ALSO applied the stand-up rotation — so trunks rendered 7 studs tall
	-- and 60-115 WIDE (pancakes under floating canopies) and the mesa/pad/pool rendered as tall thin
	-- WALLS instead of discs. Four other cylinders in the repo already had it right (UpdraftColumn,
	-- FoxFlare, FireVisuals.buildRing, goldRing), which is what proved it a typo, not a style.
	-- false ⇒ the pre-S46 world, byte-identical (the two always-correct sites are NOT affected either way).
	-- ⚠️ This is a VISIBLE world change Chad green-lit knowing two consequences: (1) it moves the
	-- mesa/pad line-riding baseline that B4.2's playtest used as its control, and (2) the deliver run
	-- now clears the pad rim (~y 258) instead of passing through the old thin blades at ~230 — which is
	-- the INTENDED design, but the waterfall updraft meant to carry you there applies no force at all
	-- (`updraftStrength` below is read by nothing — see the HANDOFF landmines).
	cylinderAxisFix    = true,

	treeStyleScoring   = true,
	treeStyleProxDist  = 16,   -- studs from the canopy SKIN (either side) where tree style charges.
	treeStyleSweetSpot = 3,    -- studs; within this of the skin, closeness = 1.0 (the knife-edge).
	nonLethalTerrain = true,

	-- TERRAIN "OOF" BOUNCE (Packet B3 — PRESENTATION ONLY). A hard terrain contact now reads as a cartoon
	-- "oof" (feather poof + a brief dazed effect over the bird) instead of a silent slide. This is FX ONLY:
	-- the graze-SLIDE math (BirdCollision.resolve → newVel/newPos) is byte-identical, flightEngine is never
	-- touched, no control/camera/kernel change. IMPACT HARDNESS is measured from the resolve output — the
	-- into-surface closing speed the eagle smacked terrain with (-velocity:Dot(hit.Normal)); a glancing
	-- brush is ~0 and skips, a boom-and-zoom faceplant is large. Only the POSSESSED eagle shows this, and
	-- only while nonLethalTerrain is on (inert otherwise). Fires the shared feather pool (no new particle sys).
	bounceEnabled    = true,   -- master switch for the bounce presentation (false → silent slide, as before).
	bounceMinSpeed   = 28,     -- studs/s of into-surface closing speed to trigger the "oof". WAS 55 — at cruise (~130)
	                           -- only a >25° near-faceplant cleared it, so real rescue skims NEVER fired. 28
	                           -- fires a 15-20° cruise contact (~34-44 closing) while a gentle <12° skim or a
	                           -- slow careful touch (~60 speed) stays a quiet graze. PLAYTEST-PROVISIONAL.
	bounceDebounce   = 0.6,    -- s cooldown so sliding along a slope doesn't machine-gun the FX every frame.
	                           -- ALSO requires the eagle to have left terrain (a graze-free frame) since the last
	                           -- bounce — so a sustained scrape fires once, not once per debounce tick.
	dazeTime         = 2.0,    -- s the world-space dazed "stars" read hovers over the bird after a hard contact.
	                           -- WAS 1.5 — too brief to read at flight camera distances.
	bounceFeathers   = 10,     -- IMPACT ACCENT dust-puff particle count (S41: the FEATHERS are now discrete
	                           -- parts — GameConfig.Rescue.featherParts; this emitter is only the brief pale
	                           -- "smack" scuff). WAS 44 (the brown-ball cloud, retired).
	dazeStarCount    = 4,      -- little "stars" orbiting the bird's head during the daze (procedural gray-box).

	-- FEATHER-PARTS RING (S41 — "a poof RING of FALLING feathers"). The old soft-sprite emitter
	-- blended into a brown BALL 3×; discrete anchored PARTS make the blob structurally impossible.
	-- Pooled, code-driven kinematics (FeatherBurst.step) — no Roblox physics bodies, presentation only.
	-- Gated by tests/featherburst.spec.luau (ring / fall / flutter / persistence).
	featherParts = {
		count     = 12,    -- feathers per burst; 12 = one every 30° — reads DISCRETE, still a full ring.
		poolMax   = 36,    -- 3 full bursts alive at once (life 3.0 / debounce 0.6); oldest is stolen after.
		life      = 3.0,   -- s; Chad's 2.5-3.5 linger window — outlives the 2.0s daze so the aftermath hangs.
		ringSpeed = 26,    -- studs/s radial; with drag 2.2 the ring opens to ~11 studs — reads at flight camera.
		upSpeed   = 12,    -- studs/s pop; apex ~+2 studs at ~0.5s — burst UP first, then the fall reads.
		gravity   = 14,    -- studs/s²; terminal sink 14/2.2 ≈ 6.4 studs/s — visibly FALLING, still feather-slow.
		drag      = 2.2,   -- 1/s linear; arrests the radial burst in ~0.5s, sets the gentle sink.
		swayAmp   = 1.2,   -- studs; tangential sinusoid amplitude — a leaf-flutter wander, not a plumb drop.
		swayFreq  = 2.4,   -- Hz; ~2 Hz is the falling-leaf oscillation band the eye labels "flutter".
		spinSpeed = 4.5,   -- rad/s max per euler axis; ~0.7 rev/s lazy TUMBLE (faster = spin-blur).
		size      = Vector3.new(0.15, 2.4, 0.9), -- thin × long × vane; 2.4 long = the emitter size PROVEN
		                                         -- readable at flight range; aspect 16:1 — never a ball.
		holdFrac  = 0.75,  -- fully opaque for 75% of life, then linear fade to invisible at life's end.
	},

	-- THE CATCH TRIGGER (Fable §A). Any fast flyby through the sphere fires — no aiming, guaranteed.
	-- S32 Chad playtest: "a little hard to snatch the squirrels" → BIGGER trigger + easier gate.
	triggerRadius    = 42,     -- studs; flyby sphere around the squirrel (was 28). Bigger = easier to snatch.
	ftueRadiusMult   = 1.5,    -- first `ftueCatches` catches use a 1.5× sphere so the "aww" fires unearned.
	ftueCatches      = 3,
	closingGateFrac  = 0.30,   -- must be closing at ≥ this × eagle glideSpeed (was 0.40; lowered so snatching is easier).
	ftueGateFrac     = 0.15,   -- during the first `ftueCatches`, use this EASIER closing gate so a slow/wobbly
	                           -- 5-yo still catches (Fable §D). A too-slow pass that enters the sphere but misses
	                           -- the gate triggers the squirrel's "come back!" wave — failure has a face, never silence.
	tellLeadTime     = 0.60,   -- s; the "determined survivor" tell fires this far ahead (predicted from closing speed).
	                           -- Was 0.40: at glide ~130/s that warned only ~52 studs before the sphere, so the
	                           -- plant/flare fired too late to SEE before the catch. 0.60 ≈ 78 studs = the
	                           -- anticipation beat is actually read (Fable §A3/#4). Revert to 0.40 if tells feel spammy.
	tellHoldMin      = 1.2,    -- s; once planted the tell LATCHES at least this long (a flickering tell kills the beat).
	-- CHAD S43: "the sounds pinging me after I catch just a few squirrels — it should only ping when I
	-- have 10/10." The rising chord fired on EVERY catch, so the celebration was cheap and nagging.
	-- It now fires ONCE, on the catch that fills the back (see BirdController's rider-conversion sites).
	-- true = the old per-catch ping back, for comparison.
	chimeEveryCatch  = false,
	-- THE PING LAW (S44). The S43 fix above gated the CHORD and Chad still heard pinging at 6/10 —
	-- because the chord was never the loudest offender. `playGotcha` (every catch, vol 0.6 vs the
	-- chord's 0.5), `playWoohoo` (every catch) and the CREW CHOIR (every ~1.7s from the first rider)
	-- all played the SAME electronicpingshort.wav. "Ping" names the ASSET, not a function — so the
	-- class each event speaks in now lives in RescueRules.soundClass and is swept by a spec that
	-- fails if ANY event can ring a bell below a full load. These three flags each restore ONE
	-- legacy voice for A/B; all ship false. See RescueRules "THE PING LAW".
	gotchaOnPing     = false,  -- true → the catch accent is the old two-note BELL again (was per-catch).
	woohooOnPing     = false,  -- true → the "wheee!" is the old pitch-bent bell instead of a wind rush.
	chirpOnPing      = false,  -- true → crew chatter is the old bell metronome instead of chitter clicks.
	-- S44 BOOT DIAG — Chad: "it just takes a while at the start for everything to render." The audit
	-- ruled OUT a frame-rate cliff, which leaves three very different candidates and no way to pick
	-- between them from the outside: (a) Roblox replicating the whole place at join (StreamingEnabled
	-- is false, so everything arrives before anything renders), (b) the client acquisition handshake
	-- (Birds folder → Body → NetOwner attribute), (c) the 6s INTERMISSION, which deliberately empties
	-- the valley of squirrels and then bursts ~600 instances in a single frame when the round starts.
	-- Guessing between them is how the ping bug survived five "fixes" — so: one flight, timestamped
	-- milestones on BOTH sides, and the answer is in the Output. Zero behaviour when false.
	bootDiag         = false,  -- prints [BootDiag/server] and [BootDiag/client] milestone timings.
	                           -- ✅ S45c: OFF — the cause is NAMED, FIXED and Chad-confirmed, so this is
	                           -- now just console noise on every flight (and noise buries the next
	                           -- diagnostic). THE ANSWER IT GAVE, keep it: StreamingEnabled=false, and
	                           -- RescueWorld=249 on the CLIENT at t=1s, flat at every later mark — every
	                           -- part present immediately, counts never climbing ⇒ NOT replication, NOT
	                           -- streaming, NOT assets (there are none). It was Roblox's AUTOMATIC
	                           -- graphics mode throttling RENDER DISTANCE to protect the frame budget.
	                           -- Fixed for everyone (not just a maxed slider) via Map.cheapScenery.
	                           -- Turn back on only for a NEW startup question — and if you do, remember
	                           -- the probe measures when things EXIST vs are DRAWN; those are different
	                           -- clocks and conflating them is what cost S44 a whole session.
	-- …and it WAS named, first flight. The server builds the whole world in 4 MILLISECONDS; then the
	-- valley sits deliberately EMPTY for the whole 6s intermission while the player already has a bird
	-- (Birds folder at 1.906s) and nothing to fly at. All 15 squirrels then appear in ONE frame at
	-- 6.228s — which is exactly what "it takes a while at the start for everything to render" looks
	-- like from the cockpit. Nothing was rendering slowly; the world was empty ON PURPOSE.
	-- Between rounds that pause is right (it is the breather after the results screen). At BOOT there
	-- is no previous round to break from, so it is dead air. Set nil to restore the flat 6s everywhere.
	firstIntermission = 1.0,   -- s; the FIRST intermission after a server boot only (round #1).
	sfxDiag          = false,  -- true → every one-shot prints "[SFXDiag] event class vol pitch carry/cap" to
	                           -- Output. The audio channel is the one subsystem that never had a diagnostic,
	                           -- which is how six guess-cycles happened; ONE flight now NAMES the sound.
	carryCapacity    = 10,     -- squirrels ridable on your back before you must deliver (Chad: "he can carry
	                           -- 10 squirrels that latch onto him"). Grid-packed on the back (SEAT_COLS across,
	                           -- the rest stacking toward the tail). Read by BOTH the server carry gate and the client.
	carryResetOnLoss = true,   -- S45: when a player's BIRD is destroyed (R, death, crash), zero the SERVER's
	                           -- carry to match the client, which drops every rider on any teardown. Without
	                           -- this, one R-press mid-carry silently ate capacity for the rest of the round
	                           -- and paid out for squirrels never carried. false = pre-S45 (the bug).

	-- THE LEAP ARC (Fable §A). Ballistic, solved to intercept the eagle's PREDICTED talon position.
	arcAirtimeMin    = 0.50,   -- s; airtime floor.
	arcAirtimeMax    = 0.85,   -- s; airtime ceiling (scales up slightly with closing speed).
	arcApexHeight    = 10,     -- studs above the perch the leap arcs (presentation gravity, floaty & pretty).
	arcConvergeFrac  = 0.85,   -- past this fraction of airtime, blend HARD to the talon so the catch never snaps.
	climbDur         = 1.1,    -- s; after the talon catch, the squirrel SCRAMBLES up the flank to its back seat
	                           -- over this long (Chad's #9 catch-latch beat). Real-time; the slow-mo snap-back
	                           -- (≤0.2s) rides down under its first part. Lower→snappier, higher→longer/more-readable
	                           -- scurry (→1.1 per Chad: "slow down… the climb", "heighten the resolve").
	squirrelScale    = 4.4,    -- BuildSquirrel scale for BOTH stranded world squirrels and the caught/rider
	                           -- squirrels (was 2.4 → 4.4 per Chad: "make the squirrels bigger" so the climb reads).
	crewAirtime      = 0.42,   -- s; a CREW CATCH's quick leap-to-talon (overlaps the sacred catch; no slow-mo).
	                           -- Crew catches are what let a fast chain build a combo instead of dropping.
	courageStep      = 0.08,   -- rider GROWTH per crew catch it helps haul in (Packet-03 P5 "courage is
	                           -- visible": the ones you saved grow braver, told by size alone).
	courageStages    = 3,      -- growth cap in steps (3 × 0.08 → ×1.24). NOTE: fully-grown neighbors
	                           -- already interpenetrate ~0.8 studs at the SEAT_COLS=4 grid spacing —
	                           -- a cuddly squish, Chad-gated; widen the seat factors if it reads as a merge.
	fullHintCooldown = 6,      -- s between "WINGS FULL → waterfall!" HUD nudges when you fly a real catch
	                           -- line with no room aboard (S35 Chad: capacity silence read as lost catches).
	claimTTL         = 3,      -- s a fired-for squirrel stays locally unclaimable (covers the server's
	                           -- destroy replication window; a server-REJECTED catch frees up after this).

	talonAnim        = true,   -- S39: animate the eagle's TALONS on a catch — reach down-and-forward, splay
	                           -- open, then clench/grab on contact, tucking back during the climb (Chad "I want
	                           -- eagles talons to be animated when catching squirrels"). Cosmetic Motor6D only;
	                           -- false → legs stay in the static tucked pose (instant revert).

	-- STRANDED-SQUIRREL BEACON geometry (S40 fix — Chad "its just a light cylinder, I don't even see
	-- squirrels"): a THIN marker beam whose base sits ABOVE the squirrel's head so it never occludes the
	-- critter (the client fades it to invisible up close). ONE source of truth: RescueServer.buildBeacon
	-- reads these AND the headless model-build gate (tests/models.spec.luau) asserts the base clears the
	-- actual built squirrel's top. beaconBaseY is measured from the squirrel Body origin (studs).
	beaconWidth      = 1.6,
	beaconHeight     = 70,
	beaconBaseY      = 8,      -- base this far above Body origin; clears the ~+5 head/tail at squirrelScale 4.4.

	-- SIGNAL STYLE (Stage 1 kid-legibility — docs/KID-LEGIBILITY-PLAN.md). "shaft" = today's thin
	-- beacon beam, byte-identical (IDENTITY DEFAULT — flip back = exact old build). "halo" = the
	-- kid read: the CRITTER is the landmark — a flat ground halo disc it stands in + a small
	-- bobbing chevron gem over its head (rarity by hue + gold/rainbow size), a bigger squirrel,
	-- waveBig as the default stranded pose, and little urgent client-side hops.
	beaconStyle       = "halo",   -- S41 Stage 1 LIVE (Chad "just light cylinders"): the critter is the landmark. Revert to "shaft" for the old beam.
	haloRadius        = 10,    -- studs; halo disc radius. Squirrel arm-span half-extent at scale 5.5
	                           -- is ~4.5 → 10 FRAMES the critter (>2×) and reads as a bright ~20-stud
	                           -- dot from scan altitude; flat at the feet, so it can never engulf.
	haloGroundY       = -1.5,  -- studs below Body origin the disc centre sits. Disc is 3 thick → top
	                           -- at Body origin, well below the head bottom (+0.31·s ≈ +1.7): a glow
	                           -- POOL at the shins, never over the face. (-6 = true foot-bottom is
	                           -- INSIDE the perch — invisible from above — so we sit it at the shin.)
	chevronY          = 14,    -- studs above Body origin the gem hovers. Squirrel top ≈ +7.98 at 5.5;
	                           -- worst-case gem bottom = 14 − 2.73(gold gem halfY) − 1.0(gem bob)
	                           -- − 1.0(squirrel hop) = 9.27 → clears the head by ≥1.29 always
	                           -- (models.spec asserts this exact inequality).
	-- THE WAYFINDER (S45) — Chad: "I dont even know where to fly at the start… dropped in cold."
	-- The gold streamer (BirdController updateWayfinder) is the ONLY directional mechanism in the game,
	-- and it was gated on carrying ≥1 rider — dark at spawn, dark after every delivery, i.e. off at
	-- precisely the six-or-so moments a round when you are empty-handed and looking for somewhere to go.
	-- Un-gated, an empty-handed eagle gets the same certified ribbon pointing at the nearest catchable
	-- squirrel. Policy (incl. the anti-strobe hysteresis) is pure in RescueRules.wayfindTarget.
	wayfindEmpty      = true,  -- KILL SWITCH: false → empty-handed is dark again (pre-S45 byte-identical).
	wayfindSwap       = 0.2,   -- a challenger must be this fraction CLOSER to steal the ribbon from the
	                           -- held target. 0 = re-pick every frame (strobes between equal groves).
	wayfindTipLift    = 12,    -- studs the WATERFALL ribbon's tip lifts (arcs up toward the mesa). The
	                           -- squirrel ribbon deliberately does NOT lift: groves sit ~40° BELOW the
	                           -- spawn frame, so the whole answer is "point down there".

	-- THE SKY GEM (S44) — Chad: "I can barely see the squirrels, they render in after 30s." Nothing
	-- renders late (all 15 spawn on the round's first frame) — the markers are fixed WORLD-size objects
	-- and he acquires from 600-2,600 studs, where the 2.2-stud chevron is ~2.4 px and the flat halo is a
	-- ~13x6 px smudge; 45% of perches are under the canopy with no sky signal at all. So the gem becomes
	-- a per-frame DISTANCE PROJECTION: constant ANGULAR size + a lift clearing the treeline at range,
	-- descending back onto the head as you close (near = byte-identical Stage 1). See RescueRules
	-- "THE SKY GEM" for why every other technique's ceiling is below this read.
	skyGem            = true,  -- ✅ LIVE (Chad flew it, S45c: "yes on gems"). false → skyGemProjection
	                           -- returns the identity, i.e. a config-only revert to the Stage-1 gem.
	skyGemStudsPerStud = 0.015,-- gem studs per stud of distance ⇒ ~11.6 px at 1080p/FOV70, at ANY range.
	skyGemMaxSize     = 34,    -- studs; growth ceiling so a 2 km gem can't become a sky-filling billboard.
	skyGemClearY      = 160,   -- world Y the gem's BOTTOM rises to at range. buildTree's worst case is
	                           -- crownY 115 + blob 35 = 150 — this clears it with margin.
	skyGemFarT        = 0.05,  -- transparency the gem blends toward at range. The Stage-1 far read was
	                           -- held at 0.30-0.50 (half-dimmed AND subpixel); at distance it goes solid.
	skyGemNear        = 70,    -- reuse the certified Stage-1 fade band so near behaviour is provably…
	skyGemFar         = 260,   -- …today's: at ≤70 studs the projection is the identity, by construction.
	chevronAspect     = 1.6,   -- the gem is built this many × taller than wide (buildBeacon) — the
	                           -- projection needs it to place the gem's BOTTOM at skyGemClearY.
	chevronSize       = 2.2,   -- studs; gem base size (built 1.6× tall, rolled 45° → a kite pointer,
	                           -- the known "map marker" icon). Gold/Rainbow (mult ≥ 4) build ×1.35.
	chevronBobAmp     = 1.0,   -- studs; the gem's own slow bob ("THIS one!" pointer life).
	haloSquirrelScale = 5.5,   -- BuildSquirrel scale under "halo" (+25% over 4.4 → ~14 studs tall: the
	                           -- critter itself reads at gameplay range). Used for world squirrels AND
	                           -- the client presentation/riders so the leap handoff has no size pop.
	                           -- squirrelScale (4.4) stays the untouched "shaft" identity value.
	hopAmp            = 1.0,   -- studs; stranded urgent-hop height (client |sin| bounce — sharp landing
	                           -- = a HOP, not a float; ~7% of body height: eager, not flying).
	hopFreq           = 2.0,   -- × squirrelPhase (3 rad/s) → hop period ≈ 0.52 s ≈ 2 hops/s: urgent,
	                           -- below the ~4 Hz band that reads as vibration at flight distance.
	dangerHopMult     = 1.5,   -- STAGE 3a: hop-amplitude × at Danger ≥1 (fire closing). Capped at 1.5 so a
	                           -- frightened critter never hops out of its halo frame — the Stage-1 read wins.
	dangerHopFreqMult = 1.5,   -- STAGE 3a: hop-frequency × at Danger 2 only. 2 → 3 hops/s: frightened =
	                           -- faster and smaller-feeling, still under the ~4 Hz vibration band.

	-- ── B5 SQUIRREL SQUEAKS (Chad S42 #2: "little squirrel sounds and music even the better"). ──
	-- Discrete one-shots (NOT a loop — the stateless-projection law is for sustained sounds). Volume,
	-- cadence AND the whole schedule (stagger, stale-reseed, per-frame budget, jitter floor, pitch) are
	-- pure functions in RescueRules, so the anti-chirp-storm rails are headlessly gated rather than
	-- asserted in a comment — the first draft kept the rate machinery inline and a mutation deleting the
	-- per-frame cap passed the entire suite. The cadence keys
	-- off the Stage-3a Danger level, so the EAR learns triage the same way the eye does: a panicking
	-- squirrel calls out ~6× more often than a calm one. Doubles as a LOCATOR — you can hear a critter
	-- you have not spotted yet, which is the kid-floor gift.
	squeakAudio       = true,  -- LIVE, but inert until squeakAssetIds has a working id (see below).
	squeakHearR       = 260,   -- studs: silent at/beyond. Roughly the halo's legibility range, so ear
	                           -- and eye acquire a squirrel at about the same moment.
	squeakFullR       = 60,    -- studs: full volume at/inside (about one scoop-approach out).
	squeakMaxVol      = 0.5,   -- a small animal is not loud. NOTE the live flapVolBase is 1.6 (Chad S38
	                           -- "LOUDER again"), so this is an ABSOLUTE ceiling, not a ratio — the spec
	                           -- pins it absolutely because a relative gate compared against 1.6 was
	                           -- toothless and let a squeak-as-loud-as-the-wingbeat mutation pass.
	squeakIntervalCalm    = 7.0, -- s between chirps at Danger 0 — sparse: a living wood, not a mob.
	squeakIntervalWorried = 3.0, -- Danger 1.
	squeakIntervalPanic   = 1.2, -- Danger 2 — insistent, still short of a machine-gun chirp.
	squeakJitter      = 0.35,  -- ± fraction of the interval, derived deterministically from SquirrelId +
	                           -- time so 15 squirrels never chirp in unison (no math.random in FX).
	squeakPitchSpread = 0.22,  -- ± playback-speed spread by SquirrelId: individuals, not clones.
	squeakPitchDanger = 0.18,  -- extra playback speed at Danger 2 — fear raises a small animal's pitch.
	squeakMaxPerFrame = 2,     -- hard cap on simultaneous starts, so a grove going up can never burst.
	squeakMinInterval = 0.6,   -- absolute floor between one squirrel's chirps, so even a mis-set jitter
	                           -- (>1 would drive the interval to zero) cannot produce a machine gun.
	squeakMaxLen      = 3,     -- s: an asset longer than this is ambience/music, not a squeak — the
	                           -- resolver REJECTS it. Loadability is not plausibility (the S43 fire-id
	                           -- lesson: a wrong-content asset loads perfectly and lies).
	-- ⚠️ EMPTY BY DESIGN — see Fire.roarAssetIds for the full post-mortem. Three web-sourced fire ids
	-- failed three different ways (wrong type / privated / turned out to be MUSIC), which is why an id
	-- must come from someone who can PREVIEW it. Built-in rbxasset:// sounds are moderation-immune (see
	-- the S36 note by SFX_SWOOSH in BirdController) but none of them is an animal.
	-- TO FILL IT: Studio → Toolbox → Marketplace → Audio → search "squirrel" / "chipmunk squeak" /
	-- "mouse chirp" → PREVIEW → Copy Asset ID → paste below → Stop and re-Play.
	squeakAssetIds    = { },

	-- ── B5a THE SAVED CROWD — "the crowd IS the scoreboard" (bible P4; Fable S43 consult). ──
	-- Delivered squirrels currently get :Destroy()ed, so the whole rescue effort evaporates. Instead
	-- they persist as a growing crowd on the safe meadow: a five-year-old reads their score with zero
	-- numerals, and a skilled pilot's meadow visibly fills faster. Seat i depends ONLY on i, so the
	-- crowd ACCRETES — nobody ever relocates when it grows (a reshuffling crowd reads as a flock
	-- milling about, not as a record of what you did).
	-- QUARANTINE (load-bearing): crowd members get NO halo, NO chevron and NO hops. Those three ARE
	-- the certified Stage-1 "this critter needs you" signal — a saved squirrel wearing them would bait
	-- a rescue read at the one place on the map where nobody needs rescuing.
	savedCrowd        = true,  -- S43 LIVE for Chad's flight (the S41 pattern: headless-gate it, then hand
	                           -- him a WORKING result rather than a flag to find). false = byte-identical
	                           -- to the pre-B5a round; the kill switch also tears an existing crowd down.
	crowdMax          = 24,    -- ~2 good rounds of individuals. Overflow stops adding MODELS (honest
	                           -- session-scale ceiling); C1/C2 persistence makes the Den the long-term home.
	crowdRadiusMin    = 45,    -- studs from deliverPos: inside this is the landing pad itself — keep clear.
	crowdRadiusMax    = 85,    -- …and outside this they are too far to read as a crowd from the pad.
	crowdRingGap      = 12,    -- radial spacing between seat rings.
	crowdSlotSpacing  = 13,    -- target arc spacing between seats in a ring. With the ring gap this makes
	                           -- minimum separation STRUCTURAL — the anti-brown-blob rail (S41 feather lesson).
	crowdExcludeDeg   = 120,   -- total wedge (±60°) kept clear around the climb-out bearing…
	crowdExcludeBearingDeg = 180, -- …which is the waterfall/updraft side (−X). Crowd is dead ahead as you
	                           -- crest the falls, never in the climb-out line, never over the pad.
	crowdScale        = 4.0,   -- BuildSquirrel scale for a crowd member (below the 5.5 stranded scale:
	                           -- present, but never mistaken for the critter you are flying at).
	crowdLift         = 1.2,   -- studs above the raycast-found pad surface (feet on the grass, not sunk).
	crowdCheerS       = 2.2,   -- s the whole crowd waves BIG after each arrival — the deliver payoff.
	crowdPoseRange    = 600,   -- studs: beyond this the crowd is static props and costs nothing to pose.

	-- CLIENT SLOW-MO PRESENTATION (Fable §B) — dilates anim/FX/audio/camera ONLY; the sim runs real-time.
	slowmoScale      = 0.25,   -- presentation timescale during the catch window.
	slowmoEaseIn     = 0.11,   -- s; fast attack into slow-mo.
	slowmoSnapBack   = 0.20,   -- s; the "whip" back to real time (the exclamation point).
	hitStopFrames    = 3,      -- frames of freeze on contact (~50ms; ≥6 reads as lag).
	-- S33 Chad playtest: "it zooms in too much when you catch a squirrel — we need it to not do that."
	-- Both the camera dolly-in (camPullIn) AND the FOV narrow read as a zoom, so both are OFF. The
	-- slow-mo now reads through desat + cant + hit-stop only (no zoom). Nudge camFovDelta to ~-2 if a
	-- faint optic-flow "slow" cue is wanted back, but keep camPullIn at 0 (the dolly was the zoom).
	camFovDelta      = 0,      -- deg; FOV change during slow-mo (was -5 → read as zoom; OFF per playtest).
	camPullIn        = 0,      -- fraction closer the camera eases during slow-mo (was 0.15 → the zoom-in; OFF).
	camCant          = 4,      -- deg; cinematic camera cant during slow-mo (a tilt, not a zoom — kept).
	woohooDelay      = 0.50,   -- s after snap-back completes for the delayed "…woohoo!".

	-- STYLE (Fable §C) — the skill ceiling. Line-riding (skim/thread under canopy) charges a meter that
	-- MULTIPLIES catch points. Never gates the catch. Reuses the terrain proximity ray-fan.
	styleProxDist    = 40,     -- studs; terrain within this charges style (the line-riding closeness).
	styleSweetSpot   = 7,      -- studs; at/under this, closeness = 1.0 (the knife-edge).
	minStyleSpeed    = 60,     -- studs/s; must actually be flying a line (not hovering) to charge style.
	styleChargeRate  = 0.9,    -- style-meter units/sec at max closeness (meter feeds styleMult = 1 + meter).
	styleDecayHalf   = 2.0,    -- s; meter half-life when not riding a line (must fly the line INTO the catch).
	styleMultMax     = 3.0,    -- cap on the catch multiplier from style.
	swiftFrac        = 0.60,   -- closing speed ≥ this × glideSpeed on catch = "SWIFT!" bonus stamp.
	blazingFrac      = 0.90,   -- ≥ this = "BLAZING!".

	-- SCORING.
	basePoints       = 100,    -- base points per rescue (generous, identical for everyone).
	-- MATH DELIVERY (Packet-03 §3 — the stealth-math headline; replaced the flat deliverBonus n×25).
	-- Deliver bonus = n × n ACORNS (an acorn = acornValue points; the HUD divides by acornValue so every
	-- number a kid sees is small & round). n² makes one brave trip of 10 beat two safe trips of 5 —
	-- the push-your-luck call IS the multiplication lesson. carryCapacity=10 → 10×10 = the hundred chart.
	acornValue       = 10,     -- points per acorn (display currency divisor; also the n² bonus unit).
	hopBeat          = 0.35,   -- s; deliver hop-off cadence — one rider dismounts per beat, ticking the
	                           -- tally by n (one ROW of the n×n array) so the kid skip-counts to n².
	perfectTenAt     = 10,     -- rescues delivered in one round to arm the PERFECT TEN finale.
	timeFactor3At    = 30,     -- s left on the clock at the 10th rescue → TIME FACTOR ×3 (else ×2).
	timeFactor4At    = 60,     -- s left → the rare ×4 (a blazing round; keep it aspirational).
	-- ENERGY BONUS (Chad S34: "bonus for high-velocity retrievals to teach energy dynamics"). A fast
	-- catch = you traded altitude for speed (boom-and-zoom) → your dive MULTIPLIES the rescue. Tiered so
	-- MORE speed = MORE reward (a legible energy lesson), shown to the kid as "×N ENERGY" on the catch.
	swiftMult        = 1.5,    -- closing speed ≥ swiftFrac×glide (SWIFT) multiplies the catch score.
	blazingMult      = 2.0,    -- ≥ blazingFrac×glide (BLAZING) multiplies more (top of the energy curve).
	-- BOOM-AND-ZOOM REWARD (Chad S34). scoopSpeedGain = a TRADE SECRET: flying faster widens the catch
	-- "scoop" a little (rewards speed without telling the player). comboWindow/comboMax = the SHOWN reward:
	-- consecutive catches within the window build a ×2..×comboMax combo that multiplies the score.
	scoopSpeedGain   = 0.25,   -- TRADE SECRET: max fractional widening of the catch scoop at ~2× glide (no UI).
	comboWindow      = 4.0,    -- s; catch again within this to keep the combo chain alive (else it resets).
	comboMax         = 6,      -- combo multiplier cap (×2 ×3 ×4 ×5 ×6 in a row).

	-- THE ROUND.
	roundLength      = 120,    -- s; a 2-minute round.
	intermission     = 6,      -- s between rounds.
	triageAt         = 60,     -- s remaining when the scripted beacon-expiry (fake fire triage) begins.
	squirrelCount    = 15,     -- squirrels live on the field at once (was 11; more, to populate the bigger map).
	respawnDelay     = 5,      -- s before a caught/expired squirrel re-appears at a fresh perch.

	-- DELIVERY (the waterfall safe zone). Shared world constants (server builds it, client rides the
	-- updraft) so no remote is needed for the geometry.
	deliverRadius    = 70,     -- studs; entering (horizontally) this with cargo delivers the batch.
	deliverPos       = Vector3.new(1150, 250, 0), -- top-of-waterfall safe pad centre (world).
	deliverPadY      = 250,    -- Y of the safe pad (deliver when above ~this).
	updraftBaseY     = 20,     -- updraft column runs from here…
	updraftTopY      = 280,    -- …to here (ride it up the waterfall face).
	updraftRadius    = 90,     -- studs; horizontal radius of the waterfall updraft column.
	updraftStrength  = 210,    -- upward accel (studs/s²) at the column centre (composes w/ Thermals).

	-- WORLD placement (server-side gray-box valley). Squirrels + trees live in this XZ box.
	-- S32 Chad playtest: "a bigger map would be good" → wider valley + more trees/squirrels to fill it.
	valleyRadius     = 1600,   -- squirrels/trees scatter within this radius of origin (was 1000).
	treeCount        = 68,     -- gray-box trees, bounded for perf (was 46; scaled up with the bigger valley).
	canopyY          = 110,    -- treetop crown height; under-canopy squirrels sit below this.

	-- GROVES (Fable Packet-02 §B5, queue #1). Squirrels spawn ONLY on grove perches, so they cluster
	-- into a few tight clumps instead of scattering evenly across the r=1600 valley. This kills the
	-- ~250-stud dead travel between catches → catch CHAINS + style-meter continuity (2s half-life).
	-- Each grove is a tight cluster of `grovePerchCount` trees (each seeding one perch); the remaining
	-- `treeCount` budget is scattered as VISUAL-only backdrop forest (no perches). With groveCount×
	-- grovePerchCount perches > squirrelCount, respawns still land in groves but keep some variety.
	groveCount       = 4,      -- number of squirrel groves (clumps) in the valley.
	grovePerchCount  = 5,      -- perch-trees per grove (>~ live squirrels/grove so respawns have slack).
	groveRadius      = 130,    -- studs; a grove's trees/perches cluster within this radius of its centre.
	groveMinGap      = 520,    -- studs; keep grove centres at least this far apart (distinct clumps).

	-- MUSIC (Packet-03 §6, queue #14 — as it ACTUALLY is after S38 + the S44 ping law).
	-- ⚠️ This block used to describe a layered swell (pizzicato@1 → rhythm@4 → brass@7 → heartbeat@8)
	-- that has NOT existed since S38: Chad said three times "should ONLY be 10/10", so every build layer
	-- below a full load was REMOVED (BirdController updateMusic). The four knobs that drove them
	-- (musicPizzAt / musicRhythmAt / musicBrassAt / musicHeartAt) were left behind reading from NOTHING
	-- and are now deleted — a next agent "tuning the pizzicato entry" would have turned a dead dial.
	-- WHAT IS LIVE: a beat clock whose tempo tightens with your CARRY, and on it exactly one voice —
	-- the full-load "di-dah" jingle at 10/10 (jingleLo/jingleHi). Plus the crew choir below. That is all.
	-- Everything is consumed with `or`-defaults so a config-only revert stays build-green.
	musicEnabled     = true,
	musicWindVol     = 0,      -- OFF (Chad S38): this bed also looped SFX_SWOOSH (pitched to a rumble) — killed so
	                           -- there is truly NO continuous whoosh anywhere. The music BEAT layer (pings/snaps) stays.
	musicBeatSlow    = 0.75,   -- s between music beats at 0 progress (calm)…
	musicBeatFast    = 0.34,   -- …tightening to this at the Perfect Ten (urgent, driving).
	jingleLo         = 1.2,    -- FULL-LOAD JINGLE (Chad S38): only at a FULL 10/10 carry, a warm rising two-note
	jingleHi         = 1.6,    -- "di-dah" says "get to the waterfall!" — then STOPS on deliver. Lower/warmer than the old tick.
	musicLayerVol    = 0.16,   -- per-layer one-shot volume (low; layered they build without clipping).
	-- CREW CHOIR — rider chatter scales with crew size ("the crew is the choir").
	chirpBase        = 2.4,    -- s between rider chirps with 1 rider aboard…
	chirpPerRider    = 0.14,   -- …minus this per extra rider (a fuller back = busier chatter), floored.
	chirpMinGap      = 0.5,    -- s; never chirp faster than this (perf + cacophony floor).
	chirpVol         = 0.14,   -- rider chirp volume.

	-- FLIGHT AUDIO — flap sound = a STATELESS PER-FRAME PROJECTION (Fable-5 audit, Chad S38, definitive rewrite
	-- after 6 failed patches). ONE persistent LOOPED Sound that is NEVER Stopped or seeked; every frame its
	-- Volume is recomputed from scratch as `(flapVolBase + flapVolPow·pow) · level · am`, where:
	--   • level = eff · foldFade · exert   → if the wings aren't beating (throttle→0 via Ctrl) OR are folded in a
	--     dive, level is 0, so Volume is 0 THE SAME FRAME. "Persists after I stop" is now ARITHMETICALLY
	--     impossible — silence is a computed value, not a timer/stop that can fail to fire.
	--   • am = flapAmFloor + (1-flapAmFloor)·max(0,sin(flapPhase))^flapPulseExp  → the wingbeat rhythm (a sharp
	--     per-beat pulse over a faint air-bed) so it reads as whoosh-whoosh, not a drone.
	--   • exert = 1 while Shift is held (hands-on flap), else flapHandsFree (hands-free cruise on the sticky
	--     throttle is quieter but not silent — the wings are still visibly beating). flapHandsFree=0 → releasing
	--     Shift goes fully silent (one knob if Chad wants that).
	-- PlaybackSpeed = flapPitchBase + flapPitchPow·pow (tracks effort only — NEVER phase-modulated, which warbles).
	flapVolBase      = 1.6,    -- flap volume at the faintest beat (Chad S38: LOUDER again — "still barely noticeable")…
	flapVolPow       = 1.8,    -- …+ this × flap power → full-effort peak ≈ 3.4 (Roblox Volume caps at 10; >1 is legal & loud).
	flapHandsFree    = 0.55,   -- voice while flapping HANDS-FREE (Shift not held, sticky throttle still up). 0 = silent.
	flapAmFloor      = 0.45,   -- air-bed between wingbeats (raised Chad S38: the old 0.12 kept it near-silent between
	                           -- beats → "barely noticeable"; a higher floor makes the flap continuously PRESENT).
	flapPitchBase    = 1.2,    -- PlaybackSpeed at a gentle beat (Chad S38: raised from 0.85 — "feels muffled"; brighter/crisper)…
	flapPitchPow     = 0.75,   -- …+ this × flap power → tops at ~1.95 (present + crisp, still below the old 2.4 screech).
	flapPulseExp     = 2,      -- sharpness of the per-beat pulse (Chad S38: 3→2 = broader, louder peaks, still rhythmic).
	audioDiag        = false,  -- (was ON to hunt the persist bug — structurally fixed now). Logs a line each time the flap crosses
	                           -- audible/silent — so a flight PROVES the fix (or names the exact nonzero input if not).
	-- NOSE-DOWN FADE (dive wing-fold): foldFade 1 at/above the fold onset → 0 at the committed stoop; it's a factor
	-- of `level`, so a dive silences the flap as the wings tuck. Values mirror BirdBuilder DIVE_ONSET_Y / DIVE_FULL_Y.
	wingNoseFadeOnset = -0.12,  -- LookVector.Y where the flap begins to fade (wings start folding)…
	wingNoseFadeFull  = -0.92,  -- …and where it's fully silent (committed stoop, wings fully tucked).
	-- WIND (Chad S38: back ON, quieter + earlier). A SMOOTH low speed-rush whose volume/pitch rise with SPEED —
	-- NOT rhythmic, so it reads as air rushing, not wingbeats (and the flap is now a distinct physics-driven pump
	-- + the music a clock tick, so nothing else competes). Threshold lowered so normal fast flight hears it ("I
	-- can barely hear the wind at speed, maybe I didn't go that fast"). Kept LOW-pitched, well below any flap read.
	windMinSpeed     = 130,    -- studs/s where wind STARTS (just above easy glide ~110 → audible in normal fast flight)…
	windFullSpeed    = 330,    -- …and where it reaches full (a real dive).
	windMaxVol       = 0.5,    -- wind volume at full speed (a present rush, not a roar). If it EVER reads as flapping again → 0.
	windPitchBase    = 0.7,    -- wind PlaybackSpeed at windMinSpeed (low airy rush)…
	windPitchSpeed   = 0.5,    -- …+ this × speed-fraction (0.7→1.2 — a gentle rise, stays a low rush).

	-- CATCH DIAGNOSTIC (S36) — when true, the server logs every CatchSquirrel outcome (ACCEPT / the exact
	-- REJECT reason + carry + distance) and the client logs FULL-suppresses + phantom trims, tagged
	-- "[RescueDIAG]". This is the SYNC check for Chad's recurring "catches around 8-9/10 don't count":
	-- one flight's Output names the exact cause. Flip false (or delete) once the fix lands. Zero behavior.
	-- S36: the fix (velocity-aware gate below) is Chad-confirmed ("seems good") → flag OFF; code stays gated.
	catchDiag        = false,
	-- CATCH DISTANCE GATE (S36 fix) — the server's sanity bound on how far the eagle may be from a squirrel
	-- when it claims a catch. The old flat `triggerRadius*2.5` (=105) clipped LEGIT catches: the big (2.3×)
	-- eagle flying fast is 80-110 studs out by the time the catch reaches the server (diag-confirmed
	-- REJECT:dist at 106-110) → phantoms → "8-9/10 don't count". Now VELOCITY-AWARE: base slack + the
	-- distance the eagle covers during ~catchGateLatency. Still bounds gross teleport-claims (PvE, low-stakes).
	catchGateMult    = 3.5,    -- base gate = triggerRadius × this (=147 at triggerRadius 42; covers eagle size).
	catchGateLatency = 0.25,   -- + eagleSpeed × this (studs of fly-past slack; scales with boom-and-zoom speed).

	-- MISSION SELECTOR (Packet-03 §4, queue #15). The active mission from GameConfig.Missions below.
	-- "ember_valley" = the shipped behavior (S50: the fire round is what Chad flies). Flip this ONE
	-- string to switch missions (e.g. to "fox_on_the_hill" once THE FOX lands). Every mission field falls back to the current literal, so an
	-- absent/malformed mission reproduces today's round exactly — the selector is inert by construction.
	activeMission    = "ember_valley",       -- GATE B (S41, Chad "fire is gone?"): flipped ON the FIRE mission — a
	                                         -- wind-driven burn sweeps the valley (FireGrid clock + FireVisuals).
	                                         -- resolveMission reads ONCE at boot → RESTART Play to load it.
	                                         -- Revert to "waterfall_meadow" for the fireless shipped round.
}

-- ENVIRONMENT-VARIED MISSIONS (Packet-03 §4). One sacred loop (scan→dive→catch→deliver); the WORLD varies
-- it. A mission = one hazard driver + deliver geometry + critter-placement rule + ≤1 special verb, as a
-- DATA TABLE consumed by RescueServer at round start. Missions change how you FLY, never how you CATCH (P8),
-- and are ALWAYS non-lethal (P3). Fields mirror the current hard-coded Rescue values so the resolver can
-- fall back to them — waterfall_meadow is therefore byte-identical to the shipped round.
GameConfig.Missions = {
	waterfall_meadow = {
		displayName = "Waterfall Meadow",
		hazard    = { kind = "triage", triageAt = 60, cadence = 4, minStranded = 2 },  -- the shipped triage timer
		deliver   = { kind = "waterfall" },                                            -- ride the updraft to the pad
		placement = { groveCount = 4, perchPerGrove = 5, groveRadius = 130, groveMinGap = 520 }, -- disc groves
		verb      = nil,                                                                -- no special verb (free routing)
	},
	-- THE FOX (#15b, Packet-03 slice 4) — a ground-locked patrol that adds ROUTING PRESSURE (never lethal, P3).
	-- A single Y-locked fox trots from its den to a grove; on approach the grove FLARES orange + the squirrels
	-- there go frantic (a "come rescue us NOW" tell); if it reaches them they SCATTER to fresh perches — costing
	-- you TIME (farther/spread), never lives. The fox visibly loses (sits, then trots home). Flip activeMission
	-- to this to Play it (resolveMission reads once at boot → restart Play). All numbers live in hazard below.
	fox_on_the_hill = {
		displayName = "Fox on the Hill",
		hazard = {
			kind = "fox",
			-- NUMBERS (Fable-5 post-audit): the den sits ~1360 studs out, so TRAVEL time dominates, not den-idle.
			-- At speed 40 a den→grove trip is ~35-45s → patrol #1 lands ~t=55-70s with a chance of a #2; the old
			-- speed 24 gave ONE late patrol or none (Chad would judge the fox's ABSENCE, not its beat). Still
			-- 3-9× slower than the eagle → a CLOCK you can beat, never a chase.
			firstAt         = 100,  -- timeLeft when patrols begin (a ~20s calm opening; mirrors the triageAt idiom)
			cadence         = 8,    -- s idle in the den between patrols (travel paces the round; keep dead air short)
			speed           = 40,   -- studs/s trot — travel ~35-45s per leg; the eagle glides ~3× faster (a clock)
			flareRange      = 600,  -- flare fires this far out → 600/40 = 15s warning (reach-if-you-commit window)
			arriveRange     = 40,   -- "reached the grove centre" → the squirrels scatter
			franticRadius   = 170,  -- groveRadius(130)+40 slack → the whole grove panics together
			relocateMinDist = 400,  -- scatter target ≥ this from the fox (vs groveMinGap 520 → usually a NEW grove)
			scamperTime     = 1.4,  -- s of server-animated hop to the new perch (uncatchable but READABLE, not a teleport)
			dwell           = 3,    -- s the fox sits ("aw, missed 'em") before trotting home — it visibly loses
			minTargets      = 2,    -- a grove needs ≥ this many stranded squirrels to be worth a patrol
			footY           = 4,    -- fox pivot height above the Y=0 baseplate top
			foxScale        = 3.0,  -- bumped (Chad S38: "no fox seen") — plus a tall danger beacon + 🦊 billboard track it
		},
		deliver   = { kind = "waterfall" },                                            -- same ride home
		placement = { groveCount = 4, perchPerGrove = 5, groveRadius = 130, groveMinGap = 520 },
		verb      = nil,
	},
	-- EMBER VALLEY (packet B1, Commit 2) — the FIRE mission. hazard.kind="fire" swaps the fake triage
	-- balloon-lift for the real FireGrid clock: a wind-driven burn sweeps the valley; when the fire ring
	-- walls a grove off from safety (cut-off), a short cutoffGraceS countdown then lifts THAT grove's
	-- still-stranded squirrels on the SAME Ranger Balloon (P3 non-lethal — no damage path, ever). The fire
	-- wind is FICTIONAL (shapes the burn only); it NEVER touches FlightPhysics/WIND_BASE/the kernel. This
	-- mission WAS documented as INERT-by-default ("activeMission STAYS waterfall_meadow"). ⚠️ That has been
	-- FALSE since the fire shipped: `activeMission = "ember_valley"` above makes THIS the mission you fly —
	-- ember_valley IS the shipped mission now, not an opt-in.
	-- The same file contradicting itself 100 lines apart is how a next agent mis-attributes a live bug. Same FLY/CATCH/deliver as waterfall_meadow
	-- (P8: missions change how you FLY, never how you CATCH); only the hazard differs. All dials → GameConfig.Fire.
	ember_valley = {
		displayName = "Ember Valley",
		hazard    = { kind = "fire", igniteAt = 10, cutoffGraceS = 8 },  -- igniteAt is backfilled from GameConfig.Fire below (Fire is defined after this table); the 10 here is just a same-typed placeholder
		deliver   = { kind = "waterfall" },                                            -- same ride up the waterfall
		placement = { groveCount = 4, perchPerGrove = 5, groveRadius = 130, groveMinGap = 520 }, -- same disc groves
		verb      = nil,
	},
}

-- FIRE-CELL GRID (packet B1, MASTER-PLAN §3.3 + docs/rescue-consults/B1-FireGrid-design.md §2).
-- The fire is the CLOCK made visible (P2 pressure, P3 non-lethal — there is NO damage path; a
-- cut-off grove's squirrels leave by the EXISTING Ranger Balloon triage). Consumed by the PURE
-- FireGrid.luau (server per-round in Commit 2; INERT in Commit 1 — nothing reads this yet). Every
-- number is a §3.3 dial tuned by the headless SIM (tests/firegrid.spec.luau) BEFORE it renders.
-- The wind vector is FICTIONAL (reshapes the burn per round = replayability with one number) and
-- NEVER touches FlightPhysics SetWind/WIND_BASE — the flight kernel is untouched.
GameConfig.Fire = {
	cellSize        = 60,      -- studs per fire cell → 54×54 bounding grid over the r=1600 valley.
	gridHalf        = 1620,    -- half-extent of the square cell grid (2·1620/60 = 54 cells/side).
	tickHz          = 2,       -- fire ticks per second (server steps it from its 0.25s loop).
	annulusInner    = 300,     -- burnable forest annulus inner radius (clear centre, no fire on the mesa)…
	annulusOuter    = 1400,    -- …outer radius (≈1490 burnable cells; §3.3 band 1300-2000).
	padClearRadius  = 400,     -- the top-of-waterfall safe pad (deliverPos ~1150,0) is SACRED — never burns.
	smolderMin      = 15,      -- a cell smolders 15-25s (drawn from the LCG) before it BURNS…
	smolderMax      = 25,
	burnTime        = 20,      -- …then BURNS ~20s before going EMBER (inert). Spread is active SMOLDER→BURNING.
	spreadP         = 0.045,   -- THE front-speed dial: E[neighbor-ignite delay]=1/(2·0.045)=11.1s → front ≈5.4 studs/s.
	                           -- Kept at the derivation value; SIM front-speed mean 5.4 studs/s (§3.3 [4.8,6.2]).
	crossFrac       = 0.50,    -- crosswind neighbor gets spreadP·crossFrac; downwind spreadP; upwind 0 (wind shapes the burn).
	                           -- The REAL threat/cut-off lever is the ignition LINE width (seedArcDeg + seedCount), NOT lateral
	                           -- spread — cranking crossFrac flattens the wind character into a growing blob (Fable B1 audit).
	                           -- 0.50 keeps a 3:1 wind-elongated front (downwind ≫ crosswind, upwind 0).
	seedCount       = 8,       -- ignition seeds spaced EVENLY across the upwind arc → a broad fire LINE the wind blows downwind
	                           -- as a wall. 8 seeds over the 180° arc give grove-threat mean 0.40, cut-off mean ~3.8 (§3.3 band)
	                           -- with front speed IN the [4.8,6.2] window — no band-widening (Fable B1 after-audit fix).
	seedArcDeg      = 180,     -- the upwind arc (degrees) the ignition line spans; wider = a broader front, more of the valley lit.
	igniteAt        = 10,      -- t≈10s into the round the seeds light (a calm opening before the clock starts).
	cutoffCheckEvery= 4,       -- run the BFS cut-off check every N ticks (O(2916); cached between).
	maxVisualBurning= 30,      -- B2 hard-cap: at most 30 BURNING cells get a flipbook+emitter+light.

	-- B2 VISUAL layer (FireVisuals.luau). Server-side, replicates via a Workspace folder; INERT under
	-- waterfall_meadow (built only when the fire is active). The pool is created ONCE at maxVisualBurning
	-- and re-positioned/re-colored each render — no per-tick create/destroy.
	visualEvery     = 2,       -- render the fire visuals every N fire-ticks (fire steps at tickHz=2; N=2 → ~1 Hz
	                           -- visual refresh). Keeps the per-render cost well under the ≤1.5ms/tick server budget.
	markerHeight    = 34,      -- studs the burning marker column stands (base at ground → reads over low canopy).
	lightRange      = 90,      -- warm PointLight range per burning marker (modest — 30 lights hard-capped).
	lightBrightness = 2.0,
	smokeRate       = 6,       -- particles/sec per burning marker's ONE smoke emitter.
	emberTint       = Color3.fromRGB(46, 34, 28),  -- charred-ground tint for EMBER cells (cheap color swap, NO light/emitter).
	maxVisualEmber  = 40,      -- coarse cap on ember tint patches (a pooled subset — ember is a free color, but instances aren't free).

	-- THE SMOLDER TELL (S44). Chad: "it takes a while for me to see fire too." Convicted mechanism:
	-- the burn seeds at igniteAt=10s, then each cell smolders smolderMin..smolderMax (15-25s) — and
	-- FireVisuals rendered BURNING ONLY, so SMOLDER had NO visual at all. First flame therefore
	-- appeared ~25-35s in (clock ~1:35) and accreted cell-by-cell. Meanwhile spread is ACTIVE from
	-- SMOLDER and Danger keys off any non-GREEN cell, so an INVISIBLE front was propagating and
	-- panicking squirrels for up to 25s (Stage 3a fear pointing at nothing you could see).
	-- The fix renders the state as what it physically IS — smoke, no flame — which keeps the
	-- smoke→flame escalation. Do NOT "fix" this by cutting smolderMin/Max instead: smolder SPREADS,
	-- so that races the entire front — a balance change wearing a visibility costume.
	smolderTell     = true,    -- false → no wisp pool is even allocated (byte-identical to pre-S44).
	maxVisualSmolder= 14,      -- pooled wisp cap. Worst case is rail-checked in tests/perf.spec.luau
	                           -- against the SAME §3.4 particle budget as the flames.
	smolderWispRate = 5,       -- particles/s per wisp…
	smolderWispLife = 3.2,     -- …× this max lifetime = alive per wisp. 14 × 5 × 3.2 = 224 alive worst
	                           -- case, on top of the ~1160 the flames already use (rail 1600).
	smolderWispY    = 8,       -- studs; wisp origin above the ground — low, so it reads as ground-level
	                           -- smoke curling up out of the brush rather than a floating cloud.
	                           -- NOTE: deliberately NO PointLight on a smoldering cell. Lights are the
	                           -- low-end/mobile cliff; §3.4 pins 30 with zero shadow-casters and this
	                           -- tell must not move that number.
	ringColor       = Color3.fromRGB(255, 90, 40),  -- cut-off countdown warning ring (grove about to be ranger-rescued).
	perfDiag        = false,   -- print "[Fire] tick N cells=%d ms=%.2f" per render when true (OFF by default).

	-- ── STAGE 2 (kid-legibility): fire reads as FIRE (docs/KID-LEGIBILITY-PLAN.md §Stage-2). ──
	-- IDENTITY GATE: flameStyle="static" + smokeLeanStrength=0 reproduce the pre-Stage-2 render
	-- byte-identically (no teardrop, no flame emitter, no plume, smoke straight up). Flip
	-- flameStyle="flicker" (+ smokeLeanStrength ~22) to fly the read; flip back = today.
	flameStyle      = "flicker", -- S41 Stage 2 LIVE (Chad "the fire is square"): teardrop + flames + leaning smoke + centroid plume. Revert to "static" for the old boxes.
	smokeLeanStrength = 22,     -- studs/s^2 of horizontal smoke Acceleration along grid wind. ~40deg downwind lean = spread telegraph. 0 = straight up (inert).
	                           -- FLIGHT VALUE 22: atan(22/26) ≈ 40° lean — plume clearly points downwind but still rises.
	flameRate       = 10,      -- flame particles/sec per burning marker (vetted 8-12). 30 markers × 10 = 300/s peak of
	                           -- small additive sprites — the engine animates flicker at 60fps for free; server cost 0.
	flickerAmp      = 0.15,    -- ±15% marker-height breathing per ~1Hz sync (vetted). Arithmetic-only, off tickN + cell phase.
	teardropLeanDeg = 8,       -- degrees the teardrop tip leans DOWNWIND ("a few degrees" — readable, not toppled).
	teardropCoreRatio = 0.45,  -- inner yellow core size as a fraction of the outer shell (two-tone taper).
	plumeRateBase   = 2,       -- centroid mega-plume: Rate = min(plumeRateMax, base + perCell·burningN) —
	plumeRatePerCell= 0.5,     -- the plume GROWS with the fire = a free urgency channel.
	plumeRateMax    = 20,      -- cap: ONE emitter, ≤20/s of 6-10s-lived particles ≈ ≤200 alive worst case.
	plumeRise       = 14,      -- studs/s^2 vertical Acceleration (slow rise + long life = a tall horizon column).
	plumeSizeMin    = 20,      -- particle Size grows 20 → 80 over its life (vetted): a smudge readable from scan altitude.
	plumeSizeMax    = 80,

	-- ── STAGE 3a (kid-legibility): URGENCY BECOMES VISCERAL — the critter's own FEAR. ──
	-- A kid needs no decoding to read a scared animal, so the squirrel itself carries the clock:
	-- the server stamps a Danger 0/1/2 attribute from pure FireGrid arithmetic (FireGrid.dangerAt)
	-- and the client renders it in Stage 1's ALREADY-SHIPPED vocabulary — pose speed, hop
	-- amplitude, halo colour/flash. Fear must LEAD the loss (it is pressure you can still act on,
	-- never a notification that a grove is already gone), which is why it keys off fire DISTANCE
	-- and not off fireGraceUntil. Non-lethal as always (P3): a squirrel you miss still leaves on
	-- the Ranger Balloon — panic means "help me faster", never suffering.
	-- IDENTITY GATE: urgencyTells=false FROM BOOT ⇒ the server never writes Danger, so every client
	-- branch is dead ⇒ byte-identical round. Honest caveat: the client branches are gated on the
	-- ATTRIBUTE, not on this flag, so flipping it false mid-session leaves already-stamped squirrels
	-- rendering their last value. The revert is edit + restart, which is how it is always used.
	urgencyTells    = true,    -- S43 Stage 3a LIVE for Chad's flight (revert to false = the pre-Stage-3 round).
	dangerFarR      = 280,     -- Danger 1 "worried": fire within 280 studs. At the SIM'd ~5.4 studs/s front
	                           -- that is ~50s of warning — early enough to re-route, late enough to mean it.
	dangerNearR     = 140,     -- Danger 2 "PANIC": within 140 studs ≈ 26s out. Comfortably LEADS cut-off +
	                           -- the 8s cutoffGraceS balloon, so panic is actionable pressure, not a loss notice.

	-- ── STAGE 3c (kid-legibility): the fire ROARS. ──
	-- The second urgency channel: a world that gets audibly worse. ONE looping Sound whose Volume is a
	-- STATELESS per-frame function of the eagle's distance to the nearest burning cell
	-- (RescueRules.fireRoarVolume) — the S37/S38 architecture that closed the audio saga. No envelope,
	-- no tween, no stop-timer ⇒ "it kept roaring after the fire went out" is arithmetically impossible.
	fireAudio       = true,    -- S43 LIVE for Chad's flight. false FROM BOOT = no Sound is ever created
	                           -- (byte-identical). Flipping it false mid-session just skips the update
	                           -- block, so an existing roar would hold its last volume — revert = restart.
	roarNearR       = 200,     -- studs: full roar at/inside this range of the nearest burning cell.
	roarFarR        = 900,     -- studs: silent at/beyond. ~2 grove-gaps: the fire is heard before it is legible.
	roarMaxVol      = 0.35,    -- sits just under the flight wind's ceiling so it layers, never masks. (The old
	                           -- comment cited "windMaxVol 0.38"; that knob is 0.5 — the rationale drifted.)
	roarGamma       = 1.5,     -- >1 = the roar swells as you COMMIT toward the fire, instead of a flat fade.
	roarPitch       = 0.85,    -- a shade below source pitch = bigger/deeper fire.
	-- ⚠️ ASSET IDS ARE THE ONE THING NO HEADLESS GATE CAN VERIFY. Roblox's 2022 audio-privacy change
	-- privated the vast majority of pre-2022 third-party audio and new uploads default to private, so a
	-- web-sourced id is a COIN FLIP. The client walks this list in order and prints a one-glance verdict
	-- per id, so a dead id costs a line in Output instead of a wasted test flight.
	-- ── S43 STUDIO RESULTS — three web-sourced guesses, three different failures: ──
	--   9046850948 → "Asset type does not match requested type" (not audio at all)
	--   4604808051 → "Asset is not approved for the requester" (the 2022 privacy purge)
	--   1836383941 → loaded fine… and is MUSIC. Chad: "I didn't really hear fire, I heard music."
	-- THE LESSON, which is why this list is now EMPTY: a load gate proves an asset RESOLVES, never that
	-- it is the right SOUND. Guessing ids off the web burns a real playtest per attempt at no better
	-- than chance, so the id must come from someone who can PREVIEW it — i.e. from Studio's Toolbox,
	-- where you hear it before you paste it. Silence is strictly better than music-as-fire: it is
	-- honest about being unfinished, where wrong audio actively lies about what the fire is.
	-- TO FILL IT: Studio → Toolbox → Marketplace → Audio → search "fire crackling" → PREVIEW it →
	-- Copy Asset ID → paste below → Stop and re-Play (the chain is walked once per session).
	roarAssetIds    = { },
}

-- Backfill ember_valley's fire ignite time from GameConfig.Fire (Fire is defined AFTER Missions, so the
-- literal above could not reference it inline). Keeps the single source of truth: nudge Fire.igniteAt only.
GameConfig.Missions.ember_valley.hazard.igniteAt = GameConfig.Fire.igniteAt

GameConfig.Thermals = {
	count = 8,
	radius = meters(110),     -- ≈393 studs
	minHeight = meters(30),   -- ≈107 studs
	maxHeight = meters(1800), -- ≈6428 studs (carries you into the high soaring band below the 7143 ceiling)
	strength = 140 * GRAV_SCALE, -- upward accel at the core (studs/s^2), ratio-to-gravity preserved
}

--------------------------------------------------------------------------------
-- DEBUG — development aids. `soloFlight` lets ONE player spawn and fly for a smoke
-- test WITHOUT a full Eagle-vs-Crow match (no scored round; AI crows + possession
-- still run so a solo crow can exercise the squad). It transitions cleanly into a
-- real scored match the moment a second team joins. Set false for production.
-- (Consumed by GameServer.RoundLoop.)
--------------------------------------------------------------------------------
-- VISUAL — cosmetic-only knobs (no gameplay/balance effect).
GameConfig.Visual = {
	birdScale = 2.3,  -- VISUAL scale of bird models so they read at range (1.8→2.3 per Chad: "make the eagle
	                  -- bigger"). Collision + attack reach still use profile.bodyRadius, so this does NOT change
	                  -- combat distances/balance — it only changes how much of the screen the bird fills (framing).
	                  -- The client's talon/seat offsets multiply by this so riders sit ON the scaled body.
}

--------------------------------------------------------------------------------
-- MAP — the world's readability knobs. Cosmetic only (see GroundDetail: every patch is emitted
-- non-queryable, so BirdCollision cannot see them and the crash surface is unchanged).
--------------------------------------------------------------------------------
GameConfig.Map = {
	-- S45, Chad: "I nose down into the ground every time… I'm diving so the ground will render but it
	-- does so too late." BuildMap's own comment says patches exist because "a flat green plain gives no
	-- perspective cue" — and then placed EVERY landmark outside the ±1200 spawn core (patches, river,
	-- buttes all say so in-line). So the box the player actually flies in is a uniform 2,400-stud plane
	-- with nothing to judge height against. This scatters multi-scale flat patches back INTO it.
	coreDetail     = true,   -- KILL SWITCH: false → GroundDetail returns {} and the map is byte-identical.
	coreHalfExtent = 1200,   -- must track GameServer's spawn box (random ±1200 XZ). Bigger = wider carpet.
	coreDetailSeed = 20260721, -- deterministic LCG seed; the valley is identical on every boot.

	-- S45c — THE RENDER FIX, and the cause is now MEASURED, not guessed. The boot probe proved every
	-- part is on the client at t=1s and the counts never climb (so: not replication, not streaming —
	-- StreamingEnabled read false — and zero external assets means nothing downloads). Chad then set
	-- Graphics → Manual → max and the problem VANISHED. That convicts quality-scaled render distance:
	-- Roblox's Automatic mode throttles how far it draws parts to protect the frame budget, so parts
	-- exist but go undrawn until you fly at them. His slider fixes it for HIM only — every player lands
	-- on Automatic — so the shippable fix is to stop spending the frame budget:
	--   • CastShadow=false on Map parts. addPart never cleared it, so all 78 cast shadows (a
	--     16,000-stud baseplate; 45 mountains up to 3,400 tall). RescueServer's builder has always set
	--     false, so the trees cast nothing — this was an inconsistency, not an art choice.
	--   • The Sea stops being GLASS with Reflectance. It is 6,000 × 18,000 studs of transparent
	--     reflective surface across the whole western horizon; at 11,000 studs it reads the same as
	--     SmoothPlastic and costs a fraction.
	-- false = the pre-S45c look, byte-identical (shadows back on, glass sea back).
	cheapScenery   = true,

	-- M1 (Phase V) — KILL SWITCH for the Terrain valley + lighting rig v2 (src/shared/WorldGen.luau).
	-- Defaults ON: the new sculpted Terrain bowl + golden-hour lighting IS the map now (the heading
	-- requires the published look to visibly change; machine gates protect correctness; Chad flies
	-- only at gates). false = the untouched legacy gray-box path (SetupLighting + BuildMap) exactly,
	-- Terrain cleared. One-word revert. Legacy retained until Gate V passes, then deleted.
	worldV2        = true,

	-- M4-B (Phase V mesh pass, S50) — one-word kill switches for the curated free-asset mesh visuals
	-- layered onto worldV2 (src/shared/MeshFactory.luau + src/shared/AssetManifest.luau). Each defaults
	-- true; false restores the prior geometry/behavior byte-identically (honesty rule) — flag off means
	-- ZERO new mesh Instances, not "fewer".
	meshTrees   = true,  -- procedural tree Trunk/Canopy parts (RescueServer.buildTree) go Transparency=1
	                      -- and a curated tree mesh (AssetManifest.props.treeLeafy/treePine) is built at
	                      -- the same base position, scaled to the same total visual height.
	meshScatter = true,  -- WorldGen.scatterPlan's deterministic rocks/bushes/logs (≤140 total, rim base
	                      -- ring + shores + meadow annulus) get built as mesh Models.
	meshSky     = true,  -- Lighting.Sky gets AssetManifest.skybox's six textures + sun; the existing
	                      -- Atmosphere/ColorCorrection/Bloom golden-hour rig is untouched.
}

-- AMBIENT ATMOSPHERE — the drifting "burning valley" field (drifting embers, ash, warm motes).
-- Consumed ONLY by src/client/AmbientFX.client.luau (an isolated client LocalScript; does NOT
-- touch BirdController / the LOCKED controls, nor GameUI). Purely cosmetic, client-side particles.
--
-- LEGIBILITY CONSTRAINT (subordinate to kid-legibility, which is SIGNED OFF and must NOT regress):
-- these particles are subtle, sparse, peripheral MOOD — never a screen-filling storm. A kid must
-- still instantly spot a squirrel/beacon through them. Rates/sizes are deliberately low; err toward
-- too-subtle. Tune DOWN, not up.
--
-- KILL SWITCH: enabled=false → AmbientFX early-returns and creates ZERO instances (one-flag revert).
GameConfig.Ambient = {
	enabled     = true,   -- master kill switch. false = AmbientFX does nothing, no host part, no emitters.
	emberRate   = 10,     -- embers/sec (warm orange→amber glowing motes). Keep single digits-ish.
	ashRate     = 8,      -- ash specks/sec (tiny gray, drifting down).
	hazeEnabled = true,   -- the very-faint golden haze motes. false = skip that emitter entirely.
	hazeRate    = 4,      -- haze motes/sec. Larger + extremely transparent (0.85→1). Barely-there.
	volume      = 320,    -- host-part XZ extent (studs). Emitters emit from this VOLUME around the
	                      -- camera, NOT a point at the lens, so particles spawn at varied depths and
	                      -- near-lens/dead-center density stays low. Bigger = sparser/deeper field.
}

GameConfig.Debug = {
	soloFlight = true,
	-- aiCrowOpponents: when a lone HUMAN Eagle is present with no human Crow, spawn a fully
	-- AI-controlled crow squad that HUNTS and rams the eagle, respawning in fresh waves of
	-- `Squad.size` so a solo player can practice the talon/beak parry without a second human.
	-- The bots count for NOTHING in scoring (round logic still keys off human players); they
	-- exist only in the soloFlight branch and are cleared the moment a real match can be fielded.
	-- EAGLES-TO-THE-RESCUE (S32): combat is shelved — the solo eagle now flies a rescue round,
	-- NOT a dogfight. No crows on the field. Flip back to true only to revisit combat.
	aiCrowOpponents = false,
}

--------------------------------------------------------------------------------
-- SECURITY — server-side anti-cheat envelope (GameServer.processAntiCheat). The client
-- OWNS its bird's physics (SetNetworkOwner) for latency-free flight, so the server cannot
-- trust the CFrame/velocity it reads. It instead measures SERVER-OBSERVED speed from
-- position deltas and validates against a generous envelope, using a leaky bucket so honest
-- dives/loops/lag spikes never false-positive. Response ladder: rubber-band → reclaim
-- ownership → kick, with strikes that decay over time. Tune conservatively — a false
-- positive on an honest player feels awful. See docs/HANDOFF.md queue #5.
--------------------------------------------------------------------------------
GameConfig.Security = {
	enabled = true,
	-- Sustained-speed ceiling = Flight.TERMINAL_VELOCITY × this. We base it on TERMINAL (not a
	-- profile's diveSpeedCap) because the honest FlightPhysics engine clamps top speed to
	-- TERMINAL, so an honest bird FREE-FALLING (not diving — diveSpeedCap doesn't apply) can
	-- legitimately approach TERMINAL. Basing the ceiling on diveSpeedCap would false-positive a
	-- plummeting crow. A real speed hack still blows past TERMINAL×1.25.
	speedAllowMult = 1.25,    -- => ~525 studs/s sustained ceiling at TERMINAL 420 (legit flap-top 420 stays inside)
	teleportMult = 1.5,       -- per-frame teleport gate = TERMINAL × this (~630); single-frame jump
	pingPadStuds = 30,        -- extra teleport-gate slack for replication jitter/lag
	bucketCapacity = 120,     -- accumulated overshoot (studs) before a strike is issued
	bucketLeak = 200,         -- bucket drains this many studs/sec (transient spikes forgiven)
	graceSeconds = 0.5,       -- ignore window right after a legit server move (spawn/swap/respawn)
	reclaimStrikes = 3,       -- strikes before the server reclaims network ownership
	kickStrikes = 6,          -- strikes before the player is kicked
	strikeDecaySeconds = 12,  -- one strike forgiven per this many seconds of clean flight
}

--------------------------------------------------------------------------------
-- ROUND / SCORING
--------------------------------------------------------------------------------
GameConfig.Round = {
	intermission = 8,
	roundLength = 240,
	respawnDelay = 4,
	-- Win conditions: eagle dead -> crows win; all 4 crows dead -> eagle wins.
}

--------------------------------------------------------------------------------
-- CAMERA — consumed by BirdController (do not rename keys; client reads these)
--------------------------------------------------------------------------------
GameConfig.Camera = {
	baseDistance = 40,
	mouseAimDistance = 230, -- STRICT FIXED chase distance while in mouse-aim mode (wheel zoom ignored there; pulled well
	                        -- back behind the bird — player ask). 135→200→230 (S26, Chad live-tuned: "zoom back ~50%…
	                        -- back up to 230, a little more — I need to see more around me in the turn"; 200 was
	                        -- "a lot better"). Revert toward 135 if the eagle reads too small to fly.
	-- MOUSE-AIM CAMERA HEADING LAG (rad/s): how fast the aim camera's view catches up to the bird's heading. LOW
	-- lag = when you fling the cursor and the bird turns, the view DOESN'T snap around, so the world-anchored
	-- cursor hangs out on screen and the nose visibly LAGS behind and chases it. Higher = the view
	-- tracks the heading tighter (cursor re-centres sooner). Too low and the bird drifts to the frame edge in a
	-- long hard turn. Only used in mouse-aim mode; chase-only/free-look use chaseTurnRate.
	-- ⚠️ PREFERENCE REVISED (Chad, for the record, 2026-07-02 S26c): "I don't like the aim heading lag —
	-- that is NOT my preference, I wanted it tighter… the reticle should snap tighter to the cursor."
	-- 1.4→2.6→4.0 (lag time-constant ~0.71s→0.38s→0.25s). S26d: 2.6 was good but mouse-aim COMBINED with
	-- full-deflection keystrokes still wanted more turn authority (the laggy view read as sluggish
	-- response during combo flying) → 4.0 per Chad.
	-- ⚠️ S30 (2026-07-13): the Stage-1 decouple RE-SCOPED this knob (it now lags the AIM, not the bird
	-- heading). Lowering it to 1.6 to let the world-cursor reach the screen edge was FLOWN + REJECTED (Chad:
	-- "slow and porposing… still a cone… I don't have full authority of the cursor"). REVERTED to 4.0 (the
	-- loved Stage-1 baseline). Root problem: the world-anchored cursor + camera-eases-ONTO-aim structurally
	-- re-centres the cursor, so no lag value gives independent screen authority. Chad chose the proper fix —
	-- a SCREEN-REFERENCED independent cursor + a lag-FOLLOW camera + a deterministic cascade driven by the
	-- free cursor (Fable architecture consult; see docs/HANDOFF.md). This knob is part of that redesign.
	-- S30 Stage 1: LOOSENED 4.0→2.5 for the FREE-CURSOR mode — the camera catches the aim slower so the freed
	-- cursor roams out and is "not pulled back to centre quick" (Chad). Only meaningful with aimFreeCursor=true
	-- (with the clamps on they re-centred the cursor regardless of lag). Sweep 2.0-3.5; view too floaty → raise.
	-- S30: 2.5→2.2 (Chad: "a little wee bit more lag") — the freed cursor roams a touch further before re-centre.
	aimHeadingLag = 2.2,
	baseFOV = 70,
	diveFOV = 95,
	climbFOV = 62,
	speedDistanceFactor = 0.06,
	bankTiltFactor = 0.6,
	followSmoothing = 0.18,
	shakeAtSpeed = 200,
	shakeMagnitude = 0.6,
	-- Spacebar free-look: orbit the camera while the bird holds its flight path.
	-- Raised 0.35 -> 0.7 (player ask 2026-06-30: free-look "gets stuck"/feels limiting — want a crisp,
	-- near-1:1 War-Thunder-arcade pan). Drives both the mouse orbit (·0.01) and the arrow-key orbit (·2.5).
	freeLookSensitivity = 0.7,
	-- FREE-LOOK VERTICAL LIMIT (CS-2 REVISED, Chad 2026-06-30): the free-look camera is an UPRIGHT spherical
	-- orbit — horizon always level, operator never rolls or inverts ("stay upright, not sideways or upside
	-- down"). Yaw is a full 360°; pitch tilts up/down to this many degrees short of straight-up/straight-down
	-- (a SOFT STOP at vertical — it does NOT continue over the top to the far side). Keeping it just under 90°
	-- holds the view off the exact pole so CFrame.lookAt (hard world-up) never degenerates. Raise toward 90 for
	-- a steeper straight-down/up look, lower if the extreme ever jitters. (BirdController updateCamera.)
	freeLookPitchLimitDeg = 85,
	-- FREE-LOOK CRISPNESS (player ask 2026-06-30 "it feels like it gets stuck"). The orbit used to reuse the
	-- heavy chase followSmoothing (0.18 ≈ 16%/frame) → the view crawled after the mouse ("molasses"). When
	-- free-look is active the final camera lerp uses THIS smoothing instead → near-1:1 tracking. It's the single
	-- biggest "unstick" knob: higher = crisper (toward 1:1), lower = floatier. ~1.0 ≈ 63%/frame at 60fps.
	freeLookSmoothing = 1.0,
	-- RIGHT-CLICK = AWARENESS ZOOM (player asks 2026-06-30: "zoom right click is for looking outward… it should
	-- zoom to the centre of where I'm heading"; "the zoom right click cannot affect flying, it is for awareness
	-- only"; "the mouse wheel is for zooming in and out with reference to eagle"). Hold RMB to MAGNIFY the view
	-- centred on your heading: it narrows the FOV toward aimFOV and FADES the eagle locally (aimFadeBird) so its
	-- opaque body can't block the view. It does NOT move the camera or swing the look (that offset the zoom and
	-- fed back into flight) — the look stays heading-centred and steering is SUSPENDED while zooming, so it's
	-- purely awareness. Eagle-distance is the MOUSE WHEEL's job (zoomStep/userZoom), in chase AND free-look.
	aimFOV          = 22,   -- narrower FOV while zoomed (from baseFOV 70) — the magnification (lower = stronger zoom). ~3.2x at 22
	aimFadeBird     = 0.85, -- how transparent the eagle goes at full zoom (0 = opaque, 1 = invisible) so it can't block
	aimZoomLerp     = 0.22, -- per-frame ease of the zoom factor toward 0/1 (higher = snappier zoom)
	-- Mouse-wheel zoom: scroll adjusts a user offset folded into the chase distance.
	-- The final distance is clamped to [minDistance, maxDistance] so you can't clip
	-- into the bird or fly the camera off to infinity.
	zoomStep      = 24,  -- studs of distance change per wheel notch (bigger range -> bigger step)
	minDistance   = 10,  -- closest the chase cam may sit
	maxDistance   = 800, -- farthest the chase cam may pull back (raised 340->800: zoom way out for situational awareness)
	-- Camera HEIGHT vs zoom: the chase cam rides at `heightFactor × dist` above the bird up close,
	-- rising toward `zoomTopDownHeight × dist` as you wheel out — so zoomed out gives a higher,
	-- more top-down view of the eagle.
	heightFactor     = 0.35, -- base height ratio when zoomed in (a little above the bird)
	zoomTopDownHeight = 0.55, -- height ratio at full zoom-out (a touch more top-down, but a happy medium — angles stay close)
	-- CHASE FRAMING (S30, Chad: "the mouse aim + eagle nose pointer should rest at CENTRE screen; drop the view
	-- of the bird DOWN — the bird should sit LOWER"). The cam rides ABOVE the bird (heightFactor) and looks a touch
	-- AHEAD, which tilts the look DOWN onto the bird and pushes the forward AIM above centre. Raise the chase look
	-- target along the camera-up by this fraction of the follow distance to tilt the view UP: the resting aim drops
	-- to screen CENTRE and the bird sits LOWER. 0 = old framing (bird ~centred, aim high). Aim still ABOVE centre /
	-- bird too high → raise toward 0.32; aim now BELOW centre / bird too low → lower toward 0.18. Chase-mode only.
	chaseLookUp = 0.25,
	-- Loop/crest damping: when the bird's facing rotates fast (e.g. cresting a loop), the chase
	-- cam softens its follow so the view glides through the flip instead of whipping. crestAngRate
	-- is the facing angular speed (rad/s) at which damping reaches full; crestDamp is the max
	-- fraction by which follow smoothing is reduced. Camera-only — never touches flight.
	crestAngRate = 3.0,  -- facing rotation rate (rad/s) at which crest damping reaches full (lower = damps earlier)
	crestDamp    = 0.85, -- max fraction the follow is softened at the crest (higher = smoother loop top)
	-- CAMERA UP = PARALLEL TRANSPORT along the flight-path direction (BirdController.updateCamera). It carries the
	-- up vector WITH THE CHASE DIRECTION, so a LOOP arcs over the top with no pole-flip while a barrel/snap ROLL
	-- (which barely moves the flight path) leaves the camera steady so you WATCH the bird roll (player ask
	-- 2026-06-30). A gentle self-level pulls the horizon back to world-level in ordinary flight, FADED OUT as the
	-- flight path goes vertical (a loop) so it can't yank the camera to the degenerate world reference over the top.
	horizonLevelRate = 2.0, -- per-sec relax of the camera horizon back to world-level (0 = never self-levels; higher = snappier)
	loopVertStart = 0.5,    -- flight-path verticality (|dir.Y|, ~30°) at which the horizon self-level BEGINS fading out
	loopVertBand  = 0.4,    -- verticality span over which self-level fades fully off (start+band = 0.9 ≈ 64°, before the pole)
	loopFollowUp = 0.0,     -- LEGACY / unused (superseded by parallel transport — the old cf.UpVector lean spun the camera in rolls)
	-- NO-SNAP CHASE (deep-research RESEARCH §v4, anti motion-sickness). The chase direction is a single
	-- CONTINUOUS full-3D vector (bird->camera), slewed toward "behind the bird's motion" at a CAPPED
	-- angular rate — NOT rebuilt from the bird's horizontal heading (which reverses ~180° over a loop top
	-- and SNAPS the camera, the old nausea bug). Combined with Roblox CFrame:Lerp (shortest-path quaternion
	-- slerp), a full 360° loop arcs smoothly with no azimuth snap and no over/under pole flip.
	chaseTurnRate   = 3.6,  -- rad/s cap on how fast the chase direction slews (>= the bird's loop rate so it keeps up, but smooths fast flicks). THE no-snap knob.
	lookAheadFactor = 0.3,  -- how much the chase blends toward the VELOCITY vector vs the nose (0 = pure nose, 1 = pure velocity); a little lead reads the flight path
}

--------------------------------------------------------------------------------
-- CONTROLS — input scheme (consumed by BirdController). MOUSE STEERING: the mouse is locked
-- to screen center and drives a VIRTUAL STICK (mouse movement deflects it; it springs back
-- toward center at mouseReturn). The stick = analog pitch/roll, SUMMED with W/S/A/D. It starts
-- at exactly zero every life (NOT absolute cursor position — that spawned a spiral crash when
-- the real cursor wasn't centered). Free-look (hold Space) suspends steering and orbits the
-- camera. The lowered STABILITY_RATE is what lets your aim actually hold. PLAYTEST-PROVISIONAL.
--------------------------------------------------------------------------------
GameConfig.Controls = {
	-- ── B7-P1 TOUCH AIM (INERT — nothing reads these yet; B7-P2 wires the one-line seam). ──
	-- The touch analog of the aim law, as a pure DELTA SOURCE above BirdController:2495. The whole
	-- signed-off pipeline (world cursor, curl, rim-pin, CS-1, CS-8) lives downstream of that read and
	-- never knows the device, so touch is a source swap and NEVER a new control law.
	-- ⚠️ CHAD GATE: whether the finger MOVES the cursor (drag, these dials) or PLACES it (tap) is his
	-- call — B7-P2's proof-of-feel flight is what answers it. Do not build past P2 before he flies.
	touchAim        = false, -- INERT. true = the adapter feeds the aim seam on touch devices.
	touchDragGain   = 1.4,   -- finger-px → mouse units. Phone px are physically smaller than a mouse
	                         -- inch, so 1 px is worth ~1.3-1.6 to keep the SWING feeling mouse-equal.
	touchBandPx     = 60,    -- dead-band radius from where the finger LANDED. Inside it: pure 1:1 drag.
	                         -- Too big reads as "I keep swiping to hold a turn"; too small as "a nudge
	                         -- becomes a turn". Both are dials, not architecture (consult §1-A).
	touchHoldRate   = 6.0,   -- px/sec of synthetic delta per px of overshoot past the band — the
	                         -- rim-pin analog: a locked mouse has infinite travel, a finger does not.
	touchHoldMaxPxPerSec = 1400, -- clamp so a finger jammed at the screen edge can't spin the cursor.

	mouseSteer      = false, -- DISABLED for now (session 7) — keyboard-only control while we tune flight feel. Flip back to true to re-enable mouse nose-aim.
	mouseSensitivity= 1.0,   -- mouse-delta -> virtual-stick gain (higher = nose moves more per mouse inch)
	-- Spring of the stick back to center (per-sec) when you stop moving the mouse. SPLIT by axis:
	-- ROLL self-centers fast (wings level themselves -> anti-spiral, arcade-friendly), but PITCH
	-- barely returns so you can HOLD a climb or dive without continuously feeding the mouse. Pitch
	-- ~0 = the nose stays where you aim it (this is what makes sustained climbing possible).
	mouseReturnRoll  = 3.0,
	mouseReturnPitch = 0.4,
	mouseExpo       = 1.7,   -- response curve: 1 = linear, >1 = finer control near center
	-- (invertPitch used to be duplicated here — the LIVE key is the one further down with the aim knobs.
	--  Last-wins made this one dead; deleted S28 when the fixed verify tier finally flagged it.)
	-- TIME-SCALED PITCH (keyboard W/S): a TAP gives a small, precise AoA nudge; HOLDING ramps the
	-- pitch authority up to full over pitchRampTime. Lets you make fine attitude adjustments without
	-- losing strong authority on a sustained climb/dive. Releasing snaps the input to 0 (the nose
	-- then holds where you pointed it — there's no auto-level). PLAYTEST-PROVISIONAL.
	pitchTapFraction = 0.30, -- pitch authority on the first instant a key is pressed (the "tap" amount)
	pitchRampTime    = 0.55, -- seconds of holding to ramp from the tap fraction up to full (1.0)

	-- BANKING / YAW SMOOTHING (keyboard A/D, Q/E): the same tap->ramp shape as pitch so a tap eases
	-- the bank in (refined) instead of slamming to full deflection ("all or nothing"). Tone-down lever.
	bankTapFraction = 0.22, -- roll/yaw authority on the first instant the key is pressed
	bankRampTime    = 0.50, -- seconds of holding to ramp from the tap fraction up to full

	-- MOUSE-AIM (the default flight control; NOT free-look). WAR THUNDER instructor model: two
	-- on-screen markers — a freely MOVEABLE CURSOR (your desired direction of flight) and a NOSE
	-- CROSS projected out front of the bird (its boresight / where it actually points). The bird
	-- steers so the nose cross is ALWAYS APPROACHING the cursor; the control error is the screen-space
	-- gap between them. Centre the cursor on the cross = fly straight. Hold the cursor at the screen
	-- EDGE and the gap is unreachable (the chase cam keeps the heading near centre) -> the command
	-- stays saturated -> a sustained FULL TURN or LOOP (uncapped — this is the key difference from the
	-- old attitude-command model, which capped at an attitude and could not loop). Eased (aimResponse)
	-- so it feels soft & flighty; keyboard is ADDITIVE (tighten a turn / hard dodge, dominates at full).
	-- Hold SPACE to free-look (suspends aim, mouse drives the camera). Loops need cruise speed (>110,
	-- Flight.AEROBATIC_MIN_SPEED) — a slow bird mushes instead of looping (Energy-Maneuverability).
	-- ALL PLAYTEST-PROVISIONAL.
	mouseAim     = true,
	-- VIRTUAL CURSOR (relative mouse, 2026-06-30). The mouse delta moves the cursor as a DEFLECTION FROM THE NOSE
	-- CROSS (so its reachable zone is centred on the nose — symmetric up/down). The CURSOR stays where you put it
	-- and drives the bird's nose toward it (Chad: "the cursor moves the bird aim, not the bird aim moving the mouse
	-- against me"). aimRecenterRate is a GENTLE relax of the deflection back toward the nose so a held deflection
	-- eventually STRAIGHTENS instead of turning forever — soft enough not to rubber-band/fight you. 0 = fully
	-- persistent cursor (no relax); HIGHER = straightens out sooner. aimMouseSensitivity scales the cursor travel.
	aimMouseSensitivity = 1.0,  -- overall gain on cursor swing per mouse unit (multiplies aimAnglePerPixel)
	-- NOSE-CHASES-CURSOR aim (BirdController.computeMouseAim): the CURSOR is a world direction you swing with the
	-- mouse out to a big cone (the reachable circle); it stays where you put it. The bird's NOSE (amber reticle,
	-- out front) chases it and the beak lands on it when it catches up. aimAnglePerPixel = the cursor swing SPEED;
	-- aimLeadScreenFrac/aimMaxLeadDeg = the reachable-circle SIZE (how far the cursor can lead the nose before it's
	-- clamped — bigger = a bigger circle you can pilot before the nose catches up). Command is body-relative to the
	-- nose (so cursor-on-nose = straight) and zoom-independent; the camera is also fixed in aim mode.
	aimAnglePerPixel    = 0.0024, -- radians the cursor swings per mouse unit (× aimMouseSensitivity). 0.006->0.0035->0.0024
	                              -- (S27b, Chad: "cursor movement is too sensitive" — kept as a standalone sensitivity ask;
	                              -- it's orthogonal to the coordination rework). Too sluggish now? back toward 0.003.
	aimLeadScreenFrac   = 0.95,  -- cursor-circle size: fraction of the way to the top/bottom edge the cursor can lead (0..1).
	                             -- ⚠️⚠️ S28e post-mortem — this cone is a LOAD-BEARING STABILIZER, do not widen it:
	                             -- raising it 0.95→1.8 ("free the cursor, fix the bottom bias") made small deflections
	                             -- RUBBERBAND harder and let the bird roll THROUGH vertical and REST UPSIDE DOWN
	                             -- (Chad, playtest 4). Mechanism: when the nose overshoots the cursor, this NOSE-cone
	                             -- drags the cursor along, capping how far the error can grow behind the swing — a
	                             -- hidden overshoot limiter. At 52° the error kept growing, roll stayed saturated
	                             -- through 90°, and inverted-with-nose-on-cursor is a stable rest state (no auto-level;
	                             -- levelAssist gates off inverted). ALSO: widening it did NOT fix the bottom-biased
	                             -- cursor region — that bias is the CAMERA-ABOVE geometry (the boresight projects
	                             -- above screen centre; same root as the RMB-zoom reticle-above-target parallax), not
	                             -- this cone. ⚠️ S26 history (do NOT shrink either): 0.95→0.625 was tried and REVERTED
	                             -- — faster catch-up = pulsing. 0.95 is the tested value; leave it.
	aimMaxLeadDeg       = 70,    -- hard cap on the lead half-angle (deg); the FOV-derived value governs below it
	-- LEGACY / unused by the nose-chases-cursor model.
	aimCursorRadiusFrac = 0.9,   -- legacy
	aimRecenterRate     = 0.0,   -- legacy
	aimMarkerDistance = 600,  -- LEGACY (unused since the beak reticle was coupled to beakReticleDistance, 2026-06-30)
	-- BEAK + TALON RETICLES (melee-aim UI, reworked 2026-06-30 per Chad). The amber BEAK cross is the boresight
	-- AND the head-on strike marker: it's drawn at beakReticleDistance out front, and the aim ERROR is computed
	-- from the SAME point — that COUPLING is what makes "cursor centred on the cross == straight & level flight"
	-- (the old decoupled draw at 70 vs error at 600 dove at centre). Distance only moves the neutral cursor, not
	-- the per-pixel feel (the error is a screen-angle), so the loved gains carry. The TALON reticle is a
	-- hemispherical ARC of pips UNDER the bird (talonReticleDistance below, spanning ±talonArcSpreadDeg in the
	-- roll plane) that SWINGS as you bank. NO left/right any more — one beak zone, one talon zone; the server
	-- picks which a click throws by enemy proximity (StrikeArmedDir) and the matching reticle lights.
	beakReticleDistance = 90,   -- studs out front the beak cross sits (also the coupled aim-error reference)
	talonReticleDistance = 70,  -- studs below the belly the talon arc is drawn
	talonArcSpreadDeg = 60,     -- half-spread of the talon arc to either side of straight-down (in the roll plane)
	aimResponse  = 13.0,      -- per-sec ease of the APPLIED input toward target. RAISED 9->13: the PD damping
	                          -- below now does the smoothing, so the ease no longer has to add lag (lag = phase =
	                          -- the porpoise). Faster ease = crisper, less floaty; PD keeps it from overshooting.
	-- COORDINATED-FLIGHT INSTRUCTOR (War Thunder mouse-aim model; deep-research RESEARCH §v4). The cursor
	-- direction is resolved into the bird's BODY frame; the controller flies a REAL coordinated turn toward
	-- it — BANK + RUDDER together roll the lift vector onto the target, ELEVATOR pulls the nose through it.
	-- TWO research-grounded upgrades over the old pure-proportional law:
	--   (1) SYMMETRIC deadzone + EXPO on BOTH axes (WT "nonlinearity", documented 1.0–2.5): gentle near
	--       centre for fine aim, ramping to FULL authority at the edge — so a clean vertical LOOP still
	--       gets full pitch, and a slight sideways cursor no longer snaps into a hard bank.
	--   (2) PD (rate/derivative) DAMPING: subtract a term proportional to the bird's current body rate
	--       (FlightPhysics.pitchVel/rollVel). Pure-P over this inertial plant is underdamped -> the
	--       coordinated-turn porpoise; the rate feedback critically-damps it WITHOUT killing loop authority
	--       (at full deflection the P term saturates anyway; the damp only bites near equilibrium).
	-- LOOP COMMIT: the nose-up pull is NOT faded early. It rides to the stall edge and the protection only
	-- bites within aimStallBandDeg of stall — and is SUSPENDED entirely while powered (flap throttle up),
	-- since the wings keep lift past stall then (FLAP_LIFT_FLOOR). This is what lets a started loop finish.
	-- DEADZONE = a hair around dead-centre, just enough to settle straight (player ask 2026-06-30: "way less
	-- deadzone, full linearity"). EXPO = 1.0 → the command is LINEAR in cursor offset (no easing), so the bird
	-- answers the mouse 1:1 right off centre. Equal on both axes (round neutral). PLAYTEST-PROVISIONAL: if the
	-- nose jitters / won't settle to straight at centre, nudge the deadzones up a hair (→ 0.012).
	aimPitchDeadzone = 0.006, -- PITCH deadzone: way smaller — almost no neutral zone
	-- EXPO — response curve of command vs how far the target LEADS the nose. 2.0 (a hard parabola) made the final
	-- approach CRAWL: as the nose neared the cursor the command fell off ∝ offset² and closed too slowly (Chad:
	-- "the reticle is slow to get to the cursor when close"). Dropped to 1.2 (near-linear) so the command stays
	-- strong through the close and the nose shuts the gap at the bird's FLIGHT-MODEL turn/pitch/yaw rate
	-- (FlightPhysics rate-limits it — so closing speed = the bird's agility, as asked). A hair above 1.0 keeps a
	-- little softness at dead-centre so straight flight isn't twitchy. Sweep 1.0 (crisp/linear)..1.6 (softer).
	aimPitchExpo     = 1.0,   -- PITCH ramp: LINEAR (1.2->1.0) so small movements aren't suppressed — snappy short catch-up
	aimBankDeadzone  = 0.006, -- BANK/YAW deadzone: matched to pitch (small round neutral)
	aimBankExpo      = 1.0,   -- BANK/YAW ramp: LINEAR, matched to pitch
	-- OUTER cursor bound: the cursor is CLAMPED so its projection never leaves a circle CENTRED on the
	-- screen covering this fraction of the screen AREA. radius = sqrt(areaFrac · W · H / π); the clamp is
	-- CAMERA-relative in computeMouseAim, so it holds even mid-turn when the lagging aim camera puts the
	-- nose (and its lead cone) off-centre. Past the rim the command just stays saturated (full turn/loop).
	-- ⚠️ S26 (2026-07-02): this knob had been silently UNREAD since the 2026-06-30 nose-chases-cursor
	-- rewrite (the old screen clamp was dropped — why the cursor roamed to the edges/corners again).
	-- RESTORED as the camera-relative clamp. S26c: back to 0.5 ("a little smaller" — 0.625 still hit the
	-- flat top of the screen) AND the code now hard-caps the radius to fit INSIDE the screen (round rim
	-- everywhere — the flat-edge rim made turn direction uncontrollable). On 16:9 the fit-inside cap
	-- binds (≈ half-height − 48px), so to shrink the circle further drop this below ~0.38; raising it
	-- past ~0.44 has no effect. PLAYTEST-PROVISIONAL.
	aimCursorAreaFrac = 0.5,
	-- FREE-THE-CURSOR (S30, Chad: "let the mouse aim operate on the WHOLE screen, let the camera lag").
	-- The circle's true wall on 16:9 is this fit-inside margin (half-height − margin), NOT aimCursorAreaFrac
	-- (whose area term already exceeds the cap). Lowering it from 48 lets the cursor reach nearer the top/
	-- bottom edge = "whole screen" reach; the Stage-1 camera decouple does the re-centring. Identity = 48
	-- (today's behavior). ⚠️ this margin also keeps the ~48px cursor RING on-screen and guarantees the rim
	-- stays round — at 8 the ring may clip the very edge at the extreme rim; if that reads badly or the turn
	-- direction gets ambiguous at the rim (the S26c complaint), raise toward 24–32 (round rim, ring on-screen,
	-- still most of the screen). Does NOT touch the nose-cone aimLeadScreenFrac (the load-bearing upside-down
	-- guard — do NOT widen that). PLAYTEST-PROVISIONAL.
	-- S30: 8 was FLOWN — insufficient alone (the binding wall was the camera lag, not this cap) → REVERTED to
	-- the 48 identity. Becomes relevant again under the screen-referenced-cursor redesign (Fable consult).
	aimCursorEdgeMarginPx = 48,
	-- Gains RAISED (player ask: "reticle catch-up snappy at short movements, rate-limited by the eagle's turn rate
	-- at big deflections"). Higher gain SATURATES the command for anything but a tiny error, so the bird turns at
	-- its MAX rate to close small/medium gaps fast (snappy), while big gaps still take the eagle's turn rate to
	-- sweep (rate-limited). Tiny errors stay sub-saturation for fine aim. PD damps the overshoot the gain adds.
	-- SNAPPIER (Chad 2026-07-02): "at close range it lags; small deflections should SNAP to the cursor in a
	-- fraction of a second — only a LARGE deflection has noticeable reticle catch-up." Raised the P-gains so the
	-- command saturates at a SMALLER error → the nose closes small/medium gaps at the eagle's max turn rate (a
	-- small angle at max rate = a fraction of a second = "snap"), while a big deflection still needs the full
	-- turn-rate sweep to catch up (the intended lag). Damping raised in step so the faster response doesn't ring.
	aimRollGain      = 7.5,   -- bank-to-turn strength (4.5->6.5->7.5: S26c "faster reticle catch-up, snap tighter")
	aimPitchGain     = 8.2,   -- elevator: pulls the nose toward the target (5.0->7.2->8.2: S26c snap; full deflection still saturates -> loop)
	aimYawGain       = 1.55,  -- rudder, coordinated WITH the bank (scaled with the gain bumps)
	aimLevelGain     = 0.9,   -- wings-levelling toward straight flight when there's no bank demand (anti stuck-in-bank)
	aimPitchDamp     = 0.64,  -- PD: nose-pitch-rate feedback (0.58->0.64: raised in ratio with the S26c gain bump)
	                          -- ⚠️ S27b tried 0.80 to fight reticle overshoot — REVERTED: didn't fix the horizontal
	                          -- sail-past (that's a coordination problem, not a damping one) and added arrival lag. The
	                          -- real fix is the coordinated-flight instructor rework — see HANDOFF S28 research directive.
	aimRollDamp      = 0.58,  -- PD: roll-rate feedback (0.52->0.58: raised in ratio with the S26c gain bump)
	                          -- ⚠️ S27b tried 0.72 + S27c added a derivative-on-error "aimHorizLeadDamp" — BOTH REVERTED:
	                          -- the derivative term amplified mouse jitter (small-input reticle "jumping around", Chad) and
	                          -- neither killed the horizontal sail-past. Root cause is that the instructor is 3 independent
	                          -- per-axis PD servos, NOT a coordinated-turn law — see the HANDOFF S28 research directive.
	aimHeadDamp      = 0.45,  -- STAGE A (S30, THE porpoise fix): heading-RATE damp on the roll law. aimRollDamp
	                          -- settles the BANK; this settles the HEADING (the nose), which nothing damped before —
	                          -- so the nose sailed PAST the cursor (bank→turn→heading double integrator). Reuses the
	                          -- headRate math; normalised by pitchRate; added AFTER the saturating P so rim-pinned
	                          -- full-deflection turns are UNCHANGED — it only bites at arrival, like the loved vertical
	                          -- channel. UP (→0.6-0.8)=more damping / risk of lazy arrival; DOWN (→0.2, 0)=porpoise
	                          -- returns; 0 = byte-identical to today. ⚠️ if the overshoot GROWS, the sign is flipped
	                          -- (BirdController roll law) — flip +↔−. First step of the screen-cursor plan (HANDOFF).
	-- ▶ SCREEN-CURSOR STAGE 1 (S30) — FREE THE CURSOR (Chad: "free the cursor right away, then build the cascade
	-- from the right setup"). When true, mouse-aim BYPASSES both cursor clamps (the nose-cone aimLeadScreenFrac +
	-- the screen-circle aimCursorAreaFrac) so the reticle roams the WHOLE screen; the camera (looser aimHeadingLag
	-- below) lag-follows it and is what visually re-centres it. The CURRENT cascade still flies the bird — a big
	-- far-corner throw banking to the ceiling and MUSHING there is the EXPECTED tell that the cascade needs
	-- rebuilding (Stage 2), NOT a failure. false = today byte-for-byte (both clamps on).
	aimFreeCursor    = true,
	-- INVERSION GUARD (REQUIRED with the free cursor — no auto-level, CS-5): fades ONLY bank-DEEPENING roll
	-- authority as |bank| nears this angle (20° soft shoulder), so a far-corner cursor cannot roll the bird past
	-- it and rest inverted (the S28 tombstone); leveling/recovery authority is untouched. 85 keeps the bird
	-- UPRIGHT (UpVector.Y>0 so levelAssist still recovers) while allowing hard ~65-80° turns. Turns feel capped →
	-- raise; leans too far toward inverted → lower toward 75. 180 = no ceiling. Only active in free-cursor mode.
	aimRollCeilingDeg = 85,
	aimStallBandDeg  = 7,     -- degrees below stall where the nose-up PULL eases. 4→7 (S26f, Chad: slow-turn porpoise
	                          -- "still oscillates a little"): the narrow 4° band gated the pull on/off almost bang-bang
	                          -- as AoA crossed it in a slow turn — 7° makes the ease gradual, killing the limit-cycle.
	                          -- Loop commit is protected by the POWERED bypass (flap throttle up ⇒ full pull past stall),
	                          -- so a powered loop still always commits; only unpowered near-stall pulls get gentler.
	-- LOW-SPEED INSTRUCTOR STABILITY (S26e — Chad: up/down oscillation in slow sustained turns). The aim
	-- pitch P-gain is scaled by airspeed/(glideSpeed·aimSpeedGainRef), clamped to [aimLowSpeedGainFloor, 1].
	-- At/above glide speed the feel is EXACTLY the tuned snap (scale = 1); below it the pull softens while
	-- the D-damping stays constant → relatively better damped → the slow turn sustains instead of pumping.
	-- Oscillates still: lower the floor toward 0.35 or widen aimStallBandDeg toward 7 (the stall-guard
	-- gating the pull on/off across its narrow 4° band is the other half of the porpoise mechanism).
	-- Slow turns feel too weak/mushy: raise the floor toward 0.7.
	aimLowSpeedGainFloor = 0.35, -- minimum pitch-gain multiplier at very low airspeed (0.5→0.35 S26f: residual slow-turn
	                             -- oscillation — softer floor + the wider stall band above. Slow turns too mushy: back to 0.5)
	aimSpeedGainRef      = 1.0,  -- speed at which full gain returns, as a multiple of profile glideSpeed
	-- COORDINATED-TURN PITCH FEEDFORWARD (S27, deep-research RESEARCH-instructor-turn.md). Chad: in a HARD
	-- banked mouse turn the nose DIPS/porpoises ("it goes down to keep the speed… throws you off target when
	-- merging with the crows"). Root cause: the instructor pulls the nose up ONLY after the bank's lift-sag
	-- shows up as cursor ERROR (a pure PD error loop), and the S26 stability concessions soften that pull
	-- exactly in the turn. The fix real autopilots use (ArduPilot PTCH2SRV_RLL) + real raptors use (tail-twist
	-- nose-up pitch applied WITH the bank; Science Robotics 2024) is a bank-proportional FEEDFORWARD: add
	-- nose-up pitch as a function of BANK, before the error develops. In a level turn the wing must make
	-- n=1/cos(bank) times the lift to hold altitude, so the extra-lift demand ∝ (sec(bank)-1) — ZERO at
	-- wings-level, growing with bank. computeMouseAim adds bankFF = (sec(bank)-1)·this to the pitch command
	-- BEFORE the AoA/stall gate (so it's stall-limited at low speed, exactly where the merge fight lives) and
	-- the CS-1 aim gate (mouse-aim only; keyboard still zeroes it). WHY IT DOESN'T RE-PORPOISE: feedforward is
	-- open-loop (function of bank, not cursor error) so it adds NO closed-loop phase lag — it CANCELS the known
	-- lift-sag so the PD loop chases LESS, unlike raising P-gain which fights its own damping. Zero at
	-- wings-level ⇒ straight flight + the signed-off snap feel are numerically UNTOUCHED. Kernel untouched.
	-- TUNING (ArduPilot's tuned analogue sits near unity on a comparable term; ours is normalized [-1,1], so
	-- 0.35 is conservative): nose still DIPS in a hard bank → raise toward 0.5-0.6; nose now BALLOONS/climbs
	-- in turns → lower toward 0.2 (ArduPilot's own reduce-if-it-gains-height rule). 0 = feature off. PROVISIONAL.
	aimBankFeedforward = 0.35,
	-- KNIFE-EDGE TAPER (S30) — fade the bank feedforward above ~combat bank. The sec(bank) FF is only valid for
	-- an altitude-holding turn (≤ ~combat bank); past that sec EXPLODES (slope near vertical) and its magnitude
	-- saturates the elevator, so tiny bank wobbles pump the nose UP/DOWN in a steep turn (Chad S30, on the freed
	-- cursor: "the lateral knife edge produces up-and-down oscillation while turning"). Free-cursor made 65-85°
	-- banks reachable (the cone capped ~33° before), which is why this tail only just surfaced. Fades the FF to 0
	-- over a 15° shoulder as |bank| nears this angle: full anti-dip through ≤40° bank, gone above 55°. Nose DIPS
	-- again in hard turns → raise toward 65; still porpoises at knife-edge with it tapered → the driver is the
	-- pitch-P, not the FF (next dial). 0 = no taper = byte-identical to today.
	aimBankFFTaperDeg = 55,
	-- S28e TURN-RATE LEAD — the coordination law (RESEARCH-coordinated-flight.md applied to THIS kernel; see
	-- the BANK+RUDDER + lead blocks in BirdController.computeMouseAim for the full derivation). The S26
	-- servo shape is kept verbatim (it IS the loved snap); the horizontal error is PREDICTED aimLeadTime
	-- ahead using the eagle's ACTUAL heading rate from clean kernel body rates (ψ̇ = pitchVel·sin(bank) −
	-- yawVel·cos(bank), scaled into the error's space by cosB·cosΔψ; nothing differentiates the mouse) and
	-- roll/yaw steer from that prediction. A WASHOUT keeps it from ever capping a commanded turn: the lead
	-- fades to 0 while the cursor is pinned against the screen circle (= the sustained-turn command → full
	-- S26 plant-capped rate, eagle ~1.8 / possessed crow ~3+ rad/s) and while any flight key is held (CS-1
	-- mute — no counter-turn pre-charge at the key-release seam). With the cursor world-fixed INSIDE the
	-- circle, the lead shapes the ARRIVAL: the command reverses a lead-time early, the bank unwinds, the
	-- nose lands ON the cursor; a held inside-circle deflection gives a steady proportional-rate fine turn.
	-- The vertical channel already had this medicine (its damp is plant pitch-rate feedback) — this
	-- balances the horizontal axis the same way.
	-- ⚠️ HISTORY (do not re-try): the S28/S28b sqrt-controller→commanded-bank cascade + error-size mode
	-- blend was REVERTED after Chad's play — the blend seam mushed 10-20° inputs ("yaws but won't bank,
	-- lost its snap") and the slew-limited bank fought the aggressive pitch ("over-delivering, up and
	-- around the cursor, porpoising"). Its knobs (aimDirectConeDeg/aimTurnP/aimTurnAccel/aimMaxBankDeg/
	-- aimBankSlew/aimBankGain) are RETIRED.
	aimLeadTime      = 0,    -- s — turn-rate lead. ⚠️ 0 = the lead is OFF = numerically the PURE S26 servo law
	                         -- (the Chad-loved baseline). 0.28 was PLAYTEST-REJECTED (2026-07-02, playtest 3:
	                         -- "porpoising, rubberbanding and lagging in short displacements" — the lead's
	                         -- feedback through the aimResponse/washout/roll-inertia lags oscillates in ways
	                         -- the static analysis missed). The machinery is kept ONLY for a Chad-in-the-loop
	                         -- live-tune from tiny values (0.08-0.12) IF he wants to try — never re-ship a
	                         -- big value untested. Three structures (cascade v1/v2, lead 0.28) have now failed
	                         -- his feel test while passing build+red-team: knob changes from the loved
	                         -- baseline with him flying each step, or don't touch the aim loop.
	aimLeadWashRate  = 6,    -- per-sec ease of the lead's washout scale (rim-pinned / keys-held → 0; free
	                         -- cursor → 1). Sets how fast the lead returns after a throw ends (~150 ms to
	                         -- mostly-on at 6). Short sharp flicks still overshoot a touch → RAISE toward
	                         -- 9-10 (lead comes back sooner); lead pumping at the circle rim → LOWER.
	invertPitch      = false, -- false = cursor ABOVE the nose pitches UP (the cross rises to meet it)

	-- FLAP THROTTLE (sticky, NOT momentary): hold SHIFT to ramp UP-thrust (climb), CTRL to ramp DOWN-thrust
	-- (powered descent). It HOLDS where you set it so you can set a climb power and then fly a hands-free
	-- defensive climbing spiral on the mouse (War Thunder feel). 0 = glide (stamina regen). The HUD shows
	-- the resulting flap frequency + power-per-stroke. Feeds FlightPhysics via input.flapThrottle, which
	-- scales flap thrust / climb force / dive thrust / stamina cost / wingbeat frequency by the effort.
	flapThrottleRate = 1.1,   -- units/sec the throttle ramps while Shift/Ctrl held (full -1..1 sweep in ~1.8s)
}

--------------------------------------------------------------------------------
-- UI — consumed by GameUI (do not rename keys; client reads these)
--------------------------------------------------------------------------------
GameConfig.UI = {
	hudColor = Color3.fromRGB(18, 20, 28),
	hudTransparency = 0.35,
	font = Enum.Font.GothamBold,
	fontMedium = Enum.Font.GothamMedium,
	eagleAccent = Color3.fromRGB(220, 170, 70),
	crowAccent = Color3.fromRGB(150, 120, 220),
	healthBarColor = Color3.fromRGB(90, 200, 110),
	staminaBarColor = Color3.fromRGB(90, 170, 230),
}

--------------------------------------------------------------------------------
-- PERF — S50 audit FIX 6: the composite particle rail. tests/perf.spec.luau's original §3.4 budget
-- (PARTICLE_BUDGET=1600) only measured the FIRE's own worst-case alive-particle count in isolation.
-- On a live round the fire never plays alone — the waterfall FX, every squirrel's beacon sparkle, and
-- AmbientFX's embers/ash/haze all run concurrently, and nothing pinned their SUM. This is a growth stop,
-- not a knockdown: no emitter rate changes here, the FX are Chad-approved as shipped.
--------------------------------------------------------------------------------
GameConfig.Perf = {
	-- scene-wide alive-particle rail; the 1600 mobile ceiling (PARTICLE_BUDGET in perf.spec.luau) is a
	-- KNOWN gap tracked for the B7 mobile pass — this rail pins today's desktop reality so growth is
	-- caught. Derivation (S50, verifier-corrected): fire 1384 (in-situ measurement, perf.spec test 1)
	-- + waterfall ~412 (RescueServer.server.luau:256-324) + squirrel sparkles ~448 (20 concurrent ×
	-- 22.4, RescueServer.server.luau:547-557) + ambient ~148 (AmbientFX.client.luau:87-164) = 2392
	-- live. The ceiling sits a deliberate 8 particles (0.3%) above: ANY particle growth trips the
	-- rail and forces an explicit decision — that is the rail's job, not an accident of margin.
	compositeParticleCeiling = 2400,
}

return GameConfig
