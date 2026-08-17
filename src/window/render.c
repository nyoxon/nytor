#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "window/render.h"

void window_move_screen_cursor(const Window* w);
static void move_terminal_cursor(size_t x, size_t y);

static size_t draw_textu8
(
	const char* text,
	size_t horizontal_limit
);

void window_draw
(
	const Window* w,
	const struct window_color* color
) 
{
	Position pos = w->pos;

	size_t max_width = w->tsize.cols;
	size_t max_height = w->tsize.rows;

	if (pos.x > max_width || pos.y > max_height) {
		return;
	}

	move_terminal_cursor(pos.x, pos.y);

	write_color(&color->border);
	u32_print_cp(UP_LEFT);

	size_t screen_rows = MIN(max_height - pos.y, window_height(w));
	size_t screen_cols = MIN(max_width - pos.x, window_width(w));

	size_t middle;
	char tmp[64];

	if (w->view.row_offset > 0) {
		sprintf(tmp, "+%zu", w->view.row_offset);
		middle = (window_width(w) - strlen(tmp)) / 2;
	}

	else {
		middle = window_width(w);
		tmp[0] = '\0';
	}

	for (size_t i = 0; i < screen_cols; i++) {
		if (i == middle) {
			size_t printed = draw_textu8(tmp, screen_cols);
			i += printed;
		}

		u32_print_cp(HORIZONTAL);
	}

	if (pos.x + window_width(w) <= max_width) {
		u32_print_cp(UP_RIGHT);
	}


	const Vector* content = &w->content;

	for (size_t y = 0; y < screen_rows; y++) {
		move_terminal_cursor(pos.x, pos.y + y + 1);
		u32_print_cp(VERTICAL);

		size_t window_row = y + w->view.row_offset;

		reset_color();

		if (window_row < content->size) {
			if (window_row == w->cursor.pos.y) {
				write_color(&color->current_line);
			}

			else {
				write_color(&color->text);
			}

			const u32string* text = vector_get_const(content, window_row);

			size_t printed = u32string_print_range(
				text,
				w->view.col_offset,
				screen_cols
			);

			size_t remaining = screen_cols - printed;

			for (size_t i = 0; i < remaining; i++) {
				u32_print_cp(U' ');
			}
		}

		else {
			write_color(&color->text);

			for (size_t j = 0; j < screen_cols; j++) {
				u32_print_cp(U' ');
			}
		}

		reset_color();
		
		write_color(&color->border);
		if (pos.x + window_width(w) <= max_width) {
			u32_print_cp(VERTICAL);
		}
	}

	if (pos.y + window_height(w) <= max_height) {
		move_terminal_cursor(pos.x, pos.y + 1 + window_height(w));

		u32_print_cp(DOWN_LEFT);

		if (content->size > screen_rows + w->view.row_offset) {
			sprintf(tmp, "+%zu", content->size - 
				(screen_rows + w->view.row_offset));

			middle = (window_width(w) - strlen(tmp)) / 2;
		}

		else {
			middle = window_width(w);
			tmp[0] = '\0';
		}

		for (size_t i = 0; i < screen_cols; i++) {
			if (i == middle) 
			{
				size_t printed = draw_textu8(tmp, screen_cols);

				i += printed;
			}

			u32_print_cp(HORIZONTAL);
		}

		if (pos.x + window_width(w) <= max_width) {
			u32_print_cp(DOWN_RIGHT);
		}
	}

	reset_color();
	window_move_screen_cursor(w);
}

void window_move_screen_cursor(const Window* w) {
	size_t cursor_screen_x = window_cursor_screen_x(w);

	Position screen_cursor = (Position) {
		cursor_screen_x + w->pos.x + 1 - w->view.col_offset,
		w->pos.y + w->cursor.pos.y + 1 - w->view.row_offset		
	};

	move_terminal_cursor(screen_cursor.x, screen_cursor.y);
}

void window_clamp_cursor_to_view(Window* w) {
	const u32string* line = vector_get_const(
		&w->content,
		w->cursor.pos.y
	);

	size_t cursor_screen_x = file_to_screen_x(
		line,
		w->cursor.pos.x,
		w->tab_size
	);

	size_t left = w->view.col_offset;
	size_t right = left + window_width(w) - 1;

	if (cursor_screen_x < left) {
		w->cursor.pos.x = screen_to_file_x(
			line,
			left,
			w->tab_size
		);

		window_sync_cursor(w);
	}

	else if (cursor_screen_x > right) {
		w->cursor.pos.x = screen_to_file_x(
			line,
			right,
			w->tab_size
		);

		window_sync_cursor(w);
	}
}

static void move_terminal_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\033[%zu;%zuH", y + 1, x + 1);
	write(STDOUT_FILENO, buf, len);
}

static size_t draw_textu8
(
	const char* text,
	size_t horizontal_limit
)
{
	if (horizontal_limit > strlen(text)) {
		horizontal_limit = strlen(text);
	}

	for (size_t i = 0; i < horizontal_limit; i++) {
		write(STDOUT_FILENO, &text[i], 1);
	}

	return horizontal_limit;
}