#include <stdio.h>
#include "render.h"
#include "input.h"
#include "scroll.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

// contains the main loop

#define CTRL_KEY(k) ((k) & 0x1f)
#define TAB_SIZE 4

volatile sig_atomic_t exit_requested = 0;

void handle_exit_signal() {
	exit_requested = 1;
}

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		printf("USAGE: %s [filename]\n", argv[0]);
		return -1;
	}

	signal(SIGTERM, handle_exit_signal);
	enable_raw_mode();

	File* file = file_open(argv[1]);
	Cursor c = {0, 0};
	View view = {0, 0};
	TerminalSize tsize = {.rows = 24, .cols = 80};
	Line* clipboard = NULL;

	while (1) {	
		render_screen(file, &c, &view, &tsize);

		int key = read_key();

		if (exit_requested) {
			write(STDOUT_FILENO, "\033[2J", 4);
			write(STDOUT_FILENO, "\033[H", 3);

			if (clipboard) {
				free(clipboard->text);
				free(clipboard);
			}

			disable_raw_mode();
			file_free(file);
			exit(0);
		}

		if (key == CTRL_KEY('q')) {
			if (file->dirty) {
				write(STDOUT_FILENO, "\033[2J", 4);
				write(STDOUT_FILENO, "\033[H", 3);
				char msg[] = "file has been changed but not saved. do you want to save?\n(y or n)";		
				write(STDOUT_FILENO, msg, strlen(msg));

				while (1) {
					key = read_key();

					if (key == 'y') {
						file_save(file);
						break;
					}

					if (key == 'n') {
						break;
					}
				}
			}

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

		update_terminal_size(&tsize);
		update_scroll(&view, &c, &tsize);
	}

	write(STDOUT_FILENO, "\033[2J", 4);
	write(STDOUT_FILENO, "\033[H", 3);

	if (clipboard) {
		free(clipboard->text);
		free(clipboard);
	}

	file_free(file);
	return 0;
}