#ifndef CURSOR_H
#define CURSOR_H

// definition of a Cursor and functions to handle it

// a Cursor must be a logical representation of
// the cursor that is rendered on terminal

#include <stddef.h>

#include "util/types/position.h"
#include "util/types/u32string.h"

typedef struct {
	Position pos;
	size_t preferred_column;
} Cursor;

void cursor_move_to(Cursor*c, size_t cx, size_t cy);
void cursor_move_left(Cursor* c);
void cursor_move_right(Cursor* c,size_t line_size);
void cursor_move_up(Cursor* c);
void cursor_move_down(Cursor*c, size_t line_count);
void cursor_clamp(Cursor* c, size_t line_size, size_t line_count);

size_t char_screen_width
(
	uint32_t c,
	size_t screen_x,
	size_t tab_size
);

size_t file_to_screen_x
(
	const u32string* line,
	size_t file_x,
	size_t tab_size
);

size_t screen_to_file_x
(
	const u32string* line,
	size_t desired_screen_x,
	size_t tab_size
);

#endif