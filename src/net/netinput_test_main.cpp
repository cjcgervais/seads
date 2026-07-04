// SEADS INPUT-UPSTREAM determinism BRIDGE for the authoritative input server (netcode LAYER 15b —
// the FIRST BIDIRECTIONAL layer).
//
// Every layer 5–15a is server->client. Layer 15b lets a client send tick-stamped INPUT-001 Commands
// UP into the authoritative sealed kernel. Because a network reorders and re-chunks bytes, the layer
// "brushes the determinism rail" — so its correctness rests on the ORDERING CONTRACT (cmdqueue.h),
// not the transport. This bridge proves, over real 127.0.0.1 sockets with NO sleeps / NO timing
// guesses, two claims:
//
//   LEG 1 (UPSTREAM ORDER + CHUNK INVARIANCE — the socket leg): a client sends a whole scenario's
//   worth of commands UP in a SCRAMBLED order and adversarial byte CHUNKING (down to 1 byte/send, so
//   the framing reassembler is maximally stressed), while the server steps the kernel from them and
//   streams the resulting snapshots back DOWN. The commands use GRID-EXACT (dyadic) values so the
//   lossy wire round-trips them bit-for-bit; therefore the input-driven server's frames are
//   BYTE-IDENTICAL to session::build_server_frames of the same scenario — the phase-schedule path.
//   Two sub-legs with DIFFERENT scrambles/chunkings both reproduce that byte stream: the kernel's
//   output is a pure function of the command SET, blind to arrival order. (This is the input-direction
//   analogue of the downstream layers' "lossy != nondeterministic".)
//
//   LEG 2 (the DROP + HOLD-LAST contract — in-process): the CommandQueue is exercised directly:
//   feeding the same commands REVERSED (plus injected STALE and OUT-OF-RANGE commands that must be
//   rejected, and lower-seq duplicates that must lose) reproduces the canonical frames exactly; a
//   command whose apply_tick has been stepped past is dropped (STALE); an out-of-range aircraft index
//   is dropped (OUT_OF_RANGE); and the (apply_tick, aircraft) winner is the max-seq command
//   regardless of submit order. Hold-last is implicit in reproducing build_server_frames (a tick with
//   no command reuses the aircraft's last one — exactly session::phase_at semantics).
//
// Every assertion is on delivered BYTES; the rendezvous is cv+notify_all (no sleeps; the server's
// on_frame(0) hook blocks until the client has sent all its commands, so they are ingested before
// their apply_tick). A finite watchdog fails (not wedges) on any socket hang. Exit 0 PASS, 1 FAIL.
//
// TRANSPORT: no kernel/det_math/golden touched; the INPUT-001 wire is a new sealed rail block (a seal
// only because the wire is a sealed rail, like WEAPON-001 v1.12r0). Rides seal v1.26r0.
#include "inputserver.h"
#include "input001.h"
#include "cmdqueue.h"
#include "session.h"
#include "session_vectors.h"
#include "framing.h"
#include "socket.h"
#include "snapshot.h"
#include "golden_params.h"
#include "kernel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// INPUT-SK-001: a GRID-EXACT scenario. Reuses the three SESSION-SK-001 envelopes + start states, but
// its command values are all DYADIC (0.0, +-0.5, 0.25, 1.0, 1.5, 2.0, 0.75) so quantize->dequantize
// at 1e6 is the identity — the input path can then be BYTE-IDENTICAL to the phase path. The schedules
// exercise hold-last (gaps between phases), bank/g/throttle changes, and firing.
// ---------------------------------------------------------------------------------------------
static const session::Phase IN_P0[] = {
    {0u,   0.0,  1.0, 0.75, true},   // P-47D: fire, wings level, cruise
    {50u,  0.5,  1.5, 1.0,  false},  // bank 0.5 rad, 1.5 g, full throttle
    {120u, 0.0,  1.0, 0.75, false},  // roll out
};
static const session::Phase IN_P1[] = {
    {0u,  -0.5,  1.0, 0.5,  false},  // A6M2: opposite bank
    {80u,  0.25, 2.0, 1.0,  true},   // tighten + fire
};
static const session::Phase IN_P2[] = {
    {0u,   0.5,  1.5, 1.0,  false},  // Spitfire: steady turn
};

static session::AircraftSpec IN_AC[3];
static const session::Scenario& input_scenario() {
    static bool built = [] {
        for (int i = 0; i < 3; ++i) IN_AC[i] = sess_vec::AIRCRAFT[i];  // envelope + start state
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

static std::vector<std::vector<std::uint8_t>> payloads_of(const session::ServerFrames& f) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const auto& p : f) out.push_back(p.second);
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

// Read the whole downstream to EOF, reassembling frames.
static bool collect_to_eof(netsock::socket_t s, std::vector<std::vector<std::uint8_t>>& out,
                           std::size_t& pending) {
    framing::StreamReassembler r;
    std::uint8_t buf[4096];
    bool ok = true;
    while (true) {
        std::ptrdiff_t n = netsock::recv_some(s, buf, sizeof(buf));
        if (n > 0) {
            if (!r.feed(buf, static_cast<std::size_t>(n), out)) { ok = false; break; }
        } else {
            break;
        }
    }
    pending = r.pending();
    return ok;
}

// ---------------------------------------------------------------------------------------------
// LEG 1: one socket sub-leg. The client sends `cmds` UP in the given order, chunked to `chunk` bytes
// per send; the server produces frames the client reads back. Asserts the downstream frames are
// byte-identical to `ref` (== build_server_frames). Returns fail count.
// ---------------------------------------------------------------------------------------------
static int run_socket_subleg(const Rails& rails, const session::Scenario& sc,
                             const std::vector<std::vector<std::uint8_t>>& ref,
                             const std::vector<input001::InputCommand>& cmds, std::size_t chunk,
                             const char* label) {
    // Encode the (already-ordered) commands into one upstream byte stream: framing(encode_command).
    std::vector<std::uint8_t> upstream;
    for (const auto& c : cmds) {
        std::vector<std::uint8_t> rec, framed;
        input001::encode_command(c, rec);
        framing::encode_frame(rec, framed);
        upstream.insert(upstream.end(), framed.begin(), framed.end());
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::uint16_t port_val = 0;
    bool port_ready = false;
    bool commands_sent = false;

    netinput::Stats stats;
    std::atomic<int> done{0};

    std::thread server([&] {
        std::uint16_t port = 0;
        netsock::socket_t listener = netsock::listen_loopback(0, port, /*backlog=*/2);
        if (netsock::is_valid(listener)) netsock::set_nonblocking(listener);  // accept_one must poll
        {
            std::lock_guard<std::mutex> lk(mtx);
            port_val = netsock::is_valid(listener) ? port : 0;
            port_ready = true;
        }
        cv.notify_all();
        if (!netsock::is_valid(listener)) { ++done; return; }

        netinput::CommandQueue q(sc.n_aircraft);
        netinput::InputProducer producer(rails, sc, q);
        // rendezvous: block the very first iteration until the client has sent ALL commands, so they
        // are ingested (floor 0) before any tick is stepped. No sleeps — coupled to commands_sent.
        auto on_frame = [&](std::size_t fi) {
            if (fi != 0) return;
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&] { return commands_sent; });
        };
        stats = netinput::broadcast_input(listener, producer, q, /*min_initial=*/1,
                                          /*accept_deadline_ms=*/10000, on_frame);
        netsock::close_socket(listener);
        ++done;
    });

    auto wait_port = [&]() -> std::uint16_t {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return port_ready; });
        return port_val;
    };

    std::vector<std::vector<std::uint8_t>> got;
    std::size_t pending = 0;
    std::thread client([&] {
        std::uint16_t port = wait_port();
        if (port == 0) { ++done; return; }
        netsock::socket_t s = netsock::connect_loopback(port);
        if (netsock::is_valid(s)) {
            // send the scrambled command stream UP, chunked (send_all per chunk)
            for (std::size_t off = 0; off < upstream.size(); off += chunk) {
                std::size_t take = (off + chunk <= upstream.size()) ? chunk : upstream.size() - off;
                netsock::send_all(s, upstream.data() + off, take);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                commands_sent = true;
            }
            cv.notify_all();
            collect_to_eof(s, got, pending);  // read the downstream snapshots back
            netsock::close_socket(s);
        }
        ++done;
    });

    std::thread watchdog([&] {
        for (int i = 0; i < 400 && done.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // up to ~40 s
        if (done.load() < 2) {
            std::printf("FAIL layer-15b socket sub-leg [%s] TIMED OUT (socket hang)\n", label);
            std::fflush(stdout);
            std::_Exit(1);
        }
    });
    watchdog.detach();

    server.join();
    client.join();

    int fails = 0;
    const std::size_t n_cmds = cmds.size();
    if (!stats.ok || stats.frames_sent != ref.size() || stats.joins != 1) {
        ++fails;
        std::printf("FAIL [%s]: server stats (ok=%d sent=%zu/%zu joins=%zu)\n", label,
                    stats.ok ? 1 : 0, stats.frames_sent, ref.size(), stats.joins);
    }
    if (stats.cmds_ok != n_cmds || stats.cmds_stale != 0 || stats.cmds_oob != 0) {
        ++fails;
        std::printf("FAIL [%s]: command accounting (ok=%zu stale=%zu oob=%zu; want %zu/0/0)\n", label,
                    stats.cmds_ok, stats.cmds_stale, stats.cmds_oob, n_cmds);
    }
    if (pending != 0 || !frames_equal(got, ref)) {
        ++fails;
        std::printf("FAIL [%s]: downstream frames NOT byte-identical to build_server_frames "
                    "(got %zu of %zu, pending %zu)\n", label, got.size(), ref.size(), pending);
    }
    if (fails == 0)
        std::printf("  sub-leg [%s] PASS: %zu commands sent UP scrambled (chunk=%zu B) -> %zu frames "
                    "byte-identical to the phase-schedule server\n", label, n_cmds, chunk, ref.size());
    return fails;
}

static int run_socket_leg() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);

    // Guard the grid-exactness assumption: every command must survive the lossy wire UNCHANGED, or
    // byte-identity to build_server_frames would be impossible.
    int fails = 0;
    for (const auto& c : canon) {
        std::vector<std::uint8_t> rec;
        input001::encode_command(c, rec);
        std::size_t pos = 0;
        input001::InputCommand d;
        bool ok = input001::decode_command(rec.data(), rec.size(), pos, d) && pos == rec.size();
        if (!ok || d.apply_tick != c.apply_tick || d.aircraft != c.aircraft || d.seq != c.seq ||
            d.target_phi != c.target_phi || d.target_g != c.target_g || d.throttle != c.throttle ||
            d.fire != c.fire) {
            ++fails;
            std::printf("FAIL: INPUT-001 command not grid-exact (aircraft=%lld tick=%lld) — the "
                        "scenario values must be dyadic\n",
                        static_cast<long long>(c.aircraft), static_cast<long long>(c.apply_tick));
        }
    }
    if (fails) return fails;

    // Sanity: the in-process reference itself reproduces the phase-schedule server.
    if (!frames_equal(run_input_frames(rails, sc, canon), ref)) {
        std::printf("FAIL: in-process input producer != build_server_frames (canonical order)\n");
        return 1;
    }

    // Sub-leg A: fully REVERSED command order, 1 byte per send (maximal reassembler fragmentation).
    std::vector<input001::InputCommand> rev(canon.rbegin(), canon.rend());
    fails += run_socket_subleg(rails, sc, ref, rev, /*chunk=*/1, "reversed/1B");

    // Sub-leg B: ROTATE-by-3 order, 7 bytes per send (a different scramble + chunk boundary).
    std::vector<input001::InputCommand> rot = canon;
    if (rot.size() > 3) std::rotate(rot.begin(), rot.begin() + 3, rot.end());
    fails += run_socket_subleg(rails, sc, ref, rot, /*chunk=*/7, "rotate3/7B");
    return fails;
}

// ---------------------------------------------------------------------------------------------
// LEG 2: the DROP + HOLD-LAST ordering contract, in-process (no sockets, no timing).
// ---------------------------------------------------------------------------------------------
static int run_contract_leg() {
    const Rails rails = sealed_rails();
    const session::Scenario& sc = input_scenario();
    const auto ref = payloads_of(session::build_server_frames(rails, sc));
    const auto canon = netinput::commands_from_scenario(sc);
    int fails = 0;

    // (a) REVERSED order + injected STALE (past-tick) and OUT-OF-RANGE commands + a lower-seq
    // duplicate that must LOSE — all still reproduce the canonical frames.
    std::vector<input001::InputCommand> scrambled(canon.rbegin(), canon.rend());
    input001::InputCommand stale;   // apply_tick -5 is below floor 0 => STALE, ignored
    stale.apply_tick = -5; stale.aircraft = 0; stale.seq = 99; stale.target_phi = 0.5;
    scrambled.push_back(stale);
    input001::InputCommand oob;      // aircraft 7 out of [0,3) => OUT_OF_RANGE, ignored
    oob.apply_tick = 10; oob.aircraft = 7; oob.seq = 99; oob.target_g = 2.0;
    scrambled.push_back(oob);
    for (const auto& c : canon) {    // duplicate every command with a LOWER seq: must never win
        input001::InputCommand lo = c; lo.seq = c.seq - 1000; lo.throttle = 0.0; lo.target_phi = -1.0;
        scrambled.push_back(lo);
    }
    if (!frames_equal(run_input_frames(rails, sc, scrambled), ref)) {
        ++fails;
        std::printf("FAIL contract (a): reversed + stale/oob/low-seq injected != canonical frames\n");
    }

    // (b) queue-level: STALE drop, OUT_OF_RANGE, and order-independent max-seq winner.
    {
        netinput::CommandQueue q(3);
        input001::InputCommand c; c.apply_tick = 5; c.aircraft = 0; c.seq = 0;
        bool ok = (q.submit(c) == netinput::CommandQueue::Result::ACCEPTED) && q.buffered() == 1;
        // consume ticks 0..5 -> floor becomes 6; a resubmit for tick 5 is now STALE.
        for (int t = 0; t <= 5; ++t) q.consume(t);
        input001::InputCommand late; late.apply_tick = 5; late.aircraft = 0; late.seq = 1;
        ok = ok && (q.submit(late) == netinput::CommandQueue::Result::STALE) && q.floor() == 6;
        input001::InputCommand bad; bad.apply_tick = 10; bad.aircraft = 3; bad.seq = 0;  // idx 3 oob
        ok = ok && (q.submit(bad) == netinput::CommandQueue::Result::OUT_OF_RANGE);
        if (!ok) { ++fails; std::printf("FAIL contract (b): stale/oob/floor accounting\n"); }
    }
    {
        // max-seq winner is order-independent: submit (seq 1) then (seq 5) vs (seq 5) then (seq 1).
        auto winner_throttle = [](std::initializer_list<int> seqs) {
            netinput::CommandQueue q(1);
            int i = 0;
            for (int s : seqs) {
                input001::InputCommand c; c.apply_tick = 3; c.aircraft = 0; c.seq = s;
                c.throttle = (s == 5) ? 1.0 : 0.25;  // the seq-5 command carries a distinct throttle
                q.submit(c); ++i;
            }
            input001::InputCommand out;
            return q.peek(3, 0, out) ? out.throttle : -1.0;
        };
        double a = winner_throttle({1, 5});
        double b = winner_throttle({5, 1});
        if (a != 1.0 || b != 1.0) {
            ++fails;
            std::printf("FAIL contract (b): max-seq winner not order-independent (%.3f vs %.3f)\n",
                        a, b);
        }
    }

    if (fails == 0)
        std::printf("  leg 2 PASS: drop (stale/oob) + hold-last + max-seq selection reproduce the "
                    "canonical %zu-frame stream regardless of submit order\n", ref.size());
    return fails;
}

// Cross-impl byte pin: the SAME command the input001_ref.py self-test pins must encode to the SAME
// bytes here (C++ ≡ Python for the INPUT-001 wire, inherited through the sealed geo001 pipeline).
static int run_codec_parity_leg() {
    input001::InputCommand c;
    c.apply_tick = 5; c.aircraft = 1; c.seq = 2;
    c.target_phi = 0.5; c.target_g = 1.5; c.throttle = 0.75; c.fire = true;
    std::vector<std::uint8_t> got;
    input001::encode_command(c, got);
    const std::uint8_t want[] = {0x0a, 0x02, 0x04, 0xc0, 0x84, 0x3d, 0xc0, 0x8d, 0xb7, 0x01,
                                 0xe0, 0xc6, 0x5b, 0x02};
    bool ok = got.size() == sizeof(want);
    for (std::size_t i = 0; ok && i < sizeof(want); ++i) ok = (got[i] == want[i]);
    // and a full round-trip recovers the command exactly (dyadic values are grid-exact)
    std::size_t pos = 0;
    input001::InputCommand d;
    ok = ok && input001::decode_command(got.data(), got.size(), pos, d) && pos == got.size() &&
         d.apply_tick == c.apply_tick && d.aircraft == c.aircraft && d.seq == c.seq &&
         d.target_phi == c.target_phi && d.target_g == c.target_g && d.throttle == c.throttle &&
         d.fire == c.fire;
    if (!ok) {
        std::printf("FAIL INPUT-001 codec parity: encoding/round-trip mismatch vs input001_ref.py\n");
        return 1;
    }
    std::printf("  codec PASS: INPUT-001 encode == input001_ref.py pin (14 bytes), round-trip exact\n");
    return 0;
}

int main() {
    netsock::WsaGuard wsa;
    int fails = 0;
    fails += run_codec_parity_leg();
    fails += run_socket_leg();
    fails += run_contract_leg();
    if (fails == 0) {
        std::printf("PASS: layer-15b input-upstream — the authoritative kernel's frames are a pure "
                    "function of the tick-stamped command SET, invariant to upstream order/chunking\n");
        return 0;
    }
    std::printf("RESULT: layer-15b input bridge FAIL (%d mismatches)\n", fails);
    return 1;
}
