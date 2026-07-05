// SEADS STRONG-CREDENTIAL determinism BRIDGE for the authoritative keyed-MAC auth server (LAYER 26).
//
// Layer 21 (broadcast_auth) bound a client's seat to an abstracted i64 `token` the server merely
// LOOKED UP — a public username with no proof of possession: anyone who knew a token could take its
// seat. Layer 26 (broadcast_authmac) makes the credential a keyed MAC over a server-issued CHALLENGE
// that the server VERIFIES against a pre-shared secret. On accept the server sends a fresh
// CHALLENGE-001 nonce; the client responds with HELLO-002 = [token, mac], mac =
// SipHash(secret[token], nonce||token); SecretTable::authenticate verifies the mac before binding the
// designated seat (a bad/forged/stale MAC -> SPECTATOR, the same reject class as an unknown token).
// This bridge proves, over real 127.0.0.1 sockets with NO sleeps / NO timing guesses, three claims:
//
//   LEG 1 (VERIFIED IDENTITY SEATS, INVARIANT TO JOIN ORDER — the headline): THREE clients each hold a
//   DISTINCT token AND its correct secret, are challenged, PROVE possession, and are bound to the seat
//   their token designates — deliberately token order != seat order (100->2, 200->0, 300->1). Each
//   upstreams ONLY that seat's commands (SCRAMBLED + adversarially chunked); their union is the whole
//   INPUT-SK-001 command set, so the produced downstream frames are BYTE-IDENTICAL to
//   session::build_server_frames. Same guarantee as layer 21, now gated on a verified proof.
//
//   LEG 2 (VERIFY + AUTHORIZATION BOUNDARY — the new capability): THREE clients upstream the WHOLE
//   scenario. Client A holds token 200 AND its correct secret -> seat 0; its foreign-aircraft commands
//   are dropped (a seated client cannot steer another plane). Client B knows token 300 but presents a
//   FORGED mac (wrong secret) -> SPECTATOR; ALL its commands dropped (knowing the token is NOT enough —
//   exactly what layer 21 could not stop). Client C presents an UNKNOWN token 999 -> SPECTATOR; all
//   dropped. All three receive the IDENTICAL aircraft-0-only world byte-for-byte.
//
//   LEG 3 (SipHash vector + CHALLENGE-001/HELLO-002 codecs + SecretTable verify — in-process): the
//   OFFICIAL SipHash-2-4 test vector, the two record codecs (known-encoding pins vs tools/authmac_ref.py
//   + round-trip + wrong-version reject), and SecretTable (verify, forgery-reject, replay-reject,
//   unknown->spectator, double-login->spectator, release-then-reclaim-own-seat) + the reused
//   seat_authorizes predicate — no sockets, no timing.
//
// Every socket assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; CHALLENGE-001/HELLO-002/BIND-001 are transport metadata
// (modelled on the framing envelope, NOT a sealed rails.wire block) ⇒ no seal. Rides v1.26r0.
#include "authmacserver.h"
#include "authmac001.h"
#include "siphash.h"
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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// FIXED here so the run is reproducible. The SAME key the clients would NOT need — they read their
// nonce from the CHALLENGE record — but the server uses it to derive each nonce.
static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact 3-aircraft scenario the layer-15b..23 bridges use.
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

// The pre-shared roster the server enrolls: token -> (designated seat, 128-bit secret). Token order !=
// seat order (so a join-order policy would NOT reproduce it). Clients that hold the matching secret
// can prove possession; a client with a wrong secret is a forger.
struct Credential { std::int64_t token; std::int64_t seat; std::uint64_t k0, k1; };
static const Credential ROSTER[] = {
    {100, 2, 0x0A11ULL, 0x0A12ULL},
    {200, 0, 0x0B21ULL, 0x0B22ULL},
    {300, 1, 0x0C31ULL, 0x0C32ULL},
};

static void enroll_roster(netinput::SecretTable& creds) {
    for (const auto& c : ROSTER) creds.enroll(c.token, c.seat, c.k0, c.k1);
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
// A generic socket leg: N clients. Each holds a TOKEN + the (k0,k1) SECRET it USES to answer the
// challenge (which may differ from the enrolled secret => a forger). Given the seat the server binds
// it (learned from BIND, after proving its credential), a plan builds the commands it upstreams, plus
// a scramble/chunking. The server runs broadcast_authmac(min_initial = N) over an enrolled roster.
// ---------------------------------------------------------------------------------------------
struct ClientPlan {
    std::int64_t token;
    std::uint64_t k0, k1;  // the secret the CLIENT uses to compute its mac (a forger's differs)
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

static void run_authmac_socket_leg(const Rails& rails, const session::Scenario& sc,
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
        netinput::SecretTable creds(producer.n_aircraft());
        enroll_roster(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent_count == N; });
        };
        stats_out = netinput::broadcast_authmac(listener, producer, q, creds, SESSION_K0, SESSION_K1,
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

            // 2) answer with HELLO-002 = [token, mac], mac = SipHash(my secret, nonce||token).
            {
                std::uint64_t mac = authmac001::compute_mac(plans[ci].k0, plans[ci].k1, nonce,
                                                            plans[ci].token);
                authmac001::Hello2Info hi{plans[ci].token, mac};
                std::vector<std::uint8_t> rec, framed;
                authmac001::encode_hello2(hi, rec);
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
            std::printf("FAIL layer-26 authmac leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each PROVE their credential and fly the seat their token designates (token
// order != seat order); the union reproduces build_server_frames byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, ROSTER[0].k0, ROSTER[0].k1, own_seat_cmds, /*chunk=*/1, /*reverse=*/true},   // ->seat 2
        {200, ROSTER[1].k0, ROSTER[1].k1, own_seat_cmds, /*chunk=*/7, /*reverse=*/false},  // ->seat 0
        {300, ROSTER[2].k0, ROSTER[2].k1, own_seat_cmds, /*chunk=*/3, /*reverse=*/true},   // ->seat 1
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authmac_socket_leg(rails, sc, plans, st, res, "leg1/3-verified-identities");

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
        std::printf("  LEG1 PASS: 3 identities each PROVED possession of its secret (challenge->MAC) and "
                    "flew the seat its TOKEN designates (100->2, 200->0, 300->1; scrambled/chunked) -> "
                    "%zu frames byte-identical to build_server_frames\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands are dropped; a FORGER (right token, wrong secret)
// and an UNKNOWN token are both seated as spectators and their commands ALL dropped; all three see
// the aircraft-0-only world byte-for-byte.
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
        // A: token 200 + CORRECT secret -> seat 0; upstreams everything, foreign rejected.
        {200, ROSTER[1].k0, ROSTER[1].k1, send_everything, /*chunk=*/1, /*reverse=*/true},
        // B: token 300 but a FORGED secret -> bad MAC -> spectator; all rejected (the headline).
        {300, 0xBADBAD0ULL, 0xBADBAD1ULL, send_everything, /*chunk=*/5, /*reverse=*/false},
        // C: unknown token 999 -> spectator; all rejected.
        {999, 0x0ULL, 0x0ULL, send_everything, /*chunk=*/3, /*reverse=*/true},
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_authmac_socket_leg(rails, sc, plans, st, res, "leg2/verify-boundary");

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
        std::printf("FAIL leg2: client B (FORGED mac) should be a SPECTATOR (ok=%d seat=%lld)\n",
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
                    "wrong secret -> bad MAC -> spectator) + an unknown-token's %zu all rejected "
                    "(cmds_unauth=%zu); knowing the token is NOT enough — the MAC must verify\n",
                    a_foreign, b_all, c_all, st.cmds_unauth);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: SipHash official vector + CHALLENGE-001/HELLO-002 codec pins + SecretTable verify (in-process).
// ---------------------------------------------------------------------------------------------
static int run_leg3() {
    int fails = 0;

    // OFFICIAL SipHash-2-4 vector: key = 00..0f, msg = 00..0e (15 bytes) -> 0xa129ca6149be45e5.
    {
        std::uint64_t k0 = 0, k1 = 0;
        for (int i = 0; i < 8; ++i) k0 |= static_cast<std::uint64_t>(i) << (8 * i);
        for (int i = 0; i < 8; ++i) k1 |= static_cast<std::uint64_t>(i + 8) << (8 * i);
        std::uint8_t msg[15];
        for (int i = 0; i < 15; ++i) msg[i] = static_cast<std::uint8_t>(i);
        std::uint64_t out = siphash::siphash24(k0, k1, msg, sizeof(msg));
        if (out != 0xA129CA6149BE45E5ULL) {
            ++fails;
            std::printf("FAIL leg3: SipHash-2-4 official vector %016llx != a129ca6149be45e5\n",
                        static_cast<unsigned long long>(out));
        }
    }

    // CHALLENGE-001 known encoding: nonce=7 -> zz 14 -> [0x01, 0x0E]. HELLO-002 known encoding:
    // token=7 (zz 0x0E), mac=1 (zz 0x02) -> [0x02, 0x0E, 0x02]. (The SAME bytes in tools/authmac_ref.py.)
    {
        std::vector<std::uint8_t> cg, h2;
        authmac001::encode_challenge({7}, cg);
        authmac001::encode_hello2({7, 1}, h2);
        const std::uint8_t wc[] = {0x01, 0x0E};
        const std::uint8_t wh[] = {0x02, 0x0E, 0x02};
        bool ok = cg.size() == sizeof(wc) && h2.size() == sizeof(wh);
        for (std::size_t i = 0; ok && i < sizeof(wc); ++i) ok = (cg[i] == wc[i]);
        for (std::size_t i = 0; ok && i < sizeof(wh); ++i) ok = (h2[i] == wh[i]);
        // round-trip a range (nonce full-width; token incl. negative; big mac)
        for (std::uint64_t nonce : {std::uint64_t(0), std::uint64_t(300), ~std::uint64_t(0)}) {
            std::vector<std::uint8_t> e; authmac001::encode_challenge({nonce}, e);
            std::size_t pos = 0; authmac001::ChallengeInfo d;
            ok = ok && authmac001::decode_challenge(e.data(), e.size(), pos, d) && pos == e.size() &&
                 d.nonce == nonce;
        }
        for (std::int64_t tok : {std::int64_t(-5), std::int64_t(0), std::int64_t(1) << 40}) {
            std::vector<std::uint8_t> e; authmac001::encode_hello2({tok, 0xDEADBEEFFEEDFACEULL}, e);
            std::size_t pos = 0; authmac001::Hello2Info d;
            ok = ok && authmac001::decode_hello2(e.data(), e.size(), pos, d) && pos == e.size() &&
                 d.token == tok && d.mac == 0xDEADBEEFFEEDFACEULL;
        }
        // the two records are NOT interchangeable: a CHALLENGE (v1) decoder rejects a HELLO-002 (v2)
        // record and vice-versa.
        std::uint8_t v2[] = {0x02, 0x0E, 0x02};
        std::uint8_t v1[] = {0x01, 0x0E};
        std::size_t p = 0; authmac001::ChallengeInfo dc; authmac001::Hello2Info dh;
        ok = ok && !authmac001::decode_challenge(v2, sizeof(v2), p, dc);
        p = 0; ok = ok && !authmac001::decode_hello2(v1, sizeof(v1), p, dh);
        if (!ok) { ++fails; std::printf("FAIL leg3: CHALLENGE-001/HELLO-002 codec parity vs authmac_ref\n"); }
    }

    // SecretTable: verify + forgery/replay reject, identity->seat invariant to order, unknown->spectator,
    // double-login->spectator, release-then-reclaim OWN seat.
    {
        netinput::SecretTable ct(3);
        ct.enroll(100, 2, 0x0A11ULL, 0x0A12ULL);
        ct.enroll(200, 0, 0x0B21ULL, 0x0B22ULL);
        ct.enroll(300, 1, 0x0C31ULL, 0x0C32ULL);
        auto nonce_for = [](std::uint64_t counter) {
            return authmac001::derive_nonce(SESSION_K0, SESSION_K1, counter);
        };
        auto present = [&](std::int64_t token, std::uint64_t k0, std::uint64_t k1, std::uint64_t counter) {
            std::uint64_t nonce = nonce_for(counter);
            std::uint64_t mac = authmac001::compute_mac(k0, k1, nonce, token);
            return ct.authenticate(token, nonce, mac);
        };
        bool ok = present(300, 0x0C31ULL, 0x0C32ULL, 0) == 1 &&
                  present(100, 0x0A11ULL, 0x0A12ULL, 1) == 2 &&
                  present(200, 0x0B21ULL, 0x0B22ULL, 2) == 0;
        // FORGERY: right token 300 (its seat 1 is now held anyway), wrong secret -> spectator. Use a
        // fresh table so the seat is free and the reject is purely the MAC.
        netinput::SecretTable ct2(3);
        ct2.enroll(100, 2, 0x0A11ULL, 0x0A12ULL);
        std::uint64_t nz = nonce_for(0);
        std::uint64_t forged = authmac001::compute_mac(0xBADULL, 0xBADULL, nz, 100);
        ok = ok && ct2.authenticate(100, nz, forged) == bind001::SPECTATOR;
        // REPLAY: a valid MAC for one nonce is rejected under a different nonce.
        std::uint64_t good = authmac001::compute_mac(0x0A11ULL, 0x0A12ULL, nz, 100);
        ok = ok && ct2.authenticate(100, nonce_for(99), good) == bind001::SPECTATOR;
        // the genuine holder authenticates under the right nonce.
        ok = ok && ct2.authenticate(100, nz, good) == 2;
        // unknown -> spectator; double-login -> spectator; release then reclaim OWN seat.
        ok = ok && present(999, 0, 0, 3) == bind001::SPECTATOR;
        ok = ok && present(100, 0x0A11ULL, 0x0A12ULL, 4) == bind001::SPECTATOR;  // seat 2 held on ct
        ct.release(2);
        ok = ok && present(100, 0x0A11ULL, 0x0A12ULL, 5) == 2;
        if (!ok) { ++fails; std::printf("FAIL leg3: SecretTable verify/identity binding\n"); }
    }

    // authorization predicate (reused from layers 18/21): own seat only; a spectator commands nothing.
    {
        bool ok = netinput::seat_authorizes(0, 0) && netinput::seat_authorizes(2, 2) &&
                  !netinput::seat_authorizes(0, 1) && !netinput::seat_authorizes(1, 0) &&
                  !netinput::seat_authorizes(bind001::SPECTATOR, 0);
        if (!ok) { ++fails; std::printf("FAIL leg3: seat_authorizes predicate\n"); }
    }

    if (fails == 0)
        std::printf("  LEG3 PASS: SipHash-2-4 official vector + CHALLENGE-001/HELLO-002 codec pins + "
                    "SecretTable (verify, forgery-reject, replay-reject, unknown/double-login->spectator, "
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
        std::printf("PASS: layer-26 strong-credential binding — a client's seat is gated on a VERIFIED "
                    "keyed MAC over a fresh server challenge (HELLO-002 proof vs a pre-shared secret), "
                    "invariant to join order; a forger who knows only the token gets no aircraft; N "
                    "authenticated clients' inputs compose to build_server_frames byte-for-byte\n");
        return 0;
    }
    std::printf("RESULT: layer-26 authmac bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
