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
add_library(seads_net STATIC src/net/geo001.cpp src/net/snapshot.cpp)
target_include_directories(seads_net PUBLIC src/net)
target_link_libraries(seads_net PUBLIC seads_det_flags)

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

enable_testing()
add_test(NAME detmath_bitexact COMMAND seads_detmath_test)
add_test(NAME geo001_byteexact COMMAND seads_geo001_test)
add_test(NAME snapshot_byteexact COMMAND seads_snapshot_test)
# Golden hash compare is run in CI (guardian.yml) across the toolchain matrix.
