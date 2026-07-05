// SEADS authoritative ASYMMETRIC-SIGNATURE server (netcode layer 27). See authsigserver.h.
//
// This is broadcast_authmac's loop (authmacserver.cpp) with the symmetric-MAC verify replaced by an
// Ed25519 SIGNATURE verify: on accept the server sends a fresh CHALLENGE-001 nonce (reused verbatim
// from layer 26), reads the client's HELLO-003 [token, signature], and PubkeyTable::authenticate
// verifies the signature under the enrolled PUBLIC key before binding the seat. BIND-001 +
// seat_authorizes are reused VERBATIM. broadcast_authmac / broadcast_auth / broadcast_bound /
// broadcast_input and the async & catch-up siblings are byte-for-byte UNTOUCHED. Transport only:
// no kernel/det_math, no seal.
#include "authsigserver.h"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include "authmac001.h"  // CHALLENGE-001 codec + derive_nonce (freshness reused verbatim)
#include "authsig001.h"
#include "bind001.h"
#include "framing.h"
#include "input001.h"

namespace seads {
namespace netinput {

void PubkeyTable::enroll(std::int64_t token, std::int64_t seat, const std::uint8_t pubkey[32]) {
    if (seat >= 0 && seat < n_) {
        Entry e;
        e.seat = seat;
        std::memcpy(e.pk, pubkey, 32);
        roster_[token] = e;
    }
}

std::int64_t PubkeyTable::authenticate(std::int64_t token, std::uint64_t nonce,
                                       const std::uint8_t* sig, std::size_t siglen) {
    auto it = roster_.find(token);
    if (it == roster_.end()) return bind001::SPECTATOR;                 // unknown identity
    const Entry& e = it->second;
    if (occupied_[static_cast<std::size_t>(e.seat)]) return bind001::SPECTATOR;  // double-login
    // VERIFY the proof: the presented signature must verify under the enrolled PUBLIC key over
    // (nonce||token). A forgery / wrong key / stale nonce fails here -> SPECTATOR (same class).
    if (!authsig001::verify_challenge(e.pk, nonce, token, sig, siglen)) return bind001::SPECTATOR;
    occupied_[static_cast<std::size_t>(e.seat)] = true;
    return e.seat;
}

void PubkeyTable::release(std::int64_t seat) {
    if (seat >= 0 && seat < n_) occupied_[static_cast<std::size_t>(seat)] = false;
}

namespace {
// One client the server is receiving upstream commands from: its socket, a stream reassembler, and
// its resolved SEAT. Identical to layer 21/26's client — the stronger credential changes only HOW
// the seat is resolved (a verified signature vs a verified MAC vs a bare lookup).
struct AuthSigClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;
    std::int64_t seat = bind001::SPECTATOR;
};
}  // namespace

Stats broadcast_authsig(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                        PubkeyTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                        std::size_t min_initial, int accept_deadline_ms,
                        const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<AuthSigClient> clients;
    std::uint64_t accept_counter = 0;  // increments per accepted socket -> a distinct challenge nonce

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization, unchanged from layers 18/21/26).
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

    // Accept every pending connection with the challenge-response handshake: (a) send a fresh
    // CHALLENGE-001 nonce DOWN; (b) read its HELLO-003 [token, signature] UP; (c) VERIFY the
    // signature under the enrolled public key and the nonce we issued, resolving a seat (or
    // spectator); (d) reply with BIND-001. Any command frames pipelined AFTER the HELLO in the same
    // read are submitted immediately (authorized). A failed challenge send, missing/malformed HELLO,
    // failed BIND, or dropped connection during the handshake drops the joiner (its seat, if it
    // verified one, is returned).
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthSigClient ac;
            ac.s = c;

            // (a) derive + send the CHALLENGE-001 nonce (blocking; a cooperative client reads first).
            std::uint64_t nonce = authmac001::derive_nonce(session_k0, session_k1, accept_counter++);
            {
                authmac001::ChallengeInfo ci{nonce};
                std::vector<std::uint8_t> rec, framed;
                authmac001::encode_challenge(ci, rec);
                framing::encode_frame(rec, framed);
                if (!netsock::send_all(c, framed)) { netsock::close_socket(c); continue; }
            }

            // (b) read upstream until the HELLO-003 record (the first framing frame) completes.
            std::vector<std::vector<std::uint8_t>> upframes;
            std::uint8_t hbuf[4096];
            bool handshake_ok = false, dead = false;
            while (upframes.empty()) {
                if (!netsock::wait_readable(c, accept_deadline_ms)) { dead = true; break; }
                std::ptrdiff_t n = netsock::recv_some(c, hbuf, sizeof(hbuf));
                if (n <= 0) { dead = true; break; }
                if (!ac.rx.feed(hbuf, static_cast<std::size_t>(n), upframes)) { dead = true; break; }
            }
            authsig001::Hello3Info hi;
            if (!dead && !upframes.empty()) {
                std::size_t pos = 0;
                handshake_ok =
                    authsig001::decode_hello3(upframes[0].data(), upframes[0].size(), pos, hi) &&
                    pos == upframes[0].size();
            }
            if (!handshake_ok) { netsock::close_socket(c); continue; }  // no valid HELLO: never a member

            // (c) VERIFY the signature against the issued nonce; resolve a seat (or spectator).
            ac.seat = creds.authenticate(hi.token, nonce, hi.sig.data(), hi.sig.size());
            // (d) TELL the client its seat via BIND-001.
            bind001::BindInfo info{ac.seat, creds.size()};
            std::vector<std::uint8_t> rec, framed;
            bind001::encode_bind(info, rec);
            framing::encode_frame(rec, framed);
            if (!netsock::send_all(c, framed)) {   // handshake failed: never a member
                creds.release(ac.seat);
                netsock::close_socket(c);
                continue;
            }

            // submit any command frames pipelined after the HELLO in the same read (authorized).
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
        for (AuthSigClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop: read upstream commands, step the producer, send the frame down ---
    std::vector<netsock::socket_t> fds, ready;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook

        fds.clear();
        for (const AuthSigClient& c : clients) fds.push_back(c.s);
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

    for (AuthSigClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;
    return st;
}

}  // namespace netinput
}  // namespace seads
