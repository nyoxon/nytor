#ifndef CURSOR_H
#define CURSOR_H

#include <stddef.h>

typedef struct {
	size_t x;
	size_t y;
} Cursor;

typedef struct {
	int active;
	size_t start_x;
	size_t start_y;
	size_t end_x;
	size_t end_y;
} Selection;

void cursor_move_to(Cursor*c, size_t cx, size_t cy);
void cursor_move_left(Cursor* c);
void cursor_move_right(Cursor* c,size_t line_size);
void cursor_move_up(Cursor* c);
void cursor_move_down(Cursor*c, size_t line_count);
void cursor_clamp(Cursor* c, size_t line_size, size_t line_count);

void selection_normalize
(
	Selection* sel,
	size_t* x1, size_t* y1,
	size_t* x2, size_t* y2
);

void selection_start(Selection* sel, Cursor* c);
void selection_update(Selection* sel, Cursor* c);
void selection_clear(Selection* sel);

#endif