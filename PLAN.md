# PLAN

## Capture history and fixed-length repeat exploration

- [ ] Merge current upstream PCRE2 into capture-history (and its sljit pin) and update flake.lock to current nixpkgs; keep ./test green. (Peter, 2026-09-24)
- [ ] Count history memory against heap_limit and decide on an explicit event maximum, following PCRE2 precedent. (Peter, 2026-09-24)
- [ ] Make recursion/subroutines that return captures record history instead of failing with -77. (Peter, 2026-09-24)
- [ ] Build the fixed-length finder with the chain_packing family rule, checked against the oracle. (Peter, 2026-09-24)
- [ ] Implement capture history in the JIT, matching interpreter event histories exactly. (Peter, 2026-09-24)
- [x] Add heap/match/depth-limit tests with history enabled: each returns its limit error with zero events. (done 2026-09-24 14:02 EDT)
- [x] Measure disabled/enabled history overhead once: disabled within noise, enabled about 1.14x on the bench workload. (done 2026-09-24 14:10 EDT; context: CAPTURE_HISTORY.md)
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [x] Peter: history bytes count against heap_limit; consider a maximum. (done 2026-09-24 16:37 EDT)
- [x] Test JIT fallback and pcre2_jit_match rejection in a local JIT-enabled build at 8/16/32. (done 2026-09-24 14:01 EDT)
- [ ] Add a JIT-enabled Nix check so JIT history tests run in CI (needs sljit submodule in the flake source).
- [x] Peter: record returned-capture recursion; fork numbers documented as provisional pending upstream. (done 2026-09-24 16:37 EDT)
- [x] Add (*CAPTURE_HISTORY) pattern-start verb; DFA, partial and direct JIT refuse it; tested in JIT and non-JIT builds. (done 2026-09-24 14:27 EDT)
- [ ] Add a mechanical check that the fork option bit, error code and internal flag bit do not collide with upstream before rebase/release.
- [x] Expose Zig borrowed offset events through the existing fork build, with ABI/lifetime checks and no subject copies; Nix zig check runs 8/16/32. (done 2026-09-24 14:08 EDT)
- [x] Build exhaustive small counterexamples and oracle for the overlap ambiguity; chain model matches PCRE2 history by differential test. (done 2026-09-24 14:24 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Peter chose family rule A, chain_packing. (done 2026-09-24 16:37 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Implement strict normalization, tested over all 256 byte values; real corpus gives 2,900 bases identical to an independent tr pipeline. (done 2026-09-24 14:28 EDT)
- [ ] Add maximality per family and optional Pareto labels to the finder output.
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
