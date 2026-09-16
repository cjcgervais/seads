#include "render/building_asset.h"

#include <cstring>

namespace render {

namespace {
// The header + section-table field offsets (bytes). Little-endian, packed.
constexpr std::size_t kHeaderBytes = 24;   // magic4 + u32 + u64 + u32 + u32
constexpr std::size_t kSectionBytes = 24;  // tag4 + u32 + u64 + u64

// Bounds-safe [off, off+len) within [0, n) with overflow guard.
bool fits(std::size_t off, std::size_t len, std::size_t n) {
    return off <= n && len <= n - off;
}
}  // namespace

BuildingAsset parse_building_asset(const unsigned char* d, std::size_t n) {
    BuildingAsset a;
    if (d == nullptr || !fits(0, kHeaderBytes, n)) return a;
    if (std::memcmp(d, "SBLD", 4) != 0) return a;
    std::uint32_t version = 0;
    std::memcpy(&version, d + 4, 4);
    a.version = version;  // set BEFORE the check so a caller can log "unsupported"
    if (version != kBuildingAssetVersion) return a;
    std::memcpy(&a.lock_hash, d + 8, 8);
    std::memcpy(&a.hero_batch_count, d + 16, 4);
    std::uint32_t nsec = 0;
    std::memcpy(&nsec, d + 20, 4);

    if (!fits(kHeaderBytes, static_cast<std::size_t>(nsec) * kSectionBytes, n)) return a;
    for (std::uint32_t s = 0; s < nsec; ++s) {
        const std::size_t e = kHeaderBytes + static_cast<std::size_t>(s) * kSectionBytes;
        char tag[4];
        std::memcpy(tag, d + e, 4);
        std::uint32_t ss = 0;
        std::uint64_t off = 0, cnt = 0;
        std::memcpy(&ss, d + e + 4, 4);
        std::memcpy(&off, d + e + 8, 8);
        std::memcpy(&cnt, d + e + 16, 8);
        if (cnt == 0) continue;  // empty section: nothing to read (no base ptr formed)
        // element stride * count must sit inside the buffer (overflow-guarded); form
        // `base` ONLY after the offset is validated (Fable P2: d+off with an unchecked
        // off is out-of-object pointer arithmetic = UB even undereferenced).
        if (ss == 0 || cnt > (n / ss)) return a;
        if (!fits(static_cast<std::size_t>(off),
                  static_cast<std::size_t>(ss) * static_cast<std::size_t>(cnt), n))
            return a;
        const unsigned char* base = d + static_cast<std::size_t>(off);

        if (std::memcmp(tag, "VERT", 4) == 0) {
            if (ss != 36) return a;  // 3*f64 + 3*f32, PACKED
            a.verts.resize(static_cast<std::size_t>(cnt));
            for (std::uint64_t i = 0; i < cnt; ++i) {
                const unsigned char* r = base + i * 36;
                GisBuildingVertex& v = a.verts[static_cast<std::size_t>(i)];
                std::memcpy(v.dir, r, 24);       // 3 doubles
                std::memcpy(&v.h, r + 24, 4);
                std::memcpy(&v.bj, r + 28, 4);
                std::memcpy(&v.ws, r + 32, 4);
            }
        } else if (std::memcmp(tag, "IDX ", 4) == 0) {
            if (ss != 2) return a;  // u16
            a.indices.resize(static_cast<std::size_t>(cnt));
            for (std::uint64_t i = 0; i < cnt; ++i)
                std::memcpy(&a.indices[static_cast<std::size_t>(i)], base + i * 2, 2);
        } else if (std::memcmp(tag, "BAT ", 4) == 0) {
            if (ss < 16) return a;  // >= : Stage 2 appends a tile bound; read the prefix
            a.batches.resize(static_cast<std::size_t>(cnt));
            for (std::uint64_t i = 0; i < cnt; ++i) {
                const unsigned char* r = base + i * ss;
                GisBuildingBatch& b = a.batches[static_cast<std::size_t>(i)];
                std::memcpy(&b.vtx_off, r, 4);
                std::memcpy(&b.vtx_count, r + 4, 4);
                std::memcpy(&b.idx_off, r + 8, 4);
                std::memcpy(&b.idx_count, r + 12, 4);
                // Stage-2 tile bound (present iff the record is >= 48 B); an older
                // ss=16 record keeps the never-cull defaults (center +Z, half=pi).
                if (ss >= 48) {
                    std::memcpy(b.center_dir, r + 16, 24);  // 3 doubles
                    std::memcpy(&b.half_angle, r + 40, 8);
                }
            }
        } else if (std::memcmp(tag, "COLL", 4) == 0) {
            if (ss < 32) return a;  // 3*f64 dir + f32 radius + f32 height (packed)
            a.colliders.resize(static_cast<std::size_t>(cnt));
            for (std::uint64_t i = 0; i < cnt; ++i) {
                const unsigned char* r = base + i * ss;
                GisBuildingCollider& col = a.colliders[static_cast<std::size_t>(i)];
                std::memcpy(col.center_dir, r, 24);
                std::memcpy(&col.radius_m, r + 24, 4);
                std::memcpy(&col.height_m, r + 28, 4);
            }
        }
        // Unknown tags are ignored (forward-compatible with Stage 3's COLL section).
    }

    // hero_batch_count comes from the file — clamp it so a bad value can't underflow
    // `batches.size() - hero_batch_count` in a consumer (Fable P3), which would mark
    // every batch a tall "hero" and disarm the town runaway-height tripwire.
    if (a.hero_batch_count > a.batches.size())
        a.hero_batch_count = static_cast<std::uint32_t>(a.batches.size());

    // Fable P0/P1: the section BLOBS are bounds-checked vs the file, but the BAT
    // RECORDS are file data — a corrupt/buggy .bin with valid sections can still
    // carry a batch whose vtx_off/idx_off/counts run past the parsed arrays, or a
    // LOCAL index >= vtx_count, driving an OOB read in build_batch_mesh (app) or the
    // GPU. Validate EVERY batch here so BOTH consumers inherit the guard (the app
    // loader trusted these offsets raw; the unit test checked only the shipped file).
    bool batches_valid = true;
    for (const GisBuildingBatch& b : a.batches) {
        if (b.vtx_off < 0 || b.vtx_count < 0 || b.idx_off < 0 || b.idx_count < 0 ||
            static_cast<std::size_t>(b.vtx_off) + static_cast<std::size_t>(b.vtx_count) >
                a.verts.size() ||
            static_cast<std::size_t>(b.idx_off) + static_cast<std::size_t>(b.idx_count) >
                a.indices.size()) {
            batches_valid = false;
            break;
        }
        for (int k = 0; k < b.idx_count; ++k) {
            if (a.indices[static_cast<std::size_t>(b.idx_off) + k] >= b.vtx_count) {
                batches_valid = false;
                break;
            }
        }
        if (!batches_valid) break;
    }

    a.ok = batches_valid && !a.verts.empty() && !a.batches.empty() && !a.indices.empty();
    return a;
}

}  // namespace render
