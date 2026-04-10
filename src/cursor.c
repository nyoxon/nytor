#include "cursor.h"

void cursor_move_to(Cursor*c, size_t cx, size_t cy) {
	c->x = cx;
	c->y = cy;
}

void cursor_move_left(Cursor* c) {
	if (c->x > 0) c->x--;
}

void cursor_move_right(Cursor* c, size_t line_size) {
	if (c->x < line_size) c->x++;
}

void cursor_move_up(Cursor* c) {
	if (c->y > 0) c->y--;
}

void cursor_move_down(Cursor* c, size_t line_count) {
	if (c->y < line_count - 1) c->y++;
}

void cursor_clamp(Cursor* c, size_t line_size, size_t line_count) {
	if (line_count == 0) {
		c->x = 0;
		c->y = 0;
		return;
	}

	if (c->y >= line_count) {
		c->y = line_count - 1;
	}

	if (c->x > line_size) {
		c->x = line_size;
	}
}

void selection_normalize
(
	Selection* sel,
	size_t* x1, size_t* y1,
	size_t* x2, size_t* y2
)
{
	if (sel->start_y < sel->end_y || 
	   (sel->start_y == sel->end_y && sel->start_x <= sel->end_x)) {
		*x1 = sel->start_x;
		*y1 = sel->start_y;
		*x2 = sel->end_x;
		*y2 = sel->end_y;
	} else {
		*x1 = sel->end_x;
		*y1 = sel->end_y;
		*x2 = sel->start_x;
		*y2 = sel->start_y;
	}
}

void selection_start(Selection* sel, Cursor* c) {
	sel->active = 1;
	sel->start_x = c->x;
	sel->start_y = c->y;
	sel->end_x = c->x;
	sel->end_y = c->y;
}

void selection_update(Selection* sel, Cursor* c) {
	if (!sel->active) {
		return;
	}

	sel->end_x = c->x;
	sel->end_y = c->y;
}

void selection_clear(Selection* sel) {
	sel->active = 0;
	sel->start_x = 0;
	sel->start_y = 0;
	sel->end_y = 0;
	sel->end_x = 0;
}
