// SEADS CERTIFICATE-PKI determinism BRIDGE for the authoritative CA-trust auth server (LAYER 30).
//
// Layer 27 (broadcast_authsig) bound each seat to a per-token PUBLIC key ENROLLED on the server. Real
// public-key identity, but the roster is FIXED: no enrollment without a server change, no revocation,
// no key rotation. Layer 30 (broadcast_authcert) moves trust to a CERTIFICATE AUTHORITY: the server
// holds ONE CA public key; each client carries a CA-signed CERT-001 certificate binding its (token,
// seat, epoch, client_pubkey), and answers the challenge with HELLO-004 = [certificate,
// challenge_signature]. CaTable::authenticate verifies the certificate under the CA key (+ a
// revocation set + a per-token epoch floor) and the possession proof under the CERTIFIED key before
// binding the seat; any failure -> SPECTATOR (the layer-21/26/27 reject class). This bridge proves,
// over real 127.0.0.1 sockets with NO sleeps / NO timing guesses, three claims:
//
//   LEG 1 (CA-CERTIFIED SEATS, INVARIANT TO JOIN ORDER — the headline): THREE clients each hold a
//   distinct CA-issued certificate AND the matching PRIVATE seed, are challenged, present the
//   certificate + SIGN, and are bound to the seat their certificate designates — deliberately token
//   order != seat order (100->2, 200->0, 300->1). Each upstreams ONLY that seat's commands (SCRAMBLED
//   + adversarially chunked); their union is the whole INPUT-SK-001 command set, so the produced
//   downstream frames are BYTE-IDENTICAL to session::build_server_frames. The server enrolled only
//   the ONE CA public key — no per-client roster.
//
//   LEG 2 (CA VERIFY + REVOCATION + ROTATION BOUNDARY — the new capabilities): FOUR clients upstream
//   the WHOLE scenario. Client A holds a genuine token-200 certificate + correct seed -> seat 0; its
//   foreign-aircraft commands are dropped. Client B presents a SELF-SIGNED certificate (minted with
//   an attacker key, not the CA) -> non-CA -> SPECTATOR. Client C holds a genuine, correctly-signed
//   token-300 certificate that the server has REVOKED -> SPECTATOR. Client D holds a genuine token-100
//   certificate whose epoch is STALE after the server rotated that identity's key (epoch floor raised)
//   -> SPECTATOR. All four receive the IDENTICAL aircraft-0-only world byte-for-byte; only A's own-seat
//   commands ever move the sim.
//
//   LEG 3 (SHA-512 + Ed25519 pins + CERT-001/HELLO-004 codec + CaTable verify — in-process): the
//   OFFICIAL SHA-512("abc") vector, the Ed25519 fixed-seed pin, the CERT-001 body + HELLO-004
//   known-encoding pins (vs tools/cert_ref.py) + round-trip + wrong-version reject, and CaTable
//   (CA-verify, self-signed reject, forged-possession reject, replay reject, revoke/un-revoke,
//   epoch-floor rotation, unknown->spectator, double-login->spectator, reclaim-own-seat) + the reused
//   seat_authorizes predicate — no sockets, no timing.
//
// Every socket assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; CHALLENGE-001/HELLO-004/CERT-001/BIND-001 are
// transport metadata (modelled on the framing envelope, NOT a sealed rails.wire block) => no seal.
// Rides v1.26r0.
#include "authcertserver.h"
#include "cert001.h"
#include "authsig001.h"
#include "authmac001.h"
#include "ed25519.h"
#include "sha512.h"
#include "boundserver.h"
#include "inputserver.h"
#include "input001.h"
#include "bind001.h"
#include "cmdqueue.h"
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "socket.h"
#include "golden_params.h"
#include "kernel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using namespace seads;

static Rails sealed_rails() {
    Rails r;
    r.R = golden::R_M;
    r.dt = golden::DT_S;
    r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M;
    r.soft = golden::SOFT_M;
    return r;
}

// The server's session key (seeds the per-connection challenge nonce). FIXED here so the run is
// reproducible. Same values as the layer-26/27 bridge (the challenge is reused).
static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact 3-aircraft scenario the layer-15b..27 bridges use.
// ---------------------------------------------------------------------------------------------
static const session::Phase IN_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase IN_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase IN_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec IN_AC[3];
static const session::Scenario& input_scenario() {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) IN_AC[i] = sess_vec::AIRCRAFT[i];
        IN_AC[0].sched = IN_P0; IN_AC[0].n_phase = 3;
        IN_AC[1].sched = IN_P1; IN_AC[1].n_phase = 2;
        IN_AC[2].sched = IN_P2; IN_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    static const session::Scenario sc = {
        IN_AC, 3u, /*ticks=*/150u, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
        /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

// A deterministic 32-byte private seed from a base byte (a stand-in for a client's key material).
static std::array<std::uint8_t, 32> make_seed(std::uint8_t base) {
    std::array<std::uint8_t, 32> s;
    for (int i = 0; i < 32; ++i) s[i] = static_cast<std::uint8_t>((base + i) & 0xFF);
    return s;
}

// The certificate authority: ONE key pair. Only its PUBLIC key reaches the server.
static const std::uint8_t CA_BASE = 0xC0;
static std::array<std::uint8_t, 32> ca_public() {
    auto seed = make_seed(CA_BASE);
    std::array<std::uint8_t, 32> pk{};
    ed25519::public_key(seed.data(), pk.data());
    return pk;
}

// Issue a certificate with the CA seed for (token, seat, epoch) over the client's public key
// (derived from the client's private base seed).
static std::vector<std::uint8_t> issue(std::int64_t token, std::int64_t seat, std::int64_t epoch,
                                       std::uint8_t client_base) {
    auto ca = make_seed(CA_BASE);
    auto cseed = make_seed(client_base);
    std::uint8_t cpub[32];
    ed25519::public_key(cseed.data(), cpub);
    std::vector<std::uint8_t> cert;
    cert001::issue_cert(ca.data(), token, seat, epoch, cpub, cert);
    return cert;
}

// The pre-shared roster: token -> (designated seat, PRIVATE base seed). Token order != seat order.
// The server holds only the CA public key; a client that holds the matching PRIVATE seed AND a
// CA-issued certificate can seat.
struct Identity { std::int64_t token; std::int64_t seat; std::uint8_t base; };
static const Identity ROSTER[] = {
    {100, 2, 0x10},
    {200, 0, 0x20},
    {300, 1, 0x30},
};

static std::vector<std::vector<std::uint8_t>> payloads_of(const session::ServerFrames& f) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const auto& p : f) out.push_back(p.second);
    return out;
}

static std::vector<input001::InputCommand> filter_by_aircraft(
    const std::vector<input001::InputCommand>& all, std::int64_t aircraft) {
    std::vector<input001::InputCommand> out;
    for (const auto& c : all) if (c.aircraft == aircraft) out.push_back(c);
    return out;
}

static std::vector<std::vector<std::uint8_t>> run_input_frames(
    const Rails& rails, const session::Scenario& sc,
    const std::vector<input001::InputCommand>& cmds) {
    netinput::CommandQueue q(sc.n_aircraft);
    for (const auto& c : cmds) q.submit(c);
    netinput::InputProducer producer(rails, sc, q);
    std::vector<std::vector<std::uint8_t>> frames;
    std::int64_t emit_tick = 0;
    std::vector<std::uint8_t> payload;
    while (producer.next(emit_tick, payload)) frames.push_back(payload);
    return frames;
}

static bool frames_equal(const std::vector<std::vector<std::uint8_t>>& a,
                         const std::vector<std::vector<std::uint8_t>>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// ---------------------------------------------------------------------------------------------
// A generic socket leg: N clients. Each holds a TOKEN, the private SEED it USES to sign the challenge
// (which may differ from the certified key => a stolen certificate), and the CERTIFICATE bytes it
// presents (which may be self-signed / stale). Given the seat the server binds it (learned from BIND,
// after the handshake), a plan builds the commands it upstreams, plus a scramble/chunking. The server
// runs broadcast_authcert(min_initial = N) over a CaTable configured by `configure` (revocation /
// rotation applied before any client connects).
// ---------------------------------------------------------------------------------------------
struct ClientPlan {
    std::int64_t token;
    std::array<std::uint8_t, 32> seed;   // the private key the CLIENT signs the challenge with
    std::vector<std::uint8_t> cert;      // the certificate the client presents
    std::function<std::vector<input001::InputCommand>(std::int64_t seat)> build;
    std::size_t chunk;
    bool reverse;
};
struct ClientResult {
    bind001::BindInfo bind;
    bool bind_ok = false;
    bool challenge_ok = false;
    std::vector<std::vector<std::uint8_t>> frames;  // snapshot frames (after CHALLENGE + BIND)
    std::size_t pending = 0;
    std::size_t sent_cmds = 0;
};

static void run_authcert_socket_leg(const Rails& rails, const session::Scenario& sc,
                                    const std::vector<ClientPlan>& plans,
                                    const std::function<void(netinput::CaTable&)>& configure,
                                    netinput::Stats& stats_out,
                                    std::vector<ClientResult>& results_out, const char* label) {
    const std::size_t N = plans.size();
    results_out.assign(N, ClientResult{});

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    std::size_t commands_sent_count = 0;

    std::atomic<int> done{0};
    const int expect_done = static_cast<int>(N) + 1;

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/8);
        if (netsock::is_valid(listener)) netsock::set_nonblocking(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) { ++done; return; }

        netinput::CommandQueue q(sc.n_aircraft);
        netinput::InputProducer producer(rails, sc, q);
        auto ca_pk = ca_public();
        netinput::CaTable creds(producer.n_aircraft(), ca_pk.data());
        if (configure) configure(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent_count == N; });
        };
        stats_out = netinput::broadcast_authcert(listener, producer, q, creds, SESSION_K0, SESSION_K1,
                                                 /*min_initial=*/N, /*accept_deadline_ms=*/10000,
                                                 on_frame);
        netsock::close_socket(listener);
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    std::vector<std::thread> clients;
    for (std::size_t ci = 0; ci < N; ++ci) {
        clients.emplace_back([&, ci] {
            std::uint16_t port = wait_port();
            if (port == 0) { ++done; return; }
            netsock::socket_t s = netsock::connect_loopback(port);
            if (!netsock::is_valid(s)) { ++done; return; }

            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> all;
            std::uint8_t buf[4096];
            ClientResult& res = results_out[ci];

            // 1) read the server's CHALLENGE-001 (the FIRST downstream framing frame).
            while (all.size() < 1) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            std::uint64_t nonce = 0;
            if (!all.empty()) {
                std::size_t pos = 0;
                authmac001::ChallengeInfo ci_rec;
                res.challenge_ok =
                    authmac001::decode_challenge(all[0].data(), all[0].size(), pos, ci_rec) &&
                    pos == all[0].size();
                nonce = ci_rec.nonce;
            }

            // 2) answer with HELLO-004 = [certificate, challenge_signature], challenge_signature =
            //    Ed25519_sign(my seed, nonce||token).
            {
                std::uint8_t sig[64];
                authsig001::sign_challenge(plans[ci].seed.data(), nonce, plans[ci].token, sig);
                cert001::Hello4Info hi;
                hi.cert = plans[ci].cert;
                hi.sig.assign(sig, sig + 64);
                std::vector<std::uint8_t> rec, framed;
                cert001::encode_hello4(hi, rec);
                framing::encode_frame(rec, framed);
                netsock::send_all(s, framed);
            }

            // 3) read the BIND-001 (the second downstream framing frame).
            while (all.size() < 2) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            if (all.size() >= 2) {
                std::size_t pos = 0;
                res.bind_ok = bind001::decode_bind(all[1].data(), all[1].size(), pos, res.bind) &&
                              pos == all[1].size();
            }

            // 4) build this client's commands from the seat it was bound, scramble, send UP.
            std::vector<input001::InputCommand> cmds =
                res.bind_ok ? plans[ci].build(res.bind.seat) : std::vector<input001::InputCommand>{};
            if (plans[ci].reverse) std::reverse(cmds.begin(), cmds.end());
            res.sent_cmds = cmds.size();
            std::vector<std::uint8_t> upstream;
            for (const auto& c : cmds) {
                std::vector<std::uint8_t> rec, framed;
                input001::encode_command(c, rec);
                framing::encode_frame(rec, framed);
                upstream.insert(upstream.end(), framed.begin(), framed.end());
            }
            const std::size_t chunk = plans[ci].chunk ? plans[ci].chunk : 1;
            for (std::size_t off = 0; off < upstream.size(); off += chunk) {
                std::size_t take = (off + chunk <= upstream.size()) ? chunk : upstream.size() - off;
                netsock::send_all(s, upstream.data() + off, take);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                ++commands_sent_count;
            }
            cv.notify_all();

            // 5) read the downstream snapshots to EOF (frames after CHALLENGE + BIND).
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            res.pending = r.pending();
            for (std::size_t i = 2; i < all.size(); ++i) res.frames.push_back(all[i]);
            netsock::close_socket(s);
            ++done;
        });
    }

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < expect_done; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < expect_done) {
            std::printf("FAIL layer-30 authcert leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each present a CA certificate + SIGN their challenge and fly the seat their
// certificate designates (token order != seat order); the union reproduces build_server_frames.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, make_seed(ROSTER[0].base), issue(100, 2, 0, ROSTER[0].base), own_seat_cmds, 1, true},
        {200, make_seed(ROSTER[1].base), issue(200, 0, 0, ROSTER[1].base), own_seat_cmds, 7, false},
        {300, make_seed(ROSTER[2].base), issue(300, 1, 0, ROSTER[2].base), own_seat_cmds, 3, true},
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authcert_socket_leg(rails, sc, plans, {}, st, res, "leg1/3-certified-identities");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 3) {
        ++fails;
        std::printf("FAIL leg1: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != canon.size() || st.cmds_unauth != 0 || st.cmds_stale != 0 || st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg1: command accounting (ok=%zu unauth=%zu stale=%zu oob=%zu; want %zu/0/0/0)\n",
                    st.cmds_ok, st.cmds_unauth, st.cmds_stale, st.cmds_oob, canon.size());
    }
    for (std::size_t i = 0; i < res.size(); ++i) {
        const ClientResult& c = res[i];
        const std::int64_t want_seat = ROSTER[i].seat;
        if (!c.challenge_ok || !c.bind_ok || c.bind.n_aircraft != 3 || c.bind.seat != want_seat) {
            ++fails;
            std::printf("FAIL leg1: client %zu (token %lld) challenge_ok=%d bound to seat %lld, want %lld\n",
                        i, static_cast<long long>(ROSTER[i].token), c.challenge_ok ? 1 : 0,
                        static_cast<long long>(c.bind.seat), static_cast<long long>(want_seat));
        }
        if (c.pending != 0 || !frames_equal(c.frames, ref)) {
            ++fails;
            std::printf("FAIL leg1: client %zu downstream NOT byte-identical to build_server_frames "
                        "(got %zu of %zu, pending %zu)\n", i, c.frames.size(), ref.size(), c.pending);
        }
    }
    if (fails == 0)
        std::printf("  LEG1 PASS: 3 identities each presented a CA CERTIFICATE + SIGNED a fresh "
                    "challenge and flew the seat its CERTIFICATE designates (100->2, 200->0, 300->1; "
                    "scrambled/chunked) -> %zu frames byte-identical to build_server_frames; the "
                    "server enrolled only ONE CA public key\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands dropped; a SELF-SIGNED (non-CA) cert, a REVOKED
// token, and a STALE-epoch (post-rotation) cert all -> spectator; all four see the aircraft-0-only
// world byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A's non-seat-0 commands
    const std::size_t others = 3 * canon.size();               // B, C, D each drop all
    const auto ref = run_input_frames(rails, sc, own0);        // only aircraft 0 ever commanded

    // A SELF-SIGNED certificate: an attacker mints a token-100/seat-2 certificate with its OWN key
    // (not the CA). It will not verify under the CA public key.
    std::vector<std::uint8_t> self_signed;
    {
        auto attacker = make_seed(0x99);
        auto cseed = make_seed(ROSTER[0].base);
        std::uint8_t cpub[32];
        ed25519::public_key(cseed.data(), cpub);
        cert001::issue_cert(attacker.data(), 100, 2, 0, cpub, self_signed);
    }

    auto send_everything = [&](std::int64_t /*seat*/) { return canon; };
    std::vector<ClientPlan> plans = {
        // A: genuine token-200 cert + CORRECT seed -> seat 0; upstreams everything, foreign rejected.
        {200, make_seed(ROSTER[1].base), issue(200, 0, 0, ROSTER[1].base), send_everything, 1, true},
        // B: a SELF-SIGNED (non-CA) certificate -> spectator; all rejected (the headline capability).
        {100, make_seed(ROSTER[0].base), self_signed, send_everything, 5, false},
        // C: a genuine, correctly-signed token-300 cert that the server has REVOKED -> spectator.
        {300, make_seed(ROSTER[2].base), issue(300, 1, 0, ROSTER[2].base), send_everything, 3, true},
        // D: a genuine token-100 cert whose epoch (0) is STALE after the server rotated to floor 5.
        {100, make_seed(ROSTER[0].base), issue(100, 2, 0, ROSTER[0].base), send_everything, 4, false},
    };
    auto configure = [](netinput::CaTable& ct) {
        ct.revoke(300);            // client C's identity is compromised -> revoked
        ct.set_min_epoch(100, 5);  // client D's identity rotated -> epoch-0 certs superseded
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authcert_socket_leg(rails, sc, plans, configure, st, res, "leg2/ca-verify-revoke-rotate");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 4) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != own0.size() || st.cmds_unauth != (a_foreign + others) ||
        st.cmds_stale != 0 || st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg2: command accounting (ok=%zu unauth=%zu; want %zu own / %zu unauth)\n",
                    st.cmds_ok, st.cmds_unauth, own0.size(), a_foreign + others);
    }
    if (!res[0].bind_ok || res[0].bind.seat != 0) {
        ++fails;
        std::printf("FAIL leg2: client A (valid) should hold seat 0 (ok=%d seat=%lld)\n",
                    res[0].bind_ok ? 1 : 0, static_cast<long long>(res[0].bind.seat));
    }
    const char* why[] = {"", "SELF-SIGNED (non-CA)", "REVOKED token", "STALE epoch (rotated)"};
    for (std::size_t i = 1; i < res.size(); ++i) {
        if (!res[i].bind_ok || res[i].bind.seat != bind001::SPECTATOR) {
            ++fails;
            std::printf("FAIL leg2: client %zu (%s) should be a SPECTATOR (ok=%d seat=%lld)\n",
                        i, why[i], res[i].bind_ok ? 1 : 0, static_cast<long long>(res[i].bind.seat));
        }
    }
    for (std::size_t i = 0; i < res.size(); ++i) {
        if (res[i].pending != 0 || !frames_equal(res[i].frames, ref)) {
            ++fails;
            std::printf("FAIL leg2: client %zu downstream NOT the aircraft-0-only world (got %zu of %zu, "
                        "pending %zu)\n", i, res[i].frames.size(), ref.size(), res[i].pending);
        }
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: a seat-0 client's %zu foreign commands + a SELF-SIGNED cert + a "
                    "REVOKED token + a STALE-epoch (rotated) cert (%zu commands) all rejected "
                    "(cmds_unauth=%zu); CA-trust + revocation + rotation enforced\n",
                    a_foreign, others, st.cmds_unauth);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: SHA-512 official vector + Ed25519 fixed pin + CERT-001/HELLO-004 codec pins + CaTable verify.
// ---------------------------------------------------------------------------------------------
static bool hex_eq(const std::uint8_t* bytes, std::size_t n, const char* hex) {
    static const char* H = "0123456789abcdef";
    for (std::size_t i = 0; i < n; ++i) {
        if (hex[2 * i] != H[(bytes[i] >> 4) & 0xF] || hex[2 * i + 1] != H[bytes[i] & 0xF])
            return false;
    }
    return hex[2 * n] == '\0';
}

static int run_leg3() {
    int fails = 0;

    // OFFICIAL SHA-512("abc").
    {
        std::uint8_t out[64];
        sha512::hash(reinterpret_cast<const std::uint8_t*>("abc"), 3, out);
        const char* want = "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
                           "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f";
        if (!hex_eq(out, 64, want)) { ++fails; std::printf("FAIL leg3: SHA-512(\"abc\") vector\n"); }
    }

    // Ed25519 fixed pin: seed = 00..1f, msg = "SEADS-ed25519-pin" (confirmed vs `cryptography`).
    {
        std::uint8_t seed[32];
        for (int i = 0; i < 32; ++i) seed[i] = static_cast<std::uint8_t>(i);
        std::uint8_t pk[32], sig[64];
        ed25519::public_key(seed, pk);
        const char* m = "SEADS-ed25519-pin";
        ed25519::sign(seed, reinterpret_cast<const std::uint8_t*>(m), std::strlen(m), sig);
        const char* want_pk = "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8";
        const char* want_sig = "7cba67f3c33e2fc333bb5568d2c7358d54d732903df5c022dc46838eb0317599"
                               "aefc4f06518b1a5d33a539592d53fa7eb08af4acdc95719491adfff39365390f";
        bool ok = hex_eq(pk, 32, want_pk) && hex_eq(sig, 64, want_sig);
        if (!ok) { ++fails; std::printf("FAIL leg3: Ed25519 fixed pin\n"); }
    }

    // CERT-001 body known encoding: token=7 (zz 0x0E), seat=1 (zz 0x02), epoch=0 (zz 0x00),
    // pubkey=32x0x11 -> [0x01,0x0E,0x02,0x00, 0x11*32]. (The SAME bytes in tools/cert_ref.py.)
    {
        std::uint8_t pub[32];
        std::memset(pub, 0x11, 32);
        std::vector<std::uint8_t> body;
        cert001::encode_cert_body(7, 1, 0, pub, body);
        std::uint8_t want[36] = {0x01, 0x0E, 0x02, 0x00};
        for (int i = 0; i < 32; ++i) want[4 + i] = 0x11;
        bool ok = body.size() == sizeof(want);
        for (std::size_t i = 0; ok && i < sizeof(want); ++i) ok = (body[i] == want[i]);
        if (!ok) { ++fails; std::printf("FAIL leg3: CERT-001 body known encoding\n"); }
    }

    // HELLO-004 known encoding: cert=[0xAA,0xBB] (certlen 2), sig=[0xCC] (siglen 1) ->
    // [0x04, 0x02, 0xAA,0xBB, 0x01, 0xCC]. (The SAME bytes in tools/cert_ref.py.)
    {
        cert001::Hello4Info hi;
        hi.cert = {0xAA, 0xBB};
        hi.sig = {0xCC};
        std::vector<std::uint8_t> h4;
        cert001::encode_hello4(hi, h4);
        const std::uint8_t wh[] = {0x04, 0x02, 0xAA, 0xBB, 0x01, 0xCC};
        bool ok = h4.size() == sizeof(wh);
        for (std::size_t i = 0; ok && i < sizeof(wh); ++i) ok = (h4[i] == wh[i]);
        // round-trip a real CA certificate + HELLO-004 with a real 64-byte challenge signature.
        auto ca = make_seed(CA_BASE);
        for (std::int64_t tok : {std::int64_t(-5), std::int64_t(0), std::int64_t(1) << 40}) {
            auto cseed = make_seed(static_cast<std::uint8_t>(tok & 0xFF));
            std::uint8_t cpub[32];
            ed25519::public_key(cseed.data(), cpub);
            std::vector<std::uint8_t> cert;
            cert001::issue_cert(ca.data(), tok, 1, 3, cpub, cert);
            std::size_t cp = 0; cert001::CertInfo cinfo;
            ok = ok && cert001::decode_cert(cert.data(), cert.size(), cp, cinfo) && cp == cert.size() &&
                 cinfo.token == tok && cinfo.seat == 1 && cinfo.epoch == 3 &&
                 std::memcmp(cinfo.pubkey, cpub, 32) == 0;
            auto ca_pk = ca_public();
            ok = ok && cert001::verify_cert(ca_pk.data(), cinfo);
            std::uint8_t sig[64];
            authsig001::sign_challenge(cseed.data(), 12345, tok, sig);
            cert001::Hello4Info h; h.cert = cert; h.sig.assign(sig, sig + 64);
            std::vector<std::uint8_t> e; cert001::encode_hello4(h, e);
            std::size_t pos = 0; cert001::Hello4Info d;
            ok = ok && cert001::decode_hello4(e.data(), e.size(), pos, d) && pos == e.size() &&
                 d.cert == cert && d.sig.size() == 64 && std::memcmp(d.sig.data(), sig, 64) == 0;
        }
        // HELLO-004 (v4) rejects other versions (HELLO-001/2/3 = 0x01/0x02/0x03); CERT-001 rejects them too.
        std::uint8_t v3[] = {0x03, 0x0E, 0x00};
        std::size_t p = 0; cert001::Hello4Info dh;
        ok = ok && !cert001::decode_hello4(v3, sizeof(v3), p, dh);
        p = 0; cert001::CertInfo dc;
        std::uint8_t badc[] = {0x02, 0x0E, 0x02, 0x00};
        ok = ok && !cert001::decode_cert(badc, sizeof(badc), p, dc);
        if (!ok) { ++fails; std::printf("FAIL leg3: CERT-001/HELLO-004 codec parity vs cert_ref\n"); }
    }

    // CaTable: CA-verify + self-signed/forged/replay reject, revoke/un-revoke, epoch-floor rotation,
    // identity->seat invariant to order, unknown->spectator, double-login->spectator, reclaim OWN seat.
    {
        auto ca = make_seed(CA_BASE);
        auto ca_pk = ca_public();
        auto nonce_for = [](std::uint64_t counter) {
            return authmac001::derive_nonce(SESSION_K0, SESSION_K1, counter);
        };
        auto cert_for = [&](std::int64_t token, std::int64_t seat, std::int64_t epoch,
                            std::uint8_t base) {
            return issue(token, seat, epoch, base);
        };
        auto present = [&](netinput::CaTable& ct, std::int64_t token, std::uint8_t base,
                           const std::vector<std::uint8_t>& cert, std::uint64_t counter) {
            std::uint64_t nonce = nonce_for(counter);
            auto seed = make_seed(base);
            std::uint8_t sig[64];
            authsig001::sign_challenge(seed.data(), nonce, token, sig);
            return ct.authenticate(cert.data(), cert.size(), nonce, sig, 64);
        };

        netinput::CaTable ct(3, ca_pk.data());
        auto c100 = cert_for(100, 2, 0, 0x10), c200 = cert_for(200, 0, 0, 0x20),
             c300 = cert_for(300, 1, 0, 0x30);
        bool ok = present(ct, 300, 0x30, c300, 0) == 1 &&
                  present(ct, 100, 0x10, c100, 1) == 2 &&
                  present(ct, 200, 0x20, c200, 2) == 0;

        // SELF-SIGNED (non-CA) certificate -> spectator, even with a valid possession proof.
        netinput::CaTable ct2(3, ca_pk.data());
        std::vector<std::uint8_t> self_signed;
        {
            auto attacker = make_seed(0x99);
            auto cseed = make_seed(0x10);
            std::uint8_t cpub[32];
            ed25519::public_key(cseed.data(), cpub);
            cert001::issue_cert(attacker.data(), 100, 2, 0, cpub, self_signed);
        }
        std::uint64_t nz = nonce_for(0);
        {
            auto real = make_seed(0x10);
            std::uint8_t good[64];
            authsig001::sign_challenge(real.data(), nz, 100, good);
            ok = ok && ct2.authenticate(self_signed.data(), self_signed.size(), nz, good, 64) ==
                       bind001::SPECTATOR;
        }
        // STOLEN cert + WRONG key: a genuine CA cert, but the challenge is signed with the wrong key.
        {
            auto forger = make_seed(0x99);
            std::uint8_t forged[64];
            authsig001::sign_challenge(forger.data(), nz, 100, forged);
            ok = ok && ct2.authenticate(c100.data(), c100.size(), nz, forged, 64) == bind001::SPECTATOR;
        }
        // REPLAY: a valid possession proof under one nonce fails under another; genuine one seats.
        {
            auto real = make_seed(0x10);
            std::uint8_t good[64];
            authsig001::sign_challenge(real.data(), nz, 100, good);
            ok = ok && ct2.authenticate(c100.data(), c100.size(), nonce_for(99), good, 64) ==
                       bind001::SPECTATOR;
            ok = ok && ct2.authenticate(c100.data(), c100.size(), nz, good, 64) == 2;
        }

        // REVOCATION: revoke token 200; its genuine cert -> spectator; other identities unaffected;
        // un-revoke restores.
        netinput::CaTable ct3(3, ca_pk.data());
        ct3.revoke(200);
        ok = ok && present(ct3, 200, 0x20, c200, 10) == bind001::SPECTATOR;
        ok = ok && present(ct3, 100, 0x10, c100, 11) == 2;
        ct3.unrevoke(200);
        ok = ok && present(ct3, 200, 0x20, c200, 12) == 0;

        // KEY ROTATION: raise token 300's floor to 5; the epoch-0 cert -> spectator; an epoch-5 cert
        // with a NEW key authenticates.
        netinput::CaTable ct4(3, ca_pk.data());
        ct4.set_min_epoch(300, 5);
        ok = ok && present(ct4, 300, 0x30, c300, 20) == bind001::SPECTATOR;
        auto c300b = cert_for(300, 1, 5, 0x77);
        ok = ok && present(ct4, 300, 0x77, c300b, 21) == 1;

        // MALFORMED cert -> spectator; DOUBLE-LOGIN -> spectator; release then reclaim OWN seat.
        {
            std::uint8_t junk[] = {0x01, 0x02};
            std::uint64_t n = nonce_for(30);
            auto real = make_seed(0x10);
            std::uint8_t good[64];
            authsig001::sign_challenge(real.data(), n, 100, good);
            ok = ok && ct.authenticate(junk, sizeof(junk), n, good, 64) == bind001::SPECTATOR;
        }
        ok = ok && present(ct, 100, 0x10, c100, 31) == bind001::SPECTATOR;  // seat 2 held on ct
        ct.release(2);
        ok = ok && present(ct, 100, 0x10, c100, 32) == 2;
        if (!ok) { ++fails; std::printf("FAIL leg3: CaTable verify/revoke/rotate binding\n"); }
    }

    // authorization predicate (reused from layers 18/21/26/27): own seat only; a spectator commands nothing.
    {
        bool ok = netinput::seat_authorizes(0, 0) && netinput::seat_authorizes(2, 2) &&
                  !netinput::seat_authorizes(0, 1) && !netinput::seat_authorizes(1, 0) &&
                  !netinput::seat_authorizes(bind001::SPECTATOR, 0);
        if (!ok) { ++fails; std::printf("FAIL leg3: seat_authorizes predicate\n"); }
    }

    if (fails == 0)
        std::printf("  LEG3 PASS: SHA-512(\"abc\") + Ed25519 fixed pin + CERT-001/HELLO-004 codec pins "
                    "(vs cert_ref) + CaTable (CA-verify, self-signed/forged/replay reject, "
                    "revoke/un-revoke, epoch-floor rotation, unknown/double-login->spectator, "
                    "reclaim-own-seat) + own-seat authorization predicate\n");
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_leg3();  // in-process first (fast, no sockets)
    fails += run_leg1();
    fails += run_leg2();
    if (fails == 0) {
        std::printf("PASS: layer-30 certificate PKI — the server trusts ONE certificate-authority "
                    "public key; a client's seat is gated on a CA-signed CERTIFICATE (CERT-001) it "
                    "presents in HELLO-004 plus an Ed25519 possession proof over a fresh challenge, "
                    "with REVOCATION and key ROTATION; invariant to join order; N certified clients' "
                    "inputs compose to build_server_frames byte-for-byte\n");
        return 0;
    }
    std::printf("RESULT: layer-30 authcert bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
