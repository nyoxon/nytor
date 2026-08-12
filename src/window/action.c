#include <assert.h>
#include <stdio.h>

#include "window/action.h"

static void window_cursor_update(Window* w) {
	size_t text_rows = window_height(w);

	while (w->cursor.pos.y >= text_rows + w->view.row_offset) {
		w->view.row_offset += text_rows;
	}

	while (w->cursor.pos.y  < w->view.row_offset) {
		if (w->view.row_offset >= text_rows) {
			w->view.row_offset -= text_rows;
		} 

		else {
			w->view.row_offset = 0;
		}
	}

	size_t text_cols = window_width(w);

	while (window_cursor_screen_x(w) >= 
		text_cols + w->view.col_offset)
	{
		w->view.col_offset += text_cols -
			WINDOW_HSO;
	}

	while (window_cursor_screen_x(w)  < w->view.col_offset) {
		if (w->view.col_offset >= text_cols) {
			w->view.col_offset -= text_cols
				- WINDOW_HSO;
		} 

		else {
			w->view.col_offset = 0;
		}
	}

	window_sync_cursor(w);
}

void window_cursor_move(Window* w, Position pos)
{
	size_t lines = w->content.size;

	if (pos.y >= lines) {
		return;
	}

	const u32string* line = vector_get(&w->content, pos.y);
	size_t line_size = u32string_size(line);

	if (pos.x > line_size) {
		return;
	}

	w->cursor.pos = pos;
	window_cursor_update(w);
}

void window_move_cursor_right(Window* w) {
	const u32string* line = vector_get_const(
		&w->content,
		w->cursor.pos.y
	);

	if (w->cursor.pos.x > u32string_size(line)) {
		return;
	}

	w->cursor.pos.x++;
	window_cursor_update(w);
}

void window_move_cursor_left(Window* w) {
	if (w->cursor.pos.x == 0) {
		return;
	}

	w->cursor.pos.x--;
	window_cursor_update(w);
}

void window_move_cursor_up(Window* w) {
	size_t y = (w->cursor.pos.y == 0)
		? w->content.size - 1
		: w->cursor.pos.y - 1;

	const u32string* text = vector_get_const(
		&w->content,
		y
	);

	size_t x = screen_to_file_x(
		text,
		w->cursor.preferred_column,
		w->tab_size
	);

	window_cursor_move(w, (Position) { x, y });
}

void window_move_cursor_down(Window* w) {
	size_t y = (w->cursor.pos.y == w->content.size - 1)
		? 0
		: w->cursor.pos.y + 1;

	const u32string* text = vector_get_const(
		&w->content,
		y
	);

	size_t x = screen_to_file_x(
		text,
		w->cursor.preferred_column,
		w->tab_size
	);

	window_cursor_move(w, (Position) { x, y } );	
}
