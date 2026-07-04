// SEADS authoritative AUTHENTICATED-BINDING server (netcode layer 21). See authserver.h.
//
// This is broadcast_bound's loop (boundserver.cpp) with the join-order SeatPolicy replaced by an
// identity handshake: on accept the server reads the client's HELLO-001 credential, looks it up in a
// CredentialTable to a DESIGNATED seat (invariant to join order; unknown token -> spectator), and
// replies with the SAME BIND-001 record + the SAME per-seat authorization as layer 18. broadcast_bound
// / broadcast_input / the async & catch-up siblings are byte-for-byte UNTOUCHED — this is a sibling,
// matching the codebase's layer discipline. Transport only: no kernel/det_math, no seal.
#include "authserver.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "bind001.h"
#include "framing.h"
#include "hello001.h"
#include "input001.h"

namespace seads {
namespace netinput {

void CredentialTable::enroll(std::int64_t token, std::int64_t seat) {
    if (seat >= 0 && seat < n_) roster_[token] = seat;
}

std::int64_t CredentialTable::authenticate(std::int64_t token) {
    auto it = roster_.find(token);
    if (it == roster_.end()) return bind001::SPECTATOR;             // unknown identity: no aircraft
    std::int64_t seat = it->second;
    if (occupied_[static_cast<std::size_t>(seat)]) return bind001::SPECTATOR;  // double-login
    occupied_[static_cast<std::size_t>(seat)] = true;
    return seat;
}

void CredentialTable::release(std::int64_t seat) {
    if (seat >= 0 && seat < n_) occupied_[static_cast<std::size_t>(seat)] = false;
}

namespace {
// One client the server is receiving upstream commands from: its socket, a stream reassembler that
// turns arbitrary byte chunks back into whole framing records, and its resolved SEAT (aircraft index,
// or bind001::SPECTATOR). The seat is resolved from the HELLO handshake and returned to the pool on
// leave. Identical to layer 18's BoundClient — authentication changes only how the seat is chosen.
struct AuthClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;
    std::int64_t seat = bind001::SPECTATOR;
};
}  // namespace

Stats broadcast_auth(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                     CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                     const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<AuthClient> clients;

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization, unchanged from layer 18). A foreign
    // aircraft — or any command from a spectator — is dropped as unauthorized (byte-identical to never
    // arriving; the same class as the queue's OUT_OF_RANGE reject).
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

    // Accept every pending connection: read its HELLO-001 (the FIRST upstream framing frame),
    // authenticate the token to a seat, and reply with the BIND-001 handshake as the client's first
    // downstream framing frame (before any snapshot). Any command frames pipelined AFTER the HELLO in
    // the same read are submitted immediately (authorized to the resolved seat). A missing/malformed
    // HELLO, a failed BIND send, or a dropped connection during the handshake drops the joiner (its
    // seat, if it took one, is returned). Blocking handshake reads/sends of tiny records: a cooperative
    // client sends its HELLO first and reads its BIND first.
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthClient ac;
            ac.s = c;

            // 1) read upstream until the HELLO-001 record (the first framing frame) completes.
            std::vector<std::vector<std::uint8_t>> upframes;
            std::uint8_t hbuf[4096];
            bool handshake_ok = false, dead = false;
            while (upframes.empty()) {
                if (!netsock::wait_readable(c, accept_deadline_ms)) { dead = true; break; }
                std::ptrdiff_t n = netsock::recv_some(c, hbuf, sizeof(hbuf));
                if (n <= 0) { dead = true; break; }
                if (!ac.rx.feed(hbuf, static_cast<std::size_t>(n), upframes)) { dead = true; break; }
            }
            hello001::HelloInfo hi;
            if (!dead && !upframes.empty()) {
                std::size_t pos = 0;
                handshake_ok = hello001::decode_hello(upframes[0].data(), upframes[0].size(), pos, hi) &&
                               pos == upframes[0].size();
            }
            if (!handshake_ok) { netsock::close_socket(c); continue; }  // no valid HELLO: never a member

            // 2) authenticate the credential to a seat (or spectator) and TELL the client via BIND-001.
            ac.seat = creds.authenticate(hi.token);
            bind001::BindInfo info{ac.seat, creds.size()};
            std::vector<std::uint8_t> rec, framed;
            bind001::encode_bind(info, rec);
            framing::encode_frame(rec, framed);
            if (!netsock::send_all(c, framed)) {   // handshake failed: never a member
                creds.release(ac.seat);
                netsock::close_socket(c);
                continue;
            }

            // 3) submit any command frames pipelined after the HELLO in the same read (authorized).
            if (upframes.size() > 1) {
                std::vector<std::vector<std::uint8_t>> tail(upframes.begin() + 1, upframes.end());
                submit_payloads(ac.seat, tail);
            }
            clients.push_back(std::move(ac));
            ++st.joins;
        }
    };

    // --- gather the initial clients (bounded wait) before frame 0 -------------------------------
    int waited = 0;
    while (clients.size() < min_initial && waited < accept_deadline_ms) {
        if (netsock::wait_readable(listener, 100)) accept_all();
        waited += 100;
    }
    if (clients.size() < min_initial) {
        for (AuthClient& c : clients) netsock::close_socket(c.s);
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
        for (const AuthClient& c : clients) fds.push_back(c.s);
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
                    creds.release(clients[i].seat);
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
                creds.release(clients[i].seat);
                netsock::close_socket(clients[i].s);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
                ++st.leaves;
            }
        }
        ++st.frames_sent;
    }

    for (AuthClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;
    return st;
}

}  // namespace netinput
}  // namespace seads
