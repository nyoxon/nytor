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

void clean_terminal() {
	write(STDOUT_FILENO, "\033[2J", 4);
	write(STDOUT_FILENO, "\033[H", 3);	
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
	Selection sel = {0};
	int selecting = 0;
	Line* clipboard = NULL;

	while (1) {	
		render_screen(file, &c, &view, &tsize, &sel);

		int key = read_key();

		if (exit_requested) {
			clean_terminal();

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
				clean_terminal();
				char msg[] = "file has been changed but not saved. do you want to save?\n(y) or (n)";		
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
			case '{':
			case '(':
			case '[': {
				char open = key;
				char close;

				switch (open) {
					case '{' : close = '}'; break;
					case '(' : close = ')'; break;
					case '[' : close = ']'; break; 
				}

				file_insert_char(file, &c, open);
				c.x++;

				file_insert_char(file, &c, close);

				break;
			}

			case '}':
			case ')':
			case ']': {
				if (c.x < line->len && line->text[c.x] == key) {
					c.x++;
				} else {
					file_insert_char(file, &c, key);
					c.x++;
				}

				break;
			}

			case CTRL_KEY('a'):
				if (selecting) {
					selection_clear(&sel);
					selecting = 0;
				} else {
					file_select_all_file(file, &sel, &c);
					selecting = 1;
				}

				break;

			case CTRL_KEY(' '):
				if (selecting) {
					selection_clear(&sel);
					selecting = 0;
				} else {
					selection_start(&sel, &c);
					selecting = 1;
				}

				break;

			case CTRL_KEY('l'):
				if (sel.active) {
					selection_clear(&sel);
					selecting = 0;
				} else {
					file_select_line(file, c.y, &sel);
					selecting = 1;
				}

				break;

			case CTRL_KEY('c'):
				if (sel.active) {
					file_copy_selection(file, &clipboard, &sel);
					selection_clear(&sel);
					selecting = 0;
				}

				break;

			case CTRL_KEY('v'):
				if (clipboard) {
					file_paste_clipboard(file, clipboard, &c);
				}

				break;

			case KEY_CTRL_ARROW_UP:
				file_move_line_up(file, &c.y);
				break;

			case KEY_CTRL_ARROW_DOWN:
				file_move_line_down(file, &c.y);
				break;

			case KEY_ARROW_UP:
				cursor_move_up(&c);

				if (selecting) {
					selection_update(&sel, &c);
				}		

				break;

			case KEY_ARROW_DOWN:
				cursor_move_down(&c, line_count);

				if (selecting) {
					selection_update(&sel, &c);
				}		

				break;

			case KEY_ARROW_LEFT:
				cursor_move_left(&c);

				if (selecting) {
					selection_update(&sel, &c);
				}		

				break;

			case KEY_ARROW_RIGHT:
				cursor_move_right(&c, line_size);

				if (selecting) {
					selection_update(&sel, &c);
				}		

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
			case '\n': {
				int is_brace_pair = 
					(c.x > 0 && c.x < line->len) &&
					 (line->text[c.x - 1] == '{' &&
					 line->text[c.x] == '}');

				file_insert_newline(file, &c);
				c.y++;
				c.x = 0;

				int depth = file_compute_indent_level(file, c.y);
				int cur_indent = line_get_indent(line);
				int indent = (depth == 0) ? cur_indent : TAB_SIZE * depth;

				if (is_brace_pair) {
					for (int i = 0; i < indent; i++) {
						file_insert_char(file, &c, ' ');
						c.x++;
					}

					file_insert_newline(file, &c);
					c.y++;

					Line* closing = vector_get(&file->lines, c.y);
					int close_depth = file_compute_indent_level(file, c.y) - 1;

					if (close_depth < 0) {
						close_depth = 0;
					}

					line_set_indent(closing, close_depth * TAB_SIZE);
					c.y--;

					c.x = indent;
				} else {
					for (int i = 0; i < cur_indent; i++) {
						file_insert_char(file, &c, ' ');
						c.x++;
					}
				}


				break;
			}

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