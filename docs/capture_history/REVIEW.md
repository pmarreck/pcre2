# Capture-history setup and review

## Provenance

Peter requested this project on September 23, 2026. Retrieved the original
handoff byte-for-byte from Tiki's Windows Downloads directory; SHA-256:
`9dc4a1ae4c5636765dd4b4bd88d9f23e1f1dce70224cc061762cfd9f6532f1ac`.
The preserved handoff, not an edited summary, is adjacent to this file.

Checkout: `$HOME/Code/pcre2_capture_history`, branch `capture-history`.
Derived with `git clone --no-hardlinks` from `$HOME/Code/pcre2` at `f10b7bef`;
origin remains `https://github.com/pmarreck/pcre2`. No new remote or push.
The original working copy is untouched, including its untracked inbox.
Existing Zig build, package manifest, CMake, Nix flake, license and history are
retained. Do not overwrite these with a generic greenfield Zig template.

Source headers identify PCRE2 10.48-DEV; the inherited Zig package manifest
says 10.47.0. Record both rather than assuming they are synchronized.
Local additions include Zig 0.16 build support (873ecf64), compiled-pattern
DFA compatibility checks (894ef765), and CI build/check wiring (f10b7bef).

## Verified baseline

Ran `nix build $HOME/Code/pcre2#checks.x86_64-linux.test --no-link` before
changing engine source. It succeeded. Its CTest run reports 6/6 tests passing:
the upstream suite, grep, POSIX, and DFA compatibility at 8/16/32-bit widths.
The existing flake enables 8/16/32-bit libraries and disables JIT.
This does not certify native Windows, ARM Linux, macOS, shared-library tests,
history behavior, or performance. Capture history is not implemented yet.

Baseline derivation:
`/nix/store/c3m0ib44952bgk2nyji9qliyrn9k8hbk-pcre2-dfa-check-10.48-dev.drv`.
Use `nix log` on that exact derivation for evidence.

## Review before coding

1. The imported handoff retains `INSERT_OVERLONG_DNA_STRAND_HERE` verbatim.
   Peter supplied `$HOME/Documents/dna_sample.txt` on September 24: 2,901 bytes,
   2,900 bases after removing ASCII whitespace/hyphens and uppercasing, with no
   other characters. SHA-256 of the original bytes:
   `b6c469693af361128569e065604f26a53ebd0f75f22678377cbfbad763b22c5a`.
   Read this file without modifying it; keep it local pending publication consent.
   This validates input shape only, not any expected repeat counts.
2. Non-overlapping matches and a union of all offset chains are not automatically
   compatible. Example: `AAAAAA`, L=2, D=0 produces greedy chains [0,2,4]
   and [1,3]. Their union [0,1,2,3,4] has negative gaps, contradicting the
   specified 0 <= gap <= D. Splitting only on gap>D does not repair that.
   Preserve this as an unresolved semantic choice: independent greedy chains,
   canonical non-overlapping packing, or another explicitly accepted family
   rule. Do not label a union containing overlaps as a valid non-overlap family.
   The engine feature can proceed independently of this consumer decision.
3. Boundary sentinels and maximality are evaluated per entire selected family,
   not every subset. Keep the handoff's 17x8 versus 20x5 example as a required
   distinction. A maximum-length-only search would lose intended results.
4. Callout traces can include speculative paths. A list of callout observations
   is not by itself an oracle for final committed capture history.
5. Begin with interpreter semantics and unchanged ovector controls. Inspect
   `src/pcre2_match.c` frame/ovector copying and restore logic, not just the
   locations assigning capture offsets. Event order is capture-close order,
   which may differ from increasing subject offsets for nested groups.
6. Borrowed event pointers must have explicit lifetime, reset/failure semantics
   and allocator ownership. Match-data reuse must never expose stale success.
   Put bounds on speculative event growth; report limit/allocation failure
   distinctly from no-match. Disabled-history overhead needs measurement.
7. Reject or force an explicitly documented interpreter fallback for unsupported
   execution modes. Partial matches and recursive/subroutine paths cannot
   silently produce incomplete history advertised as complete.

## Sources and next implementation boundary

The handoff's current final-capture limitation is consistent with the
[PCRE2 native API](https://pcre.org/current/doc/html/pcre2api.html).
The separate [callout API](https://pcre.org/current/doc/html/pcre2callout.html)
exposes intermediate matcher state; it is a comparator, not the desired
callback-free result API. Consult the fork's own source and bundled docs when
choosing exact option bits, public symbols, error codes and width suffixes.

First implementation gate: a failing C test for `(a)+` on `aaa` requesting
history, followed by `(a+)(a)` rollback and assertion cases. Preserve upstream
test expectations and include the new suite in the existing build/test entry
points. Read the shared Zig and build-architecture guidance before adding the
consumer; keep native build outputs separated by target/profile.
