#ifndef EDITOR_H
#define EDITOR_H

#include "file.h"
#include "scroll.h"
#include <signal.h>

typedef struct {
	File file;
	Cursor cursor;
	View view;
	TerminalSize tsize;
	Selection sel;
	Clipboard cb;

	int selecting;
	int new_file;

	const char* filename;
} Editor;

int editor_init(Editor* editor, const char* filename);
void editor_free(Editor* editor);
void editor_render(Editor* editor);
int editor_handle_input(Editor* editor, int key);

int editor_quit(Editor* editor);

void clean_terminal();

#endif