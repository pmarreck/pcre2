# Repeat-family semantics when occurrences overlap

Status: open decision for Peter. The engine feature does not depend on it.

## The conflict in the handoff

The handoff defines families with `gap = next - (cur + L)` and `0 <= gap <= D`
(section 2.1), so members never overlap. Two other sections disagree with that
once a unit can overlap itself:

- Section 10 aggregates by unioning every offset the fixed-L regex reports from
  every start, then splits only where `gap > D`. Negative gaps never split.
- Section 26 describes the brute-force oracle as "find all exact occurrences",
  which includes overlapping ones.

## Candidate rules

`experiments/dna_repeats/src/families.zig` implements five rules as plain
quadratic scans:

| Rule | Definition |
| --- | --- |
| `all_occurrences` | Every exact occurrence, overlaps included, split where gap > D (section 26) |
| `regex_union` | Union of the unit and hit offsets the regex reports from every start, split where gap > D (section 10) |
| `maximal_chains` | Each start's nearest-next non-overlapping chain, minus chains that are suffixes of another |
| `greedy_packing` | Leftmost greedy non-overlapping selection of occurrences, split where gap > D |
| `chain_packing` | Regex chains accepted in start order, skipping any start inside an accepted member |

Hand-derived examples, all pinned as tests:

| Subject, L, D | all_occurrences | regex_union | maximal_chains | greedy_packing | chain_packing |
| --- | --- | --- | --- | --- | --- |
| `AAAAAA`, 2, 0 | `[0,1,2,3,4]` | `[0,1,2,3,4]` | `[0,2,4]`, `[1,3]` | `[0,2,4]` | `[0,2,4]` |
| `AAA`, 2, 0 | `[0,1]` | none | none | none | none |
| `AAACAA`, 2, 1 | `[0,1,4]` | `[1,4]` | `[1,4]` | none | `[1,4]` |
| Handoff 2.2 (no overlaps) | `[100,180]`, `[900,980]` | same | same | same | same |

In `AAACAA`, leftmost packing takes 0, then 4 with gap 2 > D, and reports
nothing. The chain from 1 reaches 4 with gap 1, a valid non-overlapping family.
Packing that ignores D when choosing members can lose families.

## Measured

`zig build explore` enumerates every subject up to a length and every `L < n`:

| Corpus | Cases | Overlapping members in `all_occurrences` / `regex_union` | Disagreements without overlapping members |
| --- | --- | --- | --- |
| `{A,C}`, n <= 10, D 0..2 | 49,164 | 11,946 / 1,678 | 0 |
| `{A,C,G,T}`, n <= 7, D 0..2 | 371,376 | 20,916 / 696 | 0 |

`greedy_packing` and `chain_packing` disagree in 352 of the `{A,C}` cases and
132 of the `{A,C,G,T}` cases, first at `AAACAA`. The last column is also a
test: exhaustive over `{A,C}` up to length 8, the
five rules coincide whenever `all_occurrences` has no overlapping members. The
ambiguity is only about overlap.

The chain model is checked against the real engine. A differential test runs
the section 8.1 regex with capture history, anchored at every start of every
`{A,C}` subject up to length 9, for L 1..4 and D 0..2. It compares the unit
and hit offsets with the model's chain: more than 10,000 comparisons, no
mismatches. A mutant that excludes gap == D fails it at once.

## Options

- **A. Non-overlapping, one chain per region (`chain_packing`).** Honors
  section 2.1's `0 <= gap`. It comes straight from regex chains: take them in
  start order and skip a start that falls inside an accepted member. `AAAAAA`
  gives `[0,2,4]`, and `AAACAA` gives `[1,4]`. A skipped chain can start inside
  an accepted one and still reach further. I have not measured how often that
  happens.
- **B. Non-overlapping, every maximal chain (`maximal_chains`).** Also honors
  `0 <= gap`, but reports interleaved chains `[0,2,4]` and `[1,3]` over the
  same bases as separate families.
- **C. Overlaps count (`all_occurrences`).** The classic maximal-repeat notion.
  It contradicts section 2.1, and the non-overlapping regex cannot produce it:
  `AAA` has a family the regex never reports. Section 32 lists overlapping
  semantics as a non-goal for the first pass.
- **D. Section 10 as written (`regex_union`).** Produces families that break
  section 2.1 (`[0,1,2,3,4]` for `AAAAAA`).
- Rejected: `greedy_packing`, because of the `AAACAA` counterexample.

Recommendation: A, or B if interleaved families are wanted. Both are
consistent with section 2.1 and come directly from the regex output. Maximality
(left/right contexts per family) is not evaluated here yet; it applies after a
family rule is chosen.
