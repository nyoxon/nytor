#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

#include "make_lexer.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

int is_make_word_char(uint32_t c) {
	unsigned char k = (unsigned char) c;

	return (isalnum(k) || k == '_');
}

struct keyword {
	const char* name;
	uint8_t len;

	enum highlight hl;
};

struct operator {
	const char* text;
	uint8_t len;
};

struct punctuation {
	unsigned char text;
};

static const char comment_fmt[] = "#";

static const char* extensions[] = {
	".mk",
};

static const char* reserved_filenames[] = {
	"makefile",
	"Makefile",
};

static const struct pair MAKE_PAIRS[] = {
	{'{', '}', 0, 1},
	{'(', ')', 0, 1},
	{'[', ']', 0, 1},
	{'"', '"', 0, 1},
	{':', ':', 1, 0},
	{'\'', '\'', 0, 1}
};

static const struct keyword MAKE_KEYWORDS[] = {
	{"include", 		7, HL_KEYWORD},
	{"ifdef",			5, HL_KEYWORD},
	{"ifndef",			6, HL_KEYWORD},
	{"endif",			5, HL_KEYWORD},
	{"ifeq",			4, HL_KEYWORD},
	{"ifneq",			5, HL_KEYWORD},
	{"else",			4, HL_KEYWORD},
	{"define",			6, HL_KEYWORD},
	{"endef",			5, HL_KEYWORD},
	{"override",		8, HL_KEYWORD},
	{"export",			7, HL_KEYWORD},
	{"unexport",		8, HL_KEYWORD},
	{"private",			7, HL_KEYWORD},
	{"vpath",			5, HL_KEYWORD},
};

static const struct operator MAKE_OPERATORS[] = {
	{":::=",			4},
	{"::=",				3},
	{":=",				2},
	{"!=",				2},
	{"+=",				2},
	{"?=",				2},
	{"$@",				2},
	{"$<",				2},
	{"$^",				2},
	{"$?",				2},
	{"$*",				2},
	{"$",				1},
	{"@",				1}
};

static const struct punctuation MAKE_PUNCTUATIONS[] = {
	{'('},
	{')'},
	{'{'},
	{'}'},
	{'['},
	{']'},
	{';'},
	{':'},
	{'.'},
	{','}
};

#define MAKE_KEYWORDS_COUNT ARRAY_SIZE(MAKE_KEYWORDS)
#define MAKE_OPERATORS_COUNT ARRAY_SIZE(MAKE_OPERATORS)
#define MAKE_PUNCTUATIONS_COUNT ARRAY_SIZE(MAKE_PUNCTUATIONS)



static int cmp_utf8_to_ascii
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	const char* word
)
{
	size_t i = 0;

	while (word[i]) {
		if (pos + i >= len) {
			return 0;
		}

		if (line[pos + i] != (unsigned char) word[i]) {
			return 0;
		}

		i++;
	}

	return 1;
}

static int is_keyword(const uint32_t* word, size_t len, int* index) {
	for (size_t i = 0; i < MAKE_KEYWORDS_COUNT; i++) {
		if (MAKE_KEYWORDS[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, MAKE_KEYWORDS[i].name)) {
			*index = i;
			return 1;
		}
	}

	return 0;
}

static int match_operator
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	size_t* op_len
)
{
	for (size_t i = 0; i < MAKE_OPERATORS_COUNT; i++) {
		size_t n = MAKE_OPERATORS[i].len;

		if (pos + n <= len &&
			cmp_utf8_to_ascii(line, len, pos, MAKE_OPERATORS[i].text))
		{
			*op_len = n;
			return 1;
		}
	}

	return 0;
}

static int is_punctuation(uint32_t c) {
	for (size_t i = 0; i < MAKE_PUNCTUATIONS_COUNT; i++) {
		if (c == MAKE_PUNCTUATIONS[i].text) {
			return 1;
		}
	}

	return 0;	
}

static int is_number_start
(
	const uint32_t* line,
	size_t i,
	size_t len
)
{
	if (i >= len) {
		return 0;
	}

	if (isdigit((unsigned char) line[i])) {
		return 1;
	}

	// .5
	if (line[i] == '.' &&
		(i + 1 < len) &&
		isdigit((unsigned char) line[i + 1]))
	{
		return 1;
	}

	return 0;
}


static void consume_number
(
	const uint32_t* line,
	size_t len,
	size_t* i
)
{
	size_t pos = *i;

	// whole part
	while (pos < len &&
		isdigit((unsigned char) line[pos]))
	{
		pos++;
	}


	// decimal part
	if (pos < len && line[pos] == '.') {
		pos++;

		while (pos < len &&
			isdigit((unsigned char) line[pos]))
		{
			pos++;
		}
	}

	*i = pos;
}

static size_t make_escape_length(const uint32_t* s, size_t len) {
	if (len < 2 || *s != '\\') {
		return 0;
	}

	switch (s[1]) {
	case '\'':
	case '"':
	case '?':
	case '\\':
	case 'a':
	case 'b':
	case 'f':
	case 'n':
	case 't':
	case 'r':
	case 'v':
		return 2;
	}

	return 0;
}

struct make_lexer {
	struct lexer base;
};


static size_t make_tokenize_line
(
	struct lexer* lexer,
	const uint32_t* line,
	size_t len,
	struct token* tokens,
	size_t max_tokens,
	enum lex_state state_in,
	enum lex_state* state_out
)
{
	(void) lexer;
	(void) state_in;

	size_t ntokens = 0;
	size_t i = 0;

	int first_token = 1;

	while (i < len && ntokens < max_tokens) {
		while (i < len && isspace((unsigned char) line[i])) {
			i++;
		}

		if (i >= len) {
			break;
		}

		//   comment //
		if (line[i] == '#')
		{
			tokens[ntokens++] = (struct token) {
				HL_COMMENT,
				i,
				len
			};

			break;
		}

		// string
		if (line[i] == '"') {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			i++;
			size_t begin = i;

			while (i < len) {
				if (line[i] == '\\') {
					size_t n = make_escape_length(
						line + i,
						len - i
					);

					// closes the prev string
					if (n != 0) {
						if (begin != i) {
							tokens[ntokens++] = (struct token) {
								HL_STRING,
								begin,
								i
							};
						}

						tokens[ntokens++] = (struct token) {
							HL_NUMBER,
							i,
							i + n
						};

						i += n;
						begin = i;
						continue;
					}
					
					i++;
					continue;
				}

				if (line[i] == '"') {
					break;
				}

				i++;
			}

			if (begin != i) {
				tokens[ntokens++] = (struct token) {
					HL_STRING,
					begin,
					i
				};				
			}

			if (i < len && line[i] == '"') {
				tokens[ntokens++] = (struct token) {
					HL_PUNCTUATION,
					i,
					i + 1
				};

				i++;			
			}

			continue;
		}

		// char
		if (line[i] == '\'') {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			i++;
			size_t begin = i;

			while (i < len) {
				if (line[i] == '\\') {
					size_t n = make_escape_length(
						line + i,
						len - i
					);

					// closes the prev char
					if (n != 0) {
						if (begin != i) {
							tokens[ntokens++] = (struct token) {
								HL_CHAR,
								begin,
								i
							};
						}

						tokens[ntokens++] = (struct token) {
							HL_NUMBER,
							i,
							i + n
						};

						i += n;
						begin = i;
						continue;
					}
					
					i++;
					continue;
				}

				if (line[i] == '\'') {
					break;
				}

				i++;
			}

			if (begin != i) {
				tokens[ntokens++] = (struct token) {
					HL_CHAR,
					begin,
					i
				};				
			}

			if (i < len && line[i] == '\'') {
				tokens[ntokens++] = (struct token) {
					HL_PUNCTUATION,
					i,
					i + 1
				};

				i++;			
			}

			continue;
		}

		// digit/number
		if (is_number_start(line, i, len)) {
			size_t begin = i;

			consume_number(line, len, &i);

			tokens[ntokens++] = (struct token) {
				HL_NUMBER,
				begin,
				i
			};

			continue;
		}

		// identifier
		if (isalpha((unsigned char) line[i]) || line[i] == '_') {

			size_t begin = i;

			while (i < len && is_make_word_char(line[i]))
			{
				i++;
			}

			int index = -1;
			enum highlight hl;

			if (is_keyword(line + begin, i - begin, &index)) {
				hl = MAKE_KEYWORDS[index].hl;
			} 

			else {
				hl = HL_NORMAL;

				size_t j = i;

				// function/label
				while (j < len && isspace((unsigned char) line[j])) {
					j++;
				}

				if (first_token && j < len && line[j] == ':') {
					hl = HL_FUNCTION;
				}
			}

			tokens[ntokens++] = (struct token) {
				hl,
				begin,
				i
			};

			continue;
		}

		// operator
		size_t op_len;

		if (match_operator(line, len, i, &op_len)) {
			tokens[ntokens++] = (struct token) {
				HL_OPERATOR,
				i,
				i + op_len
			};

			i += op_len;

			continue;
		}


		// punctuation
		if (is_punctuation(line[i])) {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1,
			};

			i++;

			continue;
		}

		// normal caractere
		tokens[ntokens++] = (struct token) {
			HL_NORMAL,
			i,
			i + 1
		};

		first_token = 0;

		i++;		
	}

	if (state_out) {
		*state_out = LEX_STATE_NORMAL;
	}

	return ntokens;
}

static void make_reset(struct lexer* lexer) {
	(void) lexer;
}

static void make_destroy(struct lexer* lexer) {
	free(lexer);
}


struct lexer* make_create_lexer(void) {
	struct make_lexer* make = calloc(1, sizeof(*make));

	make->base.tokenize_line = make_tokenize_line;
	make->base.destroy = make_destroy;
	make->base.reset = make_reset;

	return &make->base;	
}

static const struct language_rules make_rules = {
	.pairs = MAKE_PAIRS,
	.pair_count = ARRAY_SIZE(MAKE_PAIRS),
	.comment_fmt = comment_fmt,
	.auto_indent = 1,
	.is_word_char = is_make_word_char
};

static const struct language_plugin make_plugin = {
	.name = "make",
	.extensions = extensions,
	.extension_count = ARRAY_SIZE(extensions),
	.reserved_filenames = reserved_filenames,
	.reserved_filename_count = ARRAY_SIZE(reserved_filenames),
	.create_lexer = make_create_lexer,
	.rules = &make_rules
};

const struct language_plugin *plugin_init(void)
{
	return &make_plugin;
}