# Capture history (fork extension)

PCRE2's ovector reports only the final iteration of a repeated capture group.
This fork can also return every capture that closed on the successful match
path, in the order the groups closed. It is off by default.

```c
int rc = pcre2_match(code, subject, length, 0, PCRE2_CAPTURE_HISTORY,
    match_data, NULL);
if (rc >= 0) {
	PCRE2_SIZE n = pcre2_get_capture_event_count(match_data);
	const pcre2_capture_event *ev = pcre2_get_capture_event_pointer(match_data);
	/* ev[i].group, ev[i].start, ev[i].end: code-unit offsets into subject */
}
```

`(a)+` against `aaa` yields `{1,0,1} {1,1,2} {1,2,3}`. The ovector still
reports group 1 as `[2,3)`. `pcre2_capture_event` has the same layout for the
8-, 16- and 32-bit libraries; the getters carry the usual width suffix.

## Semantics

- Only captures on the successful path appear. Captures made by failed
  alternatives, abandoned loop iterations, speculative greedy attempts and
  the inner paths of negative assertions are discarded.
- Captures inside successful positive assertions, conditional assertions and
  `(*ACCEPT)` inside an assertion survive, as they do in the ovector.
- Order is capture-close order. `((a)b)` reports group 2 before group 1.
- Captures made inside a subroutine call or recursion are not recorded,
  because PCRE2 restores the caller's captures when the call returns.
- A subroutine or recursion that returns capture groups (`(?1(2))`,
  `(?R(1))`) would change the ovector without a matching history event. When
  history is enabled and such a return executes, `pcre2_match()` fails with
  `PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED` (-77) instead of returning
  incomplete history. Without the option these patterns behave as upstream.
  This rejection is provisional, pending Peter's decision between rejecting
  and recording returned groups; it is not a settled omission.

## Outcomes and lifetime

- Every call to `pcre2_match()`, `pcre2_dfa_match()` or `pcre2_jit_match()`
  resets the count to zero before doing anything else. Only a successful
  `pcre2_match()` with `PCRE2_CAPTURE_HISTORY` sets it. A no-match, an error,
  or a later match without the option never exposes older events.
- The pointer is borrowed from the match data. It stays valid until the match
  data is used for another match or freed. It may be NULL when the count is 0.
- Event storage comes from the match data's allocator and is kept for reuse,
  like the backtracking frames. Allocation failure returns
  `PCRE2_ERROR_NOMEMORY`, not a no-match. Matching without the option
  allocates nothing for history.

## Unsupported modes

| Call | Behavior with `PCRE2_CAPTURE_HISTORY` |
| --- | --- |
| `pcre2_match()` + `PCRE2_PARTIAL_SOFT` or `_HARD` | `PCRE2_ERROR_BADOPTION` |
| `pcre2_dfa_match()` | `PCRE2_ERROR_BADOPTION` |
| `pcre2_match()` on a JIT-compiled pattern | Runs the interpreter (the option is outside the JIT option mask) |
| `pcre2_jit_match()` | `PCRE2_ERROR_JIT_BADOPTION` |

The JIT rows are tested only when the library is built with JIT (a local
`-DPCRE2_SUPPORT_JIT=ON` CMake build passed at all widths on 2026-09-24,
x86_64 Linux). The Nix checks build without JIT and skip them. The fallback
has two guards: the option is outside the JIT option mask, and
`pcre2_jit_match()` returns `PCRE2_ERROR_JIT_BADOPTION`, which
`pcre2_match()` already treats as "use the interpreter".

## Implementation

Each backtracking frame already carries its own copy of the ovector, so
backtracking to an older frame restores captures. A `history_top` counter sits
in the copied part of the frame, and events live in one array on the match
data. Closing a capture appends at `history_top`; returning to an older frame
restores its smaller `history_top`, which truncates history in O(1). Assertion
`(*ACCEPT)` and conditional-assertion copy-back propagate `history_top` along
with the ovector.

Open questions:

- The frame grows by one `PCRE2_SIZE` whether or not history is enabled.
  One measurement (2026-09-24, commit 0e817597 vs pre-feature d6133fd5,
  AMD Threadripper 3990X, NixOS, 8-bit static Release, JIT off, hyperfine
  30 runs, host load about 7 on 128 cores) with
  `tests/benchmark/capture_history_bench.c`: disabled 963.9 ± 24.1 ms vs
  baseline 958.0 ± 19.2 ms, a difference within noise; enabled 1.093 ± 0.015 s,
  about 1.14× baseline. One workload set on one machine; not a gate yet.
- History memory is not counted against the heap limit. Events on the path are
  bounded by the frames that created them, but no test pins that bound.
- The option bit (`0x00080000`) and error code (-77) are provisional fork-local
  allocations. Check them mechanically against upstream before any rebase or
  release.

Tests: `tests/capture_history.c`, run by CTest at all three widths.
