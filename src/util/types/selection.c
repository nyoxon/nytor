#include "util/types/selection.h"

// PRE: sel != NULL for all functions

int selection_start_before_end(const Selection* sel) {
	return (sel->start.y < sel->end.y || 
	       (sel->start.y == sel->end.y && 
	   	    sel->start.x <= sel->end.x));
}

void selection_normalize
(
	const Selection* sel,
	Position* a,
	Position* b
)
{
	if (sel->start.y < sel->end.y || 
	   (sel->start.y == sel->end.y && 
	   	sel->start.x <= sel->end.x)) 
	{
		*a = sel->start;
		*b = sel->end;
	} else {
		*a = sel->end;
		*b = sel->start;
	}
}


// PRE: c != NULL

void selection_start(Selection* sel, Position pos) {
	sel->start = pos;
	sel->end = pos;
	sel->active = 1;
	sel->linewise = 0;
}


// PRE: c != NULL

void selection_update(Selection* sel, Position pos) {
	if (!sel->active) {
		return;
	}

	sel->end = pos;
}


void selection_clear(Selection* sel) {
	sel->active = 0;
	sel->start = POS_ZERO;
	sel->end = POS_ZERO;
	sel->linewise = 0;
}
