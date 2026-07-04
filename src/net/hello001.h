// SEADS HELLO-001 identity-claim handshake codec (ATM-Sphere netcode layer 21). Mirrors
// tools/auth_ref.py BIT-FOR-BIT.
//
// HELLO-001 is the client's ONE-TIME identity claim, sent UP as its FIRST framing frame — the
// upstream mirror of the server's downstream BIND-001 reply. It settles the last honest-scope caveat
// every bidirectional layer flagged: layer 18's client->aircraft binding was POSITIONAL (join-order
// SeatPolicy) and UNAUTHENTICATED. With HELLO-001 the client presents an identity, the server looks it
// up in a pre-shared roster (authserver.h CredentialTable), and BINDS the client to ITS designated
// seat — invariant to join order — or seats it as a spectator when the identity is unknown.
//
// It is TRANSPORT METADATA, not sealed sim state — modelled on BIND-001 / the layer-7 framing
// envelope, NOT on the sealed GEO-001/INPUT-001 wires. It carries NO simulation values, feeds NO hash,
// and is NOT a rails.wire block, so it needs no seal (rides v1.26r0). Cross-impl byte parity is still
// pinned (a Python mirror + a shared known-encoding vector) because the codebase pins every wire — but
// the layer-21 determinism claim rests on the AUTHORIZATION filter (unchanged from layer 18), not on
// this record.
//
// Wire format (a single record, carried as one framing frame on the upstream, sent BEFORE any
// INPUT-001 command frame):
//     HELLO-001 = [version:1 byte = 0x01] [ZigZag+LEB128 token]
// `token` is an opaque i64 credential identifying the client (a production system would carry a
// MAC/signature the server verifies; that is orthogonal crypto — this layer abstracts the credential
// to an i64 the server looks up in a roster). ZigZag carries the sign exactly as BIND-001's seat does.
// The integer reuses the sealed geo001::{encode_i64,decode_i64} pipeline (ZigZag+LEB128) ⇒ no new
// primitive, no det_math.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seads {
namespace hello001 {

constexpr std::uint8_t VERSION = 0x01;  // bumps only if the record shape changes

// One identity-claim record in canonical field order.
struct HelloInfo {
    std::int64_t token = 0;  // opaque credential identifying the client
};

// Encode one record (appends to out) / decode one (advances pos, returns false on a truncated,
// wrong-version, or malformed record). Reuses geo001::{encode_i64,decode_i64}.
void encode_hello(const HelloInfo& h, std::vector<std::uint8_t>& out);
bool decode_hello(const std::uint8_t* data, std::size_t len, std::size_t& pos, HelloInfo& out);

}  // namespace hello001
}  // namespace seads
