#include "render/star_glsl.h"

namespace render {

const char* const kStarVS = R"(#version 330
in vec3 vertexPosition;   // inertial catalog unit dir
in vec2 vertexTexCoord;   // corner offset in [-1,1]^2
in vec3 vertexNormal;     // .x = apparent magnitude
uniform mat4 matModel;        // raylib: DrawMesh transform = sky wheel (inertial->world)
uniform mat4 matView;         // raylib: camera view (rotation used)
uniform mat4 matProjection;   // raylib: projection incl. lens shift
uniform float uRStar;         // cosmetic view-space depth
uniform float uStarSizeView;  // FOV-compensated view-space half-size
out vec3 vWorldDir;
out vec2 vCorner;
out float vMag;
void main() {
    vec3 worldDir = normalize(mat3(matModel) * vertexPosition);
    vWorldDir = worldDir;
    vCorner = vertexTexCoord;
    vMag = vertexNormal.x;
    vec3 viewDir = mat3(matView) * worldDir;
    vec3 viewPos = viewDir * uRStar + vec3(vertexTexCoord * uStarSizeView, 0.0);
    gl_Position = matProjection * vec4(viewPos, 1.0);
}
)";

// Declared BEFORE the shared sky GLSL (uHorizonElev is read by the shared
// sky_luminance, so it must be visible when that function is defined — the same
// order sky.cpp / planet.cpp use).
const char* const kStarFsDecls = R"(
in vec3 vWorldDir;
in vec2 vCorner;
in float vMag;
uniform vec3 uSunDir;
uniform vec3 uUp;
uniform float uEyeAlt;
uniform float uHorizonElev;   // read by the shared sky_luminance (band anchor)
uniform float uStarBrightness;
uniform float uMagRef;
uniform float uGlareCos;
uniform float uWashLum;
// S-starnight: how much the ALWAYS-ON dome air veils stars, [0,1]. Ships 0 on
// Chad's ruling (clear air must not hide the starfield); 1 restores the
// original S-airdome behaviour. Star-pass-local — the sky/planet passes have
// no business dimming stars.
uniform float uStarAirExt;
uniform vec3 uMoonDir;      // light-travel (moon->scene); occlude stars in the disc
uniform float uMoonAngRad;
out vec4 finalColor;
)";

// Concatenated AFTER kSkyUniformsGLSL + kAirFieldUniformsGLSL + kScatterGLSL +
// kAirFieldGLSL + kSkyGLSL (it calls the shared sky_luminance AND the S-airdome
// march). A fwidth-crisp round core (zoom-correct like the sun disc);
// magnitude -> BRIGHTNESS via Pogson (NOT size — bright stars as balloons
// wreck constellation legibility, Fable-after P2-2); extinction fades stars
// where the SHARED sky_luminance is bright (day/dusk) AND where the S-airdome
// airAmt is high (inside a sunlit/hazy dome — spec §1.6), so stars die exactly
// where the sky brightens or a dome washes them out, but stay brilliant in the
// vacuum gap at any altitude; glare-suppressed near the sun. Additive output
// (vec3(a), alpha 1) lands the luminance on the already-drawn sky.
const char* const kStarFsMain = R"(
void main() {
    float r = length(vCorner);
    float fw = max(fwidth(r), 1e-4);
    float core = 1.0 - smoothstep(1.0 - fw, 1.0, r);
    if (core <= 0.0) discard;
    float bright = uStarBrightness * pow(10.0, -0.4 * (vMag - uMagRef));
    bright = min(bright, uStarBrightness * 6.0);
    vec3 eyePos = air_eye_pos();
    float maxDist = air_sky_max_dist(eyePos, vWorldDir);
    float tau = air_optical_depth(eyePos, vWorldDir, maxDist, uAirStepsSky);
    float airAmt = 1.0 - exp(-tau);
    float skyLum = sky_luminance(vWorldDir, uSunDir, uUp, uEyeAlt, airAmt);
    float ext = 1.0 - smoothstep(0.0, uWashLum, skyLum);
    // S-starnight (Chad's fly, 2026-08-09: "at night time the stars should be
    // visible, there is an atmosphere but no overcast"). SUPERSEDES S-airdome
    // spec §1.6's `ext *= 1 - airAmt`, which veiled stars in proportion to the
    // ALWAYS-ON air. That was wrong on his terms and on physics': clear air
    // does not hide stars, CLOUD does — and the always-on dome air reached
    // airAmt ~0.55-0.75 at the zenith, so a clear night inside a bubble lost
    // most of its starfield with no overcast anywhere.
    //
    // Extinction is now driven by the WEATHER amount (uWeatherAmt), which is
    // already air-gated at the source (render::gate_weather_by_air) and so is
    // nonzero only inside a dome — exactly Chad's "weather only in the
    // bubbles". Fly INTO a squall at night and the stars go out; sit in clear
    // dome air and they burn. The air term is KEPT but dialable and ships at
    // 0 (uStarAirExt), so the old behaviour is one config value away rather
    // than deleted. The two combine by max(), not by product, so a full
    // overcast still kills stars even with the air term off.
    float airExt = uStarAirExt * clamp(airAmt, 0.0, 1.0);
    float wxExt = clamp(uWeatherAmt, 0.0, 1.0);
    ext *= 1.0 - clamp(max(airExt, wxExt), 0.0, 1.0);
    float sunCos = dot(vWorldDir, -uSunDir);
    ext *= 1.0 - smoothstep(uGlareCos, 1.0, sunCos);
    // Occlude stars behind the moon (Fable red-team P1-2): the moon disc is drawn
    // in the sky pass BEFORE the additive stars, so a crescent's dark limb can't
    // hide the wheeling stars without this. Kill stars inside ~1.3x the disc
    // radius (the sun-glare pattern, but geometric not brightness-based).
    float moonAng = acos(clamp(dot(vWorldDir, -uMoonDir), -1.0, 1.0));
    ext *= smoothstep(uMoonAngRad, uMoonAngRad * 1.3, moonAng);
    float a = core * bright * ext;
    finalColor = vec4(vec3(a), 1.0);
}
)";

}  // namespace render
