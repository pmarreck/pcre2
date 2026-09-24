# PCRE2 + Zig exploration: gap-constrained maximal DNA repeats and capture history

## Handoff for a Codex agent

**Date:** 2026-09-23  
**Project context:** There is already a personal fork of PCRE2 that has been modified to be more Zig-friendly. Work with that fork rather than assuming pristine upstream PCRE2.

This document captures the reasoning developed during an extended exploration of whether PCRE2 can efficiently find repeated DNA substrings under a bounded-gap constraint, and whether PCRE2 itself should be extended to preserve the capture history of a repeated capturing group.

The immediate project has two related goals:

1. Build a practical regex-centric finder for **gap-constrained maximal repeat families** in DNA-like data.
2. Prototype a **general PCRE2 capture-history feature** so a repeated capturing group can return every committed capture on the successful match path, instead of only the final capture.

The second goal is useful well beyond DNA and may be a plausible upstreamable PCRE2 feature.

---

## 1. Input corpus

The actual DNA corpus should be inserted here verbatim by the human operator:

```text
INSERT_OVERLONG_DNA_STRAND_HERE
```

The client that transported the original corpus inserted linefeeds and may insert hyphens. Do **not** silently normalize arbitrary characters away, because that could conceal corrupted input.

Recommended normalization:

1. Remove ASCII whitespace.
2. Remove literal `-` characters.
3. Uppercase if desired.
4. Validate that the remainder is entirely `[ACGT]+`.
5. Fail loudly if anything else remains.

Example logical transformation:

```text
raw -> remove [ASCII whitespace and '-'] -> uppercase -> validate ^[ACGT]+$
```

Do not treat biological meaning as relevant here. For this project, the subject is simply a string over a four-symbol alphabet.

---

## 2. Desired repeat semantics

Parameters should be configurable:

```text
minimum repeat length L_min = 8
maximum gap D             = 400   # 200 was initially tried; it proved too restrictive
minimum occurrence count = 2
```

### 2.1 Gap definition

`D` is the maximum number of symbols **between the end of one occurrence and the beginning of the next**.

For a repeat of length `L` with consecutive starts `p[i]` and `p[i+1]`:

```text
gap = p[i+1] - (p[i] + L)
```

The current regex design assumes:

```text
0 <= gap <= D
```

Therefore occurrences are non-overlapping in the current model.

If overlapping repeats should eventually count, explicitly redesign this rule; do not accidentally change the semantics by tweaking a quantifier.

### 2.2 A repeat family

For a fixed repeat string `unit` of length `L`, collect all occurrence start offsets in ascending order.

Split them into connected components whenever the next occurrence has a gap greater than `D`.

Each connected component with at least two occurrences is one **gap-constrained repeat family**.

Example:

```text
unit positions: [100, 180, 900, 980]
L = 20
D = 100

100 -> 180: gap 60   => same family
180 -> 900: gap 700  => split
900 -> 980: gap 60   => same family

families:
[100, 180]
[900, 980]
```

---

## 3. Why “longest repeat” alone is the wrong objective

An important case emerged:

```text
length 20 -> 5 occurrences
length 17 -> 8 occurrences
```

The 17-mer may be the common core of all eight copies, while only five happen to share the additional three bases required for the 20-mer.

These are not necessarily competing answers. They may describe two different levels of structure:

- **20 × 5**: longer and more specific
- **17 × 8**: shorter but shared by a broader family

Crucially, a shorter repeat can remain maximal over its **entire eight-occurrence family** even when a five-occurrence subset can be extended.

Therefore do **not** globally stop after discovering the longest successful length.

A good default output strategy is to retain all maximal families and optionally mark the Pareto frontier in:

```text
(length, occurrence_count)
```

A family dominates another only if it is at least as long **and** occurs at least as many times, with one strict improvement.

Do not prematurely collapse this into a scalar score. If useful later, secondary metrics may include:

```text
coverage        = length * count
redundant_bases = length * (count - 1)
```

but these encode value judgments and should not replace the raw `(length, count)` information.

---

## 4. Maximal repeats

The useful notion is a maximal repeat, not simply every repeated substring.

Given a repeat `unit` of length `L` occurring at positions:

```text
positions = [p0, p1, p2, ...]
```

Define the context immediately to the left of each occurrence:

```text
left_context(p) = subject[p - 1] if p > 0 else BOS
```

and immediately to the right:

```text
right_context(p) = subject[p + L] if p + L < subject.len else EOS
```

Then, for the occurrence family under consideration:

```text
left_maximal  := there is more than one distinct left context
right_maximal := there is more than one distinct right context
maximal       := left_maximal AND right_maximal
```

Equivalent intuition:

- If **all** occurrences have the same left neighbor, prepend that neighbor: the repeat was not left-maximal.
- If **all** occurrences have the same right neighbor, append that neighbor: the repeat was not right-maximal.
- Only when both common extensions are impossible is the family maximal.

Use explicit `BOS` and `EOS` sentinels as contexts; boundaries are meaningful.

### Important subtlety

Maximality is evaluated against the **whole occurrence family**, not every subset.

Suppose a 17-mer occurs eight times. Five of those copies happen to extend identically for three more bases, producing a 20-mer occurring five times.

That does **not** make the 17-mer non-maximal if the other three occurrences prevent a common extension across all eight.

Thus both of these can legitimately survive:

```text
17 × 8
20 × 5
```

For this project, because the gap bound can split identical strings into distinct local families, apply maximality **per gap-connected component**. This is a gap-constrained adaptation of the standard maximal-repeat idea.

---

## 5. The first regex approach and why it was unsatisfactory

The initial idea was approximately:

```regex
(?=
    (?<unit>[ACGT]{8,1450})
    (?:
        (?>[ACGT]{0,400}?\k<unit>)
    )+
)
```

The outer positive lookahead keeps the overall match zero-width so every possible starting offset can be examined.

The intention was:

1. greedily capture the longest possible `unit`;
2. search up to `D` symbols for another copy;
3. repeat for two or more copies;
4. use atomicity to limit backtracking in the gap.

This was practically bad.

`regex101` reported catastrophic backtracking for large candidate ranges such as `{8,1450}`. Reducing the candidate range to approximately `{10,20}` avoided that warning but still timed out on the corpus.

Do not overinterpret regex101's exact diagnostic as a proof of formal exponential complexity. It has execution limits and heuristics. The useful empirical conclusion is simpler:

> Leaving candidate length variable inside the regex causes PCRE2 to redo an expensive bounded-gap/backreference search for many candidate lengths at many start positions.

Atomicizing the gap does **not** prevent PCRE2 from backtracking into the earlier variable-length `unit` capture.

### Atomic capture observation

PCRE2 has no special “atomic-capturing group” syntax, but captures can be nested inside an atomic group:

```regex
(?>(?<unit>[ACGT]{8,1450}))
```

That is not useful here, because it freezes the initial greedy capture. If the longest candidate does not repeat, PCRE2 is forbidden from shortening it.

So atomicity cannot simply be wrapped around the variable-length `unit`.

---

## 6. Backreferences are not limited to nine

Do not design around an imagined nine-backreference limit.

Old `\1`-style syntax has ambiguity with octal escapes, but PCRE2 supports large numbered and named references, for example:

```regex
\g{12}
\g{137}
\k<unit>
```

Named backreferences are preferable for this project.

---

## 7. Human insight: grow the pattern instead of asking one quantifier to discover its length

A promising conceptual path was to begin with a one-symbol repeat and progressively grow it, potentially using generated nested captures.

Example prefix materialization:

```regex
(?<p4>
    (?<p3>
        (?<p2>
            (?<p1>[ACGT])
            [ACGT]
        )
        [ACGT]
    )
    [ACGT]
)
```

For `ACGT`, this yields conceptually:

```text
p1 = A
p2 = AC
p3 = ACG
p4 = ACGT
```

A generated regex could then test `p4`, `p3`, `p2`, etc. longest-first without variable-length backtracking inside the capture itself.

This is viable because generation is trivial even when manual construction is absurd.

However, the `20 × 5` versus `17 × 8` observation shows why merely extending one chosen pair until it can no longer extend is insufficient: a shorter pattern may have a larger occurrence family and remain independently maximal.

That leads to the cleaner strategy below.

---

## 8. Recommended search strategy: fixed length, one length at a time

Move candidate-length enumeration **outside** PCRE2.

For each fixed `L`, generate a regex containing exactly `[ACGT]{L}`.

Conceptually:

```text
for L from max_length down to L_min:
    compile/run fixed-length pattern L
    collect repeat-family evidence
```

Do **not** stop after the first successful `L`, because shorter maximal families with more occurrences may also matter.

This removes the major candidate-length backtracking dimension entirely.

### 8.1 Fixed-length N-occurrence regex

A good candidate is:

```regex
(?x)
(?=
    (?<unit>[ACGT]{20})
    (?:
        (?>
            [ACGT]{0,400}?
            (?<hit>\k<unit>)
        )
    )++
)
```

For generated parameters:

```regex
(?x)
(?=
    (?<unit>[ACGT]{L})
    (?:
        (?>
            [ACGT]{0,D}?
            (?<hit>\k<unit>)
        )
    )++
)
```

Semantics:

1. `unit` is a fixed-length candidate.
2. The lazy bounded gap tries distances `0..D` until the nearest next `unit` is found.
3. The atomic group commits that gap/occurrence pair once found.
4. The possessive outer repetition greedily finds as many subsequent copies as possible and does not later reconsider the number of copies.
5. `++` requires at least one subsequent hit, therefore the total occurrence count is at least two.
6. The outer lookahead makes the whole match zero-width, allowing overlapping candidate starts to be examined.

This can logically match:

```text
unit
+ gap + hit
+ gap + hit
+ gap + hit
...
```

for arbitrary `N >= 2`.

### 8.2 Alternative deterministic gap walker

Another candidate is:

```regex
(?=
    (?<unit>[ACGT]{L})
    (?:
        (?:(?!\k<unit>)[ACGT]){0,D}+
        (?<hit>\k<unit>)
    )++
)
```

This walks one symbol at a time while `unit` does not begin at the current position.

It is appealing because the gap traversal is monotonic and possessive, but it evaluates a backreference lookahead repeatedly while walking the gap. Do not assume it is faster.

**Benchmark both forms.** The lazy bounded-gap form may let PCRE2 optimize better.

### 8.3 Why nearest-next occurrence is sufficient

For sorted occurrence positions of the same fixed `unit`, gap connectivity is one-dimensional.

If the nearest next occurrence is farther than `D`, every still-farther occurrence is also outside the current component.

Therefore repeatedly choosing the nearest reachable next copy reconstructs the unique gap-connected chain; there is no useful alternative path to explore.

---

## 9. Zero-width global scanning

Because the whole pattern is a positive lookahead, successful matches are zero-width.

Use current PCRE2's global-match helper if available in the fork, or manually advance the start offset by one DNA code unit after each zero-width match.

DNA here is ASCII, so one code unit equals one symbol.

Do not allow a zero-width global matching loop to remain at the same start offset.

Also note that the same repeat family may be rediscovered from several occurrences as successively shorter suffixes of the same family. Deduplicate after collection.

---

## 10. Aggregating fixed-length results

For each `L`, collect data keyed by the actual repeat bytes:

```text
key = (L, unit_bytes)
```

Union all observed occurrence offsets for that key, sort them, and remove duplicates.

Then split the sorted positions into gap-connected components using:

```text
gap = next_start - (current_start + L)
```

Split if:

```text
gap > D
```

Discard components with fewer than two positions.

This aggregation step eliminates duplicate suffix-family results caused by zero-width scanning from every possible start.

Suggested logical result type:

```text
RepeatFamily {
    unit
    length
    positions[]
    count
    gaps[]
    left_maximal
    right_maximal
    maximal
}
```

After building components, apply the maximality test described above.

---

## 11. Why ordinary PCRE2 cannot return all N `hit` captures

This is the engine limitation that motivated the PCRE2 fork idea.

Given:

```regex
(?<hit>\k<unit>)
```

inside a repeated group:

```regex
(?:(?<hit>\k<unit>))+
```

PCRE2's ordinary match result retains only the value/offsets from the **final iteration** of `hit`.

Current PCRE2 documentation explicitly says that when a capture group is repeated, the captured value is from the final iteration. The ordinary ovector likewise exposes only the last portion matched by a group that participated repeatedly.

PCRE2 documentation also explicitly points to **callouts** when intermediate capture values are needed.

Relevant official documentation:

- https://pcre.org/current/doc/html/pcre2pattern.html
- https://pcre.org/current/doc/html/pcre2api.html
- https://pcre.org/current/doc/html/pcre2callout.html

Callouts work, but they make the solution depend on an external callback and are therefore less self-contained than desired.

This gap in the API is general, not DNA-specific.

---

## 12. Proposed general PCRE2 feature: capture history

### Goal

Permit a repeated capture group to expose **all captures on the final successful match path**, in chronological order.

For:

```regex
(?<x>a)+
```

matched against:

```text
aaa
```

existing PCRE2 behavior remains:

```text
x final capture = [2, 3)
```

New optional history would additionally expose:

```text
x history = [
    [0, 1),
    [1, 2),
    [2, 3),
]
```

The existing ovector must remain completely backward-compatible.

### Critical semantic requirement

History must describe the **successful path**, not every speculative capture PCRE2 attempted while backtracking.

For example:

```regex
(a+)(a)
```

against:

```text
aaa
```

PCRE2 may tentatively let group 1 capture all three `a`s, discover that group 2 cannot match, then backtrack group 1 to two `a`s.

Capture history must **not** report the abandoned speculative `aaa` capture as a committed result.

Only captures that survive on the final successful path belong in history.

---

## 13. Plausible interpreter implementation

The core idea is an append-only event log with O(1) logical rollback.

### 13.1 Capture event

Prototype structure:

```c
typedef struct {
    uint32_t group;
    PCRE2_SIZE start;
    PCRE2_SIZE end;
} pcre2_capture_event;
```

Maintain in match state:

```text
history_events[]
history_count
history_capacity
```

Whenever a capture successfully closes, append:

```text
{ group_number, start_offset, end_offset }
```

### 13.2 Backtracking rollback

The difficult part is not appending events; it is removing speculative events when PCRE2 backtracks.

Whenever the matcher creates/restores a backtracking state that can invalidate captures, preserve a checkpoint:

```text
history_checkpoint = history_count
```

When backtracking restores that state:

```text
history_count = history_checkpoint
```

No event-by-event deletion is required. Stale array elements above `history_count` may simply be overwritten later.

This gives conceptually:

```text
forward capture: O(1) amortized append
rollback:        O(1) logical truncate
```

### Important instruction to Codex

Do **not** blindly add a checkpoint field to random frame structures before understanding PCRE2's existing capture restoration machinery.

First inspect the fork and identify:

1. where the interpreter writes capture start/end offsets;
2. how current capture state is restored during backtracking;
3. what frame/stack state already corresponds to an ovector restoration boundary;
4. whether the cleanest implementation is a field in existing frames or a parallel checkpoint stack.

Hook history rollback into the **same semantic boundaries** as existing capture rollback.

The design principle is clear even if the exact internal integration point depends on the PCRE2 version/fork.

---

## 14. Proposed public API

Start with the least invasive API that proves the mechanism.

A Zig-friendly design is to expose one chronological event vector rather than dynamically allocating one array per capture group during matching.

For example:

```c
PCRE2_EXP_DECL PCRE2_SIZE
pcre2_get_capture_event_count(pcre2_match_data *match_data);

PCRE2_EXP_DECL const pcre2_capture_event *
pcre2_get_capture_event_pointer(pcre2_match_data *match_data);
```

The returned event vector might contain:

```text
{ group=1, start=100, end=120 }
{ group=2, start=280, end=300 }
{ group=2, start=450, end=470 }
{ group=2, start=700, end=720 }
```

This is already an “array of repeated captures”; callers can filter by group number without callback machinery.

Later convenience APIs could include:

```c
PCRE2_SIZE pcre2_get_capture_history_count(
    pcre2_match_data *match_data,
    uint32_t group);
```

and perhaps a group-indexed span getter.

Do not require string copies. Offsets are sufficient; callers can slice the original subject. This mirrors PCRE2's existing ovector philosophy.

### Lifetime

Prefer the same conceptual lifetime as existing match data:

```text
valid until match_data is reused/freed
```

If the subject must outlive the call independently, existing PCRE2 mechanisms such as copying the matched subject remain separate concerns.

---

## 15. Enabling capture history

There are two stages.

### Stage A: API option first

For the first prototype, add an opt-in match/compile facility such as conceptually:

```text
PCRE2_CAPTURE_HISTORY
```

Exact bit allocation and whether it belongs at compile time or match time should be chosen after inspecting available option space and current fork conventions.

The important requirements are:

- history disabled by default;
- existing code pays effectively zero cost when disabled;
- current ovector behavior does not change.

### Stage B: make the regex self-describing

The eventual desirable syntax is a pattern start verb such as:

```regex
(*CAPTURE_HISTORY)
```

Then the DNA regex could literally request the engine behavior it requires:

```regex
(*CAPTURE_HISTORY)
(?=
    (?<unit>[ACGT]{20})
    (?:
        (?>
            [ACGT]{0,400}?
            (?<hit>\k<unit>)
        )
    )++
)
```

This satisfies the aesthetic and practical goal that the pattern itself declares capture-history semantics rather than requiring an unrelated external callback.

Do not bikeshed selective syntax yet. A future enhancement might record only named groups, but a global opt-in is much easier to prototype and validate.

---

## 16. JIT strategy

Do **not** attempt interpreter and JIT support simultaneously in the first implementation.

Current PCRE2 can already be forced to use the interpreter by disabling JIT, and its public API explicitly distinguishes JIT from interpreted matching.

Recommended phases:

1. Implement capture history in `pcre2_match()` interpreter only.
2. When capture history is enabled, force interpreter execution or make JIT compilation decline that mode.
3. Establish semantics, tests, and benchmark behavior.
4. Only then investigate JIT support.

The JIT implementation will need generated machine code to perform history append and rollback semantics or an equivalent representation. That is a separate project milestone.

Do not let JIT complexity block validating the underlying feature.

---

## 17. Semantics that must be decided explicitly

### Successful full match

This is the required v1 case.

History contains exactly captures belonging to the final successful path.

### No match

Prefer consistency with PCRE2's existing match-data rules rather than inventing special semantics. Document clearly whether previous history remains untouched or becomes unavailable.

At minimum, callers must not mistake stale history for a successful match result.

### Partial match

It is acceptable for v1 to declare capture history unsupported/undefined for partial-match results until semantics are deliberately designed.

### Positive assertions

Captures inside a successful positive lookahead are already meaningful in PCRE2. Capture-history events from the successful assertion path should survive exactly as ordinary captures do.

This matters directly to the DNA regex because the entire search is intentionally inside a positive lookahead.

### Negative assertions

A successful negative assertion succeeds because its inner match paths fail. Speculative capture-history events from those failed inner paths must not survive.

### Alternation and backtracking

Events produced by failed alternatives must roll back.

### Atomic groups and possessive quantifiers

History should reflect captures on the committed path exactly as normal capture semantics do.

### Recursion/subroutine calls

Do not ignore these forever. They are excellent stress tests for rollback semantics, but they can be deferred until the basic repeated-group cases work.

### `pcre2_dfa_match()`

PCRE2's DFA matcher does not provide ordinary captured substrings, so capture history should not be bolted onto it in v1.

---

## 18. Tests: build this TDD-first

Write each test **before** implementing the behavior it exercises.

### Test 1: simple repeated capture

Pattern:

```regex
(a)+
```

Subject:

```text
aaa
```

Expected history for group 1:

```text
[0,1)
[1,2)
[2,3)
```

Existing final ovector value must remain:

```text
[2,3)
```

### Test 2: speculative capture must roll back

Pattern:

```regex
(a+)(a)
```

Subject:

```text
aaa
```

Final group 1 is:

```text
aa
```

The abandoned speculative capture of all three `a`s must not appear as committed history.

### Test 3: repeated capture followed by later failure/backtrack

Choose a pattern where several loop iterations occur and a later token forces one or more iterations to be reconsidered.

Verify that events from abandoned iterations disappear.

### Test 4: alternation rollback

Create a branch that captures, fails later, and falls through to another successful branch.

History must contain only successful-branch captures.

### Test 5: positive lookahead

Pattern concept:

```regex
(?=((?<x>a)+))
```

Subject:

```text
aaa
```

History for `x` should contain the successful repeated captures even though the overall match is zero-width.

### Test 6: negative lookahead

Captures attempted inside a successful negative assertion must not survive.

### Test 7: atomic group

Confirm that successful captures inside an atomic group appear once committed.

### Test 8: possessive repeat

Confirm history across a repeated group whose repeat count cannot be reduced by backtracking.

### Test 9: disabled-history compatibility

With capture history disabled:

- byte-for-byte existing expected match results remain unchanged;
- no history allocations occur;
- performance should be statistically indistinguishable from the fork's baseline.

### Test 10: DNA-style repeated backreference

Pattern:

```regex
(?=
    (?<unit>[ACGT]{4})
    (?:
        (?>[ACGT]{0,10}?(?<hit>\k<unit>))
    )++
)
```

Construct a small deterministic subject with 3+ copies.

Expected:

- `unit` history/final capture contains the seed;
- `hit` history contains every subsequent occurrence offset in order.

### Later tests

Add recursion, subroutine calls, duplicate names, nested captures, `(*ACCEPT)`, and resource-limit interactions after basic semantics stabilize.

---

## 19. Zig-facing design

The fork is already being made more Zig-friendly. Preserve that direction.

Ideal Zig-facing data is offsets, not allocated C strings.

Conceptual Zig types:

```zig
pub const CaptureEvent = struct {
    group: u32,
    start: usize,
    end: usize,
};
```

Expose a borrowed slice valid for the lifetime of the current match result:

```zig
pub fn captureEvents(match_data: *MatchData) []const CaptureEvent
```

Then a zero-allocation iterator can filter one group:

```zig
pub fn capturesForGroup(
    events: []const CaptureEvent,
    group: u32,
) CaptureIterator
```

For the DNA finder, resolve named groups (`unit`, `hit`) to group numbers once after compilation and filter events by number.

Do not copy the repeated DNA strings for every event. Slice the subject using offsets.

---

## 20. DNA finder with capture history

For each fixed `L`:

1. Generate the fixed-length regex.
2. Compile it.
3. Scan zero-width matches across every possible subject start.
4. For each match:
   - read `unit` from its normal capture offsets;
   - read every `hit` occurrence from capture history;
   - collect occurrence offsets under `(L, unit)`.
5. Deduplicate offsets.
6. Sort offsets.
7. Split into gap-connected components.
8. Discard components with count `< 2`.
9. Apply left/right maximality per component.
10. Store maximal families.
11. Continue to shorter `L`; do not stop merely because a longer repeat existed.
12. Present results sorted by length and count, with optional Pareto-frontier marking.

### A practical maximum length

For non-overlapping repeats that must occur at least twice, a candidate cannot exceed:

```text
floor(subject_length / 2)
```

That is a safe generic upper bound for `L`.

Further pruning may be possible later.

---

## 21. Important observation: capture history is useful, but not logically required for the DNA algorithm

Be intellectually honest about this.

The DNA problem can be solved without modifying PCRE2.

For example, with fixed `L`, scanning every start and recording the seed plus the ordinary final `hit` is enough to reconstruct connected occurrence sets after aggregation. A pair-wise regex can also produce graph edges that are later unioned.

The reason to pursue capture history is stronger and more general:

- one regex match can directly return the whole repeated family;
- no callback is required;
- the engine already internally traverses these repeated captures;
- repeated-capture history is broadly useful outside DNA;
- PCRE2's own documentation identifies callouts as the workaround for missing intermediate capture values.

Therefore treat capture history as a **general PCRE2 capability motivated by this problem**, not as a hack required to solve the problem at all costs.

---

## 22. Alternative: generated mega-regex for many lengths

A theoretically interesting alternative is to generate one large regex with an independent lookahead probe for every fixed length.

Conceptually:

```regex
(?:(?=(?<u20>[ACGT]{20}) ... ))?
(?:(?=(?<u19>[ACGT]{19}) ... ))?
(?:(?=(?<u18>[ACGT]{18}) ... ))?
...
```

Because each probe has a fixed candidate length, this avoids backtracking one variable-length capture through `20 -> 19 -> 18 -> ...`.

However:

- every probe being optional means the overall regex succeeds trivially and the caller must inspect captures;
- the compiled pattern can become huge;
- the capture vector becomes huge;
- compile/JIT cost may dominate;
- debugging becomes unpleasant;
- it does not obviously beat a simple loop over fixed lengths.

Keep this as an experiment, not the first implementation.

---

## 23. Alternative: generated nested prefix captures

The earlier nested-prefix idea can also be generated:

```text
p8
p9
p10
...
pN
```

and tested longest-first.

This can express multiple candidate lengths without a variable quantifier and illustrates an important general principle:

> Generate structural alternatives explicitly instead of asking a backtracking quantifier to discover structure implicitly.

Still, fixed-length iteration is easier to benchmark, easier to reason about, and naturally surfaces shorter high-count maximal families.

Use nested-prefix generation only if it demonstrates a concrete runtime or expressiveness advantage.

---

## 24. Complexity expectations: do not overclaim

Fixed candidate length removes one major backtracking dimension, but it does not magically make the whole search linear.

For a single fixed `L`, a bounded lazy gap search may try up to `D+1` backreference positions, and each backreference comparison may inspect up to `L` symbols.

A rough local bound is therefore related to:

```text
O(D * L)
```

per attempted next occurrence.

Scanning every subject start and repeatedly traversing long repeat chains can make highly repetitive adversarial subjects much more expensive; e.g. many starts may rediscover suffixes of the same chain.

Therefore benchmark; do not infer complexity from one friendly DNA corpus.

Also separate these measurements:

1. regex compilation time;
2. optional JIT compilation time;
3. matching time;
4. aggregation/maximality time;
5. capture-history allocation/event count.

When measuring scaling by input size, avoid using simple concatenation of the same corpus as the only test: concatenation manufactures giant easy repeats and changes the problem distribution.

---

## 25. Benchmark corpus plan

Use multiple deterministic classes.

### Real corpus

Use the supplied DNA corpus with exact cleanup and several prefix sizes where possible.

### Highly repetitive adversary

```text
AAAAAA...
```

This stresses enormous numbers of repeat relationships and long chains.

### Periodic adversary

```text
ACGTACGTACGT...
```

This stresses many overlapping structural repeats.

### Low-repeat deterministic corpus

Generate a de Bruijn sequence over alphabet `{A,C,G,T}` at an appropriate order, or another deterministic sequence with controlled substring uniqueness.

This is preferable to claiming properties of an arbitrary pseudorandom sample.

### Scaling

For each corpus class, measure sizes approximately:

```text
n
2n
4n
8n
```

Report both absolute time and ratios.

Also vary independently:

```text
L
D
```

because treating `D` as a fixed constant and letting `D` grow with `n` are different complexity regimes.

---

## 26. Validation oracle

Before trusting clever PCRE2 behavior, build a deliberately simple Zig reference implementation for small inputs.

It may be slow. Correctness matters more than speed.

For every small test corpus:

1. enumerate substrings of lengths `L_min..max`;
2. find all exact occurrences;
3. sort positions;
4. split by the gap rule;
5. apply maximality;
6. compare exactly against regex-derived families.

Use this oracle for TDD and fuzzing.

Once the regex implementation matches the oracle across exhaustive small cases and randomized/fuzzed cases, benchmark larger inputs.

Do not use the optimized implementation as its own oracle.

---

## 27. Capture-history performance requirements

The PCRE2 feature should satisfy:

### History disabled

Near-zero overhead.

Ideally:

- no additional allocation;
- no per-capture branch beyond a highly predictable feature flag, or no extra branch if compilation specializes it away;
- no larger ordinary backtracking frames unless necessary.

### History enabled

Cost should be roughly proportional to the number of committed/speculatively logged captures plus rollback bookkeeping.

An append-only buffer with capacity growth is acceptable initially.

Use PCRE2's configured allocator/memory-control conventions rather than raw `malloc` if the fork preserves those abstractions.

Consider a future explicit history-event limit to prevent intentionally pathological patterns from producing unbounded result data.

---

## 28. Upstreamability considerations

Keep the feature general and conservative.

Good properties:

- opt-in only;
- existing ovector semantics unchanged;
- generic repeated-capture use case;
- offset-based result representation;
- interpreter implementation first;
- no external callback required;
- no DNA-specific API names or assumptions.

Avoid making the initial patch depend on the DNA finder.

The strongest upstream rationale is simply:

> PCRE2 already supports repeated captures internally, but its public result exposes only the final iteration; current documentation recommends callouts for intermediate values. Capture history makes those committed intermediate values first-class match data.

---

## 29. Current PCRE2 facts verified against official documentation

As of 2026-09-23, current PCRE2 documentation states or supports the following:

1. **Repeated capture groups return the final iteration's capture.**  
   See `pcre2pattern` and the ovector description in `pcre2api`.

2. **The ovector stores offset pairs for the overall match and captured groups.**  
   It uses code-unit offsets and exposes only the last portion matched when a capture group repeats.

3. **Callouts are the documented mechanism for observing intermediate matching state/capture values.**

4. **PCRE2 supports explicit named backreferences and larger numbered backreferences; there is no general nine-backreference ceiling.**

5. **JIT is optional and can be disabled so `pcre2_match()` uses the interpreter.**

Official references:

- PCRE2 pattern syntax/semantics:  
  https://pcre.org/current/doc/html/pcre2pattern.html

- PCRE2 native API / match data / ovector / JIT behavior:  
  https://pcre.org/current/doc/html/pcre2api.html

- PCRE2 callouts:  
  https://pcre.org/current/doc/html/pcre2callout.html

- PCRE2 JIT:  
  https://pcre.org/current/doc/html/pcre2jit.html

---

## 30. Suggested implementation order for the Codex agent

Follow this order unless inspection of the existing fork reveals a compelling reason to change it.

### Phase 0: inspect and baseline

- Identify exact upstream PCRE2 version/commit underlying the fork.
- Identify existing Zig-friendly modifications.
- Run the existing test suite unchanged.
- Record baseline performance for representative PCRE2 tests.
- Locate capture write and rollback machinery in the interpreter.

### Phase 1: capture history core, interpreter only

TDD sequence:

1. Add failing `(a)+` history test.
2. Add minimal history event storage and getter API.
3. Make the simple repeated capture test pass.
4. Add failing speculative-backtracking test `(a+)(a)`.
5. Implement rollback checkpoints integrated with existing capture rollback.
6. Add alternation/lookahead/atomic/possessive tests one by one.
7. Keep ordinary ovector tests unchanged.

### Phase 2: opt-in behavior

- Ensure history is off by default.
- Add API option/flag.
- Verify no-history allocation behavior.
- Benchmark baseline overhead.

### Phase 3: self-describing pattern verb

Prototype:

```regex
(*CAPTURE_HISTORY)
```

- Parse and store a compiled-code flag.
- Make matching automatically enable history for that pattern.
- Initially force interpreter mode.

### Phase 4: Zig wrapper

- Expose capture event slice.
- Add group-name-to-number resolution convenience.
- Avoid copying subject substrings.

### Phase 5: fixed-length DNA finder

- Add strict cleanup/validation.
- Implement fixed-length regex generation.
- Scan all starts safely despite zero-width results.
- Aggregate `(L, unit) -> positions`.
- Gap-split components.
- Apply maximality.
- Compare against the brute-force Zig oracle.

### Phase 6: performance investigation

- Benchmark lazy atomic gap versus tempered possessive gap.
- Benchmark capture history versus callback-based capture collection.
- Measure compilation separately from matching.
- Benchmark DNA, repetitive, periodic, and low-repeat deterministic inputs.

### Phase 7: JIT feasibility

Only after semantics are stable:

- inspect PCRE2 JIT capture-writing paths;
- determine how history append/rollback maps to generated code;
- add JIT tests matching interpreter event histories exactly.

---

## 31. Questions the implementation should answer empirically

1. Is the atomic lazy-gap form faster than the tempered possessive form for fixed `L`?
2. How much work is duplicated by zero-width scanning from every occurrence in a long chain?
3. Does aggregating and memoizing already-seen `(L, unit, position)` materially reduce runtime?
4. Is compiling one regex per `L` cheap enough, or should lengths be batched/generated into one larger pattern?
5. What is the crossover where JIT compilation cost exceeds its matching benefit for one-off fixed-length patterns?
6. What is the runtime/memory overhead of capture history versus explicit PCRE2 callouts?
7. Can history-disabled matching be made indistinguishable from the current fork in benchmarks?
8. Is per-group history indexing worth doing in C, or is a chronological event vector plus a Zig-side filter better?
9. Are partial-match semantics worth supporting, or should v1 explicitly limit history to full successful matches?
10. Can the capture-history patch be kept small and generic enough to submit upstream later?

---

## 32. Non-goals for the first pass

Do not get distracted by these before the core works:

- biological interpretation of motifs;
- approximate/fuzzy DNA matching;
- reverse complements;
- insertions/deletions/substitutions;
- overlapping-repeat semantics;
- suffix-tree/suffix-array replacement as the primary implementation;
- JIT support before interpreter correctness;
- inventing a single magical scalar score for length versus count;
- elaborate capture-history syntax for selecting individual groups.

A suffix array/tree or rolling-hash implementation is welcome as a **validation/performance comparator**, but the point of this project is specifically to explore how far PCRE2 plus a principled capture-history extension can go.

---

## 33. The concise thesis

The original variable-length regex made PCRE2 discover repeat length by backtracking through an expensive repeat search. That was the wrong place to put the variability.

Instead:

```text
external/generated structure chooses a fixed length
                 ↓
PCRE2 finds an arbitrary-length chain of exact repeated copies
                 ↓
capture history returns every copy's offsets
                 ↓
aggregation applies the bounded-gap component rule
                 ↓
maximality removes extensible staircase substrings
                 ↓
retain multiple meaningful length/count families
```

And the PCRE2 extension itself is straightforward in concept:

```text
capture closes -> append event
backtracking checkpoint -> remember history_count
backtrack -> restore history_count
successful match -> expose surviving event array
```

The devil is in integrating rollback with PCRE2's actual interpreter frame/capture machinery without imposing cost when the feature is disabled.

That is the part to inspect carefully rather than guessing.

---

## 34. Working fixed-length pattern template

Start experimentation from this pattern, generated for each fixed `L` and `D`:

```regex
(?x)
(?=
    (?<unit>[ACGT]{L})
    (?:
        (?>
            [ACGT]{0,D}?
            (?<hit>\k<unit>)
        )
    )++
)
```

For stock PCRE2, ordinary match data gives the seed `unit` and only the final repeated `hit` capture.

For the proposed fork with capture history, `hit` should yield the full chronological list of repeated occurrence offsets.

Example eventual self-describing form:

```regex
(*CAPTURE_HISTORY)
(?x)
(?=
    (?<unit>[ACGT]{L})
    (?:
        (?>
            [ACGT]{0,D}?
            (?<hit>\k<unit>)
        )
    )++
)
```

Treat that as the initial experimental target, not sacred scripture. Benchmark and falsify it aggressively.

---

## 35. Final instruction to the agent

Be skeptical of both regex folklore and cleverness.

Do not assume:

- “regex + backreference = exponential”; or
- “atomic/possessive = automatically safe”; or
- “regex101 warning = formal complexity proof”; or
- “a successful long repeat makes shorter repeats irrelevant.”

Instrument the actual PCRE2 fork, compare against a simple Zig oracle, and measure.

The interesting hypothesis is that a carefully generated **fixed-length** PCRE2 search plus **committed capture history** can make the regex solution both expressive and tractable, while capture history itself becomes a clean general-purpose engine feature.

Prove or kill that hypothesis with tests.
