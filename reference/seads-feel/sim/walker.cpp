#include "sim/walker.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace sim {

namespace {

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// The house slew, same shape as sim/sled.cpp's: one time constant, no clamp,
// no threshold. A body cannot teleport.
double slew(double cur, double target, double tau_s, double dt_s) {
    if (!(tau_s > 0.0)) return target;
    const double k = 1.0 - std::exp(-dt_s / tau_s);
    return cur + (target - cur) * k;
}

// Piecewise-linear over the three anchors, in the `kAftCeilC1` idiom: read the
// row, never lerp end-to-end. The middle anchor is the whole point — a straight
// line from hardpack to deep would say he is fast in a foot of snow, which is
// exactly what his "midly handicapped ... but slowed" refuses.
double row3(const WalkerParams& p, double depth_m, double a, double b,
            double c) {
    if (depth_m <= 0.0) return a;
    if (depth_m >= p.depth_deep_m) return c;
    if (depth_m <= p.depth_mid_m) {
        const double t = p.depth_mid_m > 0.0 ? depth_m / p.depth_mid_m : 1.0;
        return a + (b - a) * t;
    }
    const double span = p.depth_deep_m - p.depth_mid_m;
    const double t = span > 0.0 ? (depth_m - p.depth_mid_m) / span : 1.0;
    return b + (c - b) * t;
}

// The rider's own friction against each surface class. ⚠ NOT the machine's
// `SurfaceDials::mu_kin` -- a body is 10-15x draggier than a ski, and these are
// two different physical contacts, not one number with two consumers.
double mu_of(const WalkerParams& p, world::Surface s) {
    switch (s) {
        case world::Surface::TrailMain:
        case world::Surface::TrailTributary:
            return p.mu_trail;
        case world::Surface::Road:
        case world::Surface::MineWorks:
            return p.mu_road;
        case world::Surface::LakeIce:
            return p.mu_ice;
        case world::Surface::Bush:
        case world::Surface::RockOutcrop:
        default:
            return p.mu_bush;
    }
}

}  // namespace

double walker_depth_frac(const WalkerParams& p, double depth_m) {
    return p.depth_deep_m > 0.0 ? clamp01(depth_m / p.depth_deep_m) : 0.0;
}

double walker_speed_cap(const WalkerParams& p, double depth_m) {
    return row3(p, depth_m, p.speed_hardpack_mps, p.speed_mid_mps,
                p.speed_deep_mps);
}

double walker_stride(const WalkerParams& p, double depth_m) {
    // stride = 2 * step, and step = walk_ratio * cadence with cadence in
    // steps/min. Solving the walk ratio against speed (see the header):
    //     cadence = sqrt(60 * v / WR)   [steps/min]
    //     step    = WR * cadence        [m]
    // which reduces to step = sqrt(WR * 60 * v). Deep snow stretches WR.
    const double v = std::max(0.0, walker_speed_cap(p, depth_m));
    const double wr =
        p.walk_ratio *
        (1.0 + p.walk_ratio_deep_gain * walker_depth_frac(p, depth_m));
    const double step = std::sqrt(wr * 60.0 * v);
    return 2.0 * step;
}

double walker_stance_frac(const WalkerParams& p, double speed_mps) {
    (void)p;
    // Nilsson & Thorstensson's measured row, as a line through its own ends
    // and clamped to them: 0.68 at 0.6 m/s, 0.38 at 3.5 m/s. Continuous, so
    // the walk becomes a run with no threshold and no branch.
    const double t = std::clamp((speed_mps - 0.6) / (3.5 - 0.6), 0.0, 1.0);
    return 0.68 + (0.38 - 0.68) * t;
}

double walker_lift(const WalkerParams& p, double depth_m) {
    return std::min(p.lift_base_m + std::max(0.0, depth_m), p.lift_max_m);
}

double walker_mu(const WalkerParams& p, const world::SnowpackField::GroundSample& g) {
    // ★ BLENDED ACROSS THE CLASS BOUNDARY, not stepped. `sample_at` already
    // publishes what class it is blending TOWARD and how far (SK-1a, added
    // because a STEP in the machine's own per-class dials threw it into 1.3
    // rotations at a corridor edge). A body crossing from trail to bush has
    // exactly the same problem, and the fix already exists -- so it is read,
    // not re-invented.
    const double a = mu_of(p, g.surf);
    const double b = mu_of(p, g.surf_b);
    const double t = std::clamp(g.surf_mix, 0.0, 1.0);
    return a + (b - a) * t;
}

double walker_rise_s(const WalkerParams& p, double depth_m) {
    // §7.5's two numbers, over the same row. The middle is not a third measured
    // anchor -- he did not give one for the get-up -- so it sits on the line
    // between them, and that is stated rather than dressed up as a measurement.
    const double mid = 0.5 * (p.rise_s_hardpack + p.rise_s_deep);
    return row3(p, depth_m, p.rise_s_hardpack, mid, p.rise_s_deep);
}

void walker_foot_offset(double phase, double stride_m, double lift_m,
                        double stance_frac, double* fwd_m, double* up_m) {
    // Wrap first: a caller handing in 1.0 or -0.25 gets the same answer as one
    // handing in 0.0 or 0.75, because a cycle is a circle.
    phase -= std::floor(phase);
    const double ds = std::clamp(stance_frac, 0.05, 0.95);
    // ★★★ THE EXCURSION IS `ds * stride`, NOT `stride`, AND THE OLD VALUE WAS
    // A SKATE. Phase advances by distance/stride, so a foot planted for `ds` of
    // a cycle is planted while the body covers `ds * stride`. The first version
    // swept a WHOLE stride over HALF a cycle -- so the planted foot slid
    // backward through the snow at 1x body speed, 3.5 m/s of backslide on
    // hardpack, for the whole of stance. The header even stated the correct
    // intent ("travels backwards under him at exactly the rate he travels
    // forwards") while the code did twice that.
    //
    // The invariant is exact and is worth stating as one: d(fwd)/d(phase) must
    // equal -stride_m everywhere in stance, or the foot is sliding. It is
    // pinned by a test.
    const double excur = ds * stride_m;
    if (phase < ds) {
        // ★ STANCE. Planted, and a straight line -- any easing here IS a boot
        // sliding on the snow.
        if (fwd_m) *fwd_m = 0.5 * excur - stride_m * phase;
        if (up_m) *up_m = 0.0;  // and it is EXACTLY 0, not nearly
        return;
    }
    // ★★★ SWING, AND IT IS A CUBIC HERMITE, NOT A RAISED COSINE. The old
    // curve was defended as "starts and ends with zero velocity" -- which is
    // the WRONG boundary condition. Stance leaves at d(fwd)/dphase = -stride;
    // a cosine enters swing at 0. Position was continuous and VELOCITY WAS
    // NOT: a hard corner at toe-off and another at heel strike, twice a cycle.
    //
    // Matching the stance slope at both ends instead gives, in normalised
    // swing time, e(t) = -4t^3 + 6t^2 - t. It is C1 by construction, and it
    // hands back two things real gait actually does, for free:
    //   * e dips to -0.044 near t = 0.09 -- THE TOE TRAILS at toe-off.
    //   * e peaks at 1.044 near t = 0.91 and then RETRACTS -- terminal-swing
    //     foot retraction, which is the mechanism that makes a footfall read as
    //     planted rather than stamped.
    const double t = (phase - ds) / (1.0 - ds);
    const double e = -4.0 * t * t * t + 6.0 * t * t - t;
    if (fwd_m) *fwd_m = -0.5 * excur + excur * e;
    if (up_m) *up_m = lift_m * std::sin(3.14159265358979323846 * t);
}

void walker_throw(WalkerState& s, const WalkerParams& p,
                  const glm::dvec3& pos_w, const glm::dvec3& vel_w,
                  const glm::dvec3& lat_axis_w, double lat_lean_m) {
    // ★ ONE-WAY, like the latch that caused it. §7.3 stage 4 is the one
    // irreversible transition in the chain; re-throwing a man already off the
    // machine would teleport him back onto it.
    if (s.mode != WalkerMode::Riding) return;
    s.mode = WalkerMode::Falling;
    s.t_mode_s = 0.0;
    s.pos = pos_w;
    // HE KEEPS WHAT HE HAD. No forward term -- see the header.
    s.vel = vel_w;
    // ★ CHAD'S Q1: "whichever way he's already leaning out." Sign and
    // magnitude are both his lean's; the gain is the only thing added, and at
    // lat_gain_per_s 0 this term is exactly zero.
    const double lat = glm::length(lat_axis_w);
    if (lat > 0.0)
        s.vel += (lat_axis_w / lat) * (p.lat_gain_per_s * lat_lean_m);
    // Face the way he is travelling, tangentially. If he is not travelling,
    // heading is left as it was -- there is no direction in a zero vector and
    // inventing one is how a man ends up facing north on every crash.
    const double r = glm::length(s.pos);
    if (r > 0.0) {
        const glm::dvec3 up = s.pos / r;
        const glm::dvec3 t = s.vel - glm::dot(s.vel, up) * up;
        if (glm::length(t) > 1.0e-9) s.heading = glm::normalize(t);
    }
}

void walker_remount(WalkerState& s) { s = WalkerState{}; }

WalkerState step_walker(const WalkerState& s0, const WalkerInputs& in,
                        const WalkerParams& p, const world::SnowpackField& f,
                        double dt_s) {
    WalkerState s = s0;
    // The one-shot edges are exactly that: cleared every step, set by the step
    // that earns them. A consumer that misses one has a bug; a consumer that
    // sees one twice cannot exist.
    s.poof = false;
    s.landed = false;
    if (s.mode == WalkerMode::Riding || !(dt_s > 0.0)) return s;

    const double r = glm::length(s.pos);
    if (!(r > 0.0)) return s;
    const glm::dvec3 up = s.pos / r;

    // ★ THE ONE GROUND QUERY, and it is the SAME one a contact patch runs
    // (§R5+: "a foot is just another contact patch ... the snow queries, depth
    // law and surface classes all already exist"). One call per step: the
    // radius and the depth come out of it together, and a second call would be
    // the two-lookups-per-sample mistake `world/snowpack.h` forbids by name.
    const world::SnowpackField::GroundSample g = f.sample_at(up);
    s.depth_m = g.depth_m;
    s.speed_cap_mps = walker_speed_cap(p, g.depth_m);
    s.stride_m = walker_stride(p, g.depth_m);

    // ★★★ THE GET-UP CLOCK DOES NOT RUN WHILE HE IS STILL SLIDING, and that
    // is a real ordering, not bookkeeping. The two were concurrent in the first
    // draft, so on a long hardpack skid he began climbing to his feet while
    // still travelling 20 m/s and the slide was cut off mid-way by the stage
    // ending. A man does not get up out of a slide he is still in.
    //
    // ★ AND THE TEST FOR "HE HAS STOPPED" IS EXACT, WHICH IS A GIFT FROM THE
    // COULOMB LAW: it lands on zero at a definite time, so this is an equality
    // and not a threshold on a continuous quantity. Under the exponential it
    // replaced there would have been no such instant, and this would have had
    // to be a cutoff somebody picked.
    const bool sliding =
        (s.mode == WalkerMode::Buried || s.mode == WalkerMode::Down) &&
        glm::length(s.vel) > 0.0;
    if (!sliding) s.t_mode_s += dt_s;

    if (s.mode == WalkerMode::Falling) {
        // Gravity plus the same quadratic drag the drawn body flies at.
        // Semi-implicit Euler: velocity first, then position with the new
        // velocity -- the form every integrator in this repo uses.
        const double v = glm::length(s.vel);
        glm::dvec3 a = -p.gravity_mps2 * up;
        if (v > 0.0) a -= (p.drag_k_per_m * v) * s.vel;
        s.vel += a * dt_s;
        s.pos += s.vel * dt_s;

        // The floor, evaluated AFTER the move so the step that carries him into
        // the snow also puts him on it, and re-evaluated every step because the
        // surface under him is terrain, not a plane captured at the release.
        const double floor_r = g.drive_r + p.lie_clearance_m;
        const double r2 = glm::length(s.pos);
        if (r2 > 0.0 && r2 < floor_r) {
            // ★ HOW FAST HE ARRIVED, taken BEFORE the contact touches the
            // velocity. Read after, it is the aftermath and not the impact --
            // and a burst sized off it would silently be zero.
            s.poof_speed_mps = glm::length(s.vel);
            const glm::dvec3 up2 = s.pos / r2;
            s.pos = up2 * floor_r;
            const double vr = glm::dot(s.vel, up2);
            if (vr < 0.0) s.vel -= vr * up2;
            // ★ HE HITS, AND THE SNOW ANSWERS. §7.5: "landing throws a big poof
            // of snow", and it is the SAME roost/spray path (§0.3) -- this flag
            // asks for it, it does not fork a particle system.
            s.poof = true;
            s.landed = true;
            s.rise_s = walker_rise_s(p, g.depth_m);
            // ★★★ AND IN THE DEEPEST SNOW HE IS GONE. His ruling: "he should
            // dissapear and / reappear in deepest snow in a poof". Whether
            // there is a Buried stage at all is decided by the DEPTH, through
            // the same continuous fraction everything else here reads -- so in
            // hardpack the buried stage is exactly zero seconds long and he is
            // simply down, structurally rather than nearly.
            const double frac = walker_depth_frac(p, g.depth_m);
            s.mode = (p.bury_frac * frac > 0.0) ? WalkerMode::Buried
                                                : WalkerMode::Down;
            s.t_mode_s = 0.0;
            // ⚠⚠ AND HIS VELOCITY IS **NOT** ZEROED HERE. An earlier version of
            // this line read `s.vel = glm::dvec3(0.0)` and it was a REGRESSION
            // introduced by the move out of `render/rider_flight.cpp`: that
            // file bled the tangential velocity off over a distance and had a
            // leg pinning it (`the ground stops him sinking, not sliding`),
            // and neither the behaviour nor the leg came across. Chad drove it
            // and said "he needs to maintain some forward velocity / skid upon
            // falling off" -- which is the defect, felt.
            //
            // A man arriving at 15 m/s does not stop where he hits. Only the
            // INWARD component is gone (killed above): he stops sinking, he
            // does not stop sliding, and the slide is bled below.
        }
        return s;
    }

    // --- he is on the ground: the get-up sequence, on ONE clock -------------
    //
    // ★ THE WHOLE SEQUENCE IS FRACTIONS OF THE RISE THE SNOW ALREADY BOUGHT
    // HIM, so "fifteen seconds in the bush and two on the trail" stretches the
    // burial and the crawl with it and nothing has a second clock to drift on.
    const double frac = walker_depth_frac(p, s.depth_m);
    const double rise = s.rise_s > 0.0 ? s.rise_s : walker_rise_s(p, s.depth_m);

    // ★★★ THE SKID. He arrived with real speed and the snow takes it over a
    // DISTANCE, not instantly -- and it takes it before he can do anything
    // about it, which is why this runs ahead of the get-up sequence and for
    // every grounded mode rather than being a branch of one of them.
    //
    // ★★★ COULOMB, PLUS THE PLOUGH. Two terms, both measured (see the header):
    //
    //   a = mu(surface) * g   +   (0.5 rho Cd A_sub / m) * v^2
    //       \____ dry friction ___/   \______ displaced snow ______/
    //
    // The first ENDS the slide -- constant deceleration, so distance goes as
    // v^2 and a crash twice as fast skids nearly four times as far, which is
    // the thing an exponential could never do. The second is the powder term:
    // quadratic, gated on how deep he is, and STRUCTURALLY ZERO on a plowed
    // road where there is nothing to plough. On a packed trail it only bites
    // above ~14 m/s, which gives the hard first bite that then relaxes into a
    // long glide -- and that shape falls out of the two terms rather than
    // being authored.
    //
    // ⚠⚠ AND IT APPLIES ONLY WHILE HE IS NOT UNDER HIS OWN POWER. An earlier
    // draft ran for EVERY grounded mode, `Afoot` included -- so the snow ate
    // the walk's own velocity every step and a man who should jog 35 m in ten
    // seconds covered 1.6. A skid is what the snow does TO him; from
    // `Crawling` onward the velocity below is what HE does.
    if ((s.mode == WalkerMode::Buried || s.mode == WalkerMode::Down) &&
        glm::length(s.vel) > 0.0) {
        const double sp = glm::length(s.vel);
        // (Both crawls are excluded: from CrawlProne onward the velocity below
        // is what HE does, and the snow must not be charged for it twice.)
        // Immersion drives the plough's frontal area, and it is HIS depth,
        // capped: he does not plough deeper than his own body.
        const double imm = std::min(s.depth_m, p.body_immersion_max_m);
        const double a_sub = p.body_width_m * std::max(0.0, imm);
        const double c = 0.5 * p.plow_rho * p.plow_cd * a_sub;
        const double dec =
            walker_mu(p, g) * p.gravity_mps2 + (c / p.body_mass_kg) * sp * sp;
        // ★ THE KARNOPP CLAMP, and it is the SHIPPED ONE -- `sim/ground.h`
        // stops the aircraft on its gear with exactly this shape. It lands on
        // zero EXACTLY, with no jitter and no rest-speed cutoff: a Coulomb
        // stop has a definite end, which is the whole difference from the
        // exponential it replaces.
        const double dv = dec * dt_s;
        s.vel = (sp > dv) ? s.vel * (1.0 - dv / sp) : glm::dvec3(0.0);
        s.pos += s.vel * dt_s;
        // Stay ON the surface while he slides: a straight tangential step on a
        // 15 km ball lifts him (chord versus arc), so contact is PROJECTED, not
        // merely checked -- the same constraint the landing applies.
        const double rs = glm::length(s.pos);
        if (rs > 0.0) s.pos = (s.pos / rs) * (g.drive_r + p.lie_clearance_m);
    }

    if (s.mode == WalkerMode::Buried) {
        // ★★★ HOW FAR UNDER HE IS. He goes in fast (the impact drives him) and
        // comes out slower (he climbs), so the two ramps are deliberately NOT
        // symmetric: `kIn` of the stage to disappear, `kOut` to surface, and
        // he is fully gone in between. Both are FRACTIONS OF THE STAGE the
        // depth already bought, so nothing here is a second clock.
        const double stage = p.bury_frac * frac * rise;
        if (stage > 0.0) {
            const double u = clamp01(s.t_mode_s / stage);
            const double kIn = 0.18, kOut = 0.45;
            double v;
            if (u < kIn)
                v = u / kIn;
            else if (u > 1.0 - kOut)
                v = (1.0 - u) / kOut;
            else
                v = 1.0;
            // Smoothstep, so he sinks and surfaces without a corner at either
            // end -- a linear ramp reads as a lift, not a body going under.
            v = clamp01(v);
            s.submerge = v * v * (3.0 - 2.0 * v);
        }
        if (s.t_mode_s >= stage) {
            // ...and REAPPEAR in a poof. His word, and the same emitter.
            s.poof = true;
            s.mode = WalkerMode::Down;
            s.t_mode_s = 0.0;
            s.submerge = 0.0;  // he is out, and "out" is exactly 0
        }
        return s;
    }
    if (s.mode == WalkerMode::Down) {
        // Prone or supine, and then a crawl. The crawl's SHARE grows with the
        // snow: on a road he is up almost at once, in the bush he is on his
        // hands and knees for most of what the depth bought.
        // ⚠ THE THREE STAGES MUST PARTITION THE RISE, AND THEY DID NOT. This
        // was a PRODUCT of the two complements, which over-counts: at full
        // depth the sequence summed to 16.31 s against the 15 s §7.5 signs, so
        // "fifteen seconds in the bush" was really sixteen and a third. The
        // gate was slack enough not to catch it (red-team, 2026-09-01).
        // Complements SUBTRACT.
        const double down_s =
            std::max(0.0, 1.0 - (p.bury_frac + p.crawl_frac) * frac) * rise;
        if (s.t_mode_s >= down_s) {
            s.mode = WalkerMode::CrawlProne;
            s.t_mode_s = 0.0;
        }
        return s;
    }
    // ★★★ CHAD'S ORDER: belly, then knees, then his feet. The two crawls share
    // the budget the depth bought, so a road puts him up almost at once and the
    // bush makes him earn it -- and neither has a clock of its own.
    if (s.mode == WalkerMode::CrawlProne) {
        const double crawl_s = p.crawl_frac * frac * rise;
        if (s.t_mode_s >= crawl_s * p.crawl_prone_share) {
            s.mode = WalkerMode::CrawlKnees;
            s.t_mode_s = 0.0;
        }
        // ⚠ A BELLY CRAWL IS NOT A STANDSTILL AND IT IS NOT A WALK. He drags
        // himself, slowly -- half the deep-snow pace, because he is pulling
        // with his arms and pushing with his toes, not stepping.
        s.speed_cap_mps = 0.5 * p.speed_deep_mps;
    } else if (s.mode == WalkerMode::CrawlKnees) {
        const double crawl_s = p.crawl_frac * frac * rise;
        if (s.t_mode_s >= crawl_s * (1.0 - p.crawl_prone_share)) {
            s.mode = WalkerMode::Afoot;
            s.t_mode_s = 0.0;
        }
        // On hands and knees he makes ground at about the deep-snow pace,
        // whatever the snow is -- a crawl is a crawl.
        s.speed_cap_mps = p.speed_deep_mps;
    }

    // --- locomotion (§7.3 stage 7) -----------------------------------------
    //
    // Re-orthogonalise first: "forward" on a ball is not a constant, and a
    // heading carried across a hundred metres of curvature stops being tangent.
    glm::dvec3 fwd = s.heading - glm::dot(s.heading, up) * up;
    if (glm::length(fwd) < 1.0e-9) {
        // No usable heading (a dead stop on the first step): take any tangent
        // rather than normalising a zero vector.
        const glm::dvec3 seed =
            std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
        fwd = glm::normalize(seed - glm::dot(seed, up) * up);
    } else {
        fwd = glm::normalize(fwd);
    }
    // +1 = LEFT, the kernel's own steer convention (sim/sled.h). One sign
    // convention in this program, not two.
    const double turn = std::clamp(static_cast<double>(in.turn), -1.0, 1.0);
    if (turn != 0.0) {
        const glm::dvec3 left = glm::normalize(glm::cross(up, fwd));
        const double a = turn * p.turn_rate_rps * dt_s;
        fwd = glm::normalize(std::cos(a) * fwd + std::sin(a) * left);
    }
    s.heading = fwd;

    const double demand =
        std::clamp(static_cast<double>(in.forward), -1.0, 1.0) *
        s.speed_cap_mps;
    // Speed is a scalar along the heading, slewed: 87.5 kg in boots does not
    // reach its pace instantly and does not stop dead.
    const double cur = glm::dot(s.vel, fwd);
    const double next = slew(cur, demand, p.accel_tau_s, dt_s);
    s.vel = fwd * next;
    s.pos += s.vel * dt_s;
    // Stay ON the surface: a straight tangential step on a 15 km ball lifts
    // him (chord versus arc), so contact is PROJECTED every step and not
    // merely checked -- the same constraint the landing applies.
    const double r3 = glm::length(s.pos);
    if (r3 > 0.0) s.pos = (s.pos / r3) * (g.drive_r + p.lie_clearance_m);

    // ★ THE GAIT ADVANCES ON DISTANCE, NEVER ON TIME. Cadence is then
    // speed / stride and is not authored anywhere -- which is why his feet
    // cannot skate, at any frame rate, and why "quick gait on hardpack" and
    // "large stepping in deep snow" are two different motions rather than one
    // motion at two rates.
    if (s.stride_m > 0.0) {
        s.gait_phase += std::abs(next) * dt_s / s.stride_m;
        s.gait_phase -= std::floor(s.gait_phase);
    }
    return s;
}

}  // namespace sim
