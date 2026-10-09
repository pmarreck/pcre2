# Checking patterns before saving them

This fork adds `PCRE2_INFO_DFA_COMPATIBILITY` to `pcre2_pattern_info()`.
It checks the compiled instruction stream without executing a match or
calling user callouts. The query works for 8-, 16-, and 32-bit code units.

```c
int compatibility;
int result = pcre2_pattern_info(code, PCRE2_INFO_DFA_COMPATIBILITY,
    &compatibility);
/* Accept only when result == 0 && compatibility == 0. */
```

The function's return value reports query errors, such as a null pattern.
The output is an `int` with one of these values:

| Value | Meaning |
| --- | --- |
| `0` | Passed the conservative DFA compatibility check |
| `PCRE2_ERROR_DFA_UITEM` | Unsupported compiled instruction, including variable-length lookbehind |
| `PCRE2_ERROR_DFA_UCOND` | Unsupported capture-dependent or specific-recursion condition |
| `PCRE2_ERROR_DFA_UINVALID_UTF` | Compiled with `PCRE2_MATCH_INVALID_UTF` |
| `PCRE2_ERROR_DFA_RECURSE` | Contains recursion or a subroutine call, which this check does not certify |

Every compiled branch is inspected, even an unreachable branch or an unused
definition. The check intentionally declines recursion and subroutine calls,
including calls that happen to be nonrecursive. PCRE2 supports some of these,
but execution can encounter recursion-loop errors or internal vector limits.
This conservative policy suits applications that validate user patterns before
saving them. Existing DFA and standard matching APIs are unchanged.

Unicode properties, ordinary grouping and alternation, scoped case options,
fixed-length lookbehind, supported lookahead, and callout records are accepted.
Callout text and class payloads are skipped using PCRE2's existing instruction
walker. Text that looks like an unsupported feature inside a literal or callout
does not make that feature part of the pattern.

The result applies to this build of PCRE2. Revalidate saved pattern text after
changing engine versions or compilation options. An instruction-count guard
forces review when upstream adds opcodes; unknown opcodes are rejected.

This is not a proof of termination or a promise of infallible execution.
Callers must still supply valid input/options, handle resource limits and
allocation failures, and handle errors returned by any installed callouts.
Errors must not be converted into ordinary nonmatches.

## Validation

`./test` runs the upstream matcher, grep, and POSIX suites plus the compatibility
tests for all three widths, with both static and shared libraries. `./build`
builds the optimized static libraries and command-line tools through Nix.
The existing Zig package build remains available to downstream projects.

The initial regression ran before the query was implemented and failed with
`PCRE2_ERROR_BADOPTION`. Subsequent execution checks found that variable-length
lookbehind had been incorrectly accepted by the first checker; it is now
rejected. Tests distinguish `\C` in UTF-32, where PCRE2 compiles it to a
supported instruction, from its unsupported UTF-8/16 form.
