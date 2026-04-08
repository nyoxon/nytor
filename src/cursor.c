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