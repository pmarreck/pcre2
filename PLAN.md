# PLAN

## Capture history and fixed-length repeat exploration

- [x] Merge upstream PCRE2 main 315201c3 (129 commits incl. 10.48 and bulk reformat), sljit 3908d4c1, nixpkgs nixos-unstable 4975466d; ./test green. (done 2026-09-24 16:50 EDT)
- [x] Count history memory against heap_limit, covering frame-driven and possessive (event-driven) growth. (done 2026-09-24 16:48 EDT)
- [ ] Peter to decide whether to add an explicit per-match event maximum (see reply 2026-09-24).
- [x] Record returned captures at subroutine/recursion return (one event per set returned group); error -77 removed. (done 2026-09-24 16:55 EDT)
- [ ] Build the fixed-length finder with the chain_packing family rule, checked against the oracle. (Peter, 2026-09-24)
- [ ] Implement capture history in the JIT, matching interpreter event histories exactly. (Peter, 2026-09-24)
- [x] Measure disabled/enabled history overhead once: disabled within noise, enabled about 1.14x on the bench workload. (done 2026-09-24 14:10 EDT; context: CAPTURE_HISTORY.md)
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [x] Peter: history bytes count against heap_limit; consider a maximum. (done 2026-09-24 16:37 EDT)
- [ ] Add a JIT-enabled Nix check so JIT history tests run in CI (needs sljit submodule in the flake source).
- [x] Peter: record returned-capture recursion; fork numbers documented as provisional pending upstream. (done 2026-09-24 16:37 EDT)
- [x] Add (*CAPTURE_HISTORY) pattern-start verb; DFA, partial and direct JIT refuse it; tested in JIT and non-JIT builds. (done 2026-09-24 14:27 EDT)
- [ ] Add a mechanical check that the fork option bit and internal flag bit do not collide with upstream before rebase/release.
- [x] Build exhaustive small counterexamples and oracle for the overlap ambiguity; chain model matches PCRE2 history by differential test. (done 2026-09-24 14:24 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Peter chose family rule A, chain_packing. (done 2026-09-24 16:37 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Implement strict normalization, tested over all 256 byte values; real corpus gives 2,900 bases identical to an independent tr pipeline. (done 2026-09-24 14:28 EDT)
- [ ] Add maximality per family and optional Pareto labels to the finder output.
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
