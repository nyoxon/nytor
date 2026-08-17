#include <assert.h>
#include <stdio.h>

#include "window/action.h"
#include "window/render.h"

void window_cursor_update(Window* w) {
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

void window_move_cursor_start_line(Window* window) {
	if (window->cursor.pos.x == 0) {
		return;
	}

	window->cursor.pos.x = 0;
	window_cursor_update(window);
}

void window_move_cursor_end_line(Window* window) {
	const u32string* text = vector_get_const(
		&window->content,
		window->cursor.pos.y
	);

	if (window->cursor.pos.x == u32string_size(text)) {
		return;
	}

	window->cursor.pos.x = u32string_size(text);
	window_cursor_update(window);
}


void window_scroll_up(Window* w, int follow_scroll) {
	if (w->view.row_offset == 0) {
		return;
	}

	w->view.row_offset--;

	if (follow_scroll &&
		w->cursor.pos.y > w->view.row_offset +
		window_height(w) - 1) 
	{
		w->cursor.pos.y--;
		w->cursor.pos.x = 0;
	}
}

void window_scroll_down(Window* w, int follow_scroll) {
	size_t lines = w->content.size;

	if (w->view.row_offset + window_height(w) >= lines) {
		return;
	}

	w->view.row_offset++;

	if (follow_scroll &&
		w->cursor.pos.y < w->view.row_offset)
	{
		w->cursor.pos.y++;
		w->cursor.pos.x = 0;
	}
}

void window_scroll_right(Window* window, int follow_scroll) {
	const u32string* text = vector_get_const(
		&window->content,
		window->cursor.pos.y
	);

	if (window->view.col_offset == u32string_size(text)) {
		return;
	}

	window->view.col_offset++;

	if (follow_scroll) {
		window_clamp_cursor_to_view(window);
	}
}

void window_scroll_left(Window* w, int follow_scroll) {
	if (w->view.col_offset == 0) {
		return;
	}

	w->view.col_offset--;

	if (follow_scroll) {
		window_clamp_cursor_to_view(w);
	}
}