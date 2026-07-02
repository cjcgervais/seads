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
// A v2 recording ALSO carries the layer-6 combat EVENT JOURNAL: one record per connecting round
// (the kernel's per-round hit queue, `Kernel::hit_events()`, captured at the FULL 100 Hz physics
// rate — not the 20 Hz snapshot cadence), quantized exactly as the reliable event channel does
// (damage/hp in milli-hp via the sealed HP_SCALE). A viewer replays it for a precise-tick,
// per-round kill-feed + damage numbers, instead of inferring kills from 20 Hz state transitions.
// This is downstream presentation only — the hit queue is observable kernel output, never hashed.
//
// A v3 recording ALSO carries the per-aircraft AIRFRAME TYPE (one code per aircraft slot, the
// stable AircraftType presentation codes from aircraft_mesh.h) so the viewer can draw each
// aircraft's roster mesh variant. The sealed wire deliberately carries no type field — this is
// recording metadata, static per flight, appended after the journal (same back-compat pattern:
// a v1/v2 file loads with an empty type list, and every aircraft falls back to the generic mesh).
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
//   -- v2 only, appended after the frames: --
//   u32 n_events
//   n_events x { i64 tick, i64 target, i64 attacker, i64 damage_milli, i64 hp_after_milli,
//                i64 killed, i64 region }   (each i64 little-endian, two's complement)
//   -- v3 only, appended after the journal: --
//   u32 n_types
//   n_types x u32 type_code   (AircraftType code for aircraft slot i; unknown -> generic)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "snapshot.h"  // seads::netsnap::Snapshot (decoded frames)

namespace seads {
namespace client {

constexpr char     SEADSREC_MAGIC[8] = {'S', 'E', 'A', 'D', 'S', 'R', 'E', 'C'};
constexpr uint32_t SEADSREC_VERSION  = 3;   // v2 added the combat event journal; v3 the aircraft types
                                            // (v1 = frames only; every trailer is append-only + optional)

// One replayable combat event: the layer-6 journal record (mirrors event::Event) plus the struck
// region (v1.18r0). tick is the 100 Hz physics tick the round connected on; damage/hp are milli-hp
// (HP_SCALE=1e3, integer-exact). One record PER CONNECTING ROUND (two rounds on one tick = two).
struct RecEvent {
    int64_t tick = 0;
    int64_t target = 0;
    int64_t attacker = -1;
    int64_t damage_milli = 0;
    int64_t hp_after_milli = 0;
    int64_t killed = 0;
    int64_t region = 0;      // 0=ENGINE, 1=WING, 2=TAIL
};

// Recording metadata (the file header). Cadences/radius let a viewer scale time and the globe.
struct RecordingMeta {
    uint32_t version  = SEADSREC_VERSION;
    uint32_t protocol = netsnap::SNAPSHOT_PROTOCOL;
    uint32_t tick_hz  = 100;
    uint32_t snap_hz  = 20;
    double   radius_m = 15000.0;
    uint32_t n_frames = 0;
};

// A loaded recording: metadata + decoded snapshot frames (ascending server_tick) + the combat
// event journal (empty for a v1 recording; ascending tick, then round order within a tick) +
// the per-aircraft airframe type codes (empty pre-v3; types[i] belongs to aircraft slot/id i).
struct Recording {
    RecordingMeta meta;
    std::vector<netsnap::Snapshot> frames;
    std::vector<RecEvent> events;
    std::vector<uint32_t> types;
};

// Serialize a recording from already-encoded wire payloads (one per frame). `frames` holds the
// raw bytes from netsnap::encode_snapshot; this is what hits the wire, captured verbatim.
// This overload writes a v1-shaped body with an empty event journal (v2 header, 0 events).
void write_recording(const RecordingMeta& meta,
                     const std::vector<std::vector<uint8_t>>& frames,
                     std::vector<uint8_t>& out);

// As above, plus the trailing combat event journal (v2 trailer; empty type list).
void write_recording(const RecordingMeta& meta,
                     const std::vector<std::vector<uint8_t>>& frames,
                     const std::vector<RecEvent>& events,
                     std::vector<uint8_t>& out);

// As above, plus the per-aircraft airframe type codes (v3 trailer; types[i] = aircraft slot i).
void write_recording(const RecordingMeta& meta,
                     const std::vector<std::vector<uint8_t>>& frames,
                     const std::vector<RecEvent>& events,
                     const std::vector<uint32_t>& types,
                     std::vector<uint8_t>& out);

// Parse a .seadsrec image. Decodes each frame via netsnap::decode_snapshot (so a malformed or
// truncated payload is rejected) and, for a v2 image, the trailing event journal. Returns false
// on any structural error.
bool read_recording(const uint8_t* data, size_t len, Recording& out);

}  // namespace client
}  // namespace seads
