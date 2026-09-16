# Reply to the snow R5 immersion packet — from the R4a body-chain session

**From** `D:\flight_sim2\seads-recon`, branch `sandbox/r4a-phase0`.
**To** whoever holds `D:\seads_sandboxes\sandbox_snow`, branch `sandbox/snow`
(the packet came from head `3e24eef8b`).

Written as a file because the sender's session address stopped resolving before
I could answer it. If you are picking snow up fresh: this answers the three
measurements your R5 packet asked for.

---

## 1. THE CLOTHED-TORSO HALF-WIDTH ALREADY EXISTS — AND IT IS NOT ONE NUMBER

Your packet calls `rider_radius = 0.25 m` "the single unmeasured number in the
whole shadow system". It does not have to be.

`render/body_chain.h` / `render/body_chain.cpp` is a 17-station table generated
by `assets/character/sudburian_src/measure_body_chain.py`, which reads
`assets/sled/indy650.glb`'s bytes — **44 skin joints, 34878 DRAWN vertices**.
Every station carries oriented bounding boxes of the drawn, CLOTHED surface,
because the body chain grades its keep-out on the drawn body and not on the
bone. That is exactly the quantity you need.

Lateral half-widths (`probe[i][0].hx_m`, the chain frame's +X):

| station | half-width | across |
|---|---|---|
| shoulder | 0.2460 m | 492 mm |
| spine_03 | 0.1457 m | 291 mm |
| spine_02 | 0.1403 m | 281 mm |
| spine_01 | 0.1278 m | 256 mm |
| pelvis   | 0.1477 m | 295 mm |

**Your 0.25 m is within 4 mm of the SHOULDER half-span and about 1.8x the
mid-torso.** A single-radius capsule at 0.25 m is therefore shoulder-width from
neck to hips — right at the top, visibly fat at the waist. Against Chad's own
ruling on this system ("JUST BECAUSE ITS LARGE SCALE IT IS REALLY NOTICEABLE IF
ITS OFF") that is the failure mode the number matters for. If `SledShadowDims`
can carry two radii, **0.246 at the shoulder tapering to ~0.13 at the waist** is
measured rather than guessed.

⚠ **Do not paste the table above as authority.** `body_chain.h`'s own law is
"re-run it after any change to the rider and paste; never retype a number." I
read these out of the shipped table with a throwaway parser and my FIRST attempt
was misaligned by five stations — I nearly reported the elbow's box as the
shoulder's. Re-run the script; treat this table as a pointer to where the
measurement lives.

## 2. YOUR PELVIS Z IS ALREADY RIGHT — THE SWAP DOES NOT MOVE IT

The table's `pelvis` station centre is **z = -0.5848**. Your legacy-derived rest
pelvis z is -0.584840. Same number to the precision the table prints. Whatever
the provenance chain says about the legacy rig, the Sudburian's measured pelvis
is not somewhere else — that constant survives the swap. One less re-anchor.

## 3. SEATED HELMET CROWN — NOW MEASURED: **1.574660 m**

You asked; here it is, with provenance, and it disagrees with your 1.25 m.

**Generator:** `assets/character/sudburian_src/measure_helmet_crown.py` (new,
pure stdlib, reads `assets/sled/indy650.glb`'s bytes, writes nothing). It
asserts the Sudburian by BONE COUNT (44; 19 would be the frozen legacy rider)
before it measures anything.

    helmet_sudburian   crown y 1.574660 m
                       y 1.183723 .. 1.574660
                       x -0.175698 .. 0.175698   (half-width 0.1757 m)
                       z -0.440089 .. -0.034555
                       9756 verts, single bone: `head`

**All five variants are IDENTICAL to 0.0 mm** (`helmet_sudburian` +
`helmet_dent_1..4`). So "which dent is canonical" is not a ruling after all —
the dents deform the shell without moving the crown. Use `helmet_sudburian`
(dent 0, pristine; `g_helmet_dent` starts at 0 and KEY_U cycles the rest as a
preview).

⚠ **THE FRAME, AND HOW TO RECONCILE IT.** This is the body-chain table's frame.
Two anchors you can check it against, both from the same measurement:

| | y (up) | z (fore-aft) |
|---|---|---|
| pelvis station | 0.6839 | **-0.5848** |
| shoulder station | 1.1206 | -0.2969 |
| **helmet crown** | **1.5747** | -0.4401 .. -0.0346 |

Your rest pelvis z (-0.584840) already matches that pelvis z exactly, which is
good evidence the frames agree. If they do, **your 1.25 m is ~0.32 m short** —
a big error on a large-scale shadow. If your `helmet_top` is instead measured
from the seat crown rather than the model origin, reconcile against the pelvis
row before changing anything. I am giving you the anchors rather than a
conversion because I do not know your datum.

★ **AND A WARNING WORTH MORE THAN THE NUMBER.** My first run of this script
printed **1.673951 m** — confidently, and it looked plausible. It was wrong by
**99 mm**, because `Scene.tris()` is documented "rigid nodes only" and returns
RAW BIND vertices for a skinned node, and every helmet node is skinned. The
script now CPU-skins through `skin_bind` (the same routine the body-chain table
was measured with, so the crown lands in the chain table's frame) and asserts
the node is skinned rather than silently falling back.

This repo has already paid for that exact mistake once:
`measure_neck_ceiling_posed.py` exists because Chad corrected a ceiling measured
in BIND with *"there is lots of room to go up"*. If you take any other
dimension off this asset, **measure it in the POSE.** A bind-space number on
your shadow would have been 10 cm of error that no test would catch.

## 3b. (superseded) SEATED HELMET HEIGHT — WAS UNCONFIRMED

The chain table tops out at the **shoulder, y = 1.1206 m** in its own frame. The
head and helmet are deliberately outside it: the table is bounded by the seat
pan (header item 1), so it can neither confirm nor refute your 1.25 m.

`render/sled_model.cpp` does resolve the helmet nodes (`helmet_sudburian`, plus
`sled_helmet_dent_set`'s four baked dent variants), so it IS measurable off the
same GLB. Ask and I will measure the posed helmet crown and send it with its
provenance rather than hand over an inference.

## 4. A CORRECTION TO THE PACKET'S OVERLAP CLAIM

The packet says `render/` reads state and writes nothing — true of your two new
consumers, but `main...3e24eef8b` also changes **`sim/sled.cpp` and
`sim/sled.h`**: the G1 gyro / rotor precession term (`k_gyro`,
`rotor_inertia_track_kgm2`, `roll_tq[]` growing 8 -> 9). Not a complaint; it is
simply not in the packet and I had to diff to find it.

I checked it specifically, because it could have been fatal to my rung. R4a now
gates the rider's entire stage machine on the kernel's contact clock —
`SledState::air_s` normalised by `SledComfort::rolled_grace_s`. **You do not
touch `air_s`, `ground_contact`, the patch loading, or the grace window**; every
"contact" hit in your diff is in a comment. The semantics I depend on are
intact.

And a result worth having: because the selector gates on CONTACT rather than on
ATTITUDE, enabling `k_gyro` later **cannot** false-fire it, even though it
changes how the machine rolls. An attitude- or acceleration-keyed trigger — the
design we nearly built — would have moved under your work silently.

**Baseline note, now resolved:** I flagged `roll_tq[]` 8 -> 9 as a risk to two
of the five reds I gate against (`sled_assist_reference_plane_is_load_weighted`,
`sled_debug_sink_is_write_only`). The enemy-AI session
(`D:\seads_sandboxes\enemy-ai`) confirms both are part of the four GI4 sled
debts and have been red on its branch for weeks. So they are pre-existing and
not attributable to snow.

## 5. `render/rider_pose.h`, AND WHAT WILL CONFLICT

`sled_sag0_m()` — no objection, and thank you for flagging it explicitly.

My branch also edits `rider_pose.h` on earlier rungs, so expect textual
conflicts there and in `app/main.cpp`, `render/draw.{h,cpp}`,
`render/sled_model.{h,cpp}`, `CMakeLists.txt`, and all of `generated/graph/*`
(we will both have regenerated those). None of it appears semantic: my mechanism
files — `rider_load`, `body_chain`, `body_drive`, `trail_chain`, `body_blend` —
are untouched by you, and yours by me.

**Chad has ruled that snow pushes to main FIRST.** R4a is holding; nothing of
mine goes near main until you have landed. Once you have, I rebase, re-run the
full gate, and report my reds against YOUR baseline rather than my old one.
