# Buttery cascade — morning fly cards (2026-07-30)

Rung 1 is LANDED and gated (commit `e22807a04`, gate 388/388, golden deliberately
re-recorded with the gain-6 knob-off arm proven bit-exact first). Fly in this order.
One verdict per card, your words — they go in the flight-log verbatim.

## CARD 0 — BASELINE RECORDINGS FIRST (two minutes, unrecoverable later)

In the **seads-recon play build** (still at the v9 graft — lean_gain 6, the OLD table):
1. Tap **F9** (start), fly ~three FINE turn entries — the exact "turn entry reads a
   bit too flat" feeling you reported — then tap **F9** (stop/save).
2. Optionally one more F9 pair: a couple of straight-down pitch entries with a small
   off-angle (the knife-edge symptom, baseline for rung 2).

Why now: the moment the new table reaches that build, the old feel can never be
recorded again. These recordings are the attribution baseline the vessel ontology
feeds on.

## CARD 1 — RUNG 1: more bank early (`lean_gain` 6 → 8)

Fly the sandboxed build HERE: `D:\flight_sim2\seads-feel\.\build\seads.exe`

**What should be better:** bank arrives sooner and stronger on an honest few-inch
lateral movement. Measured: peak bank +33% on a 2° flick, +27% at 4.5°, +14% at 8°
(V140) — the rise is largest exactly at small early deflections.

**Sentinels (pre-registered — feel for each):**
- **Flat-top region (new):** between ~3.75° and 5° lateral the lean target now
  saturates at the 30° cap — a moderate drag may hit a "ceiling" of shallow bank
  that didn't exist before. Does it read as commitment or as a wall?
- **Held moderate carve (measured trade):** a HELD ~4.5° lateral aim settles ~2°
  LEANER than before (10.2° → 8.2° sustained). Transient up, held equilibrium
  slightly down. If a held carve reads too flat, name it — it's this coupling.
- **Fly-10 verbatim:** "does near-center centering still feel like RUDDER?"
- **Slow-flight wallow** below ~80 m/s (lean loop gain 33 → 44; rejected-at-88).
- **Freelook carve:** a held lateral aim in freelook now carves ~33% more eagerly;
  the v9 release snap itself is untouched — confirm the release still feels sealed.
- **RUDDER early bite:** judge it THIS fly. yaw_scale was already trimmed to 2.0 on
  07-28 ("we have a winner"). If the early rudder still bites too hard, the real
  pre-agreed ladder is (B) `Cy_beta` 2.5→1.5 (speed snap-back) or (C) `center_frac`
  0.0→0.3 (crab-at-rest — ⚠ walks back the Rung-M1 "nose in the MIDDLE" ruling).
  Your words pick the rung; it flies separately.

**Kill-switch / walk-back:** `lean_gain = 6.0` in `config/controller.toml` — one line.

## CARD 2 — QUEUED (lands after your Card-1 verdict): straight-down knife-edge

`[push_gate] side_cone_enter/exit` 27.5/32.5 → 35/40: a slightly off-axis
straight-down aim keeps PUSHING instead of rolling the wings over — the "bigger,
more forgiving vertical pitch-down corridor" you asked for. You predicted more
early bank makes this MORE needed, so it deliberately waits for Card 1's verdict
to attribute cleanly. F9-record a couple of straight-down entries before and after
when it lands.

## While you slept (context)

The vessel-ontology autoresearch ran docs-only (no dials beyond rung 1): the
cascade ontology with the flown REJECTED-row walls, P-47 / A6M2 / bushplane
briefs (each opening with a Feel section for your ruling), the world-scale memo
(plane size vs the 15 km sphere, the 100 m atmosphere-floor idea, collidable
trees), and the knowledge-base index — see `docs/` , everything prefixed
`docs(loop):` in the log. Nothing there changes the plane you're flying.
