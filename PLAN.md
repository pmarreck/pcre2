# PLAN

## Capture history and fixed-length repeat exploration

- [ ] Consolidate with ../pcre2 (its main merged the same upstream 315201c3 as 84bf8c8e, pushed); awaiting Peter's choice of target, DNA-experiment home and push scope. (Peter, 2026-09-24 21:32 EDT)
- [x] Merge upstream PCRE2 main 315201c3 (129 commits incl. 10.48 and bulk reformat), sljit 3908d4c1, nixpkgs nixos-unstable 4975466d; ./test green. (done 2026-09-24 16:50 EDT)
- [x] Count history memory against heap_limit, covering frame-driven and possessive (event-driven) growth. (done 2026-09-24 16:48 EDT)
- [ ] Peter to decide whether to add an explicit per-match event maximum (see reply 2026-09-24).
- [x] Record returned captures at subroutine/recursion return (one event per set returned group); error -77 removed. (done 2026-09-24 16:55 EDT)
- [x] Build the chain_packing finder and dna-repeats CLI; exhaustive differential vs oracle; corpus: 399 families, L 8..20, 0.83 s with LNRS bound. (done 2026-09-24 17:02 EDT)
- [x] JIT capture history for (*CAPTURE_HISTORY) patterns; interpreter-vs-JIT differential in CI found and fixed an interpreter duplicate-ACCEPT bug. (done 2026-09-24 17:45 EDT)
- [ ] JIT history for returned-capture recursion (currently JIT_UNSUPPORTED, interpreter runs it).
- [ ] Consider a PCRE2_JIT_CAPTURE_HISTORY jit-compile option so option-only history can use the JIT.
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [x] Peter: history bytes count against heap_limit; consider a maximum. (done 2026-09-24 16:37 EDT)
- [x] Add JIT-enabled Nix check (sljit pinned via fetchFromGitHub) to ./test and Mechatron targets. (done 2026-09-24 17:44 EDT)
- [x] Peter: record returned-capture recursion; fork numbers documented as provisional pending upstream. (done 2026-09-24 16:37 EDT)
- [ ] Add a mechanical check that the fork option bit and internal flag bit do not collide with upstream before rebase/release.
- [x] Peter chose family rule A, chain_packing. (done 2026-09-24 16:37 EDT; context: docs/capture_history/OVERLAP_SEMANTICS.md)
- [x] Implement strict normalization, tested over all 256 byte values; real corpus gives 2,900 bases identical to an independent tr pipeline. (done 2026-09-24 14:28 EDT)
- [ ] Add maximality per family and optional Pareto labels to the finder output.
- [ ] Benchmark disabled/enabled capture history and both gap patterns separately for compile/match/aggregation/memory at N,2N,4N,8N; defer JIT until semantics are proven.

## Inherited DFA-query work

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
