# DeterminismFlags.cmake — per-compiler strict-FP flags that protect the SEADS
# bit-for-bit promise. Applied via the INTERFACE target `seads_det_flags`.
#
# Rules (see CLAUDE.md §5):
#   * No FMA contraction (diverges x64 vs AArch64).
#   * No fast-math / no value-changing reassociation.
#   * Standard FP rounding/precision; no x87 excess precision.
# NEVER add /fp:fast, -ffast-math, -Ofast, or -mfma.

add_library(seads_det_flags INTERFACE)

if(MSVC)
  target_compile_options(seads_det_flags INTERFACE
    /fp:strict        # strict IEEE; disables contraction
    /fp:except-       # no FP exception semantics (perf; determinism unaffected)
    /Zc:__cplusplus
  )
else()
  target_compile_options(seads_det_flags INTERFACE
    -fno-fast-math
    -ffp-contract=off   # kill FMA contraction (critical for x64 vs AArch64)
    -fno-math-errno     # let det_sqrt lower to the hardware sqrt instruction
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(seads_det_flags INTERFACE
      -frounding-math
      -fexcess-precision=standard
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(seads_det_flags INTERFACE
      -ffp-model=strict
    )
  endif()
endif()
