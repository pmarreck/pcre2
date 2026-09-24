# PLAN

## Capture history and fixed-length repeat exploration

- [x] Create isolated checkout from Peter's Zig-enabled fork f10b7bef; preserve original checkout and imported UTF-8 handoff. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [x] Run unchanged Linux x86_64 Nix interpreter/upstream baseline: all 6 CTest tests pass, including 8/16/32-bit compatibility checks; no shared-library/JIT/macOS claim. (done 2026-09-23 22:13 EDT; baseline f10b7bef)
- [ ] Map capture writes, interpreter-frame restore boundaries, match-data ownership and available option bits before changing layouts. (context: docs/capture_history/REVIEW.md)
- [ ] Add a failing repeated-capture history C API test, then minimal opt-in interpreter event storage/getters using configured allocators; preserve ordinary ovector behavior.
- [ ] Add rollback, alternation, positive/negative assertion, atomic/possessive, match-data reuse, failure and resource-limit tests; test 8/16/32 widths and disabled-history allocations.
- [ ] Define unsupported JIT/DFA/partial/recursion behavior explicitly; add pattern-start verb only after API semantics pass.
- [ ] Expose Zig borrowed offset events through the existing fork build, with ABI/lifetime checks and no subject copies.
- [ ] Resolve overlapping-position aggregation ambiguity with exhaustive small counterexamples before implementing the DNA family oracle or optimized search. (context: docs/capture_history/REVIEW.md)
- [ ] Implement strict normalization, fixed-length regex finder and separate brute-force Zig oracle; retain all maximal families and optional Pareto labels.
- [x] Receive actual corpus at $HOME/Documents/dna_sample.txt: 2,901 bytes, 2,900 ACGT bases after ASCII-whitespace/hyphen normalization. (done 2026-09-24 13:27 EDT; provenance: docs/capture_history/REVIEW.md)
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
