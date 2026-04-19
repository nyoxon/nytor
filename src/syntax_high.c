#include "syntax_high.h"
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

// ---- DEFINITIONS AND INITIALIZATIONS ----

const char* KEYWORDS[] = {
	"int", 
	"void",
	"float",
	"double",
	"short",
	"long",
	"char",
	"unsigned",
	"signed",
	"struct",
	"typedef",
	"auto",
	"enum",
	"union",

	"NULL",

	"extern",
	"static",
	"const",
	"sizeof",
	"volatile",
	"inline",
	"restrict",

	"return", 
	"if", 
	"else", 
	"for", 
	"case",
	"while",
	"continue",
	"break",
	"default",
	"do",
	"goto",
	"register",
	"switch"
};

const size_t KEYWORD_COUNT = (sizeof(KEYWORDS)/sizeof(KEYWORDS[0]));

const char* COLORS[] = {
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR, 
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,
	TYPE_COLOR,

	NULL_COLOR,

	SPECIFIER_COLOR,
	SPECIFIER_COLOR,
	SPECIFIER_COLOR,
	SPECIFIER_COLOR,
	SPECIFIER_COLOR,
	SPECIFIER_COLOR,
	SPECIFIER_COLOR,

	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,
	FLOW_COLOR,

	NUMBER_COLOR,
	FUNCTION_COLOR,
	MACRO_COLOR
};

const char* MACROS[] = {
	"#include",
	"#define",
	"#ifndef",
	"#endif"
};

size_t MACROS_COUNT = (sizeof(MACROS)/sizeof(MACROS[0]));


// ---- FUNCTIONS ----

void reset_color() {
	write(STDOUT_FILENO, RESET_COLOR, RESET_COLOR_SIZE);
}


void invert_color() {
	write(STDOUT_FILENO, INVERT_COLOR, INVERT_COLOR_SIZE);
}


void write_color(const char* color) {
	if (strlen(color) != COLOR_SIZE) {
		return;
	}

	write(STDOUT_FILENO, color, COLOR_SIZE);
}


int check_line_in_selection(size_t y, size_t start_y, size_t end_y) {
	return (start_y <= y && y <= end_y) ||
		   (end_y <= y && y <= start_y);
}


int check_forms_a_comment_fmt(size_t x, const Line* line, int type) {
	if ((type != SYN_START) && (type != SYN_END)) {
		return 0;
	}

	char* fmt;
	size_t fmt_len;

	int is_block = 1;

	if (type == SYN_START) {
		fmt = START_BLOCK_COMMENT;
		fmt_len = strlen(fmt);

		for (size_t j = x; j < x + fmt_len; j++) {
			if (line->text[j] != fmt[j - x]) {
				is_block = 0;
				break;
			}
		}
	} else {
		fmt = END_BLOCK_COMMENT;
		fmt_len = strlen(fmt);

		for (size_t j = x, i = 1; j > x - fmt_len; j--, i++) {
			if (line->text[j] != fmt[fmt_len - i]) {
				is_block = 0;
				break;
			}
		}
	}

	return is_block;
}

int check_has_a_comment(size_t x, const Line* line) {
	size_t result = 1;

	const char* fmt = COMMENT_FMT;
	size_t fmt_len = strlen(fmt);

	for (size_t j = x; j < x + fmt_len; j++) {
		if (line->text[j] != fmt[j - x]) {
			result = 0;
			break;
		}
	}

	return result;
}

int check_is_keyword(const char* word, size_t len, int* index) {
	for (size_t i = 0; i < KEYWORD_COUNT; i++) {
		if (strlen(KEYWORDS[i]) == len &&
			strncmp(word, KEYWORDS[i], len) == 0)
		{
			*index = (int) i;
			return 1;
		}
	}

	return 0;
}

int is_word_char(char c) {
	return (c >= 'a' && c <= 'z') ||
		   (c >= 'A' && c <= 'Z') ||
		   (c >= '0' && c <= '9') ||
		   (c == '_');
}

int is_digit(char c) {
	return (c >= '0' && c <= '9');
}

size_t get_size_of_word(size_t x, const Line* line) {
	size_t i = x;

	while (i < line->len && is_word_char(line->text[i])) {
		i++;
	}

	return i - x;
}

