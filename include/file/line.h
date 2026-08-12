#ifndef LINE_H
#define LINE_H

#include <stddef.h>
#include <stdint.h>

#include "plugins/language_syntax.h"
#include "util/types/u32string.h"
#include "util/types/vector.h"

#define MAX_LINE_TOKENS 256

typedef struct {
	u32string text;
	Vector tokens;

	enum lex_state state_in;
	enum lex_state state_out;

	int dirty;
} Line;

void line_init(Line* line);
void line_free(Line* line);

Line line_new();
Line line_with_text(u32string text);

size_t line_size(const Line* line);
size_t line_tokens_size(const Line* line);

void line_tokenize(Line* line, struct lexer* lexer);


#endif