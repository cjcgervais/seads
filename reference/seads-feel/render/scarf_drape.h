// ★ SCARF-DRAPE (2026-09-03): THE SCARF'S RULED CONSTANTS, SHARED.
//
// These lived as constexprs inside render/sled_model.cpp, which was fine while
// the sled rider was the only body wearing the scarf. The flak gunner draws
// the SAME GLB's scarf through his own drawer (render/flak_gunner.cpp), and a
// second reader of a ruling must read the ruling, not retype it -- retyped
// constants are how the suffix/side and colour-space traps happened. Every
// value keeps its original banner in sled_model.cpp history; the ruling dates
// are preserved here.
#pragma once

namespace render {

// The authored pods are SEG_RUN = 0.048 m long; the bones in the file stay at
// their authored 0.080 m spacing, so the run-time chain steps this fraction of
// the measured spacing and the pods meet exactly. SAME constant as
// scarf_geom.py SEG_FRAC -- one ruling, N readers. Fourth drive: 0.288 m was
// "too short" -> 6 x 0.068 = 0.408 m.
inline constexpr float kScarfRunSegFrac = 0.85f;
// The authored tube's half-thickness (= scarf_geom.py TAIL_T, flat fabric).
inline constexpr float kScarfTubeHalfM = 0.015f;
// The SHORT tail (scarf_s01/s02): 2 links, heavily damped -- Chad: "the
// shorter one can move a little bit but more simply".
inline constexpr int kScarfShortSegs = 2;
inline constexpr float kScarfShortDamping = 0.90f;
// Clear air held between the drawn coat surface and the nearest scarf fabric.
inline constexpr float kScarfBackMarginM = 0.012f;
// The lateral strip the plane's SURFACE offset is taken over (2026-08-24).
// Narrow on purpose: the scarf lies along the spine, and the rearmost point of
// a wide band is a shoulder or an elbow, not the surface under the ribbon.
inline constexpr float kScarfBackStripHalfM = 0.070f;
// The helmet keep-out sphere sits this far up the head bone's own +Y from the
// head joint -- the joint is at the base of the skull, the helmet is not.
inline constexpr float kScarfHeadCentreUpM = 0.10f;

}  // namespace render
