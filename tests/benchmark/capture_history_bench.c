/* Fixed workload for comparing pcre2_match() cost with and without capture history. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#ifndef PCRE2_CAPTURE_HISTORY
#define PCRE2_CAPTURE_HISTORY 0
#endif

static unsigned long long run(const char *pat, const unsigned char *subj, size_t len, int all_starts, uint32_t opts, int reps)
{
	int err; PCRE2_SIZE eo;
	pcre2_code *c = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED, 0, &err, &eo, NULL);
	if (!c) { fprintf(stderr, "compile %s\n", pat); exit(2); }
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(c, NULL);
	unsigned long long sum = 0;
	for (int r = 0; r < reps; ++r) {
		if (!all_starts) { sum += (unsigned)pcre2_match(c, subj, len, 0, opts, md, NULL); continue; }
		for (size_t s = 0; s < len; ++s) {
			int rc = pcre2_match(c, subj, len, s, opts | PCRE2_ANCHORED, md, NULL);
			if (rc > 0) sum += pcre2_get_ovector_pointer(md)[3];
		}
	}
	pcre2_match_data_free(md); pcre2_code_free(c);
	return sum;
}

int main(int argc, char **argv)
{
	uint32_t opts = (argc > 1 && strcmp(argv[1], "on") == 0) ? PCRE2_CAPTURE_HISTORY : 0;
	static unsigned char as[20001], dna[4001], words[40001];
	memset(as, 'a', 20000); as[20000] = 'b';
	unsigned x = 12345;
	for (int i = 0; i < 4000; ++i) { x = x * 1103515245u + 12345u; dna[i] = "ACGT"[(x >> 16) & 3]; }
	for (int i = 0; i < 40000; ++i) { x = x * 1103515245u + 12345u; words[i] = ((x >> 16) % 6 == 0) ? ' ' : 'a' + (x >> 20) % 26; }
	unsigned long long s = 0;
	s += run("(a)+b", as, 20001, 0, opts, 2000);
	s += run("(?:(\\w+)\\s)+Q", words, 40000, 0, opts, 200);
	s += run("(?=(?<unit>[ACGT]{6})(?:(?>[ACGT]{0,40}?(?<hit>\\k<unit>)))++)", dna, 4000, 1, opts, 30);
	printf("%llu\n", s);
	return 0;
}
