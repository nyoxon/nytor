#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "terminal/input.h"
#include "terminal/style.h"

static struct termios orig_termios;
static volatile sig_atomic_t raw_mode_enabled = 0;

void hide_cursor() {
	write(STDOUT_FILENO, "\033[?25l", 6);	
}

void show_cursor() {
	write(STDOUT_FILENO, "\033[?25h", 6);
}

void clean_terminal() {
	show_cursor();
	write(STDOUT_FILENO, "\x1b]122\x07", strlen("\x1b]112\x07"));
	write(STDOUT_FILENO, "\033[3J", 4);
	write(STDOUT_FILENO, "\033[2J", 4);
	write(STDOUT_FILENO, "\033[H", 3);


	write(STDOUT_FILENO, CURSOR_STYLE_DEFAULT, CURSOR_STYLE_SIZE);
}

void disable_raw_mode() {
	if (!raw_mode_enabled) {
		return;
	}

	// best effort: tcsetattr() is not async-signal-safe,
	// but restoring the terminal is preferable during a crash.
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
	raw_mode_enabled = 0;
}

void enable_raw_mode() {
	if (raw_mode_enabled) {
		return;
	}

	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
		exit(1);
	}

	atexit(disable_raw_mode);

	struct termios raw = orig_termios;

	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_lflag &= ~(ISIG); // important
	raw.c_iflag &= ~(IXON);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	// best effort: tcsetattr() is not async-signal-safe,
	// but restoring the terminal is preferable during a crash.
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		exit(1);
	}

	raw_mode_enabled = 1;
}

void activate_terminal() {
	enable_raw_mode();
	mouse_on();
}

void deactivate_terminal() {
	clean_terminal();
	disable_raw_mode();
	mouse_off();	
}

char* str_action(int index) {
	switch (index) {
	case ACTION_SAVE:
		return strdup("save");

	case ACTION_QUIT:
		return strdup("quit");

	case ACTION_FIND:
		return strdup("find");

	case ACTION_MATCH:
		return strdup("match");

	case ACTION_GOTO:
		return strdup("goto");

	case ACTION_INDENT:
		return strdup("indent");

	case ACTION_UNINDENT:
		return strdup("unindent");

	case ACTION_COMMENT:
		return strdup("comment");

	case ACTION_MOVE_START_LINE:
		return strdup("move_start_line");

	case ACTION_MOVE_INDENT:
		return strdup("move_indent");

	case ACTION_MOVE_END_LINE:
		return strdup("move_end_line");

	case ACTION_SHOW_TABS:
		return strdup("show_tabs");

	case ACTION_SELECTION:
		return strdup("selection");

	case ACTION_SELECT_ALL:
		return strdup("select_all");

	case ACTION_SELECT_LINE:
		return strdup("select_line");

	case ACTION_COPY:
		return strdup("copy");

	case ACTION_PASTE:
		return strdup("paste");

	case ACTION_DEL_FROM_CURSOR_LEFT:
		return strdup("del_from_cursor_left");

	case ACTION_DEL_FROM_CURSOR_RIGHT:
		return strdup("del_from_cursor_RIGHT");

	case ACTION_SELECTION_MOVE_UP:
		return strdup("selection_move_up");

	case ACTION_SELECTION_MOVE_DOWN:
		return strdup("selection_move_down");

	case ACTION_SCROLL_UP:
		return strdup("scroll_up");

	case ACTION_SCROLL_DOWN:
		return strdup("scroll_down");

	case ACTION_SCROLL_TERMINAL_UP:
		return strdup("scroll_terminal_up");

	case ACTION_SCROLL_TERMINAL_DOWN:
		return strdup("scroll_terminal_down");

	case ACTION_MOVE_WORD_RIGHT:
		return strdup("move_word_right");

	case ACTION_MOVE_WORD_LEFT:
		return strdup("move_word_left");

	case ACTION_MOVE_FULLWORD_RIGHT:
		return strdup("move_fullword_right");

	case ACTION_MOVE_FULLWORD_LEFT:
		return strdup("move_fullword_left");

	case ACTION_SUSPEND:
		return strdup("suspend");

	case ACTION_SCROLL_RIGHT:
		return strdup("scroll_right");

	case ACTION_SCROLL_LEFT:
		return strdup("scroll_left");

	case ACTION_REPLACE:
		return strdup("replace");

	case ACTION_UNDO:
		return strdup("undo");

	case ACTION_REDO:
		return strdup("redo");

	case ACTION_NEXT_FILE:
		return strdup("next_file");

	case ACTION_PREV_FILE:
		return strdup("prev_file");

	case ACTION_NEW_FILE:
		return strdup("new_file");

	case ACTION_CLOSE_FILE:
		return strdup("close_file");

	case ACTION_OPEN_CMD:
		return strdup("open_cmd");

	case ACTION_TERMINAL:
		return strdup("terminal");

	default:
		return NULL;
	}
}

char* str_modifier(uint32_t modifiers) {
	switch (modifiers) {
	case KEY_MOD_CTRL:
		return strdup("ctrl");

	case KEY_MOD_ALT:
		return strdup("alt");

	case KEY_MOD_SHIFT:
		return strdup("shift");

	case KEY_MOD_CTRL_ALT:
		return strdup("ctrl + alt");

	case KEY_MOD_CTRL_SHIFT:
		return strdup("ctrl + shift");

	case KEY_MOD_SHIFT_ALT:
		return strdup("shift + alt");

	case KEY_MOD_CTRL_SHIFT_ALT:
		return strdup("ctrl + shift + alt");

	default:
		return NULL;
	}
}

int normal_key_equal
(
	const struct normal_key* a,
	const struct normal_key* b
)
{
	return a->content == b->content && a->modifiers == b->modifiers;
}