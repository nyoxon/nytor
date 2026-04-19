#ifndef EDITOR_H
#define EDITOR_H

#include "file.h"
#include "scroll.h"
#include "error.h"
#include "debug.h"

// in syntax_high.c
extern const char* MACROS[];
extern const char* KEYWORDS[];
extern const char* COLORS[];
extern const size_t KEYWORD_COUNT;
extern const size_t MACROS_COUNT;

// a Editor represents the editor itself
// it contains the global state of the program

typedef struct {
	File file;
	Cursor cursor;
	View view;
	TerminalSize tsize;
	Selection sel;
	Clipboard cb;

	int selecting;
	int new_file;
	int render_line_enumeration;
	int debug_mode;

	Result result;

	Log log;
} Editor;

int editor_init
(
	Editor* editor, 
	const char* filename, 
	int render_line_enumeration,
	int debug_mode
);

void editor_free(Editor* editor);

// prints on terminal
void editor_render(Editor* editor);
int editor_handle_input(Editor* editor, int key);

int editor_quit(Editor* editor);
void editor_log_write(const Editor* editor);

void clean_terminal();

#endif