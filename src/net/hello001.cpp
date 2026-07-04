// SEADS HELLO-001 identity-claim handshake codec (netcode layer 21). See hello001.h.
#include "hello001.h"

#include "geo001.h"

namespace seads {
namespace hello001 {

void encode_hello(const HelloInfo& h, std::vector<std::uint8_t>& out) {
    out.push_back(VERSION);
    geo001::encode_i64(h.token, out);
}

bool decode_hello(const std::uint8_t* data, std::size_t len, std::size_t& pos, HelloInfo& out) {
    if (pos >= len || data[pos] != VERSION) return false;
    ++pos;
    if (!geo001::decode_i64(data, len, pos, out.token)) return false;
    return true;
}

}  // namespace hello001
}  // namespace seads
