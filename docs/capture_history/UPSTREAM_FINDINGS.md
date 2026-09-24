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

Not reported upstream yet; Peter decides whether to file it.
