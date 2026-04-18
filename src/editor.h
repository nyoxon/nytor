#ifndef EDITOR_H
#define EDITOR_H

#include "file.h"
#include "scroll.h"
#include "error.h"
#include "debug.h"

extern const char* MACROS[];
extern const char* KEYWORDS[];
extern const char* COLORS[];

#define KEYWORD_COUNT (sizeof(KEYWORDS)/sizeof(KEYWORDS[0]))
#define MACROS_COUNT (sizeof(MACROS)/sizeof(MACROS[0]))
#define COLOR_SIZE 5

#define COMMENT_COLOR "\033[90m"
#define TYPE_COLOR "\033[34m"
#define FLOW_COLOR "\033[31m"
#define SPECIFIER_COLOR "\033[31m"
#define NUMBER_COLOR "\033[36m"
#define FUNCTION_COLOR "\033[33m"
#define MACRO_COLOR "\033[32m"

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