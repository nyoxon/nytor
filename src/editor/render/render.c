#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <assert.h>

#include "editor/render/render.h"
#include "window/render.h"
#include "terminal/style.h"

#define RENDER_BUFFER_SIZE 256

static const struct style INVALID_CHAR = {
	{ .type = COLOR_ANSI, .ansi = BLUE },
	{ .type = COLOR_ANSI, .ansi = RED },
	0
};

static void move_terminal_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\033[%zu;%zuH", y + 1, x + 1);
	write(STDOUT_FILENO, buf, len);
}

static int is_selected(
	Selection* sel, 
	Position pos
)
{
	if (!sel->active) {
		return 0;
	}

	Position a = {0}, b = {0};
	selection_normalize(sel, &a, &b);

	if (pos.y < a.y || pos.y > b.y) {
		return 0;
	}

	if (a.y == b.y) {
		return pos.x >= a.x && pos.x < b.x;
	} 

	if (pos.y == a.y) {
		return pos.x >= a.x;
	}

	if (pos.y == b.y) {
		return pos.x < b.x;
	}

	return 1;
}

static void index_line
(
	size_t y, 
	size_t line_count,
	struct style* style
) 
{
	int digits = 1;
	size_t tmp = line_count;

	while (tmp >= 10) {
		tmp /= 10;
		digits++;
	}

	char buf[32];
	int len = snprintf(buf, sizeof(buf), "%*zu", digits, y + 1);

	write_color(style);

	write(STDOUT_FILENO, buf, len);
	reset_color();
}

static int is_whitespace(uint32_t c) {
	switch (c) {
	case U' ':
	case U'\t':
	case U'\r':
	case U'\v':
	case U'\f':
		return 1;
	default:
		return 0;
	}
}

static void apply_style
(
	Editor* editor,
	enum highlight hl,
	int selected
)
{
	reset_color();

	if (selected) {
		write_color(&editor->config.ui.selection);
		
		// if (editor->actual_file->lexer) {
		// } else {
		// 	invert_color();
		// }
		return;
	}

	struct style style = editor->config.theme[hl];
	write_color(&style);
}

static int check_line_in_selection(size_t y, size_t start_y, size_t end_y) {
	return (start_y <= y && y <= end_y) ||
		   (end_y <= y && y <= start_y);
}


static size_t interval_overlap_width
(
	size_t a_begin, size_t a_end,
	size_t b_begin, size_t b_end
)
{
	size_t begin = MAX(a_begin, b_begin);
	size_t end = MIN(a_end, b_end);

	return MAX(end - begin, 0);
}

static void write_hex_byte(uint8_t x) {
	static const char hex[] = "0123456789ABCDEF";

	char out[2];
	out[0] = hex[x >> 4];
	out[1] = hex[x & 0x0F];

	write(STDOUT_FILENO, out, 2);
}

static void render_utf
(
	Editor* editor,
	size_t screen_rows,
	size_t screen_cols
)
{
	size_t last_line;

	uint32_t u32buf[screen_cols];
	size_t codepoints = 0;

	for (size_t y = 0; y < screen_rows; y++) {
		size_t file_row = y + editor->view.row_offset;
		last_line = file_row;
		int empty_selected_line = 0;

		// log_write(&editor->log, "%zu, %zu", y + 1, screen_rows);

		if (file_row >= file_num_lines(&editor->actual_file->file)) {
			break;
		}

		if (y == editor->tsize.rows - 1) {
			last_line++;
		}

		const u32string* line = file_get_line_text(
			&editor->actual_file->file,
			file_row);

		size_t size = u32string_size(line);

		if (editor->config.line_numbers) {
			int background = (file_row == editor->cursor.pos.y);
			struct style style = (background)
				? editor->config.ui.line_number_current
				: editor->config.ui.line_number;


			reset_color();

			if (background && editor->actual_file->file.dirty) {
				style.modifiers |= REVERSE;
			}

			index_line(
				file_row, 
				file_num_lines(&editor->actual_file->file),
				&style);

			if (editor->config.background_fills_all) {
				write_color(&editor->config.ui.normal);
			}

			write(STDOUT_FILENO, " ", 1);

			if (editor->config.background_fills_all) {
				reset_color();
			}
		}


		// prints a ' ' in a selected empty line
		if (size == 0 &&
			editor->sel.active && 
			check_line_in_selection(
				file_row,
				editor->sel.start.y,
				editor->sel.end.y))
		{
			empty_selected_line = 1;
			write_color(&editor->config.ui.selection);
			write(STDOUT_FILENO, " ", 1);
			reset_color();
		}

		struct lexer* lexer = editor->actual_file->lexer;

		Vector* tokens = (lexer)
			? file_get_line_tokens(&editor->actual_file->file, file_row)
			: NULL;


		size_t current = 0;
		int current_selected = 0;
		enum highlight current_highlight = HL_NONE;

		size_t cursor_screen_x = 0;

		for (size_t x = 0; x < size; x++) {
			uint32_t c = u32string_char(line, x);

			size_t width = char_screen_width(
				c,
				cursor_screen_x,
				editor->actual_file->file.tab_size
			);

			if (cursor_screen_x + width <= editor->view.col_offset) {
				cursor_screen_x += width;
				continue;
			}

			if (cursor_screen_x >= editor->view.col_offset + screen_cols) {
				if (codepoints > 0) {
					u32_print(u32buf, codepoints);
					codepoints = 0;
				}

				break;
			}

			enum highlight hl = HL_NONE;

			// checks if x is selected
			int selected = is_selected(
				&editor->sel,
				(Position) { x, file_row });

			if (is_whitespace(c)) {
				hl = HL_WHITESPACE;
			}

			// checks if x is in a token
			else if (lexer && tokens && tokens->size > 0) {
				size_t ntokens = tokens->size;
				struct token* token = vector_get(tokens, current);

				while (current + 1 < ntokens && 
					x >= token->end)
				{
					current++;
					token = vector_get(tokens, current);
				}

				if (current < ntokens &&
					x >= token->begin &&
					x < token->end)
				{
					hl = token->highlight;
				}

				// log_write(
				// 	&editor->log, "%zu:%zu -> [%zu,%zu] %d",
				// 	y + 1, x,
				// 	token->begin,
				// 	token->end,
				// 	hl);
			}

			// apply_style is only called if
			// hl or selected changes (preservation)
			if (selected != current_selected ||
				hl != current_highlight ||
				!editor->actual_file->lexer) 
			{
				u32_print(u32buf, codepoints);
				codepoints = 0;

				apply_style(editor, hl, selected);

				current_selected = selected;
				current_highlight = hl;
			}

			// substitutes ' ' by '.'
			if (c == U' ') {
				u32buf[codepoints++] = 
					editor->config.show_tabs ? U'.' :  U' ';
			}

			// print on screen
			else if (c == U'\t') {
				size_t visible = interval_overlap_width(
					cursor_screen_x, cursor_screen_x + width,
					editor->view.col_offset,
					editor->view.col_offset + screen_cols
				);

				char to_print = (editor->config.show_tabs)
					? '.'
					: ' ';

				for (size_t i = 0; i < visible; i++) {
					u32buf[codepoints++] = to_print; 
				}
			}

			else {
				u32buf[codepoints++] = c;
				if (!u32_is_printable(c)) {
					u32_print(u32buf, codepoints);
					codepoints = 0;

					if (!selected) {
						write_color(&INVALID_CHAR);
					}
					
					write_hex_byte((uint8_t) c);

					apply_style(editor, current_highlight, 0);
				}

				else {

				}
			}

			if (x == size - 1 || codepoints == size) {
				u32_print(u32buf, codepoints);

				codepoints = 0;
			}

			cursor_screen_x += width;
		}

		if (editor->config.has_background) 
		{
			size_t screen_x = (cursor_screen_x > editor->view.col_offset)
				? cursor_screen_x - editor->view.col_offset
				: 0;

			if (screen_x < screen_cols) {
				write_color(&editor->config.ui.normal);

				size_t remaining = screen_cols - screen_x;

				if (editor->config.background_fills_all) {
					remaining++;
				}

				// a ' ' has already been printed
				if (empty_selected_line && remaining > 0) {
					remaining--;
				}

				char buf[remaining];
				memset(buf, ' ', remaining);

				write(STDOUT_FILENO, buf, remaining);
			}
		}

		// codepoints = 0;
		write(STDOUT_FILENO, "\n", 1);
	}

	if (editor->config.has_background) {
		size_t first_empty_row = (last_line > editor->view.row_offset)
			? last_line - editor->view.row_offset
			: last_line + 1;

		if (first_empty_row < screen_rows) {
			write_color(&editor->config.ui.normal);

			size_t gutter = editor->tsize.cols - screen_cols;

			size_t remaining = screen_cols;
			size_t remaining_lines = screen_rows - first_empty_row;

			if (editor->config.background_fills_all) {
				remaining += gutter + 1;
				remaining_lines++;
				gutter = 0;
			}

			char buf[remaining];
			memset(buf, ' ', remaining);

			for (size_t y = 0; y < remaining_lines; y++) {
				move_terminal_cursor(gutter, first_empty_row + y);

				write(STDOUT_FILENO, buf, remaining);
			}
		}
	}
}

void editor_move_screen_cursor(Editor* editor) {
	size_t cursor_screen_x = editor_cursor_screen_x(editor);

	size_t gutter_width = (editor->config.line_numbers)
		? get_gutter_width(file_num_lines(&editor->actual_file->file))
		: 0;

	Position screen_cursor;

	size_t x = cursor_screen_x + gutter_width - editor->view.col_offset;
	size_t y = editor->cursor.pos.y - editor->view.row_offset;

	screen_cursor = (Position) { x, y };

	move_terminal_cursor(screen_cursor.x, screen_cursor.y);
}

void editor_render(Editor* editor) {
	clean_terminal();
	write(STDOUT_FILENO, editor->config.ui.cursor_style, CURSOR_STYLE_SIZE);
	write(STDOUT_FILENO, "\033[H", 3);

	size_t gutter_width = (editor->config.line_numbers)
		? get_gutter_width(file_num_lines(&editor->actual_file->file))
		: 0;

	size_t screen_rows = editor->tsize.rows;
	size_t screen_cols = editor->tsize.cols - gutter_width;

	if (!editor->config.cursor_follow_scroll &&
		(editor->cursor.pos.y < editor->view.row_offset ||
		editor->cursor.pos.y >= screen_rows + editor->view.row_offset ||
		editor->cursor.pos.x < editor->view.col_offset ||
		editor->cursor.pos.x >= screen_cols + editor->view.col_offset))
	{
		hide_cursor();
	}

	// write(STDOUT_FILENO, "\x1b]122\x07", strlen("\x1b]122\x07"));

	render_utf(editor, screen_rows, screen_cols);
	reset_color();

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_render: SUCCESS");
	}

	prompt_drawn(
		&editor->status_bar, 
		&editor->config.ui.status_bar,
		&editor->tsize);

	if (editor->has_window) {
		window_draw(
			&editor->window, 
			&editor->config.ui.window_color);
	}

	else {
		if (!editor->status_bar.active ||
			editor->status_bar.type != PT_INTERACTIVE) 
		{
			editor_move_screen_cursor(editor);
		}
	}
}