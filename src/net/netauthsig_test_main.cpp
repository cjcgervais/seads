// SEADS ASYMMETRIC-SIGNATURE determinism BRIDGE for the authoritative public-key auth server (LAYER 27).
//
// Layer 26 (broadcast_authmac) verified a client's identity with a SYMMETRIC keyed MAC (SipHash):
// the server held a shared 128-bit secret per token and recomputed the MAC. Strictly stronger than
// layer 21's bare-token lookup — but the server still holds a secret capable of forging any client's
// proof. Layer 27 (broadcast_authsig) makes the credential an Ed25519 SIGNATURE the server VERIFIES
// with only a PUBLIC key: on accept the server sends a fresh CHALLENGE-001 nonce (reused verbatim
// from layer 26); the client responds with HELLO-003 = [token, signature], sig =
// Ed25519_sign(private_seed[token], nonce||token); PubkeyTable::authenticate verifies the signature
// under the enrolled PUBLIC key before binding the designated seat (a bad/forged/stale signature ->
// SPECTATOR, the same reject class as an unknown token). This bridge proves, over real 127.0.0.1
// sockets with NO sleeps / NO timing guesses, three claims:
//
//   LEG 1 (VERIFIED PUBLIC-KEY SEATS, INVARIANT TO JOIN ORDER — the headline): THREE clients each
//   hold a DISTINCT token AND its correct PRIVATE seed, are challenged, SIGN, and are bound to the
//   seat their token designates — deliberately token order != seat order (100->2, 200->0, 300->1).
//   Each upstreams ONLY that seat's commands (SCRAMBLED + adversarially chunked); their union is the
//   whole INPUT-SK-001 command set, so the produced downstream frames are BYTE-IDENTICAL to
//   session::build_server_frames. Same guarantee as layer 26, now gated on a public-key signature.
//
//   LEG 2 (VERIFY + AUTHORIZATION BOUNDARY — the new capability): THREE clients upstream the WHOLE
//   scenario. Client A holds token 200 AND its correct seed -> seat 0; its foreign-aircraft commands
//   are dropped. Client B knows token 300 but signs with the WRONG private key -> bad signature ->
//   SPECTATOR; ALL its commands dropped (crucially: even holding the server's ENTIRE public roster,
//   an attacker cannot sign — the headline over layer 26, whose server-held secret COULD forge).
//   Client C presents an UNKNOWN token 999 -> SPECTATOR; all dropped. All three receive the IDENTICAL
//   aircraft-0-only world byte-for-byte.
//
//   LEG 3 (SHA-512 + Ed25519 pins + HELLO-003 codec + PubkeyTable verify — in-process): the OFFICIAL
//   SHA-512("abc") vector, the Ed25519 fixed-seed pin (pubkey + signature, confirmed vs the
//   `cryptography` library in ed25519_ref.py), the HELLO-003 codec (known-encoding pin vs
//   tools/authsig_ref.py + round-trip + wrong-version reject), and PubkeyTable (verify, forgery-reject,
//   replay-reject, unknown->spectator, double-login->spectator, release-then-reclaim-own-seat) + the
//   reused seat_authorizes predicate — no sockets, no timing.
//
// Every socket assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; CHALLENGE-001/HELLO-003/BIND-001 are transport
// metadata (modelled on the framing envelope, NOT a sealed rails.wire block) ⇒ no seal. Rides v1.26r0.
#include "authsigserver.h"
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

// The server's session key (seeds the per-connection challenge nonce). A CSPRNG seed in production;
// FIXED here so the run is reproducible. Same values as the layer-26 bridge (the challenge is reused).
static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact 3-aircraft scenario the layer-15b..26 bridges use.
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

// The pre-shared roster: token -> (designated seat, PRIVATE seed). Token order != seat order (so a
// join-order policy would NOT reproduce it). The server enrolls only the derived PUBLIC key; a client
// that holds the matching PRIVATE seed can sign; a client with a wrong seed is a forger.
struct Credential { std::int64_t token; std::int64_t seat; std::uint8_t base; };
static const Credential ROSTER[] = {
    {100, 2, 0x10},
    {200, 0, 0x20},
    {300, 1, 0x30},
};

static void enroll_roster(netinput::PubkeyTable& creds) {
    for (const auto& c : ROSTER) {
        auto seed = make_seed(c.base);
        std::uint8_t pk[32];
        ed25519::public_key(seed.data(), pk);   // only the PUBLIC key is enrolled
        creds.enroll(c.token, c.seat, pk);
    }
}

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
// A generic socket leg: N clients. Each holds a TOKEN + the private SEED it USES to sign the
// challenge (which may differ from the enrolled key => a forger). Given the seat the server binds it
// (learned from BIND, after signing), a plan builds the commands it upstreams, plus a
// scramble/chunking. The server runs broadcast_authsig(min_initial = N) over an enrolled roster.
// ---------------------------------------------------------------------------------------------
struct ClientPlan {
    std::int64_t token;
    std::array<std::uint8_t, 32> seed;  // the private key the CLIENT signs with (a forger's differs)
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

static void run_authsig_socket_leg(const Rails& rails, const session::Scenario& sc,
                                   const std::vector<ClientPlan>& plans, netinput::Stats& stats_out,
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
        netinput::PubkeyTable creds(producer.n_aircraft());
        enroll_roster(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent_count == N; });
        };
        stats_out = netinput::broadcast_authsig(listener, producer, q, creds, SESSION_K0, SESSION_K1,
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

            // 2) answer with HELLO-003 = [token, signature], sig = Ed25519_sign(my seed, nonce||token).
            {
                std::uint8_t sig[64];
                authsig001::sign_challenge(plans[ci].seed.data(), nonce, plans[ci].token, sig);
                authsig001::Hello3Info hi;
                hi.token = plans[ci].token;
                hi.sig.assign(sig, sig + 64);
                std::vector<std::uint8_t> rec, framed;
                authsig001::encode_hello3(hi, rec);
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
            std::printf("FAIL layer-27 authsig leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each SIGN their challenge and fly the seat their token designates (token
// order != seat order); the union reproduces build_server_frames byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, make_seed(ROSTER[0].base), own_seat_cmds, /*chunk=*/1, /*reverse=*/true},   // ->seat 2
        {200, make_seed(ROSTER[1].base), own_seat_cmds, /*chunk=*/7, /*reverse=*/false},  // ->seat 0
        {300, make_seed(ROSTER[2].base), own_seat_cmds, /*chunk=*/3, /*reverse=*/true},   // ->seat 1
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authsig_socket_leg(rails, sc, plans, st, res, "leg1/3-verified-identities");

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
        std::printf("  LEG1 PASS: 3 identities each SIGNED a fresh challenge with its PRIVATE key and "
                    "flew the seat its TOKEN designates (100->2, 200->0, 300->1; scrambled/chunked) -> "
                    "%zu frames byte-identical to build_server_frames\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands are dropped; a FORGER (right token, wrong private
// key) and an UNKNOWN token are both seated as spectators and their commands ALL dropped; all three
// see the aircraft-0-only world byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A's non-seat-0 commands
    const std::size_t b_all = canon.size();                    // forger: all dropped
    const std::size_t c_all = canon.size();                    // unknown: all dropped
    const auto ref = run_input_frames(rails, sc, own0);         // only aircraft 0 ever commanded

    auto send_everything = [&](std::int64_t /*seat*/) { return canon; };
    std::vector<ClientPlan> plans = {
        // A: token 200 + CORRECT seed -> seat 0; upstreams everything, foreign rejected.
        {200, make_seed(ROSTER[1].base), send_everything, /*chunk=*/1, /*reverse=*/true},
        // B: token 300 but a WRONG private seed -> bad signature -> spectator; all rejected (the headline).
        {300, make_seed(0xBB), send_everything, /*chunk=*/5, /*reverse=*/false},
        // C: unknown token 999 -> spectator; all rejected.
        {999, make_seed(0xCC), send_everything, /*chunk=*/3, /*reverse=*/true},
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authsig_socket_leg(rails, sc, plans, st, res, "leg2/verify-boundary");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 3) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != own0.size() || st.cmds_unauth != (a_foreign + b_all + c_all) ||
        st.cmds_stale != 0 || st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg2: command accounting (ok=%zu unauth=%zu; want %zu own / %zu unauth)\n",
                    st.cmds_ok, st.cmds_unauth, own0.size(), a_foreign + b_all + c_all);
    }
    if (!res[0].bind_ok || res[0].bind.seat != 0) {
        ++fails;
        std::printf("FAIL leg2: client A (valid) should hold seat 0 (ok=%d seat=%lld)\n",
                    res[0].bind_ok ? 1 : 0, static_cast<long long>(res[0].bind.seat));
    }
    if (!res[1].bind_ok || res[1].bind.seat != bind001::SPECTATOR) {
        ++fails;
        std::printf("FAIL leg2: client B (FORGED signature) should be a SPECTATOR (ok=%d seat=%lld)\n",
                    res[1].bind_ok ? 1 : 0, static_cast<long long>(res[1].bind.seat));
    }
    if (!res[2].bind_ok || res[2].bind.seat != bind001::SPECTATOR) {
        ++fails;
        std::printf("FAIL leg2: client C (unknown token) should be a SPECTATOR (ok=%d seat=%lld)\n",
                    res[2].bind_ok ? 1 : 0, static_cast<long long>(res[2].bind.seat));
    }
    for (std::size_t i = 0; i < res.size(); ++i) {
        if (res[i].pending != 0 || !frames_equal(res[i].frames, ref)) {
            ++fails;
            std::printf("FAIL leg2: client %zu downstream NOT the aircraft-0-only world (got %zu of %zu, "
                        "pending %zu)\n", i, res[i].frames.size(), ref.size(), res[i].pending);
        }
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: a seat-0 client's %zu foreign commands + a FORGER's %zu (right token, "
                    "WRONG private key -> bad signature -> spectator) + an unknown-token's %zu all rejected "
                    "(cmds_unauth=%zu); the public roster alone cannot sign\n",
                    a_foreign, b_all, c_all, st.cmds_unauth);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: SHA-512 official vector + Ed25519 fixed pin + HELLO-003 codec pins + PubkeyTable verify.
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

    // Ed25519 fixed pin: seed = 00..1f, msg = "SEADS-ed25519-pin" (confirmed vs `cryptography` in
    // ed25519_ref.py). The SAME hex is asserted in the Python ref.
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
        bool ok = hex_eq(pk, 32, want_pk) && hex_eq(sig, 64, want_sig) &&
                  ed25519::verify(pk, reinterpret_cast<const std::uint8_t*>(m), std::strlen(m), sig);
        // tamper: a 1-bit flip in the signature must be rejected.
        std::uint8_t bad[64]; std::memcpy(bad, sig, 64); bad[0] ^= 1;
        ok = ok && !ed25519::verify(pk, reinterpret_cast<const std::uint8_t*>(m), std::strlen(m), bad);
        if (!ok) { ++fails; std::printf("FAIL leg3: Ed25519 fixed pin / verify\n"); }
    }

    // HELLO-003 known encoding: token=7 (zz 0x0E), siglen=2, sig=[0xAA,0xBB] -> [0x03,0x0E,0x02,0xAA,0xBB].
    // (The SAME bytes in tools/authsig_ref.py.)
    {
        authsig001::Hello3Info hi;
        hi.token = 7;
        hi.sig = {0xAA, 0xBB};
        std::vector<std::uint8_t> h3;
        authsig001::encode_hello3(hi, h3);
        const std::uint8_t wh[] = {0x03, 0x0E, 0x02, 0xAA, 0xBB};
        bool ok = h3.size() == sizeof(wh);
        for (std::size_t i = 0; ok && i < sizeof(wh); ++i) ok = (h3[i] == wh[i]);
        // round-trip a range of tokens with real 64-byte signatures.
        for (std::int64_t tok : {std::int64_t(-5), std::int64_t(0), std::int64_t(1) << 40}) {
            std::uint8_t seed[32];
            for (int i = 0; i < 32; ++i) seed[i] = static_cast<std::uint8_t>((tok + i) & 0xFF);
            std::uint8_t sig[64];
            authsig001::sign_challenge(seed, 12345, tok, sig);
            authsig001::Hello3Info h; h.token = tok; h.sig.assign(sig, sig + 64);
            std::vector<std::uint8_t> e; authsig001::encode_hello3(h, e);
            std::size_t pos = 0; authsig001::Hello3Info d;
            ok = ok && authsig001::decode_hello3(e.data(), e.size(), pos, d) && pos == e.size() &&
                 d.token == tok && d.sig.size() == 64 &&
                 std::memcmp(d.sig.data(), sig, 64) == 0;
        }
        // HELLO-003 (v3) is not interchangeable with HELLO-002 (v2) / HELLO-001 (v1) / CHALLENGE (v1):
        // a HELLO-003 decoder rejects those version bytes.
        std::uint8_t v2[] = {0x02, 0x0E, 0x02};
        std::uint8_t v1[] = {0x01, 0x0E};
        std::size_t p = 0; authsig001::Hello3Info dh;
        ok = ok && !authsig001::decode_hello3(v2, sizeof(v2), p, dh);
        p = 0; ok = ok && !authsig001::decode_hello3(v1, sizeof(v1), p, dh);
        if (!ok) { ++fails; std::printf("FAIL leg3: HELLO-003 codec parity vs authsig_ref\n"); }
    }

    // PubkeyTable: verify + forgery/replay reject, identity->seat invariant to order, unknown->spectator,
    // double-login->spectator, release-then-reclaim OWN seat.
    {
        auto pk_of = [](std::uint8_t base) {
            auto seed = make_seed(base);
            std::array<std::uint8_t, 32> pk{};
            ed25519::public_key(seed.data(), pk.data());
            return pk;
        };
        netinput::PubkeyTable ct(3);
        auto pk100 = pk_of(0x10), pk200 = pk_of(0x20), pk300 = pk_of(0x30);
        ct.enroll(100, 2, pk100.data());
        ct.enroll(200, 0, pk200.data());
        ct.enroll(300, 1, pk300.data());
        auto nonce_for = [](std::uint64_t counter) {
            return authmac001::derive_nonce(SESSION_K0, SESSION_K1, counter);
        };
        auto present = [&](std::int64_t token, std::uint8_t base, std::uint64_t counter) {
            std::uint64_t nonce = nonce_for(counter);
            auto seed = make_seed(base);
            std::uint8_t sig[64];
            authsig001::sign_challenge(seed.data(), nonce, token, sig);
            return ct.authenticate(token, nonce, sig, 64);
        };
        bool ok = present(300, 0x30, 0) == 1 &&
                  present(100, 0x10, 1) == 2 &&
                  present(200, 0x20, 2) == 0;
        // FORGERY: right token 100, WRONG private key -> spectator. Fresh table so the seat is free
        // and the reject is purely the signature.
        netinput::PubkeyTable ct2(3);
        ct2.enroll(100, 2, pk100.data());
        std::uint64_t nz = nonce_for(0);
        auto forger = make_seed(0x99);
        std::uint8_t forged[64];
        authsig001::sign_challenge(forger.data(), nz, 100, forged);  // attacker's own key
        ok = ok && ct2.authenticate(100, nz, forged, 64) == bind001::SPECTATOR;
        // REPLAY: a valid signature for one nonce is rejected under a different nonce.
        auto real100 = make_seed(0x10);
        std::uint8_t good[64];
        authsig001::sign_challenge(real100.data(), nz, 100, good);
        ok = ok && ct2.authenticate(100, nonce_for(99), good, 64) == bind001::SPECTATOR;
        // the genuine holder authenticates under the right nonce.
        ok = ok && ct2.authenticate(100, nz, good, 64) == 2;
        // unknown -> spectator; double-login -> spectator; release then reclaim OWN seat.
        ok = ok && present(999, 0x10, 3) == bind001::SPECTATOR;
        ok = ok && present(100, 0x10, 4) == bind001::SPECTATOR;  // seat 2 held on ct
        ct.release(2);
        ok = ok && present(100, 0x10, 5) == 2;
        if (!ok) { ++fails; std::printf("FAIL leg3: PubkeyTable verify/identity binding\n"); }
    }

    // authorization predicate (reused from layers 18/21/26): own seat only; a spectator commands nothing.
    {
        bool ok = netinput::seat_authorizes(0, 0) && netinput::seat_authorizes(2, 2) &&
                  !netinput::seat_authorizes(0, 1) && !netinput::seat_authorizes(1, 0) &&
                  !netinput::seat_authorizes(bind001::SPECTATOR, 0);
        if (!ok) { ++fails; std::printf("FAIL leg3: seat_authorizes predicate\n"); }
    }

    if (fails == 0)
        std::printf("  LEG3 PASS: SHA-512(\"abc\") vector + Ed25519 fixed pin (pubkey+sig, vs the "
                    "`cryptography` lib) + HELLO-003 codec pins + PubkeyTable (verify, forgery-reject, "
                    "replay-reject, unknown/double-login->spectator, reclaim-own-seat) + own-seat "
                    "authorization predicate\n");
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_leg3();  // in-process first (fast, no sockets)
    fails += run_leg1();
    fails += run_leg2();
    if (fails == 0) {
        std::printf("PASS: layer-27 asymmetric-signature binding — a client's seat is gated on a VERIFIED "
                    "Ed25519 SIGNATURE over a fresh server challenge (HELLO-003 proof vs an enrolled "
                    "PUBLIC key), invariant to join order; the server holds NO secret, so its whole public "
                    "roster cannot impersonate a client; N authenticated clients' inputs compose to "
                    "build_server_frames byte-for-byte\n");
        return 0;
    }
    std::printf("RESULT: layer-27 authsig bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
