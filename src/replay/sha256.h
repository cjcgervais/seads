// Minimal SHA-256 (FIPS 180-4) for SEADS world_hash. Self-contained, deterministic.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace seads {
std::string sha256_hex(const std::vector<std::uint8_t>& data);
}
