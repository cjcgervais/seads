// SEADS authoritative INPUT server (netcode layer 15b) — the FIRST BIDIRECTIONAL netcode layer.
//
// Every layer 5–15a is server->client: the server DRIVES the sealed kernel from a scripted Phase
// schedule and broadcasts snapshots; a client only reads. Layer 15b closes the loop — a client sends
// tick-stamped INPUT-001 Commands UP, and the authoritative kernel steps from them. This is the layer
// that "brushes the determinism rail", so its correctness rests on the ORDERING CONTRACT (cmdqueue.h),
// not on the transport: the kernel's output is a pure function of the DECODED, canonically-ordered
// command SET, so any upstream ORDER / CHUNKING that delivers the same commands before their
// apply_tick yields BIT-IDENTICAL frames (proved by seads_netinput_test). Late arrival is a
// deterministic DROP (drop + hold-last).
//
// Two pieces:
//   * InputProducer — the session::FrameProducer twin whose per-tick Commands come from a CommandQueue
//     (with hold-last) instead of server_command_at. Fed the phase schedule AS commands (grid-exact,
//     so the lossy wire round-trips identically) it emits frames BYTE-IDENTICAL to build_server_frames
//     — the anchor tying layer 15b to the trusted session machinery.
//   * broadcast_input — a single-thread select() server that, each iteration, reads upstream bytes
//     from every client (framing::StreamReassembler -> input001::decode_command -> CommandQueue),
//     steps the InputProducer one frame, and sends that frame downstream. Accepted client sockets stay
//     BLOCKING and the downstream send is send_all (a cooperative client reads concurrently); the
//     back-pressure / cap / liveness machinery is deliberately layer 11–15a's concern, kept out of
//     this first bidirectional cut. TRANSPORT: outside the world_hash, no det_math.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "kernel.h"     // seads::Kernel, Rails, Command, Envelope
#include "cmdqueue.h"   // seads::netinput::CommandQueue
#include "input001.h"   // seads::input001::InputCommand
#include "session.h"    // seads::session::{Scenario, serialize_world}
#include "socket.h"     // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Input-driven authoritative producer: the session::FrameProducer op sequence, but each tick's
// per-aircraft Command is the CommandQueue's winner for (pre-step tick, aircraft), or — with no such
// command — that aircraft's last applied Command (hold-last). Aircraft init (envelope, start state,
// tick/cadence) comes from the session::Scenario; the Phase schedule is IGNORED (commands drive it).
class InputProducer {
public:
    InputProducer(const Rails& rails, const session::Scenario& sc, CommandQueue& queue);
    // Fill (emit_tick, payload) with the next frame — tick-0 pre-step world first, then one every
    // snap_every ticks — and return true; false when the scenario is exhausted (outputs untouched).
    bool next(std::int64_t& emit_tick, std::vector<std::uint8_t>& payload);

    // The world size (aircraft count). Additive accessor used by broadcast_bound (layer 18) to size
    // its SeatPolicy; broadcast_input/bidi ignore it. The producer, queue, and seat pool share this n.
    std::int64_t n_aircraft() const { return static_cast<std::int64_t>(sc_->n_aircraft); }

private:
    const session::Scenario* sc_;
    CommandQueue* q_;
    Kernel server_;
    std::vector<Command> held_;               // hold-last per aircraft (init neutral: wings level, 1 g)
    std::vector<const Envelope*> envs_;
    unsigned t_ = 0;
    bool emitted_initial_ = false;
};

struct Stats {
    std::size_t frames_sent = 0;
    std::size_t joins = 0;
    std::size_t leaves = 0;
    std::size_t cmds_ok = 0;     // commands accepted into the queue
    std::size_t cmds_stale = 0;  // commands dropped: apply_tick already stepped past (the drop policy)
    std::size_t cmds_oob = 0;    // commands dropped: aircraft index out of range
    std::size_t capped = 0;      // layer-16 downstream hygiene: clients shed for a pending backlog over
                                 // cap_bytes (drop-slowest; also a leave). 0 for broadcast_input.
    std::size_t reaped = 0;      // layer-16 downstream hygiene: clients reaped for no receive progress
                                 // over liveness_frames produced frames (also a leave). 0 for broadcast_input.
    std::size_t cmds_unauth = 0; // layer-18 binding: commands dropped for naming a foreign aircraft
                                 // (not the sender's seat) or coming from a spectator. 0 for broadcast_input/bidi.
    bool ok = false;
};

// Run the authoritative bidirectional server: gather `min_initial` clients (bounded by
// accept_deadline_ms), then for each produced frame read upstream commands, step the producer, and
// broadcast the frame downstream. `on_frame(fi)` fires at the TOP of iteration fi (before the upstream
// read + the step) — the test rendezvous hook (block there until a client has sent its commands, so
// they are ingested before their apply_tick). Returns Stats; ok iff the whole stream was produced.
Stats broadcast_input(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame = {});

// Convert a session::Scenario's per-aircraft Phase schedules into the equivalent INPUT-001 command
// list (one command per phase, apply_tick = phase.start_tick, seq = phase ordinal). Fed through the
// CommandQueue's hold-last this reproduces session::server_command_at for every tick — the bridge's
// canonical command set. Ascending (apply_tick, aircraft); a client/test may deliver them in any order.
std::vector<input001::InputCommand> commands_from_scenario(const session::Scenario& sc);

}  // namespace netinput
}  // namespace seads
