#include <stdio.h>
#include "render.h"
#include "input.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

// contains the main loop

// void print_file(File* file) {
// 	printf("---- FILE CONTENT ----\n");

// 	for (size_t i = 0; i < file->lines.size; i++) {
// 		Line* line = vector_get(&file->lines, i);
// 		printf("%zu: %s", i, line->text);
// 	}

// 	printf("---------------------\n");
// }

#define CTRL_KEY(k) ((k) & 0x1f)
#define TAB_SIZE 4

int main(void) {
	File* file = file_open("test.txt");
	Cursor c = {0, 0};

	enable_raw_mode();
	// write(STDOUT_FILENO, "\033[2J", 4);

	while (1) {
		render_screen(file, &c);

		int key = read_key();

		if (key == 'q') {
			break;
		}

		Line* line = vector_get(&file->lines, c.y);
		size_t line_size = line->len;
		size_t line_count = file->lines.size;

		switch (key) {
			case KEY_ARROW_UP:
				cursor_move_up(&c);
				break;

			case KEY_ARROW_DOWN:
				cursor_move_down(&c, line_count);
				break;

			case KEY_ARROW_LEFT:
				cursor_move_left(&c);
				break;

			case KEY_ARROW_RIGHT:
				cursor_move_right(&c, line_size);
				break;

			case 127:
				if (c.x > 0) {
					file_delete_char(file, &c);
					c.x--;
				} else if (c.y > 0) {
					file_merge_lines(file, &c);
					c.y--;

					Line* line = vector_get(&file->lines, c.y);
					c.x = line->len;

					if (c.x > 0 && line->text[c.x - 1] == '\n') {
						c.x--;
					}
				}
				break;

			case '\t':
				for (int i = 0; i < TAB_SIZE; i++) {
					file_insert_char(file, &c, ' ');
					c.x++;
				}
				break;

			case '\r':
			case '\n':
				file_insert_newline(file, &c);
				c.y++;
				c.x = 0;
				break;

			case CTRL_KEY('s'):
				file_save(file);
				break;

			default:
				if (key >= 32 && key <= 126) {
					file_insert_char(file, &c, key);
					c.x++;
				}
				break;
		}

		line = vector_get(&file->lines, c.y);
		line_size = line->len;
		line_count = file->lines.size;

		cursor_clamp(&c, line_size, line_count);
	}

	file_free(file);
	return 0;
}