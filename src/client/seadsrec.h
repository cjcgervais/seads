// SEADS flight recording container (.seadsrec) — netcode-faithful trajectory feed for the
// renderer (Step 5). DOWNSTREAM-ONLY: this is the presentation/transport boundary, strictly
// outside the kernel and the world_hash. It rides seal v1.12r0 (no rail/golden/kernel touched).
//
// A recording is exactly the byte stream a 20 Hz server would put on the wire: a small file
// header (radius + cadences, so a viewer can scale the globe) followed by N length-prefixed
// GEO-001/KIN-002/WEAPON-001 snapshot frames (netsnap::encode_snapshot — protocol 4, the sealed
// codec reused verbatim). A client decodes each frame and feeds it to interp::SnapshotBuffer
// exactly as it would a live snapshot — so the recorder proves the whole layer 1/2/4a path AND
// the weapon wire (hp + rounds) end to end.
//
// Container layout (all integers little-endian; this is a non-canonical PRESENTATION format, so
// its byte order is a local convenience and is NOT the hashed/sealed snapshot):
//   magic[8]      = "SEADSREC"
//   u32 version   = SEADSREC_VERSION
//   u32 protocol  = snapshot protocol (4)
//   u32 tick_hz   = physics rate (100)
//   u32 snap_hz   = snapshot cadence the recording was captured at (e.g. 20)
//   f64 radius_m  = sphere radius (globe scale for the viewer)
//   u32 n_frames
//   n_frames x { u32 payload_len ; u8[payload_len] = encode_snapshot(...) }
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "snapshot.h"  // seads::netsnap::Snapshot (decoded frames)

namespace seads {
namespace client {

constexpr char     SEADSREC_MAGIC[8] = {'S', 'E', 'A', 'D', 'S', 'R', 'E', 'C'};
constexpr uint32_t SEADSREC_VERSION  = 1;

// Recording metadata (the file header). Cadences/radius let a viewer scale time and the globe.
struct RecordingMeta {
    uint32_t version  = SEADSREC_VERSION;
    uint32_t protocol = netsnap::SNAPSHOT_PROTOCOL;
    uint32_t tick_hz  = 100;
    uint32_t snap_hz  = 20;
    double   radius_m = 15000.0;
    uint32_t n_frames = 0;
};

// A loaded recording: metadata + decoded snapshot frames (ascending server_tick).
struct Recording {
    RecordingMeta meta;
    std::vector<netsnap::Snapshot> frames;
};

// Serialize a recording from already-encoded wire payloads (one per frame). `frames` holds the
// raw bytes from netsnap::encode_snapshot; this is what hits the wire, captured verbatim.
void write_recording(const RecordingMeta& meta,
                     const std::vector<std::vector<uint8_t>>& frames,
                     std::vector<uint8_t>& out);

// Parse a .seadsrec image. Decodes each frame via netsnap::decode_snapshot (so a malformed or
// truncated payload is rejected). Returns false on any structural error.
bool read_recording(const uint8_t* data, size_t len, Recording& out);

}  // namespace client
}  // namespace seads
