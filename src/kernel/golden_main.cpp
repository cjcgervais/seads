// SEADS golden runner — produces GOLDEN-SK-Sphere-001 snapshot + world_hash.
// Inputs come from the generated golden_params.h (bit-identical to the Python reference).
//
// Usage:
//   seads_golden [--out run.bin]
// Prints: world_hash=<sha256 hex>
#include "kernel.h"
#include "golden_params.h"
#include "../det_math/det_math.h"
#include "../det_math/detmath_coeffs.h"  // for detm::PI used by deg2rad
#include "../replay/sha256.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace seads;

static double deg2rad(double d) {
    return d * (detm::PI / 180.0);  // same op order as ref_kernel.deg2rad
}

int main(int argc, char** argv) {
    const char* out_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    Rails rails;
    rails.R = golden::R_M;
    rails.dt = golden::DT_S;
    rails.g0 = golden::G0;
    rails.atm_top = golden::ATM_TOP_M;
    rails.soft = golden::SOFT_M;

    Kernel k(rails);
    k.add(deg2rad(golden::START_LAT_DEG), deg2rad(golden::START_LON_DEG),
          deg2rad(golden::START_PSI_DEG), deg2rad(golden::START_PHI_DEG),
          golden::START_ALT_M, golden::START_TAS_MPS);

    k.run(golden::TICKS);
    std::vector<std::uint8_t> snap = k.snapshot(golden::TICKS);
    std::string wh = sha256_hex(snap);

    std::printf("golden=GOLDEN-SK-Sphere-001 ticks=%u\n", golden::TICKS);
    std::printf("final lat=%.17g lon=%.17g psi=%.17g alt=%.17g\n",
                k.lat(0), k.lon(0), k.psi(0), k.alt(0));
    std::printf("world_hash=%s\n", wh.c_str());

    if (out_path) {
        std::FILE* f = std::fopen(out_path, "wb");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", out_path); return 2; }
        std::fwrite(snap.data(), 1, snap.size(), f);
        std::fclose(f);
        std::printf("wrote snapshot -> %s\n", out_path);
    }
    return 0;
}
