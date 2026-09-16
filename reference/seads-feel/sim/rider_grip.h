#pragma once
// ★★★ R4a — THE GRIP, KERNEL-SIDE. LADDER §7.7, STAGE 1.
//
// WHY THIS FILE EXISTS, AND WHY IT IS IN `sim/`.
//
// §7.7 puts `grip_load` / `grip_capacity` / `rider_attached` in the KERNEL,
// because when he lets go the 87.5 kg leaves the machine and `cg_off` and
// `patch_geometry` both change — a sled tumbling riderless is physically a
// different machine, and that is not a render fact. But the equation Chad's
// 10 % was calibrated against is
//
//     grip_load = landing hardness × EXTENSION FROM THE POSE AT TOUCHDOWN
//
// and the extension in that calibration is the RENDER chain's displacement.
// `sim/` may not import `render/` (SPEC §5, hard rule), and a taped kernel
// field driven by a render solve could never replay — the probe replays this
// kernel with no render in the process at all.
//
// ★ CHAD RULED IT, 2026-08-30: **the kernel grows its own extension.** The
// release reads a kernel-visible unseating; render keeps the chain that DRAWS
// it. The direction of authority stays right — the machine is never moved by
// an animation.
//
// ★★★ AND THE LAW IS SHARED, NOT COPIED. `buck_step` below is the arming
// memory Chad signed on 2026-08-30, MOVED here verbatim from
// `render/body_drive.h` and re-exported there under its old names, so there is
// exactly ONE implementation feeding the drawn body, the kernel and the probe.
// A transcribed second copy is how a rig stops describing the code it
// calibrates, and this ladder has paid for that class three times.
//
// ⚠ NOTHING IN THE KERNEL READS WHAT THIS PRODUCES — YET. That is stage 1 of
// §7.7's shipping rule and it is deliberate: the state is live and gradable,
// the capacity is set so it can never break, and the goldens are therefore
// bit-identical BY CONSTRUCTION rather than by a promise. Only after that is
// the capacity dialled to a real number, as a separate, re-taped change that
// Chad drives.

namespace sim {

// ---------------------------------------------------------------------------
// THE ARMING MEMORY — moved, not rewritten. Chad's ruling 2026-08-29:
//
//   "build the arming memory, the buck has to be remembered[,] harder the buck
//    the longer the superman ... only big bucks sent him supermanning long
//    enough and landings hard enough to lose grip. I want it to happen maybe
//    10% of jumps given my normal driving"
//
// One scalar the buck charges and time discharges, dividing the pull-back:
//
//     charge = buck_gain * max(0, |g_eff| - g0)     <- the EXCESS over rest
//     mem    = max(mem - decay * dt, charge)         <- peak-hold, linear decay
//     stiff  = (base / arm) / (1 + mem)
//
// ★ IT IS FLOAT ON PURPOSE. The shipped, signed, driven behaviour was measured
// through these exact float operations; widening them to the kernel's double
// would move what Chad signed for no reason anybody could see. The kernel
// converts at the boundary and keeps this arithmetic byte for byte.
//
// ★ WHY LINEAR DECAY AND NOT EXPONENTIAL: his words are "harder the buck the
// LONGER the superman". Under a linear discharge the time spent above any given
// softening is DIRECTLY PROPORTIONAL to the buck that charged it; under an
// exponential it would go as the LOG of it.
//
// ★ WHY THE CHARGE IS REFERENCED TO g0 AND THAT IS NOT A SNEAKED-IN THRESHOLD:
// |g_eff| is ~1 g in steady riding by construction, so the BUCK is the excess
// over the rest state — a physical datum, not a cutoff someone picked.
struct BuckMemory {
    float mem = 0.0f;  // the charge, dimensionless; 0 = fully recovered
};

struct BuckParams {
    // Softening bought per m/s^2 of buck above rest. 0 DISABLES the whole
    // mechanism bit-identically. SHIPPED AT 0.02 (render/sled_model.cpp).
    float buck_gain = 0.0f;
    // How fast he claws his way back, in mem-units per second. SHIPPED AT 3.0,
    // and ⚠ THE PAIR IS SIGNED, NOT THE GAIN: 3.0 is set by the post-landing
    // recovery bound (p90 1.03 s over his drives), not by taste. The
    // best-separation arm 0.02/1.0 leaves him limp for 20 s after the worst
    // landing in the corpus.
    float decay_per_s = 1.0f;
    // Rest-state field magnitude the buck is measured ABOVE. Physical.
    //
    // ⚠ AND IT IS DELIBERATELY *NOT* MOVED TO 9.80665, THOUGH THE KERNEL'S
    // GRAVITY IS (red-team question, 2026-08-31). Two facts settle it. (i) This
    // struct is the memory the DRAWN body runs, and render's own field is
    // 9.81 (render/trail_chain.h) -- 9.81 is the number this was measured and
    // SIGNED against, so moving it would move the shipped feel with nothing on
    // screen to explain why. (ii) On the kernel side it leaves a dead zone of
    // 9.81 - 9.80665 = 0.00335 m/s^2, i.e. 0.03 % of one g, against bucks that
    // run to hundreds -- and at rest BOTH values charge exactly nothing, which
    // is the property that matters. The calibration was re-run THROUGH the
    // corrected gravity path (kernel p90 0.8177 vs chain 0.8222, ratio 0.995),
    // so `unseat_gain` describes the code as it now stands, not as it stood.
    float g0 = 9.81f;
};

// Advance the memory one step and return the stiffness divisor (>= 1).
float buck_step(BuckMemory& m, const BuckParams& p, float g_eff_mag,
                float dt_s);

// ---------------------------------------------------------------------------
// ★★★ THE KERNEL'S OWN EXTENSION, AND WHAT IT IS NOT.
//
// It is NOT a second body chain. It is ONE scalar standing for "how far the
// buck has thrown him off his pose", built from the same two facts the render
// chain is built from and pulled back by the same softened spring:
//
//     charge  = unseat_gain * max(0, |g_eff| - g0)      the bump throws him out
//     return  = pose_hz / (1 + mem)                     the SAME softened spring
//     ext'    = ext + (charge - ext * return) * dt
//
// ⚠ IT IS A MODEL OF THE CHAIN, NOT THE CHAIN, and it must be graded against
// the chain rather than believed. `tools/sled_probe.cpp superman` prints both
// this and the render chain's own displacement per airborne window for exactly
// that reason; `unseat_gain` is CALIBRATED so the two agree in scale over his
// own drives, and is not a number anybody picked.
//
// ★ EVERY PIECE IS CONTINUOUS. "not every time / if the bump is hard enough" is
// a MAGNITUDE DEPENDENCE; this ladder's recorded disease, three rungs over, is
// a BINARY READ OF A CONTINUOUS QUANTITY. There is no threshold here and none
// is to be added — the rarity emerges from the distribution of his real bucks.
struct GripParams {
    // Metres of extension bought per m/s^2 of buck above rest, per second.
    // 0 makes the extension identically 0 and the load with it.
    //
    // ★★★ 0.0669 IS MEASURED, NOT PICKED. This scalar is a MODEL of the drawn
    // chain, so it is calibrated against the chain, over Chad's own driving:
    // 45 distinct drives, 84 airborne windows, both quantities read at the SAME
    // touchdown by `tools/sled_probe.cpp superman` (SEADS_GRIP_GAIN sweeps it).
    //
    //   gain     kernel ext p90   chain d_land p90   ratio   rank corr
    //   0.001        0.0123           0.8222         0.015    +0.948
    //   0.003        0.0369           0.8222         0.045    +0.948
    //   0.01         0.1230           0.8222         0.150    +0.958
    //   0.03         0.3689           0.8222         0.449    +0.966
    //   0.0669       0.8226           0.8222         1.000    +0.965
    //
    // ⚠ THE SWEEP'S FOUR RANK CORRELATIONS ARE NOT FOUR MEASUREMENTS. The map
    // is exactly homogeneous in this dial, and a positive scalar multiple
    // preserves rank order identically -- so the correlation is the SAME
    // statistic at every gain, and the spread across the column is CSV print
    // quantization (ties at 4 decimals), not evidence. A red-team caught the
    // table being read as corroboration of its own linearity. What the sweep
    // honestly shows is one number, measured once: the shape agrees at ~+0.96,
    // and the scale is then set by a single division. Re-measured at SUBSTEP
    // resolution the scale holds: kernel p90 0.8177 vs chain 0.8222, ratio
    // 0.995.
    //
    // ★ THE SHAPE WAS ALREADY RIGHT AND ONLY THE SCALE WAS FREE: the rank
    // correlation against the chain is ~+0.95 at EVERY gain, and extension is
    // exactly linear in this dial (the ODE's source is proportional to it and
    // its return term is not), so one arm determines the rest and the sweep
    // measures that rather than assuming it. The gain that makes the two p90s
    // agree is 0.0669, and at it they agree to 0.8226 vs 0.8222.
    //
    // ⚠ A HIGH RANK CORRELATION IS NOT THE CHAIN. This is one scalar standing
    // in for a five-station solve; it tracks the chain's ORDER of excursions
    // faithfully and says nothing about its shape. Anything that needs the
    // shape must read the chain, which is render's job and stays render's job.
    double unseat_gain = 0.0669;
    // The pull-back, in Hz, BEFORE the memory softens it. This is the same
    // 1.0 Hz base the drawn body's spring runs at (render/sled_model.cpp), and
    // it is the same number on purpose: one man, one pull-back.
    double pose_hz = 1.0;
    // ★ THE CAPACITY — "miner's hands", and THE dial (§7.7). It ships at a
    // value that CAN NEVER BE MET, which is stage 1 of the shipping rule: the
    // release exists, is measured, and does not fire, so every golden is
    // bit-identical. The real number is dialled in later, as a separate
    // re-taped change Chad drives.
    //
    // ⚠⚠ AND 57.4 IS NOT THAT NUMBER — NOT YET. 57.4 is the p90 of the PROBE's
    // windowed product: the peak field in the 0.30 s after touchdown times the
    // chain's extension AT touchdown. `load` below is the INSTANTANEOUS
    // product, and the extension decays during those 0.30 s, so the two are
    // different statistics of the same idea and cannot share a percentile by
    // assumption. Re-read the p90 off THIS quantity over his own drives before
    // any capacity ships; "his 10 % holds by construction" is only true of the
    // distribution it was actually measured on.
    //
    // ★ AND IT HAS NOW BEEN READ, AT THE RESOLUTION THE LATCH FIRES AT: at the
    // calibrated `unseat_gain`, the p90 of THIS load over the 84 airborne
    // windows of his 45 distinct drives is **78.34** (p50 4.13, p99 427,
    // max 901), against the probe's 57.4 for its windowed chain product.
    // **78.34 is the number stage 2 dials in**, and only after Chad drives it.
    //
    // ⚠ THE FIRST NUMBER PUBLISHED HERE WAS 75.72 AND IT WAS READ OFF A
    // SUBSAMPLE. The probe took the kernel's load from the per-TICK state the
    // replay hands back, while the kernel steps this every SUBSTEP -- 1 sample
    // in 12, of a quantity whose entire meaning is its peaks, against the
    // probe's own written warning not to read a threshold there. The kernel now
    // writes the load into `SledDebugSubstep` and the probe reads THAT. The
    // correction is small (75.72 -> 78.34, +3.5 %) and the method was wrong
    // either way: a threshold read off a subsample is right by luck.
    // ★★★ STAGE 2, 2026-08-31: DIALLED FROM 1e30 TO THE MEASURED 78.34, SO THE
    // RELEASE CAN NOW FIRE. Stage 1 shipped at a capacity that could never be
    // met and Chad drove it ("I rode this last night and did not fall off") --
    // which is the pass §7.7's shipping rule asks for, and the licence to dial.
    //
    // ★ AND IT MOVES NO EXISTING GOLDEN, STRUCTURALLY, NOT BY PROMISE. Every
    // tape in the corpus was cut before the grip dials entered
    // `SLEDTAPE_PARAMS_D`, so the loader's OFF-by-absence forces
    // `unseat_gain = 0` on all of them -- extension is then identically 0, the
    // load with it, and no capacity of any value can be exceeded. The three
    // goldens in test/golden/sled were checked for a `grip.` line: none has
    // one. A tape cut AFTER this change records 78.34 and replays it, which is
    // the point.
    //
    // ⚠ IT IS A DIAL AND CHAD OWNS IT (§7.9: "Chad rules grip_capacity_n.
    // Nobody self-passes it"). `SEADS_GRIP_CAPACITY=x` sets it live from the
    // app (app/main.cpp) without a rebuild, and the value it sets is what the
    // tape records -- there is no second opinion. A very large value restores
    // stage 1 exactly.
    // ★★★ 70.0 -- CHAD'S RULING, 2026-09-01: "set it to 70". It is HIS number
    // and it is not derived from the ladder below; what the ladder does is make
    // it MEASURABLE instead of asserted.
    //
    // The measurement he ruled against (probe mode `grip_release`, over the 9
    // drives in the corpus that contain real jumps -- 71 jumps):
    //
    //     capacity   % of jumps ending in a release   (filtered trigger)
    //        40                42 %
    //        70                 7 %      <- HIS RULING, and it is MEASURED:
    //                                       5 releases over 71 jumps, of which
    //                                       4 of 5 classify at a LANDING
    //        78.34               6 %      <- what shipped before him
    //       120                  0 %      <- never fires
    //
    // and at 78.34 the two TRIGGERS, which is the comparison that mattered
    // most: the INSTANTANEOUS product he originally drove fired on 68 % of
    // jumps against his stated 10 %, and the 0.1 s filtered one -- the series
    // the capacity is supposed to be compared against, per this repo's own
    // standing law -- fired on 6 %.
    //
    // ★ SO THE FILTER, NOT THIS NUMBER, DID THE WORK. 78.34 was never really
    // the defect: it was being compared against the wrong series. Moving to 70
    // buys a little more than 6 % -- back toward the 10 % of jumps he specified
    // at the start of the rung -- on a mechanism whose CHARACTER is now right
    // (the releases that remain classify at landings, not on ordinary rough
    // ground).
    //
    // ⚠ AND THE CORPUS IS THIN: 71 jumps over 9 drives, two of which carry a
    // third of them, all of them old tapes. The rate above is the best number
    // there is, not a precise one.
    double capacity = 70.0;
    // The house filter, and the same 0.1 s the rest of this rung's instruments
    // run at. 0 disables the filter exactly -- `load_lp` then follows `load`
    // and the behaviour is bit-identical to the pre-filter kernel, which is the
    // A/B arm.
    double load_tau_s = 0.1;
};

// Everything the grip law needs from one substep of the machine.
struct GripStep {
    double g_eff_mag = 0.0;  // |g_body - a_body|, the field pressing on him
    double dt_s = 0.0;
};

// The state the law carries between substeps. It lives in `SledState`, but the
// law is here so it is reachable from a test without a whole machine — the
// legs rung was lost precisely because its policy lived in a TU no test linked.
struct GripState {
    BuckMemory mem;
    double extension_m = 0.0;
    double load = 0.0;  // hardness x extension, units m^2/s^2 (see below)
    // ★★★ THE LOAD THE CAPACITY IS ACTUALLY COMPARED AGAINST, AND SHIPPING THE
    // INSTANTANEOUS ONE WAS A DEVIATION FROM THIS REPO'S OWN STANDING LAW.
    // `tools/sled_probe.cpp` states it outright: "grip_capacity_n is dialled
    // against grip_load_lp, NEVER against grip_load_n" -- because `substeps` is
    // a TAPED parameter and dt already differs 2x between the game and a tape,
    // so a threshold on the instantaneous product makes the release rate a
    // function of the integrator's resolution rather than of the machine.
    //
    // ★ IT IS ALSO THE RIGHT PHYSICS. A grip fails under SUSTAINED overload,
    // not on a one-substep spike: a rut transient carries a huge |g_eff| for a
    // few milliseconds and a man's hands do not notice it, while a landing on
    // an extended body is a real impulse that lasts. Filtering is what tells
    // those apart -- and it is CONTINUOUS, so it adds no threshold anywhere.
    // The one binary comparison downstream is §7.3 stage 4, the single
    // irreversible transition the ladder licenses by name.
    double load_lp = 0.0;
    bool attached = true;
};

// ⚠ `load` IS NOT NEWTONS, AND IT IS NOT NAMED `_n` FOR THAT REASON. It is
// (m/s^2) x m = m^2/s^2 — the product Chad's 10 % was calibrated on, and the
// capacity 57.4 lives in those same units. `tools/sled_probe.cpp probe_grip_hold`
// separately computes a genuine rod-model grip FORCE in real Newtons; the two
// are different quantities and must never share a name. LADDER §7.7 writes
// `grip_load_n`; that name would put two quantities behind one word, which is a
// lying instrument by this repo's own definition, so the field ships as
// `grip_load` and this comment is the record of the deviation.
void grip_step(GripState& s, const GripParams& gp, const BuckParams& bp,
               const GripStep& in);

}  // namespace sim
