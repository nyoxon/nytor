#ifndef CURSOR_H
#define CURSOR_H

// definition of a Cursor and functions to handle it

// a Cursor must be a logical representation of
// the cursor that is rendered on terminal

#include <stddef.h>

typedef struct {
	size_t x;
	size_t y;
} Cursor;

void cursor_move_to(Cursor*c, size_t cx, size_t cy);
void cursor_move_left(Cursor* c);
void cursor_move_right(Cursor* c,size_t line_size);
void cursor_move_up(Cursor* c);
void cursor_move_down(Cursor*c, size_t line_count);
void cursor_clamp(Cursor* c, size_t line_size, size_t line_count);

#endif