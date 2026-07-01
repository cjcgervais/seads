cmake_minimum_required(VERSION 3.16)
project(SEADS LANGUAGES CXX
        DESCRIPTION "SEADS — deterministic spherical aerial dogfight simulator (ATM-Sphere v1.2r0)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/DeterminismFlags.cmake)

# Runtime linkage. NOTE: static linking is a *distribution* convenience, not a
# determinism requirement — the bit-for-bit promise comes from det_math + the strict
# FP flags in DeterminismFlags.cmake, never from how the C++ runtime is linked. So the
# MinGW-static hack below is scoped to MinGW only; Linux/macOS link threads portably.
if(MINGW)
  # Local Windows (winlibs MinGW): self-contained .exe with no runtime libstdc++/libgcc DLL.
  # winpthread is appended last so static libstdc++'s pthread refs resolve under both the
  # GCC and Clang (mingw) drivers (winpthread does not exist on Linux/macOS).
  add_link_options(-static -static-libgcc -static-libstdc++ -pthread)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -Wl,--start-group -lstdc++ -lwinpthread -Wl,--end-group")
elseif(NOT MSVC)
  # Linux / macOS: -static is unsupported on macOS and discouraged on glibc; just link
  # pthread (static libstdc++ may reference pthread_*). Threads::Threads is the portable way.
  find_package(Threads REQUIRED)
  link_libraries(Threads::Threads)
endif()

# --- deterministic math (the only sanctioned FP primitives) ---
add_library(seads_det_math STATIC src/det_math/det_math.cpp)
target_include_directories(seads_det_math PUBLIC src/det_math)
target_link_libraries(seads_det_math PUBLIC seads_det_flags)

# --- simulation kernel ---
add_library(seads_kernel STATIC src/kernel/kernel.cpp)
target_include_directories(seads_kernel PUBLIC src/kernel)
target_link_libraries(seads_kernel PUBLIC seads_det_math seads_det_flags)

# --- replay / world_hash ---
add_library(seads_replay STATIC src/replay/sha256.cpp)
target_include_directories(seads_replay PUBLIC src/replay)
target_link_libraries(seads_replay PUBLIC seads_det_flags)

# --- net / GEO-001 wire codec (transport; outside the kernel + world_hash) ---
# Deliberately does NOT link det_math: the wire layer uses no transcendentals, and it
# must never be on the path that feeds the deterministic sim. It links the strict FP
# flags only so float->fixed quantization rounds identically cross-toolchain.
add_library(seads_net STATIC src/net/geo001.cpp src/net/snapshot.cpp src/net/interp.cpp)
target_include_directories(seads_net PUBLIC src/net)
target_link_libraries(seads_net PUBLIC seads_det_flags)

# --- net / loopback lockstep harness (netcode layer 3 — the desync tripwire) ---
# Drives TWO kernels from one input timeline and compares per-tick canonical world_hash.
# It lives under src/net/ (a net concern) but it is a kernel DRIVER, so it links seads_kernel
# + seads_replay and is its OWN lib — seads_net stays pure (geo001/snapshot wire codecs only,
# no det_math/kernel). Net code stays outside the kernel: the timeline carries sim Commands,
# never wire bits (the GEO-001 wire is lossy; decode is for remotes/rendering, not the sim).
add_library(seads_lockstep STATIC src/net/lockstep.cpp)
target_include_directories(seads_lockstep PUBLIC src/net)
target_link_libraries(seads_lockstep PUBLIC seads_kernel seads_replay seads_det_flags)

# --- net / client-side prediction harness (netcode layer 4b) ---
# Predicts the OWN aircraft from local input and reconciles against authoritative snapshots
# (snap-to-authoritative + replay buffered inputs). Like lockstep it DRIVES the kernel, so it
# is its OWN lib (links seads_kernel + seads_replay), not folded into the pure wire lib. The
# bit-exact path reconciles against CANONICAL state; the lossy GEO-001/KIN wire path is bounded
# (tested in tests/property). Reconcile re-seeds via the public Kernel::add()+step() only.
add_library(seads_predict STATIC src/net/predict.cpp)
target_include_directories(seads_predict PUBLIC src/net)
target_link_libraries(seads_predict PUBLIC seads_kernel seads_replay seads_det_flags)

# --- net / server<->client SESSION loop (netcode layer 5) ---
# The layer that finally USES the wire transport between two endpoints: a server DRIVES the kernel
# and emits protocol-4 WEAPON-001 frames at 20 Hz; an in-process transport ships them with latency
# + deterministic packet loss; the client reconstructs the whole dogfight (own PREDICTED @ now via
# seads_predict, remotes INTERPOLATED via seads_net's interp, HP/kills/rounds from the decoded
# weapon wire). Composes the earlier layers — links seads_predict (+ its kernel/replay) and
# seads_net (wire codecs + interp) — so it is its OWN lib, like lockstep/predict. Reconstruction is
# det_math + IEEE + integer only, so its whole-session digest reproduces bit-for-bit cross-toolchain.
add_library(seads_session STATIC src/net/session.cpp)
target_include_directories(seads_session PUBLIC src/net)
target_link_libraries(seads_session PUBLIC seads_predict seads_net seads_kernel seads_replay seads_det_flags)

# --- client presentation lib (Step 5 renderer): recording container + playback ---
# DOWNSTREAM-ONLY. Decodes the GEO-001/KIN-001 wire (via seads_net) and interpolates remotes
# (layer 4a) for display. It NEVER feeds the sim: it links seads_net but NOT the kernel/det_math,
# lives OUTSIDE the world_hash, and is free to use libm trig (globe projection) — the determinism
# lint scopes to kernel/det_math/replay only. No seal (rides v1.4r0).
add_library(seads_client STATIC src/client/seadsrec.cpp src/client/playback.cpp)
target_include_directories(seads_client PUBLIC src/client)
target_link_libraries(seads_client PUBLIC seads_net seads_det_flags)

# --- trajectory recorder (drives the real kernel -> .seadsrec wire stream + trajectory.js) ---
add_executable(seads_record src/client/record_main.cpp)
target_link_libraries(seads_record PRIVATE seads_client seads_net seads_kernel seads_replay seads_det_math seads_det_flags)

# --- golden runner ---
add_executable(seads_golden src/kernel/golden_main.cpp)
target_link_libraries(seads_golden PRIVATE seads_kernel seads_replay seads_det_math seads_det_flags)

# --- scenario runner (scripted-timeline goldens: Turn / Climb / TurnClimb) ---
add_executable(seads_scenario src/kernel/scenario_main.cpp)
target_link_libraries(seads_scenario PRIVATE seads_kernel seads_replay seads_det_math seads_det_flags)

# --- det_math bit-exactness test ---
add_executable(seads_detmath_test src/det_math/detmath_test_main.cpp)
target_link_libraries(seads_detmath_test PRIVATE seads_det_math seads_det_flags)

# --- GEO-001 wire codec parity test (byte-exact C++ vs Python reference) ---
add_executable(seads_geo001_test src/net/geo001_test_main.cpp)
target_link_libraries(seads_geo001_test PRIVATE seads_net seads_det_flags)

# --- GEO-001 snapshot framing parity test (byte-exact C++ vs Python reference) ---
add_executable(seads_snapshot_test src/net/snapshot_test_main.cpp)
target_link_libraries(seads_snapshot_test PRIVATE seads_net seads_det_flags)

# --- WEAPON-001 snapshot-section parity test (gunnery state: hp/fire_cd + projectiles, v1.12r0) ---
add_executable(seads_weapon_test src/net/weapon_test_main.cpp)
target_link_libraries(seads_weapon_test PRIVATE seads_net seads_det_flags)

# --- loopback lockstep parity test (two kernels, same inputs, identical per-tick hash) ---
add_executable(seads_lockstep_test src/net/lockstep_test_main.cpp)
target_link_libraries(seads_lockstep_test PRIVATE seads_lockstep seads_kernel seads_replay seads_det_math seads_det_flags)

# --- remote interpolation parity test (bit-exact C++ vs Python reference) ---
add_executable(seads_interp_test src/net/interp_test_main.cpp)
target_link_libraries(seads_interp_test PRIVATE seads_net seads_det_flags)

# --- client-side prediction parity test (seamless reconcile + digest + heal control) ---
add_executable(seads_predict_test src/net/predict_test_main.cpp)
target_link_libraries(seads_predict_test PRIVATE seads_predict seads_kernel seads_replay seads_det_math seads_det_flags)

# --- server<->client session parity test (reconstructed dogfight digest + kill replication) ---
add_executable(seads_session_test src/net/session_test_main.cpp)
target_link_libraries(seads_session_test PRIVATE seads_session seads_predict seads_net seads_kernel seads_replay seads_det_math seads_det_flags)

# --- client presentation test (globe projection + .seadsrec container + playback interp) ---
add_executable(seads_client_test src/client/client_test_main.cpp)
target_link_libraries(seads_client_test PRIVATE seads_client seads_net seads_det_flags)

# --- OPTIONAL raylib viewer (Step 5). OFF by default so CI / headless gates never pull a GUI
# lib; the determinism matrix stays kernel-only. Turn on with -DSEADS_CLIENT=ON locally. The
# viewer is read-only over a .seadsrec recording — it can never affect a world_hash (no seal). ---
option(SEADS_CLIENT "Build the raylib globe viewer (pulls a graphics dep; not for CI)" OFF)
if(SEADS_CLIENT)
  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
  # --fly mode drives the real sealed kernel through the layer-4b prediction harness, so the
  # viewer now also links seads_predict (+ kernel/replay/det_math). Still read-only over the sim:
  # input feeds Commands into the Predictor, never the world_hash — no seal (rides v1.4r0).
  add_executable(seads_viewer src/client/viewer_main.cpp)
  target_link_libraries(seads_viewer PRIVATE seads_client seads_net seads_predict
                        seads_kernel seads_replay seads_det_math raylib)
endif()

enable_testing()
add_test(NAME detmath_bitexact COMMAND seads_detmath_test)
add_test(NAME geo001_byteexact COMMAND seads_geo001_test)
add_test(NAME snapshot_byteexact COMMAND seads_snapshot_test)
add_test(NAME weapon_byteexact COMMAND seads_weapon_test)
add_test(NAME lockstep_equal COMMAND seads_lockstep_test)
add_test(NAME interp_bitexact COMMAND seads_interp_test)
add_test(NAME predict_equal COMMAND seads_predict_test)
add_test(NAME session_reconstruct COMMAND seads_session_test)
add_test(NAME client_presentation COMMAND seads_client_test)
# Golden hash compare is run in CI (guardian.yml) across the toolchain matrix.
