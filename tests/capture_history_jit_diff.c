/* Differential check: interpreter vs JIT capture history on seeded random patterns. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>

/* Default keeps ./test fast; ./fuzz raises it through CAPTURE_HISTORY_DIFF_CASES. */
#define DEFAULT_CASES 3000
#define SUBJECTS_PER_PATTERN 8

static unsigned long long rng_state = 0x9e3779b97f4a7c15ull;

/* splitmix64: deterministic, seedable, no global libc state. */
static unsigned rnd(unsigned n)
{
	unsigned long long z = (rng_state += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	z ^= z >> 31;
	return (unsigned)(z % n);
}

typedef struct { char *p; size_t n, cap; unsigned groups; } buf;

static void put(buf *b, const char *s)
{
	size_t l = strlen(s);
	if (b->n + l + 1 > b->cap) return;
	memcpy(b->p + b->n, s, l);
	b->n += l;
	b->p[b->n] = 0;
}

static void quant(buf *b)
{
	static const char *q[] = { "", "", "*", "+", "?", "{1,2}", "*?", "+?", "??", "*+", "++", "?+" };
	put(b, q[rnd(sizeof(q) / sizeof(q[0]))]);
}

/* Random regex over {a,b} with captures, alternation, quantifiers, atomic groups,
lookarounds, backreferences, ACCEPT and non-returning subroutine calls. */
static void gen(buf *b, int depth)
{
	int items = 1 + rnd(3);
	for (int i = 0; i < items; ++i) {
		unsigned k = depth > 3 ? rnd(3) : rnd(12);
		switch (k) {
		case 0: put(b, "a"); quant(b); break;
		case 1: put(b, "b"); quant(b); break;
		case 2: put(b, "[ab]"); quant(b); break;
		case 3: case 4: case 5:
			b->groups++;
			put(b, "(");
			gen(b, depth + 1);
			if (rnd(3) == 0) { put(b, "|"); gen(b, depth + 1); }
			put(b, ")");
			quant(b);
			break;
		case 6: put(b, "(?:"); gen(b, depth + 1); put(b, "|"); gen(b, depth + 1); put(b, ")"); quant(b); break;
		case 7: put(b, "(?>"); gen(b, depth + 1); put(b, ")"); quant(b); break;
		case 8: {
			static const char *look[] = { "(?=", "(?!", "(?<=", "(?<!" };
			const char *l = look[rnd(4)];
			put(b, l);
			if (l[2] == '<') { if (rnd(2)) { put(b, "(a)"); b->groups++; } else put(b, "b"); }
			else gen(b, depth + 1);
			put(b, ")");
			break;
		}
		case 9:
			if (b->groups > 0) { char r[16]; snprintf(r, sizeof(r), "\\%u", 1 + rnd(b->groups)); put(b, r); }
			break;
		case 10: if (rnd(8) == 0) put(b, "(*ACCEPT)"); break;
		case 11:
			if (b->groups > 0 && rnd(4) == 0) { char r[16]; snprintf(r, sizeof(r), "(?%u)", 1 + rnd(b->groups)); put(b, r); }
			break;
		}
	}
}

/* rc and ovector only: what an ordinary (history-free) caller observes. */
static int same_ovector(pcre2_match_data *x, int rx, pcre2_match_data *y, int ry)
{
	if (rx != ry) return 0;
	if (rx > 0) {
		PCRE2_SIZE *ox = pcre2_get_ovector_pointer(x), *oy = pcre2_get_ovector_pointer(y);
		for (int i = 0; i < 2 * rx; ++i) if (ox[i] != oy[i]) return 0;
	}
	return 1;
}

static int same_result(pcre2_match_data *x, int rx, pcre2_match_data *y, int ry)
{
	if (!same_ovector(x, rx, y, ry)) return 0;
	PCRE2_SIZE nx = pcre2_get_capture_event_count(x), ny = pcre2_get_capture_event_count(y);
	if (nx != ny) return 0;
	const pcre2_capture_event *ex = pcre2_get_capture_event_pointer(x), *ey = pcre2_get_capture_event_pointer(y);
	for (PCRE2_SIZE i = 0; i < nx; ++i)
		if (ex[i].group != ey[i].group || ex[i].start != ey[i].start || ex[i].end != ey[i].end) return 0;
	return 1;
}

int main(void)
{
	uint32_t have_jit = 0;
	pcre2_config(PCRE2_CONFIG_JIT, &have_jit);
	if (!have_jit) return 0; /* nothing to compare without JIT */
	const char *env = getenv("CAPTURE_HISTORY_DIFF_CASES");
	unsigned CASES = env ? (unsigned)strtoul(env, NULL, 10) : DEFAULT_CASES;
	const char *seed = getenv("CAPTURE_HISTORY_DIFF_SEED");
	if (seed) rng_state = strtoull(seed, NULL, 0);

	unsigned compared = 0, with_events = 0, mismatches = 0, jit_refused = 0, engine_errors = 0, upstream_divergent = 0;
	for (unsigned c = 0; c < CASES; ++c) {
		static const char verb[] = "(*CAPTURE_HISTORY)";
		char text[512];
		strcpy(text, verb);
		buf b = { text, strlen(text), sizeof(text), 0 };
		gen(&b, 0);
		PCRE2_UCHAR pat[512];
		size_t plen = 0;
		for (; text[plen]; ++plen) pat[plen] = (unsigned char)text[plen];
		const size_t vlen = sizeof(verb) - 1;
		int err;
		PCRE2_SIZE eo;
		/* Four engines: interpreter and JIT, each without and with history. */
		pcre2_code *pi = pcre2_compile(pat + vlen, plen - vlen, 0, &err, &eo, NULL);
		pcre2_code *hi = pcre2_compile(pat, plen, 0, &err, &eo, NULL);
		if (!pi || !hi) { pcre2_code_free(pi); pcre2_code_free(hi); continue; }
		pcre2_code *pj = pcre2_code_copy(pi), *hj = pcre2_code_copy(hi);
		if (pcre2_jit_compile(pj, PCRE2_JIT_COMPLETE) != 0 || pcre2_jit_compile(hj, PCRE2_JIT_COMPLETE) != 0) {
			++jit_refused;
			pcre2_code_free(pi); pcre2_code_free(hi); pcre2_code_free(pj); pcre2_code_free(hj);
			continue;
		}
		pcre2_match_data *mpi = pcre2_match_data_create_from_pattern(pi, NULL);
		pcre2_match_data *mpj = pcre2_match_data_create_from_pattern(pj, NULL);
		pcre2_match_data *mhi = pcre2_match_data_create_from_pattern(hi, NULL);
		pcre2_match_data *mhj = pcre2_match_data_create_from_pattern(hj, NULL);
		for (unsigned s = 0; s < SUBJECTS_PER_PATTERN; ++s) {
			PCRE2_UCHAR subj[12];
			unsigned n = rnd(sizeof(subj) / sizeof(subj[0]));
			for (unsigned i = 0; i < n; ++i) subj[i] = rnd(3) ? 'a' : 'b';
			int rpi = pcre2_match(pi, subj, n, 0, PCRE2_NO_JIT, mpi, NULL);
			int rpj = pcre2_jit_match(pj, subj, n, 0, 0, mpj, NULL);
			int rhi = pcre2_match(hi, subj, n, 0, PCRE2_NO_JIT, mhi, NULL);
			int rhj = pcre2_jit_match(hj, subj, n, 0, 0, mhj, NULL);
			/* Engines legitimately fail differently on limits (e.g. recursion loop vs JIT stack). */
			if ((rpi < 0 && rpi != PCRE2_ERROR_NOMATCH) || (rpj < 0 && rpj != PCRE2_ERROR_NOMATCH) ||
			    (rhi < 0 && rhi != PCRE2_ERROR_NOMATCH) || (rhj < 0 && rhj != PCRE2_ERROR_NOMATCH)) { ++engine_errors; continue; }
			/* Existing upstream interpreter/JIT disagreements are not ours to judge here. */
			if (!same_ovector(mpi, rpi, mpj, rpj)) { ++upstream_divergent; continue; }
			++compared;
			if (rhi > 0 && pcre2_get_capture_event_count(mhi) > 0) ++with_events;
			const char *why = NULL;
			if (!same_ovector(mpi, rpi, mhi, rhi)) why = "history changed the interpreter result";
			else if (!same_ovector(mpj, rpj, mhj, rhj)) why = "history changed the JIT result";
			else if (!same_result(mhi, rhi, mhj, rhj)) why = "interpreter and JIT histories differ";
			if (why && ++mismatches <= 5) {
				fprintf(stderr, "MISMATCH (%s) /%s/ subject \"", why, text);
				for (unsigned i = 0; i < n; ++i) fputc((int)subj[i], stderr);
				fprintf(stderr, "\": interp rc=%d events=%zu, jit rc=%d events=%zu\n", rhi,
					(size_t)pcre2_get_capture_event_count(mhi), rhj, (size_t)pcre2_get_capture_event_count(mhj));
			}
		}
		pcre2_match_data_free(mpi); pcre2_match_data_free(mpj);
		pcre2_match_data_free(mhi); pcre2_match_data_free(mhj);
		pcre2_code_free(pi); pcre2_code_free(hi); pcre2_code_free(pj); pcre2_code_free(hj);
	}
	fprintf(stderr, "%u comparisons, %u with events, %u engine-error cases, %u upstream interpreter/JIT divergences skipped\n",
		compared, with_events, engine_errors, upstream_divergent);
	if (compared < CASES || with_events < CASES / 4) {
		fprintf(stderr, "differential too weak\n");
		return 1;
	}
	if (engine_errors > compared / 10 || upstream_divergent > compared / 100) { fprintf(stderr, "too many excluded cases\n"); return 1; }
	if (mismatches) fprintf(stderr, "%u history mismatches in %u comparisons\n", mismatches, compared);
	return mismatches != 0 || jit_refused > CASES / 100;
}
