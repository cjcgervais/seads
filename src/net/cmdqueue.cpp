// SEADS canonical tick-stamped Command queue (netcode layer 15b ordering contract). See cmdqueue.h.
#include "cmdqueue.h"

#include <limits>

#include "geo001.h"

namespace seads {
namespace netinput {

// Total order over the wire-relevant fields, so the winner for an (apply_tick, aircraft) key is a
// pure function of the command SET (independent of submit order). seq dominates — it is the client's
// intent tag — then the QUANTIZED continuous fields (the exact bytes on the wire) and fire, so two
// commands equal here are byte-identical on the wire and interchangeable. Returns true iff a < b.
static bool wire_prec(const input001::InputCommand& a, const input001::InputCommand& b) {
    if (a.seq != b.seq) return a.seq < b.seq;
    int64_t aphi = geo001::quantize(a.target_phi, input001::PHI_SCALE);
    int64_t bphi = geo001::quantize(b.target_phi, input001::PHI_SCALE);
    if (aphi != bphi) return aphi < bphi;
    int64_t ag = geo001::quantize(a.target_g, input001::TARGETG_SCALE);
    int64_t bg = geo001::quantize(b.target_g, input001::TARGETG_SCALE);
    if (ag != bg) return ag < bg;
    int64_t at = geo001::quantize(a.throttle, input001::THROTTLE_SCALE);
    int64_t bt = geo001::quantize(b.throttle, input001::THROTTLE_SCALE);
    if (at != bt) return at < bt;
    return (a.fire ? 1 : 0) < (b.fire ? 1 : 0);
}

CommandQueue::Result CommandQueue::submit(const input001::InputCommand& c) {
    if (c.aircraft < 0 || c.aircraft >= n_) return Result::OUT_OF_RANGE;
    if (c.apply_tick < floor_) return Result::STALE;
    auto key = std::make_pair(c.apply_tick, c.aircraft);
    auto it = pend_.find(key);
    if (it == pend_.end()) {
        pend_.emplace(key, c);
    } else if (wire_prec(it->second, c)) {
        it->second = c;  // c is the new maximal winner for this key
    }
    // else: an already-stored command outranks c under the canonical order — c is dropped.
    return Result::ACCEPTED;
}

bool CommandQueue::peek(int64_t tick, int64_t aircraft, input001::InputCommand& out) const {
    auto it = pend_.find(std::make_pair(tick, aircraft));
    if (it == pend_.end()) return false;
    out = it->second;
    return true;
}

void CommandQueue::consume(int64_t tick) {
    // erase every (tick, *) entry; the map is ordered so the tick's block is contiguous.
    auto lo = pend_.lower_bound(std::make_pair(tick, std::numeric_limits<int64_t>::min()));
    auto hi = pend_.lower_bound(std::make_pair(tick + 1, std::numeric_limits<int64_t>::min()));
    pend_.erase(lo, hi);
    if (floor_ < tick + 1) floor_ = tick + 1;
}

}  // namespace netinput
}  // namespace seads
