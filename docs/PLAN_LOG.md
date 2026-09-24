# PLAN log

Completed PLAN.md items retired by plan-retire, oldest retirement first.

## Retired 2026-09-23

- [x] [Inherited DFA-query work] Add a failing C API test for a subject-independent compatibility query. Include unsupported features hidden behind literals and alternatives, plus supported controls so rejecting everything cannot pass.
- [x] [Inherited DFA-query work] Reuse the compiled-instruction walk used by callout enumeration. Check variable-length instructions, Unicode, and conditional operands.
- [x] [Inherited DFA-query work] Reject unsupported compiled instructions and incompatible options. A newly introduced opcode must not silently become accepted.
- [x] [Inherited DFA-query work] Test 8/16/32-bit builds and the upstream suite; document the query's conservative scope and separation from resource errors. Completed 2026-09-15 16:04 EDT. Static and shared CMake suites pass on Linux x86_64, as does the optimized Nix build. All three flake systems evaluate; this does not establish native macOS or Linux ARM64 execution. The first API test failed with BADOPTION before implementation. Execution checks then exposed incorrectly accepted variable-length lookbehind; the corrected checker rejects it and conservatively declines all calls.

## Retired 2026-09-24

- [x] [Capture history and fixed-length repeat exploration] Create isolated checkout from Peter's Zig-enabled fork f10b7bef; preserve original checkout and imported UTF-8 handoff. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [x] [Capture history and fixed-length repeat exploration] Run unchanged Linux x86_64 Nix interpreter/upstream baseline: all 6 CTest tests pass, including 8/16/32-bit compatibility checks; no shared-library/JIT/macOS claim. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [x] [Capture history and fixed-length repeat exploration] Receive actual corpus at $HOME/Documents/dna_sample.txt: 2,901 bytes, 2,900 ACGT bases after ASCII-whitespace/hyphen normalization. (done 2026-09-24 13:27 EDT; provenance: docs/capture_history/REVIEW.md)
- [x] [Capture history and fixed-length repeat exploration] Map capture writes, interpreter-frame restore boundaries, match-data ownership and available option bits before changing layouts. (done 2026-09-24 13:29 EDT; frame-local history_top; context: CAPTURE_HISTORY.md)
- [x] [Capture history and fixed-length repeat exploration] Add a failing repeated-capture history C API test, then minimal opt-in interpreter event storage/getters using configured allocators; preserve ordinary ovector behavior. (done 2026-09-24 13:45 EDT)
- [x] [Capture history and fixed-length repeat exploration] Add rollback, alternation, positive/negative assertion, atomic/possessive, match-data reuse, failure and allocator tests at 8/16/32 widths; 18 mutants all killed. (done 2026-09-24 13:45 EDT)
- [x] [Capture history and fixed-length repeat exploration] Define unsupported DFA/partial/recursion behavior explicitly: BADOPTION for DFA and partial; returned-capture recursion fails with PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED. (done 2026-09-24 13:45 EDT)
- [x] [Capture history and fixed-length repeat exploration] Add heap/match/depth-limit tests with history enabled: each returns its limit error with zero events. (done 2026-09-24 14:02 EDT)
- [x] [Capture history and fixed-length repeat exploration] Test JIT fallback and pcre2_jit_match rejection in a local JIT-enabled build at 8/16/32. (done 2026-09-24 14:01 EDT)
- [x] [Capture history and fixed-length repeat exploration] Expose Zig borrowed offset events through the existing fork build, with ABI/lifetime checks and no subject copies; Nix zig check runs 8/16/32. (done 2026-09-24 14:08 EDT)
