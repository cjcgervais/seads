// Winter S1 — the global winter re-skin, as executable law.
//
// The rung's whole justification (WINTER_LAW sec5) is: "Not optional and must
// precede the sled: a snowmachine over summer-coloured ground teaches nothing
// about feel." These legs are what stop summer colour creeping back in.
//
// Two of them (roads, tunnels) assert something that is ALREADY TRUE BY
// ARCHITECTURE rather than something built here. That is deliberate: a property
// nothing asserts is a property a later pass silently breaks, and the winter
// snow shader is exactly the kind of pass that would reach for them next.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_world.h"
// NOTE: no render/ includes. render/planet.h pulls raylib.h, which this test
// target does not link -- and a unit test reaching into the render layer is
// what the layering tripwire exists to stop. The struct DEFAULTS are asserted
// from source text instead, which is the honest way to check a default anyway.

namespace {

// Rec. 601 luma — the same weighting the eye applies, so "brighter" in these
// legs means brighter to Chad, not brighter to a summing junction.
double luma(double r, double g, double b) {
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

std::string read_file(const std::string& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        *ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    *ok = true;
    return ss.str();
}

const std::string kRoot = std::string(SEADS_ASSET_DIR) + "/..";

}  // namespace

// --------------------------------------------------------------------------
// D1 — lake ice
// --------------------------------------------------------------------------
TEST_CASE("winter S1: lakes freeze to ICE, and the ice is dimmer than land snow") {
    // sec6b.2 makes the lake plane the DRIVABLE ICE and sec3.5 punches through
    // it, so "where does the lake end and the shore begin" is a survival read
    // for the sled. If ice and snow are the same value the shoreline vanishes.
    bool hok = false;
    const std::string hdr = read_file(kRoot + "/render/planet.h", &hok);
    REQUIRE(hok);
    // The in-struct defaults must themselves encode the rule, so a caller that
    // forgets to plumb config still gets ice dimmer than snow.
    CHECK(hdr.find("float ice_albedo = 0.78f;") != std::string::npos);
    CHECK(hdr.find("float albedo = 0.92f;") != std::string::npos);

    // The shipped config must satisfy it too — the default being right is no
    // use if world.toml overrides it wrong.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ground.ice_albedo < w.ground.snow_albedo);
    CHECK(w.ground.ice_albedo > 0.0);

    // The separation has to be big enough to SEE, not merely nonzero. 0.05 is
    // about where a value step stops reading as a gradient on a lit sphere.
    CHECK((w.ground.snow_albedo - w.ground.ice_albedo) >= 0.05);
}

TEST_CASE("winter S1: the summer mirror is DAMPED under winter, not deleted") {
    // Real lake ice keeps a weak sheen; a mirror-strength ice sheet reads as
    // open water you could land on. But deleting the branch outright would fork
    // the river path that shares it, so both factors must be strictly inside
    // (0,1) rather than 0 or 1.
    bool hok = false;
    const std::string hdr = read_file(kRoot + "/render/planet.h", &hok);
    REQUIRE(hok);
    CHECK(hdr.find("float ice_reflect_frac = 0.18f;") != std::string::npos);
    CHECK(hdr.find("float ice_glint_frac = 0.25f;") != std::string::npos);

    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ground.ice_reflect_frac > 0.0);
    CHECK(w.ground.ice_reflect_frac < 0.35);
}

TEST_CASE("winter S1: summer is bit-unchanged -- every ice term is gated on uSeasonSnow") {
    // uSeasonSnow = 0 must be EXACT identity, or this rung silently re-tunes
    // the summer look it was never asked to touch.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);

    // The ice albedo mix is driven by iceCover, which is uSeasonSnow * water.
    REQUIRE(src.find("float iceCover = uSeasonSnow * water;") !=
            std::string::npos);
    // Both damping factors mix FROM 1.0 on uSeasonSnow, so they are exactly
    // 1.0 at zero snow whatever is nested inside them. Asserted as the OUTER
    // form rather than a whole literal line: S1b nested the night-damping term
    // into the reflect factor, and a leg pinned to the exact old string would
    // have gone red on a change that preserved the property it exists to
    // protect. Pin the invariant, not the spelling.
    REQUIRE(src.find("float iceRefl = mix(1.0, uIceReflectFrac * ") !=
            std::string::npos);
    REQUIRE(src.find(", uSeasonSnow);") != std::string::npos);
    REQUIRE(src.find("mix(1.0, uIceGlintFrac, uSeasonSnow)") !=
            std::string::npos);
}

// --------------------------------------------------------------------------
// D2 — the groomed trail
// --------------------------------------------------------------------------
TEST_CASE("winter S1: the snowmobile trail is groomed SNOW, never the summer clay") {
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    const auto& clay = w.ribbons.trail_color;
    const auto& snow = w.ribbons.trail_winter_color;

    // Clay is red-dominant earth; packed snow is blue-dominant.
    CHECK(clay.r > clay.b);
    CHECK(snow.b >= snow.r);

    // And it is a different material, not a tint of the same one.
    CHECK(luma(snow.r, snow.g, snow.b) > 2.0 * luma(clay.r, clay.g, clay.b));
}

TEST_CASE("winter S1: the groomed trail stays DIMMER than the snowpack around it") {
    // ★ S1_SPEC RT-1, and the most valuable leg in this file. The obvious
    // implementation of "make the trail wintry" is to make it white -- which is
    // LESS legible than the clay it replaced, because it then matches the
    // snowfield it sits in. That would have shipped a regression that looked
    // exactly like the fix.
    //
    // Packed snow is genuinely darker and bluer than fresh cover, so the
    // physical answer and the legible answer agree: the trail reads by value
    // contrast plus groomer corduroy, never by brightness.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ribbons.trail_winter_color.g < w.ground.snow_albedo);
    CHECK(w.ribbons.trail_winter_color.b < w.ground.snow_albedo);

    // Corduroy is what carries the read once brightness cannot. It must exist,
    // and it must stay subtle -- the road's high-frequency mottle was REMOVED
    // for aliasing into flicker at grazing flight angles, and a strong
    // transverse ripple is the same defect wearing a different hat.
    CHECK(w.ribbons.trail_corduroy > 0.0);
    CHECK(w.ribbons.trail_corduroy <= 0.15);
    CHECK(w.ribbons.trail_corduroy_m > 0.0);
}

// --------------------------------------------------------------------------
// S1b — the WINTER NIGHT (Chad, 2026-08-10)
// --------------------------------------------------------------------------
TEST_CASE("winter S1b: snow and ice keep a night lift -- they must not read as melted") {
    // Chad, after flying S1: "as we switch to night it looks like it changes to
    // summer. It should just lose brightness at night, but the stars and moon
    // should be reflecting off the snow and ice making it white."
    //
    // Snow at albedo ~0.9 really does return ~9x the starlight/skyglow that
    // summer ground does, and snow/sky multiple scattering compounds it. The
    // flat uNightFill floor cannot express that -- it knows nothing about what
    // it is lighting -- so a dedicated lift is the physically right term.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ground.night_glow.r > 0.0);  // 0 IS the bug
    CHECK(w.ground.night_glow.g > 0.0);
    CHECK(w.ground.night_glow.b > 0.0);
}

TEST_CASE("winter S1b: the night light is COOL, so bright snow does not read cream") {
    // ★ Found by measuring the render, not by reading the shader. With a
    // NEUTRAL night light, open snow came out RGB (0.782, 0.749, 0.693) --
    // R/B = 1.13, visibly cream, against Chad's "snow needs to stay WHITE".
    //
    // The cause is NOT in this rung: the post split-tone deliberately warms
    // highlights (split_hi = [1.07, 1.01, 0.90], the flown silver look of the
    // whole game). Terrain was never bright at night before, so that tint had
    // never landed on snow. It is a collision between two art rulings, not a
    // defect, so the game-wide post pass is left alone and the night light is
    // pre-compensated COOL instead.
    //
    // Which is also the physically right answer: moon and starlight are cooler
    // than sunlight, and snow under a moon genuinely reads blue-white. Both
    // reasons point the same way, which is the main argument for doing it here.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ground.night_glow.b > w.ground.night_glow.r);

    // And the post pass it compensates for must still be the warm one -- if
    // split_hi ever goes neutral, this pre-compensation becomes an over-correct
    // and the snow turns BLUE. Tie them together so that change cannot be made
    // silently.
    INFO("if split_hi is no longer warm, re-derive night_glow (it would over-cool)");
    CHECK(w.tone.split_hi.r > w.tone.split_hi.b);
}

TEST_CASE("winter S1b: the night light MULTIPLIES albedo luminance, and only snow/ice") {
    // ★ This leg was originally written to assert the lift was NEUTRAL, and the
    // MEASUREMENT overturned that premise: a neutral light reads CREAM once the
    // post split-tone warms the highlights. Rewritten to the property that is
    // actually true rather than deleted -- but the two invariants worth keeping
    // are unchanged, and both are load-bearing:
    //
    // 1. Scaled by albedo LUMINANCE, not per-channel albedo. Luminance keeps the
    //    ~9:1 snow-vs-rock separation that makes the dark verticals read (§2.5),
    //    while leaving the LIGHT's colour to the tint alone -- so the surface
    //    cannot smuggle its own cast into the night.
    // 2. Gated on winterSurf, so slopes that SHED snow stay dark and cliffs
    //    still read as rock against the white.
    //
    // It multiplies rather than adds because a flat additive lift raises snow
    // and dark rock equally, greying the scene toward the middle -- it can never
    // make snow white without lifting what should stay black. That was the first
    // attempt and Chad could see it was wrong.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("float albLum = dot(albedo, vec3(0.299, 0.587, 0.114));") !=
            std::string::npos);
    REQUIRE(src.find("lit += vec3(albLum) * uWinterNightGlow * winterSurf * "
                     "nightAmt;") != std::string::npos);
    REQUIRE(src.find("float winterSurf = max(snowCover, iceCover);") !=
            std::string::npos);
    // The uniform must be a vec3 -- a float here silently re-neutralises the
    // light and brings the cream back.
    REQUIRE(src.find("uniform vec3 uWinterNightGlow;") != std::string::npos);
}

TEST_CASE("winter S1b: the post highlight tint goes NEUTRAL at night so snow reads white") {
    // ★ THE REAL CAUSE of "snow needs to stay white", found by A/B-ing the
    // render rather than by reading code. The split-tone REPLACES hue for
    // low-saturation pixels (col ~= Y*tint); it does not multiply it. So a warm
    // split_hi repaints bright winter-night snow CREAM whatever colour the
    // light was. Measured on open night snow:
    //
    //     warm split_hi, neutral light   R/B 1.129   <- the bug
    //     warm split_hi, COOL light      R/B 1.116   <- barely moved
    //     NEUTRAL split_hi               R/B 0.972   <- a true cool white
    //
    // That is why the fix had to live in the post pass and not in the light.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");

    // Day stays WARM -- that is the flown Scarce Skies silver look and it is
    // not this rung's to change.
    CHECK(w.tone.split_hi.r > w.tone.split_hi.b);
    // Night is neutral-or-cool, so bright snow is not repainted cream.
    CHECK(w.tone.split_hi_night.r <= w.tone.split_hi_night.b);
    // And the two must actually differ, or nothing happens at night.
    CHECK(w.tone.split_hi != w.tone.split_hi_night);
}

TEST_CASE("winter S1b: DAYLIGHT is bit-identical -- the night tint blend is 0 by day") {
    // Chad has already flown and approved the daylight look. The blend must be
    // exactly the configured split_hi at day, and the default must be 0 so any
    // caller that never sets a night amount keeps the flown look untouched.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/post.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("float g_night_amt = 0.0f;") != std::string::npos);
    // Linear blend anchored ON split_hi, so night_amt == 0 => split_hi exactly.
    REQUIRE(src.find("p.split_hi + (p.split_hi_night - p.split_hi) * "
                     "g_night_amt") != std::string::npos);
}

TEST_CASE("winter S1b: the ice stops mirroring the black night sky") {
    // The SECOND cause of the same report, and the one a brightness floor
    // cannot explain: even damped to 18%, a near-black night sky mixed into an
    // already-dim surface drags the ice toward blue-black, so it read as open
    // water again. The sky-mirror therefore falls further after dark --
    // while the star/moon GLINTS are deliberately left alone, because those
    // reflections are exactly what Chad asked to keep.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.ground.ice_night_reflect_frac >= 0.0);
    CHECK(w.ground.ice_night_reflect_frac < 1.0);  // 1.0 = no night damping at all

    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("mix(1.0, uIceNightReflectFrac, nightAmt)") !=
            std::string::npos);
}

TEST_CASE("winter S1b: the night factor is single-sourced, not computed twice") {
    // The star reflection had its own copy of the same smoothstep. Two copies
    // of "when is it night" drift, and then the glow and the star sparkle
    // disagree about dusk -- which reads as a seam at the terminator.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("float wNight = nightAmt;") != std::string::npos);
    // Exactly one definition of the night factor.
    std::size_t n = 0, at = 0;
    const std::string needle = "float nightAmt = ";
    while ((at = src.find(needle, at)) != std::string::npos) {
        ++n;
        at += needle.size();
    }
    CHECK(n == 1);
}

// --------------------------------------------------------------------------
// Snow sparkle -- Chad, twice: snow "appears to sparkle" in moon AND
// starlight
// --------------------------------------------------------------------------
TEST_CASE("snow sparkle: the uniforms exist and are applied land-only") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("uniform float uSnowSparkle;") != std::string::npos);
    REQUIRE(src.find("uniform float uSnowSparkleSharp;") != std::string::npos);
    // Additive, land-only (the lake mirror owns its own star glints).
    REQUIRE(src.find("lit += vec3(snowSparkle) * (1.0 - water);") !=
            std::string::npos);
    // Starlight sparkle is load-bearing and must NOT be moon-gated: kStar is
    // an unconditional additive term in the gate, not multiplied by moon fill.
    // (Matched against the RAW SOURCE line, not the compiled shader string --
    // the literal is split across two adjacent C-string lines in planet.cpp,
    // so the needle stops at that line's own text.)
    REQUIRE(src.find("float spGate = nightAmt * winterSurf * (kStar + "
                     "uMoonFill * ") != std::string::npos);
}

TEST_CASE("snow sparkle: the shipped defaults are 0.4 amplitude / 120.0 sharp") {
    bool ok = false;
    const std::string hdr = read_file(kRoot + "/render/planet.h", &ok);
    REQUIRE(ok);
    CHECK(hdr.find("float snow_sparkle = 0.4f;") != std::string::npos);
    CHECK(hdr.find("float snow_sparkle_sharp = 120.0f;") != std::string::npos);
}

// --------------------------------------------------------------------------
// D3 — LOCK: plowed roads never whiten (sec2.4c, Chad 2026-08-10)
// --------------------------------------------------------------------------
TEST_CASE("winter S1: plowed roads read BARE and never take the snow shader") {
    // Chad RULED 2026-08-10 that roads are plowed. That is already true by
    // architecture -- roads are draped ribbon geometry with their OWN shader,
    // drawn after the planet -- so this LOCKS it rather than building it.
    // sec3.4 already implied it too: ski sparks fly "on road/rock only", which
    // only makes sense if the road is bare.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");

    // Dark asphalt, far below the snow it is cut through.
    CHECK(luma(w.ribbons.road_bed.r, w.ribbons.road_bed.g,
               w.ribbons.road_bed.b) < 0.5 * w.ground.snow_albedo);

    // And the road branch of the ribbon shader must not consult the season.
    // The winter uniform exists for the TRAIL; if it ever reaches the asphalt,
    // roads start disappearing under snow again.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/ribbons.cpp", &ok);
    REQUIRE(ok);
    const std::size_t road = src.find("// ROAD: the asphalt stays DARK");
    REQUIRE(road != std::string::npos);
    const std::size_t end = src.find("finalColor", road);
    REQUIRE(end != std::string::npos);
    CHECK(src.substr(road, end - road).find("uWinter") == std::string::npos);
}

// --------------------------------------------------------------------------
// D4 — LOCK: tunnels stay black and snow-free
// --------------------------------------------------------------------------
TEST_CASE("winter S1: tunnels stay black -- no snow term can reach the tunnel shader") {
    // Also already true by architecture (render/tunnel.cpp carries its own
    // kTunnelVS/kTunnelFS), and also worth locking: a global "whiten the world"
    // pass is exactly the change that would reach in here next.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/tunnel.cpp", &ok);
    REQUIRE(ok);
    CHECK(src.find("uSeasonSnow") == std::string::npos);
    CHECK(src.find("uSnowAlbedo") == std::string::npos);
    CHECK(src.find("uIceAlbedo") == std::string::npos);
    CHECK(src.find("uWinter") == std::string::npos);
}

// --------------------------------------------------------------------------
// D5 — sec6c.5, the OWED re-derivation
// --------------------------------------------------------------------------
TEST_CASE("winter S1: the landable-pond guard is discharged, and the ice/shore read replaces it") {
    // sec6c.5 owes a re-derivation of PAL_BARREN's separation "under a white
    // world", because the number that protected it was measured against water
    // that no longer looks like water.
    //
    // ★ THE FINDING: under winter that hazard INVERTS and largely dissolves.
    // The summer risk was dark barren rock (42,40,37) reading as a dark
    // landable pond -- PAL_WATER was (20,21,24), a separation of only a few
    // value points. Frozen, the lake is BRIGHT. Rock and pond now sit at
    // opposite ends of the range, so the old guard is strictly safer than the
    // number it was tuned to, and re-tuning PAL_BARREN would be solving a
    // problem winter already solved.
    //
    // What replaces it is a SLED question, not a plane question: bright ice
    // against bright snow-covered land, i.e. can you see the shoreline. That is
    // asserted above and in load_world's own check.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");

    // Summer adjacency, as it was measured (0-255 albedo-texture values).
    const double barren = 42.0 / 255.0;   // PAL_BARREN
    const double water = 20.0 / 255.0;    // PAL_WATER
    const double summer_sep = barren - water;

    // Winter adjacency: the same rock against ICE.
    const double winter_sep = w.ground.ice_albedo - barren;

    INFO("summer barren-vs-water " << summer_sep
                                   << " vs winter barren-vs-ice " << winter_sep);
    CHECK(winter_sep > summer_sep);
    // Not marginally -- the whole point is that this stops being the risk.
    CHECK(winter_sep > 4.0 * summer_sep);
}

// --------------------------------------------------------------------------
// sec6c.1 SLOPE GATE -- Chad's fly ruling 2026-08-11: flat barrens hold snow,
// sloped faces are the blackest.
// --------------------------------------------------------------------------
TEST_CASE("winter S2: the barren shed is GATED by slope, and the gate's floor is load-bearing") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);

    // The shed line now carries a THIRD factor, bgate -- the old
    // `snowCover *= (1.0 - uBarrenSnowShed * bshed);` no longer exists on its
    // own; a bare re-appearance of it (without `* bgate`) would silently
    // un-gate the shed and flatten out Chad's "flats read wintered" ruling.
    CHECK(src.find("snowCover *= (1.0 - uBarrenSnowShed * bshed);") ==
          std::string::npos);
    // ★ R3 amended the shed line with a FOURTH factor, and the UNCONDITIONAL
    // form must not come back: applying this multiply at full strength with no
    // dial at all sheds the rock TWICE wherever vSnowDepth already carries the
    // shed -- the exact opposite failure to the un-gated form above, and just
    // as invisible in a green build.
    CHECK(src.find("snowCover *= (1.0 - uBarrenSnowShed * bshed * bgate);") ==
          std::string::npos);
    // ★★★ THE FADE-ONLY FORM IS NOW ALSO FORBIDDEN (Chad's 2026-08-27 ruling).
    // `(1.0 - uSnowDepthMix)` ALONE welded the black-rock fence to the tone
    // dial: with exposure armed the shed faded out entirely, which is only
    // sound while full_depth is deep enough for the depth field to express the
    // shed itself. At the shipped full_depth = 0.10 m everything saturates and
    // that expression cannot -- measured, sloped rock 0.230 -> 0.431 against a
    // fence Chad has ruled on three times. Restoring this form would silently
    // re-break it, so it is pinned as an ANTI-needle, not just replaced.
    CHECK(src.find("snowCover *= (1.0 - (1.0 - uSnowDepthMix) * "
                   "uBarrenSnowShed * ") == std::string::npos);
    // The shipped form: max(fade, keep) -- keep = 0 reproduces the fade-only
    // expression exactly, keep = 1 holds the fence at any mix, and max() can
    // never shed harder than one full application (so the double-shed the
    // CHECK above guards is unreachable from this expression at any dial).
    // Split across two adjacent C-string lines, same discipline as the
    // smoothstep needles below.
    REQUIRE(src.find("snowCover *= (1.0 - max(1.0 - uSnowDepthMix, "
                     "uBarrenShedKeep) * ") != std::string::npos);
    REQUIRE(src.find("uBarrenSnowShed * bshed * bgate);") != std::string::npos);
    // The dial must actually reach the shader, or the line above is inert.
    REQUIRE(src.find("uniform float uBarrenShedKeep;") != std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uBarrenShedKeep\")") !=
            std::string::npos);

    // The gate itself mirrors world::SnowpackField::barren_slope_gate's
    // formula: a FLOOR (uBarrenFlatShed) plus a smoothstep on 1-cosSlope. The
    // floor is load-bearing -- 0 would erase the barren hero layer's lobes on
    // flat ground entirely -- so its presence in the gate expression is
    // pinned explicitly, not just assumed from the multiply above.
    REQUIRE(src.find("float bgate = uBarrenFlatShed + (1.0 - "
                     "uBarrenFlatShed) *") != std::string::npos);
    // Matched against the RAW SOURCE, not the compiled shader string: the
    // smoothstep call is split across two adjacent C-string lines in
    // planet.cpp (same discipline the spGate leg above already follows), so
    // each needle stops at its own line's text.
    REQUIRE(src.find("smoothstep(uBarrenSlopeXLo, uBarrenSlopeXHi, ") !=
            std::string::npos);
    REQUIRE(src.find("1.0 - cosMacro);") != std::string::npos);

    // ★ Fly 3: the macro normal is a MIP-SMOOTHED normalCube sample, never
    // fragNormal (except as the no-asset fallback) -- the mesh normal is
    // interpolated across ~59 m triangles, and the gate bent at triangle
    // edges exactly where the snow->rock band sits (Chad: "really
    // triangulated" near Kelly Lake). This pin is load-bearing: reverting to
    // fragNormal re-facets every barren transition.
    REQUIRE(src.find("textureLod(normalCube, normalize(fragDir), ") !=
            std::string::npos);
    REQUIRE(src.find("uBarrenNormalLod).xyz * 2.0 - 1.0)") !=
            std::string::npos);

    // ★ Fly 3: past full shed the steepest barren faces darken the ALBEDO
    // itself ("even blacker so it stands out more") -- render-only, gated by
    // bshed, ramp chained off uBarrenSlopeXHi so there is no gap and no
    // overlap with the shed's own curve.
    // ★ Fly 4: the face-dark strength is now modulated by fdMod (the cone
    // lattice's VALUE coupling, below) -- a bare reappearance of the old
    // `uBarrenFaceDark * bshed *` (without `* fdMod`) would silently drop
    // that coupling and the cones would go back to being invisible on the
    // near-black rock, so the OLD form is pinned absent and the NEW form
    // pinned present.
    CHECK(src.find("albedo *= 1.0 - uBarrenFaceDark * bshed *") ==
          std::string::npos);
    // (fly 5 added fdKeep between fdMod and bshed -- the fdMod-only form is
    // now ALSO pinned absent, same reasoning.)
    CHECK(src.find("albedo *= 1.0 - uBarrenFaceDark * fdMod * bshed *") ==
          std::string::npos);
    REQUIRE(src.find(
                "albedo *= 1.0 - uBarrenFaceDark * fdMod * fdKeep * bshed *") !=
            std::string::npos);
    // ★ Fly 5: fdKeep (the mottle resisting face-dark) is load-bearing --
    // without it the steepest faces crush to smooth uniform black and the
    // baked mottle is invisible exactly where Chad wants it loudest.
    REQUIRE(src.find("float fdKeep = 1.0 - uBarrenFaceMottle *") !=
            std::string::npos);
    REQUIRE(src.find("uBarrenFaceDark * fdMod * fdKeep * bshed *") !=
            std::string::npos);
    REQUIRE(src.find("smoothstep(uBarrenSlopeXHi, uBarrenFaceDarkXHi, ") !=
            std::string::npos);

    // ★ Fly 4: fdMod itself -- the value coupling is load-bearing (without
    // it the shatter-cone lattice only perturbs the NORMAL, invisible at the
    // deepened ~0.02 face-dark albedo), so the exact expression is pinned.
    REQUIRE(src.find(
                "float fdMod = 1.0 - 0.35 * (coneV - 0.5) * 2.0 * coneW;") !=
            std::string::npos);

    // The three new uniforms exist (cone_glsl.cpp's shared block, the same
    // block uBarrenShedLo/Hi already ride).
    bool cok = false;
    const std::string cone_src = read_file(kRoot + "/render/cone_glsl.cpp", &cok);
    REQUIRE(cok);
    CHECK(cone_src.find("uniform float uBarrenSlopeXLo;") != std::string::npos);
    CHECK(cone_src.find("uniform float uBarrenSlopeXHi;") != std::string::npos);
    CHECK(cone_src.find("uniform float uBarrenFlatShed;") != std::string::npos);

    // ★ Fly 4: cone_value exists -- the scalar VALUE channel that makes the
    // cone lattice read on black rock (§ above).
    CHECK(cone_src.find("float cone_value(vec3 p, vec3 n){") !=
          std::string::npos);
}

TEST_CASE("winter S2: the slope-gate dials are single-sourced from world::SnowParams") {
    // The degree->x conversion and the flat floor default live in
    // world/snowpack.h -- render/planet.cpp defaults its globals FROM
    // world::SnowParams (never re-typing the numbers), same discipline as
    // the existing k/lo/hi shed defaults just above them.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    CHECK(src.find("g_shed_slope_x_lo = kSnowDefaults.barren_slope_x_lo();") !=
          std::string::npos);
    CHECK(src.find("g_shed_slope_x_hi = kSnowDefaults.barren_slope_x_hi();") !=
          std::string::npos);
    CHECK(src.find("g_shed_flat_frac = kSnowDefaults.barren_flat_shed_frac;") !=
          std::string::npos);

    // The shipped config values match the derivation in world/snowpack.h.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.snowpack.barren_slope_lo_deg == Catch::Approx(8.0));
    CHECK(w.snowpack.barren_slope_hi_deg == Catch::Approx(12.0));
    CHECK(w.snowpack.barren_flat_shed_frac == Catch::Approx(0.10));
}

// --------------------------------------------------------------------------
// R5 row 3 — THE TRACK READS. The packed-track tint, pinned at the source the
// way the shed law is above: the property a later refactor will break is
// fence 5 (bit-identical at zero compaction), and its shader half lives in
// the exact FORM of these lines.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 3: the packed-track tint is armed on the patch draw only, "
          "and exact at zero") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);

    // The dial reaches the shader and its location is fetched.
    REQUIRE(src.find("uniform float uTrackPackMix;") != std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uTrackPackMix\")") !=
            std::string::npos);

    // ★ THE ZERO-EXACT FORM. clamp(uTrackPackMix * sqrt(max(vSnowDepth,0)))
    // is exactly 0 whenever either factor is 0 (sqrt(0) == 0, clamp and mix
    // pass 0 through exactly), which is what keeps every untracked pixel --
    // and the ENTIRE planet pass, where uTrackPackMix is 0 -- the shipped
    // pixel to the bit. A pow with a bias, an added ambient term, or a
    // desaturation applied outside this mix would all break that silently.
    // Needles stop at their own C-string line, the smoothstep discipline.
    REQUIRE(src.find("float packT = clamp(uTrackPackMix * sqrt(max(vSnowDepth, ") !=
            std::string::npos);
    REQUIRE(src.find("albedo = mix(albedo, vec3(0.80, 0.84, 0.90), packT * ") !=
            std::string::npos);

    // The packed colour MIRRORS the ribbons ruling (S1_SPEC RT-1: packed snow
    // is darker and bluer than the wild snowpack, never brighter) -- if
    // trail_winter_color ever moves, this leg forces the mirror to be
    // re-derived rather than silently forked.
    bool rok = false;
    const std::string rib = read_file(kRoot + "/render/ribbons.h", &rok);
    REQUIRE(rok);
    REQUIRE(rib.find("trail_winter_color{0.80f, 0.84f, 0.90f}") !=
            std::string::npos);

    // The planet pass zeroes the dial every frame; the patch draw arms it and
    // restores it -- the loc_vertex_normal_mix discipline, because the lake
    // mirror mesh shares this program and its texcoord.y is NOT compaction.
    REQUIRE(src.find("const float planet_pack_mix = 0.0f;") !=
            std::string::npos);
    REQUIRE(src.find("SEADS_TRACK_PACK") != std::string::npos);
    REQUIRE(src.find("const float pack_off = 0.0f;") != std::string::npos);
}

// --------------------------------------------------------------------------
// R5 row 8 — SUN SPARKLE. Row 3's partner: the virgin field glitters, the
// packed rut does not, so the cut reads by CONTRAST without darkening. Pinned
// at the source like row 3: the properties a later refactor will break are
// fence 1 (bit-identical at SEADS_SPARKLE=0), the one-signal compaction
// suppression, and the dual-role texcoord.y arm discipline.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 8: sun sparkle is zero-exact at dial 0, suppressed by the "
          "row-3 compaction signal, and armed on the patch draw only") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);

    // The uniforms reach the shader and their locations are fetched.
    REQUIRE(src.find("uniform float uSunSparkle;") != std::string::npos);
    REQUIRE(src.find("uniform float uSunSparkleSharp;") != std::string::npos);
    REQUIRE(src.find("uniform float uSparkleCompactArm;") != std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uSunSparkle\")") !=
            std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uSparkleCompactArm\")") !=
            std::string::npos);

    // ★ THE ZERO-EXACT FORM (fence 1). uSunSparkle multiplies LAST, outside
    // the clamp: clamp(finite) * 0.0 == 0.0 exactly, and lit += vec3(0.0) on
    // the non-negative lit is the shipped pixel to the bit. A pow with a
    // bias, an ambient floor, or folding the dial inside the gate would all
    // break that silently. Needles stop at their own C-string line.
    REQUIRE(src.find("float sunSparkle = clamp(ssGlint * ssGate, 0.0, 1.0) * ") !=
            std::string::npos);

    // ★ THE COMPACTION SUPPRESSION — one signal, two consumers. The SAME
    // sqrt-skirted form as row 3's packT (pinned above), reading the SAME
    // texcoord.y compaction channel: where the tint blooms, the sparkle dies,
    // and clamp(uSparkleCompactArm * x) == 0 on the planet pass (arm 0) keeps
    // depth-in-metres from ever suppressing the open field.
    REQUIRE(src.find("float ssPack = clamp(uSparkleCompactArm * "
                     "sqrt(max(vSnowDepth, ") != std::string::npos);

    // ★ FENCE 3 — SNOW ONLY. The gate scales by snowCover (shed rock and
    // water carry none) and deliberately NOT winterSurf, whose iceCover half
    // would sun-sparkle the frozen lakes; day-gated by the same nightAmt the
    // night sparkle uses so the two hand over at one terminator.
    REQUIRE(src.find("float ssGate = (1.0 - nightAmt) * ndl * snowCover * "
                     "(1.0 - ") != std::string::npos);

    // Additive HIGHLIGHT path, land-masked — snow albedo 0.92 has no headroom
    // below diffuse white, so the glint must ride specular, not albedo.
    REQUIRE(src.find("lit += vec3(sunSparkle) * (1.0 - water);") !=
            std::string::npos);

    // The dual-role arm discipline (texcoord.y = DEPTH on the planet pass,
    // COMPACTION on the patch pass): the planet pass zeroes the arm every
    // frame; the patch draw arms it and restores it — the uTrackPackMix
    // discipline, because the lake mirror mesh shares this program.
    REQUIRE(src.find("const float planet_sparkle_arm = 0.0f;") !=
            std::string::npos);
    REQUIRE(src.find("const float sparkle_arm_on = 1.0f;") !=
            std::string::npos);
    REQUIRE(src.find("const float sparkle_arm_off = 0.0f;") !=
            std::string::npos);

    // The dial, the SEADS_TRACK_PACK precedent: read once, validated,
    // warn-and-default, logged; 0 is the kill switch.
    REQUIRE(src.find("SEADS_SPARKLE") != std::string::npos);

    // ★ FENCE 6 ANTI-NEEDLES — the pattern must be a stable function of WORLD
    // POSITION: no clock and no screen-space seed anywhere in this shader
    // (both read as boil/crawl while driving). render/ reads no clock — house
    // law — and planet.cpp ships with neither token today.
    CHECK(src.find("uTime") == std::string::npos);
    CHECK(src.find("gl_FragCoord") == std::string::npos);
}
