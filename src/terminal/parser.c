#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include "terminal/parser.h"

static const struct event KEY_NONE = {
	.type = EVENT_KEY,
	.key = {-1, 0}
};

static const struct event KEY_ESCAPE = {
	.type = EVENT_KEY,
	.key = {'\033', 0}
};

static const struct event KEY_ARROW_UP = {
	.type = EVENT_KEY,
	.key = {ARROW_UP, 0}
};

static const struct event KEY_ARROW_DOWN = {
	.type = EVENT_KEY,
	.key = {ARROW_DOWN, 0}
};

static const struct event KEY_ARROW_RIGHT = {
	.type = EVENT_KEY,
	.key = {ARROW_RIGHT, 0}
};

static const struct event KEY_ARROW_LEFT = {
	.type = EVENT_KEY,
	.key = {ARROW_LEFT, 0}
};


static void parse_ascii(unsigned char c, struct event* event) {
	if (c == '\b' || c == '\t' || c == '\n' || c == 127) {
		event->key.content = c;
	}

	else if (c >= 1 && c <= 26) {
		event->key.content = 'a' + c - 1;
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == '\0') {
		event->key.content = c;
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == 0) {
		event->key.content = '@';
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == 27) {
		event->key.content = '[';
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == 28) {
		event->key.content = '\\';
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == 29) {
		event->key.content = ']';
		event->key.modifiers = KEY_MOD_CTRL;
 	}

	else if (c == 30) {
		event->key.content = '^';
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else if (c == 31) {
		event->key.content = '_';
		event->key.modifiers = KEY_MOD_CTRL;
	}

	else {
		event->key.content = c;
	}	
}

static void parse_utf8(unsigned char b0, struct event* event) {
	unsigned char b1, b2, b3;

	if ((b0 & 0xE0) == 0xC0) {
		if (read(STDIN_FILENO, &b1, 1) != 1) {
			return;
		}

		event->key.content = ((b0 & 0x1F) << 6) |
						 (b1 & 0x3F);

		return;
	}

	if ((b0 & 0xF0) == 0xE0) {
		if (read(STDIN_FILENO, &b1, 1) != 1) {
			return;
		}

		if (read(STDIN_FILENO, &b2, 1) != 1) {
			return;
		}

		event->key.content = ((b0 & 0x0F) << 12) |
						((b1 & 0x3F) << 6) |
						 (b2 & 0x3F);

		return;		
	}

	if ((b0 & 0xF8) == 0xF0) {
		if (read(STDIN_FILENO, &b1, 1) != 1) {
			return;
		}

		if (read(STDIN_FILENO, &b2, 1) != 1) {
			return;
		}

		if (read(STDIN_FILENO, &b3, 1) != 1) {
			return;
		}

		event->key.content = ((b0 & 0x07) << 18) |
						((b1 & 0x3F) << 12) |
						((b2 & 0x3F) << 6) |
						 (b3 & 0x3F);

		return;			
	}
	
	event->key.content = 0xFFFD;
}


static struct event normal_key(unsigned char c) {
	struct event event = KEY_NONE;

	if ((c & 0x80) == 0) {
		parse_ascii(c, &event);
	} else {
		parse_utf8(c, &event);
	}

	return event;
}

static struct event parse_modifier_sequence() {
	struct event event = KEY_NONE;

	unsigned char c;
	int modifier = 0;

	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (c == ';') {
			continue;
		}

		if (c >= '0' && c <= '9') {
			modifier = modifier * 10 + c - '0';
			continue;
		}

		switch (c) {
		case 'A':
			event = KEY_ARROW_UP;
			break;

		case 'B':
			event = KEY_ARROW_DOWN;
			break;

		case 'C':
			event = KEY_ARROW_RIGHT;
			break;

		case 'D':
			event = KEY_ARROW_LEFT;
			break;

		default:
			return KEY_NONE;
		}

		break;
	}

	switch (modifier) {
	case KEY_MOD_SHIFT:
		event.key.modifiers |= KEY_MOD_SHIFT;
		break;

	case KEY_MOD_ALT:
		event.key.modifiers |= KEY_MOD_ALT;
		break;

	case KEY_MOD_SHIFT_ALT:
		event.key.modifiers |= KEY_MOD_SHIFT_ALT;
		break;

	case KEY_MOD_CTRL:
		event.key.modifiers |= KEY_MOD_CTRL;
		break;

	case KEY_MOD_CTRL_SHIFT:
		event.key.modifiers |= KEY_MOD_CTRL_SHIFT;
		break;

	case KEY_MOD_CTRL_ALT:
		event.key.modifiers |= KEY_MOD_CTRL_ALT;
		break;

	case KEY_MOD_CTRL_SHIFT_ALT:
		event.key.modifiers |= KEY_MOD_CTRL_SHIFT_ALT;
		break;
	}

	return event;
}


// FIXME (later)
static struct event parse_special_key() {
	return KEY_NONE;
}

static struct event parse_csi_number() {
	unsigned char c;

	if (read(STDIN_FILENO, &c, 1) != 1) {
		return KEY_NONE;
	}

	if (c == ';') {
		return parse_modifier_sequence();
	}

	if (c == '~') {
		return parse_special_key();
	}

	return KEY_NONE;
}

static struct event parse_mouse() {
	int button = 0;
	int x = 0;
	int y = 0;

	int pressed = 0;

	unsigned char c;

	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (c == ';') {
			break;
		}

		if (c >= '0' && c <= '9') {
			button = button * 10 + c - '0';
		}
	}

	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (c == ';') {
			break;
		}

		if (c >= '0' && c <= '9') {
			x = x * 10 + c - '0';
		}
	}

	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (c == 'M' || c == 'm') {
			if (c == 'M') {
				pressed = 1;
			}

			break;
		}

		if (c >= '0' && c <= '9') {
			y = y * 10 + c - '0';
		}
	}

	struct mouse_event mouse = {
		button,
		x,
		y,
		pressed
	};

	struct event event;

	event.type = EVENT_MOUSE;
	event.mouse = mouse;

	return event;
}

static struct event parse_escape_sequence() {
	unsigned char c;

	if (read(STDIN_FILENO, &c, 1) != 1) {
		return KEY_ESCAPE;
	}

	switch (c) {
	case '[':
		break;

	case 'O':
		// parse_ss3();
		break;

	default: {
			struct event e = {0};

			e.type = EVENT_KEY;
			e.key.content = c;
			e.key.modifiers = KEY_MOD_ALT;
			return e;
		}
	}

	if (read(STDIN_FILENO, &c, 1) != 1) {
		return KEY_ESCAPE;
	}


	switch (c) {
	case 'A':
		return KEY_ARROW_UP;

	case 'B':
		return KEY_ARROW_DOWN;

	case 'C':
		return KEY_ARROW_RIGHT;

	case 'D':
		return KEY_ARROW_LEFT;

	case '<':
		return parse_mouse();
	
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
		return parse_csi_number();


	default:
		return KEY_NONE;
	}
}

struct event parser_read_key(void) {
	unsigned char c;


	if (read(STDIN_FILENO, &c, 1) != 1) {
		return KEY_NONE;
	}

	if (c != '\033') {
		struct event e = normal_key(c);

		return e;
	}

	return parse_escape_sequence();
}
