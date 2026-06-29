// SEADS det_math bit-exactness test. Asserts C++ det_math == Python reference vectors,
// bit-for-bit. Fast pre-golden determinism check. Exit 0 PASS, 1 FAIL.
#include "det_math.h"
#include "detmath_vectors.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace seads;

static bool bit_equal(double a, double b) {
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

// v.y is the second operand (atan2 only); unused (0.0) for unary functions.
static double call(const char* fn, double x, double y) {
    if (std::strcmp(fn, "sin") == 0)     return detm::det_sin(x);
    if (std::strcmp(fn, "cos") == 0)     return detm::det_cos(x);
    if (std::strcmp(fn, "tan") == 0)     return detm::det_tan(x);
    if (std::strcmp(fn, "atan") == 0)    return detm::det_atan(x);
    if (std::strcmp(fn, "atan2") == 0)   return detm::det_atan2(x, y);
    if (std::strcmp(fn, "asin") == 0)    return detm::det_asin(x);
    if (std::strcmp(fn, "sqrt") == 0)    return detm::det_sqrt(x);
    if (std::strcmp(fn, "wrap_pi") == 0)  return detm::wrap_pi(x);
    if (std::strcmp(fn, "wrap_2pi") == 0) return detm::wrap_2pi(x);
    std::printf("UNKNOWN FN: %s\n", fn);
    return 0.0;
}

int main() {
    int fails = 0;
    for (int i = 0; i < detm_vec::VECTOR_COUNT; ++i) {
        const auto& v = detm_vec::VECTORS[i];
        double got = call(v.fn, v.x, v.y);
        if (!bit_equal(got, v.expected)) {
            ++fails;
            std::printf("FAIL %s(%.17g, %.17g): got %a expected %a\n",
                        v.fn, v.x, v.y, got, v.expected);
        }
    }
    if (fails == 0) {
        std::printf("PASS: det_math bit-exact vs reference (%d vectors)\n",
                    detm_vec::VECTOR_COUNT);
        return 0;
    }
    std::printf("RESULT: det_math FAIL (%d mismatches)\n", fails);
    return 1;
}
