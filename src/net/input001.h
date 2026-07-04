// SEADS INPUT-001 upstream Command codec (ATM-Sphere netcode layer 15b). Mirrors
// tools/input001_ref.py BIT-FOR-BIT.
//
// INPUT-001 is the FIRST UPSTREAM wire: a client encodes a tick-stamped flight Command and sends it
// back to the authoritative server, which feeds it into the sealed kernel step. Every wire before
// this one (GEO-001 / KIN-002 / WEAPON-001) is DOWNSTREAM state; this one is INPUT.
//
// Like every SEADS wire it is a SEALED RAIL (config/rails/atm.json -> rails.wire.command): quantize
// the command's continuous fields to fixed-point i64, then ZigZag + LEB128 (the SAME geo001 pipeline
// — no new det_math, no transcendentals). It lives OUTSIDE the kernel and the world_hash. It is LOSSY
// by quantization exactly like the downstream wires; the determinism guarantee (see cmdqueue.h /
// ADR-Step-Net-Layer15b) is NOT "the wire is lossless" but "the authoritative kernel's output is a
// pure function of the DECODED, canonically-ordered command SET, independent of transport order/
// chunking". A client that wants a bit-exact server outcome sends grid-exact (dyadic) command values.
//
// Field order (contract): apply_tick, aircraft, seq, target_phi, target_g, throttle, fire.
//   * apply_tick — the PRE-step tick this command governs (applied on the step advancing
//     apply_tick -> apply_tick+1, the same convention as session::server_command_at(a, t-1)).
//   * aircraft   — SoA aircraft index the command steers (server binds a client to its aircraft).
//   * seq        — client-assigned monotone tag; the canonical tiebreak when two commands share an
//     (apply_tick, aircraft) so selection is a pure function of the SET, never of arrival order.
//   * target_phi — commanded bank, RADIANS (x1e6, like the KIN-002 angle convention).
//   * target_g   — commanded load factor n (x1e6).
//   * throttle   — [0,1] (x1e6).
//   * fire       — gun trigger (0/1).
// apply_tick/aircraft/seq are exact integers (ZigZag+LEB128, not quantized); the three continuous
// fields quantize; fire is a 0/1 integer.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace input001 {

// Sealed INPUT-001 scales (must match config/rails/atm.json rails.wire.command).
constexpr int64_t PHI_SCALE      = 1000000;  // 1e6  target_phi, radians
constexpr int64_t TARGETG_SCALE  = 1000000;  // 1e6  target_g, load factor
constexpr int64_t THROTTLE_SCALE = 1000000;  // 1e6  throttle, [0,1]

// One upstream flight command in canonical field order.
struct InputCommand {
    int64_t apply_tick = 0;
    int64_t aircraft   = 0;
    int64_t seq        = 0;
    double  target_phi = 0.0;  // radians
    double  target_g   = 1.0;
    double  throttle   = 0.0;
    bool    fire       = false;
};

// Encode one command (appends to out) / decode one (advances pos, returns false on a truncated or
// malformed record). Reuses geo001::{quantize,encode_i64,decode_i64}.
void encode_command(const InputCommand& c, std::vector<uint8_t>& out);
bool decode_command(const uint8_t* data, size_t len, size_t& pos, InputCommand& out);

}  // namespace input001
}  // namespace seads
