#include "input.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

static struct termios orig_termios;

void disable_raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
		exit(1);
	}

	atexit(disable_raw_mode);

	struct termios raw = orig_termios;

	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_lflag &= ~(ISIG);
	raw.c_iflag &= ~(IXON);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		exit(1);
	}
}

int read_key() {
	char c;

	if (read(STDIN_FILENO, &c, 1) != 1) {
		return -1;
	}

	if (c != '\033') {
		return c;
	}

	char seq[3];

	if (read(STDIN_FILENO, &seq[0], 1) != 1) {
		return '\033';
	}

	if (read(STDIN_FILENO, &seq[1], 1) != 1) {
		return '\033';
	}

	if (seq[0] == '[')  {
		switch (seq[1]) {
		case 'A':
			return KEY_ARROW_UP;
		case 'B':
			return KEY_ARROW_DOWN;
		case 'C':
			return KEY_ARROW_RIGHT;
		case 'D':
			return KEY_ARROW_LEFT;
		}
	}

	return '\033';
}