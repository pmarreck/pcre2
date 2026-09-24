# PLAN

## Capture history and fixed-length repeat exploration

- [x] Create isolated checkout from Peter's Zig-enabled fork f10b7bef; preserve original checkout and imported UTF-8 handoff. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [x] Run unchanged Linux x86_64 Nix interpreter/upstream baseline: all 6 CTest tests pass, including 8/16/32-bit compatibility checks; no shared-library/JIT/macOS claim. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [x] Map capture writes, interpreter-frame restore boundaries, match-data ownership and available option bits before changing layouts. (done 2026-09-24 13:29 EDT; frame-local history_top; context: CAPTURE_HISTORY.md)
- [x] Add a failing repeated-capture history C API test, then minimal opt-in interpreter event storage/getters using configured allocators; preserve ordinary ovector behavior. (done 2026-09-24 13:45 EDT)
- [x] Add rollback, alternation, positive/negative assertion, atomic/possessive, match-data reuse, failure and allocator tests at 8/16/32 widths; 18 mutants all killed. (done 2026-09-24 13:45 EDT)
- [ ] Add heap/match/depth-limit tests with history enabled; decide whether history bytes count against heap_limit. (context: CAPTURE_HISTORY.md)
- [x] Define unsupported DFA/partial/recursion behavior explicitly: BADOPTION for DFA and partial; returned-capture recursion fails with PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED. (done 2026-09-24 13:45 EDT)
- [ ] Test JIT fallback and pcre2_jit_match rejection in a JIT-enabled build (implemented, untested).
- [ ] Get Peter's ruling on returned-capture recursion (reject vs record) and the fork option bit/error code allocation.
- [ ] Add (*CAPTURE_HISTORY) pattern-start verb.
- [ ] Add a mechanical check that the fork option bit and error code do not collide with upstream pcre2.h before rebase/release.
- [ ] Expose Zig borrowed offset events through the existing fork build, with ABI/lifetime checks and no subject copies.
- [ ] Resolve overlapping-position aggregation ambiguity with exhaustive small counterexamples before implementing the DNA family oracle or optimized search. (context: docs/capture_history/REVIEW.md)
- [ ] Implement strict normalization, fixed-length regex finder and separate brute-force Zig oracle; retain all maximal families and optional Pareto labels.
- [x] Receive actual corpus at $HOME/Documents/dna_sample.txt: 2,901 bytes, 2,900 ACGT bases after ASCII-whitespace/hyphen normalization. (done 2026-09-24 13:27 EDT; provenance: docs/capture_history/REVIEW.md)
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
