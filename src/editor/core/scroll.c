#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "editor/core/scroll.h"

// PRE: view != NULL
// PRE: tsize != NULL

void update_scroll
(
	View* view, 
	const Position pos,
	size_t cursor_screen_x,
	TerminalSize* tsize
) 
{
	if (pos.y < view->row_offset) {
		view->row_offset = pos.y;
	}

	if (pos.y >= view->row_offset + tsize->rows) {
		view->row_offset = pos.y - tsize->rows + 1;
	}

	if (cursor_screen_x < view->col_offset) {
		view->col_offset = cursor_screen_x;
	}

	if (cursor_screen_x >= view->col_offset + tsize->cols) {
		view->col_offset = cursor_screen_x - tsize->cols + 1;
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

	if (tsize->cols > 0) {
		tsize->cols--;
	}

	return 0;	
}