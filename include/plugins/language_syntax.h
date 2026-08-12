#ifndef LANGUAGE_SYNTAX_H
#define LANGUAGE_SYNTAX_H

#include <stddef.h>
#include <stdint.h>

#include "terminal/style.h"

enum highlight {
	HL_NONE,
	HL_NORMAL,

	HL_KEYWORD,
	HL_TYPE,
	HL_LIB_TYPE,
	HL_POSIX_TYPE,

	HL_PREPROCESSOR,
	HL_CONSTANT,
	HL_NUMBER,
	HL_STRING,
	HL_CHAR,

	HL_COMMENT,
	HL_OPERATOR,
	HL_PUNCTUATION,
	HL_FUNCTION,
	HL_LIB_FUNCTION,
	HL_SPECIFIER,
	HL_POSIX,
	HL_METHOD_OR_ATTRIB,

	HL_COUNT
};

struct token {
	enum highlight highlight;

	size_t begin;
	size_t end;
};

enum lex_state {
	LEX_STATE_NORMAL,
	LEX_STATE_BLOCK_COMMENT,
	LEX_STATE_STRING,
	LEX_STATE_CHAR,
	LEX_STATE_OTHER
};

struct lexer {
	size_t (*tokenize_line)(
		struct lexer* lexer,
		const uint32_t* line,
		size_t len,
		struct token* tokens,
		size_t max_tokens,
		enum lex_state state_in,
		enum lex_state* state_out
	);

	void (*destroy)(struct lexer*);
	void (*reset)(struct lexer*);
};

#endif