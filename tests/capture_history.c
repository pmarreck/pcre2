/* Verify committed capture-close history returned by the interpreter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>

#define MAX_EVENTS 16

typedef struct {
	uint32_t group;
	PCRE2_SIZE start;
	PCRE2_SIZE end;
} expected_event;

typedef struct {
	const char *name;
	const char *pattern;
	const char *subject;
	uint32_t match_options;
	int expected_rc;
	/* Ordinary ovector pairs, including group 0; PCRE2_UNSET where unset. */
	PCRE2_SIZE ovector[8];
	unsigned ovector_pairs;
	expected_event events[MAX_EVENTS];
	unsigned event_count;
} history_case;

static unsigned failures = 0;

static unsigned to_code_units(PCRE2_UCHAR *out, const char *in)
{
	unsigned n = 0;
	for (; in[n]; ++n) out[n] = (unsigned char)in[n];
	out[n] = 0;
	return n;
}

static void fail(const char *name, const char *why)
{
	fprintf(stderr, "FAIL %s: %s\n", name, why);
	++failures;
}

/* Compare ovector and chronological history against a hand-derived expectation. */
static void run_case(const history_case *c)
{
	PCRE2_UCHAR pattern[256], subject[256];
	unsigned plen = to_code_units(pattern, c->pattern);
	unsigned slen = to_code_units(subject, c->subject);
	int error;
	PCRE2_SIZE erroffset;
	pcre2_code *code = pcre2_compile(pattern, plen, 0, &error, &erroffset, NULL);
	if (!code) { fail(c->name, "compile failed"); return; }
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	if (!md) { fail(c->name, "match data allocation failed"); pcre2_code_free(code); return; }

	int rc = pcre2_match(code, subject, slen, 0, c->match_options, md, NULL);
	if (rc != c->expected_rc) {
		fprintf(stderr, "FAIL %s: rc=%d expected %d\n", c->name, rc, c->expected_rc);
		++failures;
	}
	if (rc > 0) {
		PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
		for (unsigned i = 0; i < c->ovector_pairs * 2; ++i) {
			if (ov[i] != c->ovector[i]) {
				fprintf(stderr, "FAIL %s: ovector[%u]=%zu expected %zu\n", c->name, i, (size_t)ov[i], (size_t)c->ovector[i]);
				++failures;
			}
		}
	}

	PCRE2_SIZE count = pcre2_get_capture_event_count(md);
	const pcre2_capture_event *ev = pcre2_get_capture_event_pointer(md);
	if (count != c->event_count) {
		fprintf(stderr, "FAIL %s: event count=%zu expected %u\n", c->name, (size_t)count, c->event_count);
		++failures;
		for (PCRE2_SIZE i = 0; i < count && ev; ++i)
			fprintf(stderr, "  got {%u,%zu,%zu}\n", ev[i].group, (size_t)ev[i].start, (size_t)ev[i].end);
	} else {
		for (unsigned i = 0; i < count; ++i) {
			if (ev[i].group != c->events[i].group || ev[i].start != c->events[i].start || ev[i].end != c->events[i].end) {
				fprintf(stderr, "FAIL %s: event %u={%u,%zu,%zu} expected {%u,%zu,%zu}\n", c->name, i,
					ev[i].group, (size_t)ev[i].start, (size_t)ev[i].end,
					c->events[i].group, (size_t)c->events[i].start, (size_t)c->events[i].end);
				++failures;
			}
		}
	}
	pcre2_match_data_free(md);
	pcre2_code_free(code);
}

typedef struct { unsigned allocations_left; unsigned allocations; } alloc_budget;

static void *budget_malloc(size_t size, void *data)
{
	alloc_budget *b = data;
	if (b->allocations_left == 0) return NULL;
	--b->allocations_left;
	++b->allocations;
	return malloc(size);
}

static void budget_free(void *p, void *data) { (void)data; free(p); }

static pcre2_code *compile_ascii(const char *pat)
{
	PCRE2_UCHAR buf[256];
	unsigned n = to_code_units(buf, pat);
	int error;
	PCRE2_SIZE erroffset;
	return pcre2_compile(buf, n, 0, &error, &erroffset, NULL);
}

static int match_ascii(pcre2_code *code, const char *subj, uint32_t options, pcre2_match_data *md)
{
	static PCRE2_UCHAR buf[512];
	unsigned n = to_code_units(buf, subj);
	return pcre2_match(code, buf, n, 0, options, md, NULL);
}

/* Match-data reuse, execution-mode interaction, growth and allocator failure. */
static void check_lifecycle(void)
{
	pcre2_code *code = compile_ascii("(a)+b?");
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	if (!code || !md) { fail("lifecycle", "setup failed"); return; }

	if (match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md) != 2 || pcre2_get_capture_event_count(md) != 3)
		fail("reuse", "priming match did not record three events");
	if (match_ascii(code, "zzz", PCRE2_CAPTURE_HISTORY, md) != PCRE2_ERROR_NOMATCH || pcre2_get_capture_event_count(md) != 0)
		fail("reuse", "failed match exposed stale history");
	match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md);
	if (match_ascii(code, "aa", 0, md) != 2 || pcre2_get_capture_event_count(md) != 0)
		fail("reuse", "match without history exposed stale history");
	match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md);
	if (match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY | PCRE2_PARTIAL_HARD, md) != PCRE2_ERROR_BADOPTION || pcre2_get_capture_event_count(md) != 0)
		fail("partial", "partial matching with history was not rejected");
	if (match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY | PCRE2_PARTIAL_SOFT, md) != PCRE2_ERROR_BADOPTION)
		fail("partial", "soft partial matching with history was not rejected");

	match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md);
	{
		PCRE2_UCHAR subj[4] = { 'a', 'a', 'a', 0 };
		int workspace[64];
		int rc = pcre2_dfa_match(code, subj, 3, 0, PCRE2_CAPTURE_HISTORY, md, NULL, workspace, 64);
		if (rc != PCRE2_ERROR_BADOPTION) fail("dfa", "DFA matcher accepted PCRE2_CAPTURE_HISTORY");
		match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md);
		rc = pcre2_dfa_match(code, subj, 3, 0, 0, md, NULL, workspace, 64);
		if (rc < 0) fprintf(stderr, "FAIL dfa: plain DFA match rc=%d\n", rc), ++failures;
		if (pcre2_get_capture_event_count(md) != 0) fail("dfa", "DFA match exposed stale history");
		match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, md);
		pcre2_jit_match(code, subj, 3, 0, 0, md, NULL);
		if (pcre2_get_capture_event_count(md) != 0) fail("jit", "JIT entry point exposed stale history");
	}

	/* Growth well past the initial capacity must preserve earlier events. */
	{
		char many[301];
		memset(many, 'a', 300);
		many[300] = 0;
		int rc = match_ascii(code, many, PCRE2_CAPTURE_HISTORY, md);
		const pcre2_capture_event *ev = pcre2_get_capture_event_pointer(md);
		PCRE2_SIZE count = pcre2_get_capture_event_count(md);
		if (rc != 2 || count != 300) fail("growth", "wrong event count after growth");
		else for (unsigned i = 0; i < 300; ++i)
			if (ev[i].group != 1 || ev[i].start != i || ev[i].end != i + 1) { fail("growth", "event corrupted by growth"); break; }
	}
	pcre2_match_data_free(md);

	/* History storage uses the match data's allocator; exhaustion is an error, not a no-match. */
	{
		alloc_budget budget = { 100, 0 };
		pcre2_general_context *gc = pcre2_general_context_create(budget_malloc, budget_free, &budget);
		pcre2_match_data *bmd = gc ? pcre2_match_data_create(4, gc) : NULL;
		if (!bmd) { fail("allocator", "setup failed"); pcre2_general_context_free(gc); pcre2_code_free(code); return; }
		int rc = match_ascii(code, "aaa", 0, bmd);
		if (rc != 2) fail("allocator", "baseline match with budget allocator failed");
		unsigned before = budget.allocations;
		rc = match_ascii(code, "aaa", 0, bmd);
		if (budget.allocations != before) fail("allocator", "disabled history allocated memory on reuse");
		budget.allocations_left = 0;
		rc = match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, bmd);
		if (rc != PCRE2_ERROR_NOMEMORY) {
			fprintf(stderr, "FAIL allocator: rc=%d expected PCRE2_ERROR_NOMEMORY\n", rc);
			++failures;
		}
		if (pcre2_get_capture_event_count(bmd) != 0) fail("allocator", "failed allocation exposed history");
		budget.allocations_left = 100;
		rc = match_ascii(code, "aaa", PCRE2_CAPTURE_HISTORY, bmd);
		if (rc != 2 || pcre2_get_capture_event_count(bmd) != 3) fail("allocator", "recovery after allocation failure");
		pcre2_match_data_free(bmd);
		pcre2_general_context_free(gc);
	}
	pcre2_code_free(code);
}

int main(void)
{
	static const history_case cases[] = {
		{ "repeated group records every iteration", "(a)+", "aaa", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 3, 2, 3 }, 2, { {1, 0, 1}, {1, 1, 2}, {1, 2, 3} }, 3 },
		{ "speculative greedy capture rolls back", "(a+)(a)", "aaa", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 3, 0, 2, 2, 3 }, 3, { {1, 0, 2}, {2, 2, 3} }, 2 },
		{ "abandoned loop iteration rolls back", "(a)+ab", "aaab", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 4, 1, 2 }, 2, { {1, 0, 1}, {1, 1, 2} }, 2 },
		{ "failed alternative rolls back", "(?:(a)x|(a)y)", "ay", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 2, PCRE2_UNSET, PCRE2_UNSET, 0, 1 }, 3, { {2, 0, 1} }, 1 },
		{ "positive lookahead captures survive in close order", "(?=((a)+))", "aaa", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 0, 0, 3, 2, 3 }, 3, { {2, 0, 1}, {2, 1, 2}, {2, 2, 3}, {1, 0, 3} }, 4 },
		{ "negative lookahead captures do not survive", "(?!(a)b)(a)", "ac", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 1, PCRE2_UNSET, PCRE2_UNSET, 0, 1 }, 3, { {2, 0, 1} }, 1 },
		{ "atomic group keeps committed captures", "(?>(a)+)b", "aab", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 3, 1, 2 }, 2, { {1, 0, 1}, {1, 1, 2} }, 2 },
		{ "possessive capturing repeat", "(a|b)++c", "abc", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 3, 1, 2 }, 2, { {1, 0, 1}, {1, 1, 2} }, 2 },
		{ "no match exposes no history", "(a)+b", "aaa", PCRE2_CAPTURE_HISTORY, PCRE2_ERROR_NOMATCH,
			{ 0 }, 0, { {0, 0, 0} }, 0 },
		{ "history disabled by default", "(a)+", "aaa", 0, 2,
			{ 0, 3, 2, 3 }, 2, { {0, 0, 0} }, 0 },
		{ "bounded-gap repeated backreference chain",
			"(?=(?<unit>[ACGT]{4})(?:(?>[ACGT]{0,10}?(?<hit>\\k<unit>)))++)",
			"ACGTTACGTTTACGTCCCCCCCCCCCCACGT", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 0, 0, 4, 11, 15 }, 3, { {1, 0, 4}, {2, 5, 9}, {2, 11, 15} }, 3 },
		{ "nested groups report capture-close order", "((a)b)", "ab", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 2, 0, 2, 0, 1 }, 3, { {2, 0, 1}, {1, 0, 2} }, 2 },
		{ "lazy repeat keeps only the iterations taken", "(a)+?b", "aab", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 3, 1, 2 }, 2, { {1, 0, 1}, {1, 1, 2} }, 2 },
		{ "ACCEPT closes open groups", "(a(*ACCEPT))b", "ac", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 1, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "ACCEPT inside lookahead keeps assertion captures", "(?=(a)(*ACCEPT)b)(a)", "ac", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 1, 0, 1, 0, 1 }, 3, { {1, 0, 1}, {2, 0, 1} }, 2 },
		{ "conditional assertion captures survive", "(?(?=(a))a|b)", "a", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 1, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "subroutine-call captures are not recorded", "(a)(?1)", "aa", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 2, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "captures nested inside a subroutine call are not recorded", "(b(a))(?1)", "baba", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 4, 0, 2, 1, 2 }, 3, { {2, 1, 2}, {1, 0, 2} }, 2 },
		{ "subroutine returning captures is rejected, not silently incomplete", "(c(a|b))(?1(2))", "cacb",
			PCRE2_CAPTURE_HISTORY, PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED, { 0 }, 0, { {0, 0, 0} }, 0 },
		{ "subroutine returning captures still works without history", "(c(a|b))(?1(2))", "cacb", 0, 3,
			{ 0, 4, 0, 2, 3, 4 }, 3, { {0, 0, 0} }, 0 },
		{ "whole-pattern recursion returning captures is rejected", "c(a|b)(?:$|(?R(1)))", "cacb",
			PCRE2_CAPTURE_HISTORY, PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED, { 0 }, 0, { {0, 0, 0} }, 0 },
		{ "whole-pattern recursion returning captures works without history", "c(a|b)(?:$|(?R(1)))", "cacb", 0, 2,
			{ 0, 4, 3, 4 }, 2, { {0, 0, 0} }, 0 },
	};
	for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) run_case(&cases[i]);
	check_lifecycle();
	{
		PCRE2_UCHAR msg[256];
		if (pcre2_get_error_message(PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED, msg, 256) <= 0)
			fail("error text", "PCRE2_ERROR_CAPTURE_HISTORY_UNSUPPORTED has no message");
	}
	if (failures) fprintf(stderr, "%u capture-history failure(s)\n", failures);
	return failures != 0;
}
