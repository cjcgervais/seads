// SEADS loopback lockstep harness (netcode layer 3 — the desync tripwire).
// Mirrors tools/lockstep_ref.py. Two in-process Kernel instances stepped from ONE shared input
// timeline must produce an IDENTICAL per-tick world_hash every tick; the first inequality is a
// desync. This is the invariant a server-authoritative multiplayer loop relies on.
//
// Boundaries (doctrine):
//   * Net code stays OUTSIDE the kernel. The shared timeline carries sim Commands (target bank /
//     climb) — NEVER wire bits. The GEO-001 wire snapshot is lossy by quantization and is never
//     fed back to advance the sim; lockstep compares the CANONICAL hashing snapshot only.
//   * The per-tick hash is SHA-256 of Kernel::snapshot(tick) — the sealed-golden LE-f64 byte
//     layout, UNCHANGED. We only hash it per tick; we never alter the snapshot format.
//   * This harness DRIVES the kernel, so it links seads_kernel + seads_replay. It is therefore
//     its OWN library (seads_lockstep), kept separate from the pure wire codec lib (seads_net,
//     which deliberately links neither det_math nor the kernel).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"  // seads::Kernel, Command, Envelope (from the kernel include dir)

namespace seads {
namespace lockstep {

// Canonical per-tick world_hash: SHA-256 of Kernel::snapshot(tick) (raw LE-f64, the sealed
// byte layout). `tick` is the 1-based count of ticks elapsed (it goes into the snapshot header).
std::string tick_hash(const Kernel& k, std::uint32_t tick);

struct LockstepResult {
    bool in_sync = true;          // true => both kernels agreed on every tick
    std::uint32_t ticks = 0;      // ticks actually stepped (== timeline length if in sync)
    long divergent_tick = -1;     // -1 if in sync, else the first tick (1-based) hashes differed
    std::string hash_a;           // kernel A hash at the last stepped tick (or divergence)
    std::string hash_b;           // kernel B hash at the same tick
};

// Step BOTH kernels one tick from the SAME inputs, hash both canonical snapshots, and report
// whether they match. `tick` is the 1-based tick index for the snapshot header. ha/hb receive
// the two per-tick hashes.
bool apply_inputs(Kernel& a, Kernel& b,
                  const std::vector<Command>& cmd,
                  const std::vector<const Envelope*>& env,
                  std::uint32_t tick,
                  std::string& ha, std::string& hb);

// Drive two independently-built kernels from one shared command timeline (timeline[t] = the
// per-aircraft Command vector for tick t). Stops at the first divergent tick and reports it —
// that tick index is the binary-search handle if a toolchain ever drifts. Optionally appends
// kernel A's per-tick hash to *per_tick_hashes (one entry per tick stepped).
LockstepResult run(Kernel& a, Kernel& b,
                   const std::vector<const Envelope*>& env,
                   const std::vector<std::vector<Command>>& timeline,
                   std::vector<std::string>* per_tick_hashes = nullptr);

}  // namespace lockstep
}  // namespace seads
