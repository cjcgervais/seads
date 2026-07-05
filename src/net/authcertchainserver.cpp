// SEADS authoritative CERTIFICATE-CHAIN server (netcode layer 33). See authcertchainserver.h.
//
// This is broadcast_authcert's loop (authcertserver.cpp) with the SINGLE-certificate verify replaced
// by a certificate-CHAIN PATH verify: on accept the server sends a fresh CHALLENGE-001 nonce (reused
// verbatim from layers 26/27/30), reads the client's HELLO-005 [chain, challenge_signature], and
// CaChainTable::authenticate validates the path to the trusted root (cert001::verify_chain), the
// revocation + epoch floors on EVERY link, and the possession proof under the LEAF key before binding
// the seat. BIND-001 + seat_authorizes are reused VERBATIM. broadcast_authcert / broadcast_authsig /
// broadcast_auth / broadcast_bound / broadcast_input and the async & catch-up siblings are byte-for-
// byte UNTOUCHED. Transport only: no kernel/det_math, no seal.
#include "authcertchainserver.h"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include "authmac001.h"  // CHALLENGE-001 codec + derive_nonce (freshness reused verbatim)
#include "authsig001.h"  // verify_challenge (the possession proof over nonce||token, reused)
#include "bind001.h"
#include "cert001.h"
#include "framing.h"
#include "input001.h"

namespace seads {
namespace netinput {

CaChainTable::CaChainTable(std::int64_t n_aircraft, const std::uint8_t root_pubkey[32],
                           std::size_t max_depth)
    : n_(n_aircraft), max_depth_(max_depth),
      occupied_(static_cast<std::size_t>(n_aircraft), false) {
    std::memcpy(root_pk_, root_pubkey, 32);
}

void CaChainTable::revoke(std::int64_t token) { revoked_.insert(token); }
void CaChainTable::unrevoke(std::int64_t token) { revoked_.erase(token); }
void CaChainTable::set_min_epoch(std::int64_t token, std::int64_t epoch) { min_epoch_[token] = epoch; }

std::int64_t CaChainTable::authenticate(const std::vector<std::vector<std::uint8_t>>& chain,
                                        std::uint64_t nonce, const std::uint8_t* challenge_sig,
                                        std::size_t siglen) {
    // Decode every link (leaf first). A malformed / trailing-garbage link rejects the whole chain.
    std::vector<cert001::CertInfo> links;
    links.reserve(chain.size());
    for (const auto& raw : chain) {
        cert001::CertInfo ci;
        std::size_t pos = 0;
        if (!cert001::decode_cert(raw.data(), raw.size(), pos, ci) || pos != raw.size())
            return bind001::SPECTATOR;  // malformed link
        links.push_back(std::move(ci));
    }
    // Validate the signature PATH to the trusted root (bounds the depth too).
    if (!cert001::verify_chain(root_pk_, links, max_depth_)) return bind001::SPECTATOR;
    // Revocation + epoch floor apply to EVERY link (a revoked/rotated intermediate kills its subtree).
    for (const cert001::CertInfo& link : links) {
        if (revoked_.count(link.token)) return bind001::SPECTATOR;                    // revoked identity
        auto fe = min_epoch_.find(link.token);
        if (fe != min_epoch_.end() && link.epoch < fe->second) return bind001::SPECTATOR;  // rotated out
    }
    // The LEAF (chain[0]) governs the seat + possession.
    const cert001::CertInfo& leaf = links.front();
    if (leaf.seat < 0 || leaf.seat >= n_) return bind001::SPECTATOR;                  // non-existent seat
    if (occupied_[static_cast<std::size_t>(leaf.seat)]) return bind001::SPECTATOR;    // double-login
    if (!authsig001::verify_challenge(leaf.pubkey, nonce, leaf.token, challenge_sig, siglen))
        return bind001::SPECTATOR;                                                    // bad possession proof
    occupied_[static_cast<std::size_t>(leaf.seat)] = true;
    return leaf.seat;
}

void CaChainTable::release(std::int64_t seat) {
    if (seat >= 0 && seat < n_) occupied_[static_cast<std::size_t>(seat)] = false;
}

namespace {
// One client the server is receiving upstream commands from: its socket, a stream reassembler, and its
// resolved SEAT. Identical to layer 30's client — the credential changes only HOW the seat is resolved
// (a verified certificate CHAIN vs a single verified certificate).
struct AuthCertChainClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;
    std::int64_t seat = bind001::SPECTATOR;
};
}  // namespace

Stats broadcast_authcertchain(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                              CaChainTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                              std::size_t min_initial, int accept_deadline_ms,
                              const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<AuthCertChainClient> clients;
    std::uint64_t accept_counter = 0;  // increments per accepted socket -> a distinct challenge nonce

    // Decode one client's burst of reassembled upstream records into the CommandQueue, but ONLY for
    // commands naming this client's own seat (authorization, unchanged from layers 18/21/26/27/30).
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
    // CHALLENGE-001 nonce DOWN; (b) read its HELLO-005 [chain, challenge_signature] UP; (c) VALIDATE
    // the certificate PATH to the root (+ revocation + epoch floor on every link) and the possession
    // proof under the leaf key, resolving a seat (or spectator); (d) reply with BIND-001. Any command
    // frames pipelined AFTER the HELLO in the same read are submitted immediately (authorized). A
    // failed challenge send, missing/malformed HELLO, failed BIND, or dropped connection during the
    // handshake drops the joiner (its seat, if it verified one, is returned).
    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            AuthCertChainClient ac;
            ac.s = c;

            // (a) derive + send the CHALLENGE-001 nonce (blocking; a cooperative client reads first).
            std::uint64_t nonce = authmac001::derive_nonce(session_k0, session_k1, accept_counter++);
            {
                authmac001::ChallengeInfo cinfo{nonce};
                std::vector<std::uint8_t> rec, framed;
                authmac001::encode_challenge(cinfo, rec);
                framing::encode_frame(rec, framed);
                if (!netsock::send_all(c, framed)) { netsock::close_socket(c); continue; }
            }

            // (b) read upstream until the HELLO-005 record (the first framing frame) completes.
            std::vector<std::vector<std::uint8_t>> upframes;
            std::uint8_t hbuf[4096];
            bool handshake_ok = false, dead = false;
            while (upframes.empty()) {
                if (!netsock::wait_readable(c, accept_deadline_ms)) { dead = true; break; }
                std::ptrdiff_t n = netsock::recv_some(c, hbuf, sizeof(hbuf));
                if (n <= 0) { dead = true; break; }
                if (!ac.rx.feed(hbuf, static_cast<std::size_t>(n), upframes)) { dead = true; break; }
            }
            cert001::Hello5Info hi;
            if (!dead && !upframes.empty()) {
                std::size_t pos = 0;
                handshake_ok =
                    cert001::decode_hello5(upframes[0].data(), upframes[0].size(), pos, hi) &&
                    pos == upframes[0].size();
            }
            if (!handshake_ok) { netsock::close_socket(c); continue; }  // no valid HELLO: never a member

            // (c) VALIDATE the chain path + possession proof against the issued nonce; resolve a seat.
            ac.seat = creds.authenticate(hi.certs, nonce, hi.sig.data(), hi.sig.size());
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
        for (AuthCertChainClient& c : clients) netsock::close_socket(c.s);
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
        for (const AuthCertChainClient& c : clients) fds.push_back(c.s);
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

    for (AuthCertChainClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;
    return st;
}

}  // namespace netinput
}  // namespace seads
