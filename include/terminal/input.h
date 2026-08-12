#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stddef.h>

#include "terminal/mouse.h"

// to read and handle input from user
// responsible for activating or deactivating the raw mode on terminal

#define KEY_MOD_SHIFT 2
#define KEY_MOD_ALT 3
#define KEY_MOD_SHIFT_ALT 4
#define KEY_MOD_CTRL 5
#define KEY_MOD_CTRL_SHIFT 6
#define KEY_MOD_CTRL_ALT 7
#define KEY_MOD_CTRL_SHIFT_ALT 8


// 0x110000 > unicode upper bound
enum arrow {
	ARROW_UP = 0x110000,
	ARROW_DOWN,
	ARROW_RIGHT,
	ARROW_LEFT
};

enum event_type {
	EVENT_KEY,
	EVENT_MOUSE
};

struct normal_key {
	uint32_t content;
	uint32_t modifiers;
};

struct event {
	enum event_type type;

	union {
		struct normal_key key;
		struct mouse_event mouse;
	};
};

enum action {
	ACTION_SAVE,
	ACTION_QUIT,
	ACTION_FIND,
	ACTION_MATCH,
	ACTION_GOTO,
	ACTION_INDENT,
	ACTION_UNINDENT,
	ACTION_COMMENT,
	ACTION_MOVE_START_LINE,
	ACTION_MOVE_INDENT,
	ACTION_MOVE_END_LINE,
	ACTION_SHOW_TABS,
	ACTION_SELECTION,
	ACTION_SELECT_ALL,
	ACTION_SELECT_LINE,
	ACTION_COPY,
	ACTION_PASTE,
	ACTION_DEL_FROM_CURSOR_LEFT,
	ACTION_DEL_FROM_CURSOR_RIGHT,
	ACTION_SELECTION_MOVE_UP,
	ACTION_SELECTION_MOVE_DOWN,
	ACTION_SCROLL_UP,
	ACTION_SCROLL_DOWN,
	ACTION_SCROLL_RIGHT,
	ACTION_SCROLL_LEFT,
	ACTION_SCROLL_TERMINAL_UP,
	ACTION_SCROLL_TERMINAL_DOWN,
	ACTION_MOVE_WORD_RIGHT,
	ACTION_MOVE_WORD_LEFT,
	ACTION_MOVE_FULLWORD_RIGHT,
	ACTION_MOVE_FULLWORD_LEFT,
	ACTION_REPLACE,
	ACTION_SUSPEND,
	ACTION_UNDO,
	ACTION_REDO,
	ACTION_NEXT_FILE,
	ACTION_PREV_FILE,
	ACTION_NEW_FILE,
	ACTION_CLOSE_FILE,
	ACTION_OPEN_CMD,
	ACTION_TERMINAL,

	ACTION_COUNT
};

void hide_cursor();
void show_cursor();
void enable_raw_mode();
void disable_raw_mode();
void clean_terminal();

char* str_action(int index);
char* str_modifier(uint32_t modifiers);

int normal_key_equal
(
	const struct normal_key* a,
	const struct normal_key* b
);

void activate_terminal();
void deactivate_terminal();

#endif