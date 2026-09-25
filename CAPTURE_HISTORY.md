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

A pattern can request history itself with the start-of-pattern verb
`(*CAPTURE_HISTORY)`, which may be combined with other start verbs such as
`(*NO_JIT)` or `(*UTF)`. `pcre2_match()` then behaves as if the option were
passed. Partial matching, `pcre2_dfa_match()` and `pcre2_jit_match()` refuse
such a pattern as they refuse the option.

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
  `(?R(1))`) leaves those groups set to their values from inside the call.
  When the call returns, history records one event per returned group that is
  set, in ascending group order, carrying the returned value. So `(c(a|b))(?1(2))`
  on `cacb` gives `{2,1,2} {1,0,2} {2,3,4}`, and the last event for each group
  matches its ovector entry. Intermediate captures inside the call are not
  recorded.

## Outcomes and lifetime

- Every call to `pcre2_match()`, `pcre2_dfa_match()` or `pcre2_jit_match()`
  resets the count to zero before doing anything else. Only a successful
  `pcre2_match()` with `PCRE2_CAPTURE_HISTORY` sets it. A no-match, an error,
  or a later match without the option never exposes older events.
- The pointer is borrowed from the match data. It stays valid until the match
  data is used for another match or freed. It may be NULL when the count is 0.
- Event storage comes from the match data's allocator and is kept for reuse,
  like the backtracking frames. Allocation failure returns
  `PCRE2_ERROR_NOMEMORY`, not a no-match. History storage shares the match
  heap limit (`pcre2_set_heap_limit()`) with the frame vector: when either grows
  during a history-enabled match, the sum must fit, or the match fails with
  `PCRE2_ERROR_HEAPLIMIT`. As with frames, storage retained in reused match data
  is only checked when it has to grow. Matching without the option
  allocates nothing for history.

## Execution modes

| Call | Behavior with history requested |
| --- | --- |
| `pcre2_match()`, interpreter | Records history |
| `pcre2_match()` on a JIT-compiled `(*CAPTURE_HISTORY)` pattern | Runs the JIT, which records history |
| `pcre2_match()` on a JIT-compiled pattern, option only | Runs the interpreter (the JIT code was compiled without history) |
| `pcre2_jit_match()` on a `(*CAPTURE_HISTORY)` pattern | Records history |
| `pcre2_jit_match()` with the option on a pattern without the verb | `PCRE2_ERROR_JIT_BADOPTION` |
| `pcre2_jit_compile()` of a history pattern containing `(?n(list))` / `(?R(list))` | `PCRE2_ERROR_JIT_UNSUPPORTED`; the interpreter runs it |
| Partial matching (either engine) | `PCRE2_ERROR_BADOPTION` / `PCRE2_ERROR_JIT_BADOPTION` |
| `pcre2_dfa_match()` | `PCRE2_ERROR_BADOPTION` |

History is known at JIT-compile time only through the verb, so the verb is the
way to get JIT speed with history. The JIT keeps the count of events on the
current path in its existing `capture_last` slot, which it already saves and
restores at every capture close, bracket backtrack and subroutine call; a
helper appends each event. Callouts still receive the correct `capture_last`
(the last event's group). Capture-bracket optimizations are off for these
patterns, as they are for patterns with callouts.

Evidence: every hand-written case runs through both engines, and
`tests/capture_history_jit_diff.c` compares rc, ovector and full history from
both engines on seeded random patterns (captures, alternation, all quantifier
kinds, atomic groups, lookarounds, backreferences, `(*ACCEPT)`, subroutine
calls). The test suite runs 3,000 patterns per width; `./fuzz [cases] [seed]`
runs more. Cases where the engines fail differently on limits (for example
recursion-loop versus JIT stack) are counted separately, not compared.

The differential found an interpreter bug that the hand-written cases missed:
an `(*ACCEPT)` in an outer assertion, after a nested positive assertion had
completed, re-runs the tail after the nested assertion (upstream control flow),
and history recorded that tail twice. The nested assertion now remembers the
event count at which it completed, and the re-run starts from there.

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
- The option bit (`0x00080000`) and internal pattern flag `PCRE2_CAPHIST_SET`
  (`0x02000000`) are provisional fork-local allocations, pending upstream. Check them mechanically against upstream before any rebase or
  release.

## Zig

The fork's `build.zig` exports module `pcre2_capture_history`
(`src/zig/capture_history.zig`):

```zig
const ch = @import("pcre2_capture_history");
const Api = ch.Api(8); // links pcre2_get_capture_event_*_8
// pass ch.option to pcre2_match, then:
const events = Api.events(match_data); // []const ch.CaptureEvent, borrowed
var it = ch.groupIterator(events, hit_group);
while (it.next()) |ev| use(ev.slice(u8, subject));
```

`Api.events` takes any pointer to match data, so it works with whichever C
import the caller uses. The slice aliases the C array and follows its
lifetime. `translate-c` cannot expand PCRE2's width-suffix macros, so Zig callers
use suffixed names such as `pcre2_code_8` and `pcre2_match_8`.

Tests: `tests/capture_history.c` (CTest, all widths) and
`src/zig/capture_history_test.zig` (`zig build test`, Nix check `zig`, all
widths).

The first consumer, the chain_packing DNA repeat-family finder and its
`dna-repeats` CLI, lives in its own project (`~/Code/dna_repeats`) and
depends on this branch as a Zig package.
