#include "selection.h"

// PRE: sel != NULL for all functions

void selection_normalize
(
	const Selection* sel,
	size_t* x1, size_t* y1,
	size_t* x2, size_t* y2
)
{
	if (sel->start_y < sel->end_y || 
	   (sel->start_y == sel->end_y && sel->start_x <= sel->end_x)) {
		*x1 = sel->start_x;
		*y1 = sel->start_y;
		*x2 = sel->end_x;
		*y2 = sel->end_y;
	} else {
		*x1 = sel->end_x;
		*y1 = sel->end_y;
		*x2 = sel->start_x;
		*y2 = sel->start_y;
	}
}


// PRE: c != NULL

void selection_start(Selection* sel, Cursor* c) {
	sel->active = 1;
	sel->start_x = c->x;
	sel->start_y = c->y;
	sel->end_x = c->x;
	sel->end_y = c->y;
}


// PRE: c != NULL

void selection_update(Selection* sel, Cursor* c) {
	if (!sel->active) {
		return;
	}

	sel->end_x = c->x;
	sel->end_y = c->y;
}


void selection_clear(Selection* sel) {
	sel->active = 0;
	sel->start_x = 0;
	sel->start_y = 0;
	sel->end_y = 0;
	sel->end_x = 0;
}
