# PLAN

## Capture history and fixed-length repeat exploration

- [x] Add a failing repeated-capture history C API test, then minimal opt-in interpreter event storage/getters using configured allocators; preserve ordinary ovector behavior. (done 2026-09-24 13:45 EDT)
- [x] Add rollback, alternation, positive/negative assertion, atomic/possessive, match-data reuse, failure and allocator tests at 8/16/32 widths; 18 mutants all killed. (done 2026-09-24 13:45 EDT)
- [x] Add heap/match/depth-limit tests with history enabled: each returns its limit error with zero events. (done 2026-09-24 14:02 EDT)
- [x] Measure disabled/enabled history overhead once: disabled within noise, enabled about 1.14x on the bench workload. (done 2026-09-24 14:10 EDT; context: CAPTURE_HISTORY.md)
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [ ] Decide whether history bytes count against heap_limit. (context: CAPTURE_HISTORY.md)
- [x] Define unsupported DFA/partial/recursion behavior explicitly: BADOPTION for DFA and partial; returned-capture recursion fails with PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED. (done 2026-09-24 13:45 EDT)
- [x] Test JIT fallback and pcre2_jit_match rejection in a local JIT-enabled build at 8/16/32. (done 2026-09-24 14:01 EDT)
- [ ] Add a JIT-enabled Nix check so JIT history tests run in CI (needs sljit submodule in the flake source).
- [ ] Get Peter's ruling on returned-capture recursion (reject vs record) and the fork option bit/error code allocation.
- [x] Add (*CAPTURE_HISTORY) pattern-start verb; DFA, partial and direct JIT refuse it; tested in JIT and non-JIT builds. (done 2026-09-24 14:27 EDT)
- [ ] Add a mechanical check that the fork option bit, error code and internal flag bit do not collide with upstream before rebase/release.
- [x] Expose Zig borrowed offset events through the existing fork build, with ABI/lifetime checks and no subject copies; Nix zig check runs 8/16/32. (done 2026-09-24 14:08 EDT)
- [x] Build exhaustive small counterexamples and oracle for the overlap ambiguity; chain model matches PCRE2 history by differential test. (done 2026-09-24 14:24 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [ ] Get Peter's family-rule choice (A chain_packing, B maximal_chains, C all_occurrences) before building the finder. (context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Implement strict normalization, tested over all 256 byte values; real corpus gives 2,900 bases identical to an independent tr pipeline. (done 2026-09-24 14:28 EDT)
- [ ] Implement fixed-length regex finder and maximality against the oracle once the family rule is chosen; retain all maximal families and optional Pareto labels.
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
