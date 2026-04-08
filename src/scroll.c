#include "scroll.h"

void update_scroll(View* view, Cursor* c, size_t screen_rows) {
	if (c->y < view->row_offset) {
		view->row_offset = c->y;
	}

	if (c->y >= view->row_offset + screen_rows) {
		view->row_offset = c->y - screen_rows + 1;
	}
}