# PLAN

Completed DFA implementation details are in `docs/PLAN_LOG.md`.

## Upstream refresh

- [x] Refresh SLJIT to 39c508d1 and nixpkgs-unstable to 39ad350a; pass static/shared suites and all-width JIT/DFA checks with Zig 0.16 (done 2026-10-08 20:51 EDT).
- [x] Merge upstream main at 315201c3 while retaining the DFA query and Zig package identity (done 2026-09-24 17:02 EDT).
- [x] Lock nixpkgs-unstable at 34ca302a9572; pass static/shared Nix tests, all-width Zig tests and DFA checks, and 8-bit JIT tests on x86_64-linux; evaluate all three flake systems (done 2026-09-24 17:02 EDT).

## DFA compatibility query

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
