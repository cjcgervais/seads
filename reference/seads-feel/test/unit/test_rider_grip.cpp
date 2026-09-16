// ★★★ R4a §7.7 STAGE 1 — THE GRIP LAW'S OWN GATE.
//
// The rung above this one was lost because its policy lived in a translation
// unit no test could link, and six green cases graded a fixture instead of the
// mechanism. So the law lives in `sim/rider_grip.{h,cpp}`, pure and free of the
// machine, and these legs execute it directly. The last two run the WHOLE
// kernel, because the claim that matters most about stage 1 — that the grip
// cannot move a golden — is a claim about the machine, not about the law.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "sim/rider_grip.h"
// For SLEDTAPE_PIN_D: the roster that DEFINES what a golden compares.
#include "test/harness/sled_tape.h"
#include "sim/sled.h"
#include "world/heightfield.h"
#include "world/snowpack.h"

namespace {

// The dials as they will be measured and driven: the arming memory Chad signed
// on 2026-08-30, and an extension gain in the band the probe calibrates.
sim::BuckParams shipped_buck() {
    sim::BuckParams b;
    b.buck_gain = 0.02f;
    b.decay_per_s = 3.0f;
    return b;
}

sim::GripParams live_grip() {
    sim::GripParams g;
    g.unseat_gain = 0.02;
    return g;
}

// Hold a field for `secs` at 1/1440 s, the kernel's own substep.
void hold(sim::GripState& s, const sim::GripParams& gp,
          const sim::BuckParams& bp, double g_eff, double secs) {
    const double h = 1.0 / 1440.0;
    sim::GripStep in;
    in.g_eff_mag = g_eff;
    in.dt_s = h;
    for (int i = 0; i < static_cast<int>(secs / h); ++i)
        sim::grip_step(s, gp, bp, in);
}

}  // namespace

TEST_CASE("grip_off_is_structurally_zero_not_merely_small", "[r4a][grip]") {
    // §7.7's shipping rule starts here: at unseat_gain 0 the extension is not
    // "negligible", it is identically 0 for every input, because the charge is
    // 0 and the return term is a decay on a state that starts at 0.
    sim::GripState s;
    sim::GripParams gp;
    // ★ SET IT, DO NOT INHERIT IT. The shipped default is now the CALIBRATED
    // 0.0669 -- the mechanism ships LIVE and unable to fire, which is what
    // stage 1 means -- so a leg about the OFF arm must say OFF out loud. This
    // caught itself: both of this file's off-arm legs went red the moment the
    // default moved, which is the gate doing its job on a default change.
    gp.unseat_gain = 0.0;
    const sim::BuckParams bp = shipped_buck();
    hold(s, gp, bp, 300.0, 0.5);  // a landing far past anything in the corpus
    hold(s, gp, bp, 0.0, 0.5);    // and free fall after it
    REQUIRE(s.extension_m == 0.0);
    REQUIRE(s.load == 0.0);
    REQUIRE(s.attached);
}

TEST_CASE("grip_the_buck_throws_him_out_and_the_spring_brings_him_back",
          "[r4a][grip]") {
    sim::GripState s;
    const sim::GripParams gp = live_grip();
    const sim::BuckParams bp = shipped_buck();
    hold(s, gp, bp, 120.0, 0.10);  // the hit
    const double thrown = s.extension_m;
    REQUIRE(thrown > 0.0);
    hold(s, gp, bp, 9.81, 8.0);  // riding again, nothing pushing him out
    REQUIRE(s.extension_m < 0.05 * thrown);
}

TEST_CASE("grip_a_harder_buck_keeps_him_out_longer", "[r4a][grip]") {
    // Chad's ruling, and the reason the memory exists at all: "harder the buck
    // the longer the superman". Measured as the time to fall back under a FIXED
    // fraction of the peak, so it is a statement about the TIME CONSTANT and
    // not merely about the amplitude a bigger hit obviously buys.
    const sim::GripParams gp = live_grip();
    const sim::BuckParams bp = shipped_buck();
    const double h = 1.0 / 1440.0;
    double prev = -1.0;
    for (const double buck : {40.0, 120.0, 400.0}) {
        sim::GripState s;
        hold(s, gp, bp, buck, 0.10);
        const double peak = s.extension_m;
        sim::GripStep in;
        in.g_eff_mag = 9.81;
        in.dt_s = h;
        int n = 0;
        while (s.extension_m > 0.25 * peak && n < 1440 * 60) {
            sim::grip_step(s, gp, bp, in);
            ++n;
        }
        const double secs = static_cast<double>(n) * h;
        REQUIRE(secs > prev);
        prev = secs;
    }
}

TEST_CASE("grip_load_is_a_product_so_a_seated_man_keeps_hold", "[r4a][grip]") {
    // "A SUM would let a brutal landing throw a seated man, and a seated man is
    // not who he described." With no extension there is no load, however hard
    // the hit — the first sample of the hit is taken before the charge has had
    // a step to move him, and that is the seated case exactly.
    // WARNING: AN EARLIER DRAFT PASSED dt = 0 HERE, which skips the extension
    // block entirely -- a zero-time step, not a seated rider. It killed the sum
    // mutation and still described a setup the code never built. Time passes
    // now, and the seated case is what it physically is: he has not been
    // unseated YET, so a brutal field finds no extension to multiply.
    const sim::GripParams gp = live_grip();
    const sim::BuckParams bp = shipped_buck();
    const double h = 1.0 / 1440.0;

    sim::GripState seated;
    sim::GripStep hit;
    hit.g_eff_mag = 500.0;
    hit.dt_s = h;
    sim::grip_step(seated, gp, bp, hit);
    REQUIRE(seated.attached);

    // The SAME brutal field, on a man the buck has already thrown out of the
    // seat. The only thing that differs between the two is the extension, and
    // the load differs by orders of magnitude -- which IS the product.
    sim::GripState stretched;
    hold(stretched, gp, bp, 200.0, 0.25);
    const double ext_before = stretched.extension_m;
    sim::grip_step(stretched, gp, bp, hit);
    REQUIRE(ext_before > 0.0);
    REQUIRE(stretched.load > 100.0 * seated.load);
}

TEST_CASE("grip_the_capacity_is_a_real_number_now_and_it_separates",
          "[r4a][grip]") {
    // ★★★ STAGE 2 REPLACED STAGE 1'S LEG HERE, AND THE REPLACEMENT IS THE
    // POINT. This case used to be called `grip_ships_at_a_capacity_that_cannot
    // _break` and it asserted that the most violent thing in his corpus left
    // him holding on -- which was the whole of §7.7's stage 1 and is now false
    // BY DESIGN. Chad drove stage 1 and did not fall off; that was the pass and
    // the licence to dial the capacity to the measured 78.34.
    //
    // What must still be true, and is what this leg now pins, is the SEPARATION
    // the capacity was chosen for: an ordinary landing keeps him on, a big one
    // does not, and the difference is the PRODUCT (§7.7 -- "a hard landing
    // while he is SEATED goes through his legs and the seat").
    const sim::GripParams gp = live_grip();  // capacity = the shipped default
    const sim::BuckParams bp = shipped_buck();

    // AN ORDINARY LANDING: he was not bucked first, so there is barely any
    // extension for the field to multiply. This is the p50 end of his own
    // measured corpus (4.13), not a number invented for the assert.
    sim::GripState ordinary;
    hold(ordinary, gp, bp, 60.0, 0.02);
    REQUIRE(ordinary.load > 0.0);          // the mechanism is LIVE...
    REQUIRE(ordinary.load < gp.capacity);  // ...and it did not fire
    REQUIRE(ordinary.attached);

    // THE BIG ONE: stretched out, then landed on. Same law, same dials.
    sim::GripState big;
    hold(big, gp, bp, 400.0, 0.30);
    REQUIRE_FALSE(big.attached);

    // ★ AND STAGE 1 IS EXACTLY ONE DIAL AWAY, which is the A/B and the kill
    // switch Chad drives with (SEADS_GRIP_CAPACITY). Same violent hold, a hand
    // nothing can beat: he holds on, and the load is real rather than absent.
    sim::GripState armoured;
    sim::GripParams huge = gp;
    huge.capacity = 1.0e30;
    hold(armoured, huge, bp, 400.0, 0.30);
    REQUIRE(armoured.load > 0.0);
    REQUIRE(armoured.attached);
}

TEST_CASE("grip_release_is_one_way", "[r4a][grip]") {
    // §7.3: stages 0-3 are continuous and reversible; stage 4 is not. Getting
    // back on is R4e (the remount), never a threshold falling back under
    // itself.
    sim::GripState s;
    sim::GripParams gp = live_grip();
    gp.capacity = 1.0;  // a hand nobody has, so the leg can see the transition
    const sim::BuckParams bp = shipped_buck();
    hold(s, gp, bp, 200.0, 0.20);
    REQUIRE_FALSE(s.attached);
    hold(s, gp, bp, 0.0, 5.0);  // the load goes away
    REQUIRE(s.load < 1.0);
    REQUIRE_FALSE(s.attached);  // he does not
}

TEST_CASE("grip_the_memory_discharges_before_it_peak_holds", "[r4a][grip]") {
    // *** THE ORDER THE LAW ARGUES FOR AT LENGTH, AND WHICH NOTHING PINNED.
    // sim/rider_grip.cpp states it: discharge FIRST, then peak-hold against
    // this step's buck, because charging first and decaying after shaves one
    // step off the very peak the mechanism exists to remember. A red-team
    // mutation that swapped the two SURVIVED the whole gate -- five buck legs
    // in test_body_drive.cpp and not one of them the order.
    //
    // The signature is exact: after ONE step from rest the memory holds the
    // FULL charge, buck_gain * (|g_eff| - g0), with nothing subtracted. Swap
    // the order and it is short by decay_per_s * dt.
    sim::BuckMemory m;
    const sim::BuckParams bp = shipped_buck();
    const float g = 1000.0f, dt = 0.1f;
    const float div = sim::buck_step(m, bp, g, dt);
    const float full = bp.buck_gain * (g - bp.g0);
    REQUIRE(m.mem == full);
    REQUIRE(div == 1.0f + full);
}

TEST_CASE("grip_extension_never_goes_negative", "[r4a][grip]") {
    // The clamp at the end of the extension update is stated as load-bearing
    // and a red-team deleted it without turning anything red. It is only
    // reachable at a dt big enough for the explicit step to overshoot, so the
    // leg uses one. A body cannot be pulled back THROUGH its own pose.
    sim::GripState s;
    sim::GripParams gp = live_grip();
    gp.pose_hz = 1.0;
    const sim::BuckParams bp = shipped_buck();
    hold(s, gp, bp, 200.0, 0.20);
    REQUIRE(s.extension_m > 0.0);
    sim::GripStep quiet;
    quiet.g_eff_mag = 9.81;
    quiet.dt_s = 2.0;  // ret * dt = 2: the explicit step overshoots the pose
    sim::grip_step(s, gp, bp, quiet);
    REQUIRE(s.extension_m >= 0.0);
    REQUIRE(s.load >= 0.0);
}

// ---------------------------------------------------------------------------
// THE LEGS THAT ARE ABOUT THE MACHINE, NOT THE LAW. There were two at stage 1
// and there are three now: the corpus-safety claim, its stage-2 inverse, and
// determinism.

namespace {

// A featureless 15 km ball: `relief_scale` 0 means radius_at is exactly R
// everywhere, so the two machines below are driven over identical ground and
// any difference between them is the grip and nothing else.
world::HeightField flat_hf() {
    world::HeightField hf;
    hf.px.assign(4, 0);
    hf.w = 2;
    hf.h = 2;
    hf.R = 15000.0;
    hf.relief_scale = 0.0;
    return hf;
}

world::SnowpackField flat_snow(const world::HeightField& hf) {
    world::SnowpackField f;
    f.hf = &hf;
    f.p.base_m = 0.77;
    f.p.curv_gain = 0.0;
    f.p.drain_gain = 0.0;
    f.p.aspect_lee = 0.0;
    f.p.elev_gain_per_km = 0.0;
    f.p.slope_shed = 0.0;
    f.p.depth_max_m = 5.0;
    return f;
}

sim::SledState spawn(const sim::SledParams& p, const world::SnowpackField& f,
                     double speed_ms) {
    sim::SledState s;
    const glm::dvec3 dir(0.0, 0.0, 1.0);
    s.position = dir * (f.drive_radius_at(dir) + p.cg_height_m + 0.05);
    const glm::dvec3 up = dir, fwd(1.0, 0.0, 0.0);
    glm::dmat3 m;
    m[0] = glm::cross(fwd, up);
    m[1] = up;
    m[2] = -fwd;
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = fwd * speed_ms;
    return s;
}

// ★★★ THE WHOLE PIN ROSTER, FROM THE ROSTER ITSELF -- never a hand-copied
// subset. The first draft of this listed TEN fields and called them "every
// dynamic field"; a red-team mutation that made the kernel zero `assist_nm`,
// `air_s`, `roost_flux` and `depth_under_m` on release therefore SURVIVED, and
// those are exactly the fields a release touches first. A hand-written list is
// a snapshot of what its author remembered; `SLEDTAPE_PIN_D` is the definition
// of what a golden compares, so the leg now expands THAT and cannot fall behind
// it -- add a pinned field and this leg covers it the same day.
//
// It is also strictly stronger than a golden: bit equality on all 41 doubles
// plus the two discrete pins, not a tolerance.
bool dynamics_identical(const sim::SledState& a, const sim::SledState& b) {
    bool same = a.surface == b.surface && a.rolled == b.rolled;
#define SLEDGRIP_CMP(f) same = same && (a.f == b.f);
    SLEDTAPE_PIN_D(SLEDGRIP_CMP)
#undef SLEDGRIP_CMP
    return same;
}

}  // namespace

TEST_CASE("sled_the_tape_preset_still_moves_no_golden", "[r4a][grip][sled]") {
    // ★★★ THIS LEG IS THE STAGE-1 TRIPWIRE, TURNED THE RIGHT WAY UP FOR STAGE 2.
    //
    // Its ancestor (`sled_the_grip_law_moves_no_golden`) proved that a LIVE
    // grip moved nothing, because nothing in the kernel read it. Stage 2 wires
    // the release in, so that claim is now false on purpose and its own comment
    // said so: "this is the leg that would go red the moment somebody wires the
    // release into the machine". Deleting it would have thrown away the thing
    // it was really protecting, which is THE CORPUS -- so it is re-aimed at the
    // claim that still has to hold and now carries all the weight:
    //
    //   EVERY EXISTING TAPE REPLAYS BIT-IDENTICALLY, WHATEVER THE CAPACITY IS.
    //
    // That is structural, not lucky. `sled_tape.h` forces `grip.unseat_gain` to
    // 0 on any tape that does not record it, and every tape in the corpus was
    // cut before the dials entered `SLEDTAPE_PARAMS_D` -- the three goldens in
    // test/golden/sled contain no `grip.` line. At gain 0 the extension is
    // identically 0, the load with it, and NO capacity of any value can be
    // exceeded. This leg drives that preset against a capacity of ZERO, the
    // most hostile value there is.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = flat_snow(hf);

    sim::SledParams off;
    off.grip.unseat_gain = 0.0;  // the TAPE PRESET, stated rather than inherited
    sim::SledParams hostile = off;
    hostile.grip.capacity = 0.0;  // a hand made of paper -- and still it holds
    hostile.grip_buck = shipped_buck();

    sim::SledState a = spawn(off, f, 8.0);
    sim::SledState b = spawn(hostile, f, 8.0);
    sim::SledInputs in;
    in.throttle = 0.8f;
    in.steer = 0.3f;
    for (int i = 0; i < 1200; ++i) {
        a = sim::step_sled(a, in, off, f, 1.0 / 60.0);
        b = sim::step_sled(b, in, hostile, f, 1.0 / 60.0);
    }
    REQUIRE(dynamics_identical(a, b));
    REQUIRE(a.grip.attached);
    REQUIRE(b.grip.attached);  // the capacity never got a load to beat
    REQUIRE(a.grip.extension_m == 0.0);
    REQUIRE(b.grip.extension_m == 0.0);
}

TEST_CASE("sled_the_release_takes_the_rider_off_the_machine",
          "[r4a][grip][sled]") {
    // ★★★ STAGE 2's OWN CLAIM, AND IT IS THE EXACT INVERSE OF THE LEG ABOVE.
    // §7.7: "when he releases, `rider_mass_kg` (87.5) leaves the machine:
    // `cg_off` and `patch_geometry` both change, and a sled tumbling riderless
    // is physically a different machine." §7.4: the hands go, so the throttle
    // goes with them.
    //
    // ⚠⚠ AND THE ARMS HAVE TO DIFFER BEFORE ANY OF THIS MEANS ANYTHING. This
    // thread's own recorded trap is an A/B whose two arms were secretly the
    // same run; so the first assert is that the live arm ACTUALLY RELEASED and
    // the control arm did not, and only then that the machines diverged.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = flat_snow(hf);

    sim::SledParams held;
    held.grip.unseat_gain = 1.0;
    held.grip.capacity = 1.0e30;  // stage 1: he cannot be thrown
    held.grip_buck = shipped_buck();
    sim::SledParams let_go = held;
    let_go.grip.capacity = 0.0;  // stage 2, dialled past any real landing

    sim::SledState a = spawn(held, f, 8.0);
    sim::SledState b = spawn(let_go, f, 8.0);
    sim::SledInputs in;
    in.throttle = 0.8f;
    in.steer = 0.3f;
    in.lean_lat = 1.0f;  // leaned right over, so `cg_off` has something to lose
    for (int i = 0; i < 1200; ++i) {
        a = sim::step_sled(a, in, held, f, 1.0 / 60.0);
        b = sim::step_sled(b, in, let_go, f, 1.0 / 60.0);
    }
    // 1. THE ARMS DIFFERED.
    REQUIRE(a.grip.attached);
    REQUIRE_FALSE(b.grip.attached);
    // 2. AND THE MACHINE FELT IT -- the whole of stage 2 in one line.
    REQUIRE_FALSE(dynamics_identical(a, b));
    // 3. NAMED, NOT JUST "DIFFERENT". A difference with no mechanism behind it
    //    is a leg that would stay green through the wrong fix.
    //    (a) the hands are off the bars, so the throttle is shut and the bars
    //        have fallen back to centre (§7.4).
    REQUIRE(std::abs(b.steer_actual) < 1.0e-9);
    REQUIRE(std::abs(a.steer_actual) > 0.01);
    //    (b) he is not on it to lean it: his stored displacement has slewed
    //        home, while the man still holding on is still leaned over.
    REQUIRE(std::abs(b.rider_lat_m) < 1.0e-3);
    REQUIRE(std::abs(a.rider_lat_m) > 0.01);
    //    (c) and with no drive at the track, the riderless machine is the
    //        slower of the two -- the coast-down §7.4 describes.
    REQUIRE(glm::length(b.velocity) < glm::length(a.velocity));
}

TEST_CASE("sled_the_87_kilos_leave_and_that_is_a_separate_fact",
          "[r4a][grip][sled]") {
    // ★★★ §7.7's HEADLINE CLAIM, ISOLATED -- and it needed isolating, because
    // in an ordinary drive it is INVISIBLE BEHIND ITS NEIGHBOURS. Once the
    // hands are off, the lean COMMANDS are zeroed too, so his stored
    // displacement decays to nothing on its own and a whole-drive A/B would go
    // red even with `rider_frac` left ungated. A leg that cannot fail for the
    // reason it names is this ladder's most expensive recurring defect.
    //
    // So both arms here are handed the SAME already-leaned man, the same zero
    // inputs, and the same everything else. They differ in ONE bit: whether he
    // is still on the machine. The lean then decays IDENTICALLY in both (same
    // command, same target, same tau) -- what cannot be the same is whether
    // that displacement still moves the contact patches.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = flat_snow(hf);
    sim::SledParams p;  // the grip law itself is irrelevant here and stays off
    const sim::SledInputs quiet;  // no throttle, no brake, no bars, no lean

    sim::SledState held = spawn(p, f, 8.0);
    held.rider_lat_m = 0.3;  // leaned right out over the running board
    sim::SledState gone = held;
    gone.grip.attached = false;  // ...and the ONE bit that differs
    for (int i = 0; i < 60; ++i) {
        held = sim::step_sled(held, quiet, p, f, 1.0 / 60.0);
        gone = sim::step_sled(gone, quiet, p, f, 1.0 / 60.0);
    }
    // The lean really did stay common -- if it had not, the divergence below
    // would be about the decay and not about the mass.
    REQUIRE(held.rider_lat_m == gone.rider_lat_m);
    // ...and the machines are NOT the same machine. `cg_off` is
    // `rider_frac * (-lat, up, -fwd)` and `rider_frac` is 0 for a man who is
    // not there, so his 0.3 m of lean stops shifting the patch mounts the
    // instant he lets go.
    REQUIRE_FALSE(dynamics_identical(held, gone));

    // ★ AND THE CONTROL, WHICH IS WHAT MAKES THE LINE ABOVE MEAN ANYTHING:
    // the SAME two arms with nothing leaned. There is then no displacement for
    // the mass fraction to scale, so the two must agree BIT FOR BIT -- and any
    // OTHER difference the release might have introduced would show up right
    // here instead of hiding inside the assert above.
    sim::SledState held0 = spawn(p, f, 8.0);
    sim::SledState gone0 = held0;
    gone0.grip.attached = false;
    for (int i = 0; i < 60; ++i) {
        held0 = sim::step_sled(held0, quiet, p, f, 1.0 / 60.0);
        gone0 = sim::step_sled(gone0, quiet, p, f, 1.0 / 60.0);
    }
    REQUIRE(dynamics_identical(held0, gone0));
}

TEST_CASE("sled_the_grip_state_is_deterministic_in_the_inputs",
          "[r4a][grip][sled]") {
    // WARNING: THIS LEG PROVES DETERMINISM, AND ITS FIRST NAME CLAIMED MORE.
    // It was called `..._replays_from_taped_inputs_alone` and was offered as
    // the licence for keeping the grip out of the positional pin roster -- but
    // running one pure function twice from the same state with the same inputs
    // is a test that cannot fail, and would stay green for arbitrarily
    // path-dependent state. A red-team named it; it is renamed.
    //
    // *** THE HONEST POSITION, WHICH IS NOT "IT RE-CONVERGES":
    // `grip.attached` is a ONE-WAY LATCH and structurally CANNOT re-converge
    // from inputs. Keeping it out of the roster is safe because a tape OVERRIDE
    // assigns the whole struct from a pin that does not carry the grip, so the
    // replay RESETS it -- which means the RECORDING side must reset it too.
    // That is why app/main.cpp KEY_R autoright now zeroes `sled.grip` beside
    // `ws_exch_l`, and why this comment names that site: the property lives in
    // a TU no test can link, so it is pinned by a comment at both ends rather
    // than pretended into a leg that cannot see it.
    //
    // What IS provable here, and what this leg pins, is that nothing in the
    // grip path reads a clock, a global, or uninitialised memory.
    const world::HeightField hf = flat_hf();
    const world::SnowpackField f = flat_snow(hf);
    sim::SledParams p;
    p.grip.unseat_gain = 0.02;
    p.grip_buck = shipped_buck();

    sim::SledInputs in;
    in.throttle = 0.7f;
    in.steer = -0.4f;
    sim::SledState a = spawn(p, f, 6.0), b = spawn(p, f, 6.0);
    for (int i = 0; i < 600; ++i) {
        a = sim::step_sled(a, in, p, f, 1.0 / 60.0);
        b = sim::step_sled(b, in, p, f, 1.0 / 60.0);
    }
    REQUIRE(a.grip.extension_m == b.grip.extension_m);
    REQUIRE(a.grip.load == b.grip.load);
    REQUIRE(a.grip.mem.mem == b.grip.mem.mem);
}
