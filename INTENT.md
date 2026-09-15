# PCRE2 fork

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
