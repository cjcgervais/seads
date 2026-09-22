# D-D — FEEDBACK LOOP: what the machine tells the hand, the eye and the ear

Strand **D-D** of the sled ride audit. Worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. **READ-ONLY against main**: nothing in `sim/`, `control/`, `config/`,
`test/golden/` was opened for write; no dial moved, no golden touched, no `ctest` run, no
`seads.exe` launched, nothing pushed. `seads_sled_probe` was **not** built — every number
below comes from reading shipped source and from read-only passes over Chad's own
`.sledtape` files in `D:/flight_sim2/seads-recon/build-play`.

**Confidence vocabulary.** **MEASURED** = a number this strand computed from a tape or read
verbatim out of a shipped constant. **DERIVED** = arithmetic in this document on stated
inputs. **LITERATURE** = a cited published claim (none load-bearing here). **GUESS** =
labelled, every time.

**Every numeric claim carries its killing mutation** — the change to an input, a constant or
an assumption that would make the claim false. A claim without one is decoration.

**No feel recommendation rests on a harness number alone.** Every rung in §7 cites (a) a
tape event from Chad's own drives and (b) one of Chad's words, quoted verbatim with document
and section.

---

## 1. Chad's words that bind this strand — verbatim, with provenance

`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0** ("Chad's ruling (drive 4,
2026-08-12 — verbatim, this is the acceptance bar)"), read from the main tree at
`D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md`:

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more often
> after a roll** (even though R works). But not every time — allow it to happen. Also I've
> seen lots of snowmobiles **drift around corners and then with throttle, straighten out.
> The feel is really good. Don't lose the feel**, except the constant rolling. **Make it
> possible to roll but not the rule.** However the game feels like a nicely made sim as
> there is dynamic room and **the balance mechanism works really good. Just allow the
> balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be
> the necessary condition of not rolling over.** Just allow leaning a certain way in a
> particular condition to be the OPTIMAL weight distro for better traversing, say up a hill
> or around a corner. **Work on the math to achieve a balance of fun and accuracy to real
> physics, just as our airplane ontological counterpart does.**"

Same document **§0b** ("★★ SUPERSEDING RULING (2026-08-12 night — verbatim, OVERRIDES §0's
decode and §1's design)"):

> "no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor.
> Make it less honest but dont ruin it. **Get a fable consult and the measure of it shall be
> if my intent is heard.** Leaning shall enhance the ride an just make it more stable and
> slef righting by chance more. It should be arcadey to a degree so that it is fun.,
> **SLiding banging, punchy, jumps.** Just make it more stable."

`docs/roost_consult_packet.md` **§1** ("Chad (the owner; the only authority on feel),
2026-08-27, verbatim"):

> "yes when you say float do you mean the felt displacement at any given height within the
> snowcolumn that I am riding over. **The conditions should be palpable and observable in
> the sled performance**, also I think this is the time to make sure we make a note for the
> amount of roost (rearward thrown snow about 30 to 50 ft), volumetrically variable per snow
> depth and throttle application and duration. So I want to make sure our snow is meeting
> the **palpable precision standard**."

Same section, earlier in that thread, on why a purely visual fix was rejected:

> "thats not the only point I should see skis going into snow, deeper snow should bury me a
> bit, when I give throttle and plane out I should be planing out over this snow and not
> just making tracks but depressing snow... **Snow that is properly shaded and felt, not
> some cheap drawn in illusion**"

`docs/SESSION_HANDOFF_20260908_cam_smooth.md` **§1 "Chad's ask"**:

> "Find out why the camera motion feels jittery, what causes it, and how we fix it so
> flying, driving and viewing all around is **smooth as butter**." Then: "lets sandbox and
> fix this."

`render/sled_audio.h` (header note, carrying Chad 2026-08-17 — this is the quote's carrier
in the tree, not a primary doc; see §8 UNVERIFIED-1):

> "Idle sngine for snowmachine is [the idle] sample loop and the riding is the other engine
> snowmachine brap brap sounds that can ramp up en key pressed. **Tapping keys gives the
> brap brap ramp up**... then chose a tone for the sustained machine sound... Make sure to
> balance the sounds."

`docs/steer_lean_hud_handoff.md` §(2026-08-25), on the weight instrument:

> "Grid on sudburians back moving with him, distinct and easy to see and so are the
> handlebars, **either across the bottom of the screen awesomeness or through thte sudburian
> xray style visibility**."

`docs/README_SOUNDSCAPE.md` **§2** states the standing doctrine this strand is measured
against: "Anything whose job is to convey **speed** is synthesized in real time and driven
by a live parameter. A static loop there *'kills the sense of speed instantly'*
(`audio_ideas/wind_audio.md`)."

---

## 2. Corpus and method

**Tapes.** The six drives on the CURRENT kernel bucket, `kernel-v17-tremor-signed`
(`sled_tape_86..91.sledtape`, tags `sandbox/r4a-phase0@kernel-v17-tremor-signed-33-g4b8c68698`
and `-53-g7b377c6f0`). Pooled: **86 578 T records, dt = 0.008333 s exactly, 721.5 s of
Chad's own driving, 0 bad lines.** Older buckets (`kernel-v13g`, `pre-reconcile-20260821`,
`v14`, `v15`) were deliberately NOT pooled in: a channel audit must be measured against the
kernel a rung would ship on.

**Column order** taken from `test/harness/sled_tape.h` via the roster documented in
`tools/sled_tape_audit.py` (strand D-B's tool — read, never edited). Pin offsets used here:
`belt_speed_ms` 29, `engine_rpm` 30, `plane_frac` 31, `track_slip` 32, `roost_flux` 33,
`ground_speed_ms` 35, `depth_under_m` 36, `air_s` 38, `assist_nm` 39. Body axes:
`body_up = R*(0,1,0)`, `body_fwd = R*(0,0,-1)`, `up_cg = normalize(position)` (globe world,
|position| ≈ 15 035 m). `g_eff = |(-9.80665·up_cg) - a_world|`.

**Scripts** live in this session's scratchpad, not in the worktree (another strand owns
`tools/sled_tape_audit.py`; this strand adds no tool file):
`…/scratchpad/dd_channel_probe.py`, `dd_pass2.py`, `dd_roost.py`.

**Cross-check.** `assist_nm_abs` percentiles and `assist_active_frac` were re-pooled from
strand D-B's `docs/sled_audit/tape_summary.json` by summing the raw histogram bins (not by
averaging percentiles), so those two numbers are verified, not inherited.

---

## 3. THE CHANNEL TABLE

Latency column: tick → eye/ear, at the measured 60 Hz vsync / 120 Hz tick pairing recorded
in `docs/SESSION_HANDOFF_20260908_cam_smooth.md` §2 (frame time mean 16.667 ms, σ 0.083 ms,
2 ticks on 584 of 600 frames). Derivations in §4.

| # | Channel | State it reads | Latency tick→eye/ear | Can it LIE? | What a real rider uses that is missing |
|---|---|---|---|---|---|
| C1 | **Chase camera position** (`app/main.cpp:9560-9640`, `render/interp.h cam_lag_step`, `SEADS_SLED_CAM_TAU` default `0.06,0.12`) | `sled_draw.position/.velocity` (interpolated), or `walker_draw` once off the machine | interp ≤ 8.33 ms (mean 4.17) + anchor **0 ms steady-state** + 1 frame present ≈ **21–25 ms** | **No, and this is the one channel that is clean.** The feed-forward `ff = dt(1/kp−1)` makes the continuous-limit transfer `(1+τs)/(1+τs) = 1` — unity gain, zero phase. It does not soften a landing and does not lead the body. Pinned by `test_interp.cpp`. | — |
| C2 | **Chase camera heading** (same block, `tau_fwd` 0.12) | `sled_draw.orientation` → body −Z, tangent-projected. **The NOSE, never the velocity.** | **angular** lag = ω·(τ−dt/2) = ω·0.1117 s. Measured ω: p50 0.085 → **0.5°**, p90 0.815 → **5.2°**, p95 1.725 → **11.0°**, p99 5.545 rad/s → **35.5°** | Mildly, and in a useful direction: in a slide the frame holds the old heading, so the machine rotates *into* frame. But it has **no feed-forward** (unlike C1), so the lag is a pure first-order lag and compounds with sideslip (p90 **25.3°**, p95 **77.8°**) — at p99 the eye can be pointing >60° off the direction of travel. | a velocity-referenced option for the drift; C2 can only ever frame the nose |
| C3 | **Camera UP / horizon** (`app/main.cpp:9827`, `pose.up = sup`) | Nothing. `sup = normalize(anchor_pos)` — the planet radial, unconditionally. | n/a | **YES — it HIDES the thing Chad is judging.** Roll never reaches the eye except as the model rotating inside a level frame. Measured tilt: **23.42 % of ticks past 20°, 8.92 % past 45°**, pooled p90 37.25°, p95 100.75°. `app/rest_horizon.h` — the v13 rest-edge horizon-roll law, shared by the aeroplane AND the Sting — is **not included by the sled path** (`grep`: `app/instructor_tick.h`, `app/sting.h`, `app/feel_tape_fields.h` only). | horizon roll; the single strongest lean/roll cue on a real machine |
| C4 | **Camera FOV / speed cue** (`app/main.cpp:9376`) | Nothing sled-side. `fovy = kChaseFovyDeg + (kZoomFovyDeg − kChaseFovyDeg)·zoom_t`, and `zoom_t` is the **aeroplane's** RMB zoom (`raw_mode`/`live.zoom_held`). | n/a | It is a constant: at 8 m/s and at 30 m/s the frustum is identical. **44.48 % of ticks are above 8 m/s**, speed_all max well past that. | FOV widening, screen-edge streaming, any speed cue at all. ⚠ the R1 motion-blur/tunnel-vision attempt was REVERTED at Chad's word — see §8 UNVERIFIED-2 |
| C5 | **Camera shake / g-cue** | Nothing. There is no shake in the sled path. | n/a | It **hides**: `g_eff` pooled p95 **14.95 m/s² (1.52 g)**, p99 **32.85 (3.35 g)**; **2.65 % of ticks above 2 g, 1.26 % above 3 g, 5.99 % airborne**. The only landing cue is the model dropping in frame. | impact/landing g. **The precedent exists in-tree**: `render::flak::shake_step` (`app/main.cpp:7023`, envelope + `lookz` guard) |
| C6 | **HUD weight instrument — x-ray grid** (`render/draw.cpp:5250-5330`, default; `sled_hud_xray` default true, `H` toggles) | `info.sled_weight_x/y` = `sled.rider_lat_m / lat_max`, `sled.rider_fwd_m / fwd_max`, normalized to the LIVE reach box (`app/main.cpp:10783-10798`). **RAW kernel state, not the interpolated draw copy, not the input.** | tick-rate value drawn per frame; ≤ 8.33 ms | **No** — it is honest about what it claims. But it claims only the rider's own displacement. It says nothing about the *result*: no load transfer, no roll angle, no lateral g, no slip. It is drawn on his BACK, so a side-on freelook loses it. | a load/roll read; "is my lean working" |
| C7 | **HUD dash plate** (`render/draw.cpp:2622-2690`) | `sled_speed_ms` (raw), `sled_surface`, `sled_depth_m`, `sled_air_temp_c`, `sled_plane_frac` (bar), `sled_roost_flux` (bar), `sled_susp_x[3]` (text), `sled_rolled` (lamp), frost | ≤ 8.33 ms | **Partly.** The ROOST bar reads raw `roost_flux`; the drawn spray multiplies it by `sled_roost_intensity`'s speed key (0 below 1.5 m/s, full at 9 m/s). The source comment ("At S5 the bar and the spray are the same number") is **false**. Measured: bar lit (flux>0.10) on **22.75 %** of ticks; of those the drawn spray is **< half the bar on 15.96 %** and **effectively absent (<0.02) on 6.84 %**; mean drawn/mean bar = **0.838**. Bounded, but not "the same number". | a roll-angle readout; a slip readout; a g readout |
| C8 | **ROLL-ASSIST readout** (`sim::SledState::assist_nm`, `right_assist_nm_now`) | — | — | **The biggest hole in the loop. There is NO channel.** `grep -rn "assist_nm\|right_assist" render/ app/` returns **only `app/spawn_policy.h:276-277` (two resets)**. `render/draw.h` carries `sled_air_s`, `sled_air_grace_s`, `sled_hull_engage`, `sled_susp_sum_m`, `sled_grip_attached` — and not this. Meanwhile the assist is **non-zero on 86.6 / 86.0 / 87.6 / 88.4 / 93.7 / 79.8 % of ticks** across the six v17 tapes, pooled p90 **435 N·m**, p95 **575**, p99 **1115 N·m**. | the machine is being held up by an invisible hand 87 % of the time |
| C9 | **Engine audio** (`render/sled_audio.h` + `sled_drive.h` + `sled_synth.h`; driven at `app/main.cpp:11820`) | **`sled_thumb` — the raw input scalar** (spring-return, +2.5/s, −6.0/s, `app/main.cpp:8212-8214`), forced to 0 off the machine. **NOT** `engine_rpm`, **NOT** `belt_speed_ms`, **NOT** `track_slip`, **NOT** speed. | frame quantization 0–16.7 ms + gain slew 46.4 ms + stream queue 46.4–92.9 ms ≈ **63–130 ms transport**, on top of a deliberate rev-follower τ of **0.22 s up / 0.55 s down**, on top of the thumb's own 400 ms/167 ms ramp | **YES, measured.** See §5. It is a throttle follower wearing an engine's clothes. | real rpm; a clutch-engagement cue; a bog; engine braking |
| C10 | **Track / belt sound** | Nothing exists. | — | **YES by omission.** Kernel `belt_speed_ms` runs a mean **20.50 m/s faster than the ground** while in contact and moving; mean \|track_slip\| **0.522**; \|slip\| > 0.3 on **53.03 %** of all ticks. `grep`: `belt_speed_ms` has **zero** consumers in `render/` or `app/`. | track spin / chain whine — the defining sound of a stuck or spinning sled |
| C11 | **Wind / speed audio** (`render/wind_synth.h`, driven `app/main.cpp:11745`) | `rd.speed` where `rd = render::flight_readout(draw_state, …)` (`app/main.cpp:11298`) — **the AEROPLANE's airspeed**, whichever body the player is in. No mount gate. | same stream budget as C9 | **YES.** At 70 km/h on the sled the wind bed reports the parked (or abandoned) aeroplane. There is **no sound in the game keyed to sled speed** — `grep`: `ground_speed_ms` reaches only `render/sled_plumes.h`. | wind rush; the doctrine in `README_SOUNDSCAPE.md` §2 is being violated on this vehicle |
| C12 | **Roost plume** (`render/sled_plumes.h`, updated `app/main.cpp:10882-10944`) | `sled.roost_flux` (raw) + `sled.ground_speed_ms` (raw) + `sled_thumb`; `pos/basis/vel` from `sled_draw` | per render frame, ≤ 1 frame | **Mostly honest — the anti-fork rule holds** (it reads the kernel's one product, not a second depth source). Its two known distortions: the speed key (C7) and the flux's own death on thin snow. **33.77 % of ground-moving ticks have \|slip\| > 0.3 AND flux < 0.02** — on Road/LakeIce/RockOutcrop the track spins hard and throws nothing, correctly, so nothing tells the rider. Slip-high fraction by surface: **RockOutcrop 70.2 %, Bush 65.4 %, TrailMain 53.1 %, Road 41.3 %, LakeIce 24.7 %.** | roost density variation; ice spray; a *hard-surface* slip cue of any kind |
| C13 | **Track visual** (`render/sled_model.cpp`, 7 687 lines) | **Nothing.** `grep -n "track\|belt\|cleat\|scroll\|tread"` returns **one** incidental hit ("belt-and-braces", line 3202). `render::SledRig` (`render/sled_model.h:38-60`) has **no** belt channel. | n/a | **YES by omission.** The comment at `sim/sled.h:1307-1310` — *"Track SURFACE speed (the slip cue) — the rig reads THIS, never ground speed, because a decoupled coast at 0 belt speed while still moving IS the 100 % slip an observer should see"* — **is a lying instrument. Nothing reads it.** | the track turning; the single most legible visual on a snowmobile |
| C14 | **Rider pose as lean indicator** (`render/rider_pose.*`, `SledRig.lean_lat/up/fwd_m`, `.absorb`, `.right_push`) | `sled_draw.rider_lat/fwd/up_m` (real kernel displacement, §9d.7 — "these ARE the physics numbers"), `absorb` composed by `render::rider_absorb()` from `susp_x` + `susp_v`, `right_push` from `sled.right_shift_cmd` | interp ≤ 8.33 ms | **No** — it draws exactly what the kernel moved. But `absorb` is a **vertical** bump-soak only. The rider does not brace against lateral g, and the **self-right's leg push is drawn (`right_push`) while the assist torque that actually rights the machine (C8) is not** — so the one visible half of the righting is the small half. | a lateral brace; a posture that reads "the machine is loading up" |
| C15 | **Suspension visual travel** (`render/sled_model.cpp:5466-5478`, `CH_susp_L/R/T`) | `sled_draw.susp_x[3]` against a **render-owned** static sag `kSagDefaultM = 0.10` (`SEADS_SLED_SAG0`), which is deliberately neither the kernel's `susp_rest_m` 0.21 nor the measured drive-tape static `susp_x` ≈ 0.01 | interp ≤ 8.33 ms | **No, and the header says why in the open.** The 0.10 is a visual stance dial with no kernel owner, explicitly named so it is not "quietly single-sourced". The per-ski disagreement Chad's §2.4c.1 ruling asks for is real — same numbers as the HUD's `SUSP L/R/T` text. | ski chatter (there is no high-frequency channel; `susp_x` is drawn, not vibrated) |

---

## 4. THE LATENCY BUDGET — derivations

### 4.1 Visual: tick → eye ≈ **21–25 ms**, and the camera adds ~nothing

| Stage | Value | How |
|---|---|---|
| Fixed tick | 8.333 ms | `dt = 0.008333333333333333` read verbatim from all six tape headers. MEASURED |
| `interpolate_sled(prev, curr, alpha)` | lag `(1−α)·8.333` ms ∈ [0, 8.333], mean **4.17 ms** | `render/interp.h`. The drawn state sits between the last two ticks. DERIVED |
| `cam_lag_step` anchor, τ_pos = 0.06 | **0 ms steady-state, unity gain** | `ff = dt(1/kp − 1)`, `kp = 1 − e^{−dt/τ}`. Continuous limit `ff → τ`, so `ȧ = (1/τ)(p + τṗ − a)` ⇒ `a/p = (1+τs)/(1+τs) = 1`. DERIVED, and the fixed point is pinned by `test_interp.cpp` |
| Present | +16.67 ms at 60 Hz vsync | measured frame time in `SESSION_HANDOFF_20260908_cam_smooth.md` §2 |

**Killing mutation (4.1):** if `SEADS_SLED_CAM_TAU` is set with a non-zero `pos` **and** the
feed-forward were removed, the anchor would lag by `v·τ` — 1.8 m at 30 m/s. It is not
removed; the red-team of 2026-09-09 already caught the wrong-recurrence version. Also: if
Chad's box ever ran at 120 Hz+ display, `(1−α)` collapses and the interp term vanishes.

### 4.2 Visual heading: an ANGLE, not a delay

`kf = 1 − exp(−dt/τ_fwd)`, then `sled_cam_fwd` is rotated by `gap·kf` — **no feed-forward**.
Steady state under constant yaw rate ω: lag `= ω·dt·(1/kf − 1) → ω·(τ − dt/2)`. At 60 Hz and
τ_fwd = 0.12 that is **ω × 0.1117 s**.

| yaw rate \|ω_body.y\| (pooled, 86 578 ticks) | camera heading lag |
|---|---|
| p50 0.085 rad/s | 0.54° |
| p90 0.815 | 5.2° |
| p95 1.725 | **11.0°** |
| p99 5.545 | **35.5°** |
| max 10.10 | 64.6° |

**Killing mutation (4.2):** the claim dies if `SEADS_SLED_CAM_TAU`'s second field is 0 (welded
forward, lag 0 exactly) — the shipped default is 0.12. It also dies if the yaw-rate tail is
dominated by rollover tumble rather than driving: **8.92 % of ticks are past 45° tilt**, so
the p99 figure is partly tumble, and the honest driving number is the **p90 5.2°**.

### 4.3 Audio: thumb → ear

| Stage | Value | Source |
|---|---|---|
| `sled_thumb` ramp | +2.5/s up, −6.0/s down ⇒ **400 ms** 0→1, **167 ms** 1→0 | `app/main.cpp:8212-8214`. MEASURED |
| `set_drivers` cadence | once per FRAME (correct; a per-buffer version dropped real taps) ⇒ 0–16.7 ms | `app/main.cpp:11820`, and the red-team note in `render/sled_drive.h` |
| Rev follower | τ 0.22 s up / **0.55 s down**; 63 % of a step in τ, 95 % in 3τ ⇒ **660 ms / 1 650 ms** | `kSledRevUpTau`, `kSledRevDownTau`, `render/sled_audio.h`. MEASURED |
| Gain slew | one buffer = 1024/22050 = **46.4 ms** | `SledSynth::render` |
| Stream queue | sub-buffer pinned to 1024 frames (`SetAudioStreamBufferSizeDefault(1024)`, `app/main.cpp:1263`), double-buffered ⇒ 2048 frames = 92.9 ms total, **46.4–92.9 ms** ahead of the DAC | `raudio.c:2105-2110`, `2675-2696` |
| Device period | not measured | **GUESS**: 10–20 ms typical WASAPI shared |

**Transport ≈ 63–130 ms. Plus a designed 220 ms / 550 ms follower. Plus the 400/167 ms thumb.**

**The asymmetry is the finding:** the eye is current to ~25 ms; the ear is a quarter of a
second behind the thumb on the way up and can be **over a second** behind on a throttle chop
(167 ms thumb + 1 650 ms to 95 % of the follower's decay).

**Killing mutation (4.3):** the whole budget collapses if any 22 050 Hz hand-fed stream were
created while the default sub-buffer was 4096 — raylib **zero-fills** the unwritten remainder
(`raudio.c:2691-2693`), which is 75 % silence and a 5.4 Hz chop. **Checked: it is not.** All
seven `LoadAudioStream(22050,16,1)` calls (`app/main.cpp:1265, 1328, 1345, 1354, 1424, 1443,
1618`) sit under a 1024 default; the `boom_send` case at 1618 restores 1024 explicitly and the
comment there names this exact defect. **The sled engine stream is clean.** If a future stream
is added after `app/main.cpp:1564` (`SetAudioStreamBufferSizeDefault(4096)`) it will not be.

---

## 5. THE ENGINE AUDIO LIE, measured

`render/sled_synth.h::set_drivers(throttle, dt)` is handed `sled_thumb`. The sounded voice is
`sled_voice_hz(revs) = (1200 + 5800·revs)/60·3·0.40`. The kernel meanwhile publishes
`engine_rpm` ∈ [1700, 8000] — labelled in `sim/sled.h:1311-1313` as a *"Readout for
dash/audio"*, and `tools/sled_probe.cpp:4308-4310` already records that **"belt_speed_ms,
track_slip and engine_rpm are all exported 'for dash/audio' … and nothing currently consumes
them."** This strand measures how much that costs.

Method: replay `render::SledDrive::update()` **exactly** (same τ, same hysteretic gate, same
clamps) on each tape's `throttle` column at the tape dt, and compare `revs` against the
kernel's own rev fraction `k = (engine_rpm − 1700)/6300`, both in [0, 1].

| Measurement (pooled, 86 578 ticks / 721.5 s) | Value |
|---|---|
| mean \|audio_revs − kernel_revs\| | **0.095** |
| p90 / p95 / p99 of that gap | **0.355 / 0.485 / 0.865** |
| ticks with gap > 0.25 (a quarter of the whole rev sweep) | **13 336 = 15.40 %** |
| restricted to ticks where **both** channels are off the stop (53.9 % of the drive) | mean gap **0.175**, gap > 0.25 on **28.6 %** |
| **derivative sign disagreement** over a 0.1 s window, both channels moving > 0.03 | **672 / 6 560 = 10.24 %** — the sound goes UP while the engine goes DOWN, or the reverse |
| COAST: ground speed > 8 m/s, throttle < 0.10 (audio gate shut), in contact | **7 450 ticks = 8.60 % of the drive** |
| …during those: mean ground speed | **19.34 m/s (69.6 km/h)** |
| …mean **audio** revs / mean **kernel** revs | **0.218 / 0.388** |
| …mean kernel `engine_rpm` | **4 147 rpm** |
| …sounded-fundamental error | `voice_hz(0.388)/voice_hz(0.218) = 69.0/49.3 = ` **1.40× = 5.8 semitones flat** |
| throttle onsets ("brap") fired in 721.5 s | **113 events = 9.40/min**; inter-onset gap p50 **2.38 s**, p90 **13.78 s**, max **36.52 s** |

**Reading it.** Half the time the two agree because both are pinned (kernel_revs p50 =
0.995). The lie lives in the other half: a quarter-of-a-sweep error on 28.6 % of the
off-the-stop ticks, an outright **inverted direction on one tick in ten**, and a standing
5.8-semitone flat error through **8.6 % of every drive** — the coast, which is exactly the
moment a rider listens to the engine to judge how fast he is still going. And the "brap" that
`sled_audio.h` names as the character fires **under ten times a minute**, because Chad drives
with the thumb pinned and the hysteretic gate needs a drop below 0.10 to re-arm.

**Killing mutations (§5).**
1. If `engine_rpm` were itself a throttle follower inside the kernel, feeding it to the synth
   would change nothing. It is not: `sim/sled.cpp:1337` computes `belt_speed_ms`/`engine_rpm`
   off the clutch/belt blend (§8 P1-10 + Phase V P2-A), and `sim/sled.cpp:1118` states
   `engine_rpm` is "mapped off the SAME blend". The 0.388-vs-0.218 coast split is the proof:
   a follower cannot sit at 0.388 with the thumb shut.
2. If the two rev *ranges* are the thing being compared rather than the two *signals*, the
   gap is an artefact: the audio spans 1200–7000 rpm and the kernel 1700–8000. That is why
   every number above is in **normalized [0,1] rev fraction**, not rpm. Re-running in raw rpm
   would inflate every figure and would be wrong.
3. If `kSledVoiceTranspose` moves again (it has moved twice on Chad's ear: 0.50 → 0.40), the
   **5.8-semitone** figure survives — it is a *ratio* of two voices at the same transpose.
   What would kill it is a change to `kSledIdleRpm`/`kSledMaxRpm`.
4. The 9.40 brap/min figure dies if `kSledOnsetOn`/`Off` (0.18/0.10) move: a narrower band
   raises the count and a wider one lowers it. It also dies if a future input maps the thumb
   to an analog axis rather than the 2.5/s key ramp.

---

## 6. DEFECTS FOUND (not rungs — these are statements of fact a rung would fix)

- **D-D-F1.** `sim/sled.h:1307-1310` claims the rig reads `belt_speed_ms`. **Nothing does.**
  `grep -rn "belt_speed_ms"` over the tree returns `sim/sled.cpp` (2 writes),
  `sim/sled.h` (the field + the lying comment), `test/harness/sled_tape.h`, and
  `tools/sled_probe.cpp`. Zero in `render/` or `app/`. A comment that names a consumer that
  does not exist is the class of thing this house calls a lying instrument.
- **D-D-F2.** `render/draw.cpp:2674-2676`: *"At S5 the bar and the spray are the same
  number."* They are not — the spray carries `sled_roost_intensity`'s speed key and the bar
  does not. Bounded (mean ratio 0.838) but false as written.
- **D-D-F3.** `track_slip` has **zero** consumers outside the tape and the probe, while
  \|slip\| > 0.3 on 53.03 % of ticks.
- **D-D-F4.** `assist_nm` / `right_assist_nm_now` have **zero** consumers outside two resets
  in `app/spawn_policy.h`, while the assist is live on 79.8–93.7 % of ticks.
- **D-D-F5.** `g_eff`'s pooled max of 1 766 m/s² is a **one-tick numerical artefact** at a
  tape discontinuity (respawn / `O` record). Any future g-driven cue must carry the same
  `epoch` guard `render/trail_chain.h` already uses, or it will fire on a teleport. Stated
  here so it is not rediscovered as a mystery.

---

## 7. CANDIDATE RUNGS — six, each ONE dial, each bit-identical at its default

Every dial defaults to the value that reproduces today's build **exactly**, per the
frozen-kernel precedent (`kernel-v14-leanlead`: ONE dial, 0 = the old tree). None of these is
a kernel change: C9–C13 live entirely in `render/` and `app/`, which is why they can be
A/B'd on one drive.

---

### D-D-R1 — **the engine sings the engine** ★ highest measured payoff
**Dial:** `render::kSledRpmBlend` (+ `SEADS_SLED_RPM_BLEND`), **default 0.0**.
**What:** `SledSynth::set_drivers` takes a second argument, the kernel's rev fraction
`k = clamp((engine_rpm − 1700)/6300, 0, 1)`, and the follower's target becomes
`mix(thumb, k, blend)`. At 0.0 the call is bit-identical and `test_sled_audio.cpp` passes
untouched. The onset detector keeps running on the **thumb** — the brap is an input event and
must not be moved.
**Tape event:** the coast — 8.60 % of every drive at mean 19.34 m/s with the engine at 4 147
rpm and the voice singing 5.8 semitones flat; plus sign-inverted motion on 10.24 % of the
ticks where both channels move.
**Chad's word:** ROLL_COMFORT_HANDOFF.md §0 — *"Work on the math to achieve a balance of fun
and accuracy to real physics, just as our airplane ontological counterpart does."* And
roost_consult_packet.md §1 — *"The conditions should be palpable and observable in the sled
performance."*
**Killing mutation:** at blend 1.0 the tone stops answering a tap the instant the thumb
moves (the kernel's rpm lags the clutch), which would cost the brap's immediacy — so the
dial must be swept, and if the measured gap ever falls below ~0.08 (a quarter of the
follower's own 0.35 idle-ratio sweep) the change is inaudible and the rung is dead.

---

### D-D-R2 — **the sled gets a wind**
**Dial:** `render::kSledWindSpeedGain` (+ `SEADS_SLED_WIND`), **default 0.0**.
**What:** while mounted, `wind_synth.set_drivers` takes
`mix(rd.speed, sled.ground_speed_ms, gain)` for its speed argument. At 0.0 bit-identical —
the aeroplane keeps the channel exactly as today.
**Tape event:** **44.48 % of all ticks above 8 m/s**, 76.64 % moving, mean ground speed
13.22 m/s in contact — and `grep` shows the only consumer of `ground_speed_ms` anywhere in
`render/` is the roost plume's geometry. There is no sound in this game keyed to sled speed.
**Chad's word:** README_SOUNDSCAPE.md §2 states the standing doctrine — anything conveying
**speed** is driven by a live parameter, because a static loop *"kills the sense of speed
instantly"*. And §0b: *"SLiding banging, punchy, jumps."*
**Killing mutation:** if `rd.speed` is NOT ≈ 0 while sledding — i.e. if the player bails out
of a flying aeroplane and the plane keeps flying — the channel is already loud and this
would make it louder. That case must be measured on a real mount before the dial moves off
0; this strand did not measure it (see §8 UNVERIFIED-3).

---

### D-D-R3 — **the track is audible**
**Dial:** `render::kSledSlipVoice` (+ `SEADS_SLED_SLIPVOICE`), **default 0.0**.
**What:** a fourth layer in `SledSynth` — filtered noise whose level rides
`|track_slip|` × contact and whose brightness rides `belt_speed_ms`. At 0.0 the mix is
bit-identical (the layer's gain multiplies to zero before the soft-clip).
**Tape event:** mean \|track_slip\| **0.522** in contact and moving; belt running **20.50
m/s faster than the ground** on average; \|slip\| > 0.3 on **53.03 %** of ticks and on
**70.2 % of RockOutcrop / 65.4 % of Bush** time; and **33.77 % of ground-moving ticks have
\|slip\| > 0.3 with `roost_flux` < 0.02** — the track is spinning and there is neither a
plume nor a sound.
**Chad's word:** roost_consult_packet.md §1 — *"Snow that is properly shaded and felt, not
some cheap drawn in illusion."* And §0 — *"I've seen lots of snowmobiles drift around
corners and then with throttle, straighten out. The feel is really good."*
**Killing mutation:** if `track_slip` were near-constant the layer is a constant hiss and
worthless. It is not — p50 0.395, p90 0.955, so it spans the range. The real risk is the
opposite: at 53 % duty it could become the loudest thing in the mix, so the level must sit
under `kSledRunGain` and be swept against `render/mix_levels.h`, not chosen.

---

### D-D-R4 — **the invisible hand gets a tell**
**Dial:** `render::kAssistTell` (+ `SEADS_SLED_ASSIST_TELL`), **default 0.0**.
**What:** carry `sim::SledState::assist_nm` into `render::DrawInfo` (one field, beside the
`sled_hull_engage` / `sled_air_s` that are already there for exactly this purpose) and let it
grade **the instrument Chad already chose**: the x-ray grid's slag-orange ball brightens /
the grid tightens with `|assist_nm|`. At 0.0 the panel is pixel-identical.
**Tape event:** the assist is non-zero on **79.8 %–93.7 %** of ticks on every one of the six
v17 drives, pooled p90 **435 N·m**, p95 **575 N·m**, p99 **1 115 N·m**. Against the machine's
own tipping scale — weight × half-stance, `mass_kg` 331 (`sim/sled.h:543`) × 9.80665 ×
0.4635 m = **1 505 N·m** — p95 is **38 %** and p99 is **74 %** of the whole roll budget.
**Chad's word:** ROLL_COMFORT_HANDOFF.md §0 — *"Just allow the balance of body mechanism to
ENHANCE ability"* — a player cannot learn an enhancement he has never been shown; and §0b —
*"the measure of it shall be if my intent is heard."*
**Killing mutation:** `docs/gi4_ride_handoff.md` §2 states the static weight as
`(331 + 87.5) × 9.81 = 4106 N`, while `sim/sled.h:543-544` says `mass_kg = 331.0 // machine
+ rider, TOTAL` and `rider_mass_kg 87.5 // split OUT of mass_kg, NEVER added`. **These two
disagree and this strand does not resolve it** — on the gi4 number the tipping scale is 1 903
N·m and p95 falls to 30 %. Either way the assist is a third to three-quarters of the budget,
so the rung survives both readings; but any *number* quoted to Chad must name which mass it
used. Second killing mutation: if the assist's duty is high but its magnitude is mostly the
p50 **45 N·m** (3 % of budget), a linear tell would read as always-on — the tell must be
gated on a band, not on non-zero.

---

### D-D-R5 — **the horizon carries the roll** ⚠ ships at 0, A/B only
**Dial:** `render::kSledCamRoll` (+ `SEADS_SLED_CAM_ROLL`), **default 0.0**.
**What:** `pose.up = slerp(sup, body_up_tangent, kSledCamRoll)` with a hard cap (the blend
must never approach the degeneracy the existing `kSledLookElMax = 1.45` note already warns
about). At 0.0 `pose.up == sup` exactly and every framing number is unchanged. **The law
already exists in-tree** — `app/rest_horizon.h`, shared by the aeroplane and the Sting — so
this is a wiring change, not an invention.
**Tape event:** tilt past 20° on **23.42 %** of ticks and past 45° on **8.92 %**, pooled p90
37.25°, and the eye is told none of it: `app/main.cpp:9827` sets `pose.up = sup`
unconditionally.
**Chad's word:** ROLL_COMFORT_HANDOFF.md §0 — *"she's too unsteady … Make it possible to
roll but not the rule"* — roll is the quantity he is grading, and §0b's measure is *"if my
intent is heard."*
**Killing mutation:** at p95 tilt **100.75°** a full blend inverts the view, and the R1
motion-blur / tunnel-vision speed FX in this same family was **reverted at Chad's word**
(§8 UNVERIFIED-2). A partial blend (0.25–0.35) with a cap is the only shape worth a drive,
and if he reads the roll fine off the silhouette the rung is dead on the first A/B. This is a
candidate, not a recommendation.

---

### D-D-R6 — **the track turns**
**Dial:** `render::kSledTrackScroll` (+ `SEADS_SLED_TRACK_SCROLL`), **default 0.0**.
**What:** scroll the track/tread visual at the kernel's own `belt_speed_ms` — the field
`sim/sled.h:1307` already says the rig should read — phase-accumulated on `sled_ticks × dt_s`
(the AT-9 discipline the x-ray motes already use, never a wall clock). At 0.0 the model draws
exactly as today.
**Tape event:** `belt_speed_ms` runs a mean **20.50 m/s faster than the ground** while in
contact; `grep` finds **no** track/belt/cleat/scroll/tread animation in
`render/sled_model.cpp`'s 7 687 lines and no belt channel in `SledRig`; and `belt_speed_ms`
has zero consumers tree-wide outside the tape and the probe (defect D-D-F1).
**Chad's word:** roost_consult_packet.md §1 — *"not some cheap drawn in illusion"*; and the
kernel's own sentence at `sim/sled.h:1307-1310`, which says a decoupled coast at zero belt
speed **"IS the 100 % slip an observer should see."**
**Killing mutation:** **this may not be a dial at all.** If `indy650.glb` has no separable
track mesh or no scrollable UV, this is an ASSET rung — and the memory hook
`indy650-blend-asset-divergence` records that a full re-export of that .blend **crashes the
mount**, so the geometry would have to ship through the surgical GLB patch. Checking the GLB
is the first thing this rung does; if the mesh is welded, the rung converts to a shader UV
offset or dies. This strand did **not** open the GLB.

---

## 8. UNVERIFIED — claims this strand refuses to make

- **UNVERIFIED-1.** The Chad quotes in §1 attributed to `render/sled_audio.h` (2026-08-17 on
  the idle/riding/brap layers) and to `app/main.cpp` (the camera asks: *"can you actually
  make my camera track closer to the rider"*, *"freelook doesn't work, can't see the sled but
  from behind"*, *"view while walking is too zoomed forward"*, *"zoom level should be
  variable with mouse scroll wheel"*) are carried **by source comments**, not by a primary
  doc in this worktree. `grep` over `docs/*.md` did not find the camera ones. They are
  treated as faithfully transcribed (this house's comments carry his words verbatim by
  convention) but they are **second-hand** and are not used as the sole justification for any
  rung in §7.
- **UNVERIFIED-2.** The R1 motion-blur / tunnel-vision speed FX being reverted at Chad's word
  with the reason *"looked bad"* comes from **session memory** (`sting-rpas-plan`,
  2026-09-12; lane reset to main `28e741247`, scrap tag `scrapped/speedfx-r1-20260912`). The
  raw quote was not found in this worktree (`grep -rniE "tunnel.?vision|motion blur|speedfx"`
  over `*.md` returns only `docs/world_build_plan.md:432`, unrelated). **Treated as a
  paraphrase.** It is used only as a RISK flag on D-D-R5 and C4, never as a justification.
- **UNVERIFIED-3.** Whether `rd.speed` is actually ≈ 0 while the player is on the sled. The
  *wiring* claim is airtight (`app/main.cpp:11298` builds `rd` from `draw_state`, the
  aeroplane; `:11745` passes `rd.speed` to `wind_synth`; no sled field reaches it). The
  *value* claim — that the parked aeroplane reports ~0 — was **not measured**, and the bail-
  out-of-a-flying-plane case was not examined. D-D-R2 is blocked on measuring it.
- **UNVERIFIED-4.** The mass used for the tipping-moment scale in D-D-R4: `sim/sled.h:543-544`
  and `docs/gi4_ride_handoff.md` §2 disagree (331 kg vs 418.5 kg). Both readings are carried;
  neither is adopted.
- **UNVERIFIED-5.** The audio device period in §4.3 is a **GUESS** (10–20 ms, typical WASAPI
  shared). It was not measured and no claim depends on it.
- **REFUSED.** No statement is made about how any of this *feels*. Every §7 rung is sized by
  a tape number and anchored to one of Chad's sentences; none is graded, because grading is
  his. Per §0b: *"the measure of it shall be if my intent is heard."*
- **NOT AUDITED.** The GLB's track mesh (D-D-R6's blocker); the Sting's and the aeroplane's
  channels except where they supply a precedent; the map/HUD channels outside the sled dash;
  `SEADS_BUCK` (the R4a superman instrument) as a feedback channel.

---

## 9. One-line summary for the convergence pass

**The eye is current to ~25 ms and honest about position, attitude, suspension and the
rider's lean; the ear is a quarter-second-to-a-second behind and is listening to the player's
thumb rather than to the machine; and the three kernel channels that would close the loop —
`engine_rpm`, `belt_speed_ms`, `track_slip` — plus the roll-assist torque that is live 87 %
of the time, reach no output channel at all.**
