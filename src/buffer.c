#include "buffer.h"
#include <stdlib.h>
#include <string.h>

int line_get_indent(const Line* line) {
	int indent = 0;

	while ((size_t) indent < line->len && line->text[indent] == ' ') {
		indent++;
	}

	return indent;
}

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