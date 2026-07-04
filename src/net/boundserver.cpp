// SEADS authoritative BOUND server (netcode layer 18). See boundserver.h.
//
// This is broadcast_input's loop (inputserver.cpp) with three transport additions: a join-order
// SeatPolicy, a one-time BIND-001 handshake per client, and upstream authorization (a client may only
// command its own seat). broadcast_input / broadcast_bidi are byte-for-byte UNTOUCHED — this is a
// sibling, matching the codebase's layer discipline. Transport only: no kernel/det_math, no seal.
#include "boundserver.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "bind001.h"
#include "framing.h"
#include "input001.h"

namespace seads {
namespace netinput {

std::int64_t SeatPolicy::assign() {
    for (std::size_t i = 0; i < occupied_.size(); ++i) {
        if (!occupied_[i]) {
            occupied_[i] = true;
            return static_cast<std::int64_t>(i);
        }
    }
    return bind001::SPECTATOR;
}

void SeatPolicy::release(std::int64_t seat) {
    if (seat >= 0 && seat < static_cast<std::int64_t>(occupied_.size()))
        occupied_[static_cast<std::size_t>(seat)] = false;
}

namespace {
// One client the server is receiving upstream commands from: its socket, a stream reassembler that
// turns arbitrary byte chunks back into whole INPUT-001 records, and its assigned SEAT (aircraft
// index, or bind001::SPECTATOR). The seat is set at accept and returned to the pool on leave.
struct BoundClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;
    std::int64_t seat = bind001::SPECTATOR;
};
}  // namespace

Stats broadcast_bound(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<BoundClient> clients;
    // The seat pool is sized to the world (the producer, queue, and seats share one n_aircraft).
    SeatPolicy seats(producer.n_aircraft());

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization). A foreign-aircraft command — or any
    // command from a spectator — is dropped as unauthorized (byte-identical to never arriving).
    auto submit_payloads = [&](std::int64_t seat,
                               const std::vector<std::vector<std::uint8_t>>& payloads) {
        for (const auto& p : payloads) {
            input001::InputCommand ic;
            std::size_t pos = 0;
            if (!input001::decode_command(p.data(), p.size(), pos, ic)) continue;  // skip malformed
            if (!seat_authorizes(seat, ic.aircraft)) { ++st.cmds_unauth; continue; }
            switch (queue.submit(ic)) {
                case CommandQueue::Result::ACCEPTED: ++st.cmds_ok; break;
                case CommandQueue::Result::STALE: ++st.cmds_stale; break;
                case CommandQueue::Result::OUT_OF_RANGE: ++st.cmds_oob; break;
            }
        }
    };

    // Accept every pending connection: assign a join-order seat and send the BIND-001 handshake as the
    // client's first downstream framing frame (before any snapshot). A failed handshake send drops the
    // joiner immediately (its seat is returned). Blocking send of a tiny record: a cooperative client
    // reads it first.
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            BoundClient bc;
            bc.s = c;
            bc.seat = seats.assign();
            bind001::BindInfo info{bc.seat, seats.size()};
            std::vector<std::uint8_t> rec, framed;
            bind001::encode_bind(info, rec);
            framing::encode_frame(rec, framed);
            if (!netsock::send_all(c, framed)) {   // handshake failed: never a member
                seats.release(bc.seat);
                netsock::close_socket(c);
                continue;
            }
            clients.push_back(std::move(bc));
            ++st.joins;
        }
    };

    // Resolve the seat pool size: the CommandQueue was constructed with n_aircraft; recover it by
    // building the SeatPolicy from the producer's aircraft count exposed via the queue. Since neither
    // exposes it directly here, the caller guarantees the queue and producer share one n_aircraft; we
    // read it from the queue by probing OUT_OF_RANGE boundaries would be ugly, so require it via the
    // producer: InputProducer::n_aircraft().
    seats = SeatPolicy(producer.n_aircraft());

    // --- gather the initial clients (bounded wait) before frame 0 -------------------------------
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100)) accept_all();
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (BoundClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop: read upstream commands, step the producer, send the frame down ---
    std::vector<netsock::socket_t> fds, ready;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook: block until clients have sent their commands

        // service membership + upstream, one poll (timeout 0). A readable client either sent command
        // bytes (n>0, authorized into the queue) or closed (n<=0 => LEAVE, seat returned). Drain all
        // currently-available bytes so a whole command burst is ingested before the ticks it governs.
        fds.clear();
        for (const BoundClient& c : clients) fds.push_back(c.s);
        fds.push_back(listener);
        if (netsock::select_readable(fds, 0, ready)) {
            bool listener_ready = false;
            for (netsock::socket_t r : ready)
                if (r == listener) { listener_ready = true; break; }
            if (listener_ready) accept_all();
            for (std::size_t i = clients.size(); i-- > 0;) {
                netsock::socket_t s = clients[i].s;
                bool is_ready = false;
                for (netsock::socket_t r : ready)
                    if (r == s) { is_ready = true; break; }
                if (!is_ready) continue;
                bool leave = false;
                do {
                    std::ptrdiff_t n = netsock::recv_some(s, rbuf, sizeof(rbuf));
                    if (n <= 0) { leave = true; break; }  // clean EOF / error: LEAVE
                    pv.clear();
                    if (!clients[i].rx.feed(rbuf, static_cast<std::size_t>(n), pv)) {
                        leave = true;  // malformed framing: drop the client
                        break;
                    }
                    submit_payloads(clients[i].seat, pv);
                } while (netsock::wait_readable(s, 0));
                if (leave) {
                    seats.release(clients[i].seat);
                    netsock::close_socket(s);
                    clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
                    ++st.leaves;
                }
            }
        }

        // produce the next authoritative frame (steps the sim, consuming this batch's commands)
        std::int64_t emit_tick = 0;
        if (!producer.next(emit_tick, payload)) break;

        // broadcast it downstream (blocking send_all; a cooperative client reads concurrently)
        frame.clear();
        framing::encode_frame(payload, frame);
        for (std::size_t i = clients.size(); i-- > 0;) {
            if (!netsock::send_all(clients[i].s, frame)) {
                seats.release(clients[i].seat);
                netsock::close_socket(clients[i].s);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
                ++st.leaves;
            }
        }
        ++st.frames_sent;
    }

    for (BoundClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;
    return st;
}

}  // namespace netinput
}  // namespace seads
