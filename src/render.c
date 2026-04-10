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

static int is_selected(Selection* sel, size_t x, size_t y,
	size_t x1, size_t y1,
	size_t x2, size_t y2
)
{
	if (!sel->active) {
		return 0;
	}

	if (y < y1 || y > y2) {
		return 0;
	}

	if (y1 == y2) {
		return x >= x1 && x < x2;
	} 

	if (y == y1) {
		return x >= x1;
	}

	if (y == y2) {
		return x < x2;
	}

	return 1;
}

void render_screen(File* file, Cursor* cursor, View* view, TerminalSize* tsize, Selection* sel) {
	clear_screen();
	write(STDOUT_FILENO, "\033[H", 3);

	size_t screen_rows = tsize->rows;
	// size_t screen_cols = tsize->cols;

	size_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	if (sel->active) {
		selection_normalize(sel, &x1, &y1, &x2, &y2);
	}

	for (size_t y = 0; y < screen_rows; y++) {
		size_t file_row = y + view->row_offset;

		if (file_row >= file->lines.size) {
			break;
		}

		Line* line = vector_get(&file->lines, file_row);

		int in_selection = 0;

		/*inefficient, since it iterates over all the chars 
		on the planet (see alternative later)*/
		for (size_t x = 0; x < line->len; x++) {
			int selected = is_selected(sel, x, file_row, x1, y1, x2, y2);

			if (selected && !in_selection) {
				write(STDOUT_FILENO, "\033[7m", 4);
				in_selection = 1;
			}

			if (!selected && in_selection) {
				write(STDOUT_FILENO, "\033[0m", 4);
				in_selection = 0;
			}

			write(STDOUT_FILENO, &line->text[x], 1);
		}

		if (in_selection) {
			write(STDOUT_FILENO, "\033[0m", 4);
		}
	}

	move_cursor(cursor->x - view->col_offset, cursor->y - view->row_offset);
}