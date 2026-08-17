#ifndef LANGUAGE_SYNTAX_H
#define LANGUAGE_SYNTAX_H

#include <stddef.h>
#include <stdint.h>

#include "terminal/style.h"

// IMPORTANT: after changing something that
// the .so files depend on, don't forget
// to rebuild them

// NOTE: these HL_VALUES were created with C in mind.
// i don't think defining many different values for
// different languages is necessarily a bad thing,
// but it would be good to think about more
// generic values

// LATER: generic HL_VALUES

enum highlight {
	HL_NONE,			// must not be used
	HL_NORMAL, 			// normal text
	HL_WHITESPACE,		// whitespace

	HL_KEYWORD, 		// for, while, if etc
	HL_TYPE,			// int, float etc
	HL_LIB_TYPE, 		// size_t etc
	HL_POSIX_TYPE, 		// ssize_t etc

	HL_PREPROCESSOR, 	// #define, #include etc
	HL_CONSTANT,	 	// NULL etc
	HL_NUMBER,		 	// 1, 2, 3, 4, 5, 6...
	HL_STRING,		 	// "hello, world\n"
	HL_CHAR,		 	// 'a'

	HL_COMMENT,		 	// // or /**/
	HL_OPERATOR,	 	// >=, -, +, * etc
	HL_PUNCTUATION,	 	// ;  { etc
	HL_FUNCTION,	 	// my_function() (also used for labels)
	HL_LIB_FUNCTION, 	// printf(), malloc(), sprintf() etc
	HL_SPECIFIER,	 	// volatile, const, extern etc
	HL_POSIX,		 	// write(), read() etc
	HL_METHOD_OR_ATTRIB, // foo.x, foo->x etc

	HL_COUNT			// must not be used
};

struct token {
	enum highlight highlight;

	size_t begin;
	size_t end;
};


// a state is a information the lexer needs to know
// in order to propagate something to subsequent lines,
// given that tokenize_line receives only one line at a time
enum lex_state {
	LEX_STATE_NORMAL, // nothing to be propagated
	LEX_STATE_BLOCK_COMMENT,
	LEX_STATE_STRING,
	LEX_STATE_CHAR,
	LEX_STATE_OTHER // blabliblu
};

struct lexer {
	/*
	Yes, the line is a uint32_t* (fuck string.h)

	LATER: different encodings (uint8_t, uint16_t etc)

	Suppose the following line:

	int main(void) { printf("hello, world\n"); return 0; }

	tokenize_line could generate the following:

	int = 				{ HL_TYPE, 0, 3 }
	main = 				{ HL_FUNCTION, 4, 8}
	( = 				{ HL_PUNCTUATION, .., ..}
	void =				{ HL_TYPE, .., ..}
	) =					{ HL_PUNCTUATION, .., .. }
	{ = 				{ HL_PUNCTUATION, .., .. }
	printf =			{ HL_LIB_FUNCTION, .., .. }
	( = 				{ HL_PUNCTUATION, .., ..}
	"hello, world\n" =  { HL_STRING, .., ..}
	) = 				{ HL_PUNCTUATION, .., ..}
	; =					{ HL_PUNCTUATION, .., .. }
	return =			{ HL_KEYWORD, .., .. }
	0 =					{ HL_NUMBER, .., .. }
	; =					{ HL_PUNCTUATION, .., .. }
	} =					{ HL_PUNCTUATION, .., .. }
	*/

	size_t (*tokenize_line)(
		struct lexer* lexer,
		const uint32_t* line,
		size_t len,
		struct token* tokens,
		size_t max_tokens,
		enum lex_state state_in, // state before
		enum lex_state* state_out // state after
	);

	// responsible for deallocating memory
	void (*destroy)(struct lexer*);

	// responsible for reseting the lexer state
	void (*reset)(struct lexer*);
};

#endif