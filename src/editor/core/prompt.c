#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "util/types/u32string.h"
#include "util/files.h"
#include "util/types/cursor.h"
#include "editor/core/prompt.h"

const char PROMPT_SAVED[] = "saved";
const char PROMPT_REPLACE[] = "to replace: ";
const char PROMPT_INSERT[] = "to insert: ";
const char PROMPT_EMPTY_STRING[] = "operation denied: empty string";
const char PROMPT_NOT_FOUND[] = "operation denied: pattern not found";
const char PROMPT_PERM_DENIED[] = "operation denied: permission denied";
const char PROMPT_FIND[] = "find: ";
const char PROMPT_GOTO[] = "goto: ";
const char PROMPT_CMD[] = "> ";
const char PROMPT_READONLY[] = "operation denied: read only";
const char PROMPT_INVALID_ARG[] = "operation denied: invalid argument";
const char PROMPT_NO_ARGS[] = "operation denied: no arguments";
const char PROMPT_FILE_EXISTS[] = "operation denied: file already exists";
const char PROMPT_FILE_DNT_EXIST[] = "operation denied: filename doesn't exist";
const char PROMPT_INVALID_COMMAND[] = "operation denied: invalid command";
const char PROMPT_OUT_OF_BOUNDS[] = "operation denied: index out of bounds";
const char PROMPT_INSUFFICIENT_ARGS[] = "operation denied: insufficient arguments";
const char PROMPT_LANG_PLUGIN_NULL[] = "operation denied: lang plugin is null";
const char PROMPT_LANG_COMMENT_NULL[] = "operaiton denied: lang comment format is null";
const char PROMPT_LANG_RULES_NULL[] = "operation denied: lang rules is null";
const char PROMPT_LANG_PAIRS_NULL[] = "operation denied: lang pairs is null";
const char PROMPT_INVALID_FILENAME[] = "operation denied: file not found";
const char PROMPT_MULTILINE_ON_MATCH[] = "operation denied: the selection must not be multiline";
const char PROMPT_EMPTY_ON_MATCH[] = "operation denied: empty selection";
const char PROMPT_ARG_TOO_LONG[] = "operation denied: argument is too long";
const char PROMPT_INVALID_LANGUAGE[] = "operation denied: there is no language plugin associated";

Prompt prompt_new(TerminalSize tsize) {
	Prompt pt;

	pt.active = 0;
	pt.type = PT_INFO;
	pt.label = u32string_new();
	pt.buf = u32string_new();

	pt.view.col_offset = 0;
	pt.view.row_offset = 0;

	pt.tsize = tsize;

	pt.cursor.pos = POS_ZERO;

	return pt;
}

void prompt_init(Prompt* pt, const char* label, PromptType type) {
	// pt must be created by prompt_new()
	u32string_free(&pt->label);
	u32string_free(&pt->buf);
	// ------

	pt->active = 1;

	pt->label = u32string_from(label);
	pt->buf = u32string_new();

	pt->type = type;

	pt->invert_color = 0;

	pt->cursor.pos = POS_ZERO;

	pt->view.col_offset = 0;
	pt->view.row_offset = 0;
}

void prompt_init_u32string(Prompt* pt, const u32string* lb, PromptType type) {
	// pt must be created by prompt_new()
	u32string_free(&pt->label);
	u32string_free(&pt->buf);
	// ------

	pt->active = 1;

	pt->label = u32string_clone(lb);
	pt->buf = u32string_new();

	pt->type = type;

	pt->invert_color = 0;	
}

static size_t prompt_cursor_screen_x(const Prompt* pt) {
	size_t screen_x = 0;

	screen_x += file_to_screen_x(
		&pt->label,
		u32string_size(&pt->label),
		0
	);

	screen_x += file_to_screen_x(
		&pt->buf,
		pt->cursor.pos.x,
		0
	);

	return screen_x;
}

void prompt_update_size(Prompt* pt, TerminalSize tsize) {
	pt->tsize = tsize;

	update_scroll(
		&pt->view,
		pt->cursor.pos,
		prompt_cursor_screen_x(pt),
		&pt->tsize
	);
}

static void move_terminal_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\033[%zu;%zuH", y + 1, x + 1);
	write(STDOUT_FILENO, buf, len);
}

static void prompt_move_screen_cursor(const Prompt* pt) {
	size_t cursor_screen_x = prompt_cursor_screen_x(pt);

	Position screen_cursor = (Position) {
		cursor_screen_x - pt->view.col_offset,
		pt->tsize.rows + 1		
	};

	move_terminal_cursor(screen_cursor.x, screen_cursor.y);
}

void prompt_drawn
(
	Prompt* pt,
	const struct style* style,
	const TerminalSize* tsize
)
{
	if (!pt->active) {
		return;
	}

	char tmp[50];
	snprintf(tmp, sizeof(tmp), "\x1b[%ld;1H", tsize->rows + 1);

	write(STDOUT_FILENO, tmp, strlen(tmp));
	write(STDOUT_FILENO, "\x1b[K", 3);

	write_color(style);

	if (pt->invert_color) {
		invert_color();
	}

	size_t label_size = u32string_size(&pt->label);
	size_t buf_size = u32string_size(&pt->buf);

	size_t sizes_sum = 0;

	sizes_sum += file_to_screen_x(
		&pt->label,
		label_size,
		0
	);

	sizes_sum += file_to_screen_x(
		&pt->buf,
		buf_size,
		0
	);

	ssize_t remaining = tsize->cols;
	size_t screen_x = 0;
	size_t MAX = tsize->cols;

	if (sizes_sum <= MAX) {
		u32string_print(&pt->label);
		u32string_print(&pt->buf);

		remaining -= sizes_sum;

		screen_x = sizes_sum;
	}

	else {
		size_t start = pt->view.col_offset;

		size_t size = (label_size > (size_t) remaining + start)
			? (size_t) remaining
			: (label_size > start)
				? label_size - start
				: 0;

		u32string_print_range(&pt->label, start, size);

		remaining -= size;

		screen_x += file_to_screen_x(
			&pt->label,
			size,
			0
		);

		if (remaining <= 0) {
			goto end;
		}

		size_t start_buf = (start > label_size)
			? start - label_size
			: 0;

		size = (buf_size > (size_t) remaining + start_buf)
			? (size_t) remaining 
			: (buf_size > start_buf)
				? buf_size - start_buf
				: 0;

		u32string_print_range(&pt->buf, start_buf, size);

		remaining -= size;

		screen_x += file_to_screen_x(
			&pt->buf,
			size,
			0
		);
	}

	if (remaining <= 0) {
		remaining = 0;
	}

	for (size_t i = 0; i < (size_t) remaining + 1; i++) {
		u32_print(U' ');
	}

end:
	if (pt->type == PT_INTERACTIVE) {
		prompt_move_screen_cursor(pt);
	}

	reset_color();
}

int prompt_has_label(const Prompt* pt, const char* label) {
	return u32string_equalu8(&pt->label, label);
}

int prompt_buf_to_u32string(const Prompt* pt, u32string* string) {
	if (!pt || u32string_is_empty(&pt->buf) || !string) {
		return -1;
	}

	char* buf = u32string_into_u8(&pt->buf);

	if (buf[0] != U'"') {
		char* space = strchr(buf, ' ');

		if (space) {
			char tmp[512];
			size_t size = space - buf;
			strncpy(tmp, buf, size);

			tmp[size] = '\0';

			*string = u32string_from(tmp);
		}

		else {
			*string = u32string_from(buf);
		}

		free(buf);
		return 0;
	}

	else {
		char tmp[512];
		int valid = parse_quoted(buf, tmp, sizeof(tmp));

		if (!valid) {
			free(buf);
			return -1;
		}

		if (strlen(tmp) == 0) {
			free(buf);
			return -2;					
		}

		*string = u32string_from(tmp);

		free(buf);
		return 0;
	}	
}

int prompt_buf_to_u8string(const Prompt* pt, char** string) {
	if (!pt || u32string_is_empty(&pt->buf) || !string) {
		return -1;
	}

	char* buf = u32string_into_u8(&pt->buf);

	if (buf[0] != '"') {
		char* space = strchr(buf, ' ');

		if (space) {
			size_t size = space - buf;
			*string = malloc(size + 1);

			if (!(*string)) {
				free(buf);
				return -1;
			}

			memcpy(
				*string,
				buf,
				size * sizeof(char)
			);

			(*string)[size] = '\0';
		}

		else {
			size_t size = u32string_size(&pt->buf);
			*string = malloc(size + 1);

			if (!(*string)) {
				free(buf);
				return -1;
			}

			memcpy(
				*string,
				buf,
				size * sizeof(char)
			);

			(*string)[size] = '\0';
		}

		free(buf);
		return 0;
	}

	else {
		char tmp[512];
		int valid = parse_quoted(buf, tmp, sizeof(tmp));

		if (!valid) {
			free(buf);
			return -1;
		}

		if (strlen(tmp) == 0) {
			free(buf);
			return -2;
		}

		*string = strdup(tmp);

		free(buf);
		return 0;
	}
}

static void prompt_cursor_update(Prompt* pt) {
	size_t text_cols = pt->tsize.cols;

	while (prompt_cursor_screen_x(pt) >= 
		text_cols + pt->view.col_offset)
	{
		pt->view.col_offset += text_cols - 4;
	}

	while (prompt_cursor_screen_x(pt)  < pt->view.col_offset) {
		if (pt->view.col_offset >= text_cols) {
			pt->view.col_offset -= text_cols
				- 4;
		} 

		else {
			pt->view.col_offset = 0;
		}
	}
}

static void prompt_cursor_move(Prompt* pt, Position pos) {
	size_t buf_size = u32string_size(&pt->buf);

	if (pos.x > buf_size) {
		return;
	}

	pt->cursor.pos = pos;
	prompt_cursor_update(pt);
}

static void prompt_move_cursor_right(Prompt* pt) 
{
	if (pt->cursor.pos.x > u32string_size(&pt->buf)) 
	{
		return;
	}

	prompt_cursor_move(
		pt, 
		(Position) { pt->cursor.pos.x + 1, pt->cursor.pos.y });
}

static void prompt_move_cursor_left(Prompt* pt) {
	if (pt->cursor.pos.x == 0) {
		return;
	}

	prompt_cursor_move(
		pt,
		(Position) { pt->cursor.pos.x - 1, pt->cursor.pos.y });    
}

static void prompt_move_beginning(Prompt* pt) {
	pt->cursor.pos.x = 0;
	prompt_cursor_update(pt);
}

static void prompt_move_end(Prompt* pt) {
	pt->cursor.pos.x = u32string_size(&pt->buf);
	prompt_cursor_update(pt);
}

static void prompt_insert_char(Prompt* pt, uint32_t c) {
	u32string_insert(&pt->buf, c, pt->cursor.pos.x);

	prompt_move_cursor_right(pt);
}

static void prompt_delete_char(Prompt* pt) {
	if (pt->cursor.pos.x == 0) {
		return;
	}

	u32string_remove(&pt->buf, pt->cursor.pos.x - 1, NULL);

	prompt_move_cursor_left(pt);
}

static void prompt_del_from_cursor_left(Prompt* pt) {
	if (pt->cursor.pos.x == 0) {
		return;
	}

	u32string_remove_range(
		&pt->buf,
		0,
		pt->cursor.pos.x
	);

	pt->cursor.pos.x = 0;
	prompt_cursor_update(pt);
}

static void prompt_del_from_cursor_right(Prompt* pt) {
	size_t size = u32string_size(&pt->buf);

	if (pt->cursor.pos.x == size) {
		return;
	}

	u32string_remove_range(
		&pt->buf,
		pt->cursor.pos.x,
		size
	);

	pt->cursor.pos.x = size;
	prompt_cursor_update(pt);
}

void prompt_handle_normal_input
(
	Prompt* pt, 
	struct normal_key key,
	const struct normal_key* keybinds
) 
{
	uint32_t content = key.content;
	uint32_t modifiers = key.modifiers;

	if (content == ARROW_RIGHT && modifiers == 0) {
		prompt_move_cursor_right(pt);
	}

	else if (content == ARROW_LEFT && modifiers == 0) {
		prompt_move_cursor_left(pt);
	}

	else if (content == 127) {
		prompt_delete_char(pt);
	}

	else if (normal_key_equal(&key, &keybinds[ACTION_MOVE_START_LINE])) {
		prompt_move_beginning(pt);
	}

	else if (normal_key_equal(&key, &keybinds[ACTION_MOVE_END_LINE])) {
		prompt_move_end(pt);
	}

	else if (normal_key_equal(&key, &keybinds[ACTION_DEL_FROM_CURSOR_LEFT])) {
		prompt_del_from_cursor_left(pt);
	}

	else if (normal_key_equal(&key, &keybinds[ACTION_DEL_FROM_CURSOR_RIGHT])) {
		prompt_del_from_cursor_right(pt);
	}

	else if (u32_is_printable(content) && modifiers == 0) {
		prompt_insert_char(pt, content);
	}

	cursor_clamp(&pt->cursor, u32string_size(&pt->buf), 1);
}