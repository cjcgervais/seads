# SEADS Simulator Sickness & Camera Design Research Memo

**To:** SEADS Development (comfort program, auto/comfort-orient)
**Subject:** Motion sickness, disorientation, and orientation-recovery — research findings and actionable recommendations for mouse-aim dogfighting on a 15 km sphere
**Date:** 2026-07-16 (Sonnet research agent, Fable-inquisitor commissioned)

---

## Section 1 — Simulator Sickness Causes on Desktop Flat Screens

### 1.1 The Core Mechanism

Simulator sickness on a desktop monitor is a subtype of **visually-induced motion sickness (VIMS)**: the visual system reports motion that the vestibular system does not confirm. This is the inverse of classic vehicle sickness (vestibular signal with no visual confirmation). The brain's evolved response — evolved to interpret such sensory disagreement as neurotoxin ingestion — triggers nausea, disorientation, and cold sweating ([Simulator Sickness Explained, Motion Sick Lab](https://motionsicklab.com/blog/simulator-sickness-explained)).

**Sensory conflict theory** (Kennedy, Hettinger, Lilienthal) remains the most widely applied model: the brain's expected sensory correlations are violated, and the violation cost is proportional to its duration, amplitude, and complexity ([ScienceDirect Overview](https://www.sciencedirect.com/topics/computer-science/simulator-sickness); [Hettinger et al., 1987, SAGE](https://journals.sagepub.com/doi/10.1177/154193128703100503)).

### 1.2 Which Rotation Axes Are Worst

A key study (Flanagan et al., [PubMed 22097636](https://pubmed.ncbi.nlm.nih.gov/22097636/)) exposed 61 participants to simulated rollercoaster video on a large projection screen with different rotation conditions:

- **Pitch only:** mean sickness score **1.95** (lowest)
- **Pitch + Roll:** mean score **4.33** (significantly higher, p < 0.05)
- **Pitch + Roll + Yaw:** mean score **5.30** (highest, but not significantly worse than dual-axis)

The critical finding for SEADS: **multi-axis rotation is dramatically more sickening than single-axis.** An Immelmann (combined pitch + roll at the top) and a split-S (nose-down half-roll) force pitch and roll simultaneously, which is why they cause more disorientation than a simple loop. Chained aerobatics compound the dose monotonically until a plateau is reached around dual-axis.

A separate PLOS ONE study ([doi:10.1371/journal.pone.0175305](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0175305)) found that **vection strength — the subjective sense of self-rotation — is the dominant predictor of sickness severity**, not the type of eye movement or axis per se. Vection scores of 0 → maximum predicted sickness rising from 1.6 to 11 on their scale. Four of that study's participants aborted trials entirely.

### 1.3 VR vs. Desktop: What Transfers

A 2025 study comparing VR and desktop navigation ([Frontiers in Virtual Reality](https://www.frontiersin.org/journals/virtual-reality/articles/10.3389/frvir.2025.1518735/full)):

- VR produced significantly higher total SSQ scores than desktop (p = 0.002, effect size δ = −0.344, small to moderate)
- Both **nausea and disorientation** manifested in both modalities, with lower severity on desktop
- **Habituation transferred across modalities** — afternoon sessions showed lower symptoms in both
- Oculomotor strain showed the strongest habituation effect in both conditions

**What transfers from VR to desktop:**
- Rotation/vection as the primary sickness trigger ✓
- Benefit of stable peripheral reference frames ✓
- Frame rate consistency mattering more than peak rate ✓
- Multi-axis rotation being worse than single-axis ✓
- Snap/discrete transitions being less nauseating than smooth continuous ones ✓

**What does NOT transfer:**
- Absolute severity (VR induces roughly 2× worse SSQ scores)
- Latency requirements (desktop tolerates higher latency without vestibulo-ocular reflex conflict)
- Stereoscopic depth paradox (binocular conflict irrelevant on flat screen)
- IPD mismatch effects
- FOV restriction via vignetting has weaker evidence at desktop distances because the monitor's physical frame already provides a natural rest boundary

The desktop monitor's physical bezel is itself a partial rest frame — it is always visible peripherally. This is one reason desktop flight sims are tolerated for long sessions while VR equivalents cause quicker onset.

### 1.4 Factors Specific to Flight Sims

Research on flight simulators specifically ([MDPI Aerospace 11(2):139](https://www.mdpi.com/2226-4310/11/2/139)) identifies:

- **Pitch angle discrepancy** between visual display and physical sensory reality is the strongest correlated factor
- **Yaw velocity mismatch** is the second strongest correlate
- **Scene density / speed** modulates vection: sparser terrain → weaker vection → less sickness; at very high speeds over detailed terrain, sickness onset accelerates substantially

For SEADS on the 15 km sphere: the planet's terrain is close (low altitude), visually detailed, and curves away fast. High-speed aerobatics above the Sudbury terrain therefore generate strong optical flow, maximizing vection during maneuvers like Immelmanns.

### 1.5 FOV on Desktop Monitors

A study on cybersickness and desktop FOV ([Cybersickness and Desktop Simulations, ResearchGate 224883419](https://www.researchgate.net/publication/224883419_Cybersickness_and_desktop_simulations_Field_of_view_effects_and_user_experience)) found:

- **Below ~80° internal FOV**: significantly more nausea and fatigue
- **90–100°**: sweet spot for most players
- **Above ~110°**: fisheye distortion increases discomfort

One counterintuitive finding specific to desktop: when internal FOV (virtual camera angle) closely matches external FOV (screen size + viewing distance), sickness can paradoxically be *greater* than mismatched conditions ([ScienceDirect 0141938210000934](https://www.sciencedirect.com/science/article/abs/pii/S0141938210000934)). This is attributed to the matched condition fooling the peripheral visual system more completely.

**Practical guidance for SEADS:** 90–105° horizontal FOV is the evidence-supported range. The currently space-first dark sky provides high contrast optical flow cues at the horizon but minimal peripheral motion cues overhead — this is the right direction.

---

## Section 2 — The Snap vs. Smooth Question

### 2.1 Core Evidence

The snap-turn literature is primarily VR-focused but provides the clearest data on discrete vs. continuous visual transitions:

**Snap turning works by eliminating continuous optic flow during rotation** — the transition is instantaneous so the brain never receives a prolonged conflicting flow signal. Smooth turning provides a sustained rotation signal that, without corresponding vestibular confirmation, generates vection and sickness.

A viewpoint snapping study ([ResearchGate 325064430](https://www.researchgate.net/publication/325064430_Viewpoint_Snapping_to_Reduce_Cybersickness_in_Virtual_Reality)) found snapping reduced cybersickness with "low cost in terms of user performance and presence."

Quantitative findings from the literature:
- **30° snap increments** produced significantly lower cybersickness than continuous 30°/s baseline ([Antaeus AR Playbook](https://medium.com/antaeus-ar/beating-cybersickness-the-complete-vr-ar-comfort-playbook-2025-59ea4e083b9f))
- **22.5° snaps** at a threshold of 25°/s achieved **40% reduction in cybersickness** compared to pure continuous scrolling
- Optimal reported range: **22.5° to 45°** discrete increments

A 2024 study on FOV restriction + snap turning combined (N=201, [PubMed 39331562](https://pubmed.ncbi.nlm.nih.gov/39331562/)) found:
- Each technique alone reduced cybersickness vs. control
- **Combining them did NOT further reduce cybersickness** and in some cases the combination was as bad as no mitigation at all
- "Caution is warranted when combining multiple mitigation tools" — interactions can be counterintuitive

### 2.2 The Transition Duration Question

The evidence favors **discrete/instant over any duration of smooth**. A critical observation from the VR accessibility literature ([arXiv 2508.13051](https://arxiv.org/pdf/2508.13051)) is that snap turn's benefit is specifically its zero-duration transition — even very fast smooth turns (< 100 ms) still generate some optic flow sensation. Any non-zero duration introduces a vection signal.

The implication for chase-camera position recovery (rather than viewpoint rotation): **camera position springs (slow follow) are fine; camera orientation transitions should be as close to instant as possible**, since it is orientation change that generates vection. A camera that snaps its heading but smoothly follows position causes far less nausea than one that slews both.

### 2.3 Application to SEADS Freelook Release

The current "slow convergence from front-oblique" when releasing freelook is exactly the wrong profile: it applies a sustained rotational slew at the moment the player wants to be oriented. This generates vection during a period of high spatial uncertainty (post-maneuver).

**The evidence-supported alternative:** On freelook release, snap the camera's orientation to the aircraft nose axis (or the sanctioned open-loop roll-back animation), then allow only position (arm length, lag) to spring settle. The orientation snap itself should be frame-synchronous.

Per the SEADS codebase constraints, freelook-release with a discrete open-loop capture animation is already the sanctioned pattern. The research confirms this is the right design: the snap must be **orientation-instantaneous**, and any settling should be in **position space only** (spring arm lag, FOV ease), never in angular space.

---

## Section 3 — Stable Reference Frames and Peripheral Cues

### 3.1 The Rest Frame Effect

A **rest frame** (RF) is a set of virtual objects that remain fixed relative to the physical world (not the virtual environment), providing a stable visual anchor. Research consistently shows rest frames reduce VR sickness ([Visually-Induced Motion Sickness Reduction via Static and Dynamic Rest Frames, ResearchGate 327331902](https://www.researchgate.net/publication/327331902_Visually-Induced_Motion_Sickness_Reduction_via_Static_and_Dynamic_Rest_Frames)).

Implementations that work:
- Earth-fixed grid overlay
- Natural independent visual background (trees, clouds stationary relative to room)
- Cockpit/instrument panel overlay

An early PubMed study ([PubMed 10102741](https://pubmed.ncbi.nlm.nih.gov/10102741/)) showed that providing an independent visual background consistent with the inertial rest frame reduced simulator sickness even when the scene content was not consistent with that frame.

**For FPS/flight specifically:** A JMIR Serious Games study ([PMC8323020](https://pmc.ncbi.nlm.nih.gov/articles/PMC8323020/)) tested crosshairs as in-game rest frames in VR FPS. **Key quantitative result:**
- No visual guide: total SSQ score **16.95**
- 30%-aspect-ratio crosshair synchronized to head tracking direction: total SSQ score **12.62** (p = .04, significant reduction)
- Crosshairs synced to controller direction: no improvement
- Larger crosshairs (50% of aspect ratio): no improvement

**The critical finding:** The guide must be **head-gaze aligned** (the player's natural forward axis) not controller-aligned. It must be **30% of the vertical aspect ratio in size** — large enough to register in peripheral vision, not so large as to obstruct the scene. Crosshairs at 50% did not help.

### 3.2 The Virtual Nose Question

Purdue's "Nasum Virtualis" study ([Purdue newsroom](https://purdue.edu/newsroom/releases/2015/Q1/virtual-nose-may-reduce-simulator-sickness-in-video-games.html)) found a small static nose in the center-lower visual field extended average play time by 94.2 seconds before nausea in a villa exploration sim.

**However, a 2024 preregistered replication failed:** ([PMC11522211](https://pmc.ncbi.nlm.nih.gov/articles/PMC11522211/)) N=32 within-subjects, mean SSQ-Total identical with and without nose (0.525 vs 0.526, p = 0.993). The authors argue a virtual nose cannot function as a rest frame because it moves with the camera and thus fails to distinguish head rotations from simulated rotations — the theoretical requirement for a rest frame.

**Bottom line:** The virtual nose is **not a reliable technique** as of the best current evidence. Do not invest in it for SEADS.

### 3.3 What Does Work at the Periphery

Despite the nose replication failure, **cockpit frames / peripheral static overlays** do have evidence. The theoretical mechanism differs from the nose: a cockpit frame works by matching the player's expected real-world visual peripheral experience (you always see the inside of a car, cockpit, etc.), which suppresses the vection sensation caused by the main scene motion.

The Elite: Dangerous / VR cockpit observation is cited in multiple sources: cockpit interiors work so well in VR specifically because "even as your ship twirls in space, your first-person view of the cockpit remains constant" providing a fixed reference ([GamesRadar virtual nose article](https://www.gamesradar.com/virtual-reality-noses-motion-sickness/)).

For a peripheral rest frame to be effective, it should:
1. Be **stationary relative to the screen** (not world-aligned, not aim-aligned)
2. Live in **peripheral vision** — the lower corners and sides of the screen, not the center
3. Be **semi-transparent** to reduce obstructions (full opacity reduces presence; research notes that peripheral rest frames with varying density/opacity achieve the best balance — [Springer 10.1007/s10055-025-01116-1](https://link.springer.com/article/10.1007/s10055-025-01116-1))
4. Preferably **match the player's self-model** (cockpit frame > abstract grid)

**For SEADS specifically:** The aircraft is chase-cam but currently renders only the sky and terrain around it. Adding even a minimal frame (gunsight ring/windscreen corner struts) as screen-anchored overlay would provide a rest frame benefit without touching the camera or aim path.

### 3.4 HUD Cue Placement

Research on attitude indicators in flight sims ([Attitude Indicator Design in Primary Flight Display, Taylor & Francis](https://www.tandfonline.com/doi/abs/10.1080/24721840.2018.1486714)) found extended horizon displays (spanning the full screen width) produced better pilot performance and less disorientation than masked (traditional small ADI) versions — all trends favored larger.

Critical finding from spatial disorientation studies: pilots under disorientation tilt their head toward the visual horizon, using it as a primary anchor. The horizon in the game must be **visually distinct from the sky** to function as an anchor.

The SEADS space-first dark sky with silver horizon band is actually well-configured here — the strong luminance discontinuity at the horizon (dark space / silver atmosphere) provides an anchor. The key constraint for HUD cues:

- **Keep orientation cues (bank arc, attitude reference) at screen periphery**, not at the reticle
- A **bank angle arc at the top edge** of the screen (like a pitch ladder) with 10°/20°/30°/45° tick marks works peripherally and is the Falcon 4.0 convention
- A **ghost horizon line** spanning the full screen width (very thin, low opacity) would serve as the strongest single recovery cue — it gives the player an instant read of roll without looking away from the reticle

---

## Section 4 — Prior Art: Target Awareness Without Disorientation

### 4.1 Padlock View (Falcon 4.0, DCS World, IL-2)

**How it works:** A dedicated key locks the virtual pilot's head to a selected target. The view pans continuously to keep the target centered even if it moves behind the player. The original scene FOV is preserved; only the viewing direction changes.

**Player feedback on DCS padlock** ([DCS forums, forum.dcs.world/topic/203424](https://forum.dcs.world/topic/203424-anyone-use-padlock/)): Padlock is considered a comfort accessibility feature for players without TrackIR; it "can cause disorientation when tracking at extreme angles" and is banned on many multiplayer servers as it is considered a competitive advantage. The disorientation at extreme angles is the known failure mode: when the target goes behind and below, the head-track jumps to nearly inverted, and pilots lose the horizon completely.

**The IL-2 mitigation for this** ([Mission4Today padlock thread](https://www.mission4today.com/index.php?name=ForumsPro&file=viewtopic&t=10758)): **PadlockViewForward** — a separate key that instantly snaps the view back to the aircraft nose (forward-centered) without breaking the padlock. Players call this "vital." The design pattern is: *continuous tracking for acquisition, instant snap for recovery.* This is a discrete orientation reset embedded inside an otherwise continuous system — exactly the snap-vs.-smooth hierarchy the science supports.

### 4.2 IL-2 Snap View System

IL-2 Great Battles ([IL-2 snap view tutorial](https://forum.il2sturmovik.com/topic/7433-snap-view-tutorial-for-custom-camera-saves/)) provides four camera modes:
- **Centered snap:** view pans on key hold, returns to center on release
- **Fixed snap:** holds at the new direction until reset
- **Additive snap:** incremental movement per press
- **Pan view:** analog view following input pressure

The "centered snap" mode is preferred for dogfighting: directional checks are **key-hold gated** and release automatically re-centers to the forward axis. Players bind this to HOTAS hat switches. The snap-to-center on release means disorientation cannot accumulate beyond the hold duration.

For a mouse-aim game like SEADS without HOTAS, this maps to: hold a modifier key to freelook (mouse controls camera, not aim), release it to snap back instantly — which is the existing SEADS design. The issue is the **convergence speed on release**, not the concept.

### 4.3 War Thunder Mouse Aim

War Thunder's "Arcade" mouse aim ([GitHub brihernandez/MouseFlight](https://github.com/brihernandez/MouseFlight)) uses a **decoupled aim/camera architecture**:
- The Mouse Aim transform rotates directly from cursor position (1:1, unsmoothed)
- A separate camera rig orbits the aircraft with a spring-lag
- **Rotations happen relative to the camera** — so moving the cursor left always moves the aim point left on screen regardless of aircraft bank

The developer notes this avoids "jarring camera movement" that plague simpler orbit cameras. The key insight: camera **position** can spring-follow, but the **mapping of mouse→screen direction must always be consistent**. If the camera rolls with the plane and the mouse still maps left=world-left, the experienced pilot experiences aim drift after banking — exactly the SEADS problem during vertical maneuvers with holonomy-bearing up-vector. (SEADS solves this the other way: camera-up = carried aim-up so mouse-up ≡ screen-up — the cost is the post-maneuver world roll this program addresses.)

War Thunder's "C" key in Arcade mode disconnects the look from the aim — mouse look moves the view without affecting the Instructor parameter ([War Thunder forum](https://forum.warthunder.com/t/meaning-of-the-aircraft-mouse-aim-controls/10227)). This is the check-six equivalent in their system, and it is a **discrete key-hold** pattern, not a continuous camera.

### 4.4 Ace Combat Assault Horizon's DFM Camera

The Dogfight Mode (DFM) in Ace Combat: Assault Horizon repositioned the camera to a closer, cinematic view and partially overrode controls to align the aircraft within 500 feet behind the target. This is the most extreme "target awareness camera" in the genre ([Acepedia Close Range Assault](https://acecombat.fandom.com/wiki/Close_Range_Assault)).

**Community verdict:** The DFM system was removed in Ace Combat 7 and its absence was welcomed. Players felt it removed agency and created disorientation when the camera auto-tracked in ways that conflicted with what the pilot was trying to do. The auto-reorientation was perceived as nauseating because it was system-driven, not player-driven — the camera's velocity was unpredictable. Lesson: **auto-tracking that surprises the player is worse than no auto-tracking.**

### 4.5 Screen-Edge Target Indicators (Off-Screen Awareness)

A major source of freelook-induced disorientation in multi-enemy fights is the **need to rotate view to find threats**. The alternative — screen-edge indicators — keeps spatial awareness without requiring view rotation:

The standard game technique ([Envato Tuts+ guide](https://code.tutsplus.com/positioning-on-screen-indicators-to-point-to-off-screen-targets--gamedev-6644t)): For each enemy not in the camera frustum, compute the direction from player to target in screen space, then place an arrow indicator at the screen edge along that direction. Distance can be encoded as opacity or size.

Games that use this effectively: Elite: Dangerous, Everspace 2, War Thunder's radar mini-map. The design goal is **reducing the angular range the player must sweep the freelook** — with good screen-edge indicators, a pilot can complete an entire engagement knowing threat positions without ever rotating past ~45° off-nose.

**For SEADS:** Screen-edge threat arrows (a thin arc segment or small caret at the edge) would directly reduce the frequency of large freelook excursions that cause disorientation. These should be **in-world-consistent** (pointing in the actual threat direction in azimuth/elevation) and must not appear at the reticle center.

### 4.6 Radar / Situational Awareness Mini-Maps

Most combat flight sims pair the padlock with a **radar scope** or **mini-map** that shows threat position in a top-down or plan view. This gives the pilot a pre-attentive read of tactical geometry without any camera motion. The radar doesn't cause vection because it's a 2D abstract representation. It complements the screen-edge indicator: radar gives the full tactical picture; edge indicators give the "most urgent threat direction."

---

## Section 5 — Orientation Recovery After Aerobatics

### 5.1 The Horizon as Primary Anchor

The FAA and aviation research ([FAA Spatial Disorientation brochure](https://www.faa.gov/pilots/safety/pilotsafetybrochures/media/spatiald_visillus.pdf)) consistently identify the visible horizon as the single most important spatial orientation cue. When pilots enter unusual attitudes, the recovery technique is: find the horizon first, then correct bank, then correct pitch. Losing the horizon (night, cloud, inverted flight) is the most reliable path to spatial disorientation.

For SEADS post-aerobatics orientation recovery, the relevant insight from the FAA attitude indicator research ([PMC9388953](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9388953/)): pilots made significantly more roll reversal errors in conditions with misleading roll cues (19.4% vs. 6.9% baseline). A HUD attitude indicator must accurately reflect the aircraft's actual attitude — any conflict between the visual scene horizon and the HUD attitude indicator is actively harmful.

### 5.2 Sky/Ground Luminance Asymmetry

The single highest-leverage passive orientation cue in a flight game is **maintaining strong visual contrast between the sky half and the ground half of the screen.** The literature on spatial disorientation in real flight consistently identifies featureless conditions (arctic whiteout, dark sky over bright ground lighting that mimics stars) as the most disorienting because they destroy the sky/ground polarity.

For SEADS: the current "space-first dark sky / bright silver terrain" is specifically the correct direction. The CHI 2026 paper "Breaking Rotational Symmetry" ([ACM DL 10.1145/3772318.3791522](https://dl.acm.org/doi/10.1145/3772318.3791522)) found that even minimal asymmetric landmarks stabilize orientation in screen-based 3D games, and that luminance/edge contrast anchors are more robust than hue alone (relevant for color-blind players).

A **strongly polarized sky** (near-black zenith) against a **strongly lit ground** (silver-white snow/terrain with building density) produces a naturally readable bank angle indicator at every point in the flight: when the screen is split 50/50 horizontally with sky on top, the aircraft is wings-level; when the split is 50/50 vertically, the aircraft is 90° banked. Players internalize this without being told.

**The holonomy problem for SEADS specifically:** After an Immelmann, the carried camera up-vector has drifted. The terrain is now not at the bottom of the screen — it might be at the side. The space-first sky design means the player can still *see* up from down by the luminance (dark=sky=up), but the screen orientation doesn't match this because the camera frame has accumulated roll. This is the core orientation-recovery problem.

### 5.3 The Open-Loop Roll Recovery Option

Since a continuous per-tick camera auto-level was removed for good reason (zenith-pole degeneracy, banned smoothing on aim path), the viable alternative is an **on-demand discrete open-loop roll recovery animation.**

Pattern: player presses a "level horizon" key → the camera frame executes a single captured rotation around the aim axis to bring the screen-up vector aligned with local-up at that instant (or closest approximation), then freezes. This is identical to the sanctioned "once-captured open-loop freelook-release roll" already in the lessons.md, just exposed as a manual key.

The rotation should be **fast but not instant** (≈ 150–250 ms), on a fixed easing curve (ease-in-out sine), not proportional to angle (to avoid velocity jerk). The animation is open-loop (target computed once at keypress, then executed blind — no per-tick correction that could interact with the aim path).

Why not instant? Instant rotation of the entire visual scene is a jarring vection event (it looks like the world snapping). A very fast but not instant animation (150 ms at constant angular velocity is roughly 180°/s for a 90° recovery) reads as camera motion, not world motion — the distinction that avoids nausea.

### 5.4 Sun Position as Orientation Anchor

The sun is the oldest spatial anchor in aviation. When it's visible, its position immediately disambiguates east/west/up/down without reference to the terrain. Post-aerobatics, a visible sun at a known compass position lets the pilot re-anchor without any HUD cue.

The SEADS world handoff mentions a moving sun in Stage 3a. Once the sun disc lands, **it will function as a passive orientation anchor by default** — the stronger and more distinct the sun disc (bright center, visible halo), the more effectively it serves recovery. No additional code is needed for this; it is emergent from a good sky.

### 5.5 Altitude / Variometer Cues

In the peripheral HUD, a **variometer or altitude indicator** tells the pilot whether they are climbing or diving without requiring a horizon fix. After an Immelmann, the pilot's first question is "am I nose-up or nose-down?" A rapidly changing altitude readout at the HUD periphery answers this directly. Rate-of-change (variometer) is faster to parse than absolute altitude during rapid dynamics.

For post-loop recovery, the cue order players naturally use (paralleling FAA unusual attitude recovery):
1. **Variometer direction** (climbing or diving — first question)
2. **Sky/ground luminance** (which screen half is space = up)
3. **HUD bank arc** (how far am I rolled)
4. **Sun position** (once registered)

These are all **passive** cues that don't require the player to take an action, only to glance at the periphery.

---

## Top 6 Recommendations (Ordered by Expected Impact for SEADS)

### REC-1: Instant Orientation Snap on Freelook Release — No Angular Slew (HIGH CONFIDENCE)

**The problem:** Camera converges slowly from front-oblique after freelook release. This is a sustained rotational slew that generates vection during maximum spatial uncertainty (post-maneuver).

**The fix:** On freelook release, snap the camera's orientation to the aircraft nose direction in a single frame (or within ≤ 2 frames). Allow only the camera's **position** (spring arm, lag distance) to settle smoothly. The angular component must be instantaneous.

**Evidence:** Snap turning research consistently shows 22.5°–45° discrete orientation changes cause significantly less sickness than smooth slews of the same total angle. The zero-duration limit is universally superior. A 150 ms slew to cover a 90° recovery angle maintains 600°/s apparent motion — well above any comfort threshold. The sanctioned IL-2 pattern (PadlockViewForward) is the industry precedent: instant snap, essential for recovery.

**Codebase constraint:** This is already architecturally supported — the open-loop freelook-release roll is the sanctioned pattern. The change is: don't spring/slew the yaw component back; snap it.

### REC-2: Screen-Edge Threat Indicators — Reduce Freelook Excursion Frequency (HIGH CONFIDENCE)

**The problem:** Multi-enemy fights require large freelook excursions (checking multiple threat directions). Each freelook session is a disorientation event — the player's camera up drifts, and after release they are disoriented.

**The fix:** Add screen-edge directional indicators (caret arrows or arc segments) for each enemy aircraft not in the primary camera frustum. The indicator shows the direction and approximate elevation of the threat. This lets the player maintain situational awareness without rotating the camera more than ~30° from the nose.

**Evidence:** Off-screen threat indicators are standard in Everspace, Elite: Dangerous, War Thunder (mini-map radar), and Ace Combat (radar). The principle is elimination of the need to rotate at all — if the pilot knows an enemy is "45° left low," they don't need to freelook there to confirm. This reduces vection events directly.

**Codebase constraint:** Pure HUD draw — screen-space only, no sim/control touch. Must be peripheral (screen edge), not at the reticle. Threat positions come from world state which the render layer already reads.

### REC-3: Strong Sky/Ground Luminance Asymmetry — Passive Orientation Recovery (HIGH CONFIDENCE)

**The problem:** After Immelmanns and loops, the camera up-vector has accumulated holonomy drift. Players cannot immediately tell which screen direction is "up."

**The fix:** Maintain and strengthen the visual polarity between the sky half and terrain half of the scene. The current space-first near-black sky is correct — protect it. Ensure the **horizon band is visually distinct** (high contrast between sky and terrain, not blended). Consider adding a very faint full-screen-width ghost horizon line (1–2px, 15–25% opacity) computed from the true local-up direction and drawn in screen space. This line appears at the true horizon regardless of camera roll, giving an instant "world up" reference without being intrusive.

**Evidence:** CHI 2026 research on orientation in 3D screen games found that luminance/edge contrast anchors are the most robust orientation markers, superior to hue. FAA unusual attitude recovery training depends on finding the horizon first. The screen-wide ghost line is the game equivalent of the aircraft's ADI horizon bar — it disambiguates roll without the player having to interpret the terrain texture.

**Codebase constraint:** Screen-space HUD draw, derived from `normalize(position)` projected into clip space — pure render. Does not touch camera or aim path.

### REC-4: Bank Angle Arc (Peripheral HUD, Screen Top) — Active Orientation Readout (MEDIUM-HIGH CONFIDENCE)

**The problem:** After aerobatics, the player doesn't know their bank angle. They can see the horizon visually, but reading it accurately enough to correct is slow.

**The fix:** Add a bank angle arc at the top center of the screen (Falcon 4.0 convention) with tick marks at 10°, 20°, 30°, and 45°. Compute bank angle from `−asin(dot(body_right, local_up))` per SPEC §7 — the same formula already in the codebase. Display as a semi-circular arc with a moving pointer.

**Evidence:** Attitude indicator research ([Taylor & Francis, 2018](https://www.tandfonline.com/doi/abs/10.1080/24721840.2018.1486714)) found extended horizon displays outperformed masked ones; the bank arc is the HUD abstraction of this finding. Falcon 4.0, DCS, and IL-2 all use this convention. It belongs at the screen periphery (top), not at the reticle, per SEADS HUD rules.

**Codebase constraint:** Pure render, peripheral, reads body frame per existing extraction in `control/extract.*`. No additional sim data needed.

### REC-5: Manual "Level Horizon" Key (Open-Loop Roll Recovery Animation, 150–250 ms) (MEDIUM CONFIDENCE)

**The problem:** After vertical maneuvers, the camera up-vector has drifted (holonomy). The player has no way to recover this without entering freelook and manually rolling back — which requires knowing how much drift has accumulated.

**The fix:** Expose a "level horizon" key that captures the rotation needed to align camera-up with local-up at the moment of keypress, then executes that rotation as an open-loop animation over 150–250 ms on an ease-in-out sine curve. The animation is computed once, executed blind — no per-tick re-targeting (that would be banned smoothing in the aim path). After the animation completes, camera-up is local-up and holonomy debt is cleared.

**Evidence:** The sanctioned open-loop freelook-release roll already in lessons.md establishes this as architecturally sound. The 150–250 ms duration is derived from snap-turn research: instant snaps of large angles are themselves jarring (the visual scene appears to "snap"), while 150 ms at a typical 90° recovery is ~600°/s — fast enough to read as camera motion rather than world motion. The ease-in-out profile prevents velocity jerk at endpoints.

**Codebase constraint:** Must operate on the aim/camera frame roll only (the existing S7-hrz primitive). Must NOT introduce per-tick re-targeting. The animation fires once and stops.

### REC-6: Minimal Peripheral Cockpit Frame Overlay (Steady-State Rest Frame) (MEDIUM CONFIDENCE)

**The problem:** During sustained aerobatics and chained maneuvers, there is no stable peripheral reference frame. The player's visual system has no "ground truth" for where the screen boundary is relative to the world.

**The fix:** Add a very minimal cockpit frame overlay — windscreen corner struts (four corner elements), a gunsight ring/reticle frame, and optionally a dashboard horizon strip at the bottom edge. These are screen-anchored (not world-anchored, not aim-anchored) and remain static regardless of aircraft attitude. Semi-transparent (30–50% opacity).

**Evidence:** The FPS VR study ([PMC8323020](https://pmc.ncbi.nlm.nih.gov/articles/PMC8323020/)) found visual guides at 30% of aspect ratio reduced total SSQ from 16.95 to 12.62 (significant). Cockpit overlay effects are cited across multiple sources as stronger than abstract grids because they match the player's expected self-model. The key design constraint is **peripheral placement** (corners and edges, not center). The virtual nose replications have been mixed — corner elements rather than center nose are the preferred implementation.

**Codebase constraint:** Static screen-space draw — no world data needed. This is cosmetic render only. Must not interfere with the reticle area (center clear). Opacity tunable as a config parameter.

---

## Summary Table

| # | Recommendation | Mechanism | Evidence | Implementation Cost |
|---|---|---|---|---|
| 1 | Instant angular snap on freelook release | Eliminates sustained rotational vection | Strong (snap-turn literature, N=201) | Low — change spring behavior only |
| 2 | Screen-edge threat indicators | Reduces freelook excursion frequency | Strong (industry standard + spatial awareness research) | Medium — new HUD element |
| 3 | Strong sky/ground luminance + ghost horizon line | Passive bank angle readout always visible | Strong (CHI 2026, FAA recovery training) | Low — shader draw |
| 4 | Peripheral bank angle arc at screen top | Active attitude readout at periphery | Strong (flight sim HUD convention + attitude indicator research) | Low — HUD draw from existing extract |
| 5 | Manual "level horizon" key, open-loop 150–250 ms animation | Clears holonomy debt on demand | Moderate (sanctioned pattern + snap principles) | Medium — camera frame animation |
| 6 | Minimal cockpit frame overlay (semi-transparent, screen-anchored) | Steady-state rest frame | Moderate (FPS study, theoretical; virtual nose mixed) | Low — static screen draw |

⚠ Composition caution (PubMed 39331562, N=201): combining mitigation techniques does NOT
reliably stack benefits and can regress. Every SEADS candidate must ship independently
toggleable so Chad can fly each alone and in combination.

## Sources

(Inline throughout. Key studies: Flanagan et al. PubMed 22097636; PLOS ONE 10.1371/journal.pone.0175305; Frontiers VR 10.3389/frvir.2025.1518735; PubMed 39331562; PMC8323020; PMC11522211; CHI 2026 ACM 10.1145/3772318.3791522; PMC9388953; FAA spatial disorientation brochure; DCS/IL-2/War Thunder community sources.)
