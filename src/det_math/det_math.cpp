// SEADS det_math implementation. Mirrors tools/detmath_ref.py exactly.
#include "det_math.h"
#include "detmath_coeffs.h"

#if defined(_MSC_VER)
#include <emmintrin.h>  // SSE2 sqrt intrinsic (MSVC builds are x64 only)
#endif

namespace seads {
namespace detm {

// det_sqrt: the ONLY sanctioned hardware-FP primitive. IEEE-754 mandates a correctly
// rounded sqrt, so this is bit-identical across conforming platforms (and equals
// CPython's math.sqrt used by the reference). Implemented via hardware intrinsics so it
// never touches libm; kernel code must still route through det_sqrt, never call sqrt().
double det_sqrt(double x) {
#if defined(_MSC_VER)
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(x)));
#else
    return __builtin_sqrt(x);  // lowers to sqrtsd / fsqrt with -fno-math-errno
#endif
}

static inline double kernel_sin(double x) {
    double z = x * x;
    double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    return x + x * (z * (S1 + z * r));
}

static inline double kernel_cos(double x) {
    double z = x * x;
    double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    return 1.0 + (-0.5 * z + z * r);
}

// Cody-Waite reduction by pi/2. Returns quadrant in [0,3], r in qr_out.
static inline int reduce_half_pi(double x, double* r_out) {
    double fn = x * TWO_OVER_PI;
    double n = (fn >= 0.0) ? static_cast<double>(static_cast<long long>(fn + 0.5))
                           : static_cast<double>(static_cast<long long>(fn - 0.5));
    *r_out = (x - n * PIO2_HI) - n * PIO2_LO;
    return static_cast<int>(static_cast<long long>(n) & 3LL);
}

double det_sin(double x) {
    double r;
    int q = reduce_half_pi(x, &r);
    switch (q) {
        case 0:  return kernel_sin(r);
        case 1:  return kernel_cos(r);
        case 2:  return -kernel_sin(r);
        default: return -kernel_cos(r);
    }
}

double det_cos(double x) {
    double r;
    int q = reduce_half_pi(x, &r);
    switch (q) {
        case 0:  return kernel_cos(r);
        case 1:  return -kernel_sin(r);
        case 2:  return -kernel_cos(r);
        default: return kernel_sin(r);
    }
}

double det_tan(double x) {
    return det_sin(x) / det_cos(x);
}

double det_atan(double x) {
    const double sign = (x < 0.0) ? -1.0 : 1.0;
    double ax = (x < 0.0) ? -x : x;
    if (ax >= 0x1.0p+66) {
        return sign * (ATANHI[3] + ATANLO[3]);
    }
    if (ax < 0x1.0p-27) {
        return x;
    }
    int idx;
    double xr;
    if (ax < 0.4375) {
        idx = -1;  xr = ax;
    } else if (ax < 0.6875) {
        idx = 0;   xr = (2.0 * ax - 1.0) / (2.0 + ax);
    } else if (ax < 1.1875) {
        idx = 1;   xr = (ax - 1.0) / (ax + 1.0);
    } else if (ax < 2.4375) {
        idx = 2;   xr = (ax - 1.5) / (1.0 + 1.5 * ax);
    } else {
        idx = 3;   xr = -1.0 / ax;
    }
    double z = xr * xr;
    double w = z * z;
    double s1 = z * (AT[0] + w * (AT[2] + w * (AT[4] + w * (AT[6] + w * (AT[8] + w * AT[10])))));
    double s2 = w * (AT[1] + w * (AT[3] + w * (AT[5] + w * (AT[7] + w * AT[9]))));
    if (idx < 0) {
        return sign * (xr - xr * (s1 + s2));
    }
    double zz = ATANHI[idx] - ((xr * (s1 + s2) - ATANLO[idx]) - xr);
    return sign * zz;
}

double det_atan2(double y, double x) {
    if (x == 0.0 && y == 0.0) return 0.0;
    if (x == 0.0) return (y > 0.0) ? HALF_PI : -HALF_PI;
    double z = det_atan(y / x);
    if (x > 0.0) return z;
    if (y >= 0.0) return z + PI;
    return z - PI;
}

double det_asin(double x) {
    if (x >= 1.0) return HALF_PI;
    if (x <= -1.0) return -HALF_PI;
    return det_atan2(x, det_sqrt((1.0 - x) * (1.0 + x)));
}

double wrap_pi(double a) {
    double q = a / TWO_PI;
    double k = (q >= 0.0) ? static_cast<double>(static_cast<long long>(q + 0.5))
                          : static_cast<double>(static_cast<long long>(q - 0.5));
    double r = a - k * TWO_PI;
    if (r <= -PI) r += TWO_PI;
    else if (r > PI) r -= TWO_PI;
    return r;
}

double wrap_2pi(double a) {
    double q = a / TWO_PI;
    double k = (q >= 0.0) ? static_cast<double>(static_cast<long long>(q))
                          : static_cast<double>(static_cast<long long>(q) - 1);
    double r = a - k * TWO_PI;
    if (r < 0.0) r += TWO_PI;
    else if (r >= TWO_PI) r -= TWO_PI;
    return r;
}

}  // namespace detm
}  // namespace seads
