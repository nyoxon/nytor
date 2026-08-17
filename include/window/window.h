#ifndef EDITOR_WINDOW_H
#define EDITOR_WINDOW_H

#include "util/types/u32string.h"
#include "util/types/position.h"
#include "util/types/cursor.h"
#include "editor/core/scroll.h"

#define WINDOW_HSO 4

typedef struct Window Window;

typedef enum {
	WINDOW_KEEP_OPEN,
	WINDOW_CLOSE,
	WINDOW_QUIT_PROGRAM,
} WindowResult;

typedef WindowResult (*WindowSelectCallback) (
	void* userdata
);

typedef enum {
	WINDOWPOS_CUSTOM,
	WINDOWPOS_CENTRALIZED
} WindowPosType;

typedef struct {
	Position pos;
	WindowPosType pos_type;

	float sw; 
	float sh;

	TerminalSize tsize;
	size_t tab_size;
	
	WindowSelectCallback on_select;
} WindowOptions;

struct Window {
	Position pos;

	float sw; float sh; // relative values

	size_t tab_size;

	Vector content;

	Cursor cursor;
	View view;
	TerminalSize tsize;

	WindowSelectCallback on_select;
};

// takes ownership
Window window_new(WindowOptions* options);
void window_close(Window* w);

size_t window_width(const Window* w);
size_t window_height(const Window* w);

size_t window_cursor_screen_x(const Window* w);
void window_sync_cursor(Window* w);

void window_update_size(Window* w, TerminalSize* tsize);
#endif