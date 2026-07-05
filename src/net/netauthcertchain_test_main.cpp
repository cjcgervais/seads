// SEADS CERTIFICATE-CHAIN determinism BRIDGE for the authoritative CA-chain auth server (LAYER 33).
//
// Layer 30 (broadcast_authcert) trusted ONE self-signed root CA and each client presented a SINGLE
// certificate signed DIRECTLY by it — no delegation. Layer 33 (broadcast_authcertchain) validates a
// certificate CHAIN: an INTERMEDIATE CA (itself certified by the root) issues the leaf, and the server
// walks the PATH [leaf, intermediate, ...] up to the trusted root. Each link is an ordinary CERT-001;
// the client answers the challenge with HELLO-005 = [chain (leaf..top), challenge_signature].
// CaChainTable::authenticate validates the path to the root (cert001::verify_chain), the revocation +
// per-token epoch floor on EVERY link, and the possession proof under the LEAF key before binding the
// seat; any failure -> SPECTATOR (the layer-21/26/27/30 reject class). This bridge proves, over real
// 127.0.0.1 sockets with NO sleeps / NO timing guesses, three claims:
//
//   LEG 1 (CA-CHAINED SEATS, INVARIANT TO JOIN ORDER — the headline): THREE clients each present a
//   depth-2 chain [leaf, intermediate] (the leaf issued by an INTERMEDIATE CA the ROOT certified) AND
//   the leaf PRIVATE seed, are challenged, present the chain + SIGN, and are bound to the seat their
//   LEAF certificate designates — deliberately token order != seat order (100->2, 200->0, 300->1).
//   Each upstreams ONLY that seat's commands (SCRAMBLED + adversarially chunked); their union is the
//   whole INPUT-SK-001 command set, so the produced downstream frames are BYTE-IDENTICAL to
//   session::build_server_frames. The server trusts only the ONE ROOT public key.
//
//   LEG 2 (PATH + SUBTREE REVOCATION + ROTATION BOUNDARY — the delegation capabilities): FOUR clients
//   upstream the WHOLE scenario. A holds a genuine leaf under intermediate I1 -> seat 0; its foreign-
//   aircraft commands are dropped. B presents a SELF-SIGNED leaf ALONE (no path to the root) ->
//   SPECTATOR. C holds a genuine leaf under intermediate I2, but the server REVOKES I2 -> the whole
//   subtree is SPECTATOR. D holds a genuine leaf under I1 whose LEAF token was rotated (epoch floor
//   raised) -> its stale-epoch cert is SPECTATOR. All four receive the IDENTICAL aircraft-0-only world
//   byte-for-byte; only A's own-seat commands ever move the sim.
//
//   LEG 3 (SHA-512 + Ed25519 pins + HELLO-005 codec + verify_chain + CaChainTable — in-process): the
//   OFFICIAL SHA-512("abc") vector, the Ed25519 fixed-seed pin, the HELLO-005 known-encoding pin (vs
//   tools/cert_ref.py), verify_chain (leaf<-intermediate<-root validates; self-signed/wrong-issuer/
//   over-depth reject), and CaChainTable (chain seat, self-signed reject, forged-possession reject,
//   intermediate subtree revoke/un-revoke, intermediate + leaf epoch-floor rotation, depth-1 root-signed
//   leaf, double-login->spectator, reclaim-own-seat) + the reused seat_authorizes predicate.
//
// Every socket assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; CHALLENGE-001/HELLO-005/CERT-001/BIND-001 are transport
// metadata (modelled on the framing envelope, NOT a sealed rails.wire block) => no seal. Rides v1.26r0.
#include "authcertchainserver.h"
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
// reproducible. Same values as the layer-26/27/30 bridge (the challenge is reused).
static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact 3-aircraft scenario the layer-15b..30 bridges use.
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

// A deterministic 32-byte private seed from a base byte (a stand-in for key material).
static std::array<std::uint8_t, 32> make_seed(std::uint8_t base) {
    std::array<std::uint8_t, 32> s;
    for (int i = 0; i < 32; ++i) s[i] = static_cast<std::uint8_t>((base + i) & 0xFF);
    return s;
}
static void pub_of(std::uint8_t base, std::uint8_t out[32]) {
    auto s = make_seed(base);
    ed25519::public_key(s.data(), out);
}

// The ROOT certificate authority: ONE key pair. Only its PUBLIC key reaches the server.
static const std::uint8_t ROOT_BASE = 0xC0;
static std::array<std::uint8_t, 32> root_public() {
    std::array<std::uint8_t, 32> pk{};
    pub_of(ROOT_BASE, pk.data());
    return pk;
}

// Intermediate CAs: their own key pairs, each certified by the root (a depth-2 chain).
static const std::uint8_t INT1_BASE = 0xA0;  // token 900
static const std::uint8_t INT2_BASE = 0xA1;  // token 901
static const std::int64_t INT1_TOKEN = 900, INT2_TOKEN = 901;

// root -> intermediate certificate (the intermediate's pubkey, signed by the root).
static std::vector<std::uint8_t> intermediate_cert(std::uint8_t int_base, std::int64_t int_token,
                                                   std::int64_t epoch = 0) {
    auto root = make_seed(ROOT_BASE);
    std::uint8_t ipub[32];
    pub_of(int_base, ipub);
    std::vector<std::uint8_t> cert;
    cert001::issue_cert(root.data(), int_token, 0, epoch, ipub, cert);
    return cert;
}

// issuer -> leaf certificate (the client's pubkey, signed by `issuer_base`'s key).
static std::vector<std::uint8_t> leaf_cert(std::uint8_t issuer_base, std::int64_t token,
                                           std::int64_t seat, std::int64_t epoch,
                                           std::uint8_t client_base) {
    auto iseed = make_seed(issuer_base);
    std::uint8_t cpub[32];
    pub_of(client_base, cpub);
    std::vector<std::uint8_t> cert;
    cert001::issue_cert(iseed.data(), token, seat, epoch, cpub, cert);
    return cert;
}

// The pre-shared roster: token -> (designated seat, PRIVATE base seed). Token order != seat order.
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
// A generic socket leg: N clients. Each holds a TOKEN, the private SEED it USES to sign the challenge,
// and the CHAIN it presents (leaf first). Given the seat the server binds it (learned from BIND), a
// plan builds the commands it upstreams, plus a scramble/chunking. The server runs
// broadcast_authcertchain(min_initial = N) over a CaChainTable configured by `configure` (revocation /
// rotation applied before any client connects).
// ---------------------------------------------------------------------------------------------
struct ClientPlan {
    std::int64_t token;                                // the LEAF token the client signs under
    std::array<std::uint8_t, 32> seed;                 // the private key the CLIENT signs the challenge with
    std::vector<std::vector<std::uint8_t>> chain;      // the certificate chain (leaf first)
    std::function<std::vector<input001::InputCommand>(std::int64_t seat)> build;
    std::size_t chunk;
    bool reverse;
};
struct ClientResult {
    bind001::BindInfo bind;
    bool bind_ok = false;
    bool challenge_ok = false;
    std::vector<std::vector<std::uint8_t>> frames;
    std::size_t pending = 0;
    std::size_t sent_cmds = 0;
};

static void run_chain_socket_leg(const Rails& rails, const session::Scenario& sc,
                                 const std::vector<ClientPlan>& plans,
                                 const std::function<void(netinput::CaChainTable&)>& configure,
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
        auto root_pk = root_public();
        netinput::CaChainTable creds(producer.n_aircraft(), root_pk.data());
        if (configure) configure(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent_count == N; });
        };
        stats_out = netinput::broadcast_authcertchain(listener, producer, q, creds, SESSION_K0,
                                                      SESSION_K1, /*min_initial=*/N,
                                                      /*accept_deadline_ms=*/10000, on_frame);
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

            // 2) answer with HELLO-005 = [chain, challenge_signature], challenge_signature =
            //    Ed25519_sign(my seed, nonce||leaf_token).
            {
                std::uint8_t sig[64];
                authsig001::sign_challenge(plans[ci].seed.data(), nonce, plans[ci].token, sig);
                cert001::Hello5Info hi;
                hi.certs = plans[ci].chain;
                hi.sig.assign(sig, sig + 64);
                std::vector<std::uint8_t> rec, framed;
                cert001::encode_hello5(hi, rec);
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
            std::printf("FAIL layer-33 authcertchain leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each present a depth-2 chain (leaf issued by intermediate I1, I1 by root) +
// SIGN their challenge and fly the seat their LEAF designates (token order != seat order); the union
// reproduces build_server_frames.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    const auto int1 = intermediate_cert(INT1_BASE, INT1_TOKEN);
    auto chain_for = [&](std::int64_t token, std::int64_t seat, std::uint8_t base) {
        return std::vector<std::vector<std::uint8_t>>{leaf_cert(INT1_BASE, token, seat, 0, base), int1};
    };
    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, make_seed(ROSTER[0].base), chain_for(100, 2, ROSTER[0].base), own_seat_cmds, 1, true},
        {200, make_seed(ROSTER[1].base), chain_for(200, 0, ROSTER[1].base), own_seat_cmds, 7, false},
        {300, make_seed(ROSTER[2].base), chain_for(300, 1, ROSTER[2].base), own_seat_cmds, 3, true},
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_chain_socket_leg(rails, sc, plans, {}, st, res, "leg1/3-chained-identities");

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
        std::printf("  LEG1 PASS: 3 identities each presented a CERTIFICATE CHAIN [leaf<-intermediate<-"
                    "root] + SIGNED a fresh challenge and flew the seat its LEAF designates (100->2, "
                    "200->0, 300->1; scrambled/chunked) -> %zu frames byte-identical to "
                    "build_server_frames; the server trusts only the ONE ROOT key\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands dropped; a SELF-SIGNED leaf (no path), a leaf under
// a REVOKED intermediate, and a STALE-epoch (rotated) leaf all -> spectator; all four see the
// aircraft-0-only world byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A's non-seat-0 commands
    const std::size_t others = 3 * canon.size();               // B, C, D each drop all
    const auto ref = run_input_frames(rails, sc, own0);        // only aircraft 0 ever commanded

    const auto int1 = intermediate_cert(INT1_BASE, INT1_TOKEN);
    const auto int2 = intermediate_cert(INT2_BASE, INT2_TOKEN);
    // A SELF-SIGNED leaf: an attacker mints a token-100/seat-2 leaf with its OWN key, presented ALONE
    // (there is no path from it to the trusted root).
    std::vector<std::uint8_t> self_signed;
    {
        std::uint8_t cpub[32];
        pub_of(ROSTER[0].base, cpub);
        auto attacker = make_seed(0x99);
        cert001::issue_cert(attacker.data(), 100, 2, 0, cpub, self_signed);
    }

    auto send_everything = [&](std::int64_t /*seat*/) { return canon; };
    std::vector<ClientPlan> plans = {
        // A: genuine leaf under I1 + CORRECT seed -> seat 0; upstreams everything, foreign rejected.
        {200, make_seed(ROSTER[1].base),
         {leaf_cert(INT1_BASE, 200, 0, 0, ROSTER[1].base), int1}, send_everything, 1, true},
        // B: a SELF-SIGNED leaf presented ALONE -> no path to root -> spectator (the chain headline).
        {100, make_seed(ROSTER[0].base), {self_signed}, send_everything, 5, false},
        // C: a genuine leaf under intermediate I2, which the server has REVOKED -> subtree spectator.
        {300, make_seed(ROSTER[2].base),
         {leaf_cert(INT2_BASE, 300, 1, 0, ROSTER[2].base), int2}, send_everything, 3, true},
        // D: a genuine leaf under I1 whose LEAF token (100) is rotated (epoch floor 5), cert at epoch 0.
        {100, make_seed(ROSTER[0].base),
         {leaf_cert(INT1_BASE, 100, 2, 0, ROSTER[0].base), int1}, send_everything, 4, false},
    };
    auto configure = [](netinput::CaChainTable& ct) {
        ct.revoke(INT2_TOKEN);      // intermediate I2 compromised -> every leaf beneath it rejected
        ct.set_min_epoch(100, 5);   // client D's leaf identity rotated -> epoch-0 leaf superseded
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_chain_socket_leg(rails, sc, plans, configure, st, res, "leg2/path-revoke-rotate");

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
    const char* why[] = {"", "SELF-SIGNED leaf (no path)", "REVOKED intermediate", "STALE epoch (rotated)"};
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
        std::printf("  LEG2 PASS: a seat-0 client's %zu foreign commands + a SELF-SIGNED leaf + a leaf "
                    "under a REVOKED intermediate + a STALE-epoch (rotated) leaf (%zu commands) all "
                    "rejected (cmds_unauth=%zu); path validation + subtree revocation + rotation "
                    "enforced\n", a_foreign, others, st.cmds_unauth);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: SHA-512 official vector + Ed25519 fixed pin + HELLO-005 codec pin + verify_chain + CaChainTable.
// ---------------------------------------------------------------------------------------------
static bool hex_eq(const std::uint8_t* bytes, std::size_t n, const char* hex) {
    static const char* H = "0123456789abcdef";
    for (std::size_t i = 0; i < n; ++i) {
        if (hex[2 * i] != H[(bytes[i] >> 4) & 0xF] || hex[2 * i + 1] != H[bytes[i] & 0xF])
            return false;
    }
    return hex[2 * n] == '\0';
}

// Decode a raw cert blob into CertInfo (helper for verify_chain assertions).
static cert001::CertInfo decode1(const std::vector<std::uint8_t>& raw) {
    cert001::CertInfo ci;
    std::size_t pos = 0;
    cert001::decode_cert(raw.data(), raw.size(), pos, ci);
    return ci;
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

    // HELLO-005 known encoding: certs=[[0xAA],[0xBB]], sig=[0xCC] ->
    // [0x05, 0x02, 0x01,0xAA, 0x01,0xBB, 0x01,0xCC]. (The SAME bytes in tools/cert_ref.py.)
    {
        cert001::Hello5Info hi;
        hi.certs = {{0xAA}, {0xBB}};
        hi.sig = {0xCC};
        std::vector<std::uint8_t> h5;
        cert001::encode_hello5(hi, h5);
        const std::uint8_t want[] = {0x05, 0x02, 0x01, 0xAA, 0x01, 0xBB, 0x01, 0xCC};
        bool ok = h5.size() == sizeof(want);
        for (std::size_t i = 0; ok && i < sizeof(want); ++i) ok = (h5[i] == want[i]);
        // round-trip a real HELLO-005 (a depth-2 chain + a 64-byte challenge signature).
        auto int1 = intermediate_cert(INT1_BASE, INT1_TOKEN);
        auto lf = leaf_cert(INT1_BASE, 100, 2, 0, ROSTER[0].base);
        std::uint8_t sig[64];
        authsig001::sign_challenge(make_seed(ROSTER[0].base).data(), 12345, 100, sig);
        cert001::Hello5Info h; h.certs = {lf, int1}; h.sig.assign(sig, sig + 64);
        std::vector<std::uint8_t> e; cert001::encode_hello5(h, e);
        std::size_t pos = 0; cert001::Hello5Info d;
        ok = ok && cert001::decode_hello5(e.data(), e.size(), pos, d) && pos == e.size() &&
             d.certs.size() == 2 && d.certs[0] == lf && d.certs[1] == int1 &&
             d.sig.size() == 64 && std::memcmp(d.sig.data(), sig, 64) == 0;
        // HELLO-005 (v5) rejects other versions (HELLO-001..4 = 0x01..0x04).
        std::uint8_t v4[] = {0x04, 0x00};
        std::size_t p = 0; cert001::Hello5Info dh;
        ok = ok && !cert001::decode_hello5(v4, sizeof(v4), p, dh);
        if (!ok) { ++fails; std::printf("FAIL leg3: HELLO-005 codec parity vs cert_ref\n"); }
    }

    // verify_chain: leaf<-intermediate<-root validates; self-signed / wrong-issuer / over-depth reject.
    {
        auto root_pk = root_public();
        auto int1 = intermediate_cert(INT1_BASE, INT1_TOKEN);
        auto lf = leaf_cert(INT1_BASE, 100, 2, 0, ROSTER[0].base);
        std::vector<cert001::CertInfo> chain = {decode1(lf), decode1(int1)};
        bool ok = cert001::verify_chain(root_pk.data(), chain, 4);
        // a self-signed leaf presented ALONE does not reach the root.
        std::vector<std::uint8_t> self_leaf;
        {
            std::uint8_t cpub[32]; pub_of(ROSTER[0].base, cpub);
            auto s = make_seed(ROSTER[0].base);
            cert001::issue_cert(s.data(), 100, 2, 0, cpub, self_leaf);
        }
        std::vector<cert001::CertInfo> just_leaf = {decode1(self_leaf)};
        ok = ok && !cert001::verify_chain(root_pk.data(), just_leaf, 4);
        // a leaf signed by a DIFFERENT key than the presented intermediate fails the path.
        auto rogue = leaf_cert(0xB0, 100, 2, 0, ROSTER[0].base);
        std::vector<cert001::CertInfo> broken = {decode1(rogue), decode1(int1)};
        ok = ok && !cert001::verify_chain(root_pk.data(), broken, 4);
        // depth limit: a 2-link chain is rejected under max_depth 1.
        ok = ok && !cert001::verify_chain(root_pk.data(), chain, 1);
        if (!ok) { ++fails; std::printf("FAIL leg3: verify_chain path validation\n"); }
    }

    // CaChainTable: chain seat + self-signed/forged reject, intermediate subtree revoke, intermediate +
    // leaf epoch-floor rotation, depth-1 root-signed leaf, double-login->spectator, reclaim OWN seat.
    {
        auto root_pk = root_public();
        auto nonce_for = [](std::uint64_t counter) {
            return authmac001::derive_nonce(SESSION_K0, SESSION_K1, counter);
        };
        auto present = [&](netinput::CaChainTable& ct, std::int64_t leaf_token, std::uint8_t leaf_base,
                           const std::vector<std::vector<std::uint8_t>>& chain, std::uint64_t counter) {
            std::uint64_t nonce = nonce_for(counter);
            std::uint8_t sig[64];
            authsig001::sign_challenge(make_seed(leaf_base).data(), nonce, leaf_token, sig);
            return ct.authenticate(chain, nonce, sig, 64);
        };
        auto int1 = intermediate_cert(INT1_BASE, INT1_TOKEN);
        auto int2 = intermediate_cert(INT2_BASE, INT2_TOKEN);
        auto chain1 = [&](std::int64_t token, std::int64_t seat, std::uint8_t base) {
            return std::vector<std::vector<std::uint8_t>>{leaf_cert(INT1_BASE, token, seat, 0, base), int1};
        };

        netinput::CaChainTable ct(3, root_pk.data());
        bool ok = present(ct, 300, 0x30, chain1(300, 1, 0x30), 0) == 1 &&
                  present(ct, 100, 0x10, chain1(100, 2, 0x10), 1) == 2 &&
                  present(ct, 200, 0x20, chain1(200, 0, 0x20), 2) == 0;

        // SELF-SIGNED leaf alone -> spectator, even with a valid possession proof.
        netinput::CaChainTable ct2(3, root_pk.data());
        std::vector<std::uint8_t> self_leaf;
        {
            std::uint8_t cpub[32]; pub_of(0x10, cpub);
            auto attacker = make_seed(0x99);
            cert001::issue_cert(attacker.data(), 100, 2, 0, cpub, self_leaf);
        }
        ok = ok && present(ct2, 100, 0x10, {self_leaf}, 3) == bind001::SPECTATOR;
        // FORGED possession: right chain, wrong signing key -> spectator.
        {
            std::uint64_t nz = nonce_for(4);
            std::uint8_t forged[64];
            authsig001::sign_challenge(make_seed(0x99).data(), nz, 100, forged);
            auto c = chain1(100, 2, 0x10);
            ok = ok && ct2.authenticate(c, nz, forged, 64) == bind001::SPECTATOR;
            // the genuine holder authenticates.
            ok = ok && present(ct2, 100, 0x10, chain1(100, 2, 0x10), 5) == 2;
        }

        // INTERMEDIATE subtree revocation: revoke I2; a leaf beneath it -> spectator; a leaf under I1
        // is unaffected; un-revoke restores.
        netinput::CaChainTable ct3(3, root_pk.data());
        auto chain2 = std::vector<std::vector<std::uint8_t>>{leaf_cert(INT2_BASE, 300, 1, 0, 0x30), int2};
        ct3.revoke(INT2_TOKEN);
        ok = ok && present(ct3, 300, 0x30, chain2, 10) == bind001::SPECTATOR;
        ok = ok && present(ct3, 100, 0x10, chain1(100, 2, 0x10), 11) == 2;  // I1 subtree unaffected
        ct3.unrevoke(INT2_TOKEN);
        ok = ok && present(ct3, 300, 0x30, chain2, 12) == 1;

        // INTERMEDIATE rotation: raise I2's epoch floor to 5; the epoch-0 intermediate cert -> spectator;
        // a re-issued epoch-5 intermediate + leaf authenticates.
        netinput::CaChainTable ct4(3, root_pk.data());
        ct4.set_min_epoch(INT2_TOKEN, 5);
        ok = ok && present(ct4, 300, 0x30, chain2, 20) == bind001::SPECTATOR;
        auto int2b = intermediate_cert(INT2_BASE, INT2_TOKEN, /*epoch=*/5);
        auto leaf2b = leaf_cert(INT2_BASE, 300, 1, 5, 0x30);
        ok = ok && present(ct4, 300, 0x30, {leaf2b, int2b}, 21) == 1;

        // DEPTH-1: a leaf signed DIRECTLY by the root (the layer-30 case) still seats.
        netinput::CaChainTable ct5(3, root_pk.data());
        auto direct = leaf_cert(ROOT_BASE, 100, 2, 0, 0x10);
        ok = ok && present(ct5, 100, 0x10, {direct}, 30) == 2;

        // DOUBLE-LOGIN -> spectator; release then reclaim OWN seat.
        ok = ok && present(ct, 100, 0x10, chain1(100, 2, 0x10), 31) == bind001::SPECTATOR;  // seat 2 held
        ct.release(2);
        ok = ok && present(ct, 100, 0x10, chain1(100, 2, 0x10), 32) == 2;
        if (!ok) { ++fails; std::printf("FAIL leg3: CaChainTable path/revoke/rotate binding\n"); }
    }

    // authorization predicate (reused from layers 18/21/26/27/30): own seat only; a spectator commands nothing.
    {
        bool ok = netinput::seat_authorizes(0, 0) && netinput::seat_authorizes(2, 2) &&
                  !netinput::seat_authorizes(0, 1) && !netinput::seat_authorizes(1, 0) &&
                  !netinput::seat_authorizes(bind001::SPECTATOR, 0);
        if (!ok) { ++fails; std::printf("FAIL leg3: seat_authorizes predicate\n"); }
    }

    if (fails == 0)
        std::printf("  LEG3 PASS: SHA-512(\"abc\") + Ed25519 fixed pin + HELLO-005 codec pin (vs "
                    "cert_ref) + verify_chain (leaf<-intermediate<-root; self-signed/wrong-issuer/"
                    "over-depth reject) + CaChainTable (chain seat, forged reject, intermediate subtree "
                    "revoke, intermediate+leaf rotation, depth-1 root leaf, double-login->spectator, "
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
        std::printf("PASS: layer-33 certificate CHAINS — the server trusts ONE ROOT public key and "
                    "validates a certificate PATH [leaf<-intermediate<-root]; a client presents its "
                    "chain in HELLO-005 + an Ed25519 possession proof over a fresh challenge, with "
                    "SUBTREE revocation + per-link rotation; invariant to join order; N chained clients' "
                    "inputs compose to build_server_frames byte-for-byte\n");
        return 0;
    }
    std::printf("RESULT: layer-33 authcertchain bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
