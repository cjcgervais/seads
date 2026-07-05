// SEADS CERTIFICATE-PKI + ASYNC determinism BRIDGE (netcode LAYER 31): the layer-30 CA-CERTIFICATE binding
// merged with the layer-16/19 downstream OUTPUT HYGIENE (async send buffers / byte-cap / liveness reap) —
// the 27->28 step re-run on the CERTIFICATE credential (30->31).
//
// broadcast_authcert_async is broadcast_authcert's challenge + HELLO-004 CA-certificate verify driven through
// broadcast_authsig_async's async loop — a SIBLING of both. This bridge proves, over real 127.0.0.1 sockets
// with NO sleeps / NO timing guesses, that the composition preserves BOTH claims:
//
//   LEG 1 (certified identity survives the async path — the merge anchor): THREE clients each hold a distinct
//   CA-issued certificate AND the matching PRIVATE seed (token order != seat order: 100->2, 200->0, 300->1),
//   are CHALLENGED, present the certificate + SIGN, learn their seat from BIND, and upstream ONLY that seat's
//   commands (SCRAMBLED + chunked) while the server runs the FULL async path (cap=0, liveness=0). Their union
//   is the whole INPUT-SK-001 command set, so each downstream — after CHALLENGE + BIND — is BYTE-IDENTICAL to
//   session::build_server_frames. The server enrolled only the ONE CA public key.
//
//   LEG 2 (CA-verify + revocation + rotation + authorization survive the async path): FOUR clients upstream
//   the WHOLE scenario. Client A presents a genuine token-200 certificate + its CORRECT seed -> seat 0
//   (foreign commands dropped); B presents a SELF-SIGNED (non-CA) certificate -> SPECTATOR; C holds a genuine
//   token-300 certificate the server has REVOKED -> SPECTATOR; D holds a genuine token-100 certificate whose
//   epoch is STALE after a key rotation -> SPECTATOR. All four see the aircraft-0-only world byte-for-byte.
//
//   LEG 3 (a dead CERTIFIED client is bounded WITHOUT disturbing the sim): TWO certified clients on a LONG
//   scenario through a pinned tiny kernel send buffer. FAST (token 200 -> seat 0) is drained every frame by
//   the on_frame hook => never dropped, downstream (after CHALLENGE + BIND) byte-identical to the
//   aircraft-0-only reference, its commands drove the sim. DEAD (token 300 -> seat 1) proves its certificate
//   then reads/sends nothing => dropped by the policy under test (liveness reap at cap=0, sub-leg A; byte-cap
//   shed at liveness=0, sub-leg B) + its SEAT (1) freed (leaves==1). DEAD's delivered bytes are
//   [CHALLENGE | BIND(seat 1) | strict frame prefix].
//
// Every socket assertion is on delivered BYTES; all rendezvous are cv+notify_all (no sleeps). A finite
// watchdog fails (not wedges). Exit 0 PASS, 1 FAIL. TRANSPORT-ONLY: broadcast_authcert_async is a SIBLING of
// broadcast_authcert + broadcast_authsig_async (sealed broadcast.cpp + all prior servers untouched);
// CHALLENGE-001/HELLO-004/CERT-001/BIND-001 are transport metadata (no seal). Rides v1.26r0.
#include "authcertasyncserver.h"
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

// The server's session key (seeds the per-connection challenge nonce). Fixed for reproducibility.
static constexpr std::uint64_t SESSION_K0 = 0x1122334455667788ULL;
static constexpr std::uint64_t SESSION_K1 = 0x99AABBCCDDEEFF00ULL;

// ---------------------------------------------------------------------------------------------
// INPUT-SK-001: the SAME grid-exact (dyadic) 3-aircraft scenario the layer-15b..30 bridges use.
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
        for (int i = 0; i < 3; ++i) AA_AC[i] = sess_vec::AIRCRAFT[i];
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

// A deterministic 32-byte private seed from a base byte.
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

// Issue a CA-signed certificate for (token, seat, epoch) over the client's public key (derived from base).
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

// token -> (designated seat, private seed base). Token order != seat order. The server holds only the CA key.
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

// Client-side handshake: read the CHALLENGE-001 (all[0]), sign the challenge with `seed`, and send HELLO-004
// = [cert, challenge_sig] UP. Returns true if the challenge decoded and the HELLO was sent. Uses/extends the
// SAME reassembler `r` so the BIND record lands at all[1].
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
// LEG 1 / LEG 2 harness: N clients, each with a TOKEN, the private SEED it signs with, and the CERTIFICATE
// bytes it presents (self-signed / stale for the rejection cases). The server runs
// broadcast_authcert_async(min_initial = N) through the FULL async path (cap=0, liveness=0) over a CaTable
// configured by `configure` (revocation / rotation applied before any client connects).
// =============================================================================================
struct ClientPlan {
    std::int64_t token;
    std::array<std::uint8_t, 32> seed;
    std::vector<std::uint8_t> cert;
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
};

static void run_leg(const Rails& rails, const session::Scenario& sc,
                    const std::vector<ClientPlan>& plans,
                    const std::function<void(netinput::CaTable&)>& configure, netinput::Stats& stats_out,
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
        stats_out = netinput::broadcast_authcert_async(listener, producer, q, creds, SESSION_K0, SESSION_K1,
                                                       /*min_initial=*/N, /*accept_deadline_ms=*/10000,
                                                       on_frame, /*cap_bytes=*/0, /*liveness_frames=*/0);
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
            ClientResult& res = results_out[ci];

            // 1) read CHALLENGE-001 (all[0]), sign, send HELLO-004 = [cert, challenge_sig].
            res.challenge_ok =
                challenge_and_hello4(s, r, all, plans[ci].token, plans[ci].seed, plans[ci].cert);

            // 2) read until the BIND-001 (all[1]) is complete.
            std::uint8_t buf[4096];
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

            // 3) build this client's commands from its bound seat, scramble, send UP.
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
            // 4) read the downstream snapshots to EOF (frames after CHALLENGE + BIND).
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < expect_done) {
            std::printf("FAIL layer-31 authcert-async leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    for (auto& t : clients) t.join();
}

// ---------------------------------------------------------------------------------------------
// LEG 1: three certified identities each fly the seat their CERTIFICATE designates through the ASYNC path.
// ---------------------------------------------------------------------------------------------
static int run_leg1() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/150u);
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
    run_leg(rails, sc, plans, {}, st, res, "leg1/3-certified-async");

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
        std::printf("  LEG1 PASS: 3 identities each presented a CA CERTIFICATE + SIGNED a fresh challenge and "
                    "flew the seat its CERTIFICATE designates (100->2, 200->0, 300->1; scrambled/chunked) "
                    "through the ASYNC path -> %zu frames byte-identical to build_server_frames; zero "
                    "unauthorized/capped/reaped\n", ref.size());
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: a valid seat-0 client's foreign commands dropped; a SELF-SIGNED (non-CA) cert, a REVOKED token, and
// a STALE-epoch (post-rotation) cert all -> spectator; all four see the aircraft-0-only world, through the
// async path.
// ---------------------------------------------------------------------------------------------
static int run_leg2() {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/150u);
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const std::size_t a_foreign = canon.size() - own0.size();  // client A's non-seat-0 commands
    const std::size_t others = 3 * canon.size();               // B, C, D each drop all
    const auto ref = run_input_frames(rails, sc, own0);        // only aircraft 0 ever commanded

    // A SELF-SIGNED certificate: an attacker mints a token-100/seat-2 certificate with its OWN key (not the
    // CA). It will not verify under the CA public key.
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
        {200, make_seed(ROSTER[1].base), issue(200, 0, 0, ROSTER[1].base), send_everything, 1, true},
        {100, make_seed(ROSTER[0].base), self_signed, send_everything, 5, false},
        {300, make_seed(ROSTER[2].base), issue(300, 1, 0, ROSTER[2].base), send_everything, 3, true},
        {100, make_seed(ROSTER[0].base), issue(100, 2, 0, ROSTER[0].base), send_everything, 4, false},
    };
    auto configure = [](netinput::CaTable& ct) {
        ct.revoke(300);            // client C's identity is compromised -> revoked
        ct.set_min_epoch(100, 5);  // client D's identity rotated -> epoch-0 certs superseded
    };

    netinput::Stats st;
    std::vector<ClientResult> res;
    run_leg(rails, sc, plans, configure, st, res, "leg2/ca-verify-revoke-rotate-async");

    int fails = 0;
    if (!st.ok || st.frames_sent != ref.size() || st.joins != 4) {
        ++fails;
        std::printf("FAIL leg2: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", st.ok ? 1 : 0,
                    st.frames_sent, ref.size(), st.joins);
    }
    if (st.cmds_ok != own0.size() || st.cmds_unauth != (a_foreign + others) || st.cmds_stale != 0 ||
        st.cmds_oob != 0 || st.capped != 0 || st.reaped != 0) {
        ++fails;
        std::printf("FAIL leg2: accounting (ok=%zu unauth=%zu; want %zu own / %zu unauth, 0 hygiene)\n",
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
        std::printf("  LEG2 PASS: a seat-0 client's %zu foreign commands + a SELF-SIGNED cert + a REVOKED "
                    "token + a STALE-epoch (rotated) cert (%zu commands) all rejected (cmds_unauth=%zu) "
                    "through the async path; all see the aircraft-0-only world byte-for-byte\n",
                    a_foreign, others, st.cmds_unauth);
    return fails;
}

// =============================================================================================
// LEG 3: a dead CERTIFIED client is bounded (reaped/capped) + its SEAT freed without disturbing a cooperative
// seated client. FAST (token 200 -> seat 0, hook-drained). DEAD (token 300 -> seat 1) proves its certificate
// then reads/sends nothing. `use_liveness` selects the policy.
// =============================================================================================
static int run_hygiene_leg(bool use_liveness, const char* label) {
    const Rails rails = sealed_rails();
    const session::Scenario sc = aa_scenario(/*ticks=*/20000u);  // ~4000 frames, ~1 MB downstream
    const auto canon = netinput::commands_from_scenario(sc);
    const auto own0 = filter_by_aircraft(canon, 0);
    const auto ref = run_input_frames(rails, sc, own0);          // FAST is seat 0: aircraft-0-only world
    const std::size_t n_own = own0.size();

    const int kBufBytes = 16 * 1024;
    const std::size_t kLiveness = use_liveness ? 16 : 0;
    const std::size_t kCap = use_liveness ? 0 : 128 * 1024;

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool server_done = false;
    bool commands_sent = false;
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

        stats = netinput::broadcast_authcert_async(listener, producer, q, creds, SESSION_K0, SESSION_K1,
                                                   /*min_initial=*/2, /*accept_deadline_ms=*/10000, on_frame,
                                                   kCap, kLiveness);
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

    // DEAD: token 300 (seat 1). Prove the certificate, then read nothing until the broadcast returns, then
    // drain its kernel-buffered prefix to EOF through ONE reassembler so dead_recs = [CHALLENGE, BIND,
    // frame prefix...].
    std::vector<std::vector<std::uint8_t>> dead_recs;
    std::thread dead([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
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
            dead_recs = all;
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 600 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (done.load() < 2) {
            std::printf("FAIL layer-31 hygiene leg [%s] TIMED OUT (wedge)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    // FAST: token 200 (seat 0). Prove the certificate, read BIND, upstream ONLY seat-0 commands.
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
                while (fast_all.size() < 2) {  // read until the BIND record (all[1]) is complete
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
    // fast_all = [CHALLENGE, BIND, frame0, ...]; strip CHALLENGE + BIND to compare against ref.
    std::vector<std::vector<std::uint8_t>> fast_frames;
    for (std::size_t i = 2; i < fast_all.size(); ++i) fast_frames.push_back(fast_all[i]);

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
    if (stats.cmds_ok != n_own || stats.cmds_stale != 0 || stats.cmds_oob != 0 ||
        stats.cmds_unauth != 0) {
        ++fails;
        std::printf("FAIL [%s]: FAST's commands (ok=%zu stale=%zu oob=%zu unauth=%zu; want %zu/0/0/0)\n",
                    label, stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, stats.cmds_unauth, n_own);
    }
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
        std::printf("FAIL [%s]: FAST's stream (after CHALLENGE+BIND) not byte-identical to the "
                    "aircraft-0-only reference (got %zu of %zu frames, pending %zu)\n", label,
                    fast_frames.size(), ref.size(), fast_pending);
    }
    // DEAD's delivery: [CHALLENGE | BIND(seat 1) | strict frame-prefix].
    {
        bool dead_bind_ok = false;
        if (dead_recs.size() >= 2) {
            std::size_t pos = 0;
            bind001::BindInfo bd;
            dead_bind_ok = bind001::decode_bind(dead_recs[1].data(), dead_recs[1].size(), pos, bd) &&
                           pos == dead_recs[1].size() && bd.n_aircraft == 3 && bd.seat == 1;
        }
        const std::size_t got_frames = dead_recs.size() >= 2 ? dead_recs.size() - 2 : 0;
        bool prefix = dead_bind_ok && got_frames < ref.size();  // strictly short => dropped
        for (std::size_t i = 0; prefix && i < got_frames; ++i)
            if (dead_recs[i + 2] != ref[i]) prefix = false;
        if (!prefix) {
            ++fails;
            std::printf("FAIL [%s]: DEAD's delivery is not [CHALLENGE | BIND(seat 1) | strict frame-prefix] "
                        "(bind_ok=%d frames=%zu/%zu)\n", label, dead_bind_ok ? 1 : 0, got_frames,
                        ref.size());
        }
    }
    if (fails == 0)
        std::printf("  %s PASS: DEAD (token 300 -> seat 1) dropped by %s (reaped=%zu capped=%zu) + its seat "
                    "freed; FAST (token 200 -> seat 0) byte-identical to the aircraft-0-only reference + its "
                    "%zu commands drove the sim; DEAD got [CHALLENGE | BIND(seat 1) | strict prefix]\n",
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
        std::printf("PASS: layer-31 certificate-PKI + async server — the layer-30 CA-CERTIFICATE binding "
                    "(CHALLENGE-001 + verified HELLO-004 certificate + possession proof + BIND-001 + "
                    "authorization, with revocation + rotation) composes with the layer-16/19 "
                    "async/byte-cap/liveness downstream hygiene: certified clients each fly their designated "
                    "aircraft to build_server_frames byte-for-byte through the async path, a self-signed / "
                    "revoked / stale-epoch credential gets no seat, and a dead certified client is bounded + "
                    "its seat freed without disturbing a cooperative client\n");
        return 0;
    }
    std::printf("RESULT: layer-31 authcert-async bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
