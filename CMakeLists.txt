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

# --- golden runner ---
add_executable(seads_golden src/kernel/golden_main.cpp)
target_link_libraries(seads_golden PRIVATE seads_kernel seads_replay seads_det_math seads_det_flags)

# --- det_math bit-exactness test ---
add_executable(seads_detmath_test src/det_math/detmath_test_main.cpp)
target_link_libraries(seads_detmath_test PRIVATE seads_det_math seads_det_flags)

enable_testing()
add_test(NAME detmath_bitexact COMMAND seads_detmath_test)
# Golden hash compare is run in CI (guardian.yml) across the toolchain matrix.
