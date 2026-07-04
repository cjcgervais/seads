// SEADS INPUT-001 upstream Command codec — bit-for-bit mirror of tools/input001_ref.py.
#include "input001.h"

#include "geo001.h"

namespace seads {
namespace input001 {

void encode_command(const InputCommand& c, std::vector<uint8_t>& out) {
    geo001::encode_i64(c.apply_tick, out);
    geo001::encode_i64(c.aircraft, out);
    geo001::encode_i64(c.seq, out);
    geo001::encode_i64(geo001::quantize(c.target_phi, PHI_SCALE), out);
    geo001::encode_i64(geo001::quantize(c.target_g, TARGETG_SCALE), out);
    geo001::encode_i64(geo001::quantize(c.throttle, THROTTLE_SCALE), out);
    geo001::encode_i64(c.fire ? 1 : 0, out);
}

bool decode_command(const uint8_t* data, size_t len, size_t& pos, InputCommand& out) {
    int64_t apply_tick, aircraft, seq, phi_q, g_q, thr_q, fire_i;
    if (!geo001::decode_i64(data, len, pos, apply_tick)) return false;
    if (!geo001::decode_i64(data, len, pos, aircraft)) return false;
    if (!geo001::decode_i64(data, len, pos, seq)) return false;
    if (!geo001::decode_i64(data, len, pos, phi_q)) return false;
    if (!geo001::decode_i64(data, len, pos, g_q)) return false;
    if (!geo001::decode_i64(data, len, pos, thr_q)) return false;
    if (!geo001::decode_i64(data, len, pos, fire_i)) return false;
    out.apply_tick = apply_tick;
    out.aircraft   = aircraft;
    out.seq        = seq;
    out.target_phi = geo001::dequantize(phi_q, PHI_SCALE);
    out.target_g   = geo001::dequantize(g_q, TARGETG_SCALE);
    out.throttle   = geo001::dequantize(thr_q, THROTTLE_SCALE);
    out.fire       = (fire_i != 0);
    return true;
}

}  // namespace input001
}  // namespace seads
