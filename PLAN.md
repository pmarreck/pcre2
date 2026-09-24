# PLAN

Completed DFA implementation details are in `docs/PLAN_LOG.md`.

## Upstream refresh

- [x] Merge upstream main at 315201c3 while retaining the DFA query and Zig package identity (done 2026-09-24 17:02 EDT).
- [x] Lock nixpkgs-unstable at 34ca302a9572; pass static/shared Nix tests, all-width Zig tests and DFA checks, and 8-bit JIT tests on x86_64-linux; evaluate all three flake systems (done 2026-09-24 17:02 EDT).

## DFA compatibility query

- [ ] Connect the query to `globlike` after that library's grammar is agreed.
