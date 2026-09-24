# PCRE2 fork

## Capture-history experiment (Peter, 2026-09-23)

This dedicated checkout, `pcre2_capture_history`, extends Peter's existing
Zig-enabled PCRE2 fork. Preserve the earlier DFA-query purpose below, the
upstream C API and ordinary match behavior. Do not replace or rename the
original `../pcre2` checkout or silently switch its consumers to this branch.

The immediate outcome is optional chronological capture history containing
only events from the successful interpreter match path. Failed alternatives
and speculative iterations must roll back; positive-assertion captures must
survive. Existing ovector results remain unchanged. No DNA-specific API belongs
in PCRE2. History is disabled by default and uses PCRE2 allocator conventions.
JIT/DFA/partial-result behavior must be explicit; never silently claim history
was collected by an unsupported execution path.

A Zig consumer will explore fixed-length, bounded-gap maximal repeat families
over A/C/G/T strings, including shorter families with more occurrences. Use
strict input normalization, a simple independent enumeration oracle, and
measured scaling. Speed improvements are hypotheses, not acceptance criteria
already achieved. The actual DNA corpus is not yet supplied.

The original handoff is preserved verbatim in
[docs/capture_history/pcre2-zig-maximal-dna-repeats-capture-history-handoff.md](docs/capture_history/pcre2-zig-maximal-dna-repeats-capture-history-handoff.md).
[Review notes](docs/capture_history/REVIEW.md) identify unresolved overlap
semantics and implementation gates. PLAN.md tracks execution. This setup does
not authorize publication, upstream PRs, or changes to downstream dependency pins.

## Existing fork purpose

This fork supplies PCRE2 to Peter's Zig projects through `build.zig`.
It retains PCRE2's C API, Unicode support, and upstream matching behavior.

Peter approved adding a compiled-pattern DFA compatibility query on
September 15, 2026. RotShield and the planned `globlike` library need to reject
unsupported regex features before saving patterns. The query must inspect all
compiled branches without relying on representative subject strings.

This is a compatibility check, not a termination proof or a promise that
matching cannot exhaust resources. Existing matching APIs keep their behavior.
No new regex parser or replacement matching engine is in scope.

Success requires tests for hidden unsupported branches and supported patterns,
including Unicode, scoped flags, conditional groups, and variable-length
instructions. Preserve the upstream test suite and test all three code-unit
widths. Keep changes local until publication is requested.
