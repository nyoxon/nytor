#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define COMMENT_FMT "//"
#define START_BLOCK_COMMENT "/*"
#define END_BLOCK_COMMENT "*/"

/*
the original idea when developing the syntax highlight part was to
make the code as generic as possible to be able to integrate
with other languages different than C, but at the moment i'm doing
that i'm lazy as hell and i just want things to work :D
this is the reason why the following FMT's are
chars and not strings like the FMT's above
*/

#define MACRO_FMT '#'
#define MACRO_FMT_SIZE 1

#define START_STRING_FMT '\"'
#define END_STRING_FMT '\"'
#define STRING_FMT_SIZE 1

#define START_CHAR_FMT '\''
#define END_CHAR_FMT '\''
#define CHAR_FMT_SIZE 1

#define ESCAPE_CHAR '\\'

#define START_FUNCTION_FMT '('
// #define END_FUNCTION_FMT ')'

#define START_IMPORT_FMT '<'
#define END_IMPORT_FMT '>'
#define IMPORT_FMT_SIZE 1

#define LABEL_FMT ':' // C ONLY

// contains some types that contain some kind of buffer

// Line is a logical representation of a line (literally) inside the editor
// where a line is defined by ending or not with a '\n'

typedef struct {
	char* text;
	size_t len;
	// int dirty;
} Line;

// indent inside the editor is always made by spaces ' '

size_t line_get_indent(const Line* line);

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

typedef struct {
	size_t start;
	size_t end;
	size_t y;
} PatternMatch;

typedef struct {
	int active;
	char buf[256];
	size_t len;
	const char* label;
} Prompt;

void prompt_init(Prompt* pt, const char* label);

#endif