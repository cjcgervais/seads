#ifndef SEADS_RENDER_CONE_GLSL_H
#define SEADS_RENDER_CONE_GLSL_H

// Shatter-cone detail normals for the Sudbury impact structure.
//
// A shatter cone is 0.01-3 m. The equirect albedo/normal texel is ~6.8 m of
// ground and the terrain mesh vertex spacing is ~59 m, so the cone is two to
// three orders of magnitude below the finest thing the map can store: this is
// necessarily a RUNTIME detail pass, not a bake.
//
// The pattern is analytic. Each cell of a jittered lattice hosts one cone; the
// height field is a linear cone (apex high, falling at tan(half-angle)) plus a
// sinusoidal striation in the polar angle whose frequency RISES with radius --
// the "horsetail", striae tight at the apex and forking/fanning outward. The
// gradient is available in closed form, so no finite differences and no height
// texture are needed: three plane evaluations, one hash each.
//
// Blending uses the SURFACE-GRADIENT formulation (project the detail gradient
// into the base surface's tangent plane and subtract) rather than reoriented
// normal mapping. For a triplanar detail riding an arbitrary object-space base
// normal the two agree to first order, but the surface gradient composes
// correctly across the three planes without needing a per-plane tangent frame.
//
// Everything is gated by uConeDetail * mask * slope * distance in planet.cpp,
// so a fragment that is flat, un-masked, or far pays only the gate.
//
// Geology, stated plainly because half of it is licence (see
// docs/shatter_cone_feasibility.md): the cone geometry, the horsetail, the
// 75-90 degree Sudbury apical angle and the apex-toward-the-basin orientation
// are real. Drawing cones on the industrial barrens is Chad's ruling C and is
// artistic licence -- no source places documented shatter cones on the
// blackened Copper Cliff outcrops.

extern const char* kConeUniformsGLSL;   // uniform block, prepended to the FS
extern const char* kConeGLSL;           // hash + cone_grad + cone_detail_normal

#endif  // SEADS_RENDER_CONE_GLSL_H
