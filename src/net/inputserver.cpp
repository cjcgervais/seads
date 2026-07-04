// SEADS authoritative INPUT server (netcode layer 15b). See inputserver.h.
#include "inputserver.h"

#include <cstddef>
#include <utility>

#include "framing.h"

namespace seads {
namespace netinput {

InputProducer::InputProducer(const Rails& rails, const session::Scenario& sc, CommandQueue& queue)
    : sc_(&sc), q_(&queue), server_(rails) {
    for (unsigned i = 0; i < sc.n_aircraft; ++i) {
        const session::AircraftSpec& a = sc.aircraft[i];
        server_.add(a.lat, a.lon, a.psi, a.phi, a.alt, a.tas, 0.0, a.env->hp_start,
                    a.env->ammo_start, a.env->engine_frac, a.env->wing_frac, a.env->tail_frac);
        held_.push_back(Command{0.0, 1.0, 0.0, false});  // neutral: wings level, 1 g, idle, no fire
        envs_.push_back(a.env);
    }
}

bool InputProducer::next(std::int64_t& emit_tick, std::vector<std::uint8_t>& payload) {
    const session::Scenario& sc = *sc_;
    if (!emitted_initial_) {                          // initial world (pre-step), emit_tick 0
        emitted_initial_ = true;
        emit_tick = 0;
        payload = session::serialize_world(server_, 0);
        return true;
    }
    while (t_ < sc.ticks) {
        const unsigned t = t_ + 1;
        // The command governing the step t_ -> t is stamped for apply_tick == t_ (the pre-step tick,
        // the session::server_command_at(a, t-1) convention). No command for an aircraft => hold-last.
        for (unsigned i = 0; i < sc.n_aircraft; ++i) {
            input001::InputCommand ic;
            if (q_->peek(static_cast<std::int64_t>(t_), static_cast<std::int64_t>(i), ic))
                held_[i] = Command{ic.target_phi, ic.target_g, ic.throttle, ic.fire};
        }
        q_->consume(static_cast<std::int64_t>(t_));   // drop this tick's entries, advance the floor
        server_.step(held_, envs_);
        t_ = t;
        if (t % sc.snap_every == 0) {
            emit_tick = static_cast<std::int64_t>(t);
            payload = session::serialize_world(server_, static_cast<std::int64_t>(t));
            return true;
        }
    }
    return false;                                     // scenario exhausted
}

std::vector<input001::InputCommand> commands_from_scenario(const session::Scenario& sc) {
    std::vector<input001::InputCommand> out;
    for (unsigned i = 0; i < sc.n_aircraft; ++i) {
        const session::AircraftSpec& a = sc.aircraft[i];
        for (unsigned p = 0; p < a.n_phase; ++p) {
            const session::Phase& ph = a.sched[p];
            input001::InputCommand ic;
            ic.apply_tick = static_cast<std::int64_t>(ph.start_tick);
            ic.aircraft = static_cast<std::int64_t>(i);
            ic.seq = static_cast<std::int64_t>(p);  // phase ordinal: monotone, unique per aircraft
            ic.target_phi = ph.target_phi;
            ic.target_g = ph.target_g;
            ic.throttle = ph.throttle;
            ic.fire = ph.fire;
            out.push_back(ic);
        }
    }
    return out;
}

// One client the server is receiving upstream commands from: its socket + a stream reassembler that
// turns arbitrary byte chunks back into whole INPUT-001 command records.
namespace {
struct RxClient {
    netsock::socket_t s;
    framing::StreamReassembler rx;
};
}  // namespace

Stats broadcast_input(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame) {
    Stats st;
    std::vector<RxClient> clients;

    auto submit_payloads = [&](const std::vector<std::vector<std::uint8_t>>& payloads) {
        for (const auto& p : payloads) {
            input001::InputCommand ic;
            std::size_t pos = 0;
            if (!input001::decode_command(p.data(), p.size(), pos, ic)) continue;  // skip malformed
            switch (queue.submit(ic)) {
                case CommandQueue::Result::ACCEPTED: ++st.cmds_ok; break;
                case CommandQueue::Result::STALE: ++st.cmds_stale; break;
                case CommandQueue::Result::OUT_OF_RANGE: ++st.cmds_oob; break;
            }
        }
    };

    auto accept_all = [&]() {
        while (true) {
            netsock::socket_t c = netsock::accept_one(listener);
            if (!netsock::is_valid(c)) break;
            RxClient rc;
            rc.s = c;
            clients.push_back(std::move(rc));
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
        for (RxClient& c : clients) netsock::close_socket(c.s);
        return st;  // ok stays false
    }

    // --- bidirectional frame loop: read upstream commands, step the producer, send the frame down -
    std::vector<netsock::socket_t> fds, ready;
    std::vector<std::vector<std::uint8_t>> pv;
    std::vector<std::uint8_t> payload, frame;
    std::uint8_t rbuf[4096];
    for (std::size_t fi = 0;; ++fi) {
        if (on_frame) on_frame(fi);  // rendezvous hook: block until a client has sent its commands

        // service membership + upstream, one poll (timeout 0). A readable client either sent command
        // bytes (n>0) or closed (n<=0 => LEAVE). Drain all currently-available bytes so a whole
        // command burst is ingested before the ticks it governs are stepped this iteration.
        fds.clear();
        for (const RxClient& c : clients) fds.push_back(c.s);
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
                    submit_payloads(pv);
                } while (netsock::wait_readable(s, 0));
                if (leave) {
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
                netsock::close_socket(clients[i].s);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
                ++st.leaves;
            }
        }
        ++st.frames_sent;
    }

    for (RxClient& c : clients) netsock::close_socket(c.s);
    st.ok = true;
    return st;
}

}  // namespace netinput
}  // namespace seads
