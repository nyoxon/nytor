#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "bash_lexer.h"

int u32_isspace(uint32_t cp) {
	if (cp > UINT8_MAX) {
		return 0;
	}

	return isspace((unsigned char) cp) != 0;
}

int u32_isalpha(uint32_t cp) {
	if (cp > UINT8_MAX) {
		return 0;
	}

	return isalpha((unsigned char) cp) != 0;
}

int u32_isalnum(uint32_t cp) {
	if (cp > UINT8_MAX) {
		return 0;
	}

	return isalnum((unsigned char) cp) != 0;
}



int is_bash_word_char(uint32_t c) {
	return (u32_isalnum(c) || c == U'_');
}

struct keyword {
	const char* name;
	uint8_t len;

	enum highlight hl;
};

struct specific_keyword {
	const char* name;
	uint8_t len;
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
	".sh",
};

static const struct pair BASH_PAIRS[] = {
	{'{', '}', 1, 1},
	{'(', ')', 1, 1},
	{'[', ']', 0, 1},
	{'"', '"', 0, 1},
	{'\'', '\'', 0, 1}
};

static const struct keyword BASH_KEYWORDS[] = {
	{"case",			4, HL_KEYWORD},
	{"do",				2, HL_KEYWORD},
	{"done",			4, HL_KEYWORD},
	{"elif",			4, HL_KEYWORD},
	{"else",			4, HL_KEYWORD},
	{"esac",			4, HL_KEYWORD},
	{"fi",				2, HL_KEYWORD},
	{"for",				3, HL_KEYWORD},
	{"function",		8, HL_KEYWORD},
	{"if",				2, HL_KEYWORD},
	{"in",				2, HL_KEYWORD},
	{"then",			4, HL_KEYWORD},
	{"time",			4, HL_KEYWORD},
	{"until",			5, HL_KEYWORD},
	{"while",			5, HL_KEYWORD},
	{"exit",			4, HL_KEYWORD},
	{"select",			6, HL_KEYWORD},
	{"coproc",			6, HL_KEYWORD},
	{"shopt",			5, HL_KEYWORD},
	{"break",			5, HL_KEYWORD},
	{"continue",		8, HL_KEYWORD},
	{"return",			6, HL_KEYWORD},
	{"declare",			7, HL_KEYWORD},
	{"builtin",			7, HL_KEYWORD},
	
	{"local",			5, HL_SPECIFIER},

	{"true",			4, HL_TYPE},
	{"false",			5, HL_TYPE},
};


// HL = HL_LIB_FUNCTION
static const struct specific_keyword BASH_LIB[] = {
	{"grep",				4},
	{"egrep",				5},
	{"fgrep",				5},
	{"sed",					3},
	{"awk",					3},
	{"cut",					3},
	{"sort",				4},
	{"uniq",				4},
	{"tr",					2},
	{"head",				4},
	{"tail",				4},
	{"less",				4},
	{"more",				4},
	{"wc",					2},
	{"diff",				4},
	{"cmp",					3},
	{"comm",				4},
	{"paste",				5},
	{"join",				4},
	{"xargs",				5},
	{"tee",					3},
	{"strings",				7},
	{"column",				6},
	{"fmt",					3},
	{"fold",				4},
	{"od",					2},
	{"xxd",					3},
	{"cat",					3},
	{"set",					3},

	{"ls",					2},
	{"cp",					2},
	{"mv",					2},
	{"rm",					2},
	{"mkdir",				5},
	{"rmdir",				5},
	{"touch",				5},
	{"ln",					2},
	{"readlink",			8},
	{"realpath",			8},
	{"basename",			8},
	{"dirname",				7},
	{"find",				4},
	{"locale",				6},
	{"file",				4},
	{"stat",				4},
	{"du",					2},
	{"df",					2},
	{"tree",				4},
	{"install",				7},
	{"mktemp",				6},

	{"chmod",				5},
	{"chown",				5},
	{"chgrp",				5},
	{"umask",				5},
	{"id",					2},
	{"whoami",				6},
	{"who",					3},
	{"w",					1},
	{"users",				5},
	{"groups",				6},
	{"passwd",				6},
	{"su",					2},
	{"sudo",				4},

	{"ps",					2},
	{"top",					3},
	{"kill",				4},
	{"pkill",				5},
	{"pgrep",				5},
	{"nice",				4},
	{"renice",				6},
	{"nohup",				5},
	{"free",				4},
	{"uptime",				6},
	{"uname",				5},
	{"hostname",			8},
	{"dmesg",				5},
	{"lsof",				4},
	{"env",					3},
	{"printenv",			8},
	{"time",				4},
	{"watch",				5},
	{"shift",				5},

	{"curl",				4},
	{"wget",				4},
	{"ssh",					3},
	{"scp",					3},
	{"sftp",				4},
	{"rsync",				5},
	{"ping",				4},
	{"ip",					2},
	{"ss",					2},
	{"dig",					3},
	{"host",				4},
	{"nslookup",			8},
	{"traceroute",			10},
	{"nc",					2},
	{"netcat",				6},
	{"ftp",					3},

	{"tar",					3},
	{"gzip",				4},
	{"gunzip",				6},
	{"bzip2",				5},
	{"bunzip2",				7},
	{"xz",					2},
	{"unxz",				4},
	{"zip",					3},
	{"unzip",				5},
	{"zstd",				4},
	{"unzstd",				6},

	{"gcc",					3},
	{"g++",					3},
	{"clang",				5},
	{"clang++",				7},
	{"make",				4},
	{"cmake",				5},
	{"ninja",				5},
	{"git",					3},
	{"ld",					2},
	{"as",					2},
	{"objdump",				7},
	{"objcopy",				7},
	{"nm",					2},
	{"readelf",				7},
	{"ar",					2},
	{"ranlib",				6},
	{"strip",				5},
	{"pkg-config",			10},

	{"clear",				4},
	{"reset",				5},
	{"stty",				4},
	{"tput",				4},
	{"tty",					3},
	{"script",				6},
	{"screen",				6},
	{"tmux",				4},

	{"bc",					2},
	{"expr",				4},
	{"seq",					3},
	{"factor",				6},
	{"yes",					3},
	{"shuf",				4},
	{"date",				4},
	{"cal",					3},

	{"mount",				5},
	{"umount",				6},
	{"lsblk",				5},
	{"blkid",				5},
	{"fdisk",				5},
	{"parted",				6},
	{"dd",					2},
	{"sync",				4},
	{"fsck",				4},

	{"sleep",				5},
	{"which",				5},
	{"whereis",				7},
};


// HL = HL_POSIX
// these commands are not posix, but they are different from
// the commands above
static const struct specific_keyword BASH_BUILTINS[] = {
	{"source",				6},
	{"local",				5},
	{"let",					3},
	{"jobs",				4},
	{"bg",					2},
	{"fg",					2},
	{"cd",					2},
	{"export",				6},
	{"unset",				5},
	{"alias",				5},
	{"test",				4},
	{"echo",				4},
	{"read",				4},
	{"getopts",				7},
	{"command",				7},
	{"type",				4},
	{"printf",				6},
};

static const struct operator BASH_COMPOUND_OPERATORS[] = {
	{"-eq",				3},
	{"-ne",				3},
	{"-lt",				3},
	{"-le",				3},
	{"-gt",				3},
	{"-ge",				3},

	{"-ef",				3},
	{"-nt",				3},
	{"-ot",				3},

	{"-e",				2},
	{"-f",				2},
	{"-d",				2},
	{"-r",				2},
	{"-w",				2},
	{"-x",				2},
	{"-s",				2},
	{"-L",				2},
	{"-h",				2},
	{"-b",				2},
	{"-c",				2},
	{"-p",				2},
	{"-S",				2},
	{"-t",				2},
};

static const struct operator BASH_ARITHMETIC_OPERATORS[] = {
	{"==",				2},
	{"!=",				2},
	{"<=",				2},
	{">=",				2},
	{"++",				2},
	{"--",				2},
	{"**",				2},

	{"?",				1},
	{":",				1},
	{"-",				1},
	{"+",				1},
	{"*",				1},
	{"/",				1},
	{"%",				1},
};

static const struct operator BASH_OPERATORS[] = {
	{"<<<",				3},
	{"<<-",				3},
	{"<<=",				3},
	{">>=",				3},

	{">>",				2},
	{"<<",				2},
	{"<>",				2},
	{"<&",				2},
	{">&",				2},
	{">|",				2},

	{";;",				2},
	{"&&",				2},
	{"||",				2},
	{"|&",				2},
	{"=~",				2},

	{"+=",				2},
	{"-=",				2},
	{"*=",				2},
	{"/=",				2},
	{"&=",				2},
	{"%=",				2},
	{"^=",				2},
	{"|=",				2},

	{">",				1},
	{"<",				1},
	{"!",				1},
	{"&",				1},
	{"|",				1},
	{"=",				1},
	{"%",				1},
	{"+",				1},
	{"*",				1},
	{"^",				1},
};

static const struct punctuation BASH_PUNCTUATIONS[] = {
	{'('},
	{')'},
	{'{'},
	{'}'},
	{'['},
	{']'},
	{';'},
	{':'},
	{'.'},
	{'$'}, // $ is an operator, but i don't like seeing it in the same
		   // color as operators
	{'-'},
	{'\''},
	{'"'},
	{'\\'}	
};

// can used after $ operator
static const struct punctuation BASH_SPECIAL_PUNCTUATIONS[] = {
	{'#'}, // $#
	{'$'}, // $$
	{'@'}, // $@
	{'!'}, // $!
	{'-'}, // $-
	{'*'}, // $*
	{'?'}, // $?
};

#define BASH_PUNCTUATION_COUNT ARRAY_SIZE(BASH_PUNCTUATIONS)
#define BASH_OPERATOR_COUNT ARRAY_SIZE(BASH_OPERATORS)
#define BASH_COMPOUND_OPERATOR_COUNT ARRAY_SIZE(BASH_COMPOUND_OPERATORS)
#define BASH_ARITHMETIC_OPERATOR_COUNT ARRAY_SIZE(BASH_ARITHMETIC_OPERATORS)
#define BASH_KEYWORD_COUNT ARRAY_SIZE(BASH_KEYWORDS)
#define BASH_LIB_COUNT ARRAY_SIZE(BASH_LIB)
#define BASH_BUILTIN_COUNT ARRAY_SIZE(BASH_BUILTINS)
#define BASH_SPECIAL_PUNCTUATION_COUNT ARRAY_SIZE(BASH_SPECIAL_PUNCTUATIONS)

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
	for (size_t i = 0; i < BASH_KEYWORD_COUNT; i++) {
		if (BASH_KEYWORDS[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, BASH_KEYWORDS[i].name)) {
			*index = i;
			return 1;
		}
	}

	return 0;
}

static int is_lib(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < BASH_LIB_COUNT; i++) {
		if (BASH_LIB[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, BASH_LIB[i].name)) {
			return 1;
		}
	}

	return 0;
}

static int is_builtin(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < BASH_BUILTIN_COUNT; i++) {
		if (BASH_BUILTINS[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, BASH_BUILTINS[i].name)) {
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
	for (size_t i = 0; i < BASH_OPERATOR_COUNT; i++) {
		size_t n = BASH_OPERATORS[i].len;

		if (pos + n <= len &&
			cmp_utf8_to_ascii(line, len, pos, BASH_OPERATORS[i].text))
		{
			*op_len = n;
			return 1;
		}
	}

	return 0;
}

static int match_compound_operator
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	size_t* op_len
)
{
	for (size_t i = 0; i < BASH_COMPOUND_OPERATOR_COUNT; i++) {
		size_t n = BASH_COMPOUND_OPERATORS[i].len;

		if (pos + n <= len &&
			cmp_utf8_to_ascii(line, len, pos, BASH_COMPOUND_OPERATORS[i].text))
		{
			*op_len = n;
			return 1;
		}
	}

	return 0;
}

static int match_arithmetic_operator
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	size_t* op_len
)
{
	for (size_t i = 0; i < BASH_ARITHMETIC_OPERATOR_COUNT; i++) {
		size_t n = BASH_ARITHMETIC_OPERATORS[i].len;

		if (pos + n <= len &&
			cmp_utf8_to_ascii(line, len, pos, BASH_ARITHMETIC_OPERATORS[i].text))
		{
			*op_len = n;
			return 1;
		}
	}

	return 0;
}

static int is_punctuation(uint32_t c) {
	for (size_t i = 0; i < BASH_PUNCTUATION_COUNT; i++) {
		if (c == BASH_PUNCTUATIONS[i].text) {
			return 1;
		}
	}

	return 0;	
}

static int is_special_punctuation(uint32_t c) {
	for (size_t i = 0; i < BASH_SPECIAL_PUNCTUATION_COUNT; i++) {
		if (c == BASH_SPECIAL_PUNCTUATIONS[i].text) {
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

	*i = pos;
}

static size_t bash_escape_length(const uint32_t* s, size_t len) {
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

enum bash_state {
	BASH_LEX_NORMAL,
	BASH_LEX_COMPOUND,
	BASH_LEX_ARITHMETIC,
	BASH_LEX_STRING,
	BASH_LEX_CHAR
};

struct bash_lexer {
	struct lexer base;

	enum bash_state state;
};

static void handle_multiline_lexer_state
(
	struct bash_lexer* lexer,
	const uint32_t* line,
	size_t len,
	size_t* i,
	struct token* tokens,	
	size_t* ntokens,
	enum highlight hl,
	enum lex_state* state_out
)
{
	enum lex_state state = (hl == HL_STRING)
		? LEX_STATE_STRING
		: LEX_STATE_CHAR;

	uint32_t c = (hl == HL_STRING)
		? U'"'
		: U'\'';

	size_t begin = *i;

	while (*i < len) {
		if (line[*i] == '\\') {
			size_t n = bash_escape_length(
				line + *i,
				len - *i
			);

			if (n != 0) {
				if (begin != *i) {
					tokens[(*ntokens)++] = (struct token) {
						hl,
						begin,
						*i
					};
				}

				tokens[(*ntokens)++] = (struct token) {
					HL_NUMBER,
					*i,
					*i + n
				};

				*i += n;
				begin = *i;
				continue;
			}
			
			(*i)++;
			continue;
		}

		if (line[*i] == '$') {
			if (begin != *i) {
				tokens[(*ntokens)++] = (struct token) {
					hl,
					begin,
					*i
				};
			}

			tokens[(*ntokens)++] = (struct token) {
				HL_PUNCTUATION,
				*i,
				*i + 1
			};

			(*i)++;
			begin = *i;

			if (is_special_punctuation(line[*i])) {
				(*i)++;
			}

			else {
				while (*i < len && is_bash_word_char(line[*i]))
				{
					(*i)++;
				}
			}

			tokens[(*ntokens)++] = (struct token) {
				HL_CONSTANT,
				begin,
				*i
			};

			begin = *i;
			continue;
		}

		if (line[*i] == c) {
			state = LEX_STATE_NORMAL;

			lexer->state = BASH_LEX_NORMAL;
			break;
		}

		(*i)++;
	}

	if (begin != *i) {
		tokens[(*ntokens)++] = (struct token) {
			hl,
			begin,
			*i
		};				
	}

	if (*i < len && line[*i] == c) {
		tokens[(*ntokens)++] = (struct token) {
			HL_PUNCTUATION,
			*i,
			*i + 1
		};

		(*i)++;		
	}

	if (state_out) {
		*state_out = state;
	}	
}

static size_t bash_tokenize_line
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
	struct bash_lexer* bash = (struct bash_lexer*) lexer;

	size_t ntokens = 0;
	size_t i = 0;

	if (len == 0) {
		return ntokens;
	}

	int state_has_changed = 0;

	while (i < len && ntokens < max_tokens) {
		while (i < len && u32_isspace(line[i])) {
			i++;
		}

		if (i >= len) {
			break;
		}

		if (bash->state == BASH_LEX_STRING ||
			(!state_has_changed && state_in == LEX_STATE_STRING))
		{
			state_has_changed = 1;

			handle_multiline_lexer_state(
				bash,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_STRING,
				state_out
			);

			continue;
		}

		if (bash->state == BASH_LEX_CHAR ||
			(!state_has_changed && state_in == LEX_STATE_CHAR))
		{
			state_has_changed = 1;

			handle_multiline_lexer_state(
				bash,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_STRING,
				state_out
			);

			continue;
		}

		//   comment //
		if (line[i] == U'#' &&
			(i == 0 || (i > 0 && u32_isspace(line[i - 1]))))
		{
			tokens[ntokens++] = (struct token) {
				HL_COMMENT,
				i,
				len
			};

			break;
		}

		// string
		if (line[i] == U'"' && 
			(i == 0 || (i > 0 && line[i - 1] != U'\\')))
		{
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			state_has_changed = 1;

			i++;

			handle_multiline_lexer_state(
				bash,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_STRING,
				state_out
			);

			continue;
		}

		// char
		if (line[i] == U'\'' && 
			(i == 0 || (i > 0 && line[i - 1] != U'\\'))) 
		{
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			state_has_changed = 1;

			i++;

			handle_multiline_lexer_state(
				bash,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_CHAR,
				state_out
			);

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
		if (u32_isalpha(line[i]) || line[i] == U'_') {
			enum highlight hl;

			size_t begin = i;
			int is_param = (begin > 0 && line[begin - 1] == U'-');

			while (i < len && is_bash_word_char(line[i]))
			{
				i++;
			}

			if (is_param) {
				hl = HL_NORMAL;

				goto create_token;
			}

			int is_definition =
				(i < len && line[i] == U'=');

			if (is_definition) {
				hl = HL_FUNCTION;

				goto create_token;
			}

			int index = -1;

			if (is_keyword(line + begin, i - begin, &index)) 
			{
				hl = BASH_KEYWORDS[index].hl;
			}

			else if (is_lib(line + begin, i - begin)) 
			{
				hl = HL_LIB_FUNCTION;
			}

			else if (is_builtin(line + begin, i - begin)) 
			{
				hl = HL_POSIX;
			}

			else {
				hl = HL_NORMAL;

				size_t j = i;

				while (j < len && u32_isspace(line[j])) {
					j++;
				}

				if (j < len && line[j] == U'(') {
					hl = HL_FUNCTION;
				}
			}

		create_token:
			tokens[ntokens++] = (struct token) {
				hl,
				begin,
				i
			};

			continue;
		}

		// operator
		size_t op_len;

		if (bash->state == BASH_LEX_COMPOUND &&
			match_compound_operator(line, len, i, &op_len))
		{
			tokens[ntokens++] = (struct token) {
				HL_OPERATOR,
				i,
				i + op_len
			};

			i += op_len;

			continue;			
		}

		if (bash->state == BASH_LEX_ARITHMETIC &&
			match_arithmetic_operator(line, len, i, &op_len))
		{
			tokens[ntokens++] = (struct token) {
				HL_OPERATOR,
				i,
				i + op_len
			};

			i += op_len;

			continue;			
		}

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

			if (line[i] == U'$') {
				i++;
				size_t begin = i;

				if (is_special_punctuation(line[i])) {
					i++;
				}

				else {
					while (i < len && is_bash_word_char(line[i]))
					{
						i++;
					}
				}

				tokens[(ntokens)++] = (struct token) {
					HL_CONSTANT,
					begin,
					i
				};

				continue;
			}

			if (i == 0 || (i > 0 && line[i - 1] != U'\\')) {

			if (bash->state == BASH_LEX_NORMAL && line[i] == U'[') 
			{
				bash->state = BASH_LEX_COMPOUND;
			}

			else if (bash->state == BASH_LEX_COMPOUND && 
				line[i] == U']')
			{
				if (i + 1 == len ||
					(i + 1 < len && line[i + 1] != U']'))  
				{
					bash->state = BASH_LEX_NORMAL;
				}
			}

			else if (bash->state == BASH_LEX_NORMAL &&
				i + 1 < len && line[i] == U'(' && line[i + 1] == U'(')
			{
				bash->state = BASH_LEX_ARITHMETIC;
			}

			else if (bash->state == BASH_LEX_ARITHMETIC &&
				i > 0 && line[i] == U')' && line[i - 1] == U')')
			{
				bash->state = BASH_LEX_NORMAL;
			}

			}

			i++;

			continue;
		}

		// normal caractere
		tokens[ntokens++] = (struct token) {
			HL_NORMAL,
			i,
			i + 1
		};

		i++;
	}

	if (bash->state == BASH_LEX_COMPOUND ||
		bash->state == BASH_LEX_ARITHMETIC) 
	{
		bash->state = BASH_LEX_NORMAL;
	}

	if (!state_has_changed && state_out) {
		*state_out = LEX_STATE_NORMAL;
	}

	return ntokens;	
}

static void bash_reset(struct lexer* lexer) {
	struct bash_lexer* bash = (struct bash_lexer*) lexer;

	bash->state = BASH_LEX_NORMAL;
}

static void bash_destroy(struct lexer* lexer) {
	free(lexer);
}

/// CREATER

struct lexer* bash_create_lexer(void) {
	struct bash_lexer* bash = calloc(1, sizeof(*bash));

	bash->base.tokenize_line = bash_tokenize_line;
	bash->base.destroy = bash_destroy;
	bash->base.reset = bash_reset;

	bash->state = BASH_LEX_NORMAL;

	return &bash->base;
}

static const struct language_rules bash_rules = {
	.pairs = BASH_PAIRS,
	.pair_count = ARRAY_SIZE(BASH_PAIRS),
	.comment_fmt = comment_fmt,
	.auto_indent = 1,
	.is_word_char = is_bash_word_char
};

static const struct language_plugin bash_plugin = {
	.name = "bash",
	.extensions = extensions,
	.extension_count = ARRAY_SIZE(extensions),
	.reserved_filenames = NULL,
	.reserved_filename_count = 0,
	.create_lexer = bash_create_lexer,
	.rules = &bash_rules
};

const struct language_plugin *plugin_init(void)
{
    return &bash_plugin;
}