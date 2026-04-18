#include "scroll.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

// PRE: view != NULL
// PRE: c != NULL
// PRE: tsize != NULL

void update_scroll(View* view, Cursor* c, TerminalSize* tsize) {
	if (c->y < view->row_offset) {
		view->row_offset = c->y;
	}

	if (c->y >= view->row_offset + tsize->rows) {
		view->row_offset = c->y - tsize->rows + 1;
	}

	if (c->x < view->col_offset) {
		view->col_offset = c->x;
	}

	if (c->x >= view->col_offset + tsize->cols) {
		view->col_offset = c->x - tsize->cols + 1;
	}
}


// PRE: tsize != NULL

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