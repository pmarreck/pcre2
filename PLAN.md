# PLAN

Completed DFA implementation details are in `docs/PLAN_LOG.md`.

## Upstream refresh

- [x] Merge upstream main at 315201c3 while retaining the DFA query and Zig package identity (done 2026-09-24 17:02 EDT).
- [x] Lock nixpkgs-unstable at 34ca302a9572; pass static/shared Nix tests, all-width Zig tests and DFA checks, and 8-bit JIT tests on x86_64-linux; evaluate all three flake systems (done 2026-09-24 17:02 EDT).

## Capture history

- [ ] Report the upstream interpreter/JIT divergence ((?=(a|(*ACCEPT))(?1)x)) as a PCRE2Project issue with pristine-upstream pcre2test evidence. (Peter, 2026-09-24 21:38 EDT)
- [ ] Fix it on a clean branch from upstream/main (no fork changes) and open a separate upstream PR. (Peter, 2026-09-24 21:38 EDT)
- [ ] Consolidate into the pcre2 repository as branch capture-history (merged on main 84bf8c8e), push the branch only, retire the separate checkout; DNA finder moves to ~/Code/dna_repeats. (Peter, 2026-09-24 21:40 EDT)
	- [x] Split experiments/dna_repeats to ~/Code/dna_repeats (subtree 3b1409cb), pinned to this branch at bc340132; removed here. (done 2026-09-24 21:48 EDT)
	- [x] Push capture-history to origin as a branch only; main untouched. (done 2026-09-24 21:42 EDT, bc340132)
	- [ ] Retire this checkout after the pcre2 agent confirms it has the branch (fetch origin in ../pcre2 is theirs to run).
- [x] Count history memory against heap_limit, covering frame-driven and possessive (event-driven) growth. (done 2026-09-24 16:48 EDT)
- [ ] Peter to decide whether to add an explicit per-match event maximum (see reply 2026-09-24).
- [x] Record returned captures at subroutine/recursion return (one event per set returned group); error -77 removed. (done 2026-09-24 16:55 EDT)
- [x] JIT capture history for (*CAPTURE_HISTORY) patterns; interpreter-vs-JIT differential in CI found and fixed an interpreter duplicate-ACCEPT bug. (done 2026-09-24 17:45 EDT)
- [ ] JIT history for returned-capture recursion (currently JIT_UNSUPPORTED, interpreter runs it).
- [ ] Consider a PCRE2_JIT_CAPTURE_HISTORY jit-compile option so option-only history can use the JIT.
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [x] Peter: history bytes count against heap_limit; consider a maximum. (done 2026-09-24 16:37 EDT)
- [x] Add JIT-enabled Nix check (sljit pinned via fetchFromGitHub) to ./test and Mechatron targets. (done 2026-09-24 17:44 EDT)
- [x] Peter: record returned-capture recursion; fork numbers documented as provisional pending upstream. (done 2026-09-24 16:37 EDT)
- [ ] Add a mechanical check that the fork option bit and internal flag bit do not collide with upstream before rebase/release.
- [ ] Benchmark disabled/enabled capture history at N,2N,4N,8N (gap-pattern and finder benchmarks belong to ~/Code/dna_repeats).

## DFA compatibility query

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
