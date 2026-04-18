#include "buffer.h"
#include <stdlib.h>
#include <string.h>

// PRE: line != NULL

int line_get_indent(const Line* line) {
	int indent = 0;

	while ((size_t) indent < line->len && line->text[indent] == ' ') {
		indent++;
	}

	return indent;
}

int line_is_comment(const Line* line, size_t indent) {
	const char* text = line->text;

	size_t len = strlen(text);
	size_t comment_fmt_len = strlen(COMMENT_FMT);

	if (indent + comment_fmt_len > len) {
		return 0;
	}

	return strncmp(text + indent, COMMENT_FMT, comment_fmt_len) == 0;	
}

// PRE: line != NULL
// rewrite the text inside the line adding a tab at the beggining of it

void line_set_indent(Line* line, size_t indent) {
	size_t i = 0;

	while (i < line->len && line->text[i] == ' ') {
		i++;
	}

	size_t new_len = indent + (line->len - i);
	char* new_text = malloc(new_len + 1);

	memset(new_text, ' ', indent);
	memcpy(new_text + indent, line->text + i, line->len - i);

	new_text[new_len] = '\0';
	free(line->text);
	line->text = new_text;
	line->len = new_len;
}

void line_replace(Line* line, char* new_text, size_t new_len) {
	free(line->text);
	line->text = new_text;
	line->len = new_len;
}

void line_free(Line* line) {
	free(line->text);
}

void vector_line_destroy(void* ptr) {
	Line* line = (Line*) ptr;
	free(line->text);
}

void clipboard_free(Clipboard* cb) {
	free(cb->text);
}