--!nonstrict
--[[
	TouchAimAdapter — B7-P1. The touch analog of the mouse, as a PURE DELTA SOURCE.

	THE STRUCTURAL POINT (docs/rescue-consults/B7-touch-design.md §0): the entire signed-off aim law
	consumes exactly ONE control input — the `UserInputService:GetMouseDelta()` read inside
	`computeMouseAim` in BirdController.client.luau. (Cite the SYMBOL, never a line number: the
	consult's ":2495" had already drifted to :2559 by the time P1 was written. The only other
	GetMouseDelta in the file is the EvCDiag debug overlay reading .Magnitude for a label.)
	The world cursor, the curl, the rim-pin, the lead washout, CS-1 and CS-8 all live DOWNSTREAM of
	that read, in locked math that never knows which device produced the delta. So touch is a
	delta-SOURCE ADAPTER above that read, NEVER a new control law: the kernel and the instructor stay
	byte-identical by construction, and this module cannot reach them even in principle — it takes
	numbers and returns numbers, touches no Instance, and holds no global state.

	⚠️ BUILT AHEAD OF THE DECISION. The consult's §7 asks Chad ONE question — should a finger MOVE the
	cursor (DRAG) or PLACE it (TAP)? He has NOT answered it. This module implements Fable's
	recommendation (Mapping A, DRAG). It is pure and inert precisely so that a "TAP" answer discards it
	wholesale at zero cost; do not treat its existence as the decision having been made.

	MAPPING A, "the finger-drag cursor" (Fable's recommendation; B tap-the-sky and C rate-stick were
	ranked below it and the thumbstick/tilt/tap-to-fly answers were killed outright — see the consult):

	  DRAG regime — finger displacement this frame x touchDragGain = the delta a mouse would have
	  produced. One finger-px is worth ~1.3-1.6 mouse units because phone pixels are physically
	  smaller. This is the 1:1 positional character that makes the cursor feel like YOURS.

	  HOLD regime (the rim-pin analog) — a locked mouse has INFINITE travel; a finger runs out of
	  glass. So a finger held further than touchBandPx from where it landed emits a synthetic delta
	  STREAM whose rate is proportional to the overshoot. The cursor pins against the S26 circle,
	  rimBinding stays true, the lead washes out, and you get the full-rate sustained turn — exactly
	  the mouse's held-throw, reconstructed from a finger that physically cannot keep moving.

	  LIFT — the stream stops dead. The cursor stays world-anchored and the nose finishes the chase,
	  which is the mouse's throw-end behaviour verbatim.

	The whole module is a pure function of (state, touch, dt, cfg). `state` is caller-owned and
	mutated in place, so the same call is deterministic and testable headlessly — which is the point:
	tests/touchaim.spec.luau runs a scripted drag and asserts PARITY with what the mouse path would
	have produced, before any of this is wired to a seam.

	INERT until GameConfig.Controls.touchAim = true. Nothing requires this module yet (B7-P2 wires
	the one-line seam, and that is the CHAD GATE — the aim decision is his).
]]

local TouchAimAdapter = {}

-- A fresh adapter state. Caller owns it (one per player/session); the module keeps no globals.
function TouchAimAdapter.newState()
	return {
		active = false,   -- is a finger currently down?
		id = nil,         -- identity of the finger we are tracking (see the CALLER OBLIGATIONS below)
		anchorX = 0, anchorY = 0,   -- where this touch LANDED (the hold regime measures from here)
		lastX = 0, lastY = 0,       -- where it was last frame (the drag regime measures from here)
	}
end

--[[
	CALLER OBLIGATIONS (B7-P2's seam must honour these — they are contract, not suggestions):

	  • PASS `touch.id` (the InputObject, or any stable per-finger key). Without it the adapter cannot
	    tell "finger A lifted and finger B became primary" from one enormous drag, and a finger swap
	    across the screen would emit a single frame of thousands of px — a violent cursor yank. With an
	    id, a change re-anchors and emits nothing that frame.
	  • ON FOCUS LOSS feed one `active = false` frame. Roblox can drop InputEnded when the window loses
	    focus (the same hazard the CS-4 free-look focus-loss failsafe exists for); without the release
	    the adapter would hold a stale anchor and could resume mid-stream on the next touch.
	  • CALL newState() on possession change / respawn. A nil state soft-returns 0,0 rather than
	    erroring, so a wiring bug would present as a silently dead aim — check it at the seam.
]]

local function finite(n)
	return type(n) == "number" and n == n and n ~= math.huge and n ~= -math.huge
end

--[[
	step(state, touch, dt, cfg) -> dx, dy

	touch = { active: boolean, x: number, y: number, id: any? } in SCREEN PIXELS.
	cfg   = GameConfig.Controls (touchDragGain, touchBandPx, touchHoldRate, touchHoldMaxPxPerSec).
	Returns the synthetic mouse-delta for this frame — the exact shape BirdController:2495 consumes.

	Contract (each clause is a named assertion in the spec):
	  • finger up                      -> 0, 0 (and it can never "keep turning" after a lift)
	  • touch-down frame               -> 0, 0 (landing is not a swipe)
	  • touch.id CHANGES mid-gesture   -> 0, 0 and re-anchor (a finger swap is not a 2000px drag)
	  • moving inside the band         -> pure drag: displacement x gain, mouse-identical
	  • stationary inside the band     -> 0, 0 (no drift, no creep)
	  • held past the band             -> drag + a rate stream along the overshoot direction,
	                                      monotone in overshoot, clamped at touchHoldMaxPxPerSec
	  • any non-finite input           -> 0, 0 (a NaN must never reach the aim law)
]]
function TouchAimAdapter.step(state, touch, dt, cfg)
	cfg = cfg or {}
	if not state then
		return 0, 0
	end

	-- LIFT (or no touch at all): reset and emit nothing. The cursor stays where the world put it.
	if not (touch and touch.active) then
		state.active = false
		return 0, 0
	end

	local x, y = touch.x, touch.y
	if not (finite(x) and finite(y)) then
		-- A malformed sample must never inject NaN into the locked aim math. Note the deliberate
		-- consequence: this also drops `active`, so one bad sample mid-held-turn re-anchors and ends
		-- the hold stream (the turn stops until you drag past the band again). Chosen over carrying a
		-- possibly-corrupt anchor forward — a dropped turn is recoverable, a poisoned aim is not.
		state.active = false
		return 0, 0
	end

	-- TOUCH DOWN, or a DIFFERENT finger became primary: anchor here and emit nothing this frame
	-- (landing a finger is not a swipe, and a finger swap is not a screen-wide drag).
	if not state.active or state.id ~= touch.id then
		state.active = true
		state.id = touch.id
		state.anchorX, state.anchorY = x, y
		state.lastX, state.lastY = x, y
		return 0, 0
	end

	local gain = cfg.touchDragGain or 1.4

	-- DRAG regime: this frame's displacement, in mouse units.
	local dx = (x - state.lastX) * gain
	local dy = (y - state.lastY) * gain
	state.lastX, state.lastY = x, y

	-- HOLD regime: how far past the dead-band is the finger from where it landed?
	local ox, oy = x - state.anchorX, y - state.anchorY
	local mag = math.sqrt(ox * ox + oy * oy)
	local band = cfg.touchBandPx or 60
	if mag > band then
		local overshoot = mag - band
		-- rate is proportional to overshoot (px/sec of synthetic delta), clamped so a finger jammed
		-- at the screen edge cannot spin the cursor arbitrarily fast.
		local rate = overshoot * (cfg.touchHoldRate or 6.0)
		local maxRate = cfg.touchHoldMaxPxPerSec or 1400
		if rate > maxRate then
			rate = maxRate
		end
		local step = rate * (finite(dt) and math.max(0, dt) or 0)
		dx = dx + (ox / mag) * step
		dy = dy + (oy / mag) * step
	end

	if not (finite(dx) and finite(dy)) then
		return 0, 0
	end
	return dx, dy
end

return TouchAimAdapter
