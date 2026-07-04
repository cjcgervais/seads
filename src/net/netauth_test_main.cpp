// SEADS AUTHENTICATED-BINDING determinism BRIDGE for the authoritative auth server (netcode LAYER 21).
//
// Layer 18 (broadcast_bound) bound each client to its own aircraft by JOIN-ORDER POSITION. Layer 21
// (broadcast_auth) makes the binding a function of the client's IDENTITY: a joining client sends a
// HELLO-001 credential FIRST, the server looks the token up in a pre-shared CredentialTable to a
// DESIGNATED seat (invariant to join order; unknown token -> spectator), replies with the SAME BIND-001
// record, and authorizes upstream commands with the SAME seat_authorizes predicate. This bridge proves,
// over real 127.0.0.1 sockets with NO sleeps / NO timing guesses, three claims:
//
//   LEG 1 (IDENTITY-BASED SEATS, INVARIANT TO JOIN ORDER — the headline): THREE clients each present a
//   DISTINCT token and are bound to the seat their token DESIGNATES — deliberately with token order !=
//   seat order (token 100->seat 2, 200->seat 0, 300->seat 1). Each learns its seat from BIND, upstreams
//   ONLY that seat's commands (SCRAMBLED + adversarially chunked), and — because the roster is a fixed
//   function of identity — each client draws EXACTLY its designated seat regardless of which thread's
//   connection the OS accepted first. Their union is the whole INPUT-SK-001 command set, so the produced
//   downstream frames are BYTE-IDENTICAL to session::build_server_frames. (Layer 18 could only assert
//   "distinct seats"; here we assert the SPECIFIC seat per identity — the positional caveat is gone.)
//
//   LEG 2 (AUTHENTICATION + AUTHORIZATION BOUNDARY): TWO clients upstream the WHOLE scenario. Client A
//   presents a VALID token bound to seat 0 and its foreign-aircraft commands are dropped (a seated client
//   cannot steer another player's plane). Client B presents an UNKNOWN token, is seated as a SPECTATOR,
//   and ALL its commands are dropped (no credential -> no aircraft -> steers nothing). Both receive the
//   IDENTICAL downstream — the aircraft-0-only world byte-for-byte — so neither the foreign commands nor
//   the unauthenticated client changed anything.
//
//   LEG 3 (HELLO-001 CODEC + CredentialTable — in-process): the HELLO-001 codec (a shared known-encoding
//   pin vs tools/auth_ref.py + round-trip incl. a negative token + wrong-version reject), and the
//   CredentialTable (identity->seat invariant to enrollment/auth order, unknown->spectator,
//   double-login->spectator, release-then-reclaim-OWN-seat), and the reused seat_authorizes predicate —
//   no sockets, no timing.
//
// Every socket assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps; on_frame(0)
// blocks until every client has sent its commands, so they are ingested before their apply_tick). A
// finite watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; HELLO-001/BIND-001 are transport metadata (modelled on
// the layer-7 framing envelope, NOT a sealed rails.wire block) ⇒ no seal. Rides v1.26r0.
#include "authserver.h"
#include "boundserver.h"
#include "inputserver.h"
#include "input001.h"
#include "hello001.h"
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

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact (dyadic) 3-aircraft scenario the layer-15b..20 bridges use, so a
// grid-exact command survives the lossy wire UNCHANGED and the input path can be byte-identical to the
// phase-schedule server. Schedules exercise hold-last, bank/g/throttle changes, and firing.
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

// The pre-shared roster the server enrolls: token -> designated seat, token order != seat order (so a
// join-order policy would NOT reproduce it). The SAME mapping is asserted client-side.
struct Credential { std::int64_t token; std::int64_t seat; };
static const Credential ROSTER[] = {{100, 2}, {200, 0}, {300, 1}};

static void enroll_roster(netinput::CredentialTable& creds) {
    for (const auto& c : ROSTER) creds.enroll(c.token, c.seat);
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

// Run the input-driven producer in-process to exhaustion, submitting `cmds` (any order) up front.
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
// A generic socket leg: N clients, each with a TOKEN it presents in HELLO-001, and a plan that — GIVEN
// the seat the server binds it (learned from its BIND handshake) — produces the commands it upstreams,
// plus a scramble/chunking. The server runs broadcast_auth(min_initial = N) over an enrolled roster.
// Returns the server Stats and, per client, its decoded BIND + the snapshot frames it received.
// ---------------------------------------------------------------------------------------------
struct ClientPlan {
    std::int64_t token;
    std::function<std::vector<input001::InputCommand>(std::int64_t seat)> build;
    std::size_t chunk;
    bool reverse;
};
struct ClientResult {
    bind001::BindInfo bind;
    bool bind_ok = false;
    std::vector<std::vector<std::uint8_t>> frames;  // snapshot frames (after the BIND record)
    std::size_t pending = 0;
    std::size_t sent_cmds = 0;
};

static void run_auth_socket_leg(const Rails& rails, const session::Scenario& sc,
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
    const int expect_done = static_cast<int>(N) + 1;  // N clients + server

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
        netinput::CredentialTable creds(producer.n_aircraft());
        enroll_roster(creds);
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent_count == N; });
        };
        stats_out = netinput::broadcast_auth(listener, producer, q, creds, /*min_initial=*/N,
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

            // 1) send the HELLO-001 identity claim FIRST (as one framing frame).
            {
                hello001::HelloInfo hi{plans[ci].token};
                std::vector<std::uint8_t> rec, framed;
                hello001::encode_hello(hi, rec);
                framing::encode_frame(rec, framed);
                netsock::send_all(s, framed);
            }

            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> all;
            std::uint8_t buf[4096];
            // 2) read until the BIND handshake (the first downstream framing frame) is complete.
            while (all.empty()) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            ClientResult& res = results_out[ci];
            if (!all.empty()) {
                std::size_t pos = 0;
                res.bind_ok = bind001::decode_bind(all[0].data(), all[0].size(), pos, res.bind) &&
                              pos == all[0].size();
            }
            // 3) build this client's commands from the seat the server bound it, scramble, send UP.
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
            // 4) read the downstream snapshots to EOF (frames after the BIND).
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            res.pending = r.pending();
            for (std::size_t i = 1; i < all.size(); ++i) res.frames.push_back(all[i]);
            netsock::close_socket(s);
            ++done;
        });
    }

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < expect_done; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < expect_done) {
            std::printf("FAIL layer-21 auth leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each fly the seat their TOKEN designates (token order != seat order); the
// union reproduces build_server_frames byte-for-byte, and each client draws its SPECIFIC roster seat.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    // Every client uses the SAME rule — "given my BOUND seat, upstream exactly that aircraft's commands"
    // — so the leg is robust to accept order; the token fixes WHICH seat each client is bound to.
    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, own_seat_cmds, /*chunk=*/1, /*reverse=*/true},   // token 100 -> seat 2
        {200, own_seat_cmds, /*chunk=*/7, /*reverse=*/false},  // token 200 -> seat 0
        {300, own_seat_cmds, /*chunk=*/3, /*reverse=*/true},   // token 300 -> seat 1
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_auth_socket_leg(rails, sc, plans, st, res, "leg1/3-identities");

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
    // each client was bound to EXACTLY the seat its token designates (the headline: identity, not
    // position), n_aircraft==3, and received the identical byte-exact downstream.
    for (std::size_t i = 0; i < res.size(); ++i) {
        const ClientResult& c = res[i];
        const std::int64_t want_seat = ROSTER[i].seat;  // plans[i].token == ROSTER[i].token
        if (!c.bind_ok || c.bind.n_aircraft != 3 || c.bind.seat != want_seat) {
            ++fails;
            std::printf("FAIL leg1: client %zu (token %lld) bound to seat %lld, want %lld (n=%lld)\n", i,
                        static_cast<long long>(ROSTER[i].token), static_cast<long long>(c.bind.seat),
                        static_cast<long long>(want_seat), static_cast<long long>(c.bind.n_aircraft));
        }
        if (c.pending != 0 || !frames_equal(c.frames, ref)) {
            ++fails;
            std::printf("FAIL leg1: client %zu downstream NOT byte-identical to build_server_frames "
                        "(got %zu of %zu, pending %zu)\n", i, c.frames.size(), ref.size(), c.pending);
        }
    }
    if (fails == 0)
        std::printf("  LEG1 PASS: 3 identities each flew the seat their TOKEN designates (100->2, 200->0, "
                    "300->1; scrambled/chunked) -> %zu frames byte-identical to build_server_frames; "
                    "seat bound by identity, invariant to join order\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands are dropped AND an unknown-token spectator's commands
// are ALL dropped; both see the aircraft-0-only world byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A: its non-seat-0 commands
    const std::size_t b_all = canon.size();                    // client B (spectator): all commands
    // reference: only aircraft 0 ever commanded (the others hold neutral).
    const auto ref = run_input_frames(rails, sc, own0);

    // Client A: valid token 200 (seat 0) but upstreams EVERYTHING. Client B: unknown token 999
    // (spectator) upstreams EVERYTHING. Both send the whole canonical set regardless of their seat.
    auto send_everything = [&](std::int64_t /*seat*/) { return canon; };
    std::vector<ClientPlan> plans = {
        {200, send_everything, /*chunk=*/1, /*reverse=*/true},   // valid -> seat 0, foreign rejected
        {999, send_everything, /*chunk=*/5, /*reverse=*/false},  // unknown -> spectator, all rejected
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_auth_socket_leg(rails, sc, plans, st, res, "leg2/auth-boundary");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 2) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != own0.size() || st.cmds_unauth != (a_foreign + b_all) ||
        st.cmds_stale != 0 || st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg2: command accounting (ok=%zu unauth=%zu; want %zu own / %zu unauth)\n",
                    st.cmds_ok, st.cmds_unauth, own0.size(), a_foreign + b_all);
    }
    // client A (token 200) bound to seat 0; client B (token 999) is a spectator (no aircraft).
    if (!res[0].bind_ok || res[0].bind.seat != 0 || res[0].bind.n_aircraft != 3) {
        ++fails;
        std::printf("FAIL leg2: client A BIND (ok=%d seat=%lld; want seat 0)\n", res[0].bind_ok ? 1 : 0,
                    static_cast<long long>(res[0].bind.seat));
    }
    if (!res[1].bind_ok || res[1].bind.seat != bind001::SPECTATOR || res[1].bind.n_aircraft != 3) {
        ++fails;
        std::printf("FAIL leg2: client B (unknown token) should be a SPECTATOR (ok=%d seat=%lld)\n",
                    res[1].bind_ok ? 1 : 0, static_cast<long long>(res[1].bind.seat));
    }
    for (std::size_t i = 0; i < res.size(); ++i) {
        if (res[i].pending != 0 || !frames_equal(res[i].frames, ref)) {
            ++fails;
            std::printf("FAIL leg2: client %zu downstream NOT the aircraft-0-only world (got %zu of %zu, "
                        "pending %zu)\n", i, res[i].frames.size(), ref.size(), res[i].pending);
        }
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: a seat-0 client's %zu foreign commands + an unknown-token spectator's "
                    "%zu commands all rejected (cmds_unauth=%zu); both see the aircraft-0-only world "
                    "byte-for-byte — no credential, no aircraft; own seat only\n",
                    a_foreign, b_all, st.cmds_unauth);
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 3: HELLO-001 codec pin + CredentialTable + authorization predicate (in-process).
// ---------------------------------------------------------------------------------------------
static int run_leg3() {
    int fails = 0;

    // HELLO-001 known-encoding pin (SAME bytes asserted in tools/auth_ref.py): version 0x01,
    // token=7 -> zz 14 -> 0x0E.
    {
        hello001::HelloInfo h{7};
        std::vector<std::uint8_t> got;
        hello001::encode_hello(h, got);
        const std::uint8_t want[] = {0x01, 0x0E};
        bool ok = got.size() == sizeof(want);
        for (std::size_t i = 0; ok && i < sizeof(want); ++i) ok = (got[i] == want[i]);
        // round-trip a range of tokens incl. a negative (ZigZag carries the sign)
        for (std::int64_t tok : {std::int64_t(-5), std::int64_t(-1), std::int64_t(0),
                                 std::int64_t(300), std::int64_t(1) << 40}) {
            std::vector<std::uint8_t> enc;
            hello001::encode_hello({tok}, enc);
            std::size_t pos = 0;
            hello001::HelloInfo d;
            ok = ok && hello001::decode_hello(enc.data(), enc.size(), pos, d) && pos == enc.size() &&
                 d.token == tok;
        }
        // a wrong version byte is rejected
        std::uint8_t badver[] = {0x02, 0x0E};
        std::size_t bpos = 0;
        hello001::HelloInfo bd;
        ok = ok && !hello001::decode_hello(badver, sizeof(badver), bpos, bd);
        if (!ok) { ++fails; std::printf("FAIL leg3: HELLO-001 codec parity vs auth_ref.py\n"); }
    }

    // CredentialTable: identity determines the seat, INVARIANT to enrollment/auth order (token order !=
    // seat order); unknown -> spectator; double-login -> spectator; release then reclaim OWN seat.
    {
        netinput::CredentialTable ct(3);
        ct.enroll(100, 2);
        ct.enroll(200, 0);
        ct.enroll(300, 1);
        // authenticate in a DIFFERENT order than enrollment; each still gets ITS roster seat.
        bool ok = ct.authenticate(300) == 1 && ct.authenticate(100) == 2 && ct.authenticate(200) == 0;
        ok = ok && ct.authenticate(999) == bind001::SPECTATOR;   // unknown -> spectator
        ok = ok && ct.authenticate(100) == bind001::SPECTATOR;   // double-login -> spectator
        ct.release(2);                                           // token 100's seat freed
        ok = ok && ct.authenticate(100) == 2;                   // the SAME identity reclaims seat 2
        if (!ok) { ++fails; std::printf("FAIL leg3: CredentialTable identity binding\n"); }
    }

    // authorization predicate (reused from layer 18): own seat only; a spectator commands nothing.
    {
        bool ok = netinput::seat_authorizes(0, 0) && netinput::seat_authorizes(2, 2) &&
                  !netinput::seat_authorizes(0, 1) && !netinput::seat_authorizes(1, 0) &&
                  !netinput::seat_authorizes(bind001::SPECTATOR, 0);
        if (!ok) { ++fails; std::printf("FAIL leg3: seat_authorizes predicate\n"); }
    }

    if (fails == 0)
        std::printf("  LEG3 PASS: HELLO-001 codec pin + CredentialTable (identity->seat invariant to "
                    "order, unknown->spectator, double-login->spectator, reclaim-own-seat) + own-seat "
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
        std::printf("PASS: layer-21 authenticated binding — a client's seat is a function of its IDENTITY "
                    "(HELLO-001 credential -> CredentialTable seat), invariant to join order; an unknown "
                    "identity gets no aircraft; N authenticated clients' inputs compose to "
                    "build_server_frames byte-for-byte\n");
        return 0;
    }
    std::printf("RESULT: layer-21 auth bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
