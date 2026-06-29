# SEADS client — Step 5 renderer (`src/client/`)

**Downstream-only presentation.** Everything here reads sim output and draws it. None of it
advances a kernel, hashes state, or feeds a bit back into the deterministic core, so the whole
directory lives **outside** the determinism gate and the `world_hash` — it rides seal **v1.4r0**
with **no seal bump** (and the `lint_determinism` scan deliberately excludes `src/client`, so the
globe projection is free to use libm trig).

It is the natural consumer of the netcode work: it replays the exact **GEO-001/KIN-001 wire
stream** (protocol 2) and smooths remotes with the **layer-4a** interpolation buffer. The native
viewer also has a **`--fly`** mode that *flies* the own aircraft from live keyboard input through
the **layer-4b** prediction harness (`seads_predict`), so prediction (own) and interpolation
(remotes) run on the same globe at once.

## Pieces

| File | Role |
|------|------|
| `seadsrec.{h,cpp}` | `.seadsrec` recording container — a file header (radius/cadence) + N length-prefixed `netsnap::encode_snapshot` frames. Decodes via the sealed codec, so a malformed frame is rejected. |
| `record_main.cpp` → **`seads_record`** | Drives the **real sealed kernel** and captures the flight as the 20 Hz wire stream a server would send: writes `.seadsrec` (for the C++ viewer) **and** `trajectory.js` (for the web viewer), both from the *same decoded* frames. Has a built-in 3-ship `--demo` plus sealed-scenario replay (`--id`). |
| `globe.{h}` | Pure projection math: sphere(lat,lon,alt) → cartesian (Y = polar axis), orbit camera, perspective project, near/far-hemisphere cull. All pure functions → unit-tested headlessly. |
| `playback.{h,cpp}` | Wraps a decoded recording in `interp::SnapshotBuffer` and samples it at a render tick ~100 ms in the past (the 4a delay). Pure given a render tick. |
| `client_test_main.cpp` → **`seads_client_test`** | ctest gate: globe invariants + container round-trip + playback == layer-4a interp. Built in CI; **no GPU needed.** |
| `viewer_main.cpp` → **`seads_viewer`** | OPTIONAL raylib 3D globe (built only with `-DSEADS_CLIENT=ON`). Replay mode: wall-clock render-interpolation, trails, HUD, orbit camera, `--selfcheck N` headless data-path mode. **`--fly` mode:** own ship flown live (A/D bank, W/S climb) via a fixed-100 Hz `predict::Predictor` driving the real sealed kernel; remotes stay on the layer-4a interp path. `--fly --selfcheck N` is a headless (no-GPU, no-recording) proof of the input→prediction path. Links `seads_predict` for fly mode. |
| `web/` | Zero-build Three.js globe viewer. Same interpolation (mirrored in JS), opens in any browser. |

## Quick start

```powershell
# 1. record the demo flight (writes both consumer formats)
cmake --build build-gcc --target seads_record
.\build-gcc\seads_record.exe --demo --out flight.seadsrec --js src\client\web\trajectory.js
#   or replay a sealed golden:  --id GOLDEN-SK-TurnClimb-001

# 2a. web viewer — just open it (needs a sibling trajectory.js)
#     file:// works; or serve it:  python -m http.server -d src\client\web
start src\client\web\index.html

# 2b. native viewer (optional graphics dep)
cmake -S . -B build-client -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DSEADS_CLIENT=ON
cmake --build build-client --target seads_viewer
.\build-client\seads_viewer.exe flight.seadsrec            # GUI (replay)
.\build-client\seads_viewer.exe flight.seadsrec --selfcheck 8   # headless data-path check
.\build-client\seads_viewer.exe flight.seadsrec --fly      # FLY: own ship live (A/D bank, W/S climb)
.\build-client\seads_viewer.exe --fly --selfcheck 6        # headless fly-path proof (no recording/GPU)
```

## Boundaries (why this can't break determinism)

- **Reads, never writes the sim.** Kernel access is through the read-only getters
  (`lat()/lon()/psi()/phi()/alt()/tas()`); the recorder steps the kernel forward but only to
  *capture*, exactly like the golden runners.
- **Quantized wire is for display only.** Frames are GEO-001/KIN-001 (lossy by quantization). The
  canonical `Kernel::snapshot()` (raw LE-f64) remains the sole `world_hash` source of truth.
- **Wall clock lives in the client.** The viewers read wall time to pick a render moment ~100 ms
  in the past — banned *inside* the kernel, allowed here (presentation).
- **CI stays kernel-only.** `SEADS_CLIENT` is `OFF` by default, so `guardian.yml` never pulls a GUI
  library; the cross-toolchain determinism matrix builds nothing from this directory except the
  headless `seads_client_test`.
