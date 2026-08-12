#include "file/line.h"

void line_init(Line* line) {
	line->text = u32string_new();
	vector_init(&line->tokens, sizeof(struct token), NULL);

	line->dirty = 0;

	line->state_in = LEX_STATE_NORMAL;
	line->state_out = LEX_STATE_NORMAL;
}

void line_free(Line* line) {
	u32string_free(&line->text);
	vector_free(&line->tokens);

	line->dirty = 0;
}

Line line_new() {
	Line line;
	line_init(&line);

	return line;
}

Line line_with_text(u32string text) {
	Line line;
	line_init(&line);

	line.text = text;
	return line;
}

size_t line_size(const Line* line) {
	return u32string_size(&line->text);
}

size_t line_tokens_size(const Line* line) {
	return line->tokens.size;
}

void line_tokenize(Line* line, struct lexer* lexer) {
	if (u32string_is_empty(&line->text)) {
		return;
	}

	Vector line_tokens;
	vector_init(&line_tokens, sizeof(struct token), NULL);

	struct token tokens[MAX_LINE_TOKENS];

	size_t ntokens = lexer->tokenize_line(
		lexer,
		u32string_into_ptr_const(&line->text),
		u32string_size(&line->text),
		tokens,
		MAX_LINE_TOKENS,
		line->state_in,
		&line->state_out
	);

	for (size_t j = 0; j < ntokens; j++) {
		vector_push(&line_tokens, &tokens[j]);
	}

	vector_free(&line->tokens);
	line->tokens = line_tokens;
	line->dirty = 0;
}
