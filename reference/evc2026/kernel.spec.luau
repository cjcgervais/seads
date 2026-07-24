--!nonstrict
--[[
	tests/kernel.spec.luau — headless invariants for the LOCKED flight kernel.

	The kernel (FlightPhysics + GameConfig) is signed-off and must not regress. These
	are behavior-PRESERVING guard tests: they assert structural invariants (no NaN,
	bounded state, stamina/altitude sanity) that ANY correct edit keeps true — NOT
	specific feel numbers (those are Chad's Play, Tier 5). If a future change trips one
	of these, it changed the kernel's math, which is a LOCKED-spec event.

	This is the pattern the S40 feasibility run demonstrated (200 frames, exit-gated),
	now permanent.
]]

local H = require("./_harness")
local Loader = require("./_loader")

-- Vector3 et al. are NOT globals in a spec's own environment (only inside loaded
-- modules) — pull them off the loader.
local Vector3 = Loader.Vector3

local GameConfig = Loader.load("GameConfig")
local FlightPhysics = Loader.load("FlightPhysics")

local Eagle = GameConfig.Profiles.Eagle
local Crow = GameConfig.Profiles.Crow

-- Deterministic pseudo-random input stream (no Math.random — reproducible + no seed).
local function inputAt(frame)
	-- cheap LCG-ish churn so pitch/roll/yaw/flap/dive all get exercised
	local a = (frame * 1103515245 + 12345) % 2147483648
	local b = (frame * 214013 + 2531011) % 2147483648
	local function unit(x)
		return (x % 1000) / 500 - 1 -- in [-1, 1)
	end
	return {
		pitch = unit(a),
		roll = unit(b),
		yaw = unit(a + b),
		flap = (frame % 7) < 3,
		dive = (frame % 11) == 0,
	}
end

H.test("spawn: eagle initial state is sane", function()
	local eng = FlightPhysics.new(Eagle, Vector3.new(0, 300, 0))
	H.approx(eng.orientation.Position.Y, 300, 1e-3, "spawn altitude")
	H.finiteVec(eng.velocity, "spawn velocity")
	H.approx(eng.speed, Eagle.glideSpeed * 0.9, 1e-2, "spawn speed = 0.9*glideSpeed")
	H.between(eng.stamina, 0, Eagle.maxStamina, "spawn stamina in range")
	H.eq(eng.isStalling, false, "not stalling at spawn")
end)

H.test("glide: 200 neutral frames stay finite and bounded (eagle)", function()
	local eng = FlightPhysics.new(Eagle, Vector3.new(0, 500, 0))
	local neutral = { pitch = 0, roll = 0, yaw = 0, flap = false, dive = false }
	for frame = 1, 200 do
		eng:Update(1 / 60, neutral)
		H.finiteVec(eng.velocity, "glide velocity @" .. frame)
		H.finiteVec(eng.orientation.Position, "glide position @" .. frame)
		H.finite(eng.aoaDeg, "glide aoa @" .. frame)
		-- a gliding eagle never exceeds its dive cap and never reverses into absurd speed
		H.between(eng.speed, 0, Eagle.diveSpeedCap + 5, "glide speed bound @" .. frame)
	end
	-- gliding trims near cruise, not a runaway climb or plummet
	H.between(eng.orientation.Position.Y, 0, 900, "glide altitude did not diverge")
end)

H.test("flap: stamina stays in [0, max] and drives isFlapping (eagle)", function()
	local eng = FlightPhysics.new(Eagle, Vector3.new(0, 500, 0))
	local flapping = { pitch = 0, roll = 0, yaw = 0, flap = true, dive = false }
	local sawFlap = false
	for frame = 1, 200 do
		eng:Update(1 / 60, flapping)
		H.between(eng.stamina, 0, Eagle.maxStamina, "flap stamina in range @" .. frame)
		H.finite(eng.speed, "flap speed finite @" .. frame)
		if eng.isFlapping then
			sawFlap = true
		end
	end
	H.ok(sawFlap, "flapping input produced at least one isFlapping frame")
end)

local function noNaNStorm(profile, label)
	H.test(("noNaN: 200 mixed-input frames stay finite (%s)"):format(label), function()
		local eng = FlightPhysics.new(profile, Vector3.new(0, 600, 0))
		for frame = 1, 200 do
			eng:Update(1 / 60, inputAt(frame))
			H.finiteVec(eng.velocity, "velocity @" .. frame)
			H.finiteVec(eng.orientation.Position, "position @" .. frame)
			H.finiteVec(eng.orientation.LookVector, "look @" .. frame)
			H.finite(eng.speed, "speed @" .. frame)
			H.finite(eng.aoaDeg, "aoa @" .. frame)
			H.finite(eng.stamina, "stamina @" .. frame)
		end
	end)
end

noNaNStorm(Eagle, "eagle")
noNaNStorm(Crow, "crow")
