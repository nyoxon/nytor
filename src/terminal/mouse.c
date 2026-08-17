#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include "terminal/mouse.h"

static volatile sig_atomic_t mouse_enabled = 0;

void mouse_on() {
	if (mouse_enabled) {
		return;
	}

	write(STDOUT_FILENO, "\033[?1000h", 8); // click event
	write(STDOUT_FILENO, "\033[?1002h", 8); // movement event
	write(STDOUT_FILENO, "\033[?1006h", 8); // SGR format

	mouse_enabled = 1;
}

void mouse_off() {
	if (!mouse_enabled) {
		return;
	}

	write(STDOUT_FILENO, "\033[?1000l", 8);
	write(STDOUT_FILENO, "\033[?1002l", 8);
	write(STDOUT_FILENO, "\033[?1006l", 8);

	mouse_enabled = 0;
}


void mouse_get_event() {
	char c;

	if (read(STDIN_FILENO, &c, 1) != 1) {
		return;
	}

	if (c == '\033') {
		read(STDIN_FILENO, &c, 1);

		if (c == '[') {
			read(STDIN_FILENO, &c, 1);

			if (c == '<') {
				char buf[64];
				long unsigned i = 0;

				while (read(STDIN_FILENO, &c, 1) == 1) {
					buf[i++] = c;

					if (c == 'M' || c == 'm') {
						break;
					}

					if (i >= sizeof(buf) - 1) {
						break;
					}
				}

				buf[i] = '\0';
			}
		}
	}
}