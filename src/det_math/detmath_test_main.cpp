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

static double call(const char* fn, double x) {
    if (std::strcmp(fn, "sin") == 0)  return detm::det_sin(x);
    if (std::strcmp(fn, "cos") == 0)  return detm::det_cos(x);
    if (std::strcmp(fn, "tan") == 0)  return detm::det_tan(x);
    if (std::strcmp(fn, "atan") == 0) return detm::det_atan(x);
    if (std::strcmp(fn, "asin") == 0) return detm::det_asin(x);
    if (std::strcmp(fn, "sqrt") == 0) return detm::det_sqrt(x);
    return 0.0;
}

int main() {
    int fails = 0;
    for (int i = 0; i < detm_vec::VECTOR_COUNT; ++i) {
        const auto& v = detm_vec::VECTORS[i];
        double got = call(v.fn, v.x);
        if (!bit_equal(got, v.expected)) {
            ++fails;
            std::printf("FAIL %s(%.17g): got %a expected %a\n", v.fn, v.x, got, v.expected);
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
