// SEADS AUTHENTICATED + ASYNC determinism BRIDGE (netcode LAYER 22): layer-21 IDENTITY binding merged
// with the layer-16/19 downstream OUTPUT HYGIENE (async send buffers / byte-cap / liveness reap).
//
// Layer 21 (broadcast_auth) bound each client to its designated seat by IDENTITY (a HELLO-001 credential
// looked up in a pre-shared CredentialTable) but kept broadcast_input's BLOCKING downstream. Layer 19
// (broadcast_bound_async) gave the JOIN-ORDER bound server the async/byte-cap/liveness hygiene. Layer 22
// (broadcast_auth_async) is their product — an IDENTITY-authenticated server with the async hygiene — the
// two axes (admission vs delivery) being orthogonal. This bridge proves, over real 127.0.0.1 sockets with
// NO sleeps / NO timing guesses, that the composition preserves BOTH claims:
//
//   LEG 1 (identity binding survives the async path — the merge anchor): THREE clients each present a
//   DISTINCT token (token order != seat order: 100->2, 200->0, 300->1), learn their seat from the BIND
//   handshake, and upstream ONLY that seat's commands (SCRAMBLED + adversarially chunked) while the server
//   runs the FULL async path (non-blocking per-client send buffers, cap=0, liveness=0). Their union is the
//   whole INPUT-SK-001 command set, so each client's downstream — after its BIND — is BYTE-IDENTICAL to
//   session::build_server_frames, and each draws EXACTLY its designated seat regardless of accept order.
//   (This is layer-21 LEG 1 driven through the layer-19 async send path.)
//
//   LEG 2 (authentication + authorization survive the async path): TWO clients upstream the WHOLE scenario
//   through the async path. Client A presents a VALID token bound to seat 0 (its foreign-aircraft commands
//   are dropped); Client B presents an UNKNOWN token, is seated as a SPECTATOR (all its commands dropped).
//   Both receive the aircraft-0-only world byte-for-byte — neither the foreign commands nor the
//   unauthenticated client changed anything, through the async buffers.
//
//   LEG 3 (a dead AUTHENTICATED client is bounded WITHOUT disturbing the sim — hygiene composes with
//   identity): TWO authenticated clients on a LONG scenario through a pinned tiny kernel send buffer. FAST
//   presents token 200 (seat 0), upstreams ITS OWN seat's commands once, and is drained every frame by the
//   on_frame hook => never dropped, its downstream (after BIND) byte-identical to the aircraft-0-only
//   reference, and its commands drove the sim. DEAD presents token 300 (seat 1), reads/sends nothing =>
//   dropped by the policy under test (liveness reap at cap=0, sub-leg A; byte-cap shed at liveness=0,
//   sub-leg B) and its SEAT (1) is freed (leaves==1). The drop changes nothing about FAST's bytes or the
//   produced frames. DEAD's delivered bytes are its BIND(seat 1) record then a strict frame prefix — the
//   async buffer delivered the handshake FIRST.
//
// Every socket assertion is on delivered BYTES; all rendezvous are cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT-ONLY: no kernel/det_math/rails/wire/golden touched; broadcast_auth_async is a SIBLING of
// broadcast_auth + broadcast_bound_async (sealed broadcast.cpp + both untouched); HELLO-001/BIND-001 are
// transport metadata (layer-7 framing-envelope category, NOT a sealed rails.wire block) => no seal. Rides
// v1.26r0.
#include "authasyncserver.h"
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
// INPUT-SK-001: the SAME grid-exact (dyadic) 3-aircraft scenario the layer-15b..21 bridges use, so a
// grid-exact command survives the lossy wire UNCHANGED and the input path is byte-identical to the
// phase-schedule server. `ticks` is a parameter: a short run for LEG 1/2 (fast anchors) and a long run
// for LEG 3 (a ~1 MB downstream to overflow a never-reading client's buffers).
// ---------------------------------------------------------------------------------------------
static const session::Phase AA_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase AA_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase AA_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec AA_AC[3];
static const session::Scenario aa_scenario(unsigned ticks) {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) AA_AC[i] = sess_vec::AIRCRAFT[i];  // envelope + start state
        AA_AC[0].sched = AA_P0; AA_AC[0].n_phase = 3;
        AA_AC[1].sched = AA_P1; AA_AC[1].n_phase = 2;
        AA_AC[2].sched = AA_P2; AA_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    session::Scenario sc = {AA_AC, 3u, ticks, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
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

static std::vector<std::uint8_t> encode_upstream(const std::vector<input001::InputCommand>& cmds) {
    std::vector<std::uint8_t> upstream;
    for (const auto& c : cmds) {
        std::vector<std::uint8_t> rec, framed;
        input001::encode_command(c, rec);
        framing::encode_frame(rec, framed);
        upstream.insert(upstream.end(), framed.begin(), framed.end());
    }
    return upstream;
}

static std::vector<std::uint8_t> hello_frame(std::int64_t token) {
    hello001::HelloInfo hi{token};
    std::vector<std::uint8_t> rec, framed;
    hello001::encode_hello(hi, rec);
    framing::encode_frame(rec, framed);
    return framed;
}

// =============================================================================================
// LEG 1 / LEG 2 harness: N clients, each with a TOKEN it presents in HELLO-001 and a plan that — GIVEN the
// seat the server binds it (learned from its BIND handshake) — produces the commands it upstreams, plus a
// scramble/chunking. The server runs broadcast_auth_async(min_initial = N) through the FULL async path
// (cap=0, liveness=0) over an enrolled roster. Returns the server Stats and, per client, its decoded BIND +
// the snapshot frames it received.
// =============================================================================================
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
};

static void run_auth_async_leg(const Rails& rails, const session::Scenario& sc,
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
        // FULL async path: cap_bytes=0, liveness_frames=0 (no downstream drop can occur).
        stats_out = netinput::broadcast_auth_async(listener, producer, q, creds, /*min_initial=*/N,
                                                   /*accept_deadline_ms=*/10000, on_frame,
                                                   /*cap_bytes=*/0, /*liveness_frames=*/0);
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
                const std::vector<std::uint8_t> h = hello_frame(plans[ci].token);
                netsock::send_all(s, h);
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
            std::vector<std::uint8_t> upstream = encode_upstream(cmds);
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
            std::printf("FAIL layer-22 auth-async leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three identities each fly the seat their TOKEN designates through the ASYNC path; the union
// reproduces build_server_frames byte-for-byte, and each draws its SPECIFIC roster seat.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    auto own_seat_cmds = [&](std::int64_t seat) { return filter_by_aircraft(canon, seat); };
    std::vector<ClientPlan> plans = {
        {100, own_seat_cmds, /*chunk=*/1, /*reverse=*/true},   // token 100 -> seat 2
        {200, own_seat_cmds, /*chunk=*/7, /*reverse=*/false},  // token 200 -> seat 0
        {300, own_seat_cmds, /*chunk=*/3, /*reverse=*/true},   // token 300 -> seat 1
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_auth_async_leg(rails, sc, plans, st, res, "leg1/3-identities-async");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 3) {
        ++fails;
        std::printf("FAIL leg1: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != canon.size() || st.cmds_unauth != 0 || st.cmds_stale != 0 || st.cmds_oob != 0 ||
        st.capped != 0 || st.reaped != 0) {
        ++fails;
        std::printf("FAIL leg1: accounting (ok=%zu unauth=%zu stale=%zu oob=%zu capped=%zu reaped=%zu; "
                    "want %zu/0/0/0/0/0)\n", st.cmds_ok, st.cmds_unauth, st.cmds_stale, st.cmds_oob,
                    st.capped, st.reaped, canon.size());
    }
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
                    "300->1; scrambled/chunked) through the ASYNC path -> %zu frames byte-identical to "
                    "build_server_frames; seat bound by identity, zero unauthorized/capped/reaped\n",
                    ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands are dropped AND an unknown-token spectator's commands
// are ALL dropped, through the async path; both see the aircraft-0-only world byte-for-byte.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/150u);
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A: its non-seat-0 commands
    const std::size_t b_all = canon.size();                    // client B (spectator): all commands
    const auto ref = run_input_frames(rails, sc, own0);        // only aircraft 0 ever commanded

    auto send_everything = [&](std::int64_t /*seat*/) { return canon; };
    std::vector<ClientPlan> plans = {
        {200, send_everything, /*chunk=*/1, /*reverse=*/true},   // valid -> seat 0, foreign rejected
        {999, send_everything, /*chunk=*/5, /*reverse=*/false},  // unknown -> spectator, all rejected
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_auth_async_leg(rails, sc, plans, st, res, "leg2/auth-boundary-async");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 2) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != own0.size() || st.cmds_unauth != (a_foreign + b_all) || st.cmds_stale != 0 ||
        st.cmds_oob != 0 || st.capped != 0 || st.reaped != 0) {
        ++fails;
        std::printf("FAIL leg2: accounting (ok=%zu unauth=%zu; want %zu own / %zu unauth, 0 hygiene)\n",
                    st.cmds_ok, st.cmds_unauth, own0.size(), a_foreign + b_all);
    }
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
                    "%zu commands all rejected (cmds_unauth=%zu) through the async path; both see the "
                    "aircraft-0-only world byte-for-byte\n", a_foreign, b_all, st.cmds_unauth);
    return fails;
}

// =============================================================================================
// LEG 3: a dead AUTHENTICATED client is bounded (reaped/capped) + its SEAT freed without disturbing a
// cooperative seated client. FAST presents token 200 (seat 0), hook-drained, upstreams ONLY seat-0
// commands => byte-identical downstream (after BIND) to the aircraft-0-only reference, its commands drive
// the sim. DEAD presents token 300 (seat 1), sends/reads nothing => dropped by the policy under test, its
// seat (1) freed (leaves==1). `use_liveness` selects the policy (liveness reap at cap=0, or byte-cap at
// liveness=0). Because seats are bound by IDENTITY, FAST is deterministically seat 0 and DEAD seat 1 —
// there is no accept-order ambiguity (the layer-19 leg had to discover FAST's seat dynamically).
// =============================================================================================
static int run_hygiene_leg(bool use_liveness, const char* label) {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/20000u);  // ~4000 frames, ~1 MB downstream
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const auto ref = run_input_frames(rails, sc, own0);          // FAST is seat 0: aircraft-0-only world
    const std::size_t n_own = own0.size();

    const int kBufBytes = 16 * 1024;      // tiny kernel send buffer: DEAD stalls quickly
    const std::size_t kLiveness = use_liveness ? 16 : 0;
    const std::size_t kCap = use_liveness ? 0 : 128 * 1024;  // shed DEAD's backlog past 128 KiB

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    bool commands_sent = false;
    netsock::socket_t fast_sock = netsock::invalid_socket();  // FAST's endpoint (hook drains down)
    std::vector<std::uint8_t> fast_bytes;

    netinput::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/3);
        if (netsock::is_valid(listener)) {
            netsock::set_sndbuf(listener, kBufBytes);  // accepted sockets inherit the tiny send buffer
            netsock::set_nonblocking(listener);
        }
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
            if (fi == 0) {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return commands_sent; });
            }
            netsock::socket_t s;
            {
                std::lock_guard<std::mutex> lk(mtx);
                s = fast_sock;
            }
            if (!netsock::is_valid(s)) return;
            std::uint8_t buf[4096];
            while (netsock::wait_readable(s, 0)) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                fast_bytes.insert(fast_bytes.end(), buf, buf + n);
            }
        };

        stats = netinput::broadcast_auth_async(listener, producer, q, creds, /*min_initial=*/2,
                                              /*accept_deadline_ms=*/10000, on_frame, kCap, kLiveness);
        netsock::close_socket(listener);
        {
            std::lock_guard<std::mutex> lk(mtx);
            server_done = true;
        }
        cv.notify_all();
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    // DEAD: present token 300 (seat 1), then send/read nothing until the broadcast returns (dropped long
    // before), then drain its kernel-buffered prefix (BIND + a frame prefix) to EOF.
    std::vector<std::uint8_t> dead_bytes;
    std::thread dead([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            const std::vector<std::uint8_t> h = hello_frame(300);  // valid identity -> seat 1
            netsock::send_all(s, h);
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return server_done; });
            }
            std::uint8_t buf[4096];
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                dead_bytes.insert(dead_bytes.end(), buf, buf + n);
            }
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 600 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~60 s
        if (done.load() < 2) {
            std::printf("FAIL layer-22 hygiene leg [%s] TIMED OUT (wedge)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST connects on the main thread, presents token 200 (seat 0), reads its BIND, and upstreams ONLY
    // seat-0 commands.
    bind001::BindInfo fast_bind;
    bool fast_bind_ok = false;
    framing::StreamReassembler fast_rx;
    std::vector<std::vector<std::uint8_t>> fast_all;
    {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            if (netsock::is_valid(s)) {
                const std::vector<std::uint8_t> h = hello_frame(200);  // valid identity -> seat 0
                netsock::send_all(s, h);
                std::uint8_t buf[4096];
                while (fast_all.empty()) {  // read until the BIND record is complete
                    std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                    if (n <= 0) break;
                    if (!fast_rx.feed(buf, static_cast<std::size_t>(n), fast_all)) break;
                }
                if (!fast_all.empty()) {
                    std::size_t pos = 0;
                    fast_bind_ok = bind001::decode_bind(fast_all[0].data(), fast_all[0].size(), pos,
                                                        fast_bind) && pos == fast_all[0].size();
                }
                // send scrambled (reversed) — order-invariance still holds under hygiene
                std::vector<input001::InputCommand> rev(own0.rbegin(), own0.rend());
                const std::vector<std::uint8_t> upstream = encode_upstream(rev);
                for (std::size_t off = 0; off < upstream.size(); off += 32) {
                    std::size_t take = (off + 32 <= upstream.size()) ? 32 : upstream.size() - off;
                    netsock::send_all(s, upstream.data() + off, take);
                }
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    fast_sock = s;
                    commands_sent = true;
                }
                cv.notify_all();
            }
        }
    }

    server.join();
    dead.join();

    // Final drain of FAST (server closed its endpoint => a blocking read runs to EOF), reassemble.
    std::size_t fast_pending = 0;
    if (netsock::is_valid(fast_sock)) {
        std::uint8_t buf[4096];
        while (true) {
            std::ptrdiff_t n = netsock::recv_some(fast_sock, buf, sizeof(buf));
            if (n <= 0) break;
            fast_bytes.insert(fast_bytes.end(), buf, buf + n);
        }
        netsock::close_socket(fast_sock);
        framing::StreamReassembler r;
        r.feed(fast_bytes.data(), fast_bytes.size(), fast_all);
        fast_pending = r.pending();
    }
    // fast_all = [BIND, frame0, frame1, ...]; strip the BIND record to compare against ref.
    std::vector<std::vector<std::uint8_t>> fast_frames;
    for (std::size_t i = 1; i < fast_all.size(); ++i) fast_frames.push_back(fast_all[i]);

    int fails = 0;
    if (!stats.ok || stats.frames_sent != ref.size()) {
        ++fails;
        std::printf("FAIL [%s]: server did not finish the stream (ok=%d sent=%zu/%zu)\n", label,
                    stats.ok ? 1 : 0, stats.frames_sent, ref.size());
    }
    if (!fast_bind_ok || fast_bind.seat != 0 || fast_bind.n_aircraft != 3) {
        ++fails;
        std::printf("FAIL [%s]: FAST BIND (ok=%d seat=%lld n=%lld; want seat 0 by identity)\n", label,
                    fast_bind_ok ? 1 : 0, static_cast<long long>(fast_bind.seat),
                    static_cast<long long>(fast_bind.n_aircraft));
    }
    // FAST's own-seat commands all landed (authorized), none stale/oob/unauth.
    if (stats.cmds_ok != n_own || stats.cmds_stale != 0 || stats.cmds_oob != 0 ||
        stats.cmds_unauth != 0) {
        ++fails;
        std::printf("FAIL [%s]: FAST's commands (ok=%zu stale=%zu oob=%zu unauth=%zu; want %zu/0/0/0)\n",
                    label, stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, stats.cmds_unauth, n_own);
    }
    // Exactly DEAD dropped by the policy under test; the OTHER policy stat stays 0; DEAD's seat freed.
    const std::size_t want_reaped = use_liveness ? 1u : 0u;
    const std::size_t want_capped = use_liveness ? 0u : 1u;
    if (stats.joins != 2 || stats.leaves != 1 || stats.reaped != want_reaped ||
        stats.capped != want_capped) {
        ++fails;
        std::printf("FAIL [%s]: expected exactly DEAD dropped by %s (joins=%zu leaves=%zu reaped=%zu "
                    "capped=%zu; want 2/1/%zu/%zu)\n", label, use_liveness ? "liveness" : "byte-cap",
                    stats.joins, stats.leaves, stats.reaped, stats.capped, want_reaped, want_capped);
    }
    if (fast_pending != 0 || !frames_equal(fast_frames, ref)) {
        ++fails;
        std::printf("FAIL [%s]: FAST's stream (after BIND) not byte-identical to the aircraft-0-only "
                    "reference (got %zu of %zu frames, pending %zu)\n", label, fast_frames.size(),
                    ref.size(), fast_pending);
    }
    // DEAD's delivered bytes: reassemble; the FIRST record is its BIND(seat 1) (delivered first by the
    // async buffer), the rest a strict prefix of the frame stream (dropped before the end).
    {
        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> dead_recs;
        bool ok = r.feed(dead_bytes.data(), dead_bytes.size(), dead_recs);
        bool dead_bind_ok = false;
        if (ok && !dead_recs.empty()) {
            std::size_t pos = 0;
            bind001::BindInfo bd;
            dead_bind_ok = bind001::decode_bind(dead_recs[0].data(), dead_recs[0].size(), pos, bd) &&
                           pos == dead_recs[0].size() && bd.n_aircraft == 3 && bd.seat == 1;
        }
        const std::size_t got_frames = dead_recs.empty() ? 0 : dead_recs.size() - 1;
        bool prefix = dead_bind_ok && got_frames < ref.size();  // strictly short => it was dropped
        for (std::size_t i = 0; prefix && i < got_frames; ++i)
            if (dead_recs[i + 1] != ref[i]) prefix = false;
        if (!prefix) {
            ++fails;
            std::printf("FAIL [%s]: DEAD's delivery is not [BIND(seat 1) | strict frame-prefix] "
                        "(bind_ok=%d frames=%zu/%zu)\n", label, dead_bind_ok ? 1 : 0, got_frames,
                        ref.size());
        }
    }
    if (fails == 0)
        std::printf("  %s PASS: DEAD (token 300 -> seat 1) dropped by %s (reaped=%zu capped=%zu) + its "
                    "seat freed; FAST (token 200 -> seat 0) byte-identical to the aircraft-0-only "
                    "reference + its %zu commands drove the sim; DEAD got [BIND(seat 1) | strict prefix]\n",
                    label, use_liveness ? "liveness" : "byte-cap", stats.reaped, stats.capped, n_own);
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_leg1();
    fails += run_leg2();
    fails += run_hygiene_leg(/*use_liveness=*/true, "leg 3A (liveness reap, cap=0)");
    fails += run_hygiene_leg(/*use_liveness=*/false, "leg 3B (byte-cap shed, liveness=0)");
    if (fails == 0) {
        std::printf("PASS: layer-22 authenticated + async server — the layer-21 IDENTITY binding (HELLO-001 "
                    "+ CredentialTable + BIND-001 + authorization) composes with the layer-16/19 "
                    "async/byte-cap/liveness downstream hygiene: authenticated clients each fly their "
                    "designated aircraft to build_server_frames byte-for-byte through the async path, and a "
                    "dead authenticated client is bounded (reaped/capped) + its seat freed without "
                    "disturbing a cooperative client\n");
        return 0;
    }
    std::printf("RESULT: layer-22 auth-async bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
