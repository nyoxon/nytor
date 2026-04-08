#include "scroll.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

void update_scroll(View* view, Cursor* c, TerminalSize* tsize) {
	if (c->y < view->row_offset) {
		view->row_offset = c->y;
	}

	if (c->y >= view->row_offset + tsize->rows) {
		view->row_offset = c->y - tsize->rows + 1;
	}
}

int update_terminal_size(TerminalSize* tsize) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	}

	tsize->rows = ws.ws_row;
	tsize->cols = ws.ws_col;

	if (tsize->rows > 0) {
		tsize->rows--;
	}

	return 0;	
}