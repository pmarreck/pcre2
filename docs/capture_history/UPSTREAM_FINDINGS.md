# Upstream PCRE2 findings

## Interpreter and JIT disagree after (*ACCEPT) and a subroutine call in an assertion

Found 2026-09-24 by `tests/capture_history_jit_diff.c`. Reproduced on pristine
upstream `main` at 315201c3 (x86_64 Linux, 8-bit, JIT on), without capture
history. It is independent of this fork's changes.

```text
pattern  (?=(a|(*ACCEPT))(?1)x)     subject  a
interpreter  rc=2  ovector 0 0 0 1
JIT          rc=2  ovector 0 0 0 0

pattern  (?=(()|(*ACCEPT))(?1)x)    subject  a
interpreter  rc=3  ovector 0 0 0 0 0 0
JIT          rc=2  ovector 0 0 0 0
```

In the first case the successful path takes the `(*ACCEPT)` alternative of
group 1 at offset 0, so `[0,0]` (the JIT's answer) is expected. The
interpreter reports `[0,1]` from the abandoned first alternative. The second
case reports group 2, which the successful path never set. The reduction came
from a greedy delta reducer over a fuzzer-generated pattern (kept locally, not
in the repository).

The differential now runs each pattern on both engines without history as
well, skips cases where those already disagree (counted as upstream
divergences), and requires that enabling history changes neither engine's rc
or ovector.

Reported upstream on 2026-09-24 as
[PCRE2Project/pcre2#1008](https://github.com/PCRE2Project/pcre2/issues/1008).

Cause: the compiler emits `OP_ASSERT_ACCEPT` for an `(*ACCEPT)` lexically
inside an assertion, and the interpreter always returns `MATCH_ACCEPT` for it,
even when the group was called as a subroutine. That passes the recursion
frame and ends the nearest assertion. With no enclosing assertion,
`/(?=(b|(*ACCEPT)))(?1)x/` on `ax` returns the internal code -999 from
`pcre2_match()`.

The fix is on branch `fix-accept-in-called-assertion-group` in
`github.com/pmarreck/pcre2` (commit 324bdf2e on upstream 315201c3, no fork
changes). Assertion frames get their own `GF_ASSERT` type, and
`OP_ASSERT_ACCEPT` returns from the recursion when the recursion is the
innermost construct. Upstream RunTest passes at 8/16/32 bits with and without
JIT. Applied to this branch, `./fuzz 50000` reports 0 upstream divergences
(387,387 comparisons per width, previously 387,333 plus skipped cases). The
upstream PR is not opened yet; Peter decides.
