#include "editor.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "input.h"

#define DEFAULT_TERMINAL_WIDTH 50
#define DEFAULT_TERMINAL_HEIGHT 24

int editor_init
(
	Editor* editor, 
	const char* filename, 
	int render_line_enumeration,
	int debug_mode
) 
{
	editor->new_file = 0;
	editor->result.reason = NULL;

	if (file_open(&editor->file, filename, &editor->new_file,
				  &editor->result) < 0) 
	{
		return -1;
	}

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

	editor->render_line_enumeration = render_line_enumeration;
	editor->debug_mode = debug_mode;

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

	result_free(&editor->result);
	clipboard_free(&editor->cb);
	file_free(&editor->file);

	if (editor->log.fd > 0) {
		close(editor->log.fd);
	}
}

void editor_log_write(const Editor* editor) {
	if (editor->log.fd < 0 || !editor->result.reason) {
		return;
	}

	log_write(&editor->log, editor->result.reason);
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

		if (editor->render_line_enumeration) {
			index_line(file_row, editor->file.lines.size);
		}

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

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_render: SUCCESS");
	}

	if (editor->render_line_enumeration) {

		size_t gutter_width = get_gutter_width(editor->file.lines.size);
		move_cursor(editor->cursor.x + 
					gutter_width - editor->view.col_offset,
					editor->cursor.y - editor->view.row_offset);
	} else {
		move_cursor(editor->cursor.x,
					editor->cursor.y - editor->view.row_offset);		
	}
}

int editor_quit(Editor* editor) {
	if (editor->file.dirty) {
		while (1) { // blocks the main thread
			clean_terminal();
			char msg[] = "save before exit?\n(y/n)";       
			write(STDOUT_FILENO, msg, strlen(msg));

			int key = read_key();

			if (key == 'y' || key == 'Y') {
				if (file_save(&editor->file, &editor->result) < 0) {
					return EIE_FATAL_ERROR;
				}

				break;
			}

			if (key == 'n' || key == 'N') {
				if (editor->new_file) {
					unlink(editor->file.filename);
				}

				break;
			}
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_quit: SUCCESS");
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

static int editor_handle_open_delimiters(Editor* editor, int key) {
	char open = key;
	char close = get_delimiter_pair(open);

	int ret = file_insert_char(&editor->file, &editor->cursor, open,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	ret = file_insert_char(&editor->file, &editor->cursor, close,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	editor->cursor.x--;

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_handle_open_delimiters: SUCCESS");
	}

	return 0;
}

static int editor_handle_close_delimiters
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
		int ret = file_insert_char(&editor->file, &editor->cursor, key,
			&editor->result);

		if (ret < 0) {
			return ret;
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_handle_close_delimiters: SUCCESS");
	}

	return 0;
}

static void editor_move_cursor_to_beginning_of_line(Editor* editor) {
	editor->cursor.x = 0;

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_beginning_of_line: SUCCESS");
	}
}

static void editor_move_cursor_to_indent_of_line
(
	Editor* editor, 
	const Line* line
) 
{
	size_t indent = line_get_indent(line);
	editor->cursor.x = indent;

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_indent_of_line: SUCCESS");
	}
}

static void editor_move_cursor_to_end_of_line
(
	Editor* editor, 
	size_t line_size
) 
{
	editor->cursor.x = line_size;

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_end_of_line: SUCCESS");
	}
}

static int editor_select_all_file(Editor* editor) {
	if (editor->selecting) {
		selection_clear(&editor->sel);
		editor->selecting = 0;

		return 0;
	} else {
		int ret = file_select_all_file(&editor->file, &editor->sel, &editor->cursor,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		editor->selecting = 1;

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_select_all_file: SUCCESS");
		}

		return 0;
	}	
}

static int editor_select_line(Editor* editor) {
	if (editor->sel.active) {
		selection_clear(&editor->sel);
		editor->selecting = 0;

		return 0;
	} else {
		int ret = file_select_line(&editor->file, editor->cursor.y, &editor->sel,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		editor->selecting = 1;

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_select_line: SUCCESS");
		}

		return 0;
	}	
}

static int editor_copy_selection(Editor* editor) {
	if (editor->sel.active) {
		int ret = file_copy_selection(&editor->file, &editor->cb, &editor->sel,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		selection_clear(&editor->sel);
		editor->selecting = 0;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_copy_selection: SUCCESS");
	}

	return 0;
}

static int editor_paste_clipboard(Editor* editor) {
	if (editor->cb.text) {
		int ret = file_paste_clipboard(&editor->file, &editor->cb, 
			&editor->cursor, &editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_paste_clipboard: SUCCESS");
	}

	return 0;
}

static void editor_start_and_create_selection(Editor* editor) {
	if (editor->selecting) {
		selection_clear(&editor->sel);
		editor->selecting = 0;
	} else {
		selection_start(&editor->sel, &editor->cursor);
		editor->selecting = 1;
	}
}

static int editor_save_file(Editor* editor) {
	int ret = file_save(&editor->file, &editor->result);

	if (ret < 0) {
		return ret;
	}

	if (ret == EIE_NOT_AN_ERROR) {
		return 0;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_save_file: SUCCESS");
	}

	return 0;
}

static void editor_move_cursor_up(Editor* editor) {
	cursor_move_up(&editor->cursor);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}
}

static void editor_move_cursor_down
(
	Editor* editor, 
	size_t line_count
) 
{
	cursor_move_down(&editor->cursor, line_count);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	} 	
}

static void editor_move_cursor_right(Editor* editor, size_t line_size) {
	cursor_move_right(&editor->cursor, line_size);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}       
}

static void editor_move_cursor_left(Editor* editor) {
	cursor_move_left(&editor->cursor);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}       
}

static void editor_scroll_up(Editor* editor) {
	if (editor->view.row_offset > 0) {
		editor->view.row_offset--;

		if (editor->cursor.y > editor->view.row_offset + editor->tsize.rows - 1) {
			editor->cursor.y--;
			editor->cursor.x = 0;
		}
	}
}

static void editor_scroll_down(Editor* editor, size_t line_count) {
	if (editor->view.row_offset + editor->tsize.rows < line_count) {
		editor->view.row_offset++;

		if (editor->cursor.y < editor->view.row_offset) {
			editor->cursor.y++;
			editor->cursor.x = 0;
		}
	}	
}

static void editor_scroll_up_terminal_size(Editor* editor) {
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

static void editor_scroll_down_terminal_size
(
	Editor* editor, 
	size_t line_count
)
{
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

static int editor_move_line_up(Editor* editor) {
	int ret = file_move_line_up(&editor->file, &editor->cursor.y, 
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_line_up: SUCCESS");
	}

	return 0;
}

static int editor_move_line_down(Editor* editor) {
	int ret = file_move_line_down(&editor->file, &editor->cursor.y, &editor->result);

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_line_down: SUCCESS");
	}

	return 0;
}

static int editor_delete_char_or_selection(Editor* editor) {
	int ret;

	if (editor->sel.active) {
		if (editor->selecting) {
			editor->selecting = 0;
		}

		ret = file_delete_selection(&editor->file, &editor->cursor,
			&editor->sel, &editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_delete_char_or_selection: SUCCESS");
		}

		return 0;

	} else {
		if (editor->cursor.x > 0) {
			ret = file_delete_char(&editor->file, &editor->cursor,
				&editor->result);

			if (ret < 0) {
				return ret;
			}

			if (editor->debug_mode) {
				log_write(&editor->log, "editor_delete_char_or_selection: SUCCESS");
			}

			return 0;
		} else if (editor->cursor.y > 0) {
			ret = file_merge_lines(&editor->file, &editor->cursor,
				&editor->result);

			if (ret < 0) {
				return ret;
			}

			if (editor->debug_mode) {
				log_write(&editor->log, "editor_delete_char_or_selection: SUCCESS");
			}

			return 0;
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_delete_char_or_selection: SUCCESS");
	}

	return 0;
}

static int editor_insert_tab(Editor* editor) {
	int ret;

	for (int i = 0; i < TAB_SIZE; i++) {
		ret = file_insert_char(&editor->file, &editor->cursor, ' ',
			&editor->result);

		if (ret < 0) {
			return ret;
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_insert_tab: SUCCESS");
	}

	return 0;
}

static int editor_insert_newline(Editor* editor, const Line* line) {
	int ret;
	int is_brace_pair = 
		(editor->cursor.x > 0 && editor->cursor.x < line->len) &&
		(line->text[editor->cursor.x - 1] == '{' &&
		 line->text[editor->cursor.x] == '}');

	int indent = line_get_indent(line);
	ret = file_insert_newline(&editor->file, &editor->cursor, &editor->result);

	if (ret < 0) {
		return ret;
	}

	editor->cursor.y++;
	editor->cursor.x = 0;

	if (is_brace_pair) {
		for (int i = 0; i < indent; i++) {
			ret = file_insert_char(&editor->file, &editor->cursor, ' ',
				&editor->result);

			if (ret < 0) {
				return ret;
			}
		}

		ret = file_insert_newline(&editor->file, &editor->cursor,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		editor->cursor.y++;

		Line* closing = vector_get(&editor->file.lines, editor->cursor.y);

		if (!closing) {
			result_set_reason(&editor->result,
				"editor_insert_newline: closing is a null pointer");
			editor->result.type = ERROR_NULL_POINTER;

			return EIE_FATAL_ERROR;
		}

		line_set_indent(closing, indent);
		editor->cursor.y--;

		editor->cursor.x = indent;
		for (int i = 0; i < TAB_SIZE; i++) {
			ret = file_insert_char(&editor->file, &editor->cursor, ' ',
				&editor->result);

			if (ret < 0) {
				return ret;
			}
		}
	} else {
		for (int i = 0; i < indent; i++) {
			ret = file_insert_char(&editor->file, &editor->cursor, ' ',
				&editor->result);

			if (ret < 0) {
				return ret;
			}
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_insert_newline: SUCCESS");
	}

	return 0;
}

int editor_handle_input(Editor* editor, int key) {
	if (key == CTRL_KEY('q')) {
		return editor_quit(editor);
	}

	Line* line = vector_get(&editor->file.lines, editor->cursor.y);

	if (!line) {
		result_set_reason(&editor->result,
			"editor_handle_input: line is a null pointer");
		editor->result.type = ERROR_NULL_POINTER;

		return 0;
	}

	size_t line_size = line->len;
	size_t line_count = editor->file.lines.size;
	int ret;

	switch (key) {
		case '{':
		case '(':
		case '[': {
			ret = editor_handle_open_delimiters(editor, key);

			if (ret < 0) {
				result_set_reason(&editor->result,
					"editor_handle_input: error return by static function");

				editor->result.type = ERROR_STATIC;

				if (ret == EIE_FATAL_ERROR) {
					return 0;
				}
			}

			break;
		}

		case '}':
		case ')':
		case ']': {
			ret = editor_handle_close_delimiters(editor, line, key);

			if (ret < 0) {
				result_set_reason(&editor->result,
					"editor_handle_input: error return by static function");

				editor->result.type = ERROR_STATIC;

				if (ret == EIE_FATAL_ERROR) {
					return 0;
				}		
			}

			break;
		}

		case CTRL_KEY('w'):
			editor_move_cursor_to_beginning_of_line(editor);
			break;

		case CTRL_KEY('e'):
			editor_move_cursor_to_indent_of_line(editor, line);
			break;

		case CTRL_KEY('d'):
			editor_move_cursor_to_end_of_line(editor, line_size);
			break;

		case CTRL_KEY('a'):
			int ret = editor_select_all_file(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY('l'):
			ret = editor_select_line(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY('c'):
			ret = editor_copy_selection(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY('v'):
			ret = editor_paste_clipboard(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY(' '):
			editor_start_and_create_selection(editor);
			break;

		case CTRL_KEY('s'):
			ret = editor_save_file(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case '\t':
			ret = editor_insert_tab(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case '\n':
		case '\r': {
			ret = editor_insert_newline(editor, line);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;
		}

		case 127:
			ret = editor_delete_char_or_selection(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case KEY_ARROW_UP:
			editor_move_cursor_up(editor);
			break;

		case KEY_ARROW_DOWN:
			editor_move_cursor_down(editor, line_count);
			break;

		case KEY_ARROW_RIGHT:
			editor_move_cursor_right(editor, line_size);
			break;

		case KEY_ARROW_LEFT:
			editor_move_cursor_left(editor);
			break;

		case KEY_CTRL_ARROW_UP:
			editor_scroll_up(editor);
			break;

		case KEY_CTRL_ARROW_DOWN:
			editor_scroll_down(editor, line_count);
			break;

		case KEY_ALT_ARROW_UP:
			editor_scroll_up_terminal_size(editor);
			break;

		case KEY_ALT_ARROW_DOWN:
			editor_scroll_down_terminal_size(editor, line_count);
			break;

		case KEY_CTRL_SHIFT_ARROW_UP:
			ret = editor_move_line_up(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case KEY_CTRL_SHIFT_ARROW_DOWN:
			ret = editor_move_line_down(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		default:
			if (key >= 32 && key <= 126) {
				ret = file_insert_char(&editor->file, &editor->cursor, 
					key, &editor->result);

				if (ret == EIE_FATAL_ERROR) {
					return 0;
				}

				if (editor->debug_mode) {
					log_write(&editor->log, 
						"editor_handle_input: a char has been written (%c)", key);
				}
			}


			break;
	}

	return 1;
}