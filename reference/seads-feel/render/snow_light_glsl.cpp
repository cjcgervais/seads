#include "render/snow_light_glsl.h"

namespace render {

// Lifted VERBATIM out of render/planet.cpp's fragment shader (the expressions
// are unchanged, only their home is new), so the planet's signed look does not
// move and the banks inherit exactly it.
//
//  * ~0.4 m planet-local cells on the unit fragment direction: a stable
//    function of WORLD POSITION only — no clock, no screen-space term — so the
//    glints are nailed to the ground and animate only as the eye or the light
//    moves (no crawl, no boil).
//  * SPARSE hosts through a SOFT mask: a hard `>` is a strobing point (the
//    anti-moire rule that got the water stars through review).
//  * A per-cell jittered facet normal about the shading normal, and the glint
//    is that facet's mirror reflection of the light toward the eye.
//  * Two anti-alias fades: the fwidth LOD fade kills the term wherever one
//    pixel spans a large glint-dot range (grazing angles, the receding field),
//    and the hard distance fade kills it by ~600 m, before the cells go
//    sub-pixel and shimmer.
const char* const kSnowSparkleGLSL =
    "float p_hash13(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.yzx+33.33);\n"
    "                        return fract((p.x+p.y)*p.z); }\n"
    "vec3 snow_sparkle_cell(vec3 fragDir){\n"
    "    return floor(normalize(fragDir) * 3.5e4);\n"
    "}\n"
    "float snow_sparkle_host(vec3 spc){\n"
    "    return smoothstep(0.94, 0.97, p_hash13(spc));\n"
    "}\n"
    "vec3 snow_sparkle_facet(vec3 spc, vec3 shN){\n"
    "    vec3 spJit = (vec3(p_hash13(spc + 3.7), p_hash13(spc + 9.2),\n"
    "                       p_hash13(spc + 17.9)) - 0.5) * 0.7;\n"
    "    return normalize(shN + spJit);\n"
    "}\n"
    // lightDir is LIGHT-TRAVEL (source -> scene), the sunDir/uMoonDir
    // convention; viewDir is the unit eye -> fragment ray. UNIFORM CONTROL
    // FLOW ONLY (fwidth).
    "float snow_sparkle_glint(vec3 spN, vec3 lightDir, vec3 viewDir,\n"
    "                         float sharp, float host, float dist){\n"
    "    float d = max(dot(reflect(normalize(lightDir), spN), -viewDir), 0.0);\n"
    "    float dfw = fwidth(d);\n"
    "    float g = pow(d, sharp) * host;\n"
    "    g *= 1.0 - smoothstep(0.0, 0.6, dfw * sharp);\n"
    "    g *= 1.0 - smoothstep(400.0, 600.0, dist);\n"
    "    return g;\n"
    "}\n";

}  // namespace render
