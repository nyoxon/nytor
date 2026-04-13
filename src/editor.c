#include "editor.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "input.h"

#define DEFAULT_TERMINAL_WIDTH 50
#define DEFAULT_TERMINAL_HEIGHT 24

int editor_init(Editor* editor, const char* filename) {
	editor->new_file = 0;

	if (file_open(&editor->file, filename, &editor->new_file) < 0) {
		return -1;
	}

	editor->filename = filename;

	editor->tsize.rows = DEFAULT_TERMINAL_HEIGHT;
	editor->tsize.cols = DEFAULT_TERMINAL_WIDTH;

	editor->cursor.x = 0;
	editor->cursor.y = 0;

	editor->view.row_offset = 0;
	editor->view.col_offset = 0;

	selection_clear(&editor->sel);
	editor->selecting = 0;

	editor->cb.text = NULL;
	editor->cb.len = 0;
	editor->cb.linewise = 0;

	return 0;
}

void clean_terminal() {
	write(STDOUT_FILENO, "\033[2J", 4);
	write(STDOUT_FILENO, "\033[H", 3);
}

void editor_free(Editor* editor) {
	if (!editor) {
		return ;
	}

	clipboard_free(&editor->cb);
	file_free(&editor->file);
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

static size_t get_gutter_width(size_t line_count) {
	int digits = 1;
	size_t tmp = line_count;

	while (tmp >= 10) {
		tmp /= 10;
		digits++;
	}
	
	return digits + 1;	
}

static void index_line(size_t y, size_t line_count) {
	int digits = 1;
	size_t tmp = line_count;

	while (tmp >= 10) {
		tmp /= 10;
		digits++;
	}

	char buf[32];
	int len = snprintf(buf, sizeof(buf), "%*zu ", digits, y + 1);

	write(STDOUT_FILENO, "\033[34m", 5);
	write(STDOUT_FILENO, buf, len);
	write(STDOUT_FILENO, "\033[0m", 4);
}

void editor_render(Editor* editor) {
	clean_terminal();
	write(STDOUT_FILENO, "\033[H", 3);

	size_t screen_rows = editor->tsize.rows;
	size_t screen_cols = editor->tsize.cols;
	size_t gutter_width = get_gutter_width(editor->file.lines.size);

	size_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	if (editor->sel.active) {
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);
	}

	for (size_t y = 0; y < screen_rows; y++) {
		size_t file_row = y + editor->view.row_offset;

		if (file_row >= editor->file.lines.size) {
			break;
		}

		Line* line = vector_get(&editor->file.lines, file_row);

		index_line(file_row, editor->file.lines.size);

		size_t start = editor->view.col_offset;
		size_t end = start + screen_cols;

		if (start > line->len) {
			start = line->len;
		}

		if (end > line->len) {
			end = line->len;
		}

		int in_selection = 0;

		/*inefficient, since it iterates over all the chars 
		on the planet (see alternative later)*/
		for (size_t x = start; x < end; x++) {
			int selected = is_selected(&editor->sel, x, file_row, x1, y1, x2, y2);

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

		write(STDOUT_FILENO, "\n", 1);

		if (in_selection) {
			write(STDOUT_FILENO, "\033[0m", 4);
		}
	}

	move_cursor(editor->cursor.x + gutter_width - editor->view.col_offset,
				editor->cursor.y - editor->view.row_offset);
}

int editor_quit(Editor* editor) {
	if (editor->file.dirty) {
		while (1) { // blocks the main thread
			clean_terminal();
			char msg[] = "save before exit?\n(y/n)";       
			write(STDOUT_FILENO, msg, strlen(msg));

			int key = read_key();

			if (key == 'y' || key == 'Y') {
				file_save(&editor->file);
				break;
			}

			if (key == 'n' || key == 'N') {
				if (editor->new_file) {
					unlink(editor->filename);
				}

				break;
			}
		}
	}

	return 0;
}

static char get_delimiter_pair(int key) {
	switch (key) {
		case '{' : return '}';
		case '(' : return ')';
		case '[' : return ']';
	}

	return ' ';
}

static void editor_handle_open_delimiters(Editor* editor, int key) {
	char open = key;
	char close = get_delimiter_pair(open);

	file_insert_char(&editor->file, &editor->cursor, open);
	file_insert_char(&editor->file, &editor->cursor, close);
	editor->cursor.x--;
}

static void editor_handle_close_delimiters
(
	Editor* editor, 
	Line* line, 
	int key
) 
{
	if (editor->cursor.x < line->len &&
	 	line->text[editor->cursor.x] == key) {
		editor->cursor.x++;
	} else {
		file_insert_char(&editor->file, &editor->cursor, key);
	}	
}

static void editor_handle_ctrl_w(Editor* editor) {
	editor->cursor.x = 0;
}

static void editor_handle_ctrl_e(Editor* editor, const Line* line) {
	size_t indent = line_get_indent(line);
	editor->cursor.x = indent;
}

static void editor_handle_ctrl_d(Editor* editor, size_t line_size) {
	editor->cursor.x = line_size;
}

static void editor_handle_ctrl_a(Editor* editor) {
	if (editor->selecting) {
		selection_clear(&editor->sel);
		editor->selecting = 0;
	} else {
		file_select_all_file(&editor->file, &editor->sel, &editor->cursor);
		editor->selecting = 1;
	}	
}

static void editor_handle_ctrl_l(Editor* editor) {
	if (editor->sel.active) {
		selection_clear(&editor->sel);
		editor->selecting = 0;
	} else {
		file_select_line(&editor->file, editor->cursor.y, &editor->sel);
		editor->selecting = 1;
	}	
}

static void editor_handle_ctrl_c(Editor* editor) {
	if (editor->sel.active) {
		file_copy_selection(&editor->file, &editor->cb, &editor->sel);
		selection_clear(&editor->sel);
		editor->selecting = 0;
	}	
}

static void editor_handle_ctrl_v(Editor* editor) {
	if (editor->cb.text) {
		file_paste_clipboard(&editor->file, &editor->cb, &editor->cursor);
	}
}

static void editor_handle_space(Editor* editor) {
	if (editor->selecting) {
		selection_clear(&editor->sel);
		editor->selecting = 0;
	} else {
		selection_start(&editor->sel, &editor->cursor);
		editor->selecting = 1;
	}
}

static void editor_handle_ctrl_s(Editor* editor) {
	file_save(&editor->file);
}

static void editor_handle_arrow_up(Editor* editor) {
	cursor_move_up(&editor->cursor);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}
}

static void editor_handle_arrow_down(Editor* editor, size_t line_count) {
	cursor_move_down(&editor->cursor, line_count);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	} 	
}

static void editor_handle_arrow_right(Editor* editor, size_t line_size) {
	cursor_move_right(&editor->cursor, line_size);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}       
}

static void editor_handle_arrow_left(Editor* editor) {
	cursor_move_left(&editor->cursor);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}       
}

static void editor_handle_ctrl_arrow_up(Editor* editor) {
	if (editor->view.row_offset > 0) {
		editor->view.row_offset--;

		if (editor->cursor.y > editor->view.row_offset + editor->tsize.rows - 1) {
			editor->cursor.y--;
			editor->cursor.x = 0;
		}
	}
}

static void editor_handle_ctrl_arrow_down(Editor* editor, size_t line_count) {
	if (editor->view.row_offset + editor->tsize.rows < line_count) {
		editor->view.row_offset++;

		if (editor->cursor.y < editor->view.row_offset) {
			editor->cursor.y++;
			editor->cursor.x = 0;
		}
	}	
}

static void editor_handle_alt_arrow_up(Editor* editor) {
	if (editor->view.row_offset > 0) {
		size_t delta = editor->tsize.rows;

		if (delta > editor->view.row_offset) {
			delta = editor->view.row_offset;
		}

		editor->view.row_offset -= delta;

		if (editor->cursor.y >= delta) {
			editor->cursor.y -= delta;
		} else {
			editor->cursor.y = 0;
		}
	}
}

static void editor_handle_alt_arrow_down(Editor* editor, size_t line_count) {
	size_t max_offset = (line_count > editor->tsize.rows) 
		? line_count - editor->tsize.rows : 0;

	if (editor->view.row_offset < max_offset) {
		size_t delta = editor->tsize.rows;

		if (editor->view.row_offset + delta > max_offset) {
			delta = max_offset - editor->view.row_offset;
		}

		editor->view.row_offset += delta;
		editor->cursor.y += delta;

		if (editor->cursor.y >= line_count) {
			editor->cursor.y = line_count - 1;
		}
	}
}

static void editor_handle_ctrl_shift_arrow_up(Editor* editor) {
	file_move_line_up(&editor->file, &editor->cursor.y);
}

static void editor_handle_ctrl_shift_arrow_down(Editor* editor) {
	file_move_line_down(&editor->file, &editor->cursor.y);	
}

static void editor_handle_backspace(Editor* editor) {
	if (editor->sel.active) {
		if (editor->selecting) {
			editor->selecting = 0;
		}

		file_delete_selection(&editor->file, &editor->cursor, &editor->sel);
	} else {
		if (editor->cursor.x > 0) {
			file_delete_char(&editor->file, &editor->cursor);
		} else if (editor->cursor.y > 0) {
			file_merge_lines(&editor->file, &editor->cursor);
		}
	}
}

static void editor_handle_tab(Editor* editor) {
	for (int i = 0; i < TAB_SIZE; i++) {
		file_insert_char(&editor->file, &editor->cursor, ' ');
	}
}

static void editor_handle_newline(Editor* editor, const Line* line) {
	int is_brace_pair = 
		(editor->cursor.x > 0 && editor->cursor.x < line->len) &&
		(line->text[editor->cursor.x - 1] == '{' &&
		 line->text[editor->cursor.x] == '}');

	int indent = line_get_indent(line);
	file_insert_newline(&editor->file, &editor->cursor);
	editor->cursor.y++;
	editor->cursor.x = 0;

	if (is_brace_pair) {
		for (int i = 0; i < indent; i++) {
			file_insert_char(&editor->file, &editor->cursor, ' ');
		}

		file_insert_newline(&editor->file, &editor->cursor);
		editor->cursor.y++;

		Line* closing = vector_get(&editor->file.lines, editor->cursor.y);

		line_set_indent(closing, indent);
		editor->cursor.y--;

		editor->cursor.x = indent;
		for (int i = 0; i < TAB_SIZE; i++) {
			file_insert_char(&editor->file, &editor->cursor, ' ');
		}
	} else {
		for (int i = 0; i < indent; i++) {
			file_insert_char(&editor->file, &editor->cursor, ' ');
		}
	}	
}

int editor_handle_input(Editor* editor, int key) {
	if (key == CTRL_KEY('q')) {
		return editor_quit(editor);
	}

	Line* line = vector_get(&editor->file.lines, editor->cursor.y);
	size_t line_size = line->len;
	size_t line_count = editor->file.lines.size;

	switch (key) {
		case '{':
		case '(':
		case '[': {
			editor_handle_open_delimiters(editor, key);
			break;
		}

		case '}':
		case ')':
		case ']': {
			editor_handle_close_delimiters(editor, line, key);
			break;
		}

		case CTRL_KEY('w'):
			editor_handle_ctrl_w(editor);
			break;

		case CTRL_KEY('e'):
			editor_handle_ctrl_e(editor, line);
			break;

		case CTRL_KEY('d'):
			editor_handle_ctrl_d(editor, line_size);
			break;

		case CTRL_KEY('a'):
			editor_handle_ctrl_a(editor);
			break;

		case CTRL_KEY('l'):
			editor_handle_ctrl_l(editor);
			break;

		case CTRL_KEY('c'):
			editor_handle_ctrl_c(editor);
			break;

		case CTRL_KEY('v'):
			editor_handle_ctrl_v(editor);
			break;

		case CTRL_KEY(' '):
			editor_handle_space(editor);
			break;

		case CTRL_KEY('s'):
			editor_handle_ctrl_s(editor);
			break;

		case '\t':
			editor_handle_tab(editor);
			break;

		case '\n':
		case '\r': {
			editor_handle_newline(editor, line);
			break;
		}

		case 127:
			editor_handle_backspace(editor);
			break;

		case KEY_ARROW_UP:
			editor_handle_arrow_up(editor);
			break;

		case KEY_ARROW_DOWN:
			editor_handle_arrow_down(editor, line_count);
			break;

		case KEY_ARROW_RIGHT:
			editor_handle_arrow_right(editor, line_size);
			break;

		case KEY_ARROW_LEFT:
			editor_handle_arrow_left(editor);
			break;

		case KEY_CTRL_ARROW_UP:
			editor_handle_ctrl_arrow_up(editor);
			break;

		case KEY_CTRL_ARROW_DOWN:
			editor_handle_ctrl_arrow_down(editor, line_count);
			break;

		case KEY_ALT_ARROW_UP:
			editor_handle_alt_arrow_up(editor);
			break;

		case KEY_ALT_ARROW_DOWN:
			editor_handle_alt_arrow_down(editor, line_count);
			break;

		case KEY_CTRL_SHIFT_ARROW_UP:
			editor_handle_ctrl_shift_arrow_up(editor);
			break;

		case KEY_CTRL_SHIFT_ARROW_DOWN:
			editor_handle_ctrl_shift_arrow_down(editor);
			break;

		default:
			if (key >= 32 && key <= 126) {
				file_insert_char(&editor->file, &editor->cursor, key);
			}

			break;
	}

	return 1;
}