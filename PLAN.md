# PLAN

Completed DFA implementation details are in `docs/PLAN_LOG.md`.

## Upstream refresh

- [x] Merge upstream main at 315201c3 while retaining the DFA query and Zig package identity (done 2026-09-24 17:02 EDT).
- [x] Lock nixpkgs-unstable at 34ca302a9572; pass static/shared Nix tests, all-width Zig tests and DFA checks, and 8-bit JIT tests on x86_64-linux; evaluate all three flake systems (done 2026-09-24 17:02 EDT).

## Capture history

- [x] Report the upstream interpreter/JIT divergence ((?=(a|(*ACCEPT))(?1)x)) as a PCRE2Project issue with pristine-upstream pcre2test evidence. (Peter, 2026-09-24 21:38 EDT) (done 2026-09-24 22:15 EDT, PCRE2Project/pcre2#1008)
- [ ] Fix it on a clean branch from upstream/main (no fork changes) and open a separate upstream PR. (Peter, 2026-09-24 21:38 EDT)
	- [x] Fix and tests on branch fix-accept-in-called-assertion-group (324bdf2e), pushed to the fork; RunTest green at all widths, fork fuzz shows 0 divergences. (done 2026-09-24 22:12 EDT)
	- [ ] Peter to review, then open the upstream PR from that branch referencing #1008.
	- [ ] After upstream merges, merge it here and tighten the fuzzer to fail on any interpreter/JIT divergence.
- [x] Consolidate into the pcre2 repository as branch capture-history (merged on main 84bf8c8e), push the branch only, retire the separate checkout; DNA finder moves to ~/Code/dna_repeats. (Peter, 2026-09-24 21:40 EDT) (done 2026-09-25 12:10 EDT)
	- [x] Split experiments/dna_repeats to ~/Code/dna_repeats (subtree 3b1409cb), pinned to this branch at bc340132; removed here. (done 2026-09-24 21:48 EDT)
	- [x] Push capture-history to origin as a branch only; main untouched. (done 2026-09-24 21:42 EDT, bc340132)
	- [x] Retire this checkout after the pcre2 agent confirms it has the branch; pcre2 agent fetched daf33f08 and 324bdf2e, main 84bf8c8e has no in-flight work. Checkout moved to ~/.Trash; continue in ../pcre2 on branch capture-history. (done 2026-09-25 12:10 EDT)
- [x] Count history memory against heap_limit, covering frame-driven and possessive (event-driven) growth. (done 2026-09-24 16:48 EDT)
- [x] Per-match event limit: pcre2_set/get_capture_history_limit (uint32_t, default UINT32_MAX), PCRE2_ERROR_CAPTURE_HISTORY_LIMIT (-77, provisional), enforced by interpreter and JIT; mutants of either check fail the tests. (Peter chose setter and uint32 range 2026-09-25; done 2026-09-25 23:20 EDT)
- [x] Record returned captures at subroutine/recursion return (one event per set returned group); error -77 removed. (done 2026-09-24 16:55 EDT)
- [x] JIT capture history for (*CAPTURE_HISTORY) patterns; interpreter-vs-JIT differential in CI found and fixed an interpreter duplicate-ACCEPT bug. (done 2026-09-24 17:45 EDT)
- [ ] JIT history for returned-capture recursion (currently JIT_UNSUPPORTED, interpreter runs it).
- [ ] Consider a PCRE2_JIT_CAPTURE_HISTORY jit-compile option so option-only history can use the JIT.
- [ ] Turn tests/benchmark/capture_history_bench.c into ./bm with an ndjson log and baseline-vs-current comparison.
- [x] Add JIT-enabled Nix check (sljit pinned via fetchFromGitHub) to ./test and Mechatron targets. (done 2026-09-24 17:44 EDT)
- [ ] Add a mechanical check that the fork option bit and internal flag bit, error -77 and the fork API names do not collide with upstream before rebase/release.
- [ ] Benchmark disabled/enabled capture history at N,2N,4N,8N (gap-pattern and finder benchmarks belong to ~/Code/dna_repeats).

## DFA compatibility query

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
