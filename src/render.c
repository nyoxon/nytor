#include "render.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static void clear_screen() {
	write(STDOUT_FILENO, "\033[2J", 4);
}

static void move_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\033[%zu;%zuH", y + 1, x + 1);
	write(STDOUT_FILENO, buf, len);
}

void render_screen(File* file, Cursor* cursor, View* view, TerminalSize* tsize) {
	clear_screen();
	write(STDOUT_FILENO, "\033[H", 3);

	size_t screen_rows = tsize->rows;
	size_t screen_cols = tsize->cols;

	for (size_t y = 0; y < screen_rows; y++) {
		size_t file_row = y + view->row_offset;

		if (file_row >= file->lines.size) {
			break;
		}

		Line* line = vector_get(&file->lines, file_row);

		if (view->col_offset < line->len) {
			size_t len = line->len - view->col_offset;

			if (len > screen_cols) {
				len = screen_cols;
			}

			write(STDOUT_FILENO, line->text + view->col_offset, len);
		}
	}

	move_cursor(cursor->x - view->col_offset, cursor->y - view->row_offset);
}