/* Validate compiled patterns without choosing a subject that reaches each branch. */
#include <stdio.h>
#include <pcre2.h>

int main(void)
{
	static const struct { const char *pattern; int expected; uint32_t options; } cases[] = {
		{ "never-in-the-probe-corpus/(a)\\1", PCRE2_ERROR_DFA_UITEM },
		{ "safe|hidden/(a)\\1", PCRE2_ERROR_DFA_UITEM },
		{ "(a)?(?(1)b|c)", PCRE2_ERROR_DFA_UCOND },
		{ "hidden/\\Kfile", PCRE2_ERROR_DFA_UITEM },
#if PCRE2_CODE_UNIT_WIDTH == 32
		{ "hidden/\\C+", 0 },
#else
		{ "hidden/\\C+", PCRE2_ERROR_DFA_UITEM },
#endif
		{ "a(*PRUNE)b", PCRE2_ERROR_DFA_UITEM },
		{ "(*napla:a)a", PCRE2_ERROR_DFA_UITEM },
		{ "(*script_run:a)", PCRE2_ERROR_DFA_UITEM },
		{ "(?J)(?<x>a)(?<x>b)\\k<x>", PCRE2_ERROR_DFA_UITEM },
		{ "(?J)(?<x>a)(?<x>b)(?(x)c|d)", PCRE2_ERROR_DFA_UCOND },
		{ "(a)(?(R1)a|b)", PCRE2_ERROR_DFA_UCOND },
		{ "(?(DEFINE)(?<helper>(a)\\2))a", PCRE2_ERROR_DFA_UITEM },
		{ "\\x{1f680}+(a)\\1", PCRE2_ERROR_DFA_UITEM },
		{ "(?i:users|home)/(?-i:Peter)/.*", 0 },
		{ "[\\p{L}\\p{M}]+", 0 },
		{ "[a-z]{0,3}(?:foo|bar)+", 0 },
		{ "(?(?=a)a|b)", 0 },
		{ "(?C'variable length')hello", 0 },
		{ "(?C'(a)\\1')hello", 0 },
		{ "(?<helper>a)(?&helper)", PCRE2_ERROR_DFA_RECURSE },
		{ "(?R)", PCRE2_ERROR_DFA_RECURSE },
		{ "(?<=a{1,3})b", PCRE2_ERROR_DFA_UITEM },
		{ "(?<=aaa)b", 0 },
		{ "(?<=a|aaa)b", 0 },
		{ "[\\x{100}-\\x{1f680}]+\\p{L}{1,3}", 0 },
		{ "\\x{1f680}{1,3}\\x{e9}+", 0 },
		{ "\\Q(a)\\1\\E", 0 },
		{ "abc", PCRE2_ERROR_DFA_UINVALID_UTF, PCRE2_MATCH_INVALID_UTF },
	};
	unsigned failures = 0;
	if (pcre2_pattern_info(NULL, PCRE2_INFO_DFA_COMPATIBILITY, NULL) != sizeof(int)) ++failures;
	int result;
	if (pcre2_pattern_info(NULL, PCRE2_INFO_DFA_COMPATIBILITY, &result) != PCRE2_ERROR_NULL) ++failures;
	for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		PCRE2_UCHAR pattern[256];
		unsigned j = 0;
		for (; cases[i].pattern[j]; ++j) pattern[j] = (unsigned char)cases[i].pattern[j];
		pattern[j] = 0;
		int error, compatibility = 999;
		PCRE2_SIZE offset;
		pcre2_code *code = pcre2_compile(pattern, j, PCRE2_UTF | PCRE2_UCP | cases[i].options, &error, &offset, NULL);
		if (!code) {
			fprintf(stderr, "compile failed: %s (%d at %zu)\n", cases[i].pattern, error, (size_t)offset);
			++failures;
			continue;
		}
		int rc = pcre2_pattern_info(code, PCRE2_INFO_DFA_COMPATIBILITY, &compatibility);
		if (rc != 0 || compatibility != cases[i].expected) {
			fprintf(stderr, "compatibility: %s: query=%d result=%d expected=%d\n", cases[i].pattern, rc, compatibility, cases[i].expected);
			++failures;
		}
		if (cases[i].expected == 0) {
			static const char *subjects[] = { "", "a", "b", "hello", "hidden/x", "home/Peter/file" };
			pcre2_match_data *data = pcre2_match_data_create(64, NULL);
			if (!data) return 1;
			for (unsigned s = 0; s < sizeof(subjects) / sizeof(subjects[0]); ++s) {
				PCRE2_UCHAR subject[64];
				unsigned len = 0;
				for (; subjects[s][len]; ++len) subject[len] = (unsigned char)subjects[s][len];
				int workspace[1024];
				int match = pcre2_dfa_match(code, subject, len, 0, 0, data, NULL, workspace, 1024);
				if (match < 0 && match != PCRE2_ERROR_NOMATCH) {
					fprintf(stderr, "accepted pattern failed during DFA matching: %s (%d)\n", cases[i].pattern, match);
					++failures;
				}
			}
			pcre2_match_data_free(data);
		}
		pcre2_code_free(code);
	}
	return failures ? 1 : 0;
}
