#include "editor.h"
#include "input.h"
#include "syntax_high.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

static int check_is_macro(const char* word, size_t len) {
	for (size_t i = 0; i < MACROS_COUNT; i++) {
		if (strlen(MACROS[i]) == len &&
			strncmp(word, MACROS[i], len) == 0)
		{
			return 1;
		}
	}

	return 0;	
}

void editor_render(Editor* editor) {
	clean_terminal();
	write(STDOUT_FILENO, "\033[H", 3);

	size_t screen_rows = editor->tsize.rows;
	size_t screen_cols = editor->tsize.cols;

	size_t start_comment_len = strlen(START_BLOCK_COMMENT);
	size_t end_comment_len = strlen(END_BLOCK_COMMENT);
	size_t comment_len = strlen(COMMENT_FMT);

	// if the actual character belongs to a block comment
	int in_block_comment = 0;

	size_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	if (editor->sel.active) {
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);
	}

	for (size_t y = 0; y < screen_rows; y++) {
		size_t file_row = y + editor->view.row_offset;

		if (file_row >= editor->file.lines.size) {
			break;
		}

		const Line* line = vector_get(&editor->file.lines, file_row);

		if (editor->render_line_enumeration) {
			index_line(file_row, editor->file.lines.size);
		}

		// prints a ' ' in a selected empty line
		if (line->len == 0 && 
			editor->sel.active && 
			check_line_in_selection(y, editor->sel.start_y,
									   editor->sel.end_y))
		{
			invert_color();
			write(STDOUT_FILENO, " ", 1);
			reset_color();
		}

		size_t start = editor->view.col_offset;
		size_t end = start + screen_cols;

		if (start > line->len) {
			start = line->len;
		}

		if (end > line->len) {
			end = line->len;
		}

		size_t indent = line_get_indent(line);

		// just useful when the entire line is a comment
		int is_comment = line_is_comment(line, indent);

		// if the actual char belongs to a comment and the
		// line is not a comment
		int in_comment = 0;

		// if the actual word starts with digit
		int start_with_digit = 0;

		// if the word must be treated as a function
		int is_function = 0;

		// if the word must be treated as a macro
		int is_macro = 0;

		// if the actual character belongs to a keyword
		int in_keyword = 0;

		// position in COLORS array
		int keyword_index = -1;

		int in_selection = 0;

		if (in_block_comment || is_comment) {
			write_color(COMMENT_COLOR);
		}

		/*
		COLORING PRIORITIES:

		deciding the color of current char is to find
		in which case it is given the following order:

		1 - in a selection
		2 - in a line that is a comment
		3 - in a comment inside a line
		4 - in a block comment
		5 - in a keyword

		*/

		/*inefficient, since it iterates over all the chars 
		on the planet (FUTURE: find a alternative)*/

		for (size_t x = start; x < end; x++) {
			// log_write(&editor->log, "%ld %ld", y, in_block_comment);

			// --- SELECTION ---

			// if a char is selected, so any other case
			// must be ignored in rendering

			int selected = is_selected(&editor->sel, x, 
				file_row, x1, y1, x2, y2);

			// beggining of the selection
			if (selected && !in_selection) {
				invert_color();
				in_selection = 1;
			}

			// reseting and inverting the color is more important
			// than any other case
			if (selected && 
			   (in_keyword ||
			    is_comment ||
			    in_comment)) 
			{
			// (FUTURE: find out how to do the same thing
			// when in_block_comment is set)
				reset_color();
				invert_color();
			}

			// end of the selection
			if (!selected && in_selection) {
				reset_color();
				in_selection = 0;
			}

			/*
			ignores the rest, except in these cases:

			case (in_keyword && !is_word_char(current_char)):

			imagine the following code:

				int variable;

			if the selection starts in the middle of the 
			'variable' and the user moves it to the left
			in order to select 'int' or the ' ' just after it, 
			the letters after the start of the selection will 
			be the same color as 'int' is rendered

			the reason is simple: given that the ' ' just after 
			'int' is selected and its existence is precisely
			the condition of reseting the in_keyword, the program
			when seeing that this ' ' is selected would jump directly
			to render.
			this means that when the current char leaves 'int',
			in_keyword would not be reset, remaining equal to 1.
			in this case, when arriving at the letters that
			are after the start of the selection, these letters
			would be painted the same color that 'int' is painted
			(after all, keyword_index would not be reset either)
			
			case (!in_keyword && is_word_char(current_char)):

			if this case were ignored and the current char
			was a beginning of a keyword, the other letters
			that make up that keyword would not be painted
			the way the should if they were out of the selection
			so, in this case, we need to know if current_char is
			the beggining of a keyword to change in_keyword if
			necessary

			case (!in_keyword && !is_word_char(current_char)):

			the current_char could not be a word char, but if
			this case were ignored and the current_char was
			a special char that defines a different kind
			of keyword (macro, string literal etc), the same
			in the last case would happen to the other letters
			that form the word


			if none of the above cases are true, then we can
			finally 'ignore the rest' and jump to render
			*/
			if (selected) {
				if (in_keyword && !is_word_char(line->text[x])) {
					in_keyword = 0;
					keyword_index = -1;
				}

				if (!in_keyword) {
					if (is_word_char(line->text[x])) {
						goto keyword;
					} else {
						goto special_no_word_char;
					}
				}

				goto render;
			}

			if (is_comment) {
				goto render;
			}

			// --- LINE IS A COMMENT ---

			// that is decide outside this loop


			// --- LINE HAS A COMMENT ---

			if (!in_comment &&
				line->text[x] == COMMENT_FMT[0] &&
				end >= comment_len &&
				x <= end - comment_len)
			{
				size_t comment = check_has_a_comment(
					x,
					line
				);

				if (comment) {
					write_color(COMMENT_COLOR);
					in_comment = 1;
				}
			}

			if (in_comment) {
				goto render;
			}

			// --- BLOCK COMMENTS ---

			// after selection, block comments have priority
			// which means that if the actual char is in
			// a block comment, so any other case of coloring
			// (except selection) must be ignored


			// if the actual char is equal to the
			// first char of the start block comment format
			// and whether or not is possible
			// to that char belongs to a start block comment fmt
			if (!in_block_comment &&
				line->text[x] == START_BLOCK_COMMENT[0] && 
				end >= start_comment_len &&
				x <= end - start_comment_len) 
			{
				int is_block = check_forms_a_comment_fmt(
					x,
					line,
					SYN_START
				);

				if (is_block) {
					write_color(COMMENT_COLOR);
					in_block_comment = 1;

					goto render;
				}
			}

			// if the actual char is equal to the
			// last char of the end block comment format
			// and whether or not is possible
			// to that char belongs to a end block comment fmt

			// of course, that verification must be done
			// if and only if in_block_comment is set
			if (in_block_comment &&
				line->text[x] == END_BLOCK_COMMENT[end_comment_len - 1] && 
				x >= end_comment_len - 1) 
			{
				size_t end_block = check_forms_a_comment_fmt(
					x,
					line,
					SYN_END
				);

				if (end_block) {
					in_block_comment = 0;
					write(STDOUT_FILENO, &line->text[x], 1);
					reset_color();

					continue;
				}
			}

			if (in_block_comment) {
				goto render;
			}

			// --- KEYWORD ---


			// where most of the in_keyword = 1 happen
			if (!in_keyword &&
				is_word_char(line->text[x]))
			{

			keyword:
				// the keyword is inside a word
				// ex: aint, intvoid
				if (x > 0 && is_word_char(line->text[x - 1])) {
					goto render;
				}

				// if the word starts with a number,
				// then the entire word is automatically
				// considered as a number
				if (is_digit(line->text[x])) {
					keyword_index = KEYWORD_COUNT;
					start_with_digit = 1;
					in_keyword = 1;

					goto render;
				}

				size_t len = get_size_of_word(x, line);

				if (end - (len + x) >= 2) {
					// there is no check to know if
					// there is a ')' pair closing
					// the parentheses
					if (line->text[(len + x)] == '(') {
						is_function = 1;

						keyword_index = KEYWORD_COUNT + 1;
						in_keyword = 1;

						goto render;
					}
				}

				if (check_is_keyword(&line->text[x], len, &keyword_index)) {
					in_keyword = 1;
				}

				goto render;
			}

			// where most of the in_keyword = 0 happen.
			// contains the special_no_word_char label
			// that verifies if the char is a special no
			// word char
			if (!is_word_char(line->text[x])) 
			{
				if (in_keyword) {
					// if '.' is inside a word considered
					// as number, the it must not break
					// the color rendering of the number
					if (start_with_digit && line->text[x] == '.') {
						goto render;
					}

					// if it finds one '(' and is_function
					// is set, the we automatically stop
					// coloring using the FUNCTION_COLOR
					if (is_function && 
					   (line->text[x] == '(')) 
					{
						reset_color();
						in_keyword = 0;
						keyword_index = -1;

						goto render;
					}

					if (start_with_digit) {
						start_with_digit = 0;
					}

					if (is_macro) {
						is_macro = 0;
					}

					reset_color();
					in_keyword = 0;
					keyword_index = -1;
				} else {
					// a special no word char is a char
					// that is not a word char but
					// defines a different kind of keyword
					// (like macros or string literal)
				special_no_word_char:
					if (line->text[x] == MACRO_FMT) {
						if (x != indent) {
							goto render;
						}
						
						size_t i = x + 1;

						while (i < line->len &&
							is_word_char(line->text[i])) 
						{
							i++;
						}

						size_t len = i - x;		

						if (check_is_macro(&line->text[x], 
							len)) 
						{
							keyword_index = KEYWORD_COUNT + 2;
							is_macro = 1;
							in_keyword = 1;
						}
					}					
				}
			}

			// --- RENDERING ---
		render:
			if (!selected && 
				!in_block_comment && 
				!is_comment && 
				in_keyword && 
				keyword_index >= 0) 
			{
				write_color(COLORS[keyword_index]);
			}

			// a variable name should not start with a digit,
			// then the editor indicates it to the user
			// inverting the color
			if (start_with_digit && !is_digit(line->text[x])) {
				if (line->text[x] != '.') {
					invert_color();
				}
			}

			write(STDOUT_FILENO, &line->text[x], 1);

			// if the case above is true, reset the color
			if (start_with_digit && !is_digit(line->text[x])) {
				if (line->text[x] != '.') {
					reset_color();
				}
			}
		}

		write(STDOUT_FILENO, "\n", 1);

		if (in_selection || is_comment) {
			reset_color();
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
		log_write(&editor->log, 
			"editor_handle_open_delimiters: SUCCESS");
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
		log_write(&editor->log, 
			"editor_handle_close_delimiters: SUCCESS");
	}

	return 0;
}

static void editor_move_cursor_to_beginning_of_line(Editor* editor) {
	editor->cursor.x = 0;

	if (editor->sel.active) {
		editor->sel.end_x = 0;
	}

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

	if (editor->sel.active) {
		editor->sel.end_x = indent;
	}

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

	if (editor->sel.active) {
		editor->sel.end_x = line_size;
	}

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

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_start_and_create_selection: SUCCESS");
		}
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

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_up: SUCCESS");
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

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_down: SUCCESS");
	}	
}

static void editor_move_cursor_right(Editor* editor, size_t line_size) {
	cursor_move_right(&editor->cursor, line_size);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_right: SUCCESS");
	}   
}

static void editor_move_cursor_left(Editor* editor) {
	cursor_move_left(&editor->cursor);

	if (editor->selecting) {
		selection_update(&editor->sel, &editor->cursor);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_left: SUCCESS");
	}    
}

static void editor_scroll_up(Editor* editor) {
	if (editor->view.row_offset > 0) {
		editor->view.row_offset--;

		if (editor->cursor.y > editor->view.row_offset + editor->tsize.rows - 1) {
			editor->cursor.y--;
			editor->cursor.x = 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_scroll_up: SUCCESS");
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

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_down: SUCCESS");
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

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_up_terminal_size: SUCCESS");
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

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_down_terminal_size: SUCCESS");
		}
	}
}

static int editor_move_line_or_selection_up(Editor* editor) {
	int ret;

	if (!editor->sel.active) {
		ret = file_move_line_up(&editor->file, &editor->cursor.y, 
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_move_line_or_selection_up: SUCCESS");
		}

		return 0;
	} else {
		size_t x1, y1, x2, y2;
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);

		if (y1 == 0) {
			if (editor->debug_mode) {
				log_write(&editor->log, 
					"editor_move_line_or_selection_up: y1 == 0 (NOT A ERROR)");
			}

			return 0;
		}

		editor->cursor.y = y1;

		for (size_t i = 0; i < (y2 - y1 + 1); i++) {
			ret = file_move_line_up(&editor->file, &editor->cursor.y,
				&editor->result);

			if (ret < 0) {
				return EIE_FATAL_ERROR;
			}

			editor->cursor.y = y1 + (i + 1);
		}

		if (y1 > 0) {
			y1 -= 1;
			y2 -= 1;
		}

		if (editor->sel.start_y < editor->sel.end_y ||
		   (editor->sel.start_y == editor->sel.end_y &&
			editor->sel.start_x <= editor->sel.end_x)) {

			editor->cursor.y = y2;

			editor->sel.start_x = x1;
			editor->sel.start_y = y1;
			editor->sel.end_x = x2;
			editor->sel.end_y = y2;
		} else {
			editor->cursor.y = y1;

			editor->sel.start_x = x2;
			editor->sel.start_y = y2;
			editor->sel.end_x = x1;
			editor->sel.end_y = y1;
		}		

		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_move_line_or_selection_up: SUCCESS");
		}

		return 0;
	}
}

static int editor_move_line_or_selection_down(Editor* editor) {
	int ret;

	if (!editor->sel.active) {
		ret = file_move_line_down(&editor->file, &editor->cursor.y, 
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_move_line_or_selection_down: SUCCESS");
		}

		return 0;
	} else {
		size_t x1, y1, x2, y2;
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);

		if (y2 >= editor->file.lines.size - 1) {
			if (editor->debug_mode) {
				log_write(&editor->log,
			"editor_move_line_or_selection_down: y2 >= lines.size - 1 (NOT A ERROR)");
			}

			return 0;
		}

		editor->cursor.y = y2;

		for (size_t i = 0; i < (y2 - y1 + 1); i++) {
			ret = file_move_line_down(&editor->file, &editor->cursor.y,
				&editor->result);

			if (ret < 0) {
				return EIE_FATAL_ERROR;
			}

			editor->cursor.y = y2 - (i + 1);
		}

		if (y2 < editor->file.lines.size -1) {
			y1 += 1;
			y2 += 1;
		}

		if (editor->sel.start_y < editor->sel.end_y ||
		   (editor->sel.start_y == editor->sel.end_y &&
			editor->sel.start_x <= editor->sel.end_x)) {

			editor->cursor.y = y2;

			editor->sel.start_x = x1;
			editor->sel.start_y = y1;
			editor->sel.end_x = x2;
			editor->sel.end_y = y2;
		} else {
			editor->cursor.y = y1;

			editor->sel.start_x = x2;
			editor->sel.start_y = y2;
			editor->sel.end_x = x1;
			editor->sel.end_y = y1;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_move_line_or_selection_down: SUCCESS");
		}

		return 0;
	}
}

static int editor_comment_line_or_selection(Editor* editor) {
	int ret;

	if (!editor->sel.active) {
		ret = file_comment_line(&editor->file, &editor->cursor,
			1, -1, &editor->result);

		if (ret < 0) {
			return ret;
		}

		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_comment_line_or_selection: SUCCESS");
		}

		return 0;
	} else {
		size_t x1, y1, x2, y2;
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);
		size_t start_cursor_x = editor->cursor.x;
		size_t start_cursor_y = editor->cursor.y;

		int all_lines_are_comments = 1;

		for (size_t i = y1; i <= y2; i++) {
			const Line* line = vector_get(&editor->file.lines, i);

			if (!line) {
				result_set_reason(&editor->result,
					"editor_comment_line_or_selection: line is a null pointer");
				editor->result.type = ERROR_NULL_POINTER;

				return EIE_FATAL_ERROR;
			}

			size_t indent = line_get_indent(line);

			if (!line_is_comment(line, indent)) {
				all_lines_are_comments = 0;
				break;
			}
		}

		editor->cursor.y = y1;

		for (size_t i = 0; i <= (y2 - y1); i++) {
			ret = file_comment_line(&editor->file, &editor->cursor,
				editor->cursor.y == start_cursor_y,
				all_lines_are_comments,
				&editor->result);

			if (ret < 0) {
				return EIE_FATAL_ERROR;
			}

			editor->cursor.y++;
		}

		int cursor_increment = (int) editor->cursor.x -
			(int) start_cursor_x;

		if (cursor_increment > 0) {
			cursor_increment = strlen(COMMENT_FMT) + 1;
		}

		x1 += cursor_increment;
		x2 += cursor_increment;

		if (editor->sel.start_y < editor->sel.end_y ||
		   (editor->sel.start_y == editor->sel.end_y &&
			editor->sel.start_x <= editor->sel.end_x)) {

			editor->cursor.y = y2;

			editor->sel.start_x = x1;
			editor->sel.end_x = editor->cursor.x;
		} else {
			editor->cursor.y = y1;

			editor->sel.start_x = x2;
			editor->sel.end_x = x1;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_move_line_or_selection_down: SUCCESS");
		}

		return 0;
	}
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

static int editor_indent_selection_or_line(Editor* editor) {
	int ret;

	if (!editor->sel.active) {
		ret = file_indent_a_line(&editor->file, &editor->cursor,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_indent_a_line: SUCCESS");
		}

		return 0;
	} else {
		size_t x1, x2, y1, y2;
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);

		editor->cursor.y = y1;

		for (size_t i = 0; i <= (y2 - y1); i++) {
			ret = file_indent_a_line(&editor->file, &editor->cursor,
				&editor->result);

			if (ret < 0) {
				editor->cursor.y = y1;
				editor->cursor.x = x1;
				return EIE_FATAL_ERROR;
			}	

			editor->cursor.y++;
			editor->cursor.x = x1;
		}

		x1 += TAB_SIZE;
		x2 += TAB_SIZE;

		if (editor->sel.start_y < editor->sel.end_y ||
		   (editor->sel.start_y == editor->sel.end_y &&
			editor->sel.start_x <= editor->sel.end_x)) {

			editor->cursor.y = y2;
			editor->cursor.x = x2;

			editor->sel.start_x = (editor->sel.start_x == 0) ?
				0 : x1;
			editor->sel.start_y = y1;
			editor->sel.end_x = x2;
			editor->sel.end_y = y2;
		} else {
			editor->cursor.y = y1;
			editor->cursor.x = x1;

			editor->sel.start_x = x2;
			editor->sel.start_y = y2;
			editor->sel.end_x = (editor->sel.end_x == 0) ?
				0: x1;
			editor->sel.end_y = y1;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_indent_a_line: SUCCESS");
		}

		return 0;
	}
}

static int editor_unindent_selection_or_line(Editor* editor) {
	int ret;

	if (!editor->sel.active) {
		ret = file_unindent_a_line(&editor->file, &editor->cursor,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_unindent_selection_or_line: SUCCESS");
		}

		return 0;
	} else {
		size_t x1, x2, y1, y2;
		selection_normalize(&editor->sel, &x1, &y1, &x2, &y2);

		editor->cursor.y = y1;
		editor->cursor.x = 0;

		const Line* line = vector_get(&editor->file.lines,
			y1);

		if (!line) {
			result_set_reason(&editor->result,
			"editor_unindent_selection_or_line: line is a null pointer");
			editor->result.type = ERROR_NULL_POINTER;

			return EIE_NOT_FATAL_ERROR;
		}

		size_t indent = line_get_indent(line);

		line = NULL;

		for (size_t i = 0; i < (y2 - y1 + 1); i++) {
			ret = file_unindent_a_line(&editor->file, &editor->cursor,
				&editor->result);

			if (ret < 0) {
				editor->cursor.x = x1;
				editor->cursor.y = y1;

				return EIE_FATAL_ERROR;
			}

			editor->cursor.y++;
		}

		editor->cursor.y--;

		size_t move_cursor = (indent >= TAB_SIZE) ?
			(TAB_SIZE) : (indent);

		if (x1 >= indent) {
			x1 -= move_cursor;
			x2 -= move_cursor;
		}

		if (editor->sel.start_y < editor->sel.end_y ||
		   (editor->sel.start_y == editor->sel.end_y &&
			editor->sel.start_x <= editor->sel.end_x)) {

			editor->cursor.y = y2;
			editor->cursor.x = x2;

			editor->sel.start_x = (editor->sel.start_x == 0) ?
				0 : x1;
			editor->sel.start_y = y1;
			editor->sel.end_x = x2;
			editor->sel.end_y = y2;
		} else {
			editor->cursor.y = y1;
			editor->cursor.x = x1;

			editor->sel.start_x = x2;
			editor->sel.start_y = y2;
			editor->sel.end_x = (editor->sel.end_x == 0) ?
				0: x1;
			editor->sel.end_y = y1;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_indent_a_line: SUCCESS");
		}

		return 0;
	}
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

		case CTRL_KEY('p'):
			ret = editor_indent_selection_or_line(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY('o'):
			ret = editor_unindent_selection_or_line(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case CTRL_KEY('k'):
			ret = editor_comment_line_or_selection(editor);

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
			ret = editor_move_line_or_selection_up(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		case KEY_CTRL_SHIFT_ARROW_DOWN:
			ret = editor_move_line_or_selection_down(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			break;

		default:
			if (key >= 32 && key <= 126) {
				if (editor->sel.active) {
					editor->selecting = 0;
					selection_clear(&editor->sel);
				}

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