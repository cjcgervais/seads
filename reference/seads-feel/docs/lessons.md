
- A mutation revert via `mv file.orig file` PRESERVES the orig's OLD mtime — ninja
  sees nothing to do, the object file still holds the MUTANT, and the next "clean"
  gate/harness run certifies (or fails) the mutant, not HEAD. The inverse of the
  stale-binary trap: here the SOURCE is honest and the BINARY lies. After ANY
  mutation revert: `touch` the file, rebuild, and require the compile line (not
  just the link) in the build output. Caught live: a 362/362 gate turned 2-fail
  because the post-restore rebuild printed "[1/1] Linking" — link-only = the
  mutant object shipped into both seads_tests.exe AND seads_harness.exe, so even
  the measured AFTER grid was contaminated until rebuilt. (v5 S-truedepth)
