// SEADS BIND-001 seat-assignment handshake codec (ATM-Sphere netcode layer 18). Mirrors
// tools/bound_ref.py BIT-FOR-BIT.
//
// BIND-001 is the server's ONE-TIME reply to a joining client: "you are seated in aircraft `seat`
// of a world of `n_aircraft`." It settles layer 15b's positional/unauthenticated binding — a client
// no longer GUESSES which aircraft is its own; the server ASSIGNS one (join-order seat) and TELLS the
// client, which then upstreams INPUT-001 commands only for that seat (the server enforces it).
//
// It is TRANSPORT METADATA, not sealed sim state — modelled on the layer-7 framing envelope, NOT on
// the sealed GEO-001/INPUT-001 wires. It carries NO simulation values, feeds NO hash, and is NOT a
// rails.wire block, so it needs no seal (rides v1.26r0). Cross-impl byte parity is still pinned (a
// Python mirror + a shared known-encoding vector) because the codebase pins every wire — but the
// determinism claim of layer 18 rests on the AUTHORIZATION filter (boundserver.h), not on this record.
//
// Wire format (a single record, itself carried as one framing frame on the downstream, sent BEFORE
// any snapshot frame):
//     BIND-001 = [version:1 byte = 0x01] [ZigZag+LEB128 seat] [ZigZag+LEB128 n_aircraft]
// `seat` is the assigned aircraft index, or -1 for a SPECTATOR (receive-only, no aircraft) — ZigZag
// carries the sign exactly as last_hit_by does on the WEAPON-001 wire. The two integers reuse the
// sealed geo001::{encode_i64,decode_i64} pipeline (ZigZag+LEB128) ⇒ no new primitive, no det_math.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace bind001 {

constexpr std::uint8_t VERSION = 0x01;  // bumps only if the record shape changes
constexpr std::int64_t SPECTATOR = -1;  // seat value for a receive-only client (no aircraft)

// One seat-assignment record in canonical field order.
struct BindInfo {
    std::int64_t seat = SPECTATOR;  // assigned aircraft index, or SPECTATOR
    std::int64_t n_aircraft = 0;    // total seats in the world
};

// Encode one record (appends to out) / decode one (advances pos, returns false on a truncated,
// wrong-version, or malformed record). Reuses geo001::{encode_i64,decode_i64}.
void encode_bind(const BindInfo& b, std::vector<std::uint8_t>& out);
bool decode_bind(const std::uint8_t* data, std::size_t len, std::size_t& pos, BindInfo& out);

}  // namespace bind001
}  // namespace seads
