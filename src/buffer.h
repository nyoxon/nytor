#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define COMMENT_FMT "//"
#define START_BLOCK_COMMENT "/*"
#define END_BLOCK_COMMENT "*/"
#define MACRO_FMT '#'
#define MACRO_FMT_SIZE 1

// contains some types that contain some kind of buffer

// Line is a logical representation of a line (literally) inside the editor
// where a line is defined by ending or not with a '\n'

typedef struct {
	char* text;
	size_t len;
	// int dirty;
} Line;

// indent inside the editor is always made by spaces ' '

int line_get_indent(const Line* line);

int line_is_comment(const Line* line, size_t indent);

void line_set_indent(Line* line, size_t indent);

// replaces the text of the line
void line_replace(Line* line, char* new_text, size_t new_len);

void line_free(Line* line);

// Destructor of a line, callback to vector_init
void vector_line_destroy(void* ptr);

// Clipboard must be a logical representation of a clipboard buffer
// inside the program.
// clipboard is a LOCAL buffer, which means that the system's clipboard
// is not read or changed during the program execution

typedef struct {
	char* text;
	size_t len;
	int linewise;
} Clipboard;

void clipboard_free(Clipboard* cb);

#endif