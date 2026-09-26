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

/* Compare one outcome's rc, ovector and chronological history with the expectation. */
static void check_outcome(const history_case *c, const char *engine, pcre2_match_data *md, int rc)
{
	if (rc != c->expected_rc) {
		fprintf(stderr, "FAIL %s [%s]: rc=%d expected %d\n", c->name, engine, rc, c->expected_rc);
		++failures;
	}
	if (rc > 0) {
		PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
		for (unsigned i = 0; i < c->ovector_pairs * 2; ++i) {
			if (ov[i] != c->ovector[i]) {
				fprintf(stderr, "FAIL %s [%s]: ovector[%u]=%zu expected %zu\n", c->name, engine, i, (size_t)ov[i], (size_t)c->ovector[i]);
				++failures;
			}
		}
	}
	PCRE2_SIZE count = pcre2_get_capture_event_count(md);
	const pcre2_capture_event *ev = pcre2_get_capture_event_pointer(md);
	if (count != c->event_count) {
		fprintf(stderr, "FAIL %s [%s]: event count=%zu expected %u\n", c->name, engine, (size_t)count, c->event_count);
		++failures;
		for (PCRE2_SIZE i = 0; i < count && ev; ++i)
			fprintf(stderr, "  got {%u,%zu,%zu}\n", ev[i].group, (size_t)ev[i].start, (size_t)ev[i].end);
		return;
	}
	for (unsigned i = 0; i < count; ++i) {
		if (ev[i].group != c->events[i].group || ev[i].start != c->events[i].start || ev[i].end != c->events[i].end) {
			fprintf(stderr, "FAIL %s [%s]: event %u={%u,%zu,%zu} expected {%u,%zu,%zu}\n", c->name, engine, i,
				ev[i].group, (size_t)ev[i].start, (size_t)ev[i].end,
				c->events[i].group, (size_t)c->events[i].start, (size_t)c->events[i].end);
			++failures;
		}
	}
}

/* Same case through the JIT: the pattern requests history with the start verb, and
pcre2_jit_match() is called directly so an interpreter fallback cannot mask a gap. */
static void run_case_jit(const history_case *c)
{
	char verb_pattern[300];
	PCRE2_UCHAR pattern[300], subject[256];
	snprintf(verb_pattern, sizeof(verb_pattern), "(*CAPTURE_HISTORY)%s", c->pattern);
	unsigned plen = to_code_units(pattern, verb_pattern);
	unsigned slen = to_code_units(subject, c->subject);
	int error;
	PCRE2_SIZE erroffset;
	pcre2_code *code = pcre2_compile(pattern, plen, 0, &error, &erroffset, NULL);
	if (!code) { fail(c->name, "JIT variant compile failed"); return; }
	int jrc = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
	int returns_captures = strstr(c->pattern, "(?1(") != NULL || strstr(c->pattern, "(?R(") != NULL;
	if (returns_captures) {
		/* Not yet supported in JIT history: must be refused, leaving the interpreter to run it. */
		if (jrc != PCRE2_ERROR_JIT_UNSUPPORTED) {
			fprintf(stderr, "FAIL %s [jit]: jit_compile=%d expected PCRE2_ERROR_JIT_UNSUPPORTED\n", c->name, jrc);
			++failures;
		}
		pcre2_code_free(code);
		return;
	}
	if (jrc != 0) {
		fprintf(stderr, "FAIL %s [jit]: jit_compile=%d\n", c->name, jrc);
		++failures;
		pcre2_code_free(code);
		return;
	}
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	int rc = pcre2_jit_match(code, subject, slen, 0, 0, md, NULL);
	check_outcome(c, "jit", md, rc);
	pcre2_match_data_free(md);
	pcre2_code_free(code);
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
	check_outcome(c, "interpreter", md, pcre2_match(code, subject, slen, 0, c->match_options, md, NULL));
	pcre2_match_data_free(md);
	pcre2_code_free(code);

	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	/* Start-verb cases already choose their own mode; error cases have no JIT outcome to compare. */
	if (have_jit && (c->match_options & PCRE2_CAPTURE_HISTORY) != 0 && strncmp(c->pattern, "(*", 2) != 0
	    && (c->expected_rc > 0 || c->expected_rc == PCRE2_ERROR_NOMATCH))
		run_case_jit(c);
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

/* Resource limits hit while history is enabled report the limit, never a match or no-match. */
static void check_limits(void)
{
	static const struct { const char *name; int (*set)(pcre2_match_context *, uint32_t); uint32_t value; int expected; } limits[] = {
		{ "match limit", pcre2_set_match_limit, 10, PCRE2_ERROR_MATCHLIMIT },
		{ "depth limit", pcre2_set_depth_limit, 10, PCRE2_ERROR_DEPTHLIMIT },
		{ "heap limit", pcre2_set_heap_limit, 1, PCRE2_ERROR_HEAPLIMIT },
	};
	char many[2002];
	memset(many, 'a', 2000);
	many[2000] = 'b';
	many[2001] = 0;
	PCRE2_UCHAR subject[2002];
	unsigned n = to_code_units(subject, many);
	pcre2_code *code = compile_ascii("(a)+b");
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	pcre2_match_context *mc = pcre2_match_context_create(NULL);
	if (!code || !md || !mc) { fail("limits", "setup failed"); return; }
	if (pcre2_match(code, subject, n, 0, PCRE2_CAPTURE_HISTORY, md, mc) != 2 || pcre2_get_capture_event_count(md) != 2000)
		fail("limits", "control match without limits did not record 2000 events");
	for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
		/* Fresh match data: a frame vector grown by an earlier match would hide the heap limit. */
		pcre2_match_data *fresh = pcre2_match_data_create_from_pattern(code, NULL);
		pcre2_match_context *ctx = pcre2_match_context_copy(mc);
		limits[i].set(ctx, limits[i].value);
		match_ascii(code, "aab", PCRE2_CAPTURE_HISTORY, fresh);   /* leave history to go stale */
		int rc = pcre2_match(code, subject, n, 0, PCRE2_CAPTURE_HISTORY | PCRE2_NO_JIT, fresh, ctx);
		if (rc != limits[i].expected) {
			fprintf(stderr, "FAIL %s: rc=%d expected %d\n", limits[i].name, rc, limits[i].expected);
			++failures;
		}
		if (pcre2_get_capture_event_count(fresh) != 0) fail(limits[i].name, "limit error exposed history");
		pcre2_match_context_free(ctx);
		pcre2_match_data_free(fresh);
	}
	pcre2_match_context_free(mc);
	pcre2_match_data_free(md);
	pcre2_code_free(code);
}

/* With JIT available, history requests must use the interpreter or be refused, never run in JIT. */
static void check_jit(void)
{
	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	if (!have_jit) return;
	pcre2_code *code = compile_ascii("(a)+b?");
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	if (!code || !md || pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) != 0) { fail("jit", "setup failed"); return; }
	PCRE2_UCHAR subj[4] = { 'a', 'a', 'a', 0 };
	if (pcre2_match(code, subj, 3, 0, PCRE2_CAPTURE_HISTORY, md, NULL) != 2 || pcre2_get_capture_event_count(md) != 3)
		fail("jit", "pcre2_match on JIT-compiled pattern did not fall back to interpreter history");
	if (pcre2_match(code, subj, 3, 0, 0, md, NULL) != 2 || pcre2_get_capture_event_count(md) != 0)
		fail("jit", "JIT match without history exposed stale history");
	pcre2_match(code, subj, 3, 0, PCRE2_CAPTURE_HISTORY, md, NULL);
	if (pcre2_jit_match(code, subj, 3, 0, PCRE2_CAPTURE_HISTORY, md, NULL) != PCRE2_ERROR_JIT_BADOPTION
	    || pcre2_get_capture_event_count(md) != 0)
		fail("jit", "pcre2_jit_match accepted PCRE2_CAPTURE_HISTORY");
	if (pcre2_jit_match(code, subj, 3, 0, 0, md, NULL) != 2)
		fail("jit", "plain pcre2_jit_match no longer works");
	pcre2_match_data_free(md);
	pcre2_code_free(code);
}

/* A pattern that requests history must be refused by modes that cannot honor it. */
static void check_verb_modes(void)
{
	pcre2_code *code = compile_ascii("(*CAPTURE_HISTORY)(a)+b?");
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	if (!code || !md) { fail("verb", "setup failed"); return; }
	PCRE2_UCHAR subj[4] = { 'a', 'a', 'a', 0 };
	int workspace[64];
	if (pcre2_match(code, subj, 3, 0, PCRE2_PARTIAL_SOFT, md, NULL) != PCRE2_ERROR_BADOPTION)
		fail("verb", "partial matching accepted a history pattern");
	if (pcre2_dfa_match(code, subj, 3, 0, 0, md, NULL, workspace, 64) != PCRE2_ERROR_BADOPTION)
		fail("verb", "DFA matcher accepted a history pattern");
	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	if (have_jit) {
		if (pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) != 0) fail("verb", "JIT compile failed");
		if (pcre2_match(code, subj, 3, 0, 0, md, NULL) != 2 || pcre2_get_capture_event_count(md) != 3)
			fail("verb", "pcre2_match on a JIT-compiled history pattern lost history");
		if (pcre2_jit_match(code, subj, 3, 0, 0, md, NULL) != 2 || pcre2_get_capture_event_count(md) != 3)
			fail("verb", "pcre2_jit_match did not record history for a (*CAPTURE_HISTORY) pattern");
		if (pcre2_jit_match(code, subj, 3, 0, PCRE2_PARTIAL_HARD, md, NULL) != PCRE2_ERROR_JIT_BADOPTION)
			fail("verb", "partial JIT matching accepted a history pattern");
	}
	pcre2_match_data_free(md);
	pcre2_code_free(code);
}

static int match_with_heap_limit(pcre2_code *code, PCRE2_UCHAR *subject, unsigned n, uint32_t opts, uint32_t kib)
{
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	pcre2_match_context *mc = pcre2_match_context_create(NULL);
	pcre2_set_heap_limit(mc, kib);
	int rc = pcre2_match(code, subject, n, 0, opts, md, mc);
	pcre2_match_context_free(mc);
	pcre2_match_data_free(md);
	return rc;
}

/* History memory counts against the heap limit. The threshold is found by search,
so the test does not depend on frame sizes, which differ by width and platform.
"(a)+b" grows frames with events; "(a)++b" keeps frames flat while events grow. */
static void check_heap_accounting_for(const char *pattern)
{
	enum { N = 2000 };
	static char many[N + 2];
	memset(many, 'a', N);
	many[N] = 'b';
	many[N + 1] = 0;
	static PCRE2_UCHAR subject[N + 2];
	unsigned n = to_code_units(subject, many);
	pcre2_code *code = compile_ascii(pattern);
	if (!code) { fail("heap accounting", "setup failed"); return; }
	uint32_t kib = 1;
	while (kib < 1u << 20 && match_with_heap_limit(code, subject, n, 0, kib) != 2) ++kib;
	uint32_t history_kib = (uint32_t)((N * sizeof(pcre2_capture_event) + 1023) / 1024);
	if (match_with_heap_limit(code, subject, n, 0, kib - 1) != PCRE2_ERROR_HEAPLIMIT)
		fail("heap accounting", "search did not find the frame threshold");
	int rc = match_with_heap_limit(code, subject, n, PCRE2_CAPTURE_HISTORY, kib);
	if (rc != PCRE2_ERROR_HEAPLIMIT) {
		fprintf(stderr, "FAIL heap accounting %s: history at frame-only limit %u KiB gave rc=%d\n", pattern, kib, rc);
		++failures;
	}
	/* Doubling growth may round history capacity up to twice the events needed. */
	rc = match_with_heap_limit(code, subject, n, PCRE2_CAPTURE_HISTORY, kib + 2 * history_kib + 1);
	if (rc != 2) {
		fprintf(stderr, "FAIL heap accounting %s: history with room for events gave rc=%d\n", pattern, rc);
		++failures;
	}
	pcre2_code_free(code);
}

static void check_heap_accounting(void)
{
	check_heap_accounting_for("(a)+b");
	check_heap_accounting_for("(a)++b");

	/* JIT history storage honours the heap limit too (the JIT has no heap frames). */
	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	if (!have_jit) return;
	enum { N = 2000 };
	static char many[N + 2];
	memset(many, 'a', N);
	many[N] = 'b';
	many[N + 1] = 0;
	static PCRE2_UCHAR subject[N + 2];
	unsigned n = to_code_units(subject, many);
	pcre2_code *code = compile_ascii("(*CAPTURE_HISTORY)(a)++b");
	if (!code || pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) != 0) { fail("jit heap", "setup failed"); return; }
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	pcre2_match_context *mc = pcre2_match_context_create(NULL);
	uint32_t need_kib = (uint32_t)((N * sizeof(pcre2_capture_event) + 1023) / 1024);
	pcre2_set_heap_limit(mc, need_kib / 2);
	int rc = pcre2_jit_match(code, subject, n, 0, 0, md, mc);
	if (rc != PCRE2_ERROR_HEAPLIMIT || pcre2_get_capture_event_count(md) != 0) {
		fprintf(stderr, "FAIL jit heap: half the needed limit gave rc=%d\n", rc);
		++failures;
	}
	pcre2_match_data_free(md);
	md = pcre2_match_data_create_from_pattern(code, NULL);
	pcre2_set_heap_limit(mc, 2 * need_kib + 1);
	rc = pcre2_jit_match(code, subject, n, 0, 0, md, mc);
	if (rc != 2 || pcre2_get_capture_event_count(md) != N) {
		fprintf(stderr, "FAIL jit heap: ample limit gave rc=%d events=%zu\n", rc, (size_t)pcre2_get_capture_event_count(md));
		++failures;
	}
	pcre2_match_context_free(mc);
	pcre2_match_data_free(md);
	pcre2_code_free(code);
}

/* Match once with a capture-history event limit; report rc and the published event count. */
static int match_with_event_limit(const char *pattern, const char *subj, uint32_t opts, uint32_t limit, int jit,
                                  PCRE2_SIZE *count)
{
	pcre2_code *code = compile_ascii(pattern);
	if (!code || (jit && pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) != 0)) { pcre2_code_free(code); return 1000; }
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
	pcre2_match_context *mc = pcre2_match_context_create(NULL);
	pcre2_set_capture_history_limit(mc, limit);
	static PCRE2_UCHAR buf[64];
	unsigned n = to_code_units(buf, subj);
	int rc = jit ? pcre2_jit_match(code, buf, n, 0, opts, md, mc) : pcre2_match(code, buf, n, 0, opts | PCRE2_NO_JIT, md, mc);
	*count = pcre2_get_capture_event_count(md);
	pcre2_match_context_free(mc);
	pcre2_match_data_free(md);
	pcre2_code_free(code);
	return rc;
}

/* The per-match event limit (default UINT32_MAX, so a count always fits in uint32_t)
bounds the events on the current path. Reaching it aborts the match, like the other
resource limits; events discarded by backtracking do not count. */
static void check_event_limit(void)
{
	static const struct { const char *name, *pattern, *subject; uint32_t opts, limit; int rc; PCRE2_SIZE count; } cases[] = {
		{ "limit equal to the events needed", "(*CAPTURE_HISTORY)(a)+", "aaaa", 0, 4, 2, 4 },
		{ "limit one below the events needed", "(*CAPTURE_HISTORY)(a)+", "aaaa", 0, 3, PCRE2_ERROR_CAPTURE_HISTORY_LIMIT, 0 },
		{ "zero limit with a capture", "(*CAPTURE_HISTORY)(a)", "a", 0, 0, PCRE2_ERROR_CAPTURE_HISTORY_LIMIT, 0 },
		{ "zero limit without captures", "(*CAPTURE_HISTORY)a", "a", 0, 0, 1, 0 },
		{ "backtracked events do not count", "(*CAPTURE_HISTORY)(?:(a)(a)(a)x|(a))", "aaa", 0, 3, 5, 1 },
		{ "path peak above the limit aborts", "(*CAPTURE_HISTORY)(?:(a)(a)(a)x|(a))", "aaa", 0, 2, PCRE2_ERROR_CAPTURE_HISTORY_LIMIT, 0 },
		{ "limit ignored without history", "(a)+", "aaaa", 0, 0, 2, 0 },
	};
	pcre2_match_context *mc = pcre2_match_context_create(NULL);
	uint32_t got = 0;
	if (!mc || pcre2_get_capture_history_limit(mc, &got) != 0 || got != UINT32_MAX)
		fail("event limit", "default limit is not UINT32_MAX");
	pcre2_set_capture_history_limit(mc, 7);
	if (pcre2_get_capture_history_limit(mc, &got) != 0 || got != 7) fail("event limit", "set/get round trip");
	pcre2_match_context_free(mc);

	PCRE2_UCHAR message[128];
	if (pcre2_get_error_message(PCRE2_ERROR_CAPTURE_HISTORY_LIMIT, message, 128) <= 0)
		fail("event limit", "no error message for PCRE2_ERROR_CAPTURE_HISTORY_LIMIT");

	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	for (int jit = 0; jit <= (int)have_jit; ++jit)
		for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
			PCRE2_SIZE count = 99;
			int rc = match_with_event_limit(cases[i].pattern, cases[i].subject, cases[i].opts, cases[i].limit, jit, &count);
			if (rc != cases[i].rc || count != cases[i].count) {
				fprintf(stderr, "FAIL event limit (%s) %s: rc=%d count=%zu, expected rc=%d count=%zu\n", jit ? "jit" : "interpreter",
					cases[i].name, rc, (size_t)count, cases[i].rc, (size_t)cases[i].count);
				++failures;
			}
		}
}

typedef struct { uint32_t last[32]; unsigned n; } callout_log;

static int log_capture_last(pcre2_callout_block *cb, void *data)
{
	callout_log *log = data;
	if (log->n < 32) log->last[log->n++] = cb->capture_last;
	return 0;
}

/* Callouts see the same capture_last from both engines when history is on,
including inside a subroutine call, where the JIT keeps the slot as an event count. */
static void check_callout_capture_last(void)
{
	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	if (!have_jit) return;
	static const char *patterns[] = {
		"(*CAPTURE_HISTORY)(a)(?C1)(b)(?C2)",
		"(*CAPTURE_HISTORY)(a(?C1))(b)(?1)(?C2)",
		"(*CAPTURE_HISTORY)(a)((?C1)b)+(?C2)",
		"(*CAPTURE_HISTORY)((a)(?C1))(b)(?1)(?C2)",
	};
	static const char *subjects[] = { "ab", "aba", "abb", "aba" };
	for (unsigned i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
		pcre2_code *code = compile_ascii(patterns[i]);
		pcre2_code *jit = code ? pcre2_code_copy(code) : NULL;
		if (!code || !jit || pcre2_jit_compile(jit, PCRE2_JIT_COMPLETE) != 0) { fail("callout", "setup failed"); continue; }
		pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
		pcre2_match_context *mc = pcre2_match_context_create(NULL);
		callout_log interp = { {0}, 0 }, jitted = { {0}, 0 };
		PCRE2_UCHAR subj[8];
		unsigned n = to_code_units(subj, subjects[i]);
		pcre2_set_callout(mc, log_capture_last, &interp);
		int ri = pcre2_match(code, subj, n, 0, PCRE2_NO_JIT, md, mc);
		pcre2_set_callout(mc, log_capture_last, &jitted);
		int rj = pcre2_jit_match(jit, subj, n, 0, 0, md, mc);
		if (ri != rj || interp.n != jitted.n || interp.n == 0 || memcmp(interp.last, jitted.last, interp.n * sizeof(uint32_t)) != 0) {
			fprintf(stderr, "FAIL callout capture_last %s: interp rc=%d n=%u, jit rc=%d n=%u\n", patterns[i], ri, interp.n, rj, jitted.n);
			for (unsigned k = 0; k < interp.n || k < jitted.n; ++k)
				fprintf(stderr, "  [%u] interp=%u jit=%u\n", k, k < interp.n ? interp.last[k] : 999u, k < jitted.n ? jitted.last[k] : 999u);
			++failures;
		}
		pcre2_match_context_free(mc);
		pcre2_match_data_free(md);
		pcre2_code_free(code);
		pcre2_code_free(jit);
	}
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
		{ "ACCEPT after a completed nested assertion records the tail once", "(?=(?=a)a((*ACCEPT)))", "ab",
			PCRE2_CAPTURE_HISTORY, 2, { 0, 0, 1, 1 }, 2, { {1, 1, 1} }, 1 },
		{ "ACCEPT after a nested capturing assertion keeps each close once", "(?=(?=(a))a((b)(*ACCEPT)))", "ab",
			PCRE2_CAPTURE_HISTORY, 4, { 0, 0, 0, 1, 1, 2, 1, 2 }, 4, { {1, 0, 1}, {3, 1, 2}, {2, 1, 2} }, 3 },
		{ "ACCEPT after a completed scan-substring assertion records the tail once", "(aa)(?=(*scs:(1)a)b((*ACCEPT)))", "aab",
			PCRE2_CAPTURE_HISTORY, 3, { 0, 2, 0, 2, 3, 3 }, 3, { {1, 0, 2}, {2, 3, 3} }, 2 },
		{ "ACCEPT after a completed non-atomic assertion records the tail once", "(?=(*napla:a)a((*ACCEPT)))", "ab",
			PCRE2_CAPTURE_HISTORY, 2, { 0, 0, 1, 1 }, 2, { {1, 1, 1} }, 1 },
		{ "conditional assertion captures survive", "(?(?=(a))a|b)", "a", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 1, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "subroutine-call captures are not recorded", "(a)(?1)", "aa", PCRE2_CAPTURE_HISTORY, 2,
			{ 0, 2, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "captures nested inside a subroutine call are not recorded", "(b(a))(?1)", "baba", PCRE2_CAPTURE_HISTORY, 3,
			{ 0, 4, 0, 2, 1, 2 }, 3, { {2, 1, 2}, {1, 0, 2} }, 2 },
		{ "subroutine returning a capture records it when the call returns", "(c(a|b))(?1(2))", "cacb",
			PCRE2_CAPTURE_HISTORY, 3, { 0, 4, 0, 2, 3, 4 }, 3, { {2, 1, 2}, {1, 0, 2}, {2, 3, 4} }, 3 },
		{ "returned group left unset by the call records nothing", "((b)?a)(?1(2))", "aa",
			PCRE2_CAPTURE_HISTORY, 2, { 0, 2, 0, 1 }, 2, { {1, 0, 1} }, 1 },
		{ "unset returned group below a set group records nothing", "(x(b)?(a))(?1(2))", "xaxa",
			PCRE2_CAPTURE_HISTORY, 4, { 0, 4, 0, 2, PCRE2_UNSET, PCRE2_UNSET, 1, 2 }, 4, { {3, 1, 2}, {1, 0, 2} }, 2 },
		{ "several returned groups are recorded in group order", "(c(a)(b))(?1(3,2))", "cabcab",
			PCRE2_CAPTURE_HISTORY, 4, { 0, 6, 0, 3, 4, 5, 5, 6 }, 4, { {2, 1, 2}, {3, 2, 3}, {1, 0, 3}, {2, 4, 5}, {3, 5, 6} }, 5 },
		{ "subroutine returning captures still works without history", "(c(a|b))(?1(2))", "cacb", 0, 3,
			{ 0, 4, 0, 2, 3, 4 }, 3, { {0, 0, 0} }, 0 },
		{ "pattern-start verb enables history without the match option", "(*CAPTURE_HISTORY)(a)+", "aaa", 0, 2,
			{ 0, 3, 2, 3 }, 2, { {1, 0, 1}, {1, 1, 2}, {1, 2, 3} }, 3 },
		{ "verb combines with other start verbs", "(*NO_JIT)(*CAPTURE_HISTORY)(*UTF)(a)+", "aa", 0, 2,
			{ 0, 2, 1, 2 }, 2, { {1, 0, 1}, {1, 1, 2} }, 2 },
		{ "whole-pattern recursion returning a capture records it on return", "c(a|b)(?:$|(?R(1)))", "cacb",
			PCRE2_CAPTURE_HISTORY, 2, { 0, 4, 3, 4 }, 2, { {1, 1, 2}, {1, 3, 4} }, 2 },
		{ "whole-pattern recursion returning captures works without history", "c(a|b)(?:$|(?R(1)))", "cacb", 0, 2,
			{ 0, 4, 3, 4 }, 2, { {0, 0, 0} }, 0 },
	};
	for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) run_case(&cases[i]);
	check_lifecycle();
	check_limits();
	check_heap_accounting();
	check_event_limit();
	check_callout_capture_last();
	check_jit();
	check_verb_modes();
	if (failures) fprintf(stderr, "%u capture-history failure(s)\n", failures);
	return failures != 0;
}
