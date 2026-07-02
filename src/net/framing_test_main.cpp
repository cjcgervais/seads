// SEADS length-prefixed stream framing parity test (netcode layer 7). Asserts the C++ framing
// codec == the Python reference vectors (tools/framing_ref.py):
//   1) ENCODE parity — encode_frame over each batch's payloads reproduces the canonical stream bytes;
//   2) CHUNK-BOUNDARY INVARIANCE — feeding the stream through StreamReassembler under EVERY listed
//      chunking yields byte-identical frames (== the original payloads), regardless of split points;
//   3) PARTIAL-FRAME BUFFERED — a stream truncated mid-payload emits no frame and buffers the tail,
//      then completing it emits exactly the buffered frame;
//   4) OVERLONG length prefix is rejected (feed returns false).
// Exit 0 PASS, 1 FAIL.
#include "framing.h"
#include "framing_vectors.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace seads;

namespace {

// The payload bytes of frame f in batch b (a sub-span of the canonical stream).
std::vector<std::uint8_t> frame_payload(const framing_vec::Batch& b, int f) {
    const auto& fs = b.frames[f];
    return std::vector<std::uint8_t>(b.stream + fs.off, b.stream + fs.off + fs.len);
}

}  // namespace

int main() {
    int fails = 0;

    for (int bi = 0; bi < framing_vec::BATCH_COUNT; ++bi) {
        const auto& b = framing_vec::BATCHES[bi];

        // expected frame payloads for this batch
        std::vector<std::vector<std::uint8_t>> expected;
        for (int f = 0; f < b.n_frames; ++f) expected.push_back(frame_payload(b, f));

        // --- 1) ENCODE parity: re-frame the payloads, expect the canonical stream bytes ---
        std::vector<std::uint8_t> enc;
        for (const auto& p : expected) framing::encode_frame(p, enc);
        bool enc_ok = (static_cast<int>(enc.size()) == b.stream_len) &&
                      (b.stream_len == 0 || std::memcmp(enc.data(), b.stream, b.stream_len) == 0);
        if (!enc_ok) {
            ++fails;
            std::printf("FAIL encode batch '%s': got %zu bytes, expected %d\n",
                        b.name, enc.size(), b.stream_len);
        }

        // --- 2) CHUNK-BOUNDARY INVARIANCE: every chunking yields the same frames ---
        for (int ci = 0; ci < b.n_chunkings; ++ci) {
            const auto& ch = b.chunkings[ci];
            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> got;
            bool ok = true;
            for (int si = 0; si < ch.n_spans; ++si) {
                const auto& sp = ch.spans[si];
                ok = r.feed(b.stream + sp.off, static_cast<std::size_t>(sp.len), got) && ok;
            }
            bool match = ok && got.size() == expected.size() && r.pending() == 0;
            for (std::size_t k = 0; match && k < got.size(); ++k) {
                if (got[k] != expected[k]) match = false;
            }
            if (!match) {
                ++fails;
                std::printf("FAIL batch '%s' chunking '%s': got %zu frames, expected %zu (pending %zu)\n",
                            b.name, ch.name, got.size(), expected.size(), r.pending());
            }
        }

        // --- 3) PARTIAL-FRAME BUFFERED: split the stream mid-last-frame ---
        if (b.stream_len > 5 && b.n_frames > 0) {
            framing::StreamReassembler r;
            std::vector<std::vector<std::uint8_t>> got;
            int cut = b.stream_len - 3;  // stop 3 bytes short of the end (inside the final payload)
            r.feed(b.stream, static_cast<std::size_t>(cut), got);
            std::size_t after_first = got.size();
            bool buffered = (after_first == expected.size() - 1) && r.pending() > 0;
            r.feed(b.stream + cut, 3, got);
            bool completed = (got.size() == expected.size()) && r.pending() == 0 &&
                             got.back() == expected.back();
            if (!buffered || !completed) {
                ++fails;
                std::printf("FAIL batch '%s' partial-frame buffering (after_first=%zu, final=%zu)\n",
                            b.name, after_first, got.size());
            }
        }
    }

    // --- 4) OVERLONG length prefix rejected ---
    {
        framing::StreamReassembler r;
        std::vector<std::vector<std::uint8_t>> got;
        std::uint8_t overlong[11];
        for (int i = 0; i < 10; ++i) overlong[i] = 0x80;  // 10 continuation bytes, no terminator
        overlong[10] = 0x00;
        if (r.feed(overlong, sizeof(overlong), got)) {
            ++fails;
            std::printf("FAIL overlong length prefix should be rejected\n");
        }
    }

    if (fails == 0) {
        std::printf("PASS: framing codec byte-exact vs reference (%d batches; chunk-boundary "
                    "invariance + partial-frame buffering + overlong reject)\n",
                    framing_vec::BATCH_COUNT);
        return 0;
    }
    std::printf("RESULT: framing FAIL (%d mismatches)\n", fails);
    return 1;
}
