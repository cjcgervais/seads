// SEADS BIND-001 seat-assignment handshake codec (netcode layer 18). See bind001.h.
#include "bind001.h"

#include "geo001.h"

namespace seads {
namespace bind001 {

void encode_bind(const BindInfo& b, std::vector<std::uint8_t>& out) {
    out.push_back(VERSION);
    geo001::encode_i64(b.seat, out);
    geo001::encode_i64(b.n_aircraft, out);
}

bool decode_bind(const std::uint8_t* data, std::size_t len, std::size_t& pos, BindInfo& out) {
    if (pos >= len || data[pos] != VERSION) return false;
    ++pos;
    if (!geo001::decode_i64(data, len, pos, out.seat)) return false;
    if (!geo001::decode_i64(data, len, pos, out.n_aircraft)) return false;
    return true;
}

}  // namespace bind001
}  // namespace seads
