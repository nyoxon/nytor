#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

// contains some types that contain some kind of buffer

typedef struct {
	char* text;
	size_t len;
	// int dirty;
} Line;

int line_get_indent(const Line* line);

void line_set_indent(Line* line, size_t indent);
void line_replace(Line* line, char* new_text, size_t new_len);
void line_free(Line* line);
void vector_line_destroy(void* ptr);

typedef struct {
	char* text;
	size_t len;
	int linewise;
} Clipboard;

void clipboard_free(Clipboard* cb);

#endif