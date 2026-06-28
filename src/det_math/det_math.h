// SEADS det_math — deterministic transcendental math (ATM-Sphere v1.2r0).
//
// Mirrors tools/detmath_ref.py BIT-FOR-BIT. Constants come from the auto-generated
// detmath_coeffs.h (single source of truth = the Python reference). Uses only IEEE-754
// basic ops (+ - * /) and the correctly-rounded hardware sqrt. No libm transcendentals,
// no FMA, no fast-math. Build with cmake/DeterminismFlags.cmake.
#pragma once

namespace seads {
namespace detm {

double det_sqrt(double x);
double det_sin(double x);
double det_cos(double x);
double det_tan(double x);
double det_atan(double x);
double det_atan2(double y, double x);
double det_asin(double x);
double wrap_pi(double a);
double wrap_2pi(double a);

}  // namespace detm
}  // namespace seads
