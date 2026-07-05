// SEADS CERTIFICATE-PKI + ASYNC + CATCH-UP determinism BRIDGE (netcode LAYER 32): the layer-30 CA-CERTIFICATE
// binding + the layer-16/19 async hygiene + broadcast_live's windowed late-join CATCH-UP, all composed — the
// LAST rung of the certificate-PKI arc (30 -> 31 -> 32, the 28->29 step re-run on the CERTIFICATE credential).
// broadcast_authcert_catchup is a SIBLING of both broadcast_authcert_async (layer 31) and
// broadcast_authsig_catchup (layer 29). This bridge proves, over real 127.0.0.1 sockets with NO sleeps / NO
// timing guesses, that the composition preserves ALL THREE claims:
//
//   LEG 1 (certified identity + catch-up window regimes — the headline): THREE identities present distinct
//   CA certificates + correct seeds from the initial gather (token order != seat order: 100->2, 200->0,
//   300->1), are CHALLENGED, present the certificate + SIGN, learn their seat from BIND, and upstream ONLY
//   that seat's commands (SCRAMBLED + chunked) — so the produced stream is BYTE-IDENTICAL to
//   session::build_server_frames. A FOURTH client joins mid-stream at frame kJoin presenting a SELF-SIGNED
//   (non-CA) certificate (-> SPECTATOR) and — across W ∈ {1, kJoin/2, retain-all} — receives EXACTLY
//   [CHALLENGE | BIND(spectator) | frames[max(0,kJoin-W):]], trimmed == max(0, N-W). The three seated
//   identities are byte-identical to the whole stream under EVERY window.
//
//   LEG 2 (verify + authorization compose with catch-up): the mid-stream SPECTATOR upstreams the WHOLE command
//   set; a spectator authorizes NOTHING, so every one is dropped (cmds_unauth == the whole set) and the
//   produced stream is unchanged — yet the spectator STILL catches up the whole stream byte-for-byte.
//
//   LEG 3 (hygiene composes with certificate + catch-up): on a LONG stream through a pinned tiny kernel send
//   buffer, a seated FAST identity (token 200 -> seat 0, hook-drained) is byte-identical to its seat's
//   reference, while a NON-READING certified DEAD identity (token 300 -> seat 1) joining mid-stream has its
//   catch-up REPLAY backlog shed by the byte-cap (Stats.capped) + its SEAT (1) freed. DEAD's delivery is
//   [CHALLENGE | BIND(seat 1) | strict byte-prefix]; FAST untouched.
//
// Every socket assertion is on delivered BYTES; all rendezvous are cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges). Exit 0 PASS, 1 FAIL. TRANSPORT-ONLY: broadcast_authcert_catchup is a SIBLING
// of broadcast_authcert_async + broadcast_authsig_catchup (sealed broadcast.cpp + all prior servers
// untouched); CHALLENGE-001/HELLO-004/CERT-001/BIND-001 are transport metadata (no seal). Rides v1.26r0.
#include "authcertcatchupserver.h"
#include "authcertserver.h"
#include "cert001.h"
#include "authsig001.h"
#include "authmac001.h"
#include "ed25519.h"
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

static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact (dyadic) 3-aircraft scenario the layer-15b..31 bridges use.
// ---------------------------------------------------------------------------------------------
static const session::Phase AC_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},
    {50u,  0.5,  1.5, 1.0,  false},
    {120u, 0.0,  1.0, 0.75, false},
};
static const session::Phase AC_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},
    {80u,  0.25, 2.0, 1.0,  true},
};
static const session::Phase AC_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},
};

static session::AircraftSpec AC_AC[3];
static const session::Scenario ac_scenario(unsigned ticks) {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) AC_AC[i] = sess_vec::AIRCRAFT[i];
        AC_AC[0].sched = AC_P0; AC_AC[0].n_phase = 3;
        AC_AC[1].sched = AC_P1; AC_AC[1].n_phase = 2;
        AC_AC[2].sched = AC_P2; AC_AC[2].n_phase = 1;
        return true;
    }();
    (void)built;
    session::Scenario sc = {AC_AC, 3u, ticks, /*snap_every=*/5u, /*lag=*/0u, /*render=*/0u,
                            /*drops=*/nullptr, /*n_drops=*/0u};
    return sc;
}

static std::array<std::uint8_t, 32> make_seed(std::uint8_t base) {
    std::array<std::uint8_t, 32> s;
    for (int i = 0; i < 32; ++i) s[i] = static_cast<std::uint8_t>((base + i) & 0xFF);
    return s;
}

static const std::uint8_t CA_BASE = 0xC0;
static std::array<std::uint8_t, 32> ca_public() {
    auto seed = make_seed(CA_BASE);
    std::array<std::uint8_t, 32> pk{};
    ed25519::public_key(seed.data(), pk.data());
    return pk;
}

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

// A SELF-SIGNED certificate (an attacker key vouches for itself, not the CA) — CaTable rejects it -> spectator.
static std::vector<std::uint8_t> self_signed_cert(std::int64_t token, std::int64_t seat,
                                                  std::uint8_t client_base) {
    auto attacker = make_seed(0x99);
    auto cseed = make_seed(client_base);
    std::uint8_t cpub[32];
    ed25519::public_key(cseed.data(), cpub);
    std::vector<std::uint8_t> cert;
    cert001::issue_cert(attacker.data(), token, seat, 0, cpub, cert);
    return cert;
}

struct Identity { std::int64_t token; std::int64_t seat; std::uint8_t base; };
static const Identity ROSTER[] = {{100, 2, 0x10}, {200, 0, 0x20}, {300, 1, 0x30}};

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

// Read the CHALLENGE-001 (all[0]), sign it with `seed`, send HELLO-004 = [cert, challenge_sig] UP.
static bool challenge_and_hello4(netsock::socket_t s, framing::StreamReassembler& r,
                                 std::vector<std::vector<std::uint8_t>>& all, std::int64_t token,
                                 const std::array<std::uint8_t, 32>& seed,
                                 const std::vector<std::uint8_t>& cert) {
    std::uint8_t buf[4096];
    while (all.empty()) {
        std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
        if (n <= 0) break;
        if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
    }
    if (all.empty()) return false;
    std::size_t pos = 0;
    authmac001::ChallengeInfo ci;
    if (!authmac001::decode_challenge(all[0].data(), all[0].size(), pos, ci) || pos != all[0].size())
        return false;
    std::uint8_t sig[64];
    authsig001::sign_challenge(seed.data(), ci.nonce, token, sig);
    cert001::Hello4Info hi;
    hi.cert = cert;
    hi.sig.assign(sig, sig + 64);
    std::vector<std::uint8_t> rec, framed;
    cert001::encode_hello4(hi, rec);
    framing::encode_frame(rec, framed);
    netsock::send_all(s, framed);
    return true;
}

// =============================================================================================
// LEG 1 / LEG 2 harness: THREE seated identities (initial gather) + a mid-stream self-signed-cert spectator.
// Server runs broadcast_authcert_catchup(min_initial=3, cap=0, liveness=0, `window`). Clients read CHALLENGE
// first, sign, send HELLO-004, then read BIND (all[1]); frames = all[2:].
// =============================================================================================
struct CatchupResult {
    bind001::BindInfo early_bind[3];
    bool early_bind_ok[3] = {false, false, false};
    std::vector<std::vector<std::uint8_t>> early_frames[3];
    std::size_t early_pending[3] = {1, 1, 1};
    bind001::BindInfo late_bind;
    bool late_bind_ok = false;
    std::vector<std::vector<std::uint8_t>> late_frames;
    std::size_t late_pending = 1;
};

static void run_catchup_leg(const Rails& rails, const session::Scenario& sc, std::size_t kJoin,
                            std::size_t window, bool spectator_sends_all,
                            const std::vector<input001::InputCommand>& canon, netinput::Stats& stats_out,
                            CatchupResult& res, const char* label) {
    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    std::size_t early_cmds_sent = 0;
    bool late_ready = false;
    bool late_connected = false;

    std::atomic<int> done{0};
    const int expect_done = 5;  // 3 early + 1 late + server

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
        auto on_frame = [&](std::size_t fi) {
            if (fi == 0) {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return early_cmds_sent == 3; });
            }
            if (fi == kJoin) {
                std::unique_lock<std::mutex> lk(mtx);
                late_ready = true;
                cv.notify_all();
                cv.wait(lk, [&] { return late_connected; });
            }
        };
        stats_out = netinput::broadcast_authcert_catchup(listener, producer, q, creds, SESSION_K0, SESSION_K1,
                                                         /*min_initial=*/3, /*accept_deadline_ms=*/10000,
                                                         on_frame, /*cap_bytes=*/0, /*liveness_frames=*/0,
                                                         window);
        netsock::close_socket(listener);
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    std::vector<std::thread> early;
    for (std::size_t ci = 0; ci < 3; ++ci) {
        early.emplace_back([&, ci] {
            std::uint16_t port = wait_port();
            if (port == 0) { ++done; return; }
            netsock::socket_t s = netsock::connect_loopback(port);
            if (!netsock::is_valid(s)) { ++done; return; }
            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> all;
            challenge_and_hello4(s, r, all, ROSTER[ci].token, make_seed(ROSTER[ci].base),
                                 issue(ROSTER[ci].token, ROSTER[ci].seat, 0, ROSTER[ci].base));
            std::uint8_t buf[4096];
            while (all.size() < 2) {  // read until the BIND record (all[1]) is complete
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            if (all.size() >= 2) {
                std::size_t pos = 0;
                res.early_bind_ok[ci] =
                    bind001::decode_bind(all[1].data(), all[1].size(), pos, res.early_bind[ci]) &&
                    pos == all[1].size();
            }
            std::vector<input001::InputCommand> cmds =
                res.early_bind_ok[ci] ? filter_by_aircraft(canon, res.early_bind[ci].seat)
                                      : std::vector<input001::InputCommand>{};
            const bool reverse = (ci % 2 == 0);
            if (reverse) std::reverse(cmds.begin(), cmds.end());
            std::vector<std::uint8_t> up = encode_upstream(cmds);
            const std::size_t chunk = ci == 0 ? 1 : (ci == 1 ? 7 : 3);
            for (std::size_t off = 0; off < up.size(); off += chunk) {
                std::size_t take = (off + chunk <= up.size()) ? chunk : up.size() - off;
                netsock::send_all(s, up.data() + off, take);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                ++early_cmds_sent;
            }
            cv.notify_all();
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            res.early_pending[ci] = r.pending();
            for (std::size_t i = 2; i < all.size(); ++i) res.early_frames[ci].push_back(all[i]);
            netsock::close_socket(s);
            ++done;
        });
    }

    std::thread late([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return late_ready; });
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            late_connected = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(s)) { ++done; return; }
        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> all;
        // a SELF-SIGNED (non-CA) certificate for token 999 -> spectator.
        challenge_and_hello4(s, r, all, 999, make_seed(0xEE), self_signed_cert(999, 0, 0xEE));
        std::uint8_t buf[4096];
        while (all.size() < 2) {  // read until the BIND record (all[1]) is complete
            std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
            if (n <= 0) break;
            if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
        }
        if (all.size() >= 2) {
            std::size_t pos = 0;
            res.late_bind_ok =
                bind001::decode_bind(all[1].data(), all[1].size(), pos, res.late_bind) &&
                pos == all[1].size();
        }
        if (spectator_sends_all) {
            std::vector<std::uint8_t> up = encode_upstream(canon);
            for (std::size_t off = 0; off < up.size(); off += 5) {
                std::size_t take = (off + 5 <= up.size()) ? 5 : up.size() - off;
                netsock::send_all(s, up.data() + off, take);
            }
        }
        while (true) {
            std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
            if (n <= 0) break;
            if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
        }
        res.late_pending = r.pending();
        for (std::size_t i = 2; i < all.size(); ++i) res.late_frames.push_back(all[i]);
        netsock::close_socket(s);
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < expect_done; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < expect_done) {
            std::printf("FAIL layer-32 authcert-catchup leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : early) t.join();
    late.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: verified certificate identity + catch-up window regimes.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;

    struct Regime { const char* name; std::size_t window; };
    const Regime regimes[] = {
        {"W=1 (minimal)", 1},
        {"W=kJoin/2 (partial)", kJoin / 2},
        {"W=0 (retain-all)", 0},
    };

    int fails = 0;
    for (const Regime& reg : regimes) {
        const std::size_t W = reg.window;
        const std::size_t start = (W > 0) ? (kJoin > W ? kJoin - W : 0) : 0;
        const std::size_t want_trimmed = (W > 0 && N > W) ? N - W : 0;

        netinput::Stats st;
        CatchupResult res;
        run_catchup_leg(rails, sc, kJoin, W, /*spectator_sends_all=*/false, canon, st, res, reg.name);

        int lf = 0;
        if (!st.ok || st.frames_sent != N || st.joins != 4 || st.leaves != 0 || st.capped != 0 ||
            st.reaped != 0 || st.trimmed != want_trimmed) {
            ++lf;
            std::printf("FAIL leg1 [%s]: server stats (ok=%d sent=%zu/%zu joins=%zu leaves=%zu capped=%zu "
                        "reaped=%zu trimmed=%zu want_trimmed=%zu)\n", reg.name, st.ok ? 1 : 0,
                        st.frames_sent, N, st.joins, st.leaves, st.capped, st.reaped, st.trimmed,
                        want_trimmed);
        }
        if (st.cmds_ok != canon.size() || st.cmds_unauth != 0 || st.cmds_stale != 0 || st.cmds_oob != 0) {
            ++lf;
            std::printf("FAIL leg1 [%s]: commands (ok=%zu unauth=%zu stale=%zu oob=%zu; want %zu/0/0/0)\n",
                        reg.name, st.cmds_ok, st.cmds_unauth, st.cmds_stale, st.cmds_oob, canon.size());
        }
        for (std::size_t ci = 0; ci < 3; ++ci) {
            const auto& b = res.early_bind[ci];
            const std::int64_t want_seat = ROSTER[ci].seat;
            if (!res.early_bind_ok[ci] || b.n_aircraft != 3 || b.seat != want_seat) {
                ++lf;
                std::printf("FAIL leg1 [%s]: identity %zu (token %lld) bound to seat %lld, want %lld\n",
                            reg.name, ci, static_cast<long long>(ROSTER[ci].token),
                            static_cast<long long>(b.seat), static_cast<long long>(want_seat));
            }
            if (res.early_pending[ci] != 0 || !frames_equal(res.early_frames[ci], ref)) {
                ++lf;
                std::printf("FAIL leg1 [%s]: seated identity %zu not byte-identical to build_server_frames "
                            "(got %zu of %zu, pending %zu)\n", reg.name, ci, res.early_frames[ci].size(),
                            N, res.early_pending[ci]);
            }
        }
        if (!res.late_bind_ok || res.late_bind.seat != bind001::SPECTATOR ||
            res.late_bind.n_aircraft != 3) {
            ++lf;
            std::printf("FAIL leg1 [%s]: spectator BIND (ok=%d seat=%lld n=%lld; want seat -1 / n 3)\n",
                        reg.name, res.late_bind_ok ? 1 : 0, static_cast<long long>(res.late_bind.seat),
                        static_cast<long long>(res.late_bind.n_aircraft));
        }
        bool suffix_ok = res.late_pending == 0 && res.late_frames.size() == N - start;
        for (std::size_t i = 0; suffix_ok && i < res.late_frames.size(); ++i)
            suffix_ok = res.late_frames[i] == ref[start + i];
        if (!suffix_ok) {
            ++lf;
            std::printf("FAIL leg1 [%s]: spectator did not receive exactly frames[%zu:] (got %zu of %zu, "
                        "pending %zu)\n", reg.name, start, res.late_frames.size(), N - start,
                        res.late_pending);
        }
        if (lf == 0)
            std::printf("  LEG1 [%s] PASS: 3 identities each flew the seat their CERTIFICATE designates "
                        "(100->2, 200->0, 300->1) byte-identical to build_server_frames; a self-signed-cert "
                        "spectator caught up EXACTLY frames[%zu:] (%zu frames), trimmed=%zu\n", reg.name,
                        start, N - start, want_trimmed);
        fails += lf;
    }
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: authentication + authorization compose with catch-up (spectator's whole set rejected, still catches up).
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/150u);
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;

    netinput::Stats st;
    CatchupResult res;
    run_catchup_leg(rails, sc, kJoin, /*window=*/0, /*spectator_sends_all=*/true, canon, st, res,
                    "leg2/spectator-foreign-reject");

    int fails = 0;
    if (!st.ok || st.frames_sent != N || st.joins != 4 || st.leaves != 0 || st.capped != 0 ||
        st.reaped != 0 || st.trimmed != 0) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu leaves=%zu capped=%zu "
                    "reaped=%zu trimmed=%zu)\n", st.ok ? 1 : 0, st.frames_sent, N, st.joins, st.leaves,
                    st.capped, st.reaped, st.trimmed);
    }
    if (st.cmds_ok != canon.size() || st.cmds_unauth != canon.size() || st.cmds_stale != 0 ||
        st.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL leg2: commands (ok=%zu unauth=%zu; want %zu authorized / %zu spectator-rejected)\n",
                    st.cmds_ok, st.cmds_unauth, canon.size(), canon.size());
    }
    for (std::size_t ci = 0; ci < 3; ++ci) {
        if (res.early_pending[ci] != 0 || !frames_equal(res.early_frames[ci], ref)) {
            ++fails;
            std::printf("FAIL leg2: seated identity %zu stream changed by spectator's rejected commands "
                        "(got %zu of %zu)\n", ci, res.early_frames[ci].size(), N);
        }
    }
    if (!res.late_bind_ok || res.late_bind.seat != bind001::SPECTATOR) {
        ++fails;
        std::printf("FAIL leg2: spectator BIND (ok=%d seat=%lld; want -1)\n", res.late_bind_ok ? 1 : 0,
                    static_cast<long long>(res.late_bind.seat));
    }
    if (res.late_pending != 0 || !frames_equal(res.late_frames, ref)) {
        ++fails;
        std::printf("FAIL leg2: spectator did not catch up the whole stream (got %zu of %zu, pending %zu)\n",
                    res.late_frames.size(), N, res.late_pending);
    }
    if (fails == 0)
        std::printf("  LEG2 PASS: the self-signed-cert spectator's %zu commands were ALL rejected "
                    "(cmds_unauth=%zu); the produced stream was unchanged and the spectator still caught up "
                    "the whole %zu-frame stream byte-for-byte\n", canon.size(), st.cmds_unauth, N);
    return fails;
}

// =============================================================================================
// LEG 3: hygiene composes with certificate + catch-up. FAST (token 200 -> seat 0, hook-drained) byte-identical
// to its reference; DEAD (token 300 -> seat 1) joins mid-stream, its catch-up backlog shed by the byte-cap +
// its seat freed. DEAD's delivery: [CHALLENGE | BIND(seat 1) | strict prefix].
// =============================================================================================
static int run_leg3() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = ac_scenario(/*ticks=*/20000u);
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const auto ref = run_input_frames(rails, sc, own0);          // FAST is seat 0: aircraft-0-only world
    const std::size_t N = ref.size();
    const std::size_t kJoin = N / 2;
    const std::size_t n_own = own0.size();

    const int kBufBytes = 16 * 1024;
    const std::size_t kCap = 128 * 1024;

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    bool fast_cmds_sent = false;
    bool dead_ready = false;
    bool dead_connected = false;
    netsock::socket_t fast_sock = netsock::invalid_socket();
    std::vector<std::uint8_t> fast_bytes;

    netinput::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/3);
        if (netsock::is_valid(listener)) {
            netsock::set_sndbuf(listener, kBufBytes);
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
        auto ca_pk = ca_public();
        netinput::CaTable creds(producer.n_aircraft(), ca_pk.data());
        auto on_frame = [&](std::size_t fi) {
            if (fi == 0) {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return fast_cmds_sent; });
            }
            if (fi == kJoin) {
                std::unique_lock<std::mutex> lk(mtx);
                dead_ready = true;
                cv.notify_all();
                cv.wait(lk, [&] { return dead_connected; });
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

        stats = netinput::broadcast_authcert_catchup(listener, producer, q, creds, SESSION_K0, SESSION_K1,
                                                    /*min_initial=*/1, /*accept_deadline_ms=*/10000, on_frame,
                                                    kCap, /*liveness_frames=*/0, /*catchup_window=*/0);
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

    std::vector<std::vector<std::uint8_t>> dead_recs;
    std::thread dead([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return dead_ready; });
        }
        netsock::socket_t s = netsock::connect_loopback(port);
        {
            std::lock_guard<std::mutex> lk(mtx);
            dead_connected = true;
        }
        cv.notify_all();
        if (netsock::is_valid(s)) {
            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> all;
            challenge_and_hello4(s, r, all, 300, make_seed(ROSTER[2].base),
                                 issue(300, 1, 0, ROSTER[2].base));  // valid identity -> seat 1
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return server_done; });
            }
            std::uint8_t buf[4096];
            while (true) {
                std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                if (n <= 0) break;
                if (!r.feed(buf, static_cast<std::size_t>(n), all)) break;
            }
            netsock::close_socket(s);
            dead_recs = all;  // [CHALLENGE, BIND, frame prefix...] through ONE reassembler
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 600 && done.load() < 3; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < 3) {
            std::printf("FAIL layer-32 hygiene leg TIMED OUT (wedge)\n");
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST: token 200 (seat 0). Prove the certificate, read BIND, upstream ONLY its own seat's commands.
    bind001::BindInfo fast_bind;
    bool fast_bind_ok = false;
    framing::StreamReassembler fast_rx;
    std::vector<std::vector<std::uint8_t>> fast_all;
    {
        std::uint16_t port = wait_port();
        if (port != 0) {
            netsock::socket_t s = netsock::connect_loopback(port);
            if (netsock::is_valid(s)) {
                challenge_and_hello4(s, fast_rx, fast_all, 200, make_seed(ROSTER[1].base),
                                     issue(200, 0, 0, ROSTER[1].base));
                std::uint8_t buf[4096];
                while (fast_all.size() < 2) {
                    std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
                    if (n <= 0) break;
                    if (!fast_rx.feed(buf, static_cast<std::size_t>(n), fast_all)) break;
                }
                if (fast_all.size() >= 2) {
                    std::size_t pos = 0;
                    fast_bind_ok = bind001::decode_bind(fast_all[1].data(), fast_all[1].size(), pos,
                                                        fast_bind) && pos == fast_all[1].size();
                }
                std::vector<input001::InputCommand> rev(own0.rbegin(), own0.rend());
                const std::vector<std::uint8_t> up = encode_upstream(rev);
                for (std::size_t off = 0; off < up.size(); off += 32) {
                    std::size_t take = (off + 32 <= up.size()) ? 32 : up.size() - off;
                    netsock::send_all(s, up.data() + off, take);
                }
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    fast_sock = s;
                    fast_cmds_sent = true;
                }
                cv.notify_all();
            }
        }
    }

    server.join();
    dead.join();

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
    std::vector<std::vector<std::uint8_t>> fast_frames;
    for (std::size_t i = 2; i < fast_all.size(); ++i) fast_frames.push_back(fast_all[i]);

    int fails = 0;
    if (!stats.ok || stats.frames_sent != N) {
        ++fails;
        std::printf("FAIL leg3: server did not finish the stream (ok=%d sent=%zu/%zu)\n", stats.ok ? 1 : 0,
                    stats.frames_sent, N);
    }
    if (!fast_bind_ok || fast_bind.seat != 0 || fast_bind.n_aircraft != 3) {
        ++fails;
        std::printf("FAIL leg3: FAST BIND (ok=%d seat=%lld n=%lld; want seat 0 by identity)\n",
                    fast_bind_ok ? 1 : 0, static_cast<long long>(fast_bind.seat),
                    static_cast<long long>(fast_bind.n_aircraft));
    }
    if (stats.cmds_ok != n_own || stats.cmds_stale != 0 || stats.cmds_oob != 0 ||
        stats.cmds_unauth != 0) {
        ++fails;
        std::printf("FAIL leg3: FAST's commands (ok=%zu stale=%zu oob=%zu unauth=%zu; want %zu/0/0/0)\n",
                    stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, stats.cmds_unauth, n_own);
    }
    if (stats.capped != 1 || stats.reaped != 0 || stats.leaves != stats.joins - 1 ||
        stats.joins < 1 || stats.joins > 2) {
        ++fails;
        std::printf("FAIL leg3: expected DEAD shed by the byte-cap (joins=%zu leaves=%zu capped=%zu "
                    "reaped=%zu; want capped 1 / reaped 0 / leaves==joins-1 / joins in {1,2})\n",
                    stats.joins, stats.leaves, stats.capped, stats.reaped);
    }
    if (fast_pending != 0 || !frames_equal(fast_frames, ref)) {
        ++fails;
        std::printf("FAIL leg3: FAST's stream (after CHALLENGE+BIND) not byte-identical to the "
                    "aircraft-0-only reference (got %zu of %zu, pending %zu)\n", fast_frames.size(),
                    ref.size(), fast_pending);
    }
    {
        bool dead_bind_ok = false;
        if (dead_recs.size() >= 2) {
            std::size_t pos = 0;
            bind001::BindInfo bd;
            dead_bind_ok = bind001::decode_bind(dead_recs[1].data(), dead_recs[1].size(), pos, bd) &&
                           pos == dead_recs[1].size() && bd.n_aircraft == 3 && bd.seat == 1;
        }
        const std::size_t got_frames = dead_recs.size() >= 2 ? dead_recs.size() - 2 : 0;
        bool prefix = dead_bind_ok && got_frames < ref.size();  // strictly short => shed
        for (std::size_t i = 0; prefix && i < got_frames; ++i)
            if (dead_recs[i + 2] != ref[i]) prefix = false;
        if (!prefix) {
            ++fails;
            std::printf("FAIL leg3: DEAD's delivery is not [CHALLENGE | BIND(seat 1) | strict prefix] "
                        "(bind_ok=%d frames=%zu/%zu)\n", dead_bind_ok ? 1 : 0, got_frames, ref.size());
        }
    }
    if (fails == 0)
        std::printf("  LEG3 PASS: DEAD (token 300 -> seat 1) shed by the byte-cap (capped=1) mid catch-up + "
                    "its seat freed; FAST (token 200 -> seat 0) byte-identical to the aircraft-0-only "
                    "reference + its %zu commands drove the sim; DEAD got [CHALLENGE | BIND(seat 1) | "
                    "strict prefix]\n", n_own);
    return fails;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_leg1();
    fails += run_leg2();
    fails += run_leg3();
    if (fails == 0) {
        std::printf("PASS: layer-32 certificate-PKI + async + catch-up server — the layer-30 CA-CERTIFICATE "
                    "binding + the layer-16/19 async/byte-cap/liveness hygiene + layer-20 windowed late-join "
                    "CATCH-UP all compose: certified clients each fly their designated aircraft to "
                    "build_server_frames byte-for-byte, a mid-stream joiner catches up an exact window "
                    "suffix, a self-signed-cert spectator's commands are all rejected yet it still catches "
                    "up, and a dead certified client's replay backlog is shed by the byte-cap + its seat "
                    "freed\n");
        return 0;
    }
    std::printf("RESULT: layer-32 authcert-catchup bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
