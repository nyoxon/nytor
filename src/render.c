#include "render.h"
#include <unistd.h>
#include <stdio.h>

static void clear_screen() {
	write(STDOUT_FILENO, "\033[2J", 4);
}

static void move_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\033[%zu;%zuH", y + 1, x + 1);
	write(STDOUT_FILENO, buf, len);
}

void render_screen(File* file, Cursor* cursor) {
	clear_screen();
	
	write(STDOUT_FILENO, "\033[H", 3);

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = vector_get(&file->lines, i);
		write(STDOUT_FILENO, line->text, line->len);
	}

	move_cursor(cursor->x, cursor->y);
}