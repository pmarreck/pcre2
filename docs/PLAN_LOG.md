# PLAN log

Completed PLAN.md items retired by plan-retire, oldest retirement first.

## Retired 2026-09-23

- [x] [Inherited DFA-query work] Add a failing C API test for a subject-independent compatibility query. Include unsupported features hidden behind literals and alternatives, plus supported controls so rejecting everything cannot pass.
- [x] [Inherited DFA-query work] Reuse the compiled-instruction walk used by callout enumeration. Check variable-length instructions, Unicode, and conditional operands.
- [x] [Inherited DFA-query work] Reject unsupported compiled instructions and incompatible options. A newly introduced opcode must not silently become accepted.
- [x] [Inherited DFA-query work] Test 8/16/32-bit builds and the upstream suite; document the query's conservative scope and separation from resource errors. Completed 2026-09-15 16:04 EDT. Static and shared CMake suites pass on Linux x86_64, as does the optimized Nix build. All three flake systems evaluate; this does not establish native macOS or Linux ARM64 execution. The first API test failed with BADOPTION before implementation. Execution checks then exposed incorrectly accepted variable-length lookbehind; the corrected checker rejects it and conservatively declines all calls.

## Retired 2026-09-24

- [x] [Capture history and fixed-length repeat exploration] Create isolated checkout from Peter's Zig-enabled fork f10b7bef; preserve original checkout and imported UTF-8 handoff. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
